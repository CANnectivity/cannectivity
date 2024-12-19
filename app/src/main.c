/*
 * Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usbd.h>

#include <cannectivity/usb/class/gs_usb.h>

#include "cannectivity.h"
#include "zephyr/app_version.h"

LOG_MODULE_REGISTER(main, CONFIG_CANNECTIVITY_LOG_LEVEL);

#ifdef CONFIG_CANNECTIVITY_BOOT_BANNER
#if defined(APP_BUILD_VERSION) && !IS_EMPTY(APP_BUILD_VERSION)
#define CANNECTIVITY_BANNER_VERSION STRINGIFY(APP_BUILD_VERSION)
#else
#define CANNECTIVITY_BANNER_VERSION APP_VERSION_STRING
#endif
#endif /* CONFIG_CANNECTIVITY_BOOT_BANNER */

#define CHANNEL_CAN_CONTROLLER_DT_GET(node_id) DEVICE_DT_GET(DT_PHANDLE(node_id, can_controller))

static const struct gs_usb_ops gs_usb_ops = {
#ifdef CONFIG_CANNECTIVITY_TIMESTAMP_COUNTER
	.timestamp = cannectivity_timestamp_get,
#endif
#ifdef CONFIG_CANNECTIVITY_LED_GPIO
	.event = cannectivity_led_event,
#endif
#ifdef CONFIG_CANNECTIVITY_TERMINATION_GPIO
	.set_termination = cannectivity_set_termination,
	.get_termination = cannectivity_get_termination,
#endif
};

int main(void)
{
	const struct device *gs_usb = DEVICE_DT_GET(DT_NODELABEL(gs_usb0));
	const struct device *channels[] = {
#if CANNECTIVITY_DT_HAS_CHANNEL
		CANNECTIVITY_DT_FOREACH_CHANNEL_SEP(CHANNEL_CAN_CONTROLLER_DT_GET, (,))
#else /* CANNECTIVITY_DT_HAS_CHANNEL */
		DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus))
#endif /* !CANNECTIVITY_DT_HAS_CHANNEL */
	};
	int err;

#ifdef CONFIG_CANNECTIVITY_BOOT_BANNER
	printk("*** CANnectivity firmware " CANNECTIVITY_BANNER_VERSION " ***\n");
#endif /* CONFIG_CANNECTIVITY_BOOT_BANNER */

	if (!device_is_ready(gs_usb)) {
		LOG_ERR("gs_usb USB device not ready");
		return 0;
	}

	if (IS_ENABLED(CONFIG_CANNECTIVITY_LED_GPIO)) {
		err = cannectivity_led_init();
		if (err != 0) {
			return 0;
		}
	}

	if (IS_ENABLED(CONFIG_CANNECTIVITY_TERMINATION_GPIO)) {
		err = cannectivity_termination_init();
		if (err != 0) {
			return 0;
		}
	}

	if (IS_ENABLED(CONFIG_CANNECTIVITY_TIMESTAMP_COUNTER)) {
		err = cannectivity_timestamp_init();
		if (err != 0) {
			return 0;
		}
	}

	err = gs_usb_register(gs_usb, channels, ARRAY_SIZE(channels), &gs_usb_ops, NULL);
	if (err != 0U) {
		LOG_ERR("failed to register gs_usb (err %d)", err);
		return 0;
	}

	err = cannectivity_usb_init();
	if (err) {
		LOG_ERR("failed to enable USB device");
		return err;
	}

	if (IS_ENABLED(CONFIG_BOOTLOADER_MCUBOOT)) {
		err = cannectivity_dfu_init();
		if (err) {
			LOG_ERR("failed to initialize DFU");
			return err;
		}
	}

	LOG_INF("CANnectivity firmware initialized with %u channel%s", ARRAY_SIZE(channels),
		ARRAY_SIZE(channels) > 1 ? "s" : "");
}
