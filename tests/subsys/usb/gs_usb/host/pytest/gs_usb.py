# SPDX-FileCopyrightText: Copyright (c) 2024-2026 Henrik Brix Andersen <henrik@brixandersen.dk>
#
# SPDX-License-Identifier: Apache-2.0

"""
Utility class implementing the gs_usb protocol.
"""

import logging
import struct

from dataclasses import dataclass, astuple
from enum import IntEnum, IntFlag

import usb.core
import usb.util

logger = logging.getLogger(__name__)

def timeout_ms(timeout: float) -> int:
    """Return a timeout in milliseconds for PyUSB, where 0 means no timeout."""
    return max(1, round(timeout * 1000))

class GsUSBDeviceNotFound(Exception):
    """
    Geschwister Schneider USB/CAN device not found.
    """

class GsUSBbRequest(IntEnum):
    """
    Geschwister Schneider USB/CAN protocol bRequest types.
    """
    # Host format (little endian vs. big endian)
    HOST_FORMAT = 0
    # Set CAN channel bit timing (CAN classic)
    BITTIMING = 1
    # Set CAN channel operational mode
    MODE = 2
    # CAN channel bus error (unsupported)
    BERR = 3
    # Get CAN channel bit timing limits (CAN classic)
    BT_CONST = 4
    # Get device configuration
    DEVICE_CONFIG = 5
    # Get device hardware timestamp
    TIMESTAMP = 6
    # Set CAN channel identify
    IDENTIFY = 7
    # Get device user ID (unsupported)
    GET_USER_ID = 8
    # Set device user ID  (unsupported)
    SET_USER_ID = 9
    # Set CAN channel bit timing (CAN FD data phase)
    DATA_BITTIMING = 10
    # Get CAN channel bit timing limits (CAN FD)
    BT_CONST_EXT = 11
    # Set CAN channel bus termination
    SET_TERMINATION = 12
    # Get CAN channel bus termination
    GET_TERMINATION = 13
    # Get CAN channel bus state
    GET_STATE = 14

class GsUSBCANChannelFeature(IntFlag):
    """
    Geschwister Schneider USB/CAN protocol CAN channel features.
    """
    # CAN channel supports listen-only mode, in which it is not allowed to send dominant bits.
    LISTEN_ONLY = 2**0
    # CAN channel supports loopback mode, in which it receives own frames.
    LOOP_BACK = 2**1
    # CAN channel supports triple sampling mode
    TRIPLE_SAMPLE = 2**2
    # CAN channel supports not retransmitting in case of lost arbitration or missing ACK.
    ONE_SHOT = 2**3
    # CAN channel supports hardware timestamping of CAN frames.
    HW_TIMESTAMP = 2**4
    # CAN channel supports visual identification.
    IDENTIFY = 2**5
    # CAN channel supports user IDs (unsupported).
    USER_ID = 2**6
    # CAN channel supports padding of host frames (unsupported).
    PAD_PKTS_TO_MAX_PKT_SIZE = 2**7
    # CAN channel supports transmitting/receiving CAN FD frames.
    FD = 2**8
    # CAN channel support LCP546xx specific quirks (Unused)
    REQ_USB_QUIRK_LPC546XX = 2**9
    # CAN channel supports extended bit timing limits.
    BT_CONST_EXT = 2**10
    # CAN channel supports configurable bus termination.
    TERMINATION = 2**11
    # CAN channel supports bus error reporting (Unsupported, always enabled)
    BERR_REPORTING = 2**12
    # CAN channel supports reporting of bus state.
    GET_STATE = 2**13

class GsUSBCANChannelMode(IntEnum):
    """
    Geschwister Schneider USB/CAN protocol CAN channel mode.
    """
    # Reset CAN channel
    RESET = 0
    # Start CAN channel
    START = 1

