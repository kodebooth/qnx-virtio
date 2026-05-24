/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */

/**
 * @file virtio.h
 * @brief VirtIO device abstraction layer for QNX
 *
 * This file provides a transport-agnostic interface for VirtIO devices,
 * supporting both PCI and MMIO transports. It implements the VirtIO
 * specification v1.1+ device initialization and operation.
 */

#ifndef QNX_VIRTIO_VIRTIO_H_INCLUDED
#define QNX_VIRTIO_VIRTIO_H_INCLUDED

#include "virtq.h"
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

/** @brief Device has acknowledged it is a VirtIO device */
#define VIRTIO_DEVICE_STATUS_ACKNOWLEDGE (1U << 0)

/** @brief Driver has loaded and recognized the device */
#define VIRTIO_DEVICE_STATUS_DRIVER (1U << 1)

/** @brief Driver is ready to drive the device */
#define VIRTIO_DEVICE_STATUS_DRIVER_OK (1U << 2)

/** @brief Driver has acknowledged all features it understands */
#define VIRTIO_DEVICE_STATUS_FEATURES_OK (1U << 3)

/** @brief Device has experienced an error and needs reset */
#define VIRTIO_DEVICE_STATUS_DEVICE_NEEDS_RESET (1U << 6)

/** @brief Driver has encountered an error and given up on the device */
#define VIRTIO_DEVICE_STATUS_FAILED (1U << 7)

/** @brief default device reset timeout */
#define VIRTIO_CONFIG_DEVICE_RESET_TIMEOUT_MS 500U

/** @brief default device reset poll interval */
#define VIRTIO_CONFIG_DEVICE_RESET_POLL_INTERVAL_MS 10U

/** @brief Forward declaration of VirtIO device structure */
struct virtio_device;

/**
 * @brief VirtIO transport operations
 *
 * Transport-specific operations that abstract PCI and MMIO implementations.
 * These callbacks are implemented by the specific transport layer (PCI/MMIO).
 */
struct virtio_ops {
  /** @brief Read device feature bits
   * @param dev VirtIO device instance
   * @param features Output buffer for feature bits
   * @param count Number of feature words to read
   * @return 0 on success, negative error code on failure
   */
  int (*read_device_features)(struct virtio_device *const dev,
                              uint32_t *features, size_t count);

  /** @brief Write driver feature bits
   * @param dev VirtIO device instance
   * @param features Feature bits accepted by driver
   * @param count Number of feature words to write
   * @return 0 on success, negative error code on failure
   */
  int (*write_driver_features)(struct virtio_device *const dev,
                               const uint32_t *features, size_t count);

  /** @brief Set device status (replaces current status)
   * @param dev VirtIO device instance
   * @param status New status value
   * @return 0 on success, negative error code on failure
   */
  int (*set_device_status)(struct virtio_device *const dev, uint8_t status);

  /** @brief Get current device status
   * @param dev VirtIO device instance
   * @param status Output buffer for status value
   * @return 0 on success, negative error code on failure
   */
  int (*get_device_status)(struct virtio_device *const dev, uint8_t *status);

  /** @brief Reset the device
   * @param dev VirtIO device instance
   * @return 0 on success, negative error code on failure
   */
  int (*reset_device)(struct virtio_device *const dev);

  /** @brief Read device-specific configuration
   * @param dev VirtIO device instance
   * @param dst Destination buffer
   * @param len Number of bytes to read
   * @param offset Offset into device config space
   * @return 0 on success, negative error code on failure
   */
  int (*read_device_config)(struct virtio_device *const dev, void *dst,
                            size_t len, size_t offset);

  /** @brief Set the physical address of a virtqueue
   * @param dev VirtIO device instance
   * @param vq Virtual queue to configure
   * @return 0 on success, negative error code on failure
   */
  int (*set_queue_addr)(struct virtio_device *const dev, struct virtq *vq);

  /** @brief Select a queue for subsequent operations
   * @param dev VirtIO device instance
   * @param index Queue index to select
   * @return 0 on success, negative error code on failure
   */
  int (*select_queue)(struct virtio_device *const dev, uint16_t index);

