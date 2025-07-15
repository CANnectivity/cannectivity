/*
 * Copyright (c) 2024-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
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
#ifdef CONFIG_CANNECTIVITY_DFU_BACKEND_APP
#include <zephyr/dfu/mcuboot.h>
#endif /* CONFIG_CANNECTIVITY_DFU_BACKEND_APP */
#ifdef CONFIG_CANNECTIVITY_DFU_REBOOT
#include <zephyr/sys/reboot.h>
#endif /* CONFIG_CANNECTIVITY_DFU_REBOOT */

#include <cannectivity/usb/class/gs_usb.h>

#include "cannectivity.h"
#include "zephyr/app_version.h"

LOG_MODULE_REGISTER(usb, CONFIG_CANNECTIVITY_LOG_LEVEL);

#define GS_USB_CLASS_INSTANCE_NAME      "gs_usb_0"
#define DFU_RUNTIME_CLASS_INSTANCE_NAME "dfu_runtime"
#define DFU_DFU_CLASS_INSTANCE_NAME     "dfu_dfu"

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
#ifdef CONFIG_CANNECTIVITY_DFU_BACKEND_APP
	struct msosv2_configuration_subset_header cfg_header;
	struct msosv2_function_subset_header gs_usb_func_header;
#endif /* CONFIG_CANNECTIVITY_DFU_BACKEND_APP */
	struct msosv2_compatible_id gs_usb_compatible_id;
	struct msosv2_guids_property gs_usb_guids_property;
	struct msosv2_vendor_revision gs_usb_vendor_revision;
#ifdef CONFIG_CANNECTIVITY_DFU_BACKEND_APP
	struct msosv2_function_subset_header dfu_runtime_func_header;
	struct msosv2_compatible_id dfu_runtime_compatible_id;
	struct msosv2_guids_property dfu_runtime_guids_property;
	struct msosv2_vendor_revision dfu_runtime_vendor_revision;
#endif /* CONFIG_CANNECTIVITY_DFU_BACKEND_APP */
} __packed;

struct cannectivity_dfu_msosv2_descriptor {
	struct msosv2_descriptor_set_header header;
	struct msosv2_compatible_id dfu_dfu_compatible_id;
	struct msosv2_guids_property dfu_dfu_guids_property;
	struct msosv2_vendor_revision dfu_dfu_vendor_revision;
} __packed;

#define COMPATIBLE_ID_WINUSB 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00

/* clang-format off */
/* CANnectivity (random) DeviceInterfaceUUID: {B24D8379-235F-4853-95E7-7772516FA2D5} */
#define CANNECTIVITY_GS_USB_DEVICE_INTERFACE_GUID \
	'{', 0x00, 'B', 0x00, '2', 0x00, '4', 0x00, 'D', 0x00, '8', 0x00, \
	'3', 0x00, '7', 0x00, '9', 0x00, '-', 0x00, '2', 0x00, '3', 0x00, \
	'5', 0x00, 'F', 0x00, '-', 0x00, '4', 0x00, '8', 0x00, '5', 0x00, \
	'3', 0x00, '-', 0x00, '9', 0x00, '5', 0x00, 'E', 0x00, '7', 0x00, \
	'-', 0x00, '7', 0x00, '7', 0x00, '7', 0x00, '2', 0x00, '5', 0x00, \
	'1', 0x00, '6', 0x00, 'F', 0x00, 'A', 0x00, '2', 0x00, 'D', 0x00, \
	'5', 0x00, '}', 0x00, 0x00, 0x00

#ifdef CONFIG_CANNECTIVITY_DFU_BACKEND_APP

/* CANnectivity DFU runtime (random) DeviceInterfaceUUID: {A2E25357-68EB-4B7B-AE60-6F79C174A4D7} */
#define CANNECTIVITY_DFU_RUNTIME_DEVICE_INTERFACE_GUID \
	'{', 0x00, 'A', 0x00, '2', 0x00, 'E', 0x00, '2', 0x00, '5', 0x00, \
	'3', 0x00, '5', 0x00, '7', 0x00, '-', 0x00, '6', 0x00, '8', 0x00, \
	'E', 0x00, 'B', 0x00, '-', 0x00, '4', 0x00, 'B', 0x00, '7', 0x00, \
	'B', 0x00, '-', 0x00, 'A', 0x00, 'E', 0x00, '6', 0x00, '0', 0x00, \
	'-', 0x00, '6', 0x00, 'F', 0x00, '7', 0x00, '9', 0x00, 'C', 0x00, \
	'1', 0x00, '7', 0x00, '4', 0x00, 'A', 0x00, '4', 0x00, 'D', 0x00, \
	'7', 0x00, '}', 0x00, 0x00, 0x00

