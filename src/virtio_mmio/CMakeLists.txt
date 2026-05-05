# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

add_library(virtio-mmio
  STATIC
    virtio_mmio.c
    virtio_mmio.h
)
target_include_directories(virtio-mmio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(virtio-mmio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(virtio-mmio PUBLIC virtio)