  /** @brief Notify device of available buffers in queue
   * @param dev VirtIO device instance
   * @param index Queue index to notify
   * @return 0 on success, negative error code on failure
   */
  int (*notify_queue)(struct virtio_device *const dev, uint16_t index);

  /** @brief Enable or disable a queue
   * @param dev VirtIO device instance
   * @param enable True to enable, false to disable
   * @return 0 on success, negative error code on failure
   */
  int (*enable_queue)(struct virtio_device *const dev, bool enable);

  /** @brief Reset a specific queue
   * @param dev VirtIO device instance
   * @return 0 on success, negative error code on failure
   */
  int (*reset_queue)(struct virtio_device *const dev);

  /** @brief Get maximum supported queue size
   * @param dev VirtIO device instance
   * @param size Output buffer for maximum size
   * @return 0 on success, negative error code on failure
   */
  int (*max_queue_size)(struct virtio_device *const dev, uint16_t *size);

  /** @brief Set queue size
   * @param dev VirtIO device instance
   * @param size Desired queue size (must be power of 2)
   * @return 0 on success, negative error code on failure
   */
  int (*set_queue_size)(struct virtio_device *const dev, uint16_t size);

  /** @brief Register virtqueue interrupt callback
   * @param dev VirtIO device instance
   * @param vq Virtqueue instance
   * @param callback Callback function to invoke on interrupt
   * @return 0 on success, negative error code on failure
   */
  int (*virtq_callback)(struct virtio_device *const dev, struct virtq *vq,
                        int (*callback)(struct virtio_device *dev,
                                        struct virtq *vq));
};

/**
 * @brief VirtIO device instance
 *
 * Represents a VirtIO device regardless of transport mechanism.
 * This structure is typically embedded in a transport-specific structure.
 */
struct virtio_device {
  /** @brief Spinlock for thread-safe access */
  pthread_spinlock_t lock;

  /** @brief True if device operates in legacy mode (pre-1.0) */
  bool leagcy;

  /** @brief Transport-specific operations */
  struct virtio_ops ops;

  /** @brief device reset timeout */
  unsigned int device_reset_timeout_ms;

  /** @brief device reset poll interval
   *
   * This is the interval at which the device status is polled to determine if
   * a reset was successful.
   */
  unsigned int device_reset_poll_interval_ms;

  /** @brief Interrupt number */
  int irq;

  /** @brief Private data for transport-specific use */
  void *priv;
};

/**
 * @brief Read device feature bits
 * @param dev VirtIO device instance
 * @param features Output buffer for feature bits
 * @param count Number of feature words to read
 * @return 0 on success, negative error code on failure
 */
static inline int virtio_read_device_features(struct virtio_device *dev,
                                              uint32_t *features,
                                              size_t count) {
  return dev->ops.read_device_features(dev, features, count);
}

/**
 * @brief Write driver-accepted feature bits
 * @param dev VirtIO device instance
 * @param features Feature bits accepted by driver
 * @param count Number of feature words to write
 * @return 0 on success, negative error code on failure
 */
static inline int virtio_write_driver_features(struct virtio_device *dev,
                                               const uint32_t *features,
                                               size_t count) {
  return dev->ops.write_driver_features(dev, features, count);
}

/**
 * @brief Set device status (replaces current status)
 * @param dev VirtIO device instance
 * @param status New status value (VIRTIO_DEVICE_STATUS_* bits)
 * @return 0 on success, negative error code on failure
 */
static inline int virtio_set_device_status(struct virtio_device *dev,
                                           uint8_t status) {
  return dev->ops.set_device_status(dev, status);
}

/**
 * @brief Get current device status
 * @param dev VirtIO device instance
 * @param status Output buffer for status value
 * @return 0 on success, negative error code on failure
 */
static inline int virtio_get_device_status(struct virtio_device *dev,
                                           uint8_t *status) {
  return dev->ops.get_device_status(dev, status);
}

/**
 * @brief Read device-specific configuration space
 * @param dev VirtIO device instance
 * @param dst Destination buffer
 * @param len Number of bytes to read
 * @param offset Offset into device config space
 * @return 0 on success, negative error code on failure
 */
static inline int virtio_read_device_config(struct virtio_device *dev,
                                            void *dst, size_t len,
                                            size_t offset) {
  return dev->ops.read_device_config(dev, dst, len, offset);
}

