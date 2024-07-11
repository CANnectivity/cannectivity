/*
 * Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CANNECTIVITY_APP_CANNECTIVITY_H_
#define CANNECTIVITY_APP_CANNECTIVITY_H_

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <cannectivity/usb/class/gs_usb.h>

#define CANNECTIVITY_DT_NODE_ID DT_NODELABEL(cannectivity)

#define CANNECTIVITY_DT_HAS_CHANNEL DT_HAS_COMPAT_STATUS_OKAY(cannectivity_channel)

#define CANNECTIVITY_DT_FOREACH_CHANNEL_SEP(fn, sep)                                               \
	DT_FOREACH_CHILD_STATUS_OKAY_SEP(CANNECTIVITY_DT_NODE_ID, fn, sep)

int cannectivity_led_identify(const struct device *dev, uint16_t ch, bool identify,
			      void *user_data);

int cannectivity_led_state(const struct device *dev, uint16_t ch, bool started, void *user_data);

int cannectivity_led_activity(const struct device *dev, uint16_t ch, void *user_data);

int cannectivity_led_init(void);

int cannectivity_set_termination(const struct device *dev, uint16_t ch, bool terminate,
				 void *user_data);

int cannectivity_get_termination(const struct device *dev, uint16_t ch, bool *terminated,
				 void *user_data);

int cannectivity_termination_init(void);

int cannectivity_timestamp_get(const struct device *dev, uint32_t *timestamp, void *user_data);

int cannectivity_timestamp_init(void);

int cannectivity_usb_init(void);

#endif /* CANNECTIVITY_APP_CANNECTIVITY_H_ */
