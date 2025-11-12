<a href="https://github.com/CANnectivity/cannectivity/actions/workflows/build.yml?query=branch%3Amain">
   <img src="https://github.com/CANnectivity/cannectivity/actions/workflows/build.yml/badge.svg">
</a>
<a href="https://github.com/CANnectivity/cannectivity/actions/workflows/docs.yml?query=branch%3Amain">
   <img src="https://github.com/CANnectivity/cannectivity/actions/workflows/docs.yml/badge.svg">
</a>

# CANnectivity

## Overview

CANnectivity is an open source firmware for Universal Serial Bus (USB) to Controller Area Network
(CAN) adapters.

The firmware implements the Geschwister Schneider USB/CAN device protocol (often referred to as
"gs_usb").  This protocol is supported by the Linux kernel SocketCAN [gs_usb
driver](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/net/can/usb/gs_usb.c),
by [python-can](https://python-can.readthedocs.io/en/stable/interfaces/gs_usb.html), and by many
other software packages.

The firmware, which is based on the [Zephyr RTOS](https://www.zephyrproject.org), allows turning
your favorite microcontroller development board into a full-fledged USB to CAN adapter.

CANnectivity is licensed under the [Apache-2.0 license](LICENSE). The CANnectivity documentation is
licensed under the [CC BY 4.0 license](doc/LICENSE).

## Firmware Features

The CANnectivity firmware supports the following features, some of which depend on hardware support:

- CAN classic
- CAN FD (flexible data rate)
- Fast-speed and Hi-speed USB
- Multiple, independent CAN channels
- LEDs:
  - CAN channel state LEDs
  - CAN channel activity LEDs
  - CAN channel error LEDs
  - Visual CAN channel identification
- GPIO-controlled CAN bus termination resistors
- Hardware timestamping of received and transmitted CAN frames
- CAN bus state reporting
- CAN bus error reporting
- Automatic gs_usb driver loading under Linux using custom [udev rules](99-cannectivity.rules)
- Automatic WinUSB driver installation under Microsoft Windows 8.1 and newer
- USB Device Firmware Upgrade (DFU) mode

## Hardware Requirements

Since the CANnectivity firmware is based on the Zephyr RTOS, it requires Zephyr support for the
board it is to run on. The board configuration must support both an USB device driver and at least
one CAN controller.

Check the list of [supported boards](https://docs.zephyrproject.org/latest/boards/index.html) in the
Zephyr documentation to see if your board is already supported. If not, have a look at the
instructions in the [board porting
guide](https://docs.zephyrproject.org/latest/hardware/porting/board_porting.html).

## Board Configuration

By default, CANnectivity relies on the
[devicetree](https://docs.zephyrproject.org/latest/build/dts/index.html) `zephyr,canbus` chosen node
property for specifying the CAN controller to use. If a devicetree alias for `led0` is present, it
is used as status LED. This means that virtually any Zephyr board configuration supporting USB
device, a CAN controller, and an optional user LED will work without any further configuration.

Advanced board configuration (e.g. multiple CAN controllers, multiple LEDs, hardware timestamp
counter) is also supported via devicetree overlays. Check the description for the
[cannectivity](app/dts/bindings/cannectivity.yaml) devicetree binding and [example devicetree
overlays](app/boards).

## Building and Running

Building the CANnectivity firmware requires a proper Zephyr development environment. Follow the
official [Zephyr Getting Started
Guide](https://docs.zephyrproject.org/latest/getting_started/index.html) to establish one.

Once a proper Zephyr development environment is established, inialize the workspace folder (here
`my-workspace`). This will clone the CANnectivity firmware repository and download the necessary
Zephyr modules:

```shell
west init -m https://github.com/CANnectivity/cannectivity --mr main my-workspace
cd my-workspace
west update
```

Next, build the CANnectivity firmware in its default configuration for your board (here
`lpcxpresso55s16`) using `west`:

```shell
west build -b lpcxpresso55s16/lpc55s16 cannectivity/app/
```

To use the `release` configuration, which has reduced log levels, set `FILE_SUFFIX=release`:

```shell
west build -b lpcxpresso55s16/lpc55s16 cannectivity/app/ -- -DFILE_SUFFIX=release
```

After building, the firmware can be flashed to the board by running the `west flash` command.

> **Note:** Build configurations for using the experimental `device_next` USB device stack in
> Zephyr are also provided. These can be selected by setting either `FILE_SUFFIX=usbd_next` or
> `FILE_SUFFIX=usbd_next_release`.

## USB Device Firmware Upgrade (DFU) Mode

CANnectivity supports USB Device Firmware Upgrade
([DFU](https://docs.zephyrproject.org/latest/services/device_mgmt/dfu.html)) via the
[MCUboot](https://www.trustedfirmware.org/projects/mcuboot/) bootloader. This is intended for use
with boards without an on-board programmer.

To build CANnectivity with MCUboot integration for USB DFU use
[sysbuild](https://docs.zephyrproject.org/latest/build/sysbuild/index.html) with the
`sysbuild-dfu.conf` configuration file when building for your board (here `frdm_k64f`):

```shell
west build -b frdm_k64f/mk64f12  --sysbuild cannectivity/app/ -- -DSB_CONF_FILE=sysbuild-dfu.conf
```

After building, MCUboot and the CANnectivity firmware can be flashed to the board by running the
`west flash` command.

Subsequent CANnectivity firmware updates can be applied via USB DFU. In order to do so, the board
must first be booted into USB DFU mode. If your board has a dedicated DFU button (identified by the
`mcuboot-button0` devicetree alias) press and hold it for 5 seconds or press and hold the button
while powering up the board. If your board has a DFU LED (identified by the `mcuboot-led0`
devicetree alias), the LED will flash while the DFU button is being held and change to constant on
once DFU mode is activated. Refer to your board documentation for further details.

Once in DFU mode, the CANnectivity firmware can be updated using
[dfu-util](https://dfu-util.sourceforge.net/):

```shell
dfu-util -a 1 -D build/app/zephyr/zephyr.signed.bin.dfu
```

## CANnectivity as a Zephyr Module

The CANnectivity firmware repository is a [Zephyr
module](https://docs.zephyrproject.org/latest/develop/modules.html) which allows for reuse of its
components (i.e. the "gs_usb" protocol implementation) outside of the CANnectivity firmware
application.

To pull in CANnectivity as a Zephyr module, either add it as a West project in the `west.yaml` file
or pull it in by adding a submanifest (e.g. `zephyr/submanifests/cannectivity.yaml`) file with the
following content and run `west update`:

```yaml
manifest:
  projects:
    - name: cannectivity
      url: https://github.com/CANnectivity/cannectivity.git
      revision: main
      path: custom/cannectivity # adjust the path as needed
```
