/*
 * Copyright (c) 2022-2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/buf.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/usb/msos_desc.h>
#include <zephyr/usb/usb_device.h>

#include <usb_descriptor.h>

#include <cannectivity/usb/class/gs_usb.h>

LOG_MODULE_REGISTER(gs_usb, CONFIG_USB_DEVICE_GS_USB_LOG_LEVEL);

#define DT_DRV_COMPAT gs_usb

/* USB endpoint indexes */
#define GS_USB_IN_EP_IDX    0U
#define GS_USB_DUMMY_EP_IDX 1U
#define GS_USB_OUT_EP_IDX   2U

struct gs_usb_config {
	struct usb_association_descriptor iad;
	struct usb_if_descriptor if0;
	struct usb_ep_descriptor if0_in_ep;
	struct usb_ep_descriptor if0_dummy_ep;
	struct usb_ep_descriptor if0_out_ep;
} __packed;

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
	struct usb_dev_data common;
	struct gs_usb_channel_data channels[CONFIG_USB_DEVICE_GS_USB_MAX_CHANNELS];
	size_t nchannels;
	struct gs_usb_ops ops;
	void *user_data;
	struct net_buf_pool *pool;

#ifdef CONFIG_USB_DEVICE_GS_USB_TIMESTAMP
	uint32_t timestamp;
	bool sof_seen;
#endif /* CONFIG_USB_DEVICE_GS_USB_TIMESTAMP */

	struct k_fifo rx_fifo;
	struct k_thread rx_thread;

	uint8_t tx_buffer[GS_USB_HOST_FRAME_MAX_SIZE];
	struct k_fifo tx_fifo;
	struct k_thread tx_thread;

	K_KERNEL_STACK_MEMBER(rx_stack, CONFIG_USB_DEVICE_GS_USB_RX_THREAD_STACK_SIZE);
	K_KERNEL_STACK_MEMBER(tx_stack, CONFIG_USB_DEVICE_GS_USB_TX_THREAD_STACK_SIZE);
};

static sys_slist_t gs_usb_data_devlist;
static gs_usb_vendorcode_callback_t vendorcode_callback;

static void gs_usb_transfer_tx_prepare(const struct device *dev);
static int gs_usb_reset_channel(const struct device *dev, uint16_t ch);

static int gs_usb_request_host_format(int32_t tlen, const uint8_t *tdata)
{
	struct gs_usb_host_config *hc = (struct gs_usb_host_config *)tdata;
	uint32_t byte_order;

	if (tlen != sizeof(*hc)) {
		LOG_ERR("invalid length for host format request (%d)", tlen);
		return -EINVAL;
	}

	byte_order = sys_le32_to_cpu(hc->byte_order);

	if (byte_order != GS_USB_HOST_FORMAT_LITTLE_ENDIAN) {
		LOG_ERR("unsupported host byte order (0x%08x)", byte_order);
		return -ENOTSUP;
	}

	return 0;
}

static int gs_usb_request_bt_const(const struct device *dev, uint16_t ch, int32_t *tlen,
				   uint8_t **tdata)
{
	struct gs_usb_device_bt_const *bt_const = (struct gs_usb_device_bt_const *)*tdata;
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
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

	bt_const->feature = sys_cpu_to_le32(channel->features);
	bt_const->fclk_can = sys_cpu_to_le32(rate);
	bt_const->tseg1_min = sys_cpu_to_le32(min->prop_seg + min->phase_seg1);
	bt_const->tseg1_max = sys_cpu_to_le32(max->prop_seg + max->phase_seg1);
	bt_const->tseg2_min = sys_cpu_to_le32(min->phase_seg2);
	bt_const->tseg2_max = sys_cpu_to_le32(max->phase_seg1);
	bt_const->sjw_max = sys_cpu_to_le32(max->sjw);
	bt_const->brp_min = sys_cpu_to_le32(min->prescaler);
	bt_const->brp_max = sys_cpu_to_le32(max->prescaler);
	bt_const->brp_inc = 1U;

	*tlen = sizeof(*bt_const);

	return 0;
}

