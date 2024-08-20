# Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
# SPDX-License-Identifier: Apache-2.0

"""
Test suites for testing gs_usb
"""

import logging
import pytest

from gs_usb import  GsUSBCANChannelFeature, GsUSBCANChannelFlag, GsUSBCANChannelMode, \
    GsUSBCANChannelState, GsUSBDeviceBittiming, GsUSBDeviceMode

logger = logging.getLogger(__name__)

TIMEOUT = 1.0

DEV_NAME = 'gs_usb0'
USER_DATA = '0x12345678'

FAKE_CHANNELS = [ 0, 1 ]
LOOPBACK_CHANNELS = [ 2, 3 ]
NUM_CHANNELS = len(FAKE_CHANNELS + LOOPBACK_CHANNELS)

@pytest.mark.usefixtures('dut', 'dev')
class TestGsUsbRequests():
    """
    Class for testing gs_usb requests.
    """

    def test_bittiming(self, dut, dev) -> None:
        """Test the bittiming request"""
        timing = GsUSBDeviceBittiming(0, 139, 20, 10, 1)

        for ch in FAKE_CHANNELS:
            dev.bittiming(ch, timing)
            regex = fr'fake_can{ch}: sjw = 10, prop_seg = 0, phase_seg1 = 139, phase_seg2 = 20, ' \
                    'prescaler = 1'
            dut.readlines_until(regex=regex, timeout=TIMEOUT)

    def test_mode(self, dut, dev) -> None:
        """Test the mode request"""
        mode = GsUSBDeviceMode(GsUSBCANChannelMode.RESET, GsUSBCANChannelFlag.NORMAL)

        for ch in FAKE_CHANNELS:
            mode = GsUSBDeviceMode(GsUSBCANChannelMode.START, GsUSBCANChannelFlag.NORMAL)
            dev.mode(ch, mode)

            regex = fr'fake_can{ch}: mode = 0'
            dut.readlines_until(regex=regex, timeout=TIMEOUT)

            regex = fr'fake_can{ch}: start'
            dut.readlines_until(regex=regex, timeout=TIMEOUT)

            regex = fr'dev = {DEV_NAME}, ch = {ch}, started = 1, user_data = {USER_DATA}'
            dut.readlines_until(regex=regex, timeout=TIMEOUT)

            mode = GsUSBDeviceMode(GsUSBCANChannelMode.RESET, GsUSBCANChannelFlag.NORMAL)
            dev.mode(ch, mode)

            regex = fr'fake_can{ch}: stop'
            dut.readlines_until(regex=regex, timeout=TIMEOUT)

            regex = fr'dev = {DEV_NAME}, ch = {ch}, started = 0, user_data = {USER_DATA}'
            dut.readlines_until(regex=regex, timeout=TIMEOUT)

    def test_bt_const(self, dev) -> None:
        """Test the bt_const request"""
        for ch in range(NUM_CHANNELS):
            features = GsUSBCANChannelFeature.HW_TIMESTAMP | \
                       GsUSBCANChannelFeature.IDENTIFY | \
                       GsUSBCANChannelFeature.FD | \
                       GsUSBCANChannelFeature.BT_CONST_EXT | \
                       GsUSBCANChannelFeature.TERMINATION | \
                       GsUSBCANChannelFeature.GET_STATE

            btc = dev.bt_const(ch)
            logger.debug('ch%d = %s', ch, btc)

            assert btc.fclk_can == 80000000

            assert btc.tseg1_min == 2
            assert btc.tseg1_max == 256
            assert btc.tseg2_min == 2
            assert btc.tseg2_max == 128
            assert btc.sjw_max == 128
            assert btc.brp_min == 1
            assert btc.brp_max == 32
            assert btc.brp_inc == 1

            if ch in LOOPBACK_CHANNELS:
                features |= GsUSBCANChannelFeature.LOOP_BACK

            assert btc.feature == features

    def test_device_config(self, dev) -> None:
        """Test the device_config request"""
        cfg = dev.device_config()
        assert cfg.nchannels == NUM_CHANNELS - 1
        assert cfg.sw_version == 2
        assert cfg.hw_version == 1

    def test_timestamp(self, dut, dev) -> None:
        """test the timestamp request"""
        timestamp = dev.timestamp()
        regex = fr'dev = {DEV_NAME}, timestamp = 0x{timestamp:08x}, user_data = {USER_DATA}'
        dut.readlines_until(regex=regex, timeout=TIMEOUT)

    def test_identify(self, dut, dev) -> None:
        """Test the identify request"""
        for ident in (False, True):
            for ch in range(NUM_CHANNELS):
                dev.identify(ch, ident)
                regex = fr'dev = {DEV_NAME}, ch = {ch}, identify = {int(ident)}, ' \
                        fr'user_data = {USER_DATA}'
                dut.readlines_until(regex=regex, timeout=TIMEOUT)

    def test_data_bittiming(self, dut, dev) -> None:
        """Test the data_bittiming request"""
        timing = GsUSBDeviceBittiming(0, 14, 5, 2, 1)

        for ch in FAKE_CHANNELS:
            dev.data_bittiming(ch, timing)
            regex = fr'fake_can{ch}: sjw = 2, prop_seg = 0, phase_seg1 = 14, phase_seg2 = 5, ' \
                    'prescaler = 1'
            dut.readlines_until(regex=regex, timeout=TIMEOUT)

    def test_bt_const_ext(self, dev) -> None:
        """Test the bt_const_ext request"""
        for ch in range(NUM_CHANNELS):
            features = GsUSBCANChannelFeature.HW_TIMESTAMP | \
                       GsUSBCANChannelFeature.IDENTIFY | \
                       GsUSBCANChannelFeature.FD | \
                       GsUSBCANChannelFeature.BT_CONST_EXT | \
                       GsUSBCANChannelFeature.TERMINATION | \
                       GsUSBCANChannelFeature.GET_STATE

            btce = dev.bt_const_ext(ch)
            logger.debug('ch%d = %s', ch, btce)

            assert btce.fclk_can == 80000000

            assert btce.tseg1_min == 2
            assert btce.tseg1_max == 256
            assert btce.tseg2_min == 2
            assert btce.tseg2_max == 128
            assert btce.sjw_max == 128
            assert btce.brp_min == 1
            assert btce.brp_max == 32
            assert btce.brp_inc == 1

            assert btce.dtseg1_min == 1
            assert btce.dtseg1_max == 32
            assert btce.dtseg2_min == 1
            assert btce.dtseg2_max == 16
            assert btce.dsjw_max == 16
            assert btce.dbrp_min == 1
            assert btce.dbrp_max == 32
            assert btce.dbrp_inc == 1

            if ch in LOOPBACK_CHANNELS:
                features |= GsUSBCANChannelFeature.LOOP_BACK

            assert btce.feature == features

    def test_set_termination(self, dut, dev) -> None:
        """Test the set_termination request"""
        for term in (False, True):
            for ch in range(NUM_CHANNELS):
                dev.set_termination(ch, term)
                regex = fr'dev = {DEV_NAME}, ch = {ch}, terminate = {int(term)}, ' \
                        fr'user_data = {USER_DATA}'
                dut.readlines_until(regex=regex, timeout=TIMEOUT)

    def test_get_termination(self, dut, dev) -> None:
        """Test the get_termination request"""
        for ch in range(NUM_CHANNELS):
            term = int(dev.get_termination(ch))
            regex = fr'dev = {DEV_NAME}, ch = {ch}, terminated = {term}, user_data = {USER_DATA}'
            dut.readlines_until(regex=regex, timeout=TIMEOUT)

    def test_get_state(self, dev) -> None:
        """Test the get_state request"""
        for ch in range(NUM_CHANNELS):
            state = dev.get_state(ch)
            logger.debug('ch%d = %s', ch, state)

            if ch in FAKE_CHANNELS:
                assert state.state == GsUSBCANChannelState.ERROR_PASSIVE
                assert state.rxerr == 96
                assert state.txerr == 128
            else:
                assert state.state == GsUSBCANChannelState.STOPPED
                assert state.rxerr == 0
                assert state.txerr == 0
