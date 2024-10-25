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

/**
 * @brief The CANnectivity devicetree hardware configuration node identifier
 */
#define CANNECTIVITY_DT_NODE_ID DT_NODELABEL(cannectivity)

/**
 * @brief True if one or more CANnectivity channels are present in the devicetree, false otherwise
 */
#define CANNECTIVITY_DT_HAS_CHANNEL DT_HAS_COMPAT_STATUS_OKAY(cannectivity_channel)

/**
 * @brief Invokes @p fn for each CANnectivity devicetree channel node with a separator
 *
 * The macro @p fn must take one parameter, which will be the node identifier of a CANnectivity
 * channel node.
 *
 * @param fn The macro to invoke.
 * @param sep Separator (e.g. comma or semicolon). Must be in parentheses;
 *            this is required to enable providing a comma as separator.
 */
#define CANNECTIVITY_DT_FOREACH_CHANNEL_SEP(fn, sep)                                               \
	DT_FOREACH_CHILD_STATUS_OKAY_SEP(CANNECTIVITY_DT_NODE_ID, fn, sep)

/**
 * @brief CANnectivity CAN channel LED event callback
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param ch CAN channel number.
 * @param event Channel event.
 * @param user_data User data provided when registering the callback.
 * @return 0 on success, negative error number otherwise.
 */
int cannectivity_led_event(const struct device *dev, uint16_t ch, enum gs_usb_event event,
			   void *user_data);

/**
 * @brief CANnectivity LED initialization function
 *
 * @return 0 on success, negative error number otherwise.
 */
int cannectivity_led_init(void);

/**
 * @brief CANnectivity CAN channel set CAN bus termination callback
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param ch CAN channel number.
 * @param terminate True if the channel termination is active, false otherwise.
 * @param user_data User data provided when registering the callback.
 * @return 0 on success, negative error number otherwise.
 */
int cannectivity_set_termination(const struct device *dev, uint16_t ch, bool terminate,
				 void *user_data);

/**
 * @brief CANnectivity CAN channel get CAN bus termination callback
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param ch CAN channel number.
 * @param[out] terminated True if the channel termination is active, false otherwise.
 * @param user_data User data provided when registering the callback.
 * @return 0 on success, negative error number otherwise.
 */
int cannectivity_get_termination(const struct device *dev, uint16_t ch, bool *terminated,
				 void *user_data);

/**
 * @brief CANnectivity CAN bus termination initialization function
 *
 * @return 0 on success, negative error number otherwise.
 */
int cannectivity_termination_init(void);

/**
 * @brief CANnectivity get hardware timestamp callback
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param[out] timestamp Current timestamp value.
 * @param user_data User data provided when registering the callback.
 * @return 0 on success, negative error number otherwise.
 */
int cannectivity_timestamp_get(const struct device *dev, uint32_t *timestamp, void *user_data);

/**
 * @brief CANnectivity hardware timestamp initialization function
 *
 * @return 0 on success, negative error number otherwise.
 */
int cannectivity_timestamp_init(void);

/**
 * @brief CANnectivity USB device initialization function
 *
 * @return 0 on success, negative error number otherwise.
 */
int cannectivity_usb_init(void);

/**
 * @brief CANnectivity USB DFU initialization function
 *
 * @return 0 on success, negative error number otherwise.
 */
int cannectivity_dfu_init(void);

#endif /* CANNECTIVITY_APP_CANNECTIVITY_H_ */
