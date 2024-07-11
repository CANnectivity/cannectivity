# Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
# SPDX-License-Identifier: Apache-2.0

add_subdirectory_ifdef(CONFIG_USB_DEVICE_STACK device)
add_subdirectory_ifdef(CONFIG_USB_DEVICE_STACK_NEXT device_next)
