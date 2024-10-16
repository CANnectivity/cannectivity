/*
 * Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "cannectivity.h"

LOG_MODULE_REGISTER(dfu, CONFIG_CANNECTIVITY_LOG_LEVEL);

#define DFU_LED_NODE DT_ALIAS(mcuboot_led0)

#ifdef CONFIG_CANNECTIVITY_DFU_LED
static int dfu_led_init(void)
{
	struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DFU_LED_NODE, gpios);
	int err;

	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("DFU LED device not ready");
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		LOG_ERR("failed to turn off DFU LED (err %d)", err);
		return err;
	}

	return 0;
}
#endif /* CONFIG_CANNECTIVITY_DFU_LED */

int cannectivity_dfu_init(void)
{
	int err;

	/*
	 * Confirm updated image if running under MCUboot booloader. This could be done on
	 * successful USB enumeration instead, but that could cause unwanted image reverts on
	 * e.g. self-powered development boards.
	 */
	if (!boot_is_img_confirmed()) {
		err = boot_write_img_confirmed();
		if (err != 0) {
			LOG_ERR("failed to confirm image (err %d)", err);
			return err;
		}

		LOG_INF("image confirmed");
	}

#ifdef CONFIG_CANNECTIVITY_DFU_LED
	err = dfu_led_init();
	if (err != 0) {
		return err;
	}
#endif /* CONFIG_CANNECTIVITY_DFU_LED */

	return 0;
}
