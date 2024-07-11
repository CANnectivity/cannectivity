/*
 * Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
#include <zephyr/usb/msos_desc.h>
#include <zephyr/sys/byteorder.h>

#ifdef CONFIG_USB_DEVICE_STACK_NEXT
#include <zephyr/usb/usbd.h>
#else /* CONFIG_USB_DEVICE_STACK_NEXT */
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/bos.h>
#include <usb_descriptor.h>
#endif /* !CONFIG_USB_DEVICE_STACK_NEXT*/

#include <cannectivity/usb/class/gs_usb.h>

#include "cannectivity.h"
#include "zephyr/app_version.h"

LOG_MODULE_REGISTER(usb, CONFIG_CANNECTIVITY_LOG_LEVEL);

#define CANNECTIVITY_USB_BCD_DRN                                                                   \
	(USB_DEC_TO_BCD(APP_VERSION_MAJOR) << 8 | USB_DEC_TO_BCD(APP_VERSION_MINOR))

#ifdef CONFIG_USB_DEVICE_STACK_NEXT
#define CANNECTIVITY_BOS_DESC_DEFINE_CAP static
#else /* CONFIG_USB_DEVICE_STACK_NEXT */
#define CANNECTIVITY_BOS_DESC_DEFINE_CAP USB_DEVICE_BOS_DESC_DEFINE_CAP
#endif /* !CONFIG_USB_DEVICE_STACK_NEXT */

CANNECTIVITY_BOS_DESC_DEFINE_CAP const struct usb_bos_capability_lpm bos_cap_lpm = {
	.bLength = sizeof(struct usb_bos_capability_lpm),
	.bDescriptorType = USB_DESC_DEVICE_CAPABILITY,
	.bDevCapabilityType = USB_BOS_CAPABILITY_EXTENSION,
	.bmAttributes = 0UL,
};

struct cannectivity_msosv2_descriptor {
	struct msosv2_descriptor_set_header header;
	struct msosv2_compatible_id compatible_id;
	struct msosv2_guids_property guids_property;
	struct msosv2_vendor_revision vendor_revision;
} __packed;

#define COMPATIBLE_ID_WINUSB 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00

/* clang-format off */
/* CANnectivity (random) DeviceInterfaceUUID: {B24D8379-235F-4853-95E7-7772516FA2D5} */
#define CANNECTIVITY_DEVICE_INTERFACE_GUID \
	'{', 0x00, 'B', 0x00, '2', 0x00, '4', 0x00, 'D', 0x00, '8', 0x00, \
	'3', 0x00, '7', 0x00, '9', 0x00, '-', 0x00, '2', 0x00, '3', 0x00, \
	'5', 0x00, 'F', 0x00, '-', 0x00, '4', 0x00, '8', 0x00, '5', 0x00, \
	'3', 0x00, '-', 0x00, '9', 0x00, '5', 0x00, 'E', 0x00, '7', 0x00, \
	'-', 0x00, '7', 0x00, '7', 0x00, '7', 0x00, '2', 0x00, '5', 0x00, \
	'1', 0x00, '6', 0x00, 'F', 0x00, 'A', 0x00, '2', 0x00, 'D', 0x00, \
	'5', 0x00, '}', 0x00, 0x00, 0x00

/*
 * TODO: MSOSv2 not possible in device_next yet. See
 * https://github.com/zephyrproject-rtos/zephyr/issues/76224
 */
__maybe_unused static const struct cannectivity_msosv2_descriptor cannectivity_msosv2_descriptor = {
	.header = {
		.wLength = sizeof(struct msosv2_descriptor_set_header),
		.wDescriptorType = MS_OS_20_SET_HEADER_DESCRIPTOR,
		/* Windows version (8.1) (0x06030000) */
		.dwWindowsVersion = 0x06030000,
		.wTotalLength = sizeof(struct cannectivity_msosv2_descriptor),
	},
	.compatible_id = {
		.wLength = sizeof(struct msosv2_compatible_id),
		.wDescriptorType = MS_OS_20_FEATURE_COMPATIBLE_ID,
		.CompatibleID = {COMPATIBLE_ID_WINUSB},
	},
	.guids_property = {
		.wLength = sizeof(struct msosv2_guids_property),
		.wDescriptorType = MS_OS_20_FEATURE_REG_PROPERTY,
		.wPropertyDataType = MS_OS_20_PROPERTY_DATA_REG_MULTI_SZ,
		.wPropertyNameLength = 42,
		.PropertyName = {DEVICE_INTERFACE_GUIDS_PROPERTY_NAME},
		.wPropertyDataLength = 80,
		.bPropertyData = {CANNECTIVITY_DEVICE_INTERFACE_GUID},
	},
	.vendor_revision = {
		.wLength = sizeof(struct msosv2_vendor_revision),
		.wDescriptorType = MS_OS_20_FEATURE_VENDOR_REVISION,
		.VendorRevision = 1U,
	},
};

struct usb_bos_capability_msosv2 {
	struct usb_bos_platform_descriptor platform;
	struct usb_bos_capability_msos cap;
} __packed;

