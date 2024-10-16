/*
 * Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/dfu/mcuboot.h>
#include <zephyr/logging/log.h>

#include "cannectivity.h"

LOG_MODULE_REGISTER(dfu, CONFIG_CANNECTIVITY_LOG_LEVEL);

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

	return 0;
}
