# Copyright (c) 2024-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
# SPDX-License-Identifier: Apache-2.0

if(SB_CONFIG_CANNECTIVITY_DFU_SUPPORT)
  # Enable DFU support in the CANnectivity firmware app
  set_config_bool(${DEFAULT_IMAGE} CONFIG_CANNECTIVITY_DFU_SUPPORT TRUE)
endif()
