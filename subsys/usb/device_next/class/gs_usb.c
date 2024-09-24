/*
 * Copyright (c) 2022-2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/usb/udc.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net_buf.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/usb/usbd.h>

#include <cannectivity/usb/class/gs_usb.h>

LOG_MODULE_REGISTER(gs_usb, CONFIG_USBD_GS_USB_LOG_LEVEL);

#define DT_DRV_COMPAT gs_usb

/* USB class instance state flag bits */
#define GS_USB_STATE_CLASS_ENABLED 0U

struct gs_usb_desc {
	struct usb_association_descriptor iad;
	struct usb_if_descriptor if0;
	struct usb_ep_descriptor if0_in_ep;
	struct usb_ep_descriptor if0_dummy_ep;
	struct usb_ep_descriptor if0_out_ep;
	struct usb_ep_descriptor if0_hs_in_ep;
	struct usb_ep_descriptor if0_hs_dummy_ep;
	struct usb_ep_descriptor if0_hs_out_ep;
	struct usb_desc_header nil_desc;
};

struct gs_usb_channel_data {
	const struct device *dev;
	struct k_sem rx_overflows;
	uint32_t features;
	uint32_t mode;
	uint16_t ch;
	bool started;
	bool busoff;
};

struct gs_usb_data {
	struct gs_usb_desc *const desc;
	const struct usb_desc_header **const fs_desc;
	const struct usb_desc_header **const hs_desc;
	struct usbd_class_data *c_data;
	const struct device *dev;

	struct gs_usb_channel_data channels[CONFIG_USBD_GS_USB_MAX_CHANNELS];
	size_t nchannels;

	struct gs_usb_ops ops;
	void *user_data;

	struct net_buf_pool *pool;

	atomic_t state;
#ifdef CONFIG_USBD_GS_USB_TIMESTAMP_SOF
	uint32_t timestamp;
	bool sof_seen;
#endif /* CONFIG_USBD_GS_USB_TIMESTAMP_SOF */

	struct k_sem in_sem;
	struct k_fifo rx_fifo;
	struct k_thread rx_thread;

	struct k_fifo tx_fifo;
	struct k_thread tx_thread;

	K_KERNEL_STACK_MEMBER(rx_stack, CONFIG_USBD_GS_USB_RX_THREAD_STACK_SIZE);
	K_KERNEL_STACK_MEMBER(tx_stack, CONFIG_USBD_GS_USB_TX_THREAD_STACK_SIZE);
};

static int gs_usb_reset_channel(const struct device *dev, uint16_t ch);

static int gs_usb_request_host_format(const struct net_buf *const buf)
{
	struct gs_usb_host_config *hc = (struct gs_usb_host_config *)buf->data;
	uint32_t byte_order;

	if (buf->len != sizeof(*hc)) {
		LOG_ERR("invalid length for host format request (%d)", buf->len);
		return -EINVAL;
	}

	byte_order = sys_le32_to_cpu(hc->byte_order);

	if (byte_order != GS_USB_HOST_FORMAT_LITTLE_ENDIAN) {
		LOG_ERR("unsupported host byte order (0x%08x)", byte_order);
		return -ENOTSUP;
	}

	return 0;
}

static int gs_usb_request_bt_const(const struct device *dev, uint16_t ch, struct net_buf *const buf)
{
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
	struct gs_usb_device_bt_const bt_const;
	const struct can_timing *min;
	const struct can_timing *max;
	uint32_t rate;
	int err;

	if (ch >= data->nchannels) {
		LOG_ERR("bt_const request for non-existing channel %u", ch);
		return -EINVAL;
	}

	channel = &data->channels[ch];

	err = can_get_core_clock(channel->dev, &rate);
	if (err != 0U) {
		LOG_ERR("failed to get core clock for channel %u (err %d)", ch, err);
		return err;
	}

	min = can_get_timing_min(channel->dev);
	max = can_get_timing_max(channel->dev);

	bt_const.feature = sys_cpu_to_le32(channel->features);
	bt_const.fclk_can = sys_cpu_to_le32(rate);
	bt_const.tseg1_min = sys_cpu_to_le32(min->prop_seg + min->phase_seg1);
	bt_const.tseg1_max = sys_cpu_to_le32(max->prop_seg + max->phase_seg1);
	bt_const.tseg2_min = sys_cpu_to_le32(min->phase_seg2);
	bt_const.tseg2_max = sys_cpu_to_le32(max->phase_seg2);
	bt_const.sjw_max = sys_cpu_to_le32(max->sjw);
	bt_const.brp_min = sys_cpu_to_le32(min->prescaler);
	bt_const.brp_max = sys_cpu_to_le32(max->prescaler);
	bt_const.brp_inc = 1U;

	net_buf_add_mem(buf, &bt_const, sizeof(bt_const));

	return 0;
}

static int gs_usb_request_bt_const_ext(const struct device *dev, uint16_t ch,
				       struct net_buf *const buf)
{
#ifdef CONFIG_CAN_FD_MODE
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
	struct gs_usb_device_bt_const_ext bt_const_ext;
	const struct can_timing *min;
	const struct can_timing *max;
	uint32_t rate;
	int err;

	if (ch >= data->nchannels) {
		LOG_ERR("bt_const_ext request for non-existing channel %u", ch);
		return -EINVAL;
	}

	channel = &data->channels[ch];

	err = can_get_core_clock(channel->dev, &rate);
	if (err != 0U) {
		LOG_ERR("failed to get core clock for channel %u (err %d)", ch, err);
		return err;
	}

	min = can_get_timing_min(channel->dev);
	max = can_get_timing_max(channel->dev);

	bt_const_ext.feature = sys_cpu_to_le32(channel->features);
	bt_const_ext.fclk_can = sys_cpu_to_le32(rate);

	bt_const_ext.tseg1_min = sys_cpu_to_le32(min->prop_seg + min->phase_seg1);
	bt_const_ext.tseg1_max = sys_cpu_to_le32(max->prop_seg + max->phase_seg1);
	bt_const_ext.tseg2_min = sys_cpu_to_le32(min->phase_seg2);
	bt_const_ext.tseg2_max = sys_cpu_to_le32(max->phase_seg2);
	bt_const_ext.sjw_max = sys_cpu_to_le32(max->sjw);
	bt_const_ext.brp_min = sys_cpu_to_le32(min->prescaler);
	bt_const_ext.brp_max = sys_cpu_to_le32(max->prescaler);
	bt_const_ext.brp_inc = 1U;

	min = can_get_timing_data_min(channel->dev);
	max = can_get_timing_data_max(channel->dev);

	if (min == NULL || max == NULL) {
		LOG_ERR("failed to get min/max data phase timing for channel %u", ch);
		return -ENOTSUP;
	};

	bt_const_ext.dtseg1_min = sys_cpu_to_le32(min->prop_seg + min->phase_seg1);
	bt_const_ext.dtseg1_max = sys_cpu_to_le32(max->prop_seg + max->phase_seg1);
	bt_const_ext.dtseg2_min = sys_cpu_to_le32(min->phase_seg2);
	bt_const_ext.dtseg2_max = sys_cpu_to_le32(max->phase_seg2);
	bt_const_ext.dsjw_max = sys_cpu_to_le32(max->sjw);
	bt_const_ext.dbrp_min = sys_cpu_to_le32(min->prescaler);
	bt_const_ext.dbrp_max = sys_cpu_to_le32(max->prescaler);
	bt_const_ext.dbrp_inc = 1U;

	net_buf_add_mem(buf, &bt_const_ext, sizeof(bt_const_ext));

	return 0;
#else /* CONFIG_CAN_FD_MODE */
	return -ENOTSUP;
#endif /* !CONFIG_CAN_FD_MODE */
}

static int gs_usb_request_get_termination(const struct device *dev, uint16_t ch,
					  struct net_buf *buf)
{
#ifdef CONFIG_USBD_GS_USB_TERMINATION
	struct gs_usb_data *data = dev->data;
	struct gs_usb_device_termination_state ts;
	bool terminated;
	int err;

