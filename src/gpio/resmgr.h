/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */

/**
 * @file resmgr.h
 * @brief QNX Resource Manager interface for VirtIO GPIO device
 *
 * This file provides the resource manager implementation for exposing
 * VirtIO GPIO pins as QNX device files (/dev/gpioX.Y). The resource
 * manager handles open, close, read, and write operations on GPIO pins.
 */

#ifndef VIRTIO_QNX_GPIO_RESMGR_H_INCLUDED
#define VIRTIO_QNX_GPIO_RESMGR_H_INCLUDED

#include "gpio.h"

/**
 * @brief Run the resource manager for a GPIO device
 *
 * Initializes and runs the QNX resource manager for the specified GPIO
 * device. This function registers device paths in the namespace and handles
 * client I/O requests. It blocks until the resource manager is shut down.
 *
 * @param dev VirtIO GPIO device instance
 * @param index Device instance number (used for /dev/gpioX naming)
 * @return 0 on success, negative error code on failure
 */
int resmgr_run(struct virtio_gpio_device *dev, uint16_t index);

#endif