class GsUSBCANChannelFlag(IntFlag):
    """
    Geschwister Schneider USB/CAN protocol CAN channel flags.
    """
    # CAN channel is in normal mode.
    NORMAL = 0
    # CAN channel is not allowed to send dominant bits.
    LISTEN_ONLY = 2**0
    # CAN channel is in loopback mode (receives own frames).
    LOOP_BACK = 2**1
    # CAN channel uses triple sampling mode.
    TRIPLE_SAMPLE = 2**2
    # CAN channel does not retransmit in case of lost arbitration or missing ACK
    ONE_SHOT = 2**3
    # CAN channel frames are timestamped.
    HW_TIMESTAMP = 2**4
    # CAN channel host frames are padded (unsupported).
    PAD_PKTS_TO_MAX_PKT_SIZE = 2**7
    # CAN channel allows transmitting/receiving CAN FD frames.
    FD = 2**8
    # CAN channel uses bus error reporting (unsupported, always enabled).
    BERR_REPORTING = 2**12

class GsUSBCANIDFlag(IntFlag):
    """
    Geschwister Schneider USB/CAN protocol CAN ID flags.
    """
    # CAN controller is in bus off state (error frames only).
    ERR_BUSOFF = 2**6
    # CAN controller restarted (error frames only).
    ERR_RESTARTED = 2**8
    # CAN frame is an error frame.
    ERR = 2**29
    # CAN frame is a Remote Transmission Request (RTR) frame.
    RTR = 2**30
    # CAN frame uses extended (29-bit) CAN ID.
    IDE = 2**31

class GsUSBCANFlag(IntFlag):
    """
    Geschwister Schneider USB/CAN protocol host frame flags.
    """
    # RX overflow occurred.
    OVERFLOW = 2**0
    # CAN frame is in CAN FD frame format.
    FD = 2**1
    # CAN frame uses CAN FD Baud Rate Switch (BRS).
    BRS = 2**2
    # CAN frame has the CAN FD Error State Indicator (ESI) set.
    ESI = 2**3

class GsUSBCANChannelState(IntEnum):
    """
    Geschwister Schneider USB/CAN protocol CAN channel state.
    """
    # Error-active state (RX/TX error count < 96).
    ERROR_ACTIVE = 0
    # Error-warning state (RX/TX error count < 128).
    ERROR_WARNING = 1
    # Error-passive state (RX/TX error count < 256).
    ERROR_PASSIVE = 2
    # Bus-off state (RX/TX error count >= 256).
    BUS_OFF = 3
    # CAN controller is stopped and does not participate in CAN communication.
    STOPPED = 4
    # CAN controller is sleeping (unused)
    SLEEPING = 5

@dataclass
class GsUSBDeviceBTConst: # pylint: disable=too-many-instance-attributes
    """
    Geschwister Schneider USB/CAN protocol CAN classic timing limits.
    """
    # Supported CAN channel features.
    feature: GsUSBCANChannelFeature
    # CAN core clock frequency.
    fclk_can: int
    # Time segment 1 minimum value (tq).
    tseg1_min: int
    # Time segment 1 maximum value (tq).
    tseg1_max: int
    # Time segment 2 minimum value (tq).
    tseg2_min: int
    # Time segment 2 maximum value (tq).
    tseg2_max: int
    # Synchronisation jump width (SJW) maximum value (tq).
    sjw_max: int
    # Bitrate prescaler minimum value.
    brp_min: int
    # Bitrate prescaler maximum value.
    brp_max: int
    # Bitrate prescaler increment.
    brp_inc: int

@dataclass
class GsUSBDeviceBTConstExt: # pylint: disable=too-many-instance-attributes
    """
    Geschwister Schneider USB/CAN protocol CAN classic extended timing limits.
    """
    # Supported CAN channel features.
    feature: GsUSBCANChannelFeature
    # CAN core clock frequency.
    fclk_can: int
    # Time segment 1 minimum value (tq).
    tseg1_min: int
    # Time segment 1 maximum value (tq).
    tseg1_max: int
    # Time segment 2 minimum value (tq).
    tseg2_min: int
    # Time segment 2 maximum value (tq).
    tseg2_max: int
    # Synchronisation jump width (SJW) maximum value (tq).
    sjw_max: int
    # Bitrate prescaler minimum value.
    brp_min: int
    # Bitrate prescaler maximum value.
    brp_max: int
    # Bitrate prescaler increment.
    brp_inc: int
    # Data phase time segment 1 minimum value (tq).
    dtseg1_min: int
    # Data phase time segment 1 maximum value (tq).
    dtseg1_max: int
    # Data phase time segment 2 minimum value (tq).
    dtseg2_min: int
    # Data phase time segment 2 maximum value (tq).
    dtseg2_max: int
    # Data phase synchronisation jump width (SJW) maximum value (tq).
    dsjw_max: int
    # Data phase bitrate prescaler minimum value.
    dbrp_min: int
    # Data phase bitrate prescaler maximum value.
    dbrp_max: int
    # Data phase bitrate prescaler increment.
    dbrp_inc: int

