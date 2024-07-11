/*
 * Copyright (c) 2022-2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Geschwister Schneider USB/CAN Device Class API
 *
 * +--------+         +----------+--------------+
 * |        |         |          |  Channel 0   |
 * |        |/-------\|          +--------------+
 * |  Host  |   USB   |  Device  |  Channel ... |
 * |        |\-------/|          +--------------+
 * |        |         |          |  Channel N   |
 * +--------+         +----------+--------------+
 *
 */

#ifndef CANNECTIVITY_INCLUDE_USB_CLASS_GS_USB_H_
#define CANNECTIVITY_INCLUDE_USB_CLASS_GS_USB_H_

#include <zephyr/device.h>

/* Software/hardware versions */
#define GS_USB_SW_VERSION 2U
#define GS_USB_HW_VERSION 1U

/* USB requests */
enum {
	GS_USB_REQUEST_HOST_FORMAT = 0,
	GS_USB_REQUEST_BITTIMING,
	GS_USB_REQUEST_MODE,
	GS_USB_REQUEST_BERR, /* Unsupported */
	GS_USB_REQUEST_BT_CONST,
	GS_USB_REQUEST_DEVICE_CONFIG,
	GS_USB_REQUEST_TIMESTAMP,
	GS_USB_REQUEST_IDENTIFY,
	GS_USB_REQUEST_GET_USER_ID, /* Unsupported */
	GS_USB_REQUEST_SET_USER_ID, /* Unsupported */
	GS_USB_REQUEST_DATA_BITTIMING,
	GS_USB_REQUEST_BT_CONST_EXT,
	GS_USB_REQUEST_SET_TERMINATION,
	GS_USB_REQUEST_GET_TERMINATION,
	GS_USB_REQUEST_GET_STATE,
};

/* CAN channel modes */
enum {
	GS_USB_CHANNEL_MODE_RESET = 0,
	GS_USB_CHANNEL_MODE_START,
};

/* CAN channel states */
enum {
	GS_USB_CHANNEL_STATE_ERROR_ACTIVE = 0,
	GS_USB_CHANNEL_STATE_ERROR_WARNING,
	GS_USB_CHANNEL_STATE_ERROR_PASSIVE,
	GS_USB_CHANNEL_STATE_BUS_OFF,
	GS_USB_CHANNEL_STATE_STOPPED,
	GS_USB_CHANNEL_STATE_SLEEPING,
};

/* CAN channel identify modes */
enum {
	GS_USB_CHANNEL_IDENTIFY_MODE_OFF = 0,
	GS_USB_CHANNEL_IDENTIFY_MODE_ON,
};

/* CAN channel termination states */
enum {
	GS_USB_CHANNEL_TERMINATION_STATE_OFF = 0,
	GS_USB_CHANNEL_TERMINATION_STATE_ON,
};

/* CAN channel features */
#define GS_USB_CAN_FEATURE_LISTEN_ONLY              BIT(0)
#define GS_USB_CAN_FEATURE_LOOP_BACK                BIT(1)
#define GS_USB_CAN_FEATURE_TRIPLE_SAMPLE            BIT(2)
#define GS_USB_CAN_FEATURE_ONE_SHOT                 BIT(3)
#define GS_USB_CAN_FEATURE_HW_TIMESTAMP             BIT(4)
#define GS_USB_CAN_FEATURE_IDENTIFY                 BIT(5)
#define GS_USB_CAN_FEATURE_USER_ID                  BIT(6)
#define GS_USB_CAN_FEATURE_PAD_PKTS_TO_MAX_PKT_SIZE BIT(7) /* Unsupported */
#define GS_USB_CAN_FEATURE_FD                       BIT(8)
#define GS_USB_CAN_FEATURE_REQ_USB_QUIRK_LPC546XX   BIT(9) /* Unused */
#define GS_USB_CAN_FEATURE_BT_CONST_EXT             BIT(10)
#define GS_USB_CAN_FEATURE_TERMINATION              BIT(11)
#define GS_USB_CAN_FEATURE_BERR_REPORTING           BIT(12) /* Unsupported (always enabled) */
#define GS_USB_CAN_FEATURE_GET_STATE                BIT(13)

