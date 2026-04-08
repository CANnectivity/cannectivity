..
  Copyright (c) 2024-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
  SPDX-License-Identifier: CC-BY-4.0

:hide-toc:

Building and Running
====================

Building the CANnectivity firmware requires a proper Zephyr development environment. Follow the
official Zephyr :external+zephyr:ref:`Getting Started Guide <getting_started>` to establish one.

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

USB Device Firmware Upgrade (DFU) Mode
--------------------------------------

CANnectivity supports USB :external+zephyr:ref:`Device Firmware Upgrade <dfu>` (DFU) via the
`MCUboot`_ bootloader. This is intended for use with boards without an on-board programmer.

To build CANnectivity with MCUboot integration for USB DFU use :external+zephyr:ref:`sysbuild
<sysbuild>` when building for your board (here ``frdm_k64f``):

.. code-block:: console

   west build -b frdm_k64f/mk64f12 --sysbuild ../custom/cannectivity/app/

After building, MCUboot and the CANnectivity firmware can be flashed to the board by running the
``west flash`` command.

Subsequent CANnectivity firmware updates can be applied via USB DFU using `dfu-util`_:

.. code-block:: console

   dfu-util -D build/app/zephyr/zephyr.signed.bin.dfu

The ``dfu-util`` command will automatically switch the firmware into DFU mode and reboot into the
new firmware once completed.

If your board has a dedicated DFU button (identified by the ``mcuboot-button0`` devicetree alias),
it can also be used to force the firmware into DFU mode by pressing and holding it for 5 seconds. If
your board has a DFU LED (identified by the ``mcuboot-led0`` devicetree alias), the LED will flash
while the DFU button is being held and change to constant on once DFU mode is activated. Refer to
your board documentation for further details.

.. _MCUboot:
   https://www.trustedfirmware.org/projects/mcuboot/

.. _dfu-util:
   https://dfu-util.sourceforge.net/
