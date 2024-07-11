/*
 * Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "cannectivity.h"

LOG_MODULE_REGISTER(termination, CONFIG_CANNECTIVITY_LOG_LEVEL);

struct termination_gpio_dt_spec {
	const struct gpio_dt_spec gpio;
	bool terminated;
};

#define CHANNEL_TERMINATION_GPIO_DT_SPEC_GET(node_id)                                              \
	{                                                                                          \
		.gpio = GPIO_DT_SPEC_GET_OR(node_id, termination_gpios, {0}),                      \
		.terminated = IS_ENABLED(CONFIG_CANNECTIVITY_TERMINATION_DEFAULT_ON),              \
	}

static struct termination_gpio_dt_spec termination_gpios[] = {
	CANNECTIVITY_DT_FOREACH_CHANNEL_SEP(CHANNEL_TERMINATION_GPIO_DT_SPEC_GET, (,))
};

int cannectivity_set_termination(const struct device *dev, uint16_t ch, bool terminate,
				 void *user_data)
{
	struct termination_gpio_dt_spec *spec;
	int err;

	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	LOG_DBG("set termination for channel %u: %s", ch, terminate ? "on" : "off");

	if (ch >= ARRAY_SIZE(termination_gpios)) {
		LOG_ERR("set termination for non-existing channel %d", ch);
		return -EINVAL;
	}

	spec = &termination_gpios[ch];

	if (spec->gpio.port == NULL) {
		return -ENODEV;
	}

	err = gpio_pin_set_dt(&spec->gpio, (int)terminate);
	if (err != 0) {
		LOG_ERR("failed to set termination for channel %u to %s (err %d)", ch,
			terminate ? "on" : "off", err);
		return err;
	}

	spec->terminated = terminate;

	return 0;
}

int cannectivity_get_termination(const struct device *dev, uint16_t ch, bool *terminated,
				 void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	if (ch >= ARRAY_SIZE(termination_gpios)) {
		LOG_ERR("set termination for non-existing channel %d", ch);
		return -EINVAL;
	}

	*terminated = termination_gpios[ch].terminated;
	LOG_DBG("get termination for channel %u: %s", ch, *terminated ? "on" : "off");

	return 0;
}

int cannectivity_termination_init(void)
{
	struct termination_gpio_dt_spec *spec;
	uint16_t ch;
	int err;

	for (ch = 0; ch < ARRAY_SIZE(termination_gpios); ch++) {
		spec = &termination_gpios[ch];

		if (spec->gpio.port == NULL) {
			continue;
		}

		if (!gpio_is_ready_dt(&spec->gpio)) {
			LOG_ERR("channel %d termination GPIO not ready", ch);
			return -ENODEV;
		}

		err = gpio_pin_configure_dt(&spec->gpio, spec->terminated ? GPIO_OUTPUT_ACTIVE
									  : GPIO_OUTPUT_INACTIVE);
		if (err != 0) {
			LOG_ERR("failed to configure channel %d termination GPIO (err %d)", ch,
				err);
			return err;
		}
	};

	return 0;
}