@dataclass
class GsUSBDeviceBittiming:
    """
    Geschwister Schneider USB/CAN protocol device bittiming.
    """
    # Propagation segment (tq) */
    prop_seg: int
    # Phase segment 1 (tq) */
    phase_seg1: int
    # Phase segment 2 (tq) */
    phase_seg2: int
    # Synchronisation jump width (tq) */
    sjw: int
    # Bitrate prescaler */
    brp: int

@dataclass
class GsUSBDeviceConfig:
    """
    Geschwister Schneider USB/CAN protocol device configuration.
    """
    # Number of CAN channels on the device minus 1 (a value of zero means one channel)
    nchannels: int
    # Device software version
    sw_version: int
    # Device hardware version
    hw_version: int

@dataclass
class GsUSBDeviceMode:
    """
    Geschwister Schneider USB/CAN protocol CAN device mode.
    """
    # CAN channel mode.
    mode: int
    # CAN channel flags.
    flags: GsUSBCANChannelFlag

@dataclass
class GsUSBDeviceState:
    """
    Geschwister Schneider USB/CAN protocol CAN device state.
    """
    # CAN channel state.
    state: GsUSBCANChannelState
    # CAN channel RX bus error count.
    rxerr: int
    # CAN channel TX bus error count.
    txerr: int

@dataclass
class GsUSBHostFrame:
    """
    Geschwister Schneider USB/CAN protocol host frame (without timestamp).
    """
    # Echo ID (ECHO_ID_RX for frames received from the CAN bus).
    echo_id: int
    # CAN ID including flags.
    can_id: int
    # CAN frame Data Length Code (DLC).
    can_dlc: int
    # CAN channel.
    channel: int
    # Host frame flags.
    flags: GsUSBCANFlag
    # CAN frame payload, padded to 8 (CAN classic) or 64 (CAN FD) bytes when packed.
    data: bytes = b''

    ECHO_ID_RX = 0xffffffff
    HEADER = struct.Struct('<IIBBBx')

    def pack(self) -> bytes:
        """Return the host frame in USB wire format."""
        size = 64 if self.flags & GsUSBCANFlag.FD else 8
        return self.HEADER.pack(self.echo_id, self.can_id, self.can_dlc, self.channel,
                                self.flags) + self.data.ljust(size, b'\0')

    @classmethod
    def unpack(cls, raw: bytes) -> 'GsUSBHostFrame':
        """Return the host frame contained in a USB transfer."""
        echo_id, can_id, can_dlc, channel, flags = cls.HEADER.unpack_from(raw)
        size = 64 if flags & GsUSBCANFlag.FD else 8
        data = bytes(raw[cls.HEADER.size:cls.HEADER.size + size])
        return cls(echo_id, can_id, can_dlc, channel, GsUSBCANFlag(flags), data)

