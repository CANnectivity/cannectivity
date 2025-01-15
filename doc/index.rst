..
  Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
  SPDX-License-Identifier: CC-BY-4.0

:hide-toc:

Introduction
============

.. toctree::
   :maxdepth: 1
   :hidden:
   :caption: Contents:

   self
   building
   porting
   module

CANnectivity is an open source firmware for Universal Serial Bus (USB) to Controller Area Network
(CAN) adapters.

The firmware implements the Geschwister Schneider USB/CAN device protocol (often referred to as
"gs_usb").  This protocol is supported by the Linux kernel SocketCAN `gs_usb driver`_, by
`python-can`_, and by many other software packages.

The firmware, which is based on the `Zephyr RTOS`_, allows turning your favorite microcontroller
development board into a full-fledged USB to CAN adapter.

CANnectivity is licenced under the `Apache-2.0 license`_. The CANnectivity documentation is licensed
under the `CC BY 4.0 license`_.

Firmware Features
-----------------

The CANnectivity firmware supports the following features, some of which depend on hardware support:

* CAN classic
* CAN FD (flexible data rate)
* Fast-speed and Hi-speed USB
* Multiple, independent CAN channels
* LEDs:

  * CAN channel state LEDs
  * CAN channel activity LEDs
  * Visual CAN channel identification

* GPIO-controlled CAN bus termination resistors
* Hardware timestamping of received and transmitted CAN frames
* CAN bus state reporting
* CAN bus error reporting
* Automatic gs_usb driver loading under Linux using custom udev rules
* Automatic WinUSB driver installation under Microsoft Windows 8.1 and newer
* USB Device Firmware Upgrade (DFU) mode

.. _gs_usb driver:
   https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/net/can/usb/gs_usb.c

.. _python-can:
   https://python-can.readthedocs.io/en/stable/interfaces/gs_usb.html

.. _Zephyr RTOS:
   https://www.zephyrproject.org

.. _Apache-2.0 license:
   http://www.apache.org/licenses/LICENSE-2.0

.. _CC BY 4.0 license:
   https://creativecommons.org/licenses/by/4.0/
