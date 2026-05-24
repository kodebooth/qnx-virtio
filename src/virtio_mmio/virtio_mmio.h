/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */

/**
 * @file virtio_mmio.h
 * @brief VirtIO MMIO transport layer implementation
 *
 * This module provides the Memory-Mapped I/O (MMIO) transport layer for VirtIO
 * devices, implementing the VirtIO specification's MMIO transport binding.
 * Supports both legacy and modern VirtIO MMIO interfaces.
 */

#ifndef QNX_VIRTIO_MMIO_H_INCLUDED
#define QNX_VIRTIO_MMIO_H_INCLUDED

#include "virtio.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Opaque VirtIO MMIO device structure
 */
struct virtio_mmio_device;

/**
 * @brief Initialize a VirtIO MMIO device
 *
 * Discovers and initializes a VirtIO MMIO device at the specified physical
 * address. Maps the MMIO region, validates the device, and sets up the
 * device operations. Automatically detects legacy vs modern interface.
 *
 * @param[in] dev VirtIO device structure to initialize
 * @param[in] paddr Physical address of the MMIO region
 * @param[in] type Expected VirtIO device type
 * @param[in] irq Interrupt request number for the device
 * @return EOK on success, error code otherwise
 */
int virtio_mmio_init(struct virtio_device *dev, uint64_t paddr, uint16_t type,
                     int irq);

#endif