class GsUSB():
    """
    Utility class implementing the gs_usb protocol.
    """

    def __init__(self, usb_vid: int, usb_pid: int, usb_sn: str) -> None:
        device = None

        if usb_sn is None:
            device = usb.core.find(idVendor=usb_vid, idProduct=usb_pid)
        else:
            devices = usb.core.find(find_all=True, idVendor=usb_vid, idProduct=usb_pid)
            for d in devices:
                if d.serial_number == usb_sn:
                    device = d
                    break

        if device is None:
            logger.error('USB device %04x:%04x S/N %s not found', usb_vid, usb_pid, usb_sn)
            raise GsUSBDeviceNotFound

        # Setting the active configuration again resets the data toggles of the host, but not
        # those of a Zephyr device, after which the first bulk packets would be dropped
        try:
            configured = device.get_active_configuration().bConfigurationValue == 1
        except usb.core.USBError:
            configured = False

        if not configured:
            device.set_configuration(1)

        self.device = device

        intf = device.get_active_configuration()[(0, 0)]
        self.ep_in = usb.util.find_descriptor(
            intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) ==
            usb.util.ENDPOINT_IN)
        self.ep_out = usb.util.find_descriptor(
            intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) ==
            usb.util.ENDPOINT_OUT)

        rtype = usb.util.build_request_type(usb.util.CTRL_OUT,
                                            usb.util.CTRL_TYPE_VENDOR,
                                            usb.util.CTRL_RECIPIENT_INTERFACE)
        # This class assumes little endian transfer format, let the device know
        data = struct.pack('<I', 0x0000beef)
        length = self.device.ctrl_transfer(bmRequestType = rtype,
                                           bRequest = GsUSBbRequest.HOST_FORMAT,
                                           data_or_wLength = data)
        assert length == len(data)

    def close(self) -> None:
        """Release the USB device, allowing another instance to use it."""
        usb.util.dispose_resources(self.device)

    def bittiming(self, ch: int, bittiming: GsUSBDeviceBittiming) -> None:
        """Send a bittiming request."""
        rtype = usb.util.build_request_type(usb.util.CTRL_OUT,
                                            usb.util.CTRL_TYPE_VENDOR,
                                            usb.util.CTRL_RECIPIENT_INTERFACE)
        data = struct.pack('<5I', *astuple(bittiming))
        length = self.device.ctrl_transfer(bmRequestType = rtype,
                                           bRequest = GsUSBbRequest.BITTIMING,
                                           wValue = ch, data_or_wLength = data)
        assert length == len(data)

    def mode(self, ch: int, mode: GsUSBDeviceMode) -> None:
        """Send a mode request."""
        rtype = usb.util.build_request_type(usb.util.CTRL_OUT,
                                            usb.util.CTRL_TYPE_VENDOR,
                                            usb.util.CTRL_RECIPIENT_INTERFACE)
        data = struct.pack('<2I', *astuple(mode))
        length = self.device.ctrl_transfer(bmRequestType = rtype,
                                           bRequest = GsUSBbRequest.MODE,
                                           wValue = ch, data_or_wLength = data)
        assert length == len(data)

    def bt_const(self, ch: int) -> GsUSBDeviceBTConst:
        """Send a bt_const request."""
        rtype = usb.util.build_request_type(usb.util.CTRL_IN,
                                            usb.util.CTRL_TYPE_VENDOR,
                                            usb.util.CTRL_RECIPIENT_INTERFACE)
        data = self.device.ctrl_transfer(bmRequestType = rtype,
                                         bRequest = GsUSBbRequest.BT_CONST,
                                         wValue = ch, data_or_wLength = struct.calcsize('<10I'))

        return GsUSBDeviceBTConst(*struct.unpack('<10I', data))

    def device_config(self) -> GsUSBDeviceConfig:
        """Send a device_config request."""
        rtype = usb.util.build_request_type(usb.util.CTRL_IN,
                                            usb.util.CTRL_TYPE_VENDOR,
                                            usb.util.CTRL_RECIPIENT_INTERFACE)
        data = self.device.ctrl_transfer(bmRequestType = rtype,
                                         bRequest = GsUSBbRequest.DEVICE_CONFIG,
                                         data_or_wLength = struct.calcsize('<xxxBII'))

        return GsUSBDeviceConfig(*struct.unpack('<xxxBII', data))

    def timestamp(self) -> int:
        """Send a timestamp request."""
        rtype = usb.util.build_request_type(usb.util.CTRL_IN,
                                            usb.util.CTRL_TYPE_VENDOR,
                                            usb.util.CTRL_RECIPIENT_INTERFACE)
        data = self.device.ctrl_transfer(bmRequestType = rtype,
                                         bRequest = GsUSBbRequest.TIMESTAMP,
                                         data_or_wLength = struct.calcsize('<I'))

        return struct.unpack('<I', data)[0]

    def identify(self, ch: int, on: bool) -> None:
        """Send an identify on/off request for the given CAN channel."""
        rtype = usb.util.build_request_type(usb.util.CTRL_OUT,
                                            usb.util.CTRL_TYPE_VENDOR,
                                            usb.util.CTRL_RECIPIENT_INTERFACE)
        data = struct.pack('<I', on)
        length = self.device.ctrl_transfer(bmRequestType = rtype,
                                           bRequest = GsUSBbRequest.IDENTIFY,
                                           wValue = ch, data_or_wLength = data)
        assert length == len(data)

    def data_bittiming(self, ch: int, bittiming: GsUSBDeviceBittiming) -> None:
        """Send a data_bittiming request."""
        rtype = usb.util.build_request_type(usb.util.CTRL_OUT,
                                            usb.util.CTRL_TYPE_VENDOR,
                                            usb.util.CTRL_RECIPIENT_INTERFACE)
        data = struct.pack('<5I', *astuple(bittiming))
        length = self.device.ctrl_transfer(bmRequestType = rtype,
                                           bRequest = GsUSBbRequest.DATA_BITTIMING,
                                           wValue = ch, data_or_wLength = data)
        assert length == len(data)

    def bt_const_ext(self, ch: int) -> GsUSBDeviceBTConstExt:
        """Send a bt_const_ext request."""
        rtype = usb.util.build_request_type(usb.util.CTRL_IN,
                                            usb.util.CTRL_TYPE_VENDOR,
                                            usb.util.CTRL_RECIPIENT_INTERFACE)
        data = self.device.ctrl_transfer(bmRequestType = rtype,
                                         bRequest = GsUSBbRequest.BT_CONST_EXT,
                                         wValue = ch, data_or_wLength = struct.calcsize('<18I'))

        return GsUSBDeviceBTConstExt(*struct.unpack('<18I', data))

    def set_termination(self, ch: int, terminate: bool) -> None:
        """Send a CAN bus termination on/off request for the given CAN channel."""
        rtype = usb.util.build_request_type(usb.util.CTRL_OUT,
                                            usb.util.CTRL_TYPE_VENDOR,
                                            usb.util.CTRL_RECIPIENT_INTERFACE)
        data = struct.pack('<I', terminate)

        length = self.device.ctrl_transfer(bmRequestType = rtype,
                                           bRequest = GsUSBbRequest.SET_TERMINATION,
                                           wValue = ch, data_or_wLength = data)
        assert length == len(data)

    def get_termination(self, ch: int) -> bool:
        """Send a CAN bus termination get request for the given CAN channel."""
        rtype = usb.util.build_request_type(usb.util.CTRL_IN,
                                            usb.util.CTRL_TYPE_VENDOR,
                                            usb.util.CTRL_RECIPIENT_INTERFACE)
        data = self.device.ctrl_transfer(bmRequestType = rtype,
                                         bRequest = GsUSBbRequest.GET_TERMINATION,
                                         wValue = ch, data_or_wLength = struct.calcsize('<I'))

        return bool(struct.unpack('<I', data)[0])

    def get_state(self, ch: int) -> GsUSBDeviceState:
        """Send a get_state request."""
        rtype = usb.util.build_request_type(usb.util.CTRL_IN,
                                            usb.util.CTRL_TYPE_VENDOR,
                                            usb.util.CTRL_RECIPIENT_INTERFACE)
        data = self.device.ctrl_transfer(bmRequestType = rtype,
                                         bRequest = GsUSBbRequest.GET_STATE,
                                         wValue = ch, data_or_wLength = struct.calcsize('<3I'))

        return GsUSBDeviceState(*struct.unpack('<3I', data))

    def write(self, data: bytes, timeout: float) -> None:
        """Write raw data to the bulk OUT endpoint, raising usb.core.USBTimeoutError on timeout."""
        length = self.ep_out.write(data, timeout=timeout_ms(timeout))
        assert length == len(data)

    def send(self, frame: GsUSBHostFrame, timeout: float) -> None:
        """Send a host frame to the device."""
        self.write(frame.pack(), timeout)

    def receive(self, timeout: float) -> GsUSBHostFrame | None:
        """Receive a host frame from the device, return None on timeout."""
        try:
            # Read a multiple of the packet size so the host never sees a babble error
            raw = self.ep_in.read(4 * self.ep_in.wMaxPacketSize, timeout=timeout_ms(timeout))
        except usb.core.USBTimeoutError:
            return None

        return GsUSBHostFrame.unpack(raw)

    def receive_all(self, timeout: float) -> list[GsUSBHostFrame]:
        """Receive host frames until none arrives within the timeout."""
        frames = []

        while (frame := self.receive(timeout)) is not None:
            frames.append(frame)

        return frames
