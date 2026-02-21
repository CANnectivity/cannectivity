.. zephyr:board:: entree

Overview
********

Entree is an open hardware Universal Serial Bus (USB) to Controller Area Network (CAN) adapter
board. It is designed to be compatible with the open source :ref:`external_module_cannectivity`.

Hardware
********

The Entree board is equipped with an STM32G0B1 microcontroller and features an USB-C connector (high-speed USB 2.0), Molex Picoblade for CAN FD (up to 8 Mbit/s), switched termination resistor and a number of status LEDs. Schematics and component placement drawings are available in the `Entree
GitHub repository`_.

Supported Features
==================

.. zephyr:board-supported-hw::

Programming and Debugging
*************************

.. zephyr:board-supported-runners::

Build and flash applications as usual (see :ref:`build_an_application` and
:ref:`application_run` for more details).

Here is an example for the :zephyr:code-sample:`blinky` application.

.. zephyr-app-commands::
   :zephyr-app: samples/basic/blinky
   :board: entree
   :Goals: flash

.. _Entree GitHub repository:
   https://github.com/tuna-f1sh/entree
