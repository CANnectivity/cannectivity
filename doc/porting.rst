..
  Copyright (c) 2024-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
  SPDX-License-Identifier: CC-BY-4.0

:hide-toc:

Board Porting Guide
===================

Hardware Requirements
---------------------

Since the CANnectivity firmware is based on the Zephyr RTOS, it requires Zephyr support for the
board it is to run on. The board configuration must support both an USB device driver and at least
one CAN controller.

Check the list of :external+zephyr:ref:`supported boards <boards>` in the Zephyr documentation to
see if your board is already supported. If not, have a look at the instructions in the Zephyr
:external+zephyr:ref:`Board Porting Guide <board_porting_guide>`.

Board Configuration
-------------------

By default, CANnectivity relies on the :external+zephyr:ref:`devicetree <devicetree>`
``zephyr,canbus`` chosen node property for specifying the CAN controller to use. If a devicetree
alias for ``led0`` is present, it is used as status LED. This means that virtually any Zephyr board
configuration supporting USB device, a CAN controller, and an optional user LED will work without
any further configuration.

Advanced board configuration (e.g. multiple CAN controllers, multiple LEDs, hardware timestamp
counter) is also supported via devicetree overlays. Check the description for the ``cannectivity``
devicetree binding ``app/dts/bindings/cannectivity.yaml`` and the example devicetree overlays under
``app/boards/``.
