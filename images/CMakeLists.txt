# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

# Determine architecture from CMAKE_SYSTEM_PROCESSOR
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64le")
  set(ARCH "aarch64le")
  set(QEMU_SYSTEM "qemu-system-aarch64")
  set(QEMU_MACHINE "virt")
  set(QEMU_CPU "cortex-a57")
  set(QEMU_GPIO_DEVICE "vhost-user-gpio-device,chardev=vgpio,id=gpio")
else()
  set(ARCH "x86_64")
  set(QEMU_SYSTEM "qemu-system-x86_64")
  set(QEMU_MACHINE "q35")
  set(QEMU_CPU "max")
  set(QEMU_GPIO_DEVICE "vhost-user-gpio-pci,chardev=vgpio,id=gpio")
endif()

add_custom_command(
  OUTPUT
    ifs.img
  DEPENDS
    ifs_${ARCH}.build
    dev-virtio-gpio
  COMMAND
    mkifs -vvv -o "${CMAKE_BINARY_DIR}" -r "${CMAKE_BINARY_DIR}/.." ${CMAKE_CURRENT_LIST_DIR}/ifs_${ARCH}.build ${CMAKE_BINARY_DIR}/ifs.img
)

add_custom_target(
  ifs
  ALL
  SOURCES
    ifs.img
)

add_custom_target(
  boot
  DEPENDS
    ifs
  COMMAND
  ${QEMU_SYSTEM}
    -monitor telnet:127.0.0.1:5555,server,nowait
    -m 4096
    --nographic
    -object memory-backend-file,id=mem,size=4G,mem-path=/dev/shm,share=on
    -numa node,memdev=mem
    --machine ${QEMU_MACHINE}
    --cpu ${QEMU_CPU}
    --kernel ${CMAKE_BINARY_DIR}/ifs.img
    -chardev socket,path=/tmp/vhost-gpio0,id=vgpio
    -device ${QEMU_GPIO_DEVICE}
)