	if (ch >= data->nchannels) {
		LOG_ERR("get_termination request for non-existing channel %u", ch);
		return -EINVAL;
	}

	if (data->ops.get_termination == NULL) {
		LOG_ERR("get termination not supported");
		return -ENOTSUP;
	}

	err = data->ops.get_termination(dev, ch, &terminated, data->user_data);
	if (err != 0U) {
		LOG_ERR("failed to get termination state for channel %u (err %d)", ch, err);
		return err;
	}

	if (terminated) {
		ts.state = sys_cpu_to_le32(GS_USB_CHANNEL_TERMINATION_STATE_ON);
	} else {
		ts.state = sys_cpu_to_le32(GS_USB_CHANNEL_TERMINATION_STATE_OFF);
	}

	net_buf_add_mem(buf, &ts, sizeof(ts));

	return 0;
#else /* CONFIG_USBD_GS_USB_TERMINATION */
	return -ENOTSUP;
#endif /* !CONFIG_USBD_GS_USB_TERMINATION */
}

static int gs_usb_request_set_termination(const struct device *dev, uint16_t ch,
					  const struct net_buf *const buf)
{
#ifdef CONFIG_USBD_GS_USB_TERMINATION
	struct gs_usb_data *data = dev->data;
	struct gs_usb_device_termination_state *ts =
		(struct gs_usb_device_termination_state *)buf->data;
	uint32_t state;
	bool terminate;

	if (ch >= data->nchannels) {
		LOG_ERR("set termination request for non-existing channel %u", ch);
		return -EINVAL;
	}

	if (data->ops.set_termination == NULL) {
		LOG_ERR("set termination not supported");
		return -ENOTSUP;
	}

	if (buf->len != sizeof(*ts)) {
		LOG_ERR("invalid length for set termination request (%d)", buf->len);
		return -EINVAL;
	}

	state = sys_le32_to_cpu(ts->state);

	switch (state) {
	case GS_USB_CHANNEL_TERMINATION_STATE_OFF:
		terminate = false;
		break;
	case GS_USB_CHANNEL_TERMINATION_STATE_ON:
		terminate = true;
		break;
	default:
		LOG_ERR("unsupported set termination state %d for channel %u", state, ch);
		return -ENOTSUP;
	}

	return data->ops.set_termination(dev, ch, terminate, data->user_data);
#else /* CONFIG_USBD_GS_USB_TERMINATION */
	return -ENOTSUP;
#endif /* !CONFIG_USBD_GS_USB_TERMINATION */
}

static int gs_usb_request_get_state(const struct device *dev, uint16_t ch,
				    struct net_buf *const buf)
{
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
	struct gs_usb_device_state ds;
	struct can_bus_err_cnt err_cnt;
	enum can_state state;
	int err;

	if (ch >= data->nchannels) {
		LOG_ERR("get_state request for non-existing channel %u", ch);
		return -EINVAL;
	}

	channel = &data->channels[ch];

	err = can_get_state(channel->dev, &state, &err_cnt);
	if (err != 0U) {
		LOG_ERR("failed to get state for channel %u (err %d)", ch, err);
		return err;
	}

	ds.rxerr = sys_cpu_to_le32(err_cnt.rx_err_cnt);
	ds.txerr = sys_cpu_to_le32(err_cnt.tx_err_cnt);

	switch (state) {
	case CAN_STATE_ERROR_ACTIVE:
		ds.state = sys_cpu_to_le32(GS_USB_CHANNEL_STATE_ERROR_ACTIVE);
		break;
	case CAN_STATE_ERROR_WARNING:
		ds.state = sys_cpu_to_le32(GS_USB_CHANNEL_STATE_ERROR_WARNING);
		break;
	case CAN_STATE_ERROR_PASSIVE:
		ds.state = sys_cpu_to_le32(GS_USB_CHANNEL_STATE_ERROR_PASSIVE);
		break;
	case CAN_STATE_BUS_OFF:
		ds.state = sys_cpu_to_le32(GS_USB_CHANNEL_STATE_BUS_OFF);
		break;
	case CAN_STATE_STOPPED:
		ds.state = sys_cpu_to_le32(GS_USB_CHANNEL_STATE_STOPPED);
		break;
	default:
		LOG_ERR("unsupported state %d for channel %u", state, ch);
		return -ENOTSUP;
	}

	net_buf_add_mem(buf, &ds, sizeof(ds));

	return 0;
}

static void gs_usb_bittiming_to_can_timing(const struct gs_usb_device_bittiming *dbt,
					   const struct can_timing *min,
					   const struct can_timing *max, struct can_timing *result)
{
	memset(result, 0U, sizeof(*result));

	result->sjw = dbt->sjw;
	result->prop_seg = dbt->prop_seg;
	result->phase_seg1 = dbt->phase_seg1;
	result->phase_seg2 = dbt->phase_seg2;
	result->prescaler = dbt->brp;

	if (result->prop_seg < min->prop_seg) {
		/* Move TQs from phase segment 1 to propagation segment */
		result->phase_seg1 -= (min->prop_seg - result->prop_seg);
		result->prop_seg = min->prop_seg;
	} else if (result->prop_seg > max->prop_seg) {
		/* Move TQs from propagation segment to phase segment 1 */
		result->phase_seg1 += result->prop_seg - max->prop_seg;
		result->prop_seg = max->prop_seg;
	}

	if (result->phase_seg1 < min->phase_seg1) {
		/* Move TQs from propagation segment to phase segment 1 */
		result->prop_seg -= (min->phase_seg1 - result->phase_seg1);
		result->phase_seg1 = min->phase_seg1;
	} else if (result->phase_seg1 > max->phase_seg1) {
		/* Move TQs from phase segment 1 to propagation segment */
		result->prop_seg += result->phase_seg1 - max->phase_seg1;
		result->phase_seg1 = max->phase_seg1;
	}

	LOG_DBG("request: prop_seg %u, phase_seg1 %u, phase_seq2 %u, sjw %u, brp %u", dbt->prop_seg,
		dbt->phase_seg1, dbt->phase_seg2, dbt->sjw, dbt->brp);
	LOG_DBG("result: prop_seg %u, phase_seg1 %u, phase_seq2 %u, sjw %u, brp %u",
		result->prop_seg, result->phase_seg1, result->phase_seg2, result->sjw,
		result->prescaler);
};

static int gs_usb_request_bittiming(const struct device *dev, uint16_t ch,
				    const struct net_buf *const buf)
{
	struct gs_usb_device_bittiming *dbtp = (struct gs_usb_device_bittiming *)buf->data;
	struct gs_usb_device_bittiming dbt;
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
	const struct can_timing *min;
	const struct can_timing *max;
	struct can_timing timing;
	int err;

	if (ch >= data->nchannels) {
		LOG_ERR("bittiming request for non-existing channel %u", ch);
		return -EINVAL;
	}

	channel = &data->channels[ch];

	if (buf->len != sizeof(*dbtp)) {
		LOG_ERR("invalid length for bittiming request (%d)", buf->len);
		return -EINVAL;
	}

	if (channel->started) {
		LOG_WRN("cannot change timing for already started channel %u", ch);
		return -EBUSY;
	}

	dbt.prop_seg = sys_le32_to_cpu(dbtp->prop_seg);
	dbt.phase_seg1 = sys_le32_to_cpu(dbtp->phase_seg1);
	dbt.phase_seg2 = sys_le32_to_cpu(dbtp->phase_seg2);
	dbt.sjw = sys_le32_to_cpu(dbtp->sjw);
	dbt.brp = sys_le32_to_cpu(dbtp->brp);

	min = can_get_timing_min(channel->dev);
	max = can_get_timing_max(channel->dev);

	gs_usb_bittiming_to_can_timing(dbtp, min, max, &timing);

	err = can_set_timing(channel->dev, &timing);
	if (err != 0U) {
		LOG_ERR("failed to set timing for channel %u (err %d)", ch, err);
		return err;
	}

	return 0;
}

