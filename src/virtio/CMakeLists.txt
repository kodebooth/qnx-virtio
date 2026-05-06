# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

add_library(virtio
  STATIC
    virtio.c
    virtio.h
    virtq.c
    virtq.h
)

target_link_libraries(virtio PUBLIC fifo logging)
target_include_directories(virtio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
