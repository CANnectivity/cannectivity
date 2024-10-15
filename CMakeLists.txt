# Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(include)

add_subdirectory(subsys)

include(cmake/cannectivity.cmake)
