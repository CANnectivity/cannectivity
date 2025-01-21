..
  Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
  SPDX-License-Identifier: CC-BY-4.0

:hide-toc:

Building and Running
====================

Building the CANnectivity firmware requires a proper Zephyr development environment. Follow the
official `Zephyr Getting Started Guide`_ to establish one.

Once a proper Zephyr development environment is established, inialize the workspace folder (here
``my-workspace``). This will clone the CANnectivity firmware repository and download the necessary
Zephyr modules:

.. code-block:: console

   west init -m https://github.com/CANnectivity/cannectivity --mr main my-workspace
   cd my-workspace
   west update

Next, build the CANnectivity firmware in its default configuration for your board (here
``lpcxpresso55s16``) using ``west``:

.. code-block:: console

   west build -b lpcxpresso55s16/lpc55s16 cannectivity/app/

To use the ``release`` configuration, which has reduced log levels, set ``FILE_SUFFIX=release``:

.. code-block:: console

   west build -b lpcxpresso55s16/lpc55s16 cannectivity/app/ -- -DFILE_SUFFIX=release

After building, the firmware can be flashed to the board by running the ``west flash`` command.

.. note::

   Build configurations for using the experimental ``device_next`` USB device stack in Zephyr are
   also provided. These can be selected by setting either ``FILE_SUFFIX=usbd_next`` or
   ``FILE_SUFFIX=usbd_next_release``.

USB Device Firmware Upgrade (DFU) Mode
--------------------------------------

CANnectivity supports USB Device Firmware Upgrade (`DFU`_) via the `MCUboot`_ bootloader. This is
intended for use with boards without an on-board programmer.

To build CANnectivity with MCUboot integration for USB DFU use `sysbuild`_ with the
``sysbuild-dfu.conf`` configuration file when building for your board (here ``frdm_k64f``):

.. code-block:: console

   west build -b frdm_k64f/mk64f12  --sysbuild ../custom/cannectivity/app/ -- -DSB_CONF_FILE=sysbuild-dfu.conf

After building, MCUboot and the CANnectivity firmware can be flashed to the board by running the
``west flash`` command.

Subsequent CANnectivity firmware updates can be applied via USB DFU. In order to do so, the board
must first be booted into USB DFU mode. If your board has a dedicated DFU button (identified by the
``mcuboot-button0`` devicetree alias) press and hold it for 5 seconds or press and hold the button
while powering up the board. If your board has a DFU LED (identified by the ``mcuboot-led0``
devicetree alias), the LED will flash while the DFU button is being held and change to constant on
once DFU mode is activated. Refer to your board documentation for further details.

Once in DFU mode, the CANnectivity firmware can be updated using
`dfu-util`_:

.. code-block:: console

   dfu-util -a 1 -D build/app/zephyr/zephyr.signed.bin.dfu

.. _Zephyr Getting Started Guide:
   https://docs.zephyrproject.org/latest/getting_started/index.html

.. _DFU:
   https://docs.zephyrproject.org/latest/services/device_mgmt/dfu.html

.. _MCUboot:
   https://www.trustedfirmware.org/projects/mcuboot/

.. _sysbuild:
   https://docs.zephyrproject.org/latest/build/sysbuild/index.html

.. _dfu-util:
   https://dfu-util.sourceforge.net/
