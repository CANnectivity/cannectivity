/*
 * Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/fff.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/can/can_fake.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <cannectivity/usb/class/gs_usb.h>

#include "test.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

DEFINE_FFF_GLOBALS;

#define USER_DATA 0x12345678
#define TIMESTAMP 0xdeadbeef

static int fake_can_get_capabilities_delegate(const struct device *dev, can_mode_t *cap)
{
	*cap = CAN_MODE_NORMAL | CAN_MODE_FD;

	return 0;
}

static int fake_can_get_state_delegate(const struct device *dev, enum can_state *state,
				       struct can_bus_err_cnt *err_cnt)
{
	*state = CAN_STATE_ERROR_PASSIVE;

	err_cnt->tx_err_cnt = 128;
	err_cnt->rx_err_cnt = 96;

	return 0;
}

static int fake_can_set_timing_delegate(const struct device *dev, const struct can_timing *timing)
{
	LOG_DBG("%s: sjw = %u, prop_seg = %u, phase_seg1 = %u, phase_seg2 = %u, "
		"prescaler = %u", dev->name, timing->sjw, timing->prop_seg, timing->phase_seg1,
		timing->phase_seg2, timing->prescaler);

	return 0;
}

static int fake_can_set_timing_data_delegate(const struct device *dev,
					     const struct can_timing *timing_data)
{
	LOG_DBG("%s: sjw = %u, prop_seg = %u, phase_seg1 = %u, phase_seg2 = %u, "
		"prescaler = %u", dev->name, timing_data->sjw, timing_data->prop_seg,
		timing_data->phase_seg1, timing_data->phase_seg2, timing_data->prescaler);

	return 0;
}

static int fake_can_start_delegate(const struct device *dev)
{
	LOG_DBG("%s: start", dev->name);

	return 0;
}

static int fake_can_stop_delegate(const struct device *dev)
{
	LOG_DBG("%s: stop", dev->name);

	return 0;
}

static int fake_can_set_mode_delegate(const struct device *dev, can_mode_t mode)
{
	LOG_DBG("%s: mode = 0x%08x", dev->name, mode);

	return 0;
}

static int event_cb(const struct device *dev, uint16_t ch, enum gs_usb_event event, void *user_data)
{
	uint32_t ud = POINTER_TO_UINT(user_data);

	switch (event) {
	case GS_USB_EVENT_CHANNEL_STARTED:
		LOG_DBG("dev = %s, ch = %u, started = 1, user_data = 0x%08x", dev->name, ch, ud);
		break;
	case GS_USB_EVENT_CHANNEL_STOPPED:
		LOG_DBG("dev = %s, ch = %u, started = 0, user_data = 0x%08x", dev->name, ch, ud);
		break;
	case GS_USB_EVENT_CHANNEL_ERROR_ON:
		LOG_DBG("dev = %s, ch = %u, error = 1, user_data = 0x%08x", dev->name, ch, ud);
		break;
	case GS_USB_EVENT_CHANNEL_ERROR_OFF:
		LOG_DBG("dev = %s, ch = %u, error = 0, user_data = 0x%08x", dev->name, ch, ud);
		break;
	case GS_USB_EVENT_CHANNEL_ACTIVITY_RX:
		LOG_DBG("dev = %s, ch = %u, rx activity = 1, user_data = 0x%08x", dev->name, ch,
			ud);
		break;
	case GS_USB_EVENT_CHANNEL_ACTIVITY_TX:
		LOG_DBG("dev = %s, ch = %u, tx activity = 1, user_data = 0x%08x", dev->name, ch,
			ud);
		break;
	case GS_USB_EVENT_CHANNEL_IDENTIFY_ON:
		LOG_DBG("dev = %s, ch = %u, identify = 1, user_data = 0x%08x", dev->name, ch, ud);
		break;
	case GS_USB_EVENT_CHANNEL_IDENTIFY_OFF:
		LOG_DBG("dev = %s, ch = %u, identify = 0, user_data = 0x%08x", dev->name, ch, ud);
		break;
	}

	return 0;
}

static int set_termination_cb(const struct device *dev, uint16_t ch, bool terminate,
			      void *user_data)
{
	uint32_t ud = POINTER_TO_UINT(user_data);

	LOG_DBG("dev = %s, ch = %u, terminate = %u, user_data = 0x%08x", dev->name, ch, terminate,
		ud);

	return 0;
}

static int get_termination_cb(const struct device *dev, uint16_t ch, bool *terminated,
			      void *user_data)
{
	uint32_t ud = POINTER_TO_UINT(user_data);

	*terminated = (ch % 2 == 0) ? true : false;
	LOG_DBG("dev = %s, ch = %u, terminated = %u, user_data = 0x%08x", dev->name, ch,
		*terminated, ud);

	return 0;
}

static int timestamp_get_cb(const struct device *dev, uint32_t *timestamp, void *user_data)
{
	uint32_t ud = POINTER_TO_UINT(user_data);

	*timestamp = TIMESTAMP;
	LOG_DBG("dev = %s, timestamp = 0x%08x, user_data = 0x%08x", dev->name, *timestamp, ud);

	return 0;
}

static const struct gs_usb_ops gs_usb_ops = {
	.timestamp = timestamp_get_cb,
	.event = event_cb,
	.set_termination = set_termination_cb,
	.get_termination = get_termination_cb,
};

int main(void)
{
	const struct device *gs_usb = DEVICE_DT_GET(DT_NODELABEL(gs_usb0));
	const struct device *channels[] = {
		DEVICE_DT_GET(DT_NODELABEL(fake_can0)),
		DEVICE_DT_GET(DT_NODELABEL(fake_can1)),
		DEVICE_DT_GET(DT_NODELABEL(can_loopback0)),
		DEVICE_DT_GET(DT_NODELABEL(can_loopback1)),
	};
	int err;
	int i;

	fake_can_get_capabilities_fake.custom_fake = fake_can_get_capabilities_delegate;
	fake_can_get_state_fake.custom_fake = fake_can_get_state_delegate;
	fake_can_set_timing_fake.custom_fake = fake_can_set_timing_delegate;
	fake_can_set_timing_data_fake.custom_fake = fake_can_set_timing_data_delegate;
	fake_can_start_fake.custom_fake = fake_can_start_delegate;
	fake_can_stop_fake.custom_fake = fake_can_stop_delegate;
	fake_can_set_mode_fake.custom_fake = fake_can_set_mode_delegate;

	if (!device_is_ready(gs_usb)) {
		LOG_ERR("gs_usb USB device not ready");
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(channels); i++) {
		if (!device_is_ready(channels[i])) {
			LOG_ERR("CAN controller channel %d not ready", i);
			return 0;
		}
	}

	err = gs_usb_register(gs_usb, channels, ARRAY_SIZE(channels), &gs_usb_ops,
			      UINT_TO_POINTER(USER_DATA));
	if (err != 0U) {
		LOG_ERR("failed to register gs_usb (err %d)", err);
		return 0;
	}

	err = test_usb_init();
	if (err != 0) {
		LOG_ERR("failed to initialize USB (err %d)", err);
		return 0;
	}
}
