/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */
#include "virtio_mmio.h"
#include "logging.h"
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

/**
 * @brief Read device feature bits
 *
 * @param[in] dev VirtIO device
 * @param[out] features Array to store feature bits
 * @param[in] count Number of 32-bit feature words to read
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_mmio_read_device_features(struct virtio_device *const dev,
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

/**
 * @brief Write driver feature bits
 *
 * @param[in] dev VirtIO device
 * @param[in] features Array of feature bits to write
 * @param[in] count Number of 32-bit feature words to write
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_mmio_write_driver_features(struct virtio_device *const dev,
                                             const uint32_t *features,
                                             size_t count) {
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

/**
 * @brief Get the device status
 *
 * @param[in] dev VirtIO device
 * @param[out] status Pointer to store the device status
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_mmio_get_device_status(struct virtio_device *dev,
                                         uint8_t *status) {
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

/**
 * @brief Set the device status
 *
 * @param[in] dev VirtIO device
 * @param[in] status Device status to set
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_mmio_set_device_status(struct virtio_device *const dev,
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

/**
 * @brief Reset a VirtIO device
 *
 * Sets the device status to 0, initiating a device reset.
 *
 * @param[in] dev VirtIO device
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_mmio_reset_device(struct virtio_device *const dev) {
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

/**
 * @brief Select a virtqueue for subsequent operations
 *
 * @param[in] dev VirtIO device
 * @param[in] index Queue index to select
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_mmio_select_queue(struct virtio_device *const dev,
                                    uint16_t index) {
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

/**
 * @brief Notify the device about available buffers in a queue
 *
 * @param[in] dev VirtIO device
 * @param[in] index Queue index to notify
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_mmio_notify_queue(struct virtio_device *const dev,
                                    uint16_t index) {
  struct virtio_mmio_device *mdev;

  if (dev == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_NOTIFY, index);

  return EOK;
}

/**
 * @brief Enable or disable a virtqueue
 *
 * @param[in] dev VirtIO device
 * @param[in] enable true to enable, false to disable
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_mmio_enable_queue(struct virtio_device *const dev,
                                    bool enable) {
  struct virtio_mmio_device *mdev;

  if (dev == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_READY, enable ? 1 : 0);

  return EOK;
}

/**
 * @brief Reset a virtqueue
 *
 * Initiates a queue reset and waits for it to complete.
 *
 * @param[in] dev VirtIO device
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_mmio_set_queue_reset(struct virtio_device *const dev) {
  struct virtio_mmio_device *mdev;

  if (dev == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_RESET, 1);

  return EOK;
}

static int virtio_mmio_get_queue_reset(struct virtio_device *const dev,
                                       uint32_t *value) {
  struct virtio_mmio_device *mdev;

  if (dev == NULL || value == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  *value = virtio_mmio_read32(mdev, VIRTIO_MMIO_QUEUE_RESET);

  return EOK;
}

/**
 * @brief Get the maximum size of a virtqueue
 *
 * @param[in] dev VirtIO device
 * @param[out] size Pointer to store the maximum queue size
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_mmio_max_queue_size(struct virtio_device *const dev,
                                      uint16_t *size) {
  struct virtio_mmio_device *mdev;

  if (dev == NULL || size == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  *size = virtio_mmio_read32(mdev, VIRTIO_MMIO_QUEUE_SIZE_MAX);

  return EOK;
}

/**
 * @brief Set the size of a virtqueue
 *
 * @param[in] dev VirtIO device
 * @param[in] size Queue size to set
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_mmio_set_queue_size(struct virtio_device *const dev,
                                      uint16_t size) {
  struct virtio_mmio_device *mdev;

  if (dev == NULL) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_SIZE, size);

  return EOK;
}

/**
 * @brief Get the interrupt status
 *
 * @param[in] dev VirtIO device
 * @param[out] status Pointer to store the interrupt status
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_mmio_get_interrupt_status(struct virtio_device *const dev,
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

/**
 * @brief Acknowledge interrupts
 *
 * @param[in] dev VirtIO device
 * @param[in] status Interrupt status bits to acknowledge
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_mmio_set_interrupt_ack(struct virtio_device *const dev,
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

/**
 * @brief Read device-specific configuration space
 *
 * @param[in] dev VirtIO device
 * @param[out] dst Destination buffer
 * @param[in] len Number of bytes to read
 * @param[in] offset Offset within device configuration space
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_mmio_read_device_config(struct virtio_device *const dev,
                                          void *dst, size_t len,
                                          size_t offset) {
  struct virtio_mmio_device *mdev;
  size_t words;

  if (dev == NULL || dst == NULL) {
    return EINVAL;
  }

  if ((len % sizeof(uint32_t)) || (offset % sizeof(uint32_t))) {
    return EINVAL;
  }

  mdev = dev->priv;

  if (mdev == NULL) {
    return EINVAL;
  }

  words = len / sizeof(uint32_t);

  for (size_t word = 0; word < words; word++) {
    ((uint32_t *)dst)[word] = virtio_mmio_read32(
        mdev, VIRTIO_MMIO_CONFIG + offset + (word * sizeof(uint32_t)));
  }

  return EOK;
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

/**
 * @brief Set the physical address of a virtqueue for MMIO device
 *
 * Configures the virtqueue physical address in MMIO registers. For legacy
 * devices, writes the page frame number (PFN) to QUEUE_PFN register. For
 * modern devices, writes 64-bit addresses for descriptor, available, and
 * used rings to their respective low/high register pairs.
 *
 * @param dev VirtIO MMIO device instance
 * @param vq Virtual queue to configure
 * @return EOK on success, error code on failure
 */
