..
  Copyright (c) 2024-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
  SPDX-License-Identifier: CC-BY-4.0

:hide-toc:

Zephyr Module
=============

The CANnectivity firmware repository is a :external+zephyr:ref:`Zephyr module <modules>` which
allows for reuse of its components (i.e. the ``gs_usb`` protocol implementation) outside of the
CANnectivity firmware application.

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
