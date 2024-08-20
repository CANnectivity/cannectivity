Geschwister Schneider USB/CAN Protocol Tests
############################################

Overview
********

This test suite uses `PyUSB`_ for testing the Geschwister Schneider USB/CAN protocol implementation.

Prerequisites
*************

The test suite has the following prerequisites:

* The PyUSB library must be installed on the host PC.
* The DUT must be connected to the host PC via USB.

Building and Running
********************

Below is an example for running the test suite on the ``frdm_k64f`` board:

.. code-block:: shell

   west twister -v -p frdm_k64f/mk64f12 --device-testing --device-serial /dev/ttyACM0 -X usb -T tests/subsys/usb/gs_usb/host/

.. _PyUSB:
   https://pyusb.github.io/pyusb/
