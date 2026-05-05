# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

add_library(fifo
  STATIC
    fifo.c
    fifo.h
)

target_include_directories(fifo INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
