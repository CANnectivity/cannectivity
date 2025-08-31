/*
 * Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/logging/log.h>

#include "cannectivity.h"

LOG_MODULE_REGISTER(timestamp, CONFIG_CANNECTIVITY_LOG_LEVEL);

const struct device *counter = DEVICE_DT_GET(DT_PHANDLE(DT_PATH(cannectivity), timestamp_counter));

int cannectivity_timestamp_get(const struct device *dev, uint32_t *timestamp, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	return counter_get_value(counter, timestamp);
}

int cannectivity_timestamp_init(void)
{
	int err;

	if (!device_is_ready(counter)) {
		LOG_ERR("timestamp device not ready");
		return -ENODEV;
	}

	if (counter_get_frequency(counter) != MHZ(1)) {
		LOG_ERR("wrong timestamp counter frequency (%u)", counter_get_frequency(counter));
		return -EINVAL;
	}

	if (counter_get_max_top_value(counter) != UINT32_MAX) {
		LOG_ERR("timestamp counter is not 32 bit wide");
		return -EINVAL;
	}

	err = counter_start(counter);
	if (err != 0) {
		LOG_ERR("failed to start timestamp counter (err %d)", err);
		return err;
	};

	return 0;
}
