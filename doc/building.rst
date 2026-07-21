.. SPDX-FileCopyrightText: Copyright (c) 2024-2026 Henrik Brix Andersen <henrik@brixandersen.dk>
..
.. SPDX-License-Identifier: CC-BY-4.0

:hide-toc:

Workspace Setup
===============

Building the CANnectivity firmware requires a proper Zephyr development environment. This repository
can either be used as a stand-alone Zephyr workspace or it can be added to an existing Zephyr
workspace as a module.

Stand-alone Workspace
---------------------

Follow the official :external+zephyr:ref:`Getting Started Guide <getting_started>` until the "Get
the Zephyr source code" step.

Replace the instructions for getting the Zephyr source code with the command below. This will clone
the CANnectivity firmware repository and download the necessary Zephyr modules:

.. code-block:: console

   west init -m https://github.com/CANnectivity/cannectivity --mr main <workspace directory>
   cd <workspace directory>
   west update

After performing the step above, continue with installing the Zephyr SDK according to the
instructions found in the Zephyr Getting Started Guide.

CANnectivity as a Zephyr Module
-------------------------------

The CANnectivity firmware repository is a :external+zephyr:ref:`Zephyr module <modules>` which
allows for adding it to an existing Zephyr workspace, either for building the CANnectivity firmware
as part of an existing workspace, or for reuse of its components (i.e. the "gs_usb" protocol
implementation) outside of the CANnectivity firmware application.

To pull in CANnectivity as a Zephyr module, either add it as a West project in the ``west.yaml``
file or pull it in by adding a submanifest (e.g. ``zephyr/submanifests/cannectivity.yaml``) file
with the following content and run ``west update``:

.. code-block:: yaml

   manifest:
     projects:
       - name: cannectivity
         url: https://github.com/CANnectivity/cannectivity.git
         revision: main
         path: custom/cannectivity # adjust the path as needed

Building and Running
--------------------

After setting up a workspace, build the CANnectivity firmware in its default configuration for your
board (here ``lpcxpresso55s16``) using ``west``:

.. code-block:: console

   west build -b lpcxpresso55s16/lpc55s16 cannectivity/app/

To use the ``debug`` configuration, which has logging enabled, set ``FILE_SUFFIX=debug``:

.. code-block:: console

   west build -b lpcxpresso55s16/lpc55s16 cannectivity/app/ -- -DFILE_SUFFIX=debug

After building, the firmware can be flashed to the board by running the ``west flash`` command.

USB Device Firmware Upgrade (DFU) Mode
--------------------------------------

CANnectivity supports USB :external+zephyr:ref:`Device Firmware Upgrade <dfu>` (DFU) via the
`MCUboot`_ bootloader. This is intended for use with boards without an on-board programmer.

To build CANnectivity with MCUboot integration for USB DFU use :external+zephyr:ref:`sysbuild
<sysbuild>` when building for your board (here ``frdm_k64f``):

.. code-block:: console

   west build -b frdm_k64f/mk64f12 --sysbuild cannectivity/app/

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
