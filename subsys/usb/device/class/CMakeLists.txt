# Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
# SPDX-License-Identifier: Apache-2.0

zephyr_sources_ifdef(CONFIG_USB_DEVICE_GS_USB gs_usb.c)