/* CAN channel flags (bit positions match corresponding channel feature bits) */
#define GS_USB_CAN_MODE_NORMAL                   0U
#define GS_USB_CAN_MODE_LISTEN_ONLY              BIT(0)
#define GS_USB_CAN_MODE_LOOP_BACK                BIT(1)
#define GS_USB_CAN_MODE_TRIPLE_SAMPLE            BIT(2)
#define GS_USB_CAN_MODE_ONE_SHOT                 BIT(3)
#define GS_USB_CAN_MODE_HW_TIMESTAMP             BIT(4)
#define GS_USB_CAN_MODE_PAD_PKTS_TO_MAX_PKT_SIZE BIT(7) /* Unsupported */
#define GS_USB_CAN_MODE_FD                       BIT(8)
#define GS_USB_CAN_MODE_BERR_REPORTING           BIT(12) /* Unsupported (always enabled) */

/* Host frame CAN flags */
#define GS_USB_CAN_FLAG_OVERFLOW BIT(0)
#define GS_USB_CAN_FLAG_FD       BIT(1)
#define GS_USB_CAN_FLAG_BRS      BIT(2)
#define GS_USB_CAN_FLAG_ESI      BIT(3)

/* CAN ID flags (nonexhaustive) */
#define GS_USB_CAN_ID_FLAG_ERR_CRTL      BIT(2)
#define GS_USB_CAN_ID_FLAG_ERR_BUSOFF    BIT(6)
#define GS_USB_CAN_ID_FLAG_ERR_RESTARTED BIT(8)
#define GS_USB_CAN_ID_FLAG_ERR_CNT       BIT(9)
#define GS_USB_CAN_ID_FLAG_ERR           BIT(29)
#define GS_USB_CAN_ID_FLAG_RTR           BIT(30)
#define GS_USB_CAN_ID_FLAG_IDE           BIT(31)

/* CAN controller error flags (nonexhaustive, set in payload[1]) */
#define GS_USB_CAN_ERR_CRTL_RX_WARNING BIT(2)
#define GS_USB_CAN_ERR_CRTL_TX_WARNING BIT(3)
#define GS_USB_CAN_ERR_CRTL_RX_PASSIVE BIT(4)
#define GS_USB_CAN_ERR_CRTL_TX_PASSIVE BIT(5)
#define GS_USB_CAN_ERR_CRTL_ACTIVE     BIT(6)

/* Supported host byte order formats */
#define GS_USB_HOST_FORMAT_LITTLE_ENDIAN 0x0000beef

/* Host frame echo ID for RX frames */
#define GS_USB_HOST_FRAME_ECHO_ID_RX_FRAME UINT32_MAX

struct gs_usb_host_config {
	uint32_t byte_order;
} __packed;

struct gs_usb_device_config {
	uint8_t reserved1;
	uint8_t reserved2;
	uint8_t reserved3;
	uint8_t nchannels;
	uint32_t sw_version;
	uint32_t hw_version;
} __packed;

struct gs_usb_device_mode {
	uint32_t mode;
	uint32_t flags;
} __packed;

struct gs_usb_device_state {
	uint32_t state;
	uint32_t rxerr;
	uint32_t txerr;
} __packed;

struct gs_usb_device_bittiming {
	uint32_t prop_seg;
	uint32_t phase_seg1;
	uint32_t phase_seg2;
	uint32_t sjw;
	uint32_t brp;
} __packed;

struct gs_usb_identify_mode {
	uint32_t mode;
} __packed;

struct gs_usb_device_termination_state {
	uint32_t state;
} __packed;

struct gs_usb_device_bt_const {
	uint32_t feature;
	uint32_t fclk_can;
	uint32_t tseg1_min;
	uint32_t tseg1_max;
	uint32_t tseg2_min;
	uint32_t tseg2_max;
	uint32_t sjw_max;
	uint32_t brp_min;
	uint32_t brp_max;
	uint32_t brp_inc;
} __packed;

struct gs_usb_device_bt_const_ext {
	uint32_t feature;
	uint32_t fclk_can;
	uint32_t tseg1_min;
	uint32_t tseg1_max;
	uint32_t tseg2_min;
	uint32_t tseg2_max;
	uint32_t sjw_max;
	uint32_t brp_min;
	uint32_t brp_max;
	uint32_t brp_inc;
	uint32_t dtseg1_min;
	uint32_t dtseg1_max;
	uint32_t dtseg2_min;
	uint32_t dtseg2_max;
	uint32_t dsjw_max;
	uint32_t dbrp_min;
	uint32_t dbrp_max;
	uint32_t dbrp_inc;
} __packed;

