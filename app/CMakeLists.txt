# Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(cannectivity LANGUAGES C)

if(CONFIG_CANNECTIVITY_USB_VID EQUAL 0x1209)
  if(CONFIG_CANNECTIVITY_USB_PID LESS_EQUAL 0x0010)
    message(WARNING
      "CONFIG_CANNECTIVITY_USB_PID is set to a generic pid.codes Test PID (${CONFIG_CANNECTIVITY_USB_PID}).
This PID is not unique and should not be used outside test environments."
      )
  endif()
endif()

target_sources(app PRIVATE
  src/main.c
  src/usb.c
)

target_sources_ifdef(CONFIG_CANNECTIVITY_LED app PRIVATE
  src/led.c
)

target_sources_ifdef(CONFIG_CANNECTIVITY_TIMESTAMP_COUNTER app PRIVATE
  src/timestamp.c
)

target_sources_ifdef(CONFIG_CANNECTIVITY_TERMINATION_GPIO app PRIVATE
  src/termination.c
)

target_sources_ifdef(CONFIG_BOOTLOADER_MCUBOOT app PRIVATE
  src/dfu.c
)
