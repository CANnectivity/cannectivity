# Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
# SPDX-License-Identifier: Apache-2.0

"""
Configuration of gs_usb test suite.
"""

import re
import logging
import time
import pytest

from twister_harness import DeviceAdapter, Shell
from gs_usb import GsUSB

logger = logging.getLogger(__name__)

def pytest_addoption(parser) -> None:
    """Add local parser options to pytest."""
    parser.addoption('--usb-delay', default=5,
                     help='Delay to wait for USB enumeration after flashing (default: 5 seconds)')
    parser.addoption('--usb-sn', default=None,
                     help='USB serial number of the DUT (default: None)')

@pytest.fixture(name='usb_vid', scope='module')
def fixture_usb_vid(shell: Shell) -> int:
    """Return the USB VID used by the DUT."""
    regex = re.compile(r'USB VID:\s+(\S+)')
    lines = shell.get_filtered_output(shell.exec_command('gs_usb vid'))

    for line in lines:
        m = regex.match(line)
        if m:
            vid = int(m.groups()[0], 16)
            return vid

    pytest.fail('USB VID not found')
    return 0x0000

@pytest.fixture(name='usb_pid', scope='module')
def fixture_usb_pid(shell: Shell) -> int:
    """Return the USB PID used by the DUT."""
    regex = re.compile(r'USB PID:\s+(\S+)')
    lines = shell.get_filtered_output(shell.exec_command('gs_usb pid'))

    for line in lines:
        m = regex.match(line)
        if m:
            pid = int(m.groups()[0], 16)
            return pid

    pytest.fail('USB PID not found')
    return 0x0000

@pytest.fixture(name='usb_sn', scope='module')
def fixture_usb_sn(request, dut: DeviceAdapter) -> str:
    """Return the USB serial number used by the DUT."""

    sn = request.config.getoption('--usb-sn')

    if sn is None:
        for fixture in dut.device_config.fixtures:
            if fixture.startswith('usb:'):
                sn = fixture.split(sep=':', maxsplit=1)[1]
                break

    if sn is not None:
        logger.info('using USB S/N: "%s"', sn)

    return sn

@pytest.fixture(name='dev', scope='module')
def fixture_gs_usb(request, usb_vid: int, usb_pid: int, usb_sn: str) -> GsUSB:
    """Return gs_usb instance for testing"""

    delay = request.config.getoption('--usb-delay')
    logger.info('Waiting %d seconds for USB enumeration...', delay)
    time.sleep(delay)

    return GsUSB(usb_vid, usb_pid, usb_sn)
