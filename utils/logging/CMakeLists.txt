# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

add_library(logging
  STATIC
    logging.c
    logging.h
)

target_include_directories(logging INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(logging PUBLIC slog2)