static int gs_usb_request_data_bittiming(const struct device *dev, uint16_t ch,
					 const struct net_buf *const buf)
{
#ifdef CONFIG_CAN_FD_MODE
	struct gs_usb_device_bittiming *dbtp = (struct gs_usb_device_bittiming *)buf->data;
	struct gs_usb_device_bittiming dbt;
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
	const struct can_timing *min;
	const struct can_timing *max;
	struct can_timing timing_data;
	int err;

	if (ch >= data->nchannels) {
		LOG_ERR("data_bittiming request for non-existing channel %u", ch);
		return -EINVAL;
	}

	channel = &data->channels[ch];

	if (buf->len != sizeof(*dbtp)) {
		LOG_ERR("invalid length for data_bittiming request (%d)", buf->len);
		return -EINVAL;
	}

	if (channel->started) {
		LOG_WRN("cannot change data phase timing for already started channel %u", ch);
		return -EBUSY;
	}

	dbt.prop_seg = sys_le32_to_cpu(dbtp->prop_seg);
	dbt.phase_seg1 = sys_le32_to_cpu(dbtp->phase_seg1);
	dbt.phase_seg2 = sys_le32_to_cpu(dbtp->phase_seg2);
	dbt.sjw = sys_le32_to_cpu(dbtp->sjw);
	dbt.brp = sys_le32_to_cpu(dbtp->brp);

	min = can_get_timing_data_min(channel->dev);
	max = can_get_timing_data_max(channel->dev);

	if (min == NULL || max == NULL) {
		LOG_ERR("failed to get min/max data phase timing for channel %u", ch);
		return -ENOTSUP;
	};

	gs_usb_bittiming_to_can_timing(&dbt, min, max, &timing_data);

	err = can_set_timing_data(channel->dev, &timing_data);
	if (err != 0U) {
		LOG_ERR("failed to set data phase timing for channel %u (err %d)", ch, err);
		return err;
	}

	return 0;
#else /* CONFIG_CAN_FD_MODE */
	return -ENOTSUP;
#endif /* !CONFIG_CAN_FD_MODE */
}

static int gs_usb_request_mode(const struct device *dev, uint16_t ch,
			       const struct net_buf *const buf)
{
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
	struct gs_usb_device_mode *dm = (struct gs_usb_device_mode *)buf->data;
	can_mode_t mode = CAN_MODE_NORMAL;
	uint32_t flags;
	int err;

	if (ch >= data->nchannels) {
		LOG_ERR("mode request for non-existing channel %u", ch);
		return -EINVAL;
	}

	if (buf->len != sizeof(*dm)) {
		LOG_ERR("invalid length for mode request (%d)", buf->len);
		return -EINVAL;
	}

	channel = &data->channels[ch];

	switch (sys_le32_to_cpu(dm->mode)) {
	case GS_USB_CHANNEL_MODE_RESET:
		err = gs_usb_reset_channel(dev, ch);
		if (err != 0) {
			return err;
		}
		break;
	case GS_USB_CHANNEL_MODE_START:
		if (channel->started) {
			LOG_WRN("channel %u already started", ch);
			return -EALREADY;
		}

		flags = sys_le32_to_cpu(dm->flags);

		if ((flags & ~(channel->features)) != 0U) {
			LOG_ERR("unsupported flags 0x%08x for channel %u", flags, ch);
			return -ENOTSUP;
		}

		if ((flags & GS_USB_CAN_MODE_LISTEN_ONLY) != 0U) {
			mode |= CAN_MODE_LISTENONLY;
		}

		if ((flags & GS_USB_CAN_MODE_LOOP_BACK) != 0U) {
			mode |= CAN_MODE_LOOPBACK;
		}

		if ((flags & GS_USB_CAN_MODE_TRIPLE_SAMPLE) != 0U) {
			mode |= CAN_MODE_3_SAMPLES;
		}

		if ((flags & GS_USB_CAN_MODE_ONE_SHOT) != 0U) {
			mode |= CAN_MODE_ONE_SHOT;
		}

		if ((flags & GS_USB_CAN_MODE_FD) != 0U) {
			mode |= CAN_MODE_FD;
		}

		err = can_set_mode(channel->dev, mode);
		if (err != 0U) {
			LOG_ERR("failed to set channel %u mode 0x%08x (err %d)", ch, mode, err);
			return err;
		}

		err = can_start(channel->dev);
		if (err != 0U) {
			LOG_ERR("failed to start channel %u (err %d)", ch, err);
			return err;
		}

		channel->mode = flags;
		channel->started = true;
		break;
	default:
		LOG_ERR("unsupported mode %d requested for channel %u channel",
			sys_le32_to_cpu(dm->mode), ch);
		return -ENOTSUP;
	}

	if (data->ops.state != NULL) {
		err = data->ops.state(dev, channel->ch, channel->started, data->user_data);
		if (err != 0) {
			LOG_ERR("failed to report channel %u state change (err %d)", channel->ch,
				err);
		}
	}

	return 0;
}

static int gs_usb_request_identify(const struct device *dev, uint16_t ch,
				   const struct net_buf *const buf)
{
	struct gs_usb_data *data = dev->data;
	struct gs_usb_identify_mode *im = (struct gs_usb_identify_mode *)buf->data;
	uint32_t mode;
	bool identify;

	if (data->ops.identify == NULL) {
		LOG_ERR("identify not supported");
		return -ENOTSUP;
	}

	if (ch >= data->nchannels) {
		LOG_ERR("identify request for non-existing channel %u", ch);
		return -EINVAL;
	}

	if (buf->len != sizeof(*im)) {
		LOG_ERR("invalid length for identify request (%d)", buf->len);
		return -EINVAL;
	}

	mode = sys_le32_to_cpu(im->mode);

	switch (mode) {
	case GS_USB_CHANNEL_IDENTIFY_MODE_OFF:
		identify = false;
		break;
	case GS_USB_CHANNEL_IDENTIFY_MODE_ON:
		identify = true;
		break;
	default:
		LOG_ERR("unsupported identify mode %d for channel %u", mode, ch);
		return -ENOTSUP;
	}

	return data->ops.identify(dev, ch, identify, data->user_data);
}

static int gs_usb_request_device_config(const struct device *dev, struct net_buf *const buf)
{
	struct gs_usb_data *data = dev->data;
	struct gs_usb_device_config dc;

	memset(&dc, 0, sizeof(dc));
	dc.nchannels = data->nchannels - 1U; /* 8 bit representing 1 to 256 */
	dc.sw_version = sys_cpu_to_le32(GS_USB_SW_VERSION);
	dc.hw_version = sys_cpu_to_le32(GS_USB_HW_VERSION);

	net_buf_add_mem(buf, &dc, sizeof(dc));

	return 0;
}

#ifdef CONFIG_USBD_GS_USB_TIMESTAMP_SOF
static void gs_usb_sof(struct usbd_class_data *const c_data)
{
	const struct device *dev = usbd_class_get_private(c_data);
	struct gs_usb_data *data = dev->data;
	int err;

	if (data->ops.timestamp == NULL) {
		return;
	}

	err = data->ops.timestamp(dev, &data->timestamp, data->user_data);
	if (err != 0) {
		LOG_ERR("failed to get current timestamp (err %d)", err);
		return;
	}

	/* Not all USB device controller drivers support SoF events */
	data->sof_seen = true;
}
#endif /* CONFIG_USBD_GS_USB_TIMESTAMP_SOF */

static int gs_usb_request_timestamp(const struct device *dev, struct net_buf *buf)
{
#ifdef CONFIG_USBD_GS_USB_TIMESTAMP
	struct gs_usb_data *data = dev->data;
	uint32_t timestamp;
	int err;

	if (data->ops.timestamp == NULL) {
		LOG_ERR("timestamp not supported");
		return -ENOTSUP;
	}

#ifdef CONFIG_USBD_GS_USB_TIMESTAMP_SOF
	if (data->sof_seen) {
		timestamp = data->timestamp;
		data->sof_seen = false;
	} else {
#endif /* CONFIG_USBD_GS_USB_TIMESTAMP_SOF */
		err = data->ops.timestamp(dev, &timestamp, data->user_data);
		if (err != 0) {
			LOG_ERR("failed to get current timestamp (err %d)", err);
			return err;
		}
#ifdef CONFIG_USBD_GS_USB_TIMESTAMP_SOF
		LOG_WRN_ONCE("USB SoF event not supported, timestamp will be less accurate");
	}
#endif /* CONFIG_USBD_GS_USB_TIMESTAMP_SOF */

	LOG_DBG("timestamp: 0x%08x", timestamp);
	net_buf_add_le32(buf, timestamp);

	return 0;
#else /* CONFIG_USBD_GS_USB_TIMESTAMP */
	return -ENOTSUP;
#endif /* !CONFIG_USBD_GS_USB_TIMESTAMP */
}