struct gs_usb_can_frame {
	uint8_t data[8];
} __packed __aligned(4);

struct gs_usb_canfd_frame {
	uint8_t data[64];
} __packed __aligned(4);

struct gs_usb_host_frame_hdr {
	uint32_t echo_id;
	uint32_t can_id;
	uint8_t can_dlc;
	uint8_t channel;
	uint8_t flags;
	uint8_t reserved;
} __packed __aligned(4);

/* USB endpoint addresses
 *
 * Existing drivers expect endpoints 0x81 and 0x02. Include a dummy endpoint
 * 0x01 to work-around the endpoint address fixup.
 */
#define GS_USB_IN_EP_ADDR    0x81
#define GS_USB_DUMMY_EP_ADDR 0x01
#define GS_USB_OUT_EP_ADDR   0x02

#if defined(CONFIG_USB_DEVICE_GS_USB_TIMESTAMP) || defined(CONFIG_USBD_GS_USB_TIMESTAMP)
#define GS_USB_TIMESTAMP_SIZE sizeof(uint32_t)
#else
#define GS_USB_TIMESTAMP_SIZE 0U
#endif

/* Host frame sizes */
#define GS_USB_HOST_FRAME_CAN_FRAME_SIZE                                                           \
	(sizeof(struct gs_usb_host_frame_hdr) + sizeof(struct gs_usb_can_frame)) +                 \
		GS_USB_TIMESTAMP_SIZE
#define GS_USB_HOST_FRAME_CANFD_FRAME_SIZE                                                         \
	(sizeof(struct gs_usb_host_frame_hdr) + sizeof(struct gs_usb_canfd_frame)) +               \
		GS_USB_TIMESTAMP_SIZE

#ifdef CONFIG_CAN_FD_MODE
#define GS_USB_HOST_FRAME_MAX_SIZE GS_USB_HOST_FRAME_CANFD_FRAME_SIZE
#else /* CONFIG_CAN_FD_MODE */
#define GS_USB_HOST_FRAME_MAX_SIZE GS_USB_HOST_FRAME_CAN_FRAME_SIZE
#endif /* !CONFIG_CAN_FD_MODE */

#define GS_USB_MS_VENDORCODE 0xaa

#ifdef CONFIG_USB_DEVICE_GS_USB
typedef int (*gs_usb_vendorcode_callback_t)(int32_t *tlen, uint8_t **tdata);

void gs_usb_register_vendorcode_callback(gs_usb_vendorcode_callback_t callback);
#endif /* CONFIG_USB_DEVICE_GS_USB */

typedef int (*gs_usb_timestamp_callback_t)(const struct device *dev, uint32_t *timestamp,
					   void *user_data);

typedef int (*gs_usb_identify_callback_t)(const struct device *dev, uint16_t ch, bool identify,
					  void *user_data);

typedef int (*gs_usb_set_termination_callback_t)(const struct device *dev, uint16_t ch,
						 bool terminate, void *user_data);

typedef int (*gs_usb_get_termination_callback_t)(const struct device *dev, uint16_t ch,
						 bool *terminated, void *user_data);

typedef int (*gs_usb_state_callback_t)(const struct device *dev, uint16_t ch, bool started,
				       void *user_data);

typedef int (*gs_usb_activity_callback_t)(const struct device *dev, uint16_t ch, void *user_data);

struct gs_usb_ops {
#if defined(CONFIG_USB_DEVICE_GS_USB_TIMESTAMP) || defined(CONFIG_USBD_GS_USB_TIMESTAMP)
	gs_usb_timestamp_callback_t timestamp;
#endif
#if defined(CONFIG_USB_DEVICE_GS_USB_TERMINATION) || defined(CONFIG_USBD_GS_USB_TERMINATION)
	gs_usb_set_termination_callback_t set_termination;
	gs_usb_get_termination_callback_t get_termination;
#endif
#if defined(CONFIG_USB_DEVICE_GS_USB_IDENTIFICATION) || defined(CONFIG_USBD_GS_USB_IDENTIFICATION)
	gs_usb_identify_callback_t identify;
#endif
	gs_usb_state_callback_t state;
	gs_usb_activity_callback_t activity;
};

int gs_usb_register(const struct device *dev, const struct device **channels, size_t nchannels,
		    const struct gs_usb_ops *ops, void *user_data);

#endif /* CANNECTIVITY_INCLUDE_USB_CLASS_GS_USB_H_ */
