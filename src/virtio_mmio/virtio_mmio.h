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
 * @brief Read device feature bits
 *
 * @param[in] dev VirtIO device
 * @param[out] features Array to store feature bits
 * @param[in] count Number of 32-bit feature words to read
 * @return EOK on success, EINVAL if parameters are invalid
 */
int virtio_mmio_read_device_features(struct virtio_device *const dev,
                                     uint32_t *features, size_t count);

/**
 * @brief Write driver feature bits
 *
 * @param[in] dev VirtIO device
 * @param[in] features Array of feature bits to write
 * @param[in] count Number of 32-bit feature words to write
 * @return EOK on success, EINVAL if parameters are invalid
 */
int virtio_mmio_write_driver_features(struct virtio_device *const dev,
                                      const uint32_t *features, size_t count);

/**
 * @brief Get the device status
 *
 * @param[in] dev VirtIO device
 * @param[out] status Pointer to store the device status
 * @return EOK on success, EINVAL if parameters are invalid
 */
int virtio_mmio_get_device_status(struct virtio_device *dev, uint8_t *status);

/**
 * @brief Set the device status
 *
 * @param[in] dev VirtIO device
 * @param[in] status Device status to set
 * @return EOK on success, EINVAL if parameters are invalid
 */
int virtio_mmio_set_device_status(struct virtio_device *const dev,
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
int virtio_mmio_add_device_status(struct virtio_device *const dev,
                                  uint8_t status);

/**
 * @brief Reset a VirtIO device
 *
 * Sets the device status to 0, initiating a device reset.
 *
 * @param[in] dev VirtIO device
 * @return EOK on success, EINVAL if parameters are invalid
 */
int virtio_mmio_reset_device(struct virtio_device *const dev);

/**
 * @brief Select a virtqueue for subsequent operations
 *
 * @param[in] dev VirtIO device
 * @param[in] index Queue index to select
 * @return EOK on success, EINVAL if parameters are invalid
 */
int virtio_mmio_select_queue(struct virtio_device *const dev, uint16_t index);

/**
 * @brief Notify the device about available buffers in a queue
 *
 * @param[in] dev VirtIO device
 * @param[in] index Queue index to notify
 * @return EOK on success, EINVAL if parameters are invalid, or error from lock
 */
int virtio_mmio_notify_queue(struct virtio_device *const dev, uint16_t index);

/**
 * @brief Enable or disable a virtqueue
 *
 * @param[in] dev VirtIO device
 * @param[in] index Queue index
 * @param[in] enable true to enable, false to disable
 * @return EOK on success, EINVAL if parameters are invalid, or error from lock
 */
int virtio_mmio_enable_queue(struct virtio_device *const dev, uint16_t index,
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
int virtio_mmio_reset_queue(struct virtio_device *const dev, uint16_t index);

/**
 * @brief Get the maximum size of a virtqueue
 *
 * @param[in] dev VirtIO device
 * @param[in] index Queue index
 * @param[out] size Pointer to store the maximum queue size
 * @return EOK on success, EINVAL if parameters are invalid, or error from lock
 */
int virtio_mmio_max_queue_size(struct virtio_device *const dev, uint16_t index,
                               uint16_t *size);

/**
 * @brief Set the size of a virtqueue
 *
 * @param[in] dev VirtIO device
 * @param[in] index Queue index
 * @param[in] size Queue size to set
 * @return EOK on success, EINVAL if parameters are invalid, or error from lock
 */
int virtio_mmio_set_queue_size(struct virtio_device *const dev, uint16_t index,
                               uint16_t size);

/**
 * @brief Get the interrupt status
 *
 * @param[in] dev VirtIO device
 * @param[out] status Pointer to store the interrupt status
 * @return EOK on success, EINVAL if parameters are invalid
 */
int virtio_mmio_get_interrupt_status(struct virtio_device *const dev,
                                     uint32_t *status);

/**
 * @brief Acknowledge interrupts
 *
 * @param[in] dev VirtIO device
 * @param[in] status Interrupt status bits to acknowledge
 * @return EOK on success, EINVAL if parameters are invalid
 */
int virtio_mmio_set_interrupt_ack(struct virtio_device *const dev,
                                  uint32_t status);

/**
 * @brief Read device-specific configuration space
 *
 * @param[in] dev VirtIO device
 * @param[out] dst Destination buffer
 * @param[in] len Number of bytes to read
 * @param[in] offset Offset within device configuration space
 * @return EOK on success, EINVAL if parameters are invalid, or error from lock
 */
int virtio_mmio_read_device_config(struct virtio_device *const dev, void *dst,
                                   size_t len, size_t offset);

/**
 * @brief Initialize a VirtIO MMIO device
 *
 * Discovers and initializes a VirtIO MMIO device at the specified physical
 * address. Maps the MMIO region, validates the device, and sets up the
 * device operations. Automatically detects legacy vs modern interface.
 *
 * @param[in,out] dev VirtIO device structure to initialize
 * @param[in] paddr Physical address of the MMIO region
 * @param[in] type Expected VirtIO device type
 * @param[in] irq Interrupt request number for the device
 * @return EOK on success, error code otherwise
 */
int virtio_mmio_init(struct virtio_device **dev, uint64_t paddr, uint16_t type,
                     int irq);

/**
 * @brief Create and configure a virtqueue
 *
 * Configures a virtqueue by setting its size, resetting it, and programming
 * the descriptor, available, and used ring addresses into the device.
 * Supports both legacy and modern VirtIO MMIO interfaces.
 *
 * @param[in] dev VirtIO device
 * @param[in] vq Virtqueue structure
 * @param[in] index Queue index
 * @return EOK on success, error code otherwise
 */
int virtio_mmio_create_queue(struct virtio_device *const dev, struct virtq *vq,
                             uint16_t index);

#endif
