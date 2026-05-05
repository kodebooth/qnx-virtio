/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */

/**
 * @file virtq.h
 * @brief VirtIO virtqueue management API
 *
 * This module provides an API for managing VirtIO virtqueues (virtual queues),
 * which are the primary mechanism for data transfer between drivers and devices
 * in the VirtIO specification. Supports both legacy and modern VirtIO queue
 * layouts.
 */

#ifndef QNX_VIRTIO_VIRTQ_H_INCLUDED
#define QNX_VIRTIO_VIRTQ_H_INCLUDED

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>

/**
 * @brief Opaque virtqueue structure
 *
 * Represents a VirtIO virtqueue. The internal implementation details are
 * hidden from users of the API.
 */
struct virtq;

/**
 * @brief Create a new virtqueue
 *
 * Allocates and initializes a VirtIO virtqueue with the specified size.
 * The queue can be created in either legacy or modern VirtIO format.
 *
 * @param[in] size Number of descriptors in the queue (must be a power of 2)
 * @param[in] legacy If true, create a legacy VirtIO queue; if false, create a
 * modern queue
 * @param[out] vq Pointer to receive the created virtqueue instance
 *
 * @return EOK on success, or an error code on failure
 */
int virtq_create(size_t size, bool legacy, struct virtq **vq);

/**
 * @brief Destroy a virtqueue
 *
 * Releases all resources associated with a virtqueue, including unmapping
 * memory regions.
 *
 * @param[in] vq Virtqueue instance to destroy
 *
 * @return EOK on success, or an error code on failure
 */
int virtq_destroy(struct virtq *const vq);

/**
 * @brief Get the physical address of the descriptor table
 *
 * @param[in] vq Virtqueue instance
 * @param[out] paddr Pointer to receive the physical address
 *
 * @return EOK on success, or an error code on failure
 */
int virtq_desc_paddr(const struct virtq *const vq, intptr_t *const paddr);

/**
 * @brief Get the physical address of the available ring
 *
 * @param[in] vq Virtqueue instance
 * @param[out] paddr Pointer to receive the physical address
 *
 * @return EOK on success, or an error code on failure
 */
int virtq_avail_paddr(const struct virtq *const vq, intptr_t *const paddr);

/**
 * @brief Get the physical address of the used ring
 *
 * @param[in] vq Virtqueue instance
 * @param[out] paddr Pointer to receive the physical address
 *
 * @return EOK on success, or an error code on failure
 */
int virtq_used_paddr(const struct virtq *const vq, intptr_t *const paddr);

/**
 * @brief Get the size of the virtqueue
 *
 * @param[in] vq Virtqueue instance
 * @param[out] num Pointer to receive the queue size (number of descriptors)
 *
 * @return EOK on success, or an error code on failure
 */
int virtq_size(const struct virtq *const vq, uint16_t *const num);

/**
 * @brief Allocate a descriptor from the virtqueue
 *
 * Allocates a free descriptor from the virtqueue's descriptor table
 * and configures it with the specified length and flags.
 *
 * @param[in] vq Virtqueue instance
 * @param[in] len Length of the buffer in bytes
 * @param[in] writable If true, the descriptor is device-writable; if false,
 * device-readable
 * @param[out] idx Pointer to receive the allocated descriptor index
 *
 * @return EOK on success, or an error code on failure:
 *         - ENOMEM if no descriptors are available
 */
int virtq_alloc_desc(struct virtq *const vq, uint32_t len, bool writable,
                     uint16_t *idx);

/**
 * @brief Free a descriptor back to the virtqueue
 *
 * Returns a previously allocated descriptor to the free list.
 *
 * @param[in] vq Virtqueue instance
 * @param[in] idx Index of the descriptor to free
 *
 * @return EOK on success, or an error code on failure
 */
int virtq_free_desc(struct virtq *const vq, uint16_t idx);

/**
 * @brief Get the virtual address associated with a descriptor
 *
 * @param[in] vq Virtqueue instance
 * @param[in] idx Descriptor index
 * @param[out] vaddr Pointer to receive the virtual address
 *
 * @return EOK on success, or an error code on failure
 */
int virtq_get_desc_vaddr(const struct virtq *const vq, uint16_t idx,
                         uint64_t *vaddr);

/**
 * @brief Get the length of a descriptor's buffer
 *
 * @param[in] vq Virtqueue instance
 * @param[in] idx Descriptor index
 * @param[out] len Pointer to receive the buffer length in bytes
 *
 * @return EOK on success, or an error code on failure
 */
int virtq_get_desc_len(const struct virtq *const vq, uint16_t idx,
                       uint32_t *len);

/**
 * @brief Get the flags of a descriptor
 *
 * @param[in] vq Virtqueue instance
 * @param[in] idx Descriptor index
 * @param[out] flags Pointer to receive the descriptor flags
 *
 * @return EOK on success, or an error code on failure
 */
int virtq_get_desc_flags(const struct virtq *const vq, uint16_t idx,
                         uint16_t *flags);

/**
 * @brief Get the context pointer associated with a descriptor
 *
 * Retrieves the user-defined context pointer that was previously set
 * for this descriptor.
 *
 * @param[in] vq Virtqueue instance
 * @param[in] idx Descriptor index
 * @param[out] context Pointer to receive the context pointer
 *
 * @return EOK on success, or an error code on failure
 */
int virtq_get_desc_context(const struct virtq *const vq, uint16_t idx,
                           void **context);

/**
 * @brief Set the context pointer for a descriptor
 *
 * Associates a user-defined context pointer with a descriptor.
 * This is useful for tracking application-specific data related to
 * the descriptor.
 *
 * @param[in] vq Virtqueue instance
 * @param[in] idx Descriptor index
 * @param[in] context Context pointer to associate with the descriptor
 *
 * @return EOK on success, or an error code on failure
 */
int virtq_set_desc_context(struct virtq *const vq, uint16_t idx, void *context);

/**
 * @brief Submit a descriptor chain to the available ring
 *
 * Places a chain of descriptors into the virtqueue's available ring,
 * making them visible to the device for processing.
 *
 * @param[in] vq Virtqueue instance
 * @param[in] count Number of descriptors in the chain
 * @param[in] idx Pointer to array of descriptor indices forming the chain
 *
 * @return EOK on success, or an error code on failure
 */
int virtq_put_desc_chain(struct virtq *const vq, size_t count, uint16_t *idx);

/**
 * @brief Retrieve a descriptor chain from the used ring
 *
 * Gets a completed descriptor chain from the virtqueue's used ring.
 * These are descriptors that have been processed by the device.
 *
 * @param[in] vq Virtqueue instance
 * @param[out] count Pointer to receive the number of descriptors in the chain
 * @param[out] idx Pointer to receive the head descriptor index
 *
 * @return EOK on success, or an error code on failure:
 *         - ENOENT if no completed descriptors are available
 */
int virtq_get_desc_chain(struct virtq *const vq, size_t *count, uint16_t *idx);

/**
 * @brief Print virtqueue information for debugging
 *
 * Outputs diagnostic information about the virtqueue state to the console.
 *
 * @param[in] vq Virtqueue instance
 */
void virtq_print(const struct virtq *const vq);

#endif