static int gs_usb_control_to_dev(struct usbd_class_data *const c_data,
				 const struct usb_setup_packet *const setup,
				 const struct net_buf *const buf)
{
	const struct device *dev = usbd_class_get_private(c_data);
	uint16_t ch = setup->wValue;

	if (setup->RequestType.recipient != USB_REQTYPE_RECIPIENT_INTERFACE) {
		errno = -ENOTSUP;
		return 0;
	}

	switch (setup->bRequest) {
	case GS_USB_REQUEST_HOST_FORMAT:
		errno = gs_usb_request_host_format(buf);
		break;
	case GS_USB_REQUEST_BITTIMING:
		errno = gs_usb_request_bittiming(dev, ch, buf);
		break;
	case GS_USB_REQUEST_MODE:
		errno = gs_usb_request_mode(dev, ch, buf);
		break;
	case GS_USB_REQUEST_IDENTIFY:
		errno = gs_usb_request_identify(dev, ch, buf);
		break;
	case GS_USB_REQUEST_DATA_BITTIMING:
		errno = gs_usb_request_data_bittiming(dev, ch, buf);
		break;
	case GS_USB_REQUEST_SET_USER_ID:
		/* Not supported */
		errno = -ENOTSUP;
		break;
	case GS_USB_REQUEST_SET_TERMINATION:
		errno = gs_usb_request_set_termination(dev, ch, buf);
		break;
	default:
		LOG_ERR("control_to_dev: bmRequestType 0x%02x bRequest 0x%02x not supported",
			setup->bmRequestType, setup->bRequest);
		errno = -ENOTSUP;
		break;
	};

	return 0;
}

static int gs_usb_control_to_host(struct usbd_class_data *const c_data,
				  const struct usb_setup_packet *const setup,
				  struct net_buf *const buf)
{
	const struct device *dev = usbd_class_get_private(c_data);
	uint16_t ch = setup->wValue;

	if (setup->RequestType.recipient != USB_REQTYPE_RECIPIENT_INTERFACE) {
		errno = -ENOTSUP;
		return 0;
	}

	switch (setup->bRequest) {
	case GS_USB_REQUEST_BERR:
		/* Not supported */
		errno = -ENOTSUP;
		break;
	case GS_USB_REQUEST_BT_CONST:
		errno = gs_usb_request_bt_const(dev, ch, buf);
		break;
	case GS_USB_REQUEST_DEVICE_CONFIG:
		errno = gs_usb_request_device_config(dev, buf);
		break;
	case GS_USB_REQUEST_TIMESTAMP:
		errno = gs_usb_request_timestamp(dev, buf);
		break;
	case GS_USB_REQUEST_GET_USER_ID:
		/* Not supported */
		errno = -ENOTSUP;
		break;
	case GS_USB_REQUEST_BT_CONST_EXT:
		errno = gs_usb_request_bt_const_ext(dev, ch, buf);
		break;
	case GS_USB_REQUEST_GET_TERMINATION:
		errno = gs_usb_request_get_termination(dev, ch, buf);
		break;
	case GS_USB_REQUEST_GET_STATE:
		errno = gs_usb_request_get_state(dev, ch, buf);
		break;
	default:
		LOG_ERR("control_to_host: bmRequestType 0x%02x bRequest 0x%02x not supported",
			setup->bmRequestType, setup->bRequest);
		errno = -ENOTSUP;
		break;
	}

	return 0;
}

static uint8_t gs_usb_get_bulk_in_ep_addr(struct usbd_class_data *const c_data)
{
	const struct device *dev = usbd_class_get_private(c_data);
	struct gs_usb_data *data = dev->data;
	struct usbd_context *uds_ctx = usbd_class_get_ctx(c_data);
	struct gs_usb_desc *desc = data->desc;

	if (usbd_bus_speed(uds_ctx) == USBD_SPEED_HS) {
		return desc->if0_hs_in_ep.bEndpointAddress;
	}

	return desc->if0_in_ep.bEndpointAddress;
}

static uint8_t gs_usb_get_bulk_out_ep_addr(struct usbd_class_data *const c_data)
{
	const struct device *dev = usbd_class_get_private(c_data);
	struct gs_usb_data *data = dev->data;
	struct usbd_context *uds_ctx = usbd_class_get_ctx(c_data);
	struct gs_usb_desc *desc = data->desc;

	if (usbd_bus_speed(uds_ctx) == USBD_SPEED_HS) {
		return desc->if0_hs_out_ep.bEndpointAddress;
	}

	return desc->if0_out_ep.bEndpointAddress;
}

static struct net_buf *gs_usb_buf_alloc(struct gs_usb_data *data, uint8_t ep)
{
	struct udc_buf_info *bi;
	struct net_buf *buf = NULL;

	buf = net_buf_alloc_fixed(data->pool, K_NO_WAIT);
	if (buf == NULL) {
		return NULL;
	}

	bi = udc_get_buf_info(buf);
	memset(bi, 0, sizeof(struct udc_buf_info));
	bi->ep = ep;

	return buf;
}

static void gs_usb_fill_echo_frame_hdr(struct net_buf *buf, uint32_t echo_id, uint8_t channel,
				       uint8_t flags, uint8_t ep)
{
	struct gs_usb_host_frame_hdr hdr = {0};
	struct udc_buf_info *bi;

	__ASSERT_NO_MSG(buf != NULL);

	hdr.echo_id = sys_cpu_to_le32(echo_id);
	hdr.channel = channel;
	hdr.flags = flags;

	net_buf_reset(buf);
	net_buf_add_mem(buf, &hdr, sizeof(hdr));

	bi = udc_get_buf_info(buf);
	memset(bi, 0, sizeof(struct udc_buf_info));
	bi->ep = ep;
}

static int gs_usb_out_start(struct usbd_class_data *const c_data)
{
	const struct device *dev = usbd_class_get_private(c_data);
	struct gs_usb_data *data = dev->data;
	struct net_buf *buf;
	uint8_t ep;
	int ret;

	if (!atomic_test_bit(&data->state, GS_USB_STATE_CLASS_ENABLED)) {
		LOG_ERR("cannot start OUT transfer, class instance not enabled");
		return -EPERM;
	}

	ep = gs_usb_get_bulk_out_ep_addr(c_data);
	buf = gs_usb_buf_alloc(data, ep);
	if (buf == NULL) {
		LOG_ERR("failed to allocate buffer for ep 0x%02x", ep);
		return -ENOMEM;
	}

	ret = usbd_ep_enqueue(c_data, buf);
	if (ret != 0) {
		LOG_ERR("failed to enqueue buffer for ep 0x%02x (err %d)", ep, ret);
		net_buf_unref(buf);
	}

	return  ret;
}

static void gs_usb_can_state_change_callback(const struct device *can_dev, enum can_state state,
					     struct can_bus_err_cnt err_cnt, void *user_data)
{
	struct gs_usb_channel_data *channel = user_data;
	struct gs_usb_data *data = CONTAINER_OF(channel, struct gs_usb_data, channels[channel->ch]);
	uint32_t can_id = GS_USB_CAN_ID_FLAG_ERR;
	struct gs_usb_host_frame_hdr hdr = {0};
	uint8_t payload[8] = {0};
	struct net_buf *buf;

	__ASSERT_NO_MSG(can_dev == channel->dev);

	buf = net_buf_alloc_fixed(data->pool, K_NO_WAIT);
	if (buf == NULL) {
		LOG_ERR("failed to allocate error frame for channel %u", channel->ch);
		k_sem_give(&channel->rx_overflows);
		return;
	}

#ifdef CONFIG_USBD_GS_USB_TIMESTAMP
	uint32_t timestamp = 0U;
	int err;

