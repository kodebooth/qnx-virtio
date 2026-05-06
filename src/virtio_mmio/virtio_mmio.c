/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */
#include "virtio_mmio.h"
#include "virtio.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VIRTIO_MMIO_MAGIC_VALUE 0x000
#define VIRTIO_MMIO_VERSION 0x004
#define VIRTIO_MMIO_DEVICE_ID 0x008
#define VIRTIO_MMIO_VENDOR_ID 0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028
#define VIRTIO_MMIO_QUEUE_SEL 0x030
#define VIRTIO_MMIO_QUEUE_SIZE_MAX 0x034
#define VIRTIO_MMIO_QUEUE_SIZE 0x038
#define VIRTIO_MMIO_QUEUE_ALIGN 0x03c
#define VIRTIO_MMIO_QUEUE_PFN 0x040
#define VIRTIO_MMIO_QUEUE_READY 0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK 0x064
#define VIRTIO_MMIO_STATUS 0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW 0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_QUEUE_DRIVER_LOW 0x090
#define VIRTIO_MMIO_QUEUE_DRIVER_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_DEVICE_LOW 0x0a0
#define VIRTIO_MMIO_QUEUE_DEVICE_HIGH 0x0a4
#define VIRTIO_MMIO_QUEUE_RESET 0x0c0
#define VIRTIO_MMIO_CONFIG_GENERATION 0x0fc
#define VIRTIO_MMIO_CONFIG 0x100

#define VIRTIO_MMIO_VERSION_LEGACY 1
#define VIRTIO_MMIO_VERSION_MODERN 2

#define VIRTIO_MMIO_INT_VIRTQ (1UL << 0)
#define VIRTIO_MMIO_INT_CONFIG (1UL << 1)

struct virtio_mmio_device {
  uint64_t paddr;
  uint64_t vaddr;
  uint16_t pagesize;
};

/**
 * @brief Write a 32-bit value to a VirtIO MMIO register
 *
 * @param[in] dev MMIO device
 * @param[in] offset Register offset
 * @param[in] value Value to write
 */
static inline void virtio_mmio_write32(struct virtio_mmio_device *dev,
                                       uint32_t offset, uint32_t value) {
  volatile uint32_t *addr = (uint32_t *)(dev->vaddr + offset);
  *addr = value;
}

/**
 * @brief Read a 32-bit value from a VirtIO MMIO register
 *
 * @param[in] dev MMIO device
 * @param[in] offset Register offset
 * @return The 32-bit value read from the register
 */
static inline uint32_t virtio_mmio_read32(struct virtio_mmio_device *dev,
                                          uint32_t offset) {
  volatile uint32_t *addr = (uint32_t *)(dev->vaddr + offset);
  return *addr;
}

