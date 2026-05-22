/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */
#include "virtio.h"
#include "logging.h"
#include "virtq.h"
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/neutrino.h>

struct virtio_interrupt {
  struct virtio_device *dev;
  struct virtq *vq;
  int irq;
  int (*callback)(struct virtio_device *dev, struct virtq *vq);
};

static inline int virtio_virtq_callback(
    struct virtio_device *dev, struct virtq *vq,
    int (*callback)(struct virtio_device *dev, struct virtq *vq)) {
  return dev->ops.virtq_callback(dev, vq, callback);
}

void *virtio_ist(void *arg) {
  int id;
  struct virtio_interrupt *intr = arg;

  id = InterruptAttachThread(intr->irq, 0);
  if (id == -1) {
    log_err("failed to attach interrupt %d: %s", intr->irq, strerror(errno));
    return NULL;
  }

  log_debug("interrupt service thread attached to irq %d", intr->irq);

  while (true) {
    InterruptUnmask(intr->irq, id);
    InterruptWait(0, NULL);

    virtio_virtq_callback(intr->dev, intr->vq, intr->callback);
  }
}

int virtio_reset_device(struct virtio_device *dev) {
  int rc;
  unsigned int remaining_timeout_ms;
  unsigned int remaining_poll_interval_ms;
  uint8_t device_status;

  if ((rc = dev->ops.reset_device(dev)) != EOK) {
    return rc;
  }

  remaining_timeout_ms = dev->device_reset_timeout_ms;

  if ((rc = virtio_get_device_status(dev, &device_status)) != EOK) {
    return rc;
  }

  while (device_status != 0 && remaining_timeout_ms > 0) {
    remaining_poll_interval_ms = dev->device_reset_poll_interval_ms;
    while (remaining_poll_interval_ms > 0) {
      remaining_poll_interval_ms = delay(remaining_poll_interval_ms);
    }

    remaining_timeout_ms -=
        min(remaining_timeout_ms, dev->device_reset_poll_interval_ms);

    if ((rc = virtio_get_device_status(dev, &device_status)) != EOK) {
      return rc;
    }
  }

  return device_status == 0 ? EOK : ETIMEDOUT;
}

int virtio_create_queue(struct virtio_device *dev, uint16_t index,
                        uint16_t size, int irq,
                        int (*callback)(struct virtio_device *dev,
                                        struct virtq *vq),
                        struct virtq **vq) {
  struct virtio_interrupt *intr;
  uint16_t queue_size;
  int rc;

  if (dev == NULL || callback == NULL || vq == NULL) {
    log_err("invalid argument: dev, callback, and vq must not be NULL: %s",
            strerror(EINVAL));
    return EINVAL;
  }

  log_debug("creating queue %d with requested size %d", index, size);

  virtio_max_queue_size(dev, index, &queue_size);
  log_debug("max queue size for queue %d is %d", index, queue_size);

  queue_size = size == 0 ? queue_size : min(size, queue_size);
  if (queue_size == 0) {
    log_err("queue size must be greater than 0: %s", strerror(ENODEV));
    return ENODEV;
  }

  log_info("creating queue %d with size %d", index, queue_size);
  rc = virtq_create(queue_size, dev->leagcy, vq);
  if (rc != EOK) {
    log_err("failed to create virtqueue: %s", strerror(rc));
    return rc;
  }

  dev->ops.create_queue(dev, *vq, index);

  intr = calloc(1, sizeof(struct virtio_interrupt));
  intr->dev = dev;
  intr->vq = *vq;
  intr->irq = irq;
  intr->callback = callback;

  pthread_create(NULL, NULL, virtio_ist, intr);

  return EOK;
}

int virtio_init(struct virtio_device **dev) {
  struct virtio_device *vdev;
  int rc;

  if (dev == NULL) {
    return EINVAL;
  }

  vdev = calloc(1, sizeof(struct virtio_device));
  if (vdev == NULL) {
    rc = errno;
    log_err("failed to allocate virtio device structure: %s", strerror(rc));
    return rc;
  }

  rc = pthread_spin_init(&vdev->lock, PTHREAD_PROCESS_PRIVATE);
  if (rc != EOK) {
    log_err("failed to initialize device lock: %s", strerror(rc));
  }

  vdev->device_reset_timeout_ms = VIRTIO_CONFIG_DEVICE_RESET_TIMEOUT_MS;
  vdev->device_reset_poll_interval_ms =
      VIRTIO_CONFIG_DEVICE_RESET_POLL_INTERVAL_MS;

  *dev = vdev;
  return rc;
}

int virtio_destroy(struct virtio_device *dev) {
  int rc;

  if (dev == NULL) {
    return EINVAL;
  }

  rc = pthread_spin_destroy(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  free(dev);

  return EOK;
}