	if ((channel->mode & GS_USB_CAN_MODE_HW_TIMESTAMP) != 0U) {
		err = data->ops.timestamp(data->dev, &timestamp, data->user_data);
		if (err != 0) {
			LOG_ERR("failed to get RX timestamp (err %d)", err);
		}
	}
#endif /* CONFIG_USBD_GS_USB_TIMESTAMP */

	hdr.echo_id = GS_USB_HOST_FRAME_ECHO_ID_RX_FRAME;
	hdr.can_dlc = can_bytes_to_dlc(sizeof(payload));
	hdr.channel = channel->ch;

	switch (state) {
	case CAN_STATE_ERROR_ACTIVE:
		can_id |= GS_USB_CAN_ID_FLAG_ERR_CRTL;
		payload[1] |= GS_USB_CAN_ERR_CRTL_ACTIVE;

		if (channel->busoff) {
			can_id |= GS_USB_CAN_ID_FLAG_ERR_RESTARTED;
		}
		break;
	case CAN_STATE_ERROR_WARNING:
		can_id |= GS_USB_CAN_ID_FLAG_ERR_CRTL;
		payload[1] |= GS_USB_CAN_ERR_CRTL_TX_WARNING | GS_USB_CAN_ERR_CRTL_RX_WARNING;
		break;
	case CAN_STATE_ERROR_PASSIVE:
		can_id |= GS_USB_CAN_ID_FLAG_ERR_CRTL;
		payload[1] |= GS_USB_CAN_ERR_CRTL_TX_PASSIVE | GS_USB_CAN_ERR_CRTL_RX_PASSIVE;
		break;
	case CAN_STATE_BUS_OFF:
		can_id |= GS_USB_CAN_ID_FLAG_ERR_BUSOFF;
		break;
	case CAN_STATE_STOPPED:
		/* Not reported */
		return;
	}

	channel->busoff = (state == CAN_STATE_BUS_OFF);

	can_id |= GS_USB_CAN_ID_FLAG_ERR_CNT;
	payload[6] = err_cnt.tx_err_cnt;
	payload[7] = err_cnt.rx_err_cnt;

	hdr.can_id = sys_cpu_to_le32(can_id);
	net_buf_add_mem(buf, &hdr, sizeof(hdr));
	net_buf_add_mem(buf, &payload, can_dlc_to_bytes(hdr.can_dlc));

#ifdef CONFIG_USBD_GS_USB_TIMESTAMP
	if ((channel->mode & GS_USB_CAN_MODE_HW_TIMESTAMP) != 0U) {
		net_buf_add_le32(buf, timestamp);
	}
#endif /* CONFIG_USBD_GS_USB_TIMESTAMP */

	k_fifo_put(&data->rx_fifo, buf);
}

static void gs_usb_can_rx_callback(const struct device *can_dev, struct can_frame *frame,
				   void *user_data)
{
	struct gs_usb_channel_data *channel = user_data;
	struct gs_usb_data *data = CONTAINER_OF(channel, struct gs_usb_data, channels[channel->ch]);
	struct gs_usb_host_frame_hdr hdr = {0};
	struct net_buf *buf;
	uint8_t ep;

	__ASSERT_NO_MSG(can_dev == channel->dev);

#ifdef CONFIG_USBD_GS_USB_TIMESTAMP
	uint32_t timestamp = 0U;
	int err;

	if ((channel->mode & GS_USB_CAN_MODE_HW_TIMESTAMP) != 0U) {
		err = data->ops.timestamp(data->dev, &timestamp, data->user_data);
		if (err != 0) {
			LOG_ERR("failed to get RX timestamp (err %d)", err);
		}
	}
#endif /* CONFIG_USBD_GS_USB_TIMESTAMP */

	ep = gs_usb_get_bulk_in_ep_addr(data->c_data);
	buf = gs_usb_buf_alloc(data, ep);
	if (buf == NULL) {
		LOG_ERR("failed to allocate RX host frame for channel %u", channel->ch);
		k_sem_give(&channel->rx_overflows);
		return;
	}

	hdr.echo_id = GS_USB_HOST_FRAME_ECHO_ID_RX_FRAME;
	hdr.can_id = sys_cpu_to_le32(frame->id);
	hdr.can_dlc = frame->dlc;
	hdr.channel = channel->ch;

	if ((frame->flags & CAN_FRAME_IDE) != 0U) {
		hdr.can_id |= sys_cpu_to_le32(GS_USB_CAN_ID_FLAG_IDE);
	}

	if ((frame->flags & CAN_FRAME_RTR) != 0U) {
		hdr.can_id |= sys_cpu_to_le32(GS_USB_CAN_ID_FLAG_RTR);
	}

	if (IS_ENABLED(CONFIG_CAN_FD_MODE)) {
		if ((frame->flags & CAN_FRAME_FDF) != 0U) {
			hdr.flags |= GS_USB_CAN_FLAG_FD;

			if ((frame->flags & CAN_FRAME_BRS) != 0U) {
				hdr.flags |= GS_USB_CAN_FLAG_BRS;
			}

			if ((frame->flags & CAN_FRAME_ESI) != 0U) {
				hdr.flags |= GS_USB_CAN_FLAG_ESI;
			}
		}
	}

	net_buf_add_mem(buf, &hdr, sizeof(hdr));

	if (IS_ENABLED(CONFIG_CAN_FD_MODE) && ((frame->flags & CAN_FRAME_FDF) != 0U)) {
		net_buf_add_mem(buf, &frame->data, can_dlc_to_bytes(CANFD_MAX_DLC));
	} else {
		net_buf_add_mem(buf, &frame->data, can_dlc_to_bytes(CAN_MAX_DLC));
	}

#ifdef CONFIG_USBD_GS_USB_TIMESTAMP
	if ((channel->mode & GS_USB_CAN_MODE_HW_TIMESTAMP) != 0U) {
		net_buf_add_le32(buf, timestamp);
	}
#endif /* CONFIG_USBD_GS_USB_TIMESTAMP */

	k_fifo_put(&data->rx_fifo, buf);
}

static void gs_usb_rx_thread(void *p1, void *p2, void *p3)
{
	const struct device *dev = p1;
	struct gs_usb_data *data = dev->data;
	struct gs_usb_host_frame_hdr *hdr;
	struct gs_usb_channel_data *channel;
	struct net_buf *buf;
	struct udc_buf_info *bi;
	uint32_t can_id;
	uint16_t ch;
	int err;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		buf = k_fifo_get(&data->rx_fifo, K_FOREVER);

		hdr = (struct gs_usb_host_frame_hdr *)buf->data;
		can_id = hdr->can_id;
		ch = hdr->channel;

		__ASSERT_NO_MSG(ch <= data->nchannels);
		channel = &data->channels[ch];

		if (k_sem_take(&channel->rx_overflows, K_NO_WAIT) == 0) {
			hdr->flags |= GS_USB_CAN_FLAG_OVERFLOW;
		}

		LOG_HEXDUMP_DBG(buf->data, buf->len, "RX host frame");

		err = usbd_ep_enqueue(data->c_data, buf);
		if (err != 0) {
			bi = udc_get_buf_info(buf);
			LOG_ERR("failed to enqueue buffer for ep 0x%02x (err %d)", bi->ep, err);
			net_buf_unref(buf);
			continue;
		}

		k_sem_take(&data->in_sem, K_FOREVER);
		net_buf_unref(buf);

		if ((can_id & GS_USB_CAN_ID_FLAG_ERR) != 0U) {
			/* Only indicate actual RX/TX activity, not error frames */
			continue;
		}

		if (data->ops.activity != NULL) {
			err = data->ops.activity(dev, ch, data->user_data);
			if (err != 0) {
				LOG_ERR("activity callback for channel %u failed (err %d)", ch,
					err);
			}
		}
	}
}