static int gs_usb_request_bt_const_ext(const struct device *dev, uint16_t ch, int32_t *tlen,
				       uint8_t **tdata)
{
#ifdef CONFIG_CAN_FD_MODE
	struct gs_usb_device_bt_const_ext *bt_const_ext =
		(struct gs_usb_device_bt_const_ext *)*tdata;
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
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

	bt_const_ext->feature = sys_cpu_to_le32(channel->features);
	bt_const_ext->fclk_can = sys_cpu_to_le32(rate);

	bt_const_ext->tseg1_min = sys_cpu_to_le32(min->prop_seg + min->phase_seg1);
	bt_const_ext->tseg1_max = sys_cpu_to_le32(max->prop_seg + max->phase_seg1);
	bt_const_ext->tseg2_min = sys_cpu_to_le32(min->phase_seg2);
	bt_const_ext->tseg2_max = sys_cpu_to_le32(max->phase_seg1);
	bt_const_ext->sjw_max = sys_cpu_to_le32(max->sjw);
	bt_const_ext->brp_min = sys_cpu_to_le32(min->prescaler);
	bt_const_ext->brp_max = sys_cpu_to_le32(max->prescaler);
	bt_const_ext->brp_inc = 1U;

	min = can_get_timing_data_min(channel->dev);
	max = can_get_timing_data_max(channel->dev);

	if (min == NULL || max == NULL) {
		LOG_ERR("failed to get min/max data phase timing for channel %u", ch);
		return -ENOTSUP;
	};

	bt_const_ext->dtseg1_min = sys_cpu_to_le32(min->prop_seg + min->phase_seg1);
	bt_const_ext->dtseg1_max = sys_cpu_to_le32(max->prop_seg + max->phase_seg1);
	bt_const_ext->dtseg2_min = sys_cpu_to_le32(min->phase_seg2);
	bt_const_ext->dtseg2_max = sys_cpu_to_le32(max->phase_seg1);
	bt_const_ext->dsjw_max = sys_cpu_to_le32(max->sjw);
	bt_const_ext->dbrp_min = sys_cpu_to_le32(min->prescaler);
	bt_const_ext->dbrp_max = sys_cpu_to_le32(max->prescaler);
	bt_const_ext->dbrp_inc = 1U;

	*tlen = sizeof(*bt_const_ext);

	return 0;
#else /* CONFIG_CAN_FD_MODE */
	return -ENOTSUP;
#endif /* !CONFIG_CAN_FD_MODE */
}

static int gs_usb_request_get_termination(const struct device *dev, uint16_t ch, int32_t *tlen,
					  uint8_t **tdata)
{
#ifdef CONFIG_USB_DEVICE_GS_USB_TERMINATION
	struct gs_usb_device_termination_state *ts =
		(struct gs_usb_device_termination_state *)*tdata;
	struct gs_usb_data *data = dev->data;
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
		ts->state = sys_cpu_to_le32(GS_USB_CHANNEL_TERMINATION_STATE_ON);
	} else {
		ts->state = sys_cpu_to_le32(GS_USB_CHANNEL_TERMINATION_STATE_OFF);
	}

	*tlen = sizeof(*ts);

	return 0;
#else /* CONFIG_USB_DEVICE_GS_USB_TERMINATION */
	return -ENOTSUP;
#endif /* !CONFIG_USB_DEVICE_GS_USB_TERMINATION */
}

