/*
 * Copyright (c) 2022-2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Geschwister Schneider USB/CAN Device Class API
 *
 * The Geschwister Schneider USB/CAN protocol supports 1 to 255 independent CAN channels per USB
 * device, each corresponding to a CAN controller.
 *
 * @code{.text}
 * +--------+         +----------+--------------+
 * |        |         |          |  Channel 0   |
 * |        |--------\|          +--------------+
 * |  Host  |   USB   |  Device  |  Channel ... |
 * |        |--------/|          +--------------+
 * |        |         |          |  Channel N   |
 * +--------+         +----------+--------------+
 * @endcode
 *
 */

#ifndef CANNECTIVITY_INCLUDE_USB_CLASS_GS_USB_H_
#define CANNECTIVITY_INCLUDE_USB_CLASS_GS_USB_H_

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name Geschwister Schneider USB/CAN protocol software/hardware version definitions
 * @{
 */

/**
 * @brief Software version
 */
#define GS_USB_SW_VERSION 2U

/**
 * @brief Hardware version
 */
#define GS_USB_HW_VERSION 1U

/** @} */

/**
 * @brief Geschwister Schneider USB/CAN protocol USB bRequest types
 */
enum {
	/** Host format (little endian vs. big endian) */
	GS_USB_REQUEST_HOST_FORMAT = 0,
	/** Set CAN channel bit timing (CAN classic) */
	GS_USB_REQUEST_BITTIMING,
	/** Set CAN channel operational mode */
	GS_USB_REQUEST_MODE,
	/** CAN channel bus error (unsupported) */
	GS_USB_REQUEST_BERR,
	/** Get CAN channel bit timing limits (CAN classic) */
	GS_USB_REQUEST_BT_CONST,
	/** Get device configuration */
	GS_USB_REQUEST_DEVICE_CONFIG,
	/** Get device hardware timestamp */
	GS_USB_REQUEST_TIMESTAMP,
	/** Set CAN channel identify */
	GS_USB_REQUEST_IDENTIFY,
	/** Get device user ID (unsupported) */
	GS_USB_REQUEST_GET_USER_ID,
	/** Set device user ID  (unsupported) */
	GS_USB_REQUEST_SET_USER_ID,
	/** Set CAN channel bit timing (CAN FD data phase) */
	GS_USB_REQUEST_DATA_BITTIMING,
	/** Get CAN channel bit timing limits (CAN FD) */
	GS_USB_REQUEST_BT_CONST_EXT,
	/** Set CAN channel bus termination */
	GS_USB_REQUEST_SET_TERMINATION,
	/** Get CAN channel bus termination */
	GS_USB_REQUEST_GET_TERMINATION,
	/** Get CAN channel bus state */
	GS_USB_REQUEST_GET_STATE,
};

/**
 * @brief Geschwister Schneider USB/CAN protocol CAN channel modes
 */
enum {
	/** Reset CAN channel */
	GS_USB_CHANNEL_MODE_RESET = 0,
	/** Start CAN channel */
	GS_USB_CHANNEL_MODE_START,
};

/**
 * @brief Geschwister Schneider USB/CAN protocol CAN channel states
 */
enum {
	/** Error-active state (RX/TX error count < 96). */
	GS_USB_CHANNEL_STATE_ERROR_ACTIVE = 0,
	/** Error-warning state (RX/TX error count < 128). */
	GS_USB_CHANNEL_STATE_ERROR_WARNING,
	/** Error-passive state (RX/TX error count < 256). */
	GS_USB_CHANNEL_STATE_ERROR_PASSIVE,
	/** Bus-off state (RX/TX error count >= 256). */
	GS_USB_CHANNEL_STATE_BUS_OFF,
	/** CAN controller is stopped and does not participate in CAN communication. */
	GS_USB_CHANNEL_STATE_STOPPED,
	/** CAN controller is sleeping (unused) */
	GS_USB_CHANNEL_STATE_SLEEPING,
};

/**
 * @brief Geschwister Schneider USB/CAN protocol CAN channel identify modes
 */
enum {
	/** Identify mode off */
	GS_USB_CHANNEL_IDENTIFY_MODE_OFF = 0,
	/** Identify mode on */
	GS_USB_CHANNEL_IDENTIFY_MODE_ON,
};

/**
 * @brief Geschwister Schneider USB/CAN protocol CAN channel termination states
 */
enum {
	/** Termination off */
	GS_USB_CHANNEL_TERMINATION_STATE_OFF = 0,
	/** Termination on */
	GS_USB_CHANNEL_TERMINATION_STATE_ON,
};

/**
 * @name Geschwister Schneider USB/CAN protocol CAN channel features
 * @{
 */

/** CAN channel supports listen-onlu mode, in which it is not allowed to send dominant bits. */
#define GS_USB_CAN_FEATURE_LISTEN_ONLY              BIT(0)
/** CAN channel supports in loopback mode, which it receives own frames. */
#define GS_USB_CAN_FEATURE_LOOP_BACK                BIT(1)
/** CAN channel supports triple sampling mode */
#define GS_USB_CAN_FEATURE_TRIPLE_SAMPLE            BIT(2)
/** CAN channel supports not retransmitting in case of lost arbitration or missing ACK. */
#define GS_USB_CAN_FEATURE_ONE_SHOT                 BIT(3)
/** CAN channel supports hardware timestamping of CAN frames. */
#define GS_USB_CAN_FEATURE_HW_TIMESTAMP             BIT(4)
/** CAN channel supports visual identification. */
#define GS_USB_CAN_FEATURE_IDENTIFY                 BIT(5)
/** CAN channel supports user IDs (unsupported). */
#define GS_USB_CAN_FEATURE_USER_ID                  BIT(6)
/** CAN channel supports padding of host frames (unsupported). */
#define GS_USB_CAN_FEATURE_PAD_PKTS_TO_MAX_PKT_SIZE BIT(7)
/** CAN channel supports transmitting/receiving CAN FD frames. */
#define GS_USB_CAN_FEATURE_FD                       BIT(8)
/** CAN channel support LCP546xx specific quirks (Unused) */
#define GS_USB_CAN_FEATURE_REQ_USB_QUIRK_LPC546XX   BIT(9)
/** CAN channel supports extended bit timing limits. */
#define GS_USB_CAN_FEATURE_BT_CONST_EXT             BIT(10)
/** CAN channel supports configurable bus termination. */
#define GS_USB_CAN_FEATURE_TERMINATION              BIT(11)
/** CAN channel supports bus error reporting (Unsupported, always enabled) */
#define GS_USB_CAN_FEATURE_BERR_REPORTING           BIT(12)
/** CAN channel supports reporting of bus state. */
#define GS_USB_CAN_FEATURE_GET_STATE                BIT(13)

/** @} */

/**
 * @name Geschwister Schneider USB/CAN protocol CAN channel flags
 *
 * Bit positions match corresponding channel feature bits.
 * @{
 */

/** CAN channel is in normal mode. */
#define GS_USB_CAN_MODE_NORMAL                   0U
/** CAN channel is not allowed to send dominant bits. */
#define GS_USB_CAN_MODE_LISTEN_ONLY              BIT(0)
/** CAN channel is in loopback mode (receives own frames). */
#define GS_USB_CAN_MODE_LOOP_BACK                BIT(1)
/** CAN channel uses triple sampling mode */
#define GS_USB_CAN_MODE_TRIPLE_SAMPLE            BIT(2)
/** CAN channel does not retransmit in case of lost arbitration or missing ACK */
#define GS_USB_CAN_MODE_ONE_SHOT                 BIT(3)
/** CAN channel frames are timestamped. */
#define GS_USB_CAN_MODE_HW_TIMESTAMP             BIT(4)
/** CAN channel host frames are padded (unsupported). */
#define GS_USB_CAN_MODE_PAD_PKTS_TO_MAX_PKT_SIZE BIT(7)
/** CAN channel allows transmitting/receiving CAN FD frames. */
#define GS_USB_CAN_MODE_FD                       BIT(8)
/** CAN channel uses bus error reporting (unsupported, always enabled). */
#define GS_USB_CAN_MODE_BERR_REPORTING           BIT(12)

/** @} */

/**
 * @name Geschwister Schneider USB/CAN protocol host frame CAN flags
 * @{
 */

/** RX overflow occurred. */
#define GS_USB_CAN_FLAG_OVERFLOW BIT(0)
/** CAN frame is in CAN FD frame format. */
#define GS_USB_CAN_FLAG_FD       BIT(1)
/** CAN frame uses CAN FD Baud Rate Switch (BRS). */
#define GS_USB_CAN_FLAG_BRS      BIT(2)
/** CAN frame has the CAN FD Error State Indicator (ESI) set. */
#define GS_USB_CAN_FLAG_ESI      BIT(3)

/** @} */

/**
 * @name Geschwister Schneider USB/CAN protocol host frame CAN ID flags (nonexhaustive)
 *
 * These correspond to the definitions in linux/include/uapi/linux/can.h and
 * linux/include/uapi/linux/can/error.h
 *
 * @{
 */

/** CAN controller errors, details in data[1] */
#define GS_USB_CAN_ID_FLAG_ERR_CRTL      BIT(2)
/** CAN controller is in bus off state */
#define GS_USB_CAN_ID_FLAG_ERR_BUSOFF    BIT(6)
/** CAN controller restarted */
#define GS_USB_CAN_ID_FLAG_ERR_RESTARTED BIT(8)
/** CAN controller TX/RX error counters in data[6]/data[7] */
#define GS_USB_CAN_ID_FLAG_ERR_CNT       BIT(9)
/** CAN frame is an error frame */
#define GS_USB_CAN_ID_FLAG_ERR           BIT(29)
/** CAN frame is a Remote Transmission Request (RTR) frame */
#define GS_USB_CAN_ID_FLAG_RTR           BIT(30)
/** CAN frame uses extended (29-bit) CAN ID */
#define GS_USB_CAN_ID_FLAG_IDE           BIT(31)

/** @} */

/**
 * @name Geschwister Schneider USB/CAN protocol CAN controller error flags (nonexhaustive)
 *
 * These are set in data[1] for CAN error frames and correspond to the definitions in
 * linux/include/uapi/linux/can/error.h
 *
 * @{
 */

/** RX error-warning state (RX error count < 128). */
#define GS_USB_CAN_ERR_CRTL_RX_WARNING BIT(2)
/** TX error-warning state (TX error count < 128). */
#define GS_USB_CAN_ERR_CRTL_TX_WARNING BIT(3)
/** RX error-passive state (RX error count < 256). */
#define GS_USB_CAN_ERR_CRTL_RX_PASSIVE BIT(4)
/** TX error-passive state (TX error count < 256). */
#define GS_USB_CAN_ERR_CRTL_TX_PASSIVE BIT(5)
/** Error-active state (RX/TX error count < 96). */
#define GS_USB_CAN_ERR_CRTL_ACTIVE     BIT(6)

/** @} */

/**
 * @brief Geschwister Schneider USB/CAN protocol supported host byte order format
 */
#define GS_USB_HOST_FORMAT_LITTLE_ENDIAN 0x0000beef

/**
 * @brief Geschwister Schneider USB/CAN protocol host frame echo ID for RX frames
 */
#define GS_USB_HOST_FRAME_ECHO_ID_RX_FRAME UINT32_MAX

/**
 * @brief Geschwister Schneider USB/CAN protocol @a GS_USB_REQUEST_HOST_FORMAT payload
 */
struct gs_usb_host_config {
	/** Byte order identification string, see @a GS_USB_HOST_FORMAT_LITTLE_ENDIAN */
	uint32_t byte_order;
} __packed;

/**
 * @brief Geschwister Schneider USB/CAN protocol @a GS_USB_REQUEST_DEVICE_CONFIG payload
 */
struct gs_usb_device_config {
	/** Reserved */
	uint8_t reserved1;
	/** Reserved */
	uint8_t reserved2;
	/** Reserved */
	uint8_t reserved3;
	/** Number of CAN channels on the device minus 1 (a value of zero means one channel) */
	uint8_t nchannels;
	/** Device software version */
	uint32_t sw_version;
	/** Device hardware version */
	uint32_t hw_version;
} __packed;

/**
 * @brief Geschwister Schneider USB/CAN protocol @a GS_USB_REQUEST_MODE payload
 */
struct gs_usb_device_mode {
	/** CAN channel mode */
	uint32_t mode;
	/** CAN channel flags */
	uint32_t flags;
} __packed;

/**
 * @brief Geschwister Schneider USB/CAN protocol @a GS_USB_REQUEST_GET_STATE payload
 */
struct gs_usb_device_state {
	/** CAN channel state */
	uint32_t state;
	/** CAN channel RX bus error count */
	uint32_t rxerr;
	/** CAN channel TX bus error count */
	uint32_t txerr;
} __packed;

/**
 * @brief Geschwister Schneider USB/CAN protocol @a GS_USB_REQUEST_BITTIMING payload
 */
struct gs_usb_device_bittiming {
	/** Propagation segment (tq) */
	uint32_t prop_seg;
	/** Phase segment 1 (tq) */
	uint32_t phase_seg1;
	/** Phase segment 1 (tq) */
	uint32_t phase_seg2;
	/** Synchronisation jump width (tq) */
	uint32_t sjw;
	/** Bitrate prescaler */
	uint32_t brp;
} __packed;

/**
 * @brief Geschwister Schneider USB/CAN protocol @a GS_USB_REQUEST_IDENTIFY payload
 */
struct gs_usb_identify_mode {
	/** @a GS_USB_CHANNEL_IDENTIFY_MODE_OFF or @a GS_USB_CHANNEL_IDENTIFY_MODE_ON */
	uint32_t mode;
} __packed;


/**
 * @brief Geschwister Schneider USB/CAN protocol @a GS_USB_REQUEST_SET_TERMINATION and @a
 *        GS_USB_REQUEST_GET_TERMINATION payload
 */
struct gs_usb_device_termination_state {
	/** @a GS_USB_CHANNEL_TERMINATION_STATE_OFF or @a GS_USB_CHANNEL_TERMINATION_STATE_ON */
	uint32_t state;
} __packed;

/**
 * @brief Geschwister Schneider USB/CAN protocol @a GS_USB_REQUEST_BT_CONST payload
 */
struct gs_usb_device_bt_const {
	/** Supported CAN channel features */
	uint32_t feature;
	/** CAN core clock frequency */
	uint32_t fclk_can;
	/** Time segment 1 minimum value (tq) */
	uint32_t tseg1_min;
	/** Time segment 1 maximum value (tq) */
	uint32_t tseg1_max;
	/** Time segment 2 minimum value (tq) */
	uint32_t tseg2_min;
	/** Time segment 2 maximum value (tq) */
	uint32_t tseg2_max;
	/** Synchronisation jump width (SJW) maximum value (tq) */
	uint32_t sjw_max;
	/** Bitrate prescaler minimum value */
	uint32_t brp_min;
	/** Bitrate prescaler maximum value */
	uint32_t brp_max;
	/** Bitrate prescaler increment */
	uint32_t brp_inc;
} __packed;

/**
 * @brief Geschwister Schneider USB/CAN protocol @a GS_USB_REQUEST_BT_CONST_EXT payload
 */
struct gs_usb_device_bt_const_ext {
	/** Supported CAN channel features */
	uint32_t feature;
	/** CAN core clock frequency */
	uint32_t fclk_can;
	/** Time segment 1 minimum value (tq) */
	uint32_t tseg1_min;
	/** Time segment 1 maximum value (tq) */
	uint32_t tseg1_max;
	/** Time segment 2 minimum value (tq) */
	uint32_t tseg2_min;
	/** Time segment 2 maximum value (tq) */
	uint32_t tseg2_max;
	/** Synchronisation jump width (SJW) maximum value (tq) */
	uint32_t sjw_max;
	/** Bitrate prescaler minimum value */
	uint32_t brp_min;
	/** Bitrate prescaler maximum value */
	uint32_t brp_max;
	/** Bitrate prescaler increment */
	uint32_t brp_inc;
	/** Data phase time segment 1 minimum value (tq) */
	uint32_t dtseg1_min;
	/** Data phase time segment 1 maximum value (tq) */
	uint32_t dtseg1_max;
	/** Data phase time segment 2 minimum value (tq) */
	uint32_t dtseg2_min;
	/** Data phase time segment 2 maximum value (tq) */
	uint32_t dtseg2_max;
	/** Data phasde synchronisation jump width (SJW) maximum value (tq) */
	uint32_t dsjw_max;
	/** Data phase bitrate prescaler minimum value */
	uint32_t dbrp_min;
	/** Data phase bitrate prescaler maximum value */
	uint32_t dbrp_max;
	/** Data phase bitrate prescaler increment */
	uint32_t dbrp_inc;
} __packed;

/**
 * @brief Geschwister Schneider USB/CAN protocol CAN classic host frame data
 */
struct gs_usb_can_frame {
	/** CAN frame payload */
	uint8_t data[8];
} __packed __aligned(4);

/**
 * @brief Geschwister Schneider USB/CAN protocol CAN FD host frame data
 */
struct gs_usb_canfd_frame {
	/** CAN frame payload */
	uint8_t data[64];
} __packed __aligned(4);

/**
 * @brief Geschwister Schneider USB/CAN protocol host frame header
 */
struct gs_usb_host_frame_hdr {
	/** Echo ID */
	uint32_t echo_id;
	/** CAN ID */
	uint32_t can_id;
	/** CAN DLC */
	uint8_t can_dlc;
	/** CAN channel */
	uint8_t channel;
	/** Host frame flags */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved;
} __packed __aligned(4);

/**
 * @name USB endpoint addresses
 *
 * Existing drivers expect endpoints 0x81 and 0x02. Include a dummy endpoint 0x01 to work-around the
 * endpoint address fixup of the Zephyr USB device stack.
 *
 * @{
 */

/** USB bulk IN endpoint address */
#define GS_USB_IN_EP_ADDR    0x81
/** USB (dummy) bulk OUT endpoint address */
#define GS_USB_DUMMY_EP_ADDR 0x01
/** USB bulk OUT endpoint address */
#define GS_USB_OUT_EP_ADDR   0x02

/** @} */

/**
 * @brief Geschwister Schneider USB/CAN protocol timestamp field size
 */
#if defined(CONFIG_USB_DEVICE_GS_USB_TIMESTAMP) || defined(CONFIG_USBD_GS_USB_TIMESTAMP)
#define GS_USB_TIMESTAMP_SIZE sizeof(uint32_t)
#else
#define GS_USB_TIMESTAMP_SIZE 0U
#endif

/**
 * @name Geschwister Schneider USB/CAN protocol host frame sizes
 *
 * @{
 */

/** CAN classic host frame size */
#define GS_USB_HOST_FRAME_CAN_FRAME_SIZE                                                           \
	(sizeof(struct gs_usb_host_frame_hdr) + sizeof(struct gs_usb_can_frame)) +                 \
		GS_USB_TIMESTAMP_SIZE

/** CAN FD host frame size */
#define GS_USB_HOST_FRAME_CANFD_FRAME_SIZE                                                         \
	(sizeof(struct gs_usb_host_frame_hdr) + sizeof(struct gs_usb_canfd_frame)) +               \
		GS_USB_TIMESTAMP_SIZE

/** Maximum host frame size */
#ifdef CONFIG_CAN_FD_MODE
#define GS_USB_HOST_FRAME_MAX_SIZE GS_USB_HOST_FRAME_CANFD_FRAME_SIZE
#else /* CONFIG_CAN_FD_MODE */
#define GS_USB_HOST_FRAME_MAX_SIZE GS_USB_HOST_FRAME_CAN_FRAME_SIZE
#endif /* !CONFIG_CAN_FD_MODE */

/** @} */

/**
 * @brief Custom (random) MSOSv2 vendor code
 */
#define GS_USB_MS_VENDORCODE 0xaa

#ifdef CONFIG_USB_DEVICE_GS_USB
/**
 * @brief Defines the callback signature for responding to MSOSv2 vendor code USB requests
 *
 * @param[out] tlen Length of the MSOSv2 USB descriptor.
 * @param[out] tdata The MSOSv2 USB descriptor.
 * @return 0 on success, negative error number otherwise.
 */
typedef int (*gs_usb_vendorcode_callback_t)(int32_t *tlen, uint8_t **tdata);

void gs_usb_register_vendorcode_callback(gs_usb_vendorcode_callback_t callback);
#endif /* CONFIG_USB_DEVICE_GS_USB */

/**
 * @brief Defines the callback signature for obtaining a hardware timestamp
 *
 * Provided hardware timestamps must be 32-bit wide, incrementing with a rate of 1MHz. Hardware
 * timestamps are used for timestamping of received and transmitted CAN frames.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param[out] timestamp Current timestamp value.
 * @param user_data User data provided when registering the callback.
 * @return 0 on success, negative error number otherwise.
 */
typedef int (*gs_usb_timestamp_callback_t)(const struct device *dev, uint32_t *timestamp,
					   void *user_data);

/**
 * @brief Defines the callback signature for visually identifying a given CAN channel
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param ch CAN channel number.
 * @param identify True if the channel identify is active, false otherwise.
 * @param user_data User data provided when registering the callback.
 * @return 0 on success, negative error number otherwise.
 */
typedef int (*gs_usb_identify_callback_t)(const struct device *dev, uint16_t ch, bool identify,
					  void *user_data);

/**
 * @brief Defines the callback signature for setting the bus termination of a given CAN channel
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param ch CAN channel number.
 * @param terminate True if the channel termination is active, false otherwise.
 * @param user_data User data provided when registering the callback.
 * @return 0 on success, negative error number otherwise.
 */
typedef int (*gs_usb_set_termination_callback_t)(const struct device *dev, uint16_t ch,
						 bool terminate, void *user_data);

/**
 * @brief Defines the callback signature for getting the bus termination of a given CAN channel
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param ch CAN channel number.
 * @param[out] terminated True if the channel termination is active, false otherwise.
 * @param user_data User data provided when registering the callback.
 * @return 0 on success, negative error number otherwise.
 */
typedef int (*gs_usb_get_termination_callback_t)(const struct device *dev, uint16_t ch,
						 bool *terminated, void *user_data);

/**
 * @brief Defines the callback signature for reporting the state of a given CAN channel
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param ch CAN channel number.
 * @param started True if the channel is started, false otherwise.
 * @param user_data User data provided when registering the callback.
 * @return 0 on success, negative error number otherwise.
 */
typedef int (*gs_usb_state_callback_t)(const struct device *dev, uint16_t ch, bool started,
				       void *user_data);

/**
 * @brief Defines the callback signature for reporting RX/TX activity of a given CAN channel
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param ch CAN channel number.
 * @param user_data User data provided when registering the callback.
 * @return 0 on success, negative error number otherwise.
 */
typedef int (*gs_usb_activity_callback_t)(const struct device *dev, uint16_t ch, void *user_data);

/**
 * @brief Callback operations structure.
 */
struct gs_usb_ops {
#if defined(CONFIG_USB_DEVICE_GS_USB_TIMESTAMP) || defined(CONFIG_USBD_GS_USB_TIMESTAMP)
	/** Optional timestamp callback */
	gs_usb_timestamp_callback_t timestamp;
#endif
#if defined(CONFIG_USB_DEVICE_GS_USB_TERMINATION) || defined(CONFIG_USBD_GS_USB_TERMINATION)
	/** Optional CAN channel set termination callback */
	gs_usb_set_termination_callback_t set_termination;
	/** Optional CAN channel get termination callback */
	gs_usb_get_termination_callback_t get_termination;
#endif
#if defined(CONFIG_USB_DEVICE_GS_USB_IDENTIFICATION) || defined(CONFIG_USBD_GS_USB_IDENTIFICATION)
	/** Optional CAN channel identify callback */
	gs_usb_identify_callback_t identify;
#endif
	/** CAN channel state callback */
	gs_usb_state_callback_t state;
	/** CAN channel activity callback */
	gs_usb_activity_callback_t activity;
};

/**
 * @brief Register a Geschwister Schneider USB/CAN device class driver instance
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param channels Pointer to an array of pointer for the CAN controller driver instances to
 *                 register.
 * @param nchannels Number of entries in the channels array.
 * @param ops Pointer to the callbacks structure.
 * @param user_data User data to pass to the callback functions.
 * @return 0 on success, negative error number otherwise.
 */
int gs_usb_register(const struct device *dev, const struct device **channels, size_t nchannels,
		    const struct gs_usb_ops *ops, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* CANNECTIVITY_INCLUDE_USB_CLASS_GS_USB_H_ */
