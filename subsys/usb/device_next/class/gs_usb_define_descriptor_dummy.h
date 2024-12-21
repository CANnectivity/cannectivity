/*
 * Copyright (c) 2022-2024 Alexander Kozhinov <ak.alexander.kozhinov@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CANNECTIVITY_INCLUDE_GS_USB_DEFINE_DESCRIPTOR_DUMMY_H_
#define CANNECTIVITY_INCLUDE_GS_USB_DEFINE_DESCRIPTOR_DUMMY_H_

#define GS_USB_DEFINE_DESCRIPTOR(n)                                                                \
	static struct gs_usb_desc gs_usb_desc_##n = {                                              \
		.iad = {                                                                           \
				.bLength = sizeof(struct usb_association_descriptor),              \
				.bDescriptorType = USB_DESC_INTERFACE_ASSOC,                       \
				.bFirstInterface = 0,                                              \
				.bInterfaceCount = 1,                                              \
				.bFunctionClass = USB_BCC_VENDOR,                                  \
				.bFunctionSubClass = 0,                                            \
				.bFunctionProtocol = 0,                                            \
				.iFunction = 0,                                                    \
		},                                                                                 \
		.if0 = {                                                                           \
				.bLength = sizeof(struct usb_if_descriptor),                       \
				.bDescriptorType = USB_DESC_INTERFACE,                             \
				.bInterfaceNumber = 0,                                             \
				.bAlternateSetting = 0,                                            \
				.bNumEndpoints = 3,                                                \
				.bInterfaceClass = USB_BCC_VENDOR,                                 \
				.bInterfaceSubClass = 0,                                           \
				.bInterfaceProtocol = 0,                                           \
				.iInterface = 0,                                                   \
		},                                                                                 \
		.if0_in_ep = {                                                                     \
				.bLength = sizeof(struct usb_ep_descriptor),                       \
				.bDescriptorType = USB_DESC_ENDPOINT,                              \
				.bEndpointAddress = GS_USB_IN_EP_ADDR,                             \
				.bmAttributes = USB_EP_TYPE_BULK,                                  \
				.wMaxPacketSize = sys_cpu_to_le16(64),                             \
				.bInterval = 0x00,                                                 \
		},                                                                                 \
		.if0_dummy_ep = {                                                                  \
				.bLength = sizeof(struct usb_ep_descriptor),                       \
				.bDescriptorType = USB_DESC_ENDPOINT,                              \
				.bEndpointAddress = GS_USB_DUMMY_EP_ADDR,                          \
				.bmAttributes = USB_EP_TYPE_BULK,                                  \
				.wMaxPacketSize = sys_cpu_to_le16(64U),                            \
				.bInterval = 0x00,                                                 \
		},                                                                                 \
		.if0_out_ep = {                                                                    \
				.bLength = sizeof(struct usb_ep_descriptor),                       \
				.bDescriptorType = USB_DESC_ENDPOINT,                              \
				.bEndpointAddress = GS_USB_OUT_EP_ADDR,                            \
				.bmAttributes = USB_EP_TYPE_BULK,                                  \
				.wMaxPacketSize = sys_cpu_to_le16(64U),                            \
				.bInterval = 0x00,                                                 \
		},                                                                                 \
		.if0_hs_in_ep = {                                                                  \
				.bLength = sizeof(struct usb_ep_descriptor),                       \
				.bDescriptorType = USB_DESC_ENDPOINT,                              \
				.bEndpointAddress = GS_USB_IN_EP_ADDR,                             \
				.bmAttributes = USB_EP_TYPE_BULK,                                  \
				.wMaxPacketSize = sys_cpu_to_le16(512),                            \
				.bInterval = 0x00,                                                 \
		},                                                                                 \
		.if0_hs_dummy_ep = {                                                               \
				.bLength = sizeof(struct usb_ep_descriptor),                       \
				.bDescriptorType = USB_DESC_ENDPOINT,                              \
				.bEndpointAddress = GS_USB_DUMMY_EP_ADDR,                          \
				.bmAttributes = USB_EP_TYPE_BULK,                                  \
				.wMaxPacketSize = sys_cpu_to_le16(512U),                           \
				.bInterval = 0x00,                                                 \
		},                                                                                 \
		.if0_hs_out_ep = {                                                                 \
				.bLength = sizeof(struct usb_ep_descriptor),                       \
				.bDescriptorType = USB_DESC_ENDPOINT,                              \
				.bEndpointAddress = GS_USB_OUT_EP_ADDR,                            \
				.bmAttributes = USB_EP_TYPE_BULK,                                  \
				.wMaxPacketSize = sys_cpu_to_le16(512U),                           \
				.bInterval = 0x00,                                                 \
		},                                                                                 \
		.nil_desc = {                                                                      \
				.bLength = 0,                                                      \
				.bDescriptorType = 0,                                              \
		},                                                                                 \
	};                                                                                         \
                                                                                                   \
	const static struct usb_desc_header *gs_usb_fs_desc_##n[] = {                              \
		(struct usb_desc_header *)&gs_usb_desc_##n.iad,                                    \
		(struct usb_desc_header *)&gs_usb_desc_##n.if0,                                    \
		(struct usb_desc_header *)&gs_usb_desc_##n.if0_in_ep,                              \
		(struct usb_desc_header *)&gs_usb_desc_##n.if0_dummy_ep,                           \
		(struct usb_desc_header *)&gs_usb_desc_##n.if0_out_ep,                             \
		(struct usb_desc_header *)&gs_usb_desc_##n.nil_desc,                               \
	};                                                                                         \
                                                                                                   \
	const static struct usb_desc_header *gs_usb_hs_desc_##n[] = {                              \
		(struct usb_desc_header *)&gs_usb_desc_##n.iad,                                    \
		(struct usb_desc_header *)&gs_usb_desc_##n.if0,                                    \
		(struct usb_desc_header *)&gs_usb_desc_##n.if0_hs_in_ep,                           \
		(struct usb_desc_header *)&gs_usb_desc_##n.if0_hs_dummy_ep,                        \
		(struct usb_desc_header *)&gs_usb_desc_##n.if0_hs_out_ep,                          \
		(struct usb_desc_header *)&gs_usb_desc_##n.nil_desc,                               \
	}

#endif  /* CANNECTIVITY_INCLUDE_GS_USB_DEFINE_DESCRIPTOR_DUMMY_H_ */
