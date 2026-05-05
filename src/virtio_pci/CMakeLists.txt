# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

add_library(virtio-pci
  STATIC
    virtio_pci.c
    virtio_pci.h
)
target_include_directories(virtio-pci PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(virtio-pci PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(virtio-pci PRIVATE virtio pci)
target_link_libraries(virtio-pci PUBLIC virtio pci)
