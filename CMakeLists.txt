# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(qnx-virtio LANGUAGES C CXX)

set(QNX_LIBGPIOD_VERSION "v2.2.4-1")

add_subdirectory(src/gpio)
add_subdirectory(images)
add_subdirectory(utils)
add_subdirectory(src/virtio)
add_subdirectory(src/virtio_mmio)
add_subdirectory(src/virtio_pci)

file(
  CREATE_LINK 
  "${CMAKE_BINARY_DIR}/compile_commands.json" 
  "${CMAKE_BINARY_DIR}/../compile_commands.json" 
  SYMBOLIC)
