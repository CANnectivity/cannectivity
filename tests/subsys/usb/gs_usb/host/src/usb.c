/*
 * Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#ifdef CONFIG_USB_DEVICE_STACK_NEXT
#include <zephyr/usb/usbd.h>
#else /* CONFIG_USB_DEVICE_STACK_NEXT */
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/bos.h>
#endif /* !CONFIG_USB_DEVICE_STACK_NEXT*/

#include "test.h"

LOG_MODULE_REGISTER(usb, LOG_LEVEL_DBG);

#ifdef CONFIG_USB_DEVICE_STACK_NEXT
#define TEST_BOS_DESC_DEFINE_CAP static
#else /* CONFIG_USB_DEVICE_STACK_NEXT */
#define TEST_BOS_DESC_DEFINE_CAP USB_DEVICE_BOS_DESC_DEFINE_CAP
#endif /* !CONFIG_USB_DEVICE_STACK_NEXT */

TEST_BOS_DESC_DEFINE_CAP const struct usb_bos_capability_lpm bos_cap_lpm = {
	.bLength = sizeof(struct usb_bos_capability_lpm),
	.bDescriptorType = USB_DESC_DEVICE_CAPABILITY,
	.bDevCapabilityType = USB_BOS_CAPABILITY_EXTENSION,
	.bmAttributes = 0UL,
};

#ifdef CONFIG_USB_DEVICE_STACK_NEXT

USBD_DEVICE_DEFINE(usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), CONFIG_TEST_USB_VID,
		   CONFIG_TEST_USB_PID);

USBD_DESC_LANG_DEFINE(lang);
USBD_DESC_MANUFACTURER_DEFINE(mfr, CONFIG_TEST_USB_MANUFACTURER);
USBD_DESC_PRODUCT_DEFINE(product, CONFIG_TEST_USB_PRODUCT);
USBD_DESC_SERIAL_NUMBER_DEFINE(sn);
USBD_DESC_CONFIG_DEFINE(fs_config_desc, "Full-Speed Configuration");
USBD_DESC_CONFIG_DEFINE(hs_config_desc, "High-Speed Configuration");

USBD_CONFIGURATION_DEFINE(fs_config, 0U, CONFIG_TEST_USB_MAX_POWER, &fs_config_desc);
USBD_CONFIGURATION_DEFINE(hs_config, 0U, CONFIG_TEST_USB_MAX_POWER, &hs_config_desc);

USBD_DESC_BOS_DEFINE(usbext, sizeof(bos_cap_lpm), &bos_cap_lpm);

static int test_usb_init_usbd(void)
{
	int err;

	err = usbd_add_descriptor(&usbd, &lang);
	if (err != 0) {
		LOG_ERR("failed to add language descriptor (err %d)", err);
		return err;
	}

	err = usbd_add_descriptor(&usbd, &mfr);
	if (err != 0) {
		LOG_ERR("failed to add manufacturer descriptor (err %d)", err);
		return err;
	}

	err = usbd_add_descriptor(&usbd, &product);
	if (err != 0) {
		LOG_ERR("failed to add product descriptor (%d)", err);
		return err;
	}

	err = usbd_add_descriptor(&usbd, &sn);
	if (err != 0) {
		LOG_ERR("failed to add S/N descriptor (err %d)", err);
		return err;
	}

	if (usbd_caps_speed(&usbd) == USBD_SPEED_HS) {
		err = usbd_add_configuration(&usbd, USBD_SPEED_HS, &hs_config);
		if (err != 0) {
			LOG_ERR("failed to add high-speed configuration (err %d)", err);
			return err;
		}

		err = usbd_register_all_classes(&usbd, USBD_SPEED_HS, 1);
		if (err != 0) {
			LOG_ERR("failed to register high-speed classes (err %d)", err);
			return err;
		}

		err = usbd_device_set_code_triple(&usbd, USBD_SPEED_HS, 0, 0, 0);
		if (err != 0) {
			LOG_ERR("failed to set high-speed code triple (err %d)", err);
			return err;
		}

		err = usbd_device_set_bcd_usb(&usbd, USBD_SPEED_HS, USB_SRN_2_0_1);
		if (err != 0) {
			LOG_ERR("failed to set high-speed bcdUSB (err %d)", err);
			return err;
		}
	}

	err = usbd_add_configuration(&usbd, USBD_SPEED_FS, &fs_config);
	if (err != 0) {
		LOG_ERR("failed to add full-speed configuration (err %d)", err);
		return err;
	}

	err = usbd_register_all_classes(&usbd, USBD_SPEED_FS, 1);
	if (err != 0) {
		LOG_ERR("failed to register full-speed classes (err %d)", err);
		return err;
	}

	err = usbd_device_set_code_triple(&usbd, USBD_SPEED_FS, 0, 0, 0);
	if (err != 0) {
		LOG_ERR("failed to set full-speed code triple (err %d)", err);
		return err;
	}

	err = usbd_device_set_bcd_usb(&usbd, USBD_SPEED_FS, USB_SRN_2_0_1);
	if (err != 0) {
		LOG_ERR("failed to set full-speed bcdUSB (err %d)", err);
		return err;
	}

	err = usbd_add_descriptor(&usbd, &usbext);
	if (err != 0) {
		LOG_ERR("failed to add USB 2.0 extension descriptor (err %d)", err);
		return err;
	}

	err = usbd_init(&usbd);
	if (err != 0) {
		LOG_ERR("failed to initialize USB device support (err %d)", err);
		return err;
	}

	err = usbd_enable(&usbd);
	if (err != 0) {
		LOG_ERR("failed to enable USB device");
		return err;
	}

	return 0;
}
#endif /* !CONFIG_USB_DEVICE_STACK_NEXT */

int test_usb_init(void)
{
#ifdef CONFIG_USB_DEVICE_STACK_NEXT
	return test_usb_init_usbd();
#else /* CONFIG_USB_DEVICE_STACK_NEXT */
	usb_bos_register_cap((void *)&bos_cap_lpm);

	return usb_enable(NULL);
#endif /* !CONFIG_USB_DEVICE_STACK_NEXT */
}
