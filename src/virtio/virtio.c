/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */
#include "virtio.h"
#include "virtq.h"
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/neutrino.h>
#include <syslog.h>

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
    syslog(LOG_ERR, "failed to attach interrupt: %s", strerror(errno));
    return NULL;
  }

  while (true) {
    InterruptUnmask(intr->irq, id);
    InterruptWait(0, NULL);

    virtio_virtq_callback(intr->dev, intr->vq, intr->callback);
  }
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
    syslog(LOG_ERR,
           "invalid argument: dev, callback, and vq must not be NULL\n");
    return EINVAL;
  }

  syslog(LOG_DEBUG, "creating queue %d with requested size %d", index, size);

  virtio_max_queue_size(dev, index, &queue_size);
  syslog(LOG_DEBUG, "max queue size for queue %d is %d", index, queue_size);

  queue_size = size == 0 ? queue_size : min(size, queue_size);
  if (queue_size == 0) {
    syslog(LOG_ERR, "queue size must be greater than 0");
    return ENODEV;
  }

  syslog(LOG_INFO, "creating queue %d with size %d", index, queue_size);
  rc = virtq_create(queue_size, dev->leagcy, vq);
  if (rc != EOK) {
    syslog(LOG_ERR, "failed to create virtqueue: %s", strerror(rc));
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
    return ENOMEM;
  }

  rc = pthread_spin_init(&vdev->lock, PTHREAD_PROCESS_PRIVATE);
  if (rc != EOK) {
    free(vdev);
  }

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
