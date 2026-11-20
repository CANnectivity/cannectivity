# SPDX-FileCopyrightText: Copyright (c) 2026 Base Power Company
#
# SPDX-License-Identifier: Apache-2.0

"""
Test suite for gs_usb host frames that fail to send.
"""

import logging
import pytest

from gs_usb import GsUSBCANChannelFlag, GsUSBCANChannelMode, GsUSBCANFlag, GsUSBCANIDFlag, \
    GsUSBDeviceMode, GsUSBHostFrame

logger = logging.getLogger(__name__)

TIMEOUT = 1.0
IDLE = 0.2

FAKE_CHANNEL = 0

# Sending a frame with this CAN ID makes a fake CAN controller go bus-off (see src/main.c)
CAN_ID_BUS_OFF = 0x7ff
# A frame with this CAN ID stays pending on a fake CAN controller until it is stopped
CAN_ID_PENDING = 0x7fe

def tx_frame(echo_id: int, can_id: int) -> GsUSBHostFrame:
    """Return a CAN classic host frame for the fake CAN channel."""
    return GsUSBHostFrame(echo_id, can_id, 2, FAKE_CHANNEL, GsUSBCANFlag(0), bytes([echo_id, 0xa5]))

@pytest.mark.usefixtures('dev')
class TestGsUsbTxErrors():
    """
    Class for testing gs_usb host frames that fail to send.
    """

    def test_echo_on_tx_failure(self, dev) -> None:
        """Test that every host frame is echoed, even if it could not be sent"""
        dev.receive_all(IDLE)
        dev.mode(FAKE_CHANNEL, GsUSBDeviceMode(GsUSBCANChannelMode.START,
                                               GsUSBCANChannelFlag.NORMAL))
        try:
            dev.send(tx_frame(0, 0x123), TIMEOUT)
            # Fails in the TX callback, and takes the controller bus-off
            dev.send(tx_frame(1, CAN_ID_BUS_OFF), TIMEOUT)
            # Fail in can_send() while bus-off
            for echo_id in range(2, 6):
                dev.send(tx_frame(echo_id, 0x123), TIMEOUT)

            frames = dev.receive_all(IDLE)
        finally:
            dev.mode(FAKE_CHANNEL, GsUSBDeviceMode(GsUSBCANChannelMode.RESET,
                                                   GsUSBCANChannelFlag.NORMAL))

        logger.debug('frames = %s', frames)
        echoes = [f.echo_id for f in frames if f.echo_id != GsUSBHostFrame.ECHO_ID_RX]
        assert echoes == list(range(6))

        bus_off = [f for f in frames if f.echo_id == GsUSBHostFrame.ECHO_ID_RX and
                   f.can_id & GsUSBCANIDFlag.ERR and f.can_id & GsUSBCANIDFlag.ERR_BUSOFF]
        assert len(bus_off) == 1

        # Starting the channel again recovers the controller from bus-off
        dev.mode(FAKE_CHANNEL, GsUSBDeviceMode(GsUSBCANChannelMode.START,
                                               GsUSBCANChannelFlag.NORMAL))
        try:
            dev.send(tx_frame(6, 0x123), TIMEOUT)
            frames = dev.receive_all(IDLE)
        finally:
            dev.mode(FAKE_CHANNEL, GsUSBDeviceMode(GsUSBCANChannelMode.RESET,
                                                   GsUSBCANChannelFlag.NORMAL))

        assert [f.echo_id for f in frames] == [6]

    def test_no_echo_after_reset(self, dev) -> None:
        """Test that frames aborted by a channel reset are not echoed"""
        dev.receive_all(IDLE)
        dev.mode(FAKE_CHANNEL, GsUSBDeviceMode(GsUSBCANChannelMode.START,
                                               GsUSBCANChannelFlag.NORMAL))
        dev.send(tx_frame(0, CAN_ID_PENDING), TIMEOUT)
        assert not dev.receive_all(IDLE)

        # The host releases its TX contexts when resetting the channel, and may reuse the
        # echo ID of an aborted frame right after starting it again
        dev.mode(FAKE_CHANNEL, GsUSBDeviceMode(GsUSBCANChannelMode.RESET,
                                               GsUSBCANChannelFlag.NORMAL))
        dev.mode(FAKE_CHANNEL, GsUSBDeviceMode(GsUSBCANChannelMode.START,
                                               GsUSBCANChannelFlag.NORMAL))
        try:
            dev.send(tx_frame(1, 0x123), TIMEOUT)
            frames = dev.receive_all(IDLE)
        finally:
            dev.mode(FAKE_CHANNEL, GsUSBDeviceMode(GsUSBCANChannelMode.RESET,
                                                   GsUSBCANChannelFlag.NORMAL))

        assert [f.echo_id for f in frames] == [1]
