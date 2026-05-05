/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */

/**
 * @file fifo.h
 * @brief Thread-safe FIFO (First-In-First-Out) queue implementation
 *
 * This module provides a generic, thread-safe FIFO queue that can store
 * fixed-size elements. The implementation uses spinlocks for synchronization.
 */

#ifndef QNX_VIRTIO_UTILS_FIFO_FIFO_H_INCLUDED
#define QNX_VIRTIO_UTILS_FIFO_FIFO_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Opaque FIFO queue structure
 *
 * This structure represents a FIFO queue. The internal implementation
 * is hidden from users of the API.
 */
struct fifo;

/**
 * @brief Create a new FIFO queue
 *
 * Allocates and initializes a new FIFO queue with the specified capacity
 * and element size. The queue is thread-safe and uses spinlocks for
 * synchronization.
 *
 * @param[in] capacity Maximum number of elements the FIFO can hold
 * @param[in] elem_size Size of each element in bytes
 * @param[out] fifo Pointer to receive the created FIFO instance
 *
 * @return EOK on success, or an error code on failure:
 *         - ENOMEM if memory allocation fails
 *         - Error code from pthread_spin_init() if spinlock initialization
 * fails
 */
int fifo_create(const size_t capacity, const size_t elem_size,
                struct fifo **fifo);

/**
 * @brief Destroy a FIFO queue
 *
 * Destroys the spinlock and frees all memory associated with the FIFO queue.
 * After this call, the FIFO pointer is no longer valid.
 *
 * @param[in] fifo FIFO instance to destroy
 *
 * @return EOK on success, or an error code from pthread_spin_destroy()
 */
int fifo_destory(struct fifo *const fifo);

/**
 * @brief Check if the FIFO is empty
 *
 * Checks whether the FIFO queue contains any elements.
 *
 * @param[in] fifo FIFO instance to check
 * @param[out] empty Pointer to receive the empty status (true if empty, false otherwise)
 *
 * @return EOK on success, or an error code on failure
 */
int fifo_empty(const struct fifo *const fifo, bool *const empty);

/**
 * @brief Check if the FIFO is full
 *
 * Checks whether the FIFO queue is at capacity.
 *
 * @param[in] fifo FIFO instance to check
 * @param[out] full Pointer to receive the full status (true if full, false otherwise)
 *
 * @return EOK on success, or an error code on failure
 */
int fifo_full(const struct fifo *const fifo, bool *const full);

/**
 * @brief Put an element into the FIFO queue
 *
 * Inserts an element at the tail of the FIFO queue. This operation is
 * thread-safe. The element data is copied into the FIFO.
 *
 * @param[in] fifo FIFO instance
 * @param[in] elem Pointer to the element to insert
 *
 * @return EOK on success, or an error code on failure:
 *         - ENOMEM if the FIFO is full
 *         - Error code from pthread_spin_lock() if lock acquisition fails
 */
int fifo_put(struct fifo *const fifo, const void *const elem);

/**
 * @brief Get an element from the FIFO queue
 *
 * Removes and retrieves an element from the head of the FIFO queue.
 * This operation is thread-safe. The element data is copied to the
 * provided buffer.
 *
 * @param[in] fifo FIFO instance
 * @param[out] elem Pointer to buffer where the element will be copied
 *
 * @return EOK on success, or an error code on failure:
 *         - ENOENT if the FIFO is empty
 *         - Error code from pthread_spin_lock() if lock acquisition fails
 */
int fifo_get(struct fifo *const fifo, void *const elem);

#endif