CANNECTIVITY_BOS_DESC_DEFINE_CAP const struct usb_bos_capability_msosv2 bos_cap_msosv2 = {
	.platform = {
		.bLength = sizeof(struct usb_bos_capability_msosv2),
		.bDescriptorType = USB_DESC_DEVICE_CAPABILITY,
		.bDevCapabilityType = USB_BOS_CAPABILITY_PLATFORM,
		.bReserved = 0,
		.PlatformCapabilityUUID = {
			/* MS OS 2.0 Platform Capability ID: D8DD60DF-4589-4CC7-9CD2-659D9E648A9F */
			0xDF, 0x60, 0xDD, 0xD8,
			0x89, 0x45,
			0xC7, 0x4C,
			0x9C, 0xD2,
			0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F,
		},
	},
	.cap = {
		/* Windows version (8.1) (0x06030000) */
		.dwWindowsVersion = sys_cpu_to_le32(0x06030000),
		.wMSOSDescriptorSetTotalLength =
			sys_cpu_to_le16(sizeof(struct cannectivity_msosv2_descriptor)),
		.bMS_VendorCode = GS_USB_MS_VENDORCODE,
		.bAltEnumCode = 0x00
	},
};
/* clang-format on */

#ifdef CONFIG_USB_DEVICE_STACK_NEXT

USBD_DEVICE_DEFINE(usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), CONFIG_CANNECTIVITY_USB_VID,
		   CONFIG_CANNECTIVITY_USB_PID);

USBD_DESC_LANG_DEFINE(lang);
USBD_DESC_MANUFACTURER_DEFINE(mfr, CONFIG_CANNECTIVITY_USB_MANUFACTURER);
USBD_DESC_PRODUCT_DEFINE(product, CONFIG_CANNECTIVITY_USB_PRODUCT);
USBD_DESC_SERIAL_NUMBER_DEFINE(sn);
USBD_DESC_CONFIG_DEFINE(fs_config_desc, "Full-Speed Configuration");
USBD_DESC_CONFIG_DEFINE(hs_config_desc, "High-Speed Configuration");

USBD_CONFIGURATION_DEFINE(fs_config, 0U, CONFIG_CANNECTIVITY_USB_MAX_POWER, &fs_config_desc);
USBD_CONFIGURATION_DEFINE(hs_config, 0U, CONFIG_CANNECTIVITY_USB_MAX_POWER, &hs_config_desc);

USBD_DESC_BOS_DEFINE(usbext, sizeof(bos_cap_lpm), &bos_cap_lpm);
/*
 * TODO: MSOSv2 not possible in device_next yet. See
 * https://github.com/zephyrproject-rtos/zephyr/issues/76224
 */
__maybe_unused USBD_DESC_BOS_DEFINE(msosv2, sizeof(bos_cap_msosv2), &bos_cap_msosv2);

static int cannectivity_usb_init_usbd(void)
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

		err = usbd_device_set_code_triple(&usbd, USBD_SPEED_HS, USB_BCC_MISCELLANEOUS, 0x02,
						  0x01);
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

	err = usbd_device_set_code_triple(&usbd, USBD_SPEED_FS, USB_BCC_MISCELLANEOUS, 0x02, 0x01);
	if (err != 0) {
		LOG_ERR("failed to set full-speed code triple (err %d)", err);
		return err;
	}

	err = usbd_device_set_bcd_usb(&usbd, USBD_SPEED_FS, USB_SRN_2_0_1);
	if (err != 0) {
		LOG_ERR("failed to set full-speed bcdUSB (err %d)", err);
		return err;
	}

	err = usbd_device_set_bcd_device(&usbd, sys_cpu_to_le16(CANNECTIVITY_USB_BCD_DRN));
	if (err != 0) {
		LOG_ERR("failed to set bcdDevice (err %d)", err);
		return err;
	}

	err = usbd_add_descriptor(&usbd, &usbext);
	if (err != 0) {
		LOG_ERR("failed to add USB 2.0 extension descriptor (err %d)", err);
		return err;
	}

#if 0
	/*
	 * TODO: MSOSv2 not possible in device_next yet. See
	 * https://github.com/zephyrproject-rtos/zephyr/issues/76224
	 */
	err = usbd_add_descriptor(&usbd, &msosv2);
	if (err != 0) {
		LOG_ERR("failed to add Microsoft OS 2.0 descriptor (err %d)", err);
		return err;
	}
#endif /* 0 */

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
#else /* CONFIG_USB_DEVICE_STACK_NEXT */
int cannectivity_usb_vendorcode_handler(int32_t *tlen, uint8_t **tdata)
{
	*tdata = (uint8_t *)(&cannectivity_msosv2_descriptor);
	*tlen = sizeof(cannectivity_msosv2_descriptor);

	return 0;
}
#endif /* !CONFIG_USB_DEVICE_STACK_NEXT */

int cannectivity_usb_init(void)
{
#ifdef CONFIG_USB_DEVICE_STACK_NEXT
	return cannectivity_usb_init_usbd();
#else /* CONFIG_USB_DEVICE_STACK_NEXT */
	struct usb_device_descriptor *desc =
		(struct usb_device_descriptor *)usb_get_device_descriptor();

	desc->bcdDevice = sys_cpu_to_le16(CANNECTIVITY_USB_BCD_DRN);

	usb_bos_register_cap((void *)&bos_cap_lpm);
	usb_bos_register_cap((void *)&bos_cap_msosv2);

	gs_usb_register_vendorcode_callback(cannectivity_usb_vendorcode_handler);

	return usb_enable(NULL);
#endif /* !CONFIG_USB_DEVICE_STACK_NEXT */
}