/* CANnectivity DFU DFU (random) DeviceInterfaceUUID: {B1371365-D4FD-4C12-9F1A-32D9E36ED477} */
#define CANNECTIVITY_DFU_DFU_DEVICE_INTERFACE_GUID \
	'{', 0x00, 'B', 0x00, '1', 0x00, '3', 0x00, '7', 0x00, '1', 0x00, \
	'3', 0x00, '6', 0x00, '5', 0x00, '-', 0x00, 'D', 0x00, '4', 0x00, \
	'F', 0x00, 'D', 0x00, '-', 0x00, '4', 0x00, 'C', 0x00, '1', 0x00, \
	'2', 0x00, '-', 0x00, '9', 0x00, 'F', 0x00, '1', 0x00, 'A', 0x00, \
	'-', 0x00, '3', 0x00, '2', 0x00, 'D', 0x00, '9', 0x00, 'E', 0x00, \
	'3', 0x00, '6', 0x00, 'E', 0x00, 'D', 0x00, '4', 0x00, '7', 0x00, \
	'7', 0x00, '}', 0x00, 0x00, 0x00

#endif /* CONFIG_CANNECTIVITY_DFU_BACKEND_APP */

static const struct cannectivity_msosv2_descriptor cannectivity_msosv2_descriptor = {
	.header = {
		.wLength = sizeof(struct msosv2_descriptor_set_header),
		.wDescriptorType = MS_OS_20_SET_HEADER_DESCRIPTOR,
		/* Windows version (8.1) (0x06030000) */
		.dwWindowsVersion = 0x06030000,
		.wTotalLength = sizeof(struct cannectivity_msosv2_descriptor),
	},
#ifdef CONFIG_CANNECTIVITY_DFU_BACKEND_APP
	.cfg_header = {
		.wLength = sizeof(struct msosv2_configuration_subset_header),
		.wDescriptorType = MS_OS_20_SUBSET_HEADER_CONFIGURATION,
		.bConfigurationValue = 0U,
		.bReserved = 0U,
		.wTotalLength = sizeof(struct cannectivity_msosv2_descriptor) -
			sizeof(struct msosv2_descriptor_set_header),
	},
	.gs_usb_func_header = {
		.wLength = sizeof(struct msosv2_function_subset_header),
		.wDescriptorType = MS_OS_20_SUBSET_HEADER_FUNCTION,
		.bFirstInterface = 0U,
		.bReserved = 0U,
		.wSubsetLength = sizeof(struct msosv2_function_subset_header) +
			sizeof(struct msosv2_compatible_id) +
			sizeof(struct msosv2_guids_property) +
			sizeof(struct msosv2_vendor_revision),
	},
#endif /* CONFIG_CANNECTIVITY_DFU_BACKEND_APP */
	.gs_usb_compatible_id = {
		.wLength = sizeof(struct msosv2_compatible_id),
		.wDescriptorType = MS_OS_20_FEATURE_COMPATIBLE_ID,
		.CompatibleID = {COMPATIBLE_ID_WINUSB},
	},
	.gs_usb_guids_property = {
		.wLength = sizeof(struct msosv2_guids_property),
		.wDescriptorType = MS_OS_20_FEATURE_REG_PROPERTY,
		.wPropertyDataType = MS_OS_20_PROPERTY_DATA_REG_MULTI_SZ,
		.wPropertyNameLength = 42,
		.PropertyName = {DEVICE_INTERFACE_GUIDS_PROPERTY_NAME},
		.wPropertyDataLength = 80,
		.bPropertyData = {CANNECTIVITY_GS_USB_DEVICE_INTERFACE_GUID},
	},
	.gs_usb_vendor_revision = {
		.wLength = sizeof(struct msosv2_vendor_revision),
		.wDescriptorType = MS_OS_20_FEATURE_VENDOR_REVISION,
		.VendorRevision = 1U,
	},
#ifdef CONFIG_CANNECTIVITY_DFU_BACKEND_APP
	.dfu_runtime_func_header = {
		.wLength = sizeof(struct msosv2_function_subset_header),
		.wDescriptorType = MS_OS_20_SUBSET_HEADER_FUNCTION,
		.bFirstInterface = 1U,
		.bReserved = 0U,
		.wSubsetLength = sizeof(struct msosv2_function_subset_header) +
			sizeof(struct msosv2_compatible_id) +
			sizeof(struct msosv2_guids_property) +
			sizeof(struct msosv2_vendor_revision),
	},
	.dfu_runtime_compatible_id = {
		.wLength = sizeof(struct msosv2_compatible_id),
		.wDescriptorType = MS_OS_20_FEATURE_COMPATIBLE_ID,
		.CompatibleID = {COMPATIBLE_ID_WINUSB},
	},
	.dfu_runtime_guids_property = {
		.wLength = sizeof(struct msosv2_guids_property),
		.wDescriptorType = MS_OS_20_FEATURE_REG_PROPERTY,
		.wPropertyDataType = MS_OS_20_PROPERTY_DATA_REG_MULTI_SZ,
		.wPropertyNameLength = 42,
		.PropertyName = {DEVICE_INTERFACE_GUIDS_PROPERTY_NAME},
		.wPropertyDataLength = 80,
		.bPropertyData = {CANNECTIVITY_DFU_RUNTIME_DEVICE_INTERFACE_GUID},
	},
	.dfu_runtime_vendor_revision = {
		.wLength = sizeof(struct msosv2_vendor_revision),
		.wDescriptorType = MS_OS_20_FEATURE_VENDOR_REVISION,
		.VendorRevision = 1U,
	},
#endif /* CONFIG_CANNECTIVITY_DFU_BACKEND_APP */
};