static void gs_usb_can_tx_callback(const struct device *can_dev, int error, void *user_data)
{
	struct net_buf *buf = user_data;
	uintptr_t *pchannel = net_buf_remove_mem(buf, sizeof(uintptr_t));
	struct gs_usb_channel_data *channel = UINT_TO_POINTER(*pchannel);
	struct gs_usb_data *data = CONTAINER_OF(channel, struct gs_usb_data, channels[channel->ch]);
	struct gs_usb_host_frame_hdr *hdr = (struct gs_usb_host_frame_hdr *)buf->data;
	size_t padding;
	uint8_t *tail;

	if (error != 0) {
		/* There is no way to report a dropped frame to the host driver */
		LOG_ERR("failed to send CAN frame (err %d)", error);
		net_buf_unref(buf);
		return;
	}

#ifdef CONFIG_USBD_GS_USB_TIMESTAMP
	uint32_t timestamp = 0U;
	int err;

	if ((channel->mode & GS_USB_CAN_MODE_HW_TIMESTAMP) != 0U) {
		err = data->ops.timestamp(data->dev, &timestamp, data->user_data);
		if (err != 0) {
			LOG_ERR("failed to get TX timestamp (err %d)", err);
		}
	}
#endif /* CONFIG_USBD_GS_USB_TIMESTAMP */

	if ((hdr->flags & GS_USB_CAN_FLAG_FD) != 0U) {
		padding = sizeof(struct gs_usb_canfd_frame);
	} else {
		padding = sizeof(struct gs_usb_can_frame);
	}

	tail = net_buf_add(buf, padding);
	memset(tail, 0, padding);

#ifdef CONFIG_USBD_GS_USB_TIMESTAMP
	if ((channel->mode & GS_USB_CAN_MODE_HW_TIMESTAMP) != 0U) {
		net_buf_add_le32(buf, timestamp);
	}
#endif /* CONFIG_USBD_GS_USB_TIMESTAMP */

	LOG_DBG("TX done");

	k_fifo_put(&data->rx_fifo, buf);
}

static void gs_usb_tx_thread(void *p1, void *p2, void *p3)
{
	const struct device *dev = p1;
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
	struct gs_usb_host_frame_hdr *hdr;
	struct can_frame frame;
	struct net_buf *buf;
	uintptr_t pchannel;
	uint32_t can_id;
	uint8_t ep;
	int err;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		memset(&frame, 0, sizeof(frame));
		buf = k_fifo_get(&data->tx_fifo, K_FOREVER);

		LOG_HEXDUMP_DBG(buf->data, buf->len, "TX host frame");

		if (buf->len < sizeof(*hdr)) {
			LOG_ERR("TX host frame contains no header (%d < %d)", buf->len,
				sizeof(*hdr));
			net_buf_unref(buf);
			continue;
		}

		hdr = net_buf_pull_mem(buf, sizeof(*hdr));

		if (hdr->channel >= data->nchannels) {
			LOG_ERR("TX host frame for non-existing channel %u", hdr->channel);
			net_buf_unref(buf);
			continue;
		}

		channel = &data->channels[hdr->channel];
		if (!channel->started) {
			LOG_ERR("channel %u not started, ignoring TX host frame", hdr->channel);
			net_buf_unref(buf);
			continue;
		}

		can_id = sys_le32_to_cpu(hdr->can_id);
		if ((can_id & GS_USB_CAN_ID_FLAG_IDE) != 0U) {
			frame.flags |= CAN_FRAME_IDE;
			frame.id = can_id & CAN_EXT_ID_MASK;
		} else {
			frame.id = hdr->can_id & CAN_STD_ID_MASK;
		}

		if (IS_ENABLED(CONFIG_CAN_FD_MODE)) {
			if ((hdr->flags & GS_USB_CAN_FLAG_FD) != 0U) {
				frame.flags |= CAN_FRAME_FDF;
			}

			if ((hdr->flags & GS_USB_CAN_FLAG_BRS) != 0U) {
				frame.flags |= CAN_FRAME_BRS;
			}
		}

		frame.dlc = hdr->can_dlc;

		if ((can_id & GS_USB_CAN_ID_FLAG_RTR) != 0U) {
			frame.flags |= CAN_FRAME_RTR;
		} else if (hdr->can_dlc != 0U) {
			if (can_dlc_to_bytes(frame.dlc) > buf->len) {
				LOG_ERR("TX host frame DLC exceeds buffer length (%d > %d)",
					can_dlc_to_bytes(frame.dlc), buf->len);
				net_buf_unref(buf);
				continue;
			}

			memcpy(&frame.data, buf->data, can_dlc_to_bytes(frame.dlc));
		}

		ep = gs_usb_get_bulk_in_ep_addr(data->c_data);
		gs_usb_fill_echo_frame_hdr(buf, sys_le32_to_cpu(hdr->echo_id), hdr->channel,
					   hdr->flags, ep);

		pchannel = POINTER_TO_UINT(channel);
		net_buf_add_mem(buf, &pchannel, sizeof(pchannel));

		err = can_send(channel->dev, &frame, K_FOREVER, gs_usb_can_tx_callback, buf);
		if (err != 0) {
			/* There is no way to report a dropped frame to the host driver */
			LOG_ERR("failed to enqueue CAN frame for TX (err %d)", err);
			net_buf_unref(buf);
		}
	}
}

static int gs_usb_request(struct usbd_class_data *const c_data, struct net_buf *buf, int err)
{
	const struct device *dev = usbd_class_get_private(c_data);
	struct gs_usb_data *data = dev->data;
	struct udc_buf_info *bi = udc_get_buf_info(buf);
	int ret;

	if (err != 0) {
		if (err == -ECONNABORTED) {
			LOG_WRN("request cancelled for ep 0x%02x (err %d)", bi->ep, err);
		} else {
			LOG_ERR("request failed for ep 0x%02x (err %d)", bi->ep, err);
		}

		net_buf_unref(buf);
		return 0;
	}

	LOG_DBG("request complete for ep 0x%02x (err %d)", bi->ep, err);

	if (bi->ep == gs_usb_get_bulk_out_ep_addr(c_data)) {
		if (!atomic_test_bit(&data->state, GS_USB_STATE_CLASS_ENABLED)) {
			LOG_WRN("class not enabled");
			net_buf_unref(buf);
			return 0;
		}

		k_fifo_put(&data->tx_fifo, buf);

		ret = gs_usb_out_start(c_data);
		if (ret != 0) {
			LOG_ERR("failed to restart OUT transfer (err %d)", ret);
		}

		return ret;
	} else if (bi->ep == gs_usb_get_bulk_in_ep_addr(c_data)) {
		net_buf_unref(buf);
		k_sem_give(&data->in_sem);
		return 0;
	}

	LOG_WRN("request complete for unsupported ep 0x%02x", bi->ep);
	return usbd_ep_buf_free(usbd_class_get_ctx(c_data), buf);
}

static void gs_usb_enable(struct usbd_class_data *const c_data)
{
	const struct device *dev = usbd_class_get_private(c_data);
	struct gs_usb_data *data = dev->data;
	int err;

	atomic_set_bit(&data->state, GS_USB_STATE_CLASS_ENABLED);
	LOG_DBG("enabled");

	err = gs_usb_out_start(c_data);
	if (err != 0) {
		LOG_ERR("failed to start OUT transfer (err %d)", err);
	}
}

static void gs_usb_disable(struct usbd_class_data *const c_data)
{
	const struct device *dev = usbd_class_get_private(c_data);
	struct gs_usb_data *data = dev->data;
	struct usbd_context *uds_ctx = usbd_class_get_ctx(c_data);
	uint16_t ch;
	uint8_t ep;
	int err;

	atomic_clear_bit(&data->state, GS_USB_STATE_CLASS_ENABLED);

	for (ch = 0; ch < data->nchannels; ch++) {
		(void)gs_usb_reset_channel(dev, ch);
	}

	ep = gs_usb_get_bulk_in_ep_addr(c_data);
	err = usbd_ep_dequeue(uds_ctx, ep);
	if (err != 0) {
		LOG_ERR("failed to dequeue IN ep 0x%02x (err %d)", ep, err);
	}

	ep = gs_usb_get_bulk_out_ep_addr(c_data);
	err = usbd_ep_dequeue(uds_ctx, ep);
	if (err != 0) {
		LOG_ERR("failed to dequeue OUT ep 0x%02x (err %d)", ep, err);
	}

	k_sem_reset(&data->in_sem);

	LOG_DBG("disabled");
}

