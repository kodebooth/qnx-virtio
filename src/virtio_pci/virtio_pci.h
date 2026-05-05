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
 * @brief Get the device status from a VirtIO device
 *
 * @param[in] dev VirtIO device
 * @param[out] status Pointer to store the device status
 * @return EOK on success, EINVAL if parameters are invalid
 */
int virtio_pci_get_device_status(struct virtio_device *const dev,
                                 uint8_t *status);

/**
 * @brief Set the device status of a VirtIO device
 *
 * @param[in] dev VirtIO device
 * @param[in] status Device status to set
 * @return EOK on success, EINVAL if parameters are invalid
 */
int virtio_pci_set_device_status(struct virtio_device *const dev,
                                 uint8_t status);

/**
 * @brief Add status flags to the device status
 *
 * This function reads the current device status, ORs it with the provided
 * status flags, and writes it back atomically using a spinlock.
 *
 * @param[in] dev VirtIO device
 * @param[in] status Status flags to add
 * @return EOK on success, EINVAL if parameters are invalid, or error from lock
 */
int virtio_pci_add_device_status(struct virtio_device *const dev,
                                 uint8_t status);

/**
 * @brief Reset a VirtIO device
 *
 * Sets the device status to 0, initiating a device reset.
 *
 * @param[in] dev VirtIO device
 * @return EOK on success, EINVAL if parameters are invalid
 */
int virtio_pci_reset_device(struct virtio_device *dev);

/**
 * @brief Select a virtqueue for subsequent operations
 *
 * @param[in] dev VirtIO device
 * @param[in] index Queue index to select
 * @return EOK on success, EINVAL if parameters are invalid
 */
int virtio_pci_select_queue(struct virtio_device *dev, uint16_t index);

/**
 * @brief Notify the device about available buffers in a queue
 *
 * @param[in] dev VirtIO device
 * @param[in] index Queue index to notify
 * @return EOK on success, EINVAL if parameters are invalid, or error from lock
 */
int virtio_pci_notify_queue(struct virtio_device *dev, uint16_t index);

/**
 * @brief Enable or disable a virtqueue
 *
 * @param[in] dev VirtIO device
 * @param[in] index Queue index
 * @param[in] enable true to enable, false to disable
 * @return EOK on success, EINVAL if parameters are invalid, or error from lock
 */
int virtio_pci_enable_queue(struct virtio_device *dev, uint16_t index,
                            bool enable);

/**
 * @brief Reset a virtqueue
 *
 * Initiates a queue reset and waits for it to complete.
 *
 * @param[in] dev VirtIO device
 * @param[in] index Queue index to reset
 * @return EOK on success, EINVAL if parameters are invalid, or error from lock
 */
int virtio_pci_reset_queue(struct virtio_device *dev, uint16_t index);

/**
 * @brief Get the size of a virtqueue
 *
 * @param[in] dev VirtIO device
 * @param[in] index Queue index
 * @param[out] size Pointer to store the queue size
 * @return EOK on success, EINVAL if parameters are invalid, or error from lock
 */
int virtio_pci_queue_size(struct virtio_device *dev, uint16_t index,
                          uint16_t *size);

/**
 * @brief Set the size of a virtqueue
 *
 * @param[in] dev VirtIO device
 * @param[in] index Queue index
 * @param[in] size Queue size to set
 * @return EOK on success, EINVAL if parameters are invalid, or error from lock
 */
int virtio_pci_set_queue_size(struct virtio_device *dev, uint16_t index,
                              uint16_t size);

/**
 * @brief Read device feature bits
 *
 * @param[in] dev VirtIO device
 * @param[out] features Array to store feature bits
 * @param[in] len Number of 32-bit feature words to read
 * @return EOK on success, EINVAL if parameters are invalid
 */
int virtio_pci_read_device_features(struct virtio_device *dev,
                                    uint32_t *features, size_t len);

/**
 * @brief Write driver feature bits
 *
 * @param[in] dev VirtIO device
 * @param[in] features Array of feature bits to write
 * @param[in] len Number of 32-bit feature words to write
 * @return EOK on success, EINVAL if parameters are invalid, or error from lock
 */
int virtio_pci_write_driver_features(struct virtio_device *dev,
                                     const uint32_t *features, size_t len);

/**
 * @brief Read device-specific configuration space
 *
 * @param[in] dev VirtIO device
 * @param[out] dst Destination buffer
 * @param[in] len Number of bytes to read
 * @param[in] offset Offset within device configuration space
 * @return EOK on success, EINVAL if parameters are invalid, or error from lock
 */
int virtio_pci_read_device_config(struct virtio_device *dev, void *dst,
                                  size_t len, size_t offset);

/**
 * @brief Read the MSI-X vector count for a PCI device
 *
 * @param[in] dev VirtIO PCI device
 * @param[out] count Pointer to store the vector count
 * @return EOK on success, EINVAL if parameters are invalid
 */
int virtio_pci_read_msix_vector_count(struct virtio_pci_device *dev,
                                      uint16_t *count);

/**
 * @brief Initialize a VirtIO PCI device
 *
 * Discovers and initializes a VirtIO PCI device, mapping BARs and setting up
 * MSI-X if requested.
 *
 * @param[in,out] dev VirtIO device structure to initialize
 * @param[in] type VirtIO device type
 * @param[in] index Device index (for multiple devices of same type)
 * @param[in] msix_vector_count Number of MSI-X vectors to configure (0 for
 * none)
 * @return EOK on success, error code otherwise
 */
int virtio_pci_init(struct virtio_device **dev, uint16_t type, size_t index,
                    uint16_t msix_vector_count);

/**
 * @brief Create and configure a virtqueue
 *
 * @param[in] dev VirtIO device
 * @param[in] vq Virtqueue structure
 * @param[in] index Queue index
 * @return EOK on success, error code otherwise
 */
int virtio_pci_create_queue(struct virtio_device *dev, struct virtq *vq,
                            const uint16_t index);

#endif
