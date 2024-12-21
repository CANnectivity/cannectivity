/*
 * Copyright (c) 2022-2024 Alexander Kozhinov <ak.alexander.kozhinov@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CANNECTIVITY_INCLUDE_GS_USB_DEVICE_DEFINE_DUMMY_H_
#define CANNECTIVITY_INCLUDE_GS_USB_DEVICE_DEFINE_DUMMY_H_

#define GS_USB_DEVICE_DEFINE(inst)                                                                 \
	BUILD_ASSERT(DT_INST_ON_BUS(inst, usb),                                                    \
		     "node " DT_NODE_PATH(                                                         \
			     DT_DRV_INST(inst)) " is not assigned to a USB device controller");    \
                                                                                                   \
	NET_BUF_POOL_FIXED_DEFINE(gs_usb_pool_##inst, CONFIG_USB_DEVICE_GS_USB_POOL_SIZE,          \
				  GS_USB_HOST_FRAME_MAX_SIZE, 0, NULL);                            \
                                                                                                   \
	USBD_CLASS_DESCR_DEFINE(primary, 0)                                                        \
	struct gs_usb_config gs_usb_config_##inst = {                                              \
		.iad = INITIALIZER_IAD,                                                            \
		.if0 = INITIALIZER_IF,                                                             \
		.if0_in_ep = INITIALIZER_IF_EP(GS_USB_IN_EP_ADDR),                                 \
		.if0_dummy_ep = INITIALIZER_IF_EP(GS_USB_DUMMY_EP_ADDR),                           \
		.if0_out_ep = INITIALIZER_IF_EP(GS_USB_OUT_EP_ADDR),                               \
	};                                                                                         \
                                                                                                   \
	static struct usb_ep_cfg_data gs_usb_ep_cfg_data_##inst[] = {                              \
		{                                                                                  \
			.ep_cb = usb_transfer_ep_callback,                                         \
			.ep_addr = GS_USB_IN_EP_ADDR,                                              \
		},                                                                                 \
		{                                                                                  \
			.ep_cb = usb_transfer_ep_callback,                                         \
			.ep_addr = GS_USB_DUMMY_EP_ADDR,                                           \
		},                                                                                 \
		{                                                                                  \
			.ep_cb = usb_transfer_ep_callback,                                         \
			.ep_addr = GS_USB_OUT_EP_ADDR,                                             \
		},                                                                                 \
	};                                                                                         \
                                                                                                   \
	USBD_DEFINE_CFG_DATA(gs_usb_cfg_##inst) = {                                                \
		.usb_device_description = NULL,                                                    \
		.interface_config = gs_usb_interface_config,                                       \
		.interface_descriptor = &gs_usb_config_##inst.if0,                                 \
		.cb_usb_status = gs_usb_status_callback,                                           \
		.interface = {                                                                     \
			.class_handler = NULL,                                                     \
			.custom_handler = NULL,                                                    \
			.vendor_handler = gs_usb_vendor_request_handler,                           \
		},                                                                                 \
		.num_endpoints = ARRAY_SIZE(gs_usb_ep_cfg_data_##inst),                            \
		.endpoint = gs_usb_ep_cfg_data_##inst,                                             \
	};                                                                                         \
                                                                                                   \
	static struct gs_usb_data gs_usb_data_##inst = {                                           \
		.pool = &gs_usb_pool_##inst,                                                       \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, gs_usb_init, NULL, &gs_usb_data_##inst, &gs_usb_cfg_##inst,    \
			      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, NULL);

#endif  /* CANNECTIVITY_INCLUDE_GS_USB_DEVICE_DEFINE_DUMMY_H_ */