static uint32_t gs_usb_features_from_ops(struct gs_usb_ops *ops)
{
	uint32_t features = 0U;

	if (UTIL_AND(IS_ENABLED(CONFIG_USBD_GS_USB_TIMESTAMP), ops->timestamp != NULL)) {
		features |= GS_USB_CAN_FEATURE_HW_TIMESTAMP;
	}

	if (ops->identify != NULL) {
		features |= GS_USB_CAN_FEATURE_IDENTIFY;
	}

	if (UTIL_AND(IS_ENABLED(CONFIG_USBD_GS_USB_TERMINATION),
		     ops->set_termination != NULL && ops->get_termination != NULL)) {
		features |= GS_USB_CAN_FEATURE_TERMINATION;
	}

	return features;
}

static uint32_t gs_usb_features_from_capabilities(can_mode_t caps)
{
	uint32_t features = 0U;

	if ((caps & CAN_MODE_LOOPBACK) != 0U) {
		features |= GS_USB_CAN_FEATURE_LOOP_BACK;
	}

	if ((caps & CAN_MODE_LISTENONLY) != 0U) {
		features |= GS_USB_CAN_FEATURE_LISTEN_ONLY;
	}

	if ((caps & CAN_MODE_FD) != 0U) {
		features |= GS_USB_CAN_FEATURE_FD;
		features |= GS_USB_CAN_FEATURE_BT_CONST_EXT;
	}

	if ((caps & CAN_MODE_ONE_SHOT) != 0U) {
		features |= GS_USB_CAN_FEATURE_ONE_SHOT;
	}

	if ((caps & CAN_MODE_3_SAMPLES) != 0U) {
		features |= GS_USB_CAN_FEATURE_TRIPLE_SAMPLE;
	}

	return features;
}

static int gs_usb_reset_channel(const struct device *dev, uint16_t ch)
{
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
	int err;

	if (ch >= data->nchannels) {
		LOG_ERR("attempt to reset non-existing channel %u", ch);
		return -EINVAL;
	}

	channel = &data->channels[ch];
	channel->mode = GS_USB_CAN_MODE_NORMAL;
	channel->started = false;
	channel->busoff = false;
	k_sem_reset(&channel->rx_overflows);

	err = can_stop(channel->dev);
	if (err != 0 && err != -EALREADY) {
		LOG_ERR("failed to stop channel %u (err %d)", ch, err);
		return err;
	}

	return 0;
}

static int gs_usb_register_channel(struct gs_usb_channel_data *channel, uint16_t ch,
				   const struct device *can_dev, uint32_t common_features)
{
	const struct can_filter filters[] = {
		{
			.id = 0U,
			.mask = 0U,
			.flags = 0U,
		},
		{
			.id = 0U,
			.mask = 0U,
			.flags = CAN_FILTER_IDE,
		},
	};
	can_mode_t caps;
	int err;
	int i;

	k_sem_init(&channel->rx_overflows, 0U, K_SEM_MAX_LIMIT);

	if (!device_is_ready(can_dev)) {
		LOG_ERR("channel %d CAN device not ready", ch);
		return -ENODEV;
	}

	err = can_get_capabilities(can_dev, &caps);
	if (err != 0U) {
		LOG_ERR("failed to get capabilities for channel %u (err %d)", ch, err);
		return err;
	}

	for (i = 0; i < ARRAY_SIZE(filters); i++) {
		err = can_add_rx_filter(can_dev, gs_usb_can_rx_callback, channel, &filters[i]);
		if (err < 0U) {
			LOG_ERR("failed to add filter %d to channel %d (err %d)", i, ch, err);
			return err;
		}
	}

	can_set_state_change_callback(can_dev, gs_usb_can_state_change_callback, channel);

	channel->ch = ch;
	channel->dev = can_dev;
	channel->features = common_features;
	channel->features |= gs_usb_features_from_capabilities(caps);

	LOG_DBG("channel %d features = 0x%08x", ch, channel->features);

	return 0;
}

int gs_usb_register(const struct device *dev, const struct device **channels, size_t nchannels,
		    const struct gs_usb_ops *ops, void *user_data)
{
	struct gs_usb_data *data = dev->data;
	uint32_t common_features = GS_USB_CAN_FEATURE_GET_STATE;
	int err;
	int ch;

	if (nchannels < 1U || nchannels > ARRAY_SIZE(data->channels)) {
		LOG_ERR("unsupported number of CAN channels %u", nchannels);
		return -ENOTSUP;
	}

	if (ops != NULL) {
		data->ops = *ops;
	}

	data->nchannels = nchannels;
	data->user_data = user_data;

	common_features |= gs_usb_features_from_ops(&data->ops);

	for (ch = 0; ch < nchannels; ch++) {
		err = gs_usb_register_channel(&data->channels[ch], ch, channels[ch],
					      common_features);
		if (err != 0U) {
			LOG_ERR("failed to register channel %d (err %d)", ch, err);
			return err;
		}
	}

	return 0;
}

static void *gs_usb_get_desc(struct usbd_class_data *const c_data, const enum usbd_speed speed)
{
	const struct device *dev = usbd_class_get_private(c_data);
	struct gs_usb_data *data = dev->data;

	if (speed == USBD_SPEED_HS) {
		return data->hs_desc;
	}

	return data->fs_desc;
}

static int gs_usb_preinit(const struct device *dev)
{
	struct gs_usb_data *data = dev->data;

	data->dev = dev;

	k_fifo_init(&data->rx_fifo);
	k_fifo_init(&data->tx_fifo);

	k_thread_create(&data->rx_thread, data->rx_stack, K_KERNEL_STACK_SIZEOF(data->rx_stack),
			gs_usb_rx_thread, (void *)dev, NULL, NULL,
			CONFIG_USBD_GS_USB_RX_THREAD_PRIO, 0, K_NO_WAIT);
	k_thread_name_set(&data->rx_thread, "gs_usb_rx");

	k_thread_create(&data->tx_thread, data->tx_stack, K_KERNEL_STACK_SIZEOF(data->tx_stack),
			gs_usb_tx_thread, (void *)dev, NULL, NULL,
			CONFIG_USBD_GS_USB_TX_THREAD_PRIO, 0, K_NO_WAIT);
	k_thread_name_set(&data->tx_thread, "gs_usb_tx");

	return 0;
}

static int gs_usb_init(struct usbd_class_data *c_data)
{
	const struct device *dev = usbd_class_get_private(c_data);
	struct gs_usb_data *data = dev->data;
	struct gs_usb_desc *desc = data->desc;

	desc->iad.bFirstInterface = desc->if0.bInterfaceNumber;

	LOG_DBG("initialized class instance %p, interface number %u", c_data,
		desc->iad.bFirstInterface);

	return 0;
}

/* clang-format off */
static const struct usbd_cctx_vendor_req gs_usb_vendor_requests =
	USBD_VENDOR_REQ(GS_USB_REQUEST_HOST_FORMAT,
			GS_USB_REQUEST_BITTIMING,
			GS_USB_REQUEST_MODE,
			GS_USB_REQUEST_BT_CONST,
			GS_USB_REQUEST_DEVICE_CONFIG,
#ifdef CONFIG_USBD_GS_USB_TIMESTAMP
			GS_USB_REQUEST_TIMESTAMP,
#endif /* CONFIG_USBD_GS_USB_TIMESTAMP */
			GS_USB_REQUEST_IDENTIFY,
			GS_USB_REQUEST_DATA_BITTIMING,
			GS_USB_REQUEST_BT_CONST_EXT,
#ifdef CONFIG_USBD_GS_USB_TERMINATION
			GS_USB_REQUEST_SET_TERMINATION,
			GS_USB_REQUEST_GET_TERMINATION,
#endif /* CONFIG_USBD_GS_USB_TERMINATION */
			GS_USB_REQUEST_GET_STATE);
