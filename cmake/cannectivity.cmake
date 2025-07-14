# Copyright (c) 2024-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
# SPDX-License-Identifier: Apache-2.0

function(cannectivity_generate_usb_dfu_image)
  if(CONFIG_CANNECTIVITY_GENERATE_USB_DFU_IMAGE)
    find_program(DFU_SUFFIX dfu-suffix)

    if(NOT ${DFU_SUFFIX} STREQUAL DFU_SUFFIX-NOTFOUND)
      if(CONFIG_BOOTLOADER_MCUBOOT)
        set(bin_image ${PROJECT_BINARY_DIR}/${CONFIG_KERNEL_BIN_NAME}.signed.bin)
      else()
        set(bin_image ${PROJECT_BINARY_DIR}/${CONFIG_KERNEL_BIN_NAME}.bin)
      endif()
      set(dfu_image ${bin_image}.dfu)
      get_filename_component(dfu_image_name ${dfu_image} NAME)

      add_custom_command(
        OUTPUT ${dfu_image}
        COMMAND ${CMAKE_COMMAND} -E copy ${bin_image} ${dfu_image}
        COMMAND ${DFU_SUFFIX}
        --vid ${CONFIG_CANNECTIVITY_USB_DFU_VID}
        --pid ${CONFIG_CANNECTIVITY_USB_DFU_PID}
        --spec ${CONFIG_CANNECTIVITY_USB_DFU_SPEC_ID}
        --add ${dfu_image}
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
        DEPENDS ${bin_image}
        COMMENT "Generating ${dfu_image_name}"
        )

      add_custom_target(
        cannectivity_usb_dfu_image
        ALL
        DEPENDS ${dfu_image}
        )
    else()
      message(FATAL_ERROR "The dfu-suffix utility was not found, USB DFU image cannot be generated")
    endif()
  endif()
endfunction()

cannectivity_generate_usb_dfu_image()