#ifdef CONFIG_CANNECTIVITY_DFU_BACKEND_APP
static const struct cannectivity_dfu_msosv2_descriptor cannectivity_dfu_msosv2_descriptor = {
	.header = {
		.wLength = sizeof(struct msosv2_descriptor_set_header),
		.wDescriptorType = MS_OS_20_SET_HEADER_DESCRIPTOR,
		/* Windows version (8.1) (0x06030000) */
		.dwWindowsVersion = 0x06030000,
		.wTotalLength = sizeof(struct cannectivity_dfu_msosv2_descriptor),
	},
	.dfu_dfu_compatible_id = {
		.wLength = sizeof(struct msosv2_compatible_id),
		.wDescriptorType = MS_OS_20_FEATURE_COMPATIBLE_ID,
		.CompatibleID = {COMPATIBLE_ID_WINUSB},
	},
	.dfu_dfu_guids_property = {
		.wLength = sizeof(struct msosv2_guids_property),
		.wDescriptorType = MS_OS_20_FEATURE_REG_PROPERTY,
		.wPropertyDataType = MS_OS_20_PROPERTY_DATA_REG_MULTI_SZ,
		.wPropertyNameLength = 42,
		.PropertyName = {DEVICE_INTERFACE_GUIDS_PROPERTY_NAME},
		.wPropertyDataLength = 80,
		.bPropertyData = {CANNECTIVITY_DFU_DFU_DEVICE_INTERFACE_GUID},
	},
	.dfu_dfu_vendor_revision = {
		.wLength = sizeof(struct msosv2_vendor_revision),
		.wDescriptorType = MS_OS_20_FEATURE_VENDOR_REVISION,
		.VendorRevision = 1U,
	},
};
#endif /* CONFIG_CANNECTIVITY_DFU_BACKEND_APP */

struct usb_bos_capability_msosv2 {
	struct usb_bos_platform_descriptor platform;
	struct usb_bos_capability_msos cap;
} __packed;