/**
 * @brief Select a queue for subsequent operations
 * @param dev VirtIO device instance
 * @param index Queue index to select
 * @return 0 on success, negative error code on failure
 */
static inline int virtio_select_queue(struct virtio_device *dev,
                                      uint16_t index) {
  return dev->ops.select_queue(dev, index);
}

/**
 * @brief Set the physical address of a virtqueue
 * @param dev VirtIO device instance
 * @param vq Virtual queue to configure
 * @return 0 on success, negative error code on failure
 */
static inline int virtio_set_queue_addr(struct virtio_device *dev,
                                        struct virtq *vq) {
  return dev->ops.set_queue_addr(dev, vq);
}

/**
 * @brief Add bits to device status (OR operation)
 * @param dev VirtIO device instance
 * @param status Status bits to add (VIRTIO_DEVICE_STATUS_* bits)
 * @return 0 on success, negative error code on failure
 */
int virtio_add_device_status(struct virtio_device *dev, uint8_t status);

/**
 * @brief Reset the VirtIO device
 *
 * @param dev VirtIO device instance
 * @return 0 on success, negative error code on failure
 */
int virtio_reset_device(struct virtio_device *dev);

/**
 * @brief Notify device of available buffers in queue
 * @param dev VirtIO device instance
 * @param index Queue index to notify
 * @return 0 on success, negative error code on failure
 */
int virtio_notify_queue(struct virtio_device *dev, uint16_t index);

/**
 * @brief Enable or disable a virtqueue
 * @param dev VirtIO device instance
 * @param index Queue index
 * @param enable True to enable, false to disable
 * @return 0 on success, negative error code on failure
 */
int virtio_enable_queue(struct virtio_device *dev, uint16_t index, bool enable);

/**
 * @brief Reset a specific virtqueue
 *
 * @todo (#2) queue reset feature negotiation
 *
 * @param dev VirtIO device instance
 * @param index Queue index to reset
 * @return 0 on success, negative error code on failure
 */
int virtio_reset_queue(struct virtio_device *dev, uint16_t index);

/**
 * @brief Get maximum supported queue size
 * @param dev VirtIO device instance
 * @param index Queue index
 * @param size Output buffer for maximum size
 * @return 0 on success, negative error code on failure
 */
int virtio_max_queue_size(struct virtio_device *dev, uint16_t index,
                          uint16_t *size);

/**
 * @brief Set queue size
 *
 * @param dev VirtIO device instance
 * @param index Queue index
 * @param size Desired queue size (must be power of 2)
 * @return 0 on success, negative error code on failure
 */
int virtio_set_queue_size(struct virtio_device *dev, uint16_t index,
                          uint16_t size);

/**
 * @brief Initialize a VirtIO device instance
 *
 * Allocates and initializes a new VirtIO device structure. The device
 * must be configured with transport-specific operations before use.
 *
 * @param dev Output pointer to receive the initialized device
 * @return 0 on success, negative error code on failure
 */
int virtio_init(struct virtio_device **dev);

/**
 * @brief Destroy a VirtIO device instance
 *
 * Frees resources associated with the VirtIO device. The device should
 * be reset and all queues destroyed before calling this function.
 *
 * @param dev VirtIO device instance to destroy
 * @return 0 on success, negative error code on failure
 */
int virtio_destroy(struct virtio_device *dev);

/**
 * @brief Create and configure a virtqueue
 *
 * Allocates and initializes a new virtqueue with the specified parameters.
 * The queue is registered with the device and configured for interrupt
 * delivery.
 *
 * @param dev VirtIO device instance
 * @param index Queue index (device-specific)
 * @param size Queue size (must be power of 2, <= max_queue_size)
 * @param irq Interrupt number for queue notifications
 * @param callback Function to call when queue receives interrupt
 * @param vq Output pointer to receive the created virtqueue
 * @return 0 on success, negative error code on failure
 */
int virtio_create_queue(struct virtio_device *dev, uint16_t index,
                        uint16_t size, int irq,
                        int (*callback)(struct virtio_device *dev,
                                        struct virtq *vq),
                        struct virtq **vq);

#endif
