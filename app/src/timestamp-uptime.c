/*
 * Copyright (c) 2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "cannectivity.h"

LOG_MODULE_REGISTER(timestamp, CONFIG_CANNECTIVITY_LOG_LEVEL);

int cannectivity_timestamp_get(const struct device *dev, uint32_t *timestamp, void *user_data)
{
	uint64_t cycles;

	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	cycles = k_cycle_get_64();
	*timestamp = k_cyc_to_us_near32(cycles);

	return 0;
}

int cannectivity_timestamp_init(void)
{
	return 0;
}