int virtio_mmio_read_device_features(struct virtio_device *const dev,
                                     uint32_t *features, size_t count) {
  uint32_t select;
  struct virtio_mmio_device *mdev;

  if (dev == NULL || features == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;
  if (mdev == NULL) {
    return EINVAL;
  }

  for (select = 0; select < count; select++) {
    virtio_mmio_write32(mdev, VIRTIO_MMIO_DEVICE_FEATURES_SEL, select);
    features[select] = virtio_mmio_read32(mdev, VIRTIO_MMIO_DEVICE_FEATURES);
  }

  return EOK;
}

int virtio_mmio_write_driver_features(struct virtio_device *const dev,
                                      const uint32_t *features, size_t count) {
  uint32_t select;
  struct virtio_mmio_device *mdev;

  if (dev == NULL || features == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;
  if (mdev == NULL) {
    return EINVAL;
  }

  for (select = 0; select < count; select++) {
    virtio_mmio_write32(mdev, VIRTIO_MMIO_DRIVER_FEATURES_SEL, select);
    virtio_mmio_write32(mdev, VIRTIO_MMIO_DRIVER_FEATURES, features[select]);
  }

  return EOK;
}

int virtio_mmio_get_device_status(struct virtio_device *dev, uint8_t *status) {
  struct virtio_mmio_device *mdev;

  if (dev == NULL || status == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  *status = virtio_mmio_read32(mdev, VIRTIO_MMIO_STATUS) & 0xFF;

  return EOK;
}

int virtio_mmio_set_device_status(struct virtio_device *const dev,
                                  uint8_t status) {
  struct virtio_mmio_device *mdev;

  if (dev == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  virtio_mmio_write32(mdev, VIRTIO_MMIO_STATUS, status);

  return EOK;
}

int virtio_mmio_add_device_status(struct virtio_device *const dev,
                                  uint8_t status) {
  int rc;
  uint8_t current_status;

  if (dev == NULL) {
    return EINVAL;
  }

  rc = pthread_spin_lock(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_mmio_get_device_status(dev, &current_status);
  if (rc != EOK) {
    goto unlock;
  }

  rc = virtio_mmio_set_device_status(dev, current_status | status);
  if (rc != EOK) {
    goto unlock;
  }

unlock:
  pthread_spin_unlock(&dev->lock);
  return rc;
}

int virtio_mmio_reset_device(struct virtio_device *const dev) {
  struct virtio_mmio_device *mdev;

  if (dev == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  virtio_mmio_write32(mdev, VIRTIO_MMIO_STATUS, 0);

  return EOK;
}

int virtio_mmio_select_queue(struct virtio_device *const dev, uint16_t index) {
  struct virtio_mmio_device *mdev;

  if (dev == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_SEL, index);

  return EOK;
}

int virtio_mmio_notify_queue(struct virtio_device *const dev, uint16_t index) {
  int rc;
  struct virtio_mmio_device *mdev;

  if (dev == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  rc = pthread_spin_lock(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_mmio_select_queue(dev, index);
  if (rc != EOK) {
    goto unlock;
  }

  virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_NOTIFY, index);

unlock:
  pthread_spin_unlock(&dev->lock);
  return rc;
}

int virtio_mmio_enable_queue(struct virtio_device *const dev, uint16_t index,
                             bool enable) {
  int rc;
  struct virtio_mmio_device *mdev;

  if (dev == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  rc = pthread_spin_lock(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_mmio_select_queue(dev, index);
  if (rc != EOK) {
    goto unlock;
  }

  virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_READY, enable ? 1 : 0);

unlock:
  pthread_spin_unlock(&dev->lock);
  return rc;
}

int virtio_mmio_reset_queue(struct virtio_device *const dev, uint16_t index) {
  int rc;
  struct virtio_mmio_device *mdev;

  if (dev == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  rc = pthread_spin_lock(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_mmio_select_queue(dev, index);
  if (rc != EOK) {
    goto unlock;
  }

  virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_RESET, 1);
  while (virtio_mmio_read32(mdev, VIRTIO_MMIO_QUEUE_RESET) != 0) {
    /* wait for reset to complete */
  }

unlock:
  pthread_spin_unlock(&dev->lock);
  return rc;
}

int virtio_mmio_max_queue_size(struct virtio_device *const dev, uint16_t index,
                               uint16_t *size) {
  int rc;
  struct virtio_mmio_device *mdev;

  if (dev == NULL || size == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  rc = pthread_spin_lock(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_mmio_select_queue(dev, index);
  if (rc != EOK) {
    goto unlock;
  }

  *size = virtio_mmio_read32(mdev, VIRTIO_MMIO_QUEUE_SIZE_MAX);

unlock:
  pthread_spin_unlock(&dev->lock);
  return rc;
}

int virtio_mmio_set_queue_size(struct virtio_device *const dev, uint16_t index,
                               uint16_t size) {
  int rc;
  struct virtio_mmio_device *mdev;

  if (dev == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  rc = pthread_spin_lock(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_mmio_select_queue(dev, index);
  if (rc != EOK) {
    goto unlock;
  }

  virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_SIZE, size);

unlock:
  pthread_spin_unlock(&dev->lock);
  return rc;
}

int virtio_mmio_get_interrupt_status(struct virtio_device *const dev,
                                     uint32_t *status) {
  struct virtio_mmio_device *mdev;

  if (dev == NULL || status == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  *status = virtio_mmio_read32(mdev, VIRTIO_MMIO_INTERRUPT_STATUS);

  return EOK;
}

int virtio_mmio_set_interrupt_ack(struct virtio_device *const dev,
                                  uint32_t status) {
  struct virtio_mmio_device *mdev;

  if (dev == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  virtio_mmio_write32(mdev, VIRTIO_MMIO_INTERRUPT_ACK, status);

  return EOK;
}

int virtio_mmio_read_device_config(struct virtio_device *const dev, void *dst,
                                   size_t len, size_t offset) {
  int rc;
  struct virtio_mmio_device *mdev;

  if (dev == NULL || dst == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  rc = pthread_spin_lock(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  for (size_t i = 0; i < len; i += sizeof(uint32_t)) {
    ((uint32_t *)dst)[i] =
        virtio_mmio_read32(mdev, VIRTIO_MMIO_CONFIG + offset + i);
  }

  return pthread_spin_unlock(&dev->lock);
}

static int virtio_mmio_virtq_callback(struct virtio_device *dev,
                                      struct virtq *vq,
                                      int (*callback)(struct virtio_device *dev,
                                                      struct virtq *vq)) {
  int rc;
  uint32_t status;

  if (dev == NULL || vq == NULL || callback == NULL) {
    return EINVAL;
  }

  rc = virtio_mmio_get_interrupt_status(dev, &status);
  if (rc != EOK) {
    return rc;
  }

  if (status & VIRTIO_MMIO_INT_VIRTQ) {
    rc = virtio_mmio_set_interrupt_ack(dev, VIRTIO_MMIO_INT_VIRTQ);
    if (rc != EOK) {
      return rc;
    }
  }

  do {
    rc = callback(dev, vq);
  } while (rc == EOK);

  return rc;
}

int virtio_mmio_create_queue(struct virtio_device *const dev, struct virtq *vq,
                             uint16_t index) {
  uint16_t num;
  uint64_t paddr;
  uint64_t pfn;
  struct virtio_mmio_device *mdev;
  int rc;

  if (dev == NULL || vq == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  rc = virtq_size(vq, &num);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_mmio_reset_queue(dev, index);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_mmio_set_queue_size(dev, index, num);
  if (rc != EOK) {
    return rc;
  }

  rc = pthread_spin_lock(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  rc = virtq_desc_paddr(vq, (intptr_t *)&paddr);
  if (rc != EOK) {
    return rc;
  }
  if (dev->leagcy) {
    pfn = paddr >> 12;
    assert(pfn >> 32 == 0);

    virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_PFN, pfn);
  } else {
    virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_DESC_LOW, paddr & 0xFFFFFFFF);
    virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_DESC_HIGH, paddr >> 32);

    rc = virtq_avail_paddr(vq, (intptr_t *)&paddr);
    if (rc != EOK) {
      goto unlock;
    }
    virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_DRIVER_LOW, paddr & 0xFFFFFFFF);
    virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_DRIVER_HIGH, paddr >> 32);

    rc = virtq_used_paddr(vq, (intptr_t *)&paddr);
    if (rc != EOK) {
      goto unlock;
    }
    virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_DEVICE_LOW, paddr & 0xFFFFFFFF);
    virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_DEVICE_HIGH, paddr >> 32);
  }

unlock:
  pthread_spin_unlock(&dev->lock);
  return rc;
}

int virtio_mmio_init(struct virtio_device **dev, uint64_t address,
                     uint16_t type, int irq) {
  struct virtio_mmio_device *mdev;
  struct virtio_device *vdev;
  void *vaddr;
  int rc;

  rc = virtio_init(dev);
  if (rc != EOK) {
    return rc;
  }

  vdev = *dev;

  mdev = calloc(1, sizeof(struct virtio_mmio_device));
  if (mdev == NULL) {
    rc = ENOMEM;
    goto free_vdev;
  }

  mdev->paddr = address;
  vaddr = mmap64(0, 0x200, PROT_READ | PROT_WRITE | PROT_NOCACHE,
                 MAP_SHARED | MAP_NOINIT | MAP_PHYS, NOFD, mdev->paddr);
  if (vaddr == MAP_FAILED) {
    rc = errno;
    goto free_mdev;
  }

  mdev->vaddr = (uint64_t)vaddr;
  mdev->pagesize = getpagesize();
  vdev->irq = irq;

  vdev->priv = mdev;

  if (virtio_mmio_read32(mdev, VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976) {
    rc = ENODEV;
    goto free_mdev;
  }

  vdev->leagcy = virtio_mmio_read32(mdev, VIRTIO_MMIO_VERSION) ==
                 VIRTIO_MMIO_VERSION_LEGACY;

  uint32_t device_id = virtio_mmio_read32(mdev, VIRTIO_MMIO_DEVICE_ID);
  if (device_id != type) {
    rc = ENODEV;
    goto free_mdev;
  }

  if (vdev->leagcy) {
    virtio_mmio_write32(mdev, VIRTIO_MMIO_GUEST_PAGE_SIZE, mdev->pagesize);
  }

  vdev->ops.read_device_features = virtio_mmio_read_device_features;
  vdev->ops.write_driver_features = virtio_mmio_write_driver_features;
  vdev->ops.set_device_status = virtio_mmio_set_device_status;
  vdev->ops.add_device_status = virtio_mmio_add_device_status;
  vdev->ops.reset_device = virtio_mmio_reset_device;
  vdev->ops.get_device_status = virtio_mmio_get_device_status;
  vdev->ops.read_device_config = virtio_mmio_read_device_config;
  vdev->ops.create_queue = virtio_mmio_create_queue;
  vdev->ops.select_queue = virtio_mmio_select_queue;
  vdev->ops.notify_queue = virtio_mmio_notify_queue;
  vdev->ops.enable_queue = virtio_mmio_enable_queue;
  vdev->ops.reset_queue = virtio_mmio_reset_queue;
  vdev->ops.max_queue_size = virtio_mmio_max_queue_size;
  vdev->ops.virtq_callback = virtio_mmio_virtq_callback;

  return EOK;

free_mdev:
  free(mdev);
free_vdev:
  virtio_destroy(vdev);

  return rc;
}