static int virtio_mmio_set_queue_addr(struct virtio_device *const dev,
                                      struct virtq *vq) {
  uint64_t paddr;
  uint64_t pfn;
  struct virtio_mmio_device *mdev;
  int rc;

  mdev = dev->priv;

  if ((rc = virtq_desc_paddr(vq, (intptr_t *)&paddr)) != EOK) {
    log_err("failed to get virtq physical address");
    return rc;
  }

  if (dev->leagcy) {
    pfn = paddr >> 12;
    assert(pfn >> 32 == 0);

    virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_PFN, pfn);
  } else {
    virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_DESC_LOW, paddr & 0xFFFFFFFF);
    virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_DESC_HIGH, paddr >> 32);

    if ((rc = virtq_avail_paddr(vq, (intptr_t *)&paddr)) != EOK) {
      return rc;
    }
    virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_DRIVER_LOW, paddr & 0xFFFFFFFF);
    virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_DRIVER_HIGH, paddr >> 32);

    if ((rc = virtq_used_paddr(vq, (intptr_t *)&paddr)) != EOK) {
      return rc;
    }
    virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_DEVICE_LOW, paddr & 0xFFFFFFFF);
    virtio_mmio_write32(mdev, VIRTIO_MMIO_QUEUE_DEVICE_HIGH, paddr >> 32);
  }

  return EOK;
}

int virtio_mmio_init(struct virtio_device *dev, uint64_t address, uint16_t type,
                     int irq) {
  struct virtio_mmio_device *mdev;
  void *vaddr;
  int rc;

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
  dev->irq = irq;

  dev->priv = mdev;

  uint32_t magic = virtio_mmio_read32(mdev, VIRTIO_MMIO_MAGIC_VALUE);
  if (magic != 0x74726976) {
    log_err("invalid virtio magic value: expected 0x74726976, found 0x%x: %s",
            magic, strerror(ENODEV));
    rc = ENODEV;
    goto free_mdev;
  }

  dev->leagcy = virtio_mmio_read32(mdev, VIRTIO_MMIO_VERSION) ==
                VIRTIO_MMIO_VERSION_LEGACY;
  log_debug("virtio mmio version: %s", dev->leagcy ? "legacy" : "modern");

  uint32_t device_id = virtio_mmio_read32(mdev, VIRTIO_MMIO_DEVICE_ID);
  if (device_id != type) {
    log_err("device type mismatch: expected 0x%x, found 0x%x: %s", type,
            device_id, strerror(ENODEV));
    rc = ENODEV;
    goto free_mdev;
  }

  if (dev->leagcy) {
    virtio_mmio_write32(mdev, VIRTIO_MMIO_GUEST_PAGE_SIZE, mdev->pagesize);
  }

  dev->ops.read_device_features = virtio_mmio_read_device_features;
  dev->ops.write_driver_features = virtio_mmio_write_driver_features;
  dev->ops.set_device_status = virtio_mmio_set_device_status;
  dev->ops.reset_device = virtio_mmio_reset_device;
  dev->ops.get_device_status = virtio_mmio_get_device_status;
  dev->ops.read_device_config = virtio_mmio_read_device_config;
  dev->ops.set_queue_addr = virtio_mmio_set_queue_addr;
  dev->ops.select_queue = virtio_mmio_select_queue;
  dev->ops.notify_queue = virtio_mmio_notify_queue;
  dev->ops.enable_queue = virtio_mmio_enable_queue;
  dev->ops.set_queue_reset = virtio_mmio_set_queue_reset;
  dev->ops.get_queue_reset = virtio_mmio_get_queue_reset;
  dev->ops.max_queue_size = virtio_mmio_max_queue_size;
  dev->ops.set_queue_size = virtio_mmio_set_queue_size;
  dev->ops.virtq_callback = virtio_mmio_virtq_callback;

  log_info("virtio mmio device initialized successfully");
  return EOK;

free_mdev:
  free(mdev);
free_vdev:
  virtio_destroy(dev);

  return rc;
}
