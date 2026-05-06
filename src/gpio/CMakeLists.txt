# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

add_executable(
  dev-virtio-gpio
  main.c
  options.c
  gpio.c
  resmgr.c
)

target_link_libraries(dev-virtio-gpio PRIVATE virtio-pci virtio-mmio logging)

add_custom_command(
  TARGET dev-virtio-gpio POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:dev-virtio-gpio> ${CMAKE_BINARY_DIR}/sbin/dev-virtio-gpio
)