static int gs_usb_request_set_termination(const struct device *dev, uint16_t ch, int32_t tlen,
					  uint8_t *tdata)
{
#ifdef CONFIG_USB_DEVICE_GS_USB_TERMINATION
	struct gs_usb_device_termination_state *ts =
		(struct gs_usb_device_termination_state *)tdata;
	struct gs_usb_data *data = dev->data;
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

	if (tlen != sizeof(*ts)) {
		LOG_ERR("invalid length for set termination request (%d)", tlen);
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
#else /* CONFIG_USB_DEVICE_GS_USB_TERMINATION */
	return -ENOTSUP;
#endif /* !CONFIG_USB_DEVICE_GS_USB_TERMINATION */
}

static int gs_usb_request_get_state(const struct device *dev, uint16_t ch, int32_t *tlen,
				    uint8_t **tdata)
{
	struct gs_usb_device_state *ds = (struct gs_usb_device_state *)*tdata;
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
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

	ds->rxerr = sys_cpu_to_le32(err_cnt.rx_err_cnt);
	ds->txerr = sys_cpu_to_le32(err_cnt.tx_err_cnt);

	switch (state) {
	case CAN_STATE_ERROR_ACTIVE:
		ds->state = sys_cpu_to_le32(GS_USB_CHANNEL_STATE_ERROR_ACTIVE);
		break;
	case CAN_STATE_ERROR_WARNING:
		ds->state = sys_cpu_to_le32(GS_USB_CHANNEL_STATE_ERROR_WARNING);
		break;
	case CAN_STATE_ERROR_PASSIVE:
		ds->state = sys_cpu_to_le32(GS_USB_CHANNEL_STATE_ERROR_PASSIVE);
		break;
	case CAN_STATE_BUS_OFF:
		ds->state = sys_cpu_to_le32(GS_USB_CHANNEL_STATE_BUS_OFF);
		break;
	case CAN_STATE_STOPPED:
		ds->state = sys_cpu_to_le32(GS_USB_CHANNEL_STATE_STOPPED);
		break;
	default:
		LOG_ERR("unsupported state %d for channel %u", state, ch);
		return -ENOTSUP;
	}

	*tlen = sizeof(*ds);

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

static int gs_usb_request_bittiming(const struct device *dev, uint16_t ch, int32_t tlen,
				    uint8_t *tdata)
{
	struct gs_usb_device_bittiming *dbtp = (struct gs_usb_device_bittiming *)tdata;
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

	if (tlen != sizeof(*dbtp)) {
		LOG_ERR("invalid length for bittiming request (%d)", tlen);
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

static int gs_usb_request_data_bittiming(const struct device *dev, uint16_t ch, int32_t tlen,
					 uint8_t *tdata)
{
#ifdef CONFIG_CAN_FD_MODE
	struct gs_usb_device_bittiming *dbtp = (struct gs_usb_device_bittiming *)tdata;
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

	if (tlen != sizeof(*dbtp)) {
		LOG_ERR("invalid length for data_bittiming request (%d)", tlen);
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

static int gs_usb_request_mode(const struct device *dev, uint16_t ch, int32_t tlen, uint8_t *tdata)
{
	struct gs_usb_device_mode *dm = (struct gs_usb_device_mode *)tdata;
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
	can_mode_t mode = CAN_MODE_NORMAL;
	uint32_t flags;
	int err;

	if (ch >= data->nchannels) {
		LOG_ERR("mode request for non-existing channel %u", ch);
		return -EINVAL;
	}

	if (tlen != sizeof(*dm)) {
		LOG_ERR("invalid length for mode request (%d)", tlen);
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

static int gs_usb_request_identify(const struct device *dev, uint16_t ch, int32_t tlen,
				   uint8_t *tdata)
{
	struct gs_usb_identify_mode *im = (struct gs_usb_identify_mode *)tdata;
	struct gs_usb_data *data = dev->data;
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

	if (tlen != sizeof(*im)) {
		LOG_ERR("invalid length for identify request (%d)", tlen);
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

static int gs_usb_request_device_config(const struct device *dev, int32_t *tlen, uint8_t **tdata)
{
	struct gs_usb_device_config *dc = (struct gs_usb_device_config *)*tdata;
	struct gs_usb_data *data = dev->data;

	memset(dc, 0, sizeof(*dc));
	dc->nchannels = data->nchannels - 1U; /* 8 bit representing 1 to 256 */
	dc->sw_version = sys_cpu_to_le32(GS_USB_SW_VERSION);
	dc->hw_version = sys_cpu_to_le32(GS_USB_HW_VERSION);

	*tlen = sizeof(*dc);

	return 0;
}

static int gs_usb_request_timestamp(const struct device *dev, int32_t *tlen, uint8_t **tdata)
{
#ifdef CONFIG_USB_DEVICE_GS_USB_TIMESTAMP
	struct gs_usb_data *data = dev->data;
	uint32_t timestamp;
	int err;

	if (data->ops.timestamp == NULL) {
		LOG_ERR("timestamp not supported");
		return -ENOTSUP;
	}

	if (data->sof_seen) {
		timestamp = data->timestamp;
		data->sof_seen = false;
	} else {
		err = data->ops.timestamp(dev, &timestamp, data->user_data);
		if (err != 0) {
			LOG_ERR("failed to get current timestamp (err %d)", err);
			return err;
		}
		LOG_WRN_ONCE("USB SoF event not supported, timestamp will be less accurate");
	}

	LOG_DBG("timestamp: 0x%08x", timestamp);

	*tlen = sizeof(timestamp);

	return 0;
#else /* CONFIG_USB_DEVICE_GS_USB_TIMESTAMP */
	return -ENOTSUP;
#endif /* !CONFIG_USB_DEVICE_GS_USB_TIMESTAMP */
}

void gs_usb_register_vendorcode_callback(gs_usb_vendorcode_callback_t callback)
{
	vendorcode_callback = callback;
}

static int gs_usb_vendor_request_handler(struct usb_setup_packet *setup, int32_t *tlen,
					 uint8_t **tdata)
{
	struct usb_dev_data *common;
	const struct device *dev;
	uint16_t ch;

	switch (setup->RequestType.recipient) {
	case USB_REQTYPE_RECIPIENT_DEVICE:
		if (usb_reqtype_is_to_host(setup)) {
			if (setup->bRequest == GS_USB_MS_VENDORCODE &&
			    setup->wIndex == MS_OS_20_DESCRIPTOR_INDEX) {
				if (vendorcode_callback != NULL) {
					return vendorcode_callback(tlen, tdata);
				}

				return -ENOTSUP;
			}
		}
		break;
	case USB_REQTYPE_RECIPIENT_INTERFACE:
		common = usb_get_dev_data_by_iface(&gs_usb_data_devlist, (uint8_t)setup->wIndex);
		if (common == NULL) {
			LOG_ERR("device data not found for interface %u", setup->wIndex);
			return -ENODEV;
		}

		dev = common->dev;
		ch = setup->wValue;

		if (usb_reqtype_is_to_host(setup)) {
			/* Interface to host */
			switch (setup->bRequest) {
			case GS_USB_REQUEST_BERR:
				/* Not supported */
				break;
			case GS_USB_REQUEST_BT_CONST:
				return gs_usb_request_bt_const(dev, ch, tlen, tdata);
			case GS_USB_REQUEST_DEVICE_CONFIG:
				return gs_usb_request_device_config(dev, tlen, tdata);
			case GS_USB_REQUEST_TIMESTAMP:
				return gs_usb_request_timestamp(dev, tlen, tdata);
			case GS_USB_REQUEST_GET_USER_ID:
				/* Not supported */
				break;
			case GS_USB_REQUEST_BT_CONST_EXT:
				return gs_usb_request_bt_const_ext(dev, ch, tlen, tdata);
			case GS_USB_REQUEST_GET_TERMINATION:
				return gs_usb_request_get_termination(dev, ch, tlen, tdata);
			case GS_USB_REQUEST_GET_STATE:
				return gs_usb_request_get_state(dev, ch, tlen, tdata);
			default:
				break;
			}
		} else {
			/* Host to interface */
			switch (setup->bRequest) {
			case GS_USB_REQUEST_HOST_FORMAT:
				return gs_usb_request_host_format(*tlen, *tdata);
			case GS_USB_REQUEST_BITTIMING:
				return gs_usb_request_bittiming(dev, ch, *tlen, *tdata);
			case GS_USB_REQUEST_MODE:
				return gs_usb_request_mode(dev, ch, *tlen, *tdata);
			case GS_USB_REQUEST_IDENTIFY:
				return gs_usb_request_identify(dev, ch, *tlen, *tdata);
			case GS_USB_REQUEST_DATA_BITTIMING:
				return gs_usb_request_data_bittiming(dev, ch, *tlen, *tdata);
			case GS_USB_REQUEST_SET_USER_ID:
				/* Not supported */
				break;
			case GS_USB_REQUEST_SET_TERMINATION:
				return gs_usb_request_set_termination(dev, ch, *tlen, *tdata);
			default:
				break;
			}
		}
		break;
	default:
		break;
	}

	LOG_ERR("bmRequestType 0x%02x bRequest 0x%02x not supported", setup->bmRequestType,
		setup->bRequest);

	return -ENOTSUP;
}

static void gs_usb_fill_echo_frame_hdr(struct net_buf *buf, uint32_t echo_id, uint8_t channel,
				       uint8_t flags)
{
	struct gs_usb_host_frame_hdr hdr = {0};

	__ASSERT_NO_MSG(buf != NULL);

	hdr.echo_id = sys_cpu_to_le32(echo_id);
	hdr.channel = channel;
	hdr.flags = flags;

	net_buf_reset(buf);
	net_buf_add_mem(buf, &hdr, sizeof(hdr));
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
		err = data->ops.timestamp(data->common->dev, &timestamp, data->user_data);
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

	__ASSERT_NO_MSG(can_dev == channel->dev);

#ifdef CONFIG_USBD_GS_USB_TIMESTAMP
	uint32_t timestamp = 0U;
	int err;

	if ((channel->mode & GS_USB_CAN_MODE_HW_TIMESTAMP) != 0U) {
		err = data->ops.timestamp(data->common->dev, &timestamp, data->user_data);
		if (err != 0) {
			LOG_ERR("failed to get RX timestamp (err %d)", err);
		}
	}
#endif /* CONFIG_USBD_GS_USB_TIMESTAMP */

	buf = net_buf_alloc_fixed(data->pool, K_NO_WAIT);
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
	struct usb_cfg_data *cfg = (void *)dev->config;
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
	struct gs_usb_host_frame_hdr *hdr;
	struct net_buf *buf;
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

		usb_transfer_sync(cfg->endpoint[GS_USB_IN_EP_IDX].ep_addr, buf->data,
				  buf->len, USB_TRANS_WRITE);

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

static void gs_usb_transfer_tx_callback(uint8_t ep, int tsize, void *priv)
{
	const struct device *dev = priv;
	struct gs_usb_data *data = dev->data;
	struct net_buf *buf;

	if (tsize > 0) {
		buf = net_buf_alloc_len(data->pool, tsize, K_NO_WAIT);
		if (buf == NULL) {
			LOG_ERR("failed to allocate TX buffer");
			return;
		}

		net_buf_add_mem(buf, data->tx_buffer, tsize);
		k_fifo_put(&data->tx_fifo, buf);
	}

	gs_usb_transfer_tx_prepare(dev);
}

static void gs_usb_transfer_tx_prepare(const struct device *dev)
{
	struct usb_cfg_data *cfg = (void *)dev->config;
	struct gs_usb_data *data = dev->data;

	usb_transfer(cfg->endpoint[GS_USB_OUT_EP_IDX].ep_addr, data->tx_buffer,
		     sizeof(data->tx_buffer), USB_TRANS_READ, gs_usb_transfer_tx_callback,
		     (void *)dev);
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

		gs_usb_fill_echo_frame_hdr(buf, sys_le32_to_cpu(hdr->echo_id), hdr->channel,
					   hdr->flags);

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

static uint32_t gs_usb_features_from_ops(struct gs_usb_ops *ops)
{
	uint32_t features = 0U;

	if (UTIL_AND(IS_ENABLED(CONFIG_USB_DEVICE_GS_USB_TIMESTAMP), ops->timestamp != NULL)) {
		features |= GS_USB_CAN_FEATURE_HW_TIMESTAMP;
	}

	if (ops->identify != NULL) {
		features |= GS_USB_CAN_FEATURE_IDENTIFY;
	}

	if (UTIL_AND(IS_ENABLED(CONFIG_USB_DEVICE_GS_USB_TERMINATION),
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

	sys_slist_append(&gs_usb_data_devlist, &data->common.node);

	return 0;
}

static void gs_usb_status_callback(struct usb_cfg_data *cfg, enum usb_dc_status_code status,
				   const uint8_t *param)
{
	struct usb_dev_data *common;
	struct gs_usb_data *data;
	int ch;

	common = usb_get_dev_data_by_cfg(&gs_usb_data_devlist, cfg);
	if (common == NULL) {
		LOG_ERR("device data not found for cfg %p", cfg);
		return;
	}

	data = CONTAINER_OF(common, struct gs_usb_data, common);

	switch (status) {
	case USB_DC_ERROR:
		LOG_DBG("USB device error");
		break;
	case USB_DC_RESET:
		LOG_DBG("USB device reset");
		break;
	case USB_DC_CONNECTED:
		LOG_DBG("USB device connected");
		break;
	case USB_DC_CONFIGURED:
		LOG_DBG("USB device configured");
		LOG_DBG("EP IN addr = 0x%02x", cfg->endpoint[GS_USB_IN_EP_IDX].ep_addr);
		LOG_DBG("EP DUMMY addr = 0x%02x", cfg->endpoint[GS_USB_DUMMY_EP_IDX].ep_addr);
		LOG_DBG("EP OUT addr = 0x%02x", cfg->endpoint[GS_USB_OUT_EP_IDX].ep_addr);
		gs_usb_transfer_tx_prepare(common->dev);
		break;
	case USB_DC_DISCONNECTED:
		LOG_DBG("USB device disconnected");
		for (ch = 0; ch < data->nchannels; ch++) {
			(void)gs_usb_reset_channel(common->dev, ch);
		}

		usb_cancel_transfer(cfg->endpoint[GS_USB_IN_EP_IDX].ep_addr);
		usb_cancel_transfer(cfg->endpoint[GS_USB_OUT_EP_IDX].ep_addr);
		break;
	case USB_DC_SUSPEND:
		LOG_DBG("USB device suspend");
		break;
	case USB_DC_RESUME:
		LOG_DBG("USB device resume");
		break;
	case USB_DC_INTERFACE:
		LOG_DBG("USB device interface selected");
		break;
	case USB_DC_SET_HALT:
		LOG_DBG("USB device set halt");
		break;
	case USB_DC_CLEAR_HALT:
		LOG_DBG("USB device clear halt");
		break;
	case USB_DC_SOF:
#ifdef CONFIG_USB_DEVICE_GS_USB_TIMESTAMP
		int err;

		if (data->ops.timestamp != NULL) {
			err = data->ops.timestamp(common->dev, &data->timestamp, data->user_data);
			if (err != 0) {
				LOG_ERR("failed to get current timestamp (err %d)", err);
				break;
			}

			/* Not all USB device controller drivers support SoF events */
			data->sof_seen = true;
		}
#endif /* CONFIG_USB_DEVICE_GS_USB_TIMESTAMP */
		break;
	case USB_DC_UNKNOWN:
		__fallthrough;
	default:
		LOG_ERR("USB device unknown state");
		break;
	}
}

static void gs_usb_interface_config(struct usb_desc_header *head, uint8_t bInterfaceNumber)
{
	struct usb_if_descriptor *if_desc = (struct usb_if_descriptor *)head;
	struct gs_usb_config *desc = CONTAINER_OF(if_desc, struct gs_usb_config, if0);

	LOG_DBG("bInterfaceNumber = %u", bInterfaceNumber);
	desc->iad.bFirstInterface = bInterfaceNumber;
	desc->if0.bInterfaceNumber = bInterfaceNumber;
}

static int gs_usb_init(const struct device *dev)
{
	struct gs_usb_data *data = dev->data;

	data->common.dev = dev;

	k_fifo_init(&data->rx_fifo);
	k_fifo_init(&data->tx_fifo);

	k_thread_create(&data->rx_thread, data->rx_stack, K_KERNEL_STACK_SIZEOF(data->rx_stack),
			gs_usb_rx_thread, (void *)dev, NULL, NULL,
			CONFIG_USB_DEVICE_GS_USB_RX_THREAD_PRIO, 0, K_NO_WAIT);
	k_thread_name_set(&data->rx_thread, "gs_usb_rx");

	k_thread_create(&data->tx_thread, data->tx_stack, K_KERNEL_STACK_SIZEOF(data->tx_stack),
			gs_usb_tx_thread, (void *)dev, NULL, NULL,
			CONFIG_USB_DEVICE_GS_USB_TX_THREAD_PRIO, 0, K_NO_WAIT);
	k_thread_name_set(&data->tx_thread, "gs_usb_tx");

	return 0;
}

#define INITIALIZER_IAD                                                                            \
	{                                                                                          \
		.bLength = sizeof(struct usb_association_descriptor),                              \
		.bDescriptorType = USB_DESC_INTERFACE_ASSOC,                                       \
		.bFirstInterface = 0,                                                              \
		.bInterfaceCount = 0x01,                                                           \
		.bFunctionClass = USB_BCC_VENDOR,                                                  \
		.bFunctionSubClass = 0,                                                            \
		.bFunctionProtocol = 0,                                                            \
		.iFunction = 0,                                                                    \
	}

#define INITIALIZER_IF                                                                             \
	{                                                                                          \
		.bLength = sizeof(struct usb_if_descriptor),                                       \
		.bDescriptorType = USB_DESC_INTERFACE,                                             \
		.bInterfaceNumber = 0,                                                             \
		.bAlternateSetting = 0,                                                            \
		.bNumEndpoints = 3,                                                                \
		.bInterfaceClass = USB_BCC_VENDOR,                                                 \
		.bInterfaceSubClass = 0,                                                           \
		.bInterfaceProtocol = 0,                                                           \
		.iInterface = 0,                                                                   \
	}

#define INITIALIZER_IF_EP(addr)                                                                    \
	{                                                                                          \
		.bLength = sizeof(struct usb_ep_descriptor),                                       \
		.bDescriptorType = USB_DESC_ENDPOINT,                                              \
		.bEndpointAddress = addr,                                                          \
		.bmAttributes = USB_DC_EP_BULK,                                                    \
		.wMaxPacketSize = sys_cpu_to_le16(CONFIG_USB_DEVICE_GS_USB_MAX_PACKET_SIZE),       \
		.bInterval = 0x01,                                                                 \
	}

#define GS_USB_DEVICE_DEFINE(inst)                                                                 \
	BUILD_ASSERT(DT_INST_ON_BUS(inst, usb),                                                    \
		     "node " DT_NODE_PATH(                                                         \
			     DT_DRV_INST(inst)) " is not assigned to a USB device controller");    \
                                                                                                   \
	NET_BUF_POOL_FIXED_DEFINE(gs_usb_pool_##inst, CONFIG_USB_DEVICE_GS_USB_POOL_SIZE,          \
				  GS_USB_HOST_FRAME_MAX_SIZE, 0, NULL);                            \
                                                                                                   \
	USBD_CLASS_DESCR_DEFINE(primary, 0)                                                        \
	struct gs_usb_config gs_usb_config_##inst = {                                              \
		.iad = INITIALIZER_IAD,                                                            \
		.if0 = INITIALIZER_IF,                                                             \
		.if0_in_ep = INITIALIZER_IF_EP(GS_USB_IN_EP_ADDR),                                 \
		.if0_dummy_ep = INITIALIZER_IF_EP(GS_USB_DUMMY_EP_ADDR),                           \
		.if0_out_ep = INITIALIZER_IF_EP(GS_USB_OUT_EP_ADDR),                               \
	};                                                                                         \
                                                                                                   \
	static struct usb_ep_cfg_data gs_usb_ep_cfg_data_##inst[] = {                              \
		{                                                                                  \
			.ep_cb = usb_transfer_ep_callback,                                         \
			.ep_addr = GS_USB_IN_EP_ADDR,                                              \
		},                                                                                 \
		{                                                                                  \
			.ep_cb = usb_transfer_ep_callback,                                         \
			.ep_addr = GS_USB_DUMMY_EP_ADDR,                                           \
		},                                                                                 \
		{                                                                                  \
			.ep_cb = usb_transfer_ep_callback,                                         \
			.ep_addr = GS_USB_OUT_EP_ADDR,                                             \
		},                                                                                 \
	};                                                                                         \
                                                                                                   \
	USBD_DEFINE_CFG_DATA(gs_usb_cfg_##inst) = {                                                \
		.usb_device_description = NULL,                                                    \
		.interface_config = gs_usb_interface_config,                                       \
		.interface_descriptor = &gs_usb_config_##inst.if0,                                 \
		.cb_usb_status = gs_usb_status_callback,                                           \
		.interface = {                                                                     \
			.class_handler = NULL,                                                     \
			.custom_handler = NULL,                                                    \
			.vendor_handler = gs_usb_vendor_request_handler,                           \
		},                                                                                 \
		.num_endpoints = ARRAY_SIZE(gs_usb_ep_cfg_data_##inst),                            \
		.endpoint = gs_usb_ep_cfg_data_##inst,                                             \
	};                                                                                         \
                                                                                                   \
	static struct gs_usb_data gs_usb_data_##inst = {                                           \
		.pool = &gs_usb_pool_##inst,                                                       \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, gs_usb_init, NULL, &gs_usb_data_##inst, &gs_usb_cfg_##inst,    \
			      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, NULL);

DT_INST_FOREACH_STATUS_OKAY(GS_USB_DEVICE_DEFINE);