#ifdef CONFIG_CANNECTIVITY_DFU_BACKEND_APP
CANNECTIVITY_BOS_DESC_DEFINE_CAP struct usb_bos_capability_msosv2 bos_cap_msosv2 = {
#else /* CONFIG_CANNECTIVITY_DFU_BACKEND_APP */
CANNECTIVITY_BOS_DESC_DEFINE_CAP const struct usb_bos_capability_msosv2 bos_cap_msosv2 = {
#endif /* CONFIG_CANNECTIVITY_DFU_BACKEND_APP */
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

static const uint8_t attr =
	(IS_ENABLED(CONFIG_CANNECTIVITY_USB_SELF_POWERED) ? USB_SCD_SELF_POWERED : 0U);
USBD_CONFIGURATION_DEFINE(fs_config, attr, CONFIG_CANNECTIVITY_USB_MAX_POWER, &fs_config_desc);
USBD_CONFIGURATION_DEFINE(hs_config, attr, CONFIG_CANNECTIVITY_USB_MAX_POWER, &hs_config_desc);

#ifdef CONFIG_CANNECTIVITY_DFU_BACKEND_APP
USBD_DESC_PRODUCT_DEFINE(product_dfu, CONFIG_CANNECTIVITY_USB_DFU_PRODUCT);
USBD_DESC_CONFIG_DEFINE(fs_config_desc_dfu, "Full-Speed Configuration (DFU)");
USBD_DESC_CONFIG_DEFINE(hs_config_desc_dfu, "High-Speed Configuration (DFU)");
USBD_CONFIGURATION_DEFINE(fs_config_dfu, attr, CONFIG_CANNECTIVITY_USB_MAX_POWER,
			  &fs_config_desc_dfu);
USBD_CONFIGURATION_DEFINE(hs_config_dfu, attr, CONFIG_CANNECTIVITY_USB_MAX_POWER,
			  &hs_config_desc_dfu);

#ifdef CONFIG_CANNECTIVITY_DFU_REBOOT
static struct k_work_delayable usb_reboot_work;
#endif /* CONFIG_CANNECTIVITY_DFU_REBOOT */
#endif /* CONFIG_CANNECTIVITY_DFU_BACKEND_APP */

USBD_DESC_BOS_DEFINE(usbext, sizeof(bos_cap_lpm), &bos_cap_lpm);

static void const *pcannectivity_msosv2_descriptor = &cannectivity_msosv2_descriptor;

static int cannectivity_usb_vendorcode_handler(const struct usbd_context *const ctx,
					       const struct usb_setup_packet *const setup,
					       struct net_buf *const buf)
{
	size_t len = bos_cap_msosv2.cap.wMSOSDescriptorSetTotalLength;

	if (setup->bRequest == GS_USB_MS_VENDORCODE && setup->wIndex == MS_OS_20_DESCRIPTOR_INDEX) {
		net_buf_add_mem(buf, pcannectivity_msosv2_descriptor,
				MIN(net_buf_tailroom(buf), len));

		return 0;
	}

	return -ENOTSUP;
}

USBD_DESC_BOS_VREQ_DEFINE(msosv2, sizeof(bos_cap_msosv2), &bos_cap_msosv2,
			  GS_USB_MS_VENDORCODE, cannectivity_usb_vendorcode_handler, NULL);

#ifdef CONFIG_CANNECTIVITY_DFU_BACKEND_APP
static void cannectivity_usb_msg_cb(struct usbd_context *const usbd_ctx,
				    const struct usbd_msg *const msg);

void cannectivity_usb_switch_to_dfu_mode(void)
{
	int err;

	usbd_disable(&usbd);
	usbd_shutdown(&usbd);

	bos_cap_msosv2.cap.wMSOSDescriptorSetTotalLength =
		sizeof(struct cannectivity_dfu_msosv2_descriptor);
	pcannectivity_msosv2_descriptor = &cannectivity_dfu_msosv2_descriptor;

	err = usbd_device_set_vid(&usbd, CONFIG_CANNECTIVITY_USB_DFU_VID);
	if (err != 0) {
		LOG_ERR("failed to set vendor ID (err %d)", err);
		return;
	}

	err = usbd_device_set_pid(&usbd, CONFIG_CANNECTIVITY_USB_DFU_PID);
	if (err != 0) {
		LOG_ERR("failed to set product ID (err %d)", err);
		return;
	}

	err = usbd_add_descriptor(&usbd, &lang);
	if (err != 0) {
		LOG_ERR("failed to add language descriptor (err %d)", err);
		return;
	}

	err = usbd_add_descriptor(&usbd, &mfr);
	if (err != 0) {
		LOG_ERR("failed to add manufacturer descriptor (err %d)", err);
		return;
	}

	err = usbd_add_descriptor(&usbd, &product_dfu);
	if (err != 0) {
		LOG_ERR("failed to add product descriptor (%d)", err);
		return;
	}

	err = usbd_add_descriptor(&usbd, &sn);
	if (err != 0) {
		LOG_ERR("failed to add S/N descriptor (err %d)", err);
		return;
	}

	if (USBD_SUPPORTS_HIGH_SPEED && usbd_caps_speed(&usbd) == USBD_SPEED_HS) {
		err = usbd_add_configuration(&usbd, USBD_SPEED_HS, &hs_config_dfu);
		if (err != 0) {
			LOG_ERR("failed to add high-speed configuration (err %d)", err);
			return;
		}

		err = usbd_register_class(&usbd, DFU_DFU_CLASS_INSTANCE_NAME, USBD_SPEED_HS, 1);
		if (err != 0) {
			LOG_ERR("failed to register high-speed dfu class instance (err %d)",
				err);
			return;
		}

		err = usbd_device_set_code_triple(&usbd, USBD_SPEED_HS, 0, 0, 0);
		if (err != 0) {
			LOG_ERR("failed to set high-speed code triple (err %d)", err);
			return;
		}
	}

	err = usbd_add_configuration(&usbd, USBD_SPEED_FS, &fs_config_dfu);
	if (err != 0) {
		LOG_ERR("failed to add full-speed configuration (err %d)", err);
		return;
	}

	err = usbd_register_class(&usbd, DFU_DFU_CLASS_INSTANCE_NAME, USBD_SPEED_FS, 1);
	if (err != 0) {
		LOG_ERR("failed to register full-speed dfu class instance (err %d)",
			err);
		return;
	}

	err = usbd_device_set_code_triple(&usbd, USBD_SPEED_FS, 0, 0, 0);
	if (err != 0) {
		LOG_ERR("failed to set full-speed code triple (err %d)", err);
		return;
	}

	err = usbd_add_descriptor(&usbd, &usbext);
	if (err != 0) {
		LOG_ERR("failed to add USB 2.0 extension descriptor (err %d)", err);
		return;
	}

	err = usbd_add_descriptor(&usbd, &msosv2);
	if (err != 0) {
		LOG_ERR("failed to add Microsoft OS 2.0 descriptor (err %d)", err);
		return;
	}

	err = usbd_init(&usbd);
	if (err != 0) {
		LOG_ERR("failed to initialize USB device support (err %d)", err);
		return;
	}

	err = usbd_enable(&usbd);
	if (err != 0) {
		LOG_ERR("failed to enable USB device");
		return;
	}

#ifdef CONFIG_CANNECTIVITY_DFU_LED
	err = cannectivity_dfu_led_on();
	if (err != 0) {
		LOG_ERR("failed to turn on DFU LED (err %d)", err);
		return;
	}
#endif /* CONFIG_CANNECTIVITY_DFU_LED */
}

#ifdef CONFIG_CANNECTIVITY_DFU_REBOOT_DELAY
static void cannectivity_usb_reboot(struct k_work *work)
{
	ARG_UNUSED(work);

	LOG_INF("rebooting");
	sys_reboot(SYS_REBOOT_COLD);
}
#endif /* CONFIG_CANNECTIVITY_DFU_REBOOT_DELAY */

static void cannectivity_usb_msg_cb(struct usbd_context *const usbd_ctx,
				    const struct usbd_msg *const msg)
{
	if (msg->type == USBD_MSG_DFU_APP_DETACH) {
		cannectivity_usb_switch_to_dfu_mode();
	}

	if (msg->type == USBD_MSG_DFU_DOWNLOAD_COMPLETED) {
		LOG_INF("DFU download completed, reboot needed");
		boot_request_upgrade(BOOT_UPGRADE_TEST);

#ifdef CONFIG_CANNECTIVITY_DFU_REBOOT_DELAY
		k_work_init_delayable(&usb_reboot_work, cannectivity_usb_reboot);
		k_work_schedule(&usb_reboot_work, K_MSEC(CONFIG_CANNECTIVITY_DFU_REBOOT_DELAY));
#endif /* CONFIG_CANNECTIVITY_DFU_REBOOT_DELAY */
	}
}
#endif /* CONFIG_CANNECTIVITY_DFU_BACKEND_APP */

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

	if (USBD_SUPPORTS_HIGH_SPEED && usbd_caps_speed(&usbd) == USBD_SPEED_HS) {
		err = usbd_add_configuration(&usbd, USBD_SPEED_HS, &hs_config);
		if (err != 0) {
			LOG_ERR("failed to add high-speed configuration (err %d)", err);
			return err;
		}

		err = usbd_register_class(&usbd, GS_USB_CLASS_INSTANCE_NAME, USBD_SPEED_HS, 1);
		if (err != 0) {
			LOG_ERR("failed to register high-speed class gs_usb instance (err %d)",
				err);
			return err;
		}

#ifdef CONFIG_CANNECTIVITY_DFU_BACKEND_APP
		err = usbd_register_class(&usbd, DFU_RUNTIME_CLASS_INSTANCE_NAME, USBD_SPEED_HS, 1);
		if (err != 0) {
			LOG_ERR("failed to register high-speed dfu runtime class instance (err %d)",
				err);
			return err;
		}

		err = usbd_device_set_code_triple(&usbd, USBD_SPEED_HS, USB_BCC_MISCELLANEOUS, 0x02,
						  0x01);
		if (err != 0) {
			LOG_ERR("failed to set high-speed code triple (err %d)", err);
			return err;
		}
#else /* CONFIG_CANNECTIVITY_DFU_BACKEND_APP */
		err = usbd_device_set_code_triple(&usbd, USBD_SPEED_HS, 0, 0, 0);
		if (err != 0) {
			LOG_ERR("failed to set high-speed code triple (err %d)", err);
			return err;
		}
#endif /* !CONFIG_CANNECTIVITY_DFU_BACKEND_APP */

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

	err = usbd_register_class(&usbd, GS_USB_CLASS_INSTANCE_NAME, USBD_SPEED_FS, 1);
	if (err != 0) {
		LOG_ERR("failed to register full-speed gs_usb class instance (err %d)", err);
		return err;
	}

#ifdef CONFIG_CANNECTIVITY_DFU_BACKEND_APP
	err = usbd_register_class(&usbd, DFU_RUNTIME_CLASS_INSTANCE_NAME, USBD_SPEED_FS, 1);
	if (err != 0) {
		LOG_ERR("failed to register full-speed dfu runtime class instance (err %d)",
			err);
		return err;
	}

	err = usbd_device_set_code_triple(&usbd, USBD_SPEED_FS, USB_BCC_MISCELLANEOUS, 0x02, 0x01);
	if (err != 0) {
		LOG_ERR("failed to set full-speed code triple (err %d)", err);
		return err;
	}
#else /* CONFIG_CANNECTIVITY_DFU_BACKEND_APP */
	err = usbd_device_set_code_triple(&usbd, USBD_SPEED_FS, 0, 0, 0);
	if (err != 0) {
		LOG_ERR("failed to set full-speed code triple (err %d)", err);
		return err;
	}
#endif /* !CONFIG_CANNECTIVITY_DFU_BACKEND_APP */

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

	err = usbd_add_descriptor(&usbd, &msosv2);
	if (err != 0) {
		LOG_ERR("failed to add Microsoft OS 2.0 descriptor (err %d)", err);
		return err;
	}

	err = usbd_init(&usbd);
	if (err != 0) {
		LOG_ERR("failed to initialize USB device support (err %d)", err);
		return err;
	}

#ifdef CONFIG_CANNECTIVITY_DFU_BACKEND_APP
	err = usbd_msg_register_cb(&usbd, cannectivity_usb_msg_cb);
	if (err != 0) {
		LOG_ERR("failed to register USB message callback (err %d)", err);
		return err;
	}
#endif /* CONFIG_CANNECTIVITY_DFU_BACKEND_APP */

	err = usbd_enable(&usbd);
	if (err != 0) {
		LOG_ERR("failed to enable USB device");
		return err;
	}

	return 0;
}
#else /* CONFIG_USB_DEVICE_STACK_NEXT */
static int cannectivity_usb_vendorcode_handler(int32_t *tlen, uint8_t **tdata)
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