/* clang-format on */

struct usbd_class_api gs_usb_api = {
	.control_to_dev = gs_usb_control_to_dev,
	.control_to_host = gs_usb_control_to_host,
	.request = gs_usb_request,
#ifdef CONFIG_USBD_GS_USB_TIMESTAMP_SOF
	.sof = gs_usb_sof,
#endif /* CONFIG_USBD_GS_USB_TIMESTAMP_SOF */
	.enable = gs_usb_enable,
	.disable = gs_usb_disable,
	.get_desc = gs_usb_get_desc,
	.init = gs_usb_init,
};

#define GS_USB_DEFINE_DESCRIPTOR(n)                                                                \
	static struct gs_usb_desc gs_usb_desc_##n = {                                              \
		.iad = {                                                                           \
				.bLength = sizeof(struct usb_association_descriptor),              \
				.bDescriptorType = USB_DESC_INTERFACE_ASSOC,                       \
				.bFirstInterface = 0,                                              \
				.bInterfaceCount = 1,                                              \
				.bFunctionClass = USB_BCC_VENDOR,                                  \
				.bFunctionSubClass = 0,                                            \
				.bFunctionProtocol = 0,                                            \
				.iFunction = 0,                                                    \
		},                                                                                 \
		.if0 = {                                                                           \
				.bLength = sizeof(struct usb_if_descriptor),                       \
				.bDescriptorType = USB_DESC_INTERFACE,                             \
				.bInterfaceNumber = 0,                                             \
				.bAlternateSetting = 0,                                            \
				.bNumEndpoints = 3,                                                \
				.bInterfaceClass = USB_BCC_VENDOR,                                 \
				.bInterfaceSubClass = 0,                                           \
				.bInterfaceProtocol = 0,                                           \
				.iInterface = 0,                                                   \
		},                                                                                 \
		.if0_in_ep = {                                                                     \
				.bLength = sizeof(struct usb_ep_descriptor),                       \
				.bDescriptorType = USB_DESC_ENDPOINT,                              \
				.bEndpointAddress = GS_USB_IN_EP_ADDR,                             \
				.bmAttributes = USB_EP_TYPE_BULK,                                  \
				.wMaxPacketSize = sys_cpu_to_le16(64),                             \
				.bInterval = 0x00,                                                 \
		},                                                                                 \
		.if0_dummy_ep = {                                                                  \
				.bLength = sizeof(struct usb_ep_descriptor),                       \
				.bDescriptorType = USB_DESC_ENDPOINT,                              \
				.bEndpointAddress = GS_USB_DUMMY_EP_ADDR,                          \
				.bmAttributes = USB_EP_TYPE_BULK,                                  \
				.wMaxPacketSize = sys_cpu_to_le16(64U),                            \
				.bInterval = 0x00,                                                 \
		},                                                                                 \
		.if0_out_ep = {                                                                    \
				.bLength = sizeof(struct usb_ep_descriptor),                       \
				.bDescriptorType = USB_DESC_ENDPOINT,                              \
				.bEndpointAddress = GS_USB_OUT_EP_ADDR,                            \
				.bmAttributes = USB_EP_TYPE_BULK,                                  \
				.wMaxPacketSize = sys_cpu_to_le16(64U),                            \
				.bInterval = 0x00,                                                 \
		},                                                                                 \
		.if0_hs_in_ep = {                                                                  \
				.bLength = sizeof(struct usb_ep_descriptor),                       \
				.bDescriptorType = USB_DESC_ENDPOINT,                              \
				.bEndpointAddress = GS_USB_IN_EP_ADDR,                             \
				.bmAttributes = USB_EP_TYPE_BULK,                                  \
				.wMaxPacketSize = sys_cpu_to_le16(512),                            \
				.bInterval = 0x00,                                                 \
		},                                                                                 \
		.if0_hs_dummy_ep = {                                                               \
				.bLength = sizeof(struct usb_ep_descriptor),                       \
				.bDescriptorType = USB_DESC_ENDPOINT,                              \
				.bEndpointAddress = GS_USB_DUMMY_EP_ADDR,                          \
				.bmAttributes = USB_EP_TYPE_BULK,                                  \
				.wMaxPacketSize = sys_cpu_to_le16(512U),                           \
				.bInterval = 0x00,                                                 \
		},                                                                                 \
		.if0_hs_out_ep = {                                                                 \
				.bLength = sizeof(struct usb_ep_descriptor),                       \
				.bDescriptorType = USB_DESC_ENDPOINT,                              \
				.bEndpointAddress = GS_USB_OUT_EP_ADDR,                            \
				.bmAttributes = USB_EP_TYPE_BULK,                                  \
				.wMaxPacketSize = sys_cpu_to_le16(512U),                           \
				.bInterval = 0x00,                                                 \
		},                                                                                 \
		.nil_desc = {                                                                      \
				.bLength = 0,                                                      \
				.bDescriptorType = 0,                                              \
		},                                                                                 \
	};                                                                                         \
                                                                                                   \
	const static struct usb_desc_header *gs_usb_fs_desc_##n[] = {                              \
		(struct usb_desc_header *)&gs_usb_desc_##n.iad,                                    \
		(struct usb_desc_header *)&gs_usb_desc_##n.if0,                                    \
		(struct usb_desc_header *)&gs_usb_desc_##n.if0_in_ep,                              \
		(struct usb_desc_header *)&gs_usb_desc_##n.if0_dummy_ep,                           \
		(struct usb_desc_header *)&gs_usb_desc_##n.if0_out_ep,                             \
		(struct usb_desc_header *)&gs_usb_desc_##n.nil_desc,                               \
	};                                                                                         \
                                                                                                   \
	const static struct usb_desc_header *gs_usb_hs_desc_##n[] = {                              \
		(struct usb_desc_header *)&gs_usb_desc_##n.iad,                                    \
		(struct usb_desc_header *)&gs_usb_desc_##n.if0,                                    \
		(struct usb_desc_header *)&gs_usb_desc_##n.if0_hs_in_ep,                           \
		(struct usb_desc_header *)&gs_usb_desc_##n.if0_hs_dummy_ep,                        \
		(struct usb_desc_header *)&gs_usb_desc_##n.if0_hs_out_ep,                          \
		(struct usb_desc_header *)&gs_usb_desc_##n.nil_desc,                               \
	}

#define USBD_GS_USB_DT_DEVICE_DEFINE(n)                                                            \
	BUILD_ASSERT(DT_INST_ON_BUS(n, usb),                                                       \
		     "node " DT_NODE_PATH(                                                         \
			     DT_DRV_INST(n)) " is not assigned to a USB device controller");       \
                                                                                                   \
	GS_USB_DEFINE_DESCRIPTOR(n);                                                               \
                                                                                                   \
	USBD_DEFINE_CLASS(gs_usb_##n, &gs_usb_api, (void *)DEVICE_DT_GET(DT_DRV_INST(n)),          \
			  &gs_usb_vendor_requests);                                                \
                                                                                                   \
	NET_BUF_POOL_FIXED_DEFINE(gs_usb_pool_##n, CONFIG_USBD_GS_USB_POOL_SIZE,                   \
				  GS_USB_HOST_FRAME_MAX_SIZE, sizeof(struct udc_buf_info), NULL);  \
                                                                                                   \
	static struct gs_usb_data gs_usb_data_##n = {                                              \
		.desc = &gs_usb_desc_##n,                                                          \
		.fs_desc = gs_usb_fs_desc_##n,                                                     \
		.hs_desc = gs_usb_hs_desc_##n,                                                     \
		.c_data = &gs_usb_##n,                                                             \
		.pool = &gs_usb_pool_##n,                                                          \
		.in_sem = Z_SEM_INITIALIZER(gs_usb_data_##n.in_sem, 0, 1),                         \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, gs_usb_preinit, NULL, &gs_usb_data_##n, NULL, POST_KERNEL,        \
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &gs_usb_api);

DT_INST_FOREACH_STATUS_OKAY(USBD_GS_USB_DT_DEVICE_DEFINE);
