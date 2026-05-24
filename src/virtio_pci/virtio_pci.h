/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */

/**
 * @file virtio_pci.h
 * @brief VirtIO PCI transport layer implementation
 *
 * This module provides the PCI transport layer for VirtIO devices,
 * implementing the VirtIO specification's PCI transport binding.
 */

#ifndef QNX_VIRTIO_PCI_H_INCLUDED
#define QNX_VIRTIO_PCI_H_INCLUDED

/* clang-format off */ 
// cap_msi.h and cap_msix.h both use NULL but don't include stddef.h
#include <stddef.h>
/* clang-format on */

#include "virtio.h"
#include <pci/cap_msi.h>
#include <pci/cap_msix.h>
#include <pci/cap_vend.h>
#include <pci/pci.h>
#include <stdint.h>

/**
 * @brief Opaque VirtIO PCI device structure
 */
struct virtio_pci_device;

/**
 * @brief Initialize a VirtIO PCI device
 *
 * Discovers and initializes a VirtIO PCI device, mapping BARs and setting up
 * MSI-X if requested.
 *
 * @param[in] dev VirtIO device structure to initialize
 * @param[in] type VirtIO device type
 * @param[in] index Device index (for multiple devices of same type)
 * @param[in] msix_vector_count Number of MSI-X vectors to configure (0 for
 * none)
 * @return EOK on success, error code otherwise
 */
int virtio_pci_init(struct virtio_device *dev, uint16_t type, size_t index,
                    uint16_t msix_vector_count);

#endif
