/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */
#include "virtio_pci.h"
#include "logging.h"
#include "virtio.h"
#include "virtq.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define VIRTIO_PCI_VID ((uint16_t)0x1AF4)
#define VIRTIO_PCI_DID(type) ((uint16_t)(0x1040 + (uint16_t)(type)))

#define VIRTIO_PCI_CAP_COMMON_CFG 1
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
#define VIRTIO_PCI_CAP_ISR_CFG 3
#define VIRTIO_PCI_CAP_DEVICE_CFG 4
#define VIRTIO_PCI_CAP_PCI_CFG 5
#define VIRTIO_PCI_CAP_SHARED_MEMORY_CFG 8
#define VIRTIO_PCI_CAP_VENDOR_CFG 9

struct virtio_pci_cap {
  uint8_t cap_vndr;
  uint8_t cap_next;
  uint8_t cap_len;
  uint8_t cfg_type;
  uint8_t bar;
  uint8_t id;
  uint8_t padding[2];
  uint32_t offset;
  uint32_t length;
};

struct virtio_pci_notify_cap {
  struct virtio_pci_cap cap;
  uint32_t notify_off_multiplier;
};

struct virtio_pci_common_cfg {
  uint32_t device_feature_select;
  uint32_t device_feature;
  uint32_t driver_feature_select;
  uint32_t driver_feature;
  uint16_t config_msix_vector;
  uint16_t num_queues;
  uint8_t device_status;
  uint8_t config_generation;

  uint16_t queue_select;
  uint16_t queue_size;
  uint16_t queue_msix_vector;
  uint16_t queue_enable;
  uint16_t queue_notify_off;
  uint64_t queue_desc;
  uint64_t queue_driver; /* available */
  uint64_t queue_device; /* used */
  uint16_t queue_notif_config_data;
  uint16_t queue_reset;

  uint16_t admin_queue_index;
  uint16_t admin_queue_num;
};

struct virtio_pci_device {
  pci_bdf_t bdf;
  pci_devhdl_t pci;

  pci_cap_t msix;
  uint16_t msix_vector_count;

  struct virtio_pci_cap common_cfg_cap;
  struct virtio_pci_notify_cap notify_cfg_cap;
  struct virtio_pci_cap isr_cfg_cap;
  struct virtio_pci_cap device_cfg_cap;
  struct virtio_pci_cap pci_cfg_cap;

  struct virtio_pci_common_cfg *common_cfg;
  void *notify_cfg;
  void *device_cfg;

  pci_ba_t bars[6];
  void *mapped_bars[6];

  int bar_count;
};

static int virtio_pci_virtq_callback(struct virtio_device *dev,
                                     struct virtq *vq,
                                     int (*callback)(struct virtio_device *dev,
                                                     struct virtq *vq)) {
  int rc;

  if (dev == NULL || vq == NULL || callback == NULL) {
    return EINVAL;
  }

  do {
    rc = callback(dev, vq);
  } while (rc == EOK);

  return rc;
}

static inline void pci_bdf_format(pci_bdf_t bdf, char *buf, size_t size) {
  snprintf(buf, size, "%02x:%02x.%x", PCI_BUS(bdf), PCI_DEV(bdf),
           PCI_FUNC(bdf));
}

static inline pci_bdf_t virtio_pci_find(const unsigned index,
                                        const uint16_t type) {
  return pci_device_find(index, VIRTIO_PCI_VID, VIRTIO_PCI_DID(type),
                         PCI_CCODE_ANY);
}

static inline int
virtio_pci_read_pci_notify_cap(pci_cap_t cap,
                               struct virtio_pci_notify_cap *virtio_cap) {
  return cap_vend_read_bytes(cap,
                             sizeof(struct virtio_pci_notify_cap) -
                                 offsetof(struct virtio_pci_notify_cap, cap) -
                                 offsetof(struct virtio_pci_cap, cfg_type),
                             (uint8_t *)&virtio_cap->cap.cfg_type);
}
static inline int virtio_pci_read_pci_cap(pci_cap_t cap,
                                          struct virtio_pci_cap *virtio_cap) {
  return cap_vend_read_bytes(cap,
                             sizeof(struct virtio_pci_cap) -
                                 offsetof(struct virtio_pci_cap, cfg_type),
                             (uint8_t *)&virtio_cap->cfg_type);
}

/**
 * @brief Get the device status from a VirtIO device
 *
 * @param[in] dev VirtIO device
 * @param[out] status Pointer to store the device status
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_pci_get_device_status(struct virtio_device *const dev,
                                        uint8_t *status) {
  struct virtio_pci_device *pdev;

  if (dev == NULL || status == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;
  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  *status = pdev->common_cfg->device_status;
  return EOK;
}

/**
 * @brief Set the device status of a VirtIO device
 *
 * @param[in] dev VirtIO device
 * @param[in] status Device status to set
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_pci_set_device_status(struct virtio_device *const dev,
                                        uint8_t status) {
  struct virtio_pci_device *pdev;

  if (dev == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;

  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  pdev->common_cfg->device_status = status;

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
static int virtio_pci_reset_device(struct virtio_device *dev) {
  struct virtio_pci_device *pdev;

  if (dev == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;
  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  pdev->common_cfg->device_status = 0;

  return EOK;
}

/**
 * @brief Select a virtqueue for subsequent operations
 *
 * @param[in] dev VirtIO device
 * @param[in] index Queue index to select
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_pci_select_queue(struct virtio_device *dev, uint16_t index) {
  struct virtio_pci_device *pdev;

  if (dev == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;
  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  pdev->common_cfg->queue_select = index;

  return EOK;
}

/**
 * @brief Notify the device about available buffers in a queue
 *
 * @param[in] dev VirtIO device
 * @param[in] index Queue index to notify
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_pci_notify_queue(struct virtio_device *dev, uint16_t index) {
  struct virtio_pci_device *pdev;

  if (dev == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;

  if (pdev == NULL || pdev->common_cfg == NULL || pdev->notify_cfg == NULL) {
    return EINVAL;
  }

  // TODO: (#4) queue notification
  ((uint8_t *)pdev->notify_cfg)[pdev->common_cfg->queue_notify_off *
                                pdev->notify_cfg_cap.notify_off_multiplier] =
      index;

  return EOK;
}

/**
 * @brief Enable or disable a virtqueue
 *
 * @param[in] dev VirtIO device
 * @param[in] enable true to enable, false to disable
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_pci_enable_queue(struct virtio_device *dev, bool enable) {
  struct virtio_pci_device *pdev;

  if (dev == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;
  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  // TODO: (#3) add support for MSI-X vectors per queue
  pdev->common_cfg->queue_msix_vector = 0;
  pdev->common_cfg->queue_enable = enable ? 1 : 0;

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
static int virtio_pci_set_queue_reset(struct virtio_device *dev) {
  struct virtio_pci_device *pdev;

  if (dev == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;

  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  pdev->common_cfg->queue_reset = true;

  return EOK;
}

static int virtio_pci_get_queue_reset(struct virtio_device *dev,
                                      uint32_t *value) {
  struct virtio_pci_device *pdev;

  if (dev == NULL || value == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;

  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  *value = pdev->common_cfg->queue_reset;

  return EOK;
}

/**
 * @brief Get the size of a virtqueue
 *
 * @param[in] dev VirtIO device
 * @param[out] size Pointer to store the queue size
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_pci_queue_size(struct virtio_device *dev, uint16_t *size) {
  struct virtio_pci_device *pdev;

  if (dev == NULL || size == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;
  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  *size = pdev->common_cfg->queue_size;

  return EOK;
}

/**
 * @brief Set the size of a virtqueue
 *
 * @param[in] dev VirtIO device
 * @param[in] size Queue size to set
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_pci_set_queue_size(struct virtio_device *dev, uint16_t size) {
  struct virtio_pci_device *pdev;

  if (dev == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;
  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  pdev->common_cfg->queue_size = size;

  return EOK;
}

/**
 * @brief Read device feature bits
 *
 * @param[in] dev VirtIO device
 * @param[out] features Array to store feature bits
 * @param[in] len Number of 32-bit feature words to read
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_pci_read_device_features(struct virtio_device *dev,
                                           uint32_t *features, size_t len) {
  uint32_t select;
  struct virtio_pci_device *pdev;

  if (dev == NULL || features == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;
  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  for (select = 0; select < len; select++) {
    pdev->common_cfg->device_feature_select = select;
    features[select] = pdev->common_cfg->device_feature;
  }

  return EOK;
}

/**
 * @brief Write driver feature bits
 *
 * @param[in] dev VirtIO device
 * @param[in] features Array of feature bits to write
 * @param[in] len Number of 32-bit feature words to write
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_pci_write_driver_features(struct virtio_device *dev,
                                            const uint32_t *features,
                                            size_t len) {
  uint32_t select;
  struct virtio_pci_device *pdev;

  if (dev == NULL || features == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;

  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  for (select = 0; select < len; select++) {
    pdev->common_cfg->driver_feature_select = select;
    pdev->common_cfg->driver_feature = features[select];
  }

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
static int virtio_pci_read_device_config(struct virtio_device *dev, void *dst,
                                         size_t len, size_t offset) {
  struct virtio_pci_device *pdev;

  if (dev == NULL || dst == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;

  for (size_t i = 0; i < len; i += sizeof(uint32_t)) {
    ((uint32_t *)dst)[i] = ((uint32_t *)pdev->device_cfg)[offset + i];
  }

  return EOK;
}

/**
 * @brief Read the MSI-X vector count for a PCI device
 *
 * @param[in] dev VirtIO PCI device
 * @param[out] count Pointer to store the vector count
 * @return EOK on success, EINVAL if parameters are invalid
 */
static int virtio_pci_read_msix_vector_count(struct virtio_pci_device *dev,
                                             uint16_t *count) {
  if (dev == NULL || count == NULL) {
    return EINVAL;
  }

  if (dev->msix) {
    *count = cap_msix_get_nirq(dev->msix);
  } else {
    *count = 0;
  }

  return EOK;
}

/**
 * @brief Set the physical address of a virtqueue for PCI device
 *
 * Configures the descriptor, available, and used ring addresses in the
 * PCI common configuration structure for the specified virtqueue.
 *
 * @param dev VirtIO PCI device instance
 * @param vq Virtual queue to configure
 * @return EOK on success, error code on failure
 */
static int virtio_pci_set_queue_addr(struct virtio_device *const dev,
                                     struct virtq *vq) {
  int rc;
  struct virtio_pci_device *pdev = dev->priv;

  if ((rc = virtq_desc_paddr(vq, (intptr_t *)&pdev->common_cfg->queue_desc)) !=
      EOK) {
    return rc;
  }
  if ((rc = virtq_avail_paddr(
           vq, (intptr_t *)&pdev->common_cfg->queue_driver)) != EOK) {
    return rc;
  }
  if ((rc = virtq_used_paddr(
           vq, (intptr_t *)&pdev->common_cfg->queue_device)) != EOK) {
    return rc;
  }
  return EOK;
}

static int virtio_pci_find_caps(struct virtio_pci_device *dev) {
  int index;
  pci_err_t r;
  pci_cap_t cap;
  uint8_t cfg_type;

  index = pci_device_find_capid(dev->bdf, CAPID_VEND);

  while (index != -1) {
    cap = 0;
    r = pci_device_read_cap(dev->bdf, &cap, index);
    if (r != PCI_ERR_OK) {
      return r;
    }

    r = cap_vend_read_bytes(cap, sizeof(cfg_type), &cfg_type);
    if (r != PCI_ERR_OK) {
      return r;
    }

    switch (cfg_type) {
    case VIRTIO_PCI_CAP_COMMON_CFG: {
      r = virtio_pci_read_pci_cap(cap, &dev->common_cfg_cap);
    } break;
    case VIRTIO_PCI_CAP_NOTIFY_CFG: {
      r = virtio_pci_read_pci_notify_cap(cap, &dev->notify_cfg_cap);
    } break;
    case VIRTIO_PCI_CAP_ISR_CFG: {
      r = virtio_pci_read_pci_cap(cap, &dev->isr_cfg_cap);
    } break;
    case VIRTIO_PCI_CAP_DEVICE_CFG: {
      r = virtio_pci_read_pci_cap(cap, &dev->device_cfg_cap);
    } break;
    case VIRTIO_PCI_CAP_PCI_CFG: {
      r = virtio_pci_read_pci_cap(cap, &dev->pci_cfg_cap);
    } break;
    default:
      break;
    }

    if (r != PCI_ERR_OK) {
      return r;
    }

    index = pci_device_find_capid_next(dev->bdf, CAPID_VEND, index);
  }

  index = pci_device_find_capid(dev->bdf, CAPID_MSIX);
  if (index != -1) {
    r = pci_device_read_cap(dev->bdf, &dev->msix, index);
    if (r != PCI_ERR_OK) {
      return r;
    }
  }

  return EOK;
}

static int virtio_pci_map_bars(struct virtio_pci_device *dev) {
  pci_err_t pci_err;

  dev->bar_count = sizeof(dev->bars) / sizeof(dev->bars[0]);
  pci_err = pci_device_read_ba(dev->pci, &dev->bar_count, dev->bars,
                               pci_reqType_e_UNSPECIFIED);
  if (pci_err != PCI_ERR_OK) {
    return pci_err;
  }

  for (int idx = 0; idx < dev->bar_count; idx++) {
    if (dev->bars[idx].size == 0) {
      continue;
    }

    dev->mapped_bars[dev->bars[idx].bar_num] =
        mmap64(0, dev->bars[idx].size, PROT_READ | PROT_WRITE | PROT_NOCACHE,
               MAP_SHARED | MAP_PHYS, NOFD, dev->bars[idx].addr);
  }

  return EOK;
}

int virtio_pci_init(struct virtio_device *dev, uint16_t type, size_t index,
                    uint16_t msix_vector_count) {
  pci_err_t pci_err;
  int rc;
  struct virtio_pci_device *pdev;

  pdev = calloc(1, sizeof(struct virtio_pci_device));
  if (pdev == NULL) {
    rc = ENOMEM;
    goto free_vdev;
  }

  dev->priv = pdev;
  dev->leagcy = false;

  pdev->bdf = virtio_pci_find(index, type);
  if (pdev->bdf == PCI_BDF_NONE) {
    log_err("pci device not found: %s", strerror(ENODEV));
    rc = ENODEV;
    goto free_pdev;
  }

  char bdf_str[16];
  pci_bdf_format(pdev->bdf, bdf_str, sizeof(bdf_str));
  log_debug("found pci device at %s", bdf_str);

  pdev->pci = pci_device_attach(pdev->bdf, pci_attachFlags_DEFAULT, &pci_err);
  if (pci_err != PCI_ERR_OK) {
    log_err("failed to attach pci device: %s", strerror(pci_err));
    rc = pci_err;
    goto free_pdev;
  }

  rc = virtio_pci_find_caps(pdev);
  if (rc != 0) {
    log_err("failed to find pci capabilities: %s", strerror(rc));
    goto detach_pci;
  }

  rc = virtio_pci_map_bars(pdev);
  if (rc != 0) {
    log_err("failed to map pci bars: %s", strerror(rc));
    goto detach_pci;
  }

  pdev->common_cfg =
      pdev->mapped_bars[pdev->common_cfg_cap.bar] + pdev->common_cfg_cap.offset;
  pdev->device_cfg =
      pdev->mapped_bars[pdev->device_cfg_cap.bar] + pdev->device_cfg_cap.offset;
  pdev->notify_cfg = pdev->mapped_bars[pdev->notify_cfg_cap.cap.bar] +
                     pdev->notify_cfg_cap.cap.offset;

  if (msix_vector_count > 0 && !pdev->msix) {
    log_err("msix requested but not available: %s", strerror(EINVAL));
    rc = EINVAL;
    goto detach_pci;
  }

  if (pdev->msix) {
    virtio_pci_read_msix_vector_count(pdev, &pdev->msix_vector_count);
    if (msix_vector_count > pdev->msix_vector_count) {
      log_err("requested msix vectors (%u) exceeds available (%u): %s",
              msix_vector_count, pdev->msix_vector_count, strerror(EINVAL));
      rc = EINVAL;
      goto detach_pci;
    }
    pdev->msix_vector_count = msix_vector_count;
    log_debug("configuring %u msix vectors", msix_vector_count);

    pci_err = cap_msix_set_nirq(pdev->pci, pdev->msix, pdev->msix_vector_count);
    if (pci_err != PCI_ERR_OK) {
      log_err("failed to set msix nirq: %s", strerror(pci_err));
      rc = pci_err;
      goto detach_pci;
    }
    pci_err = cap_msix_set_irq_entry(pdev->pci, pdev->msix, 0, 0);
    if (pci_err != PCI_ERR_OK) {
      log_err("failed to set msix irq entry: %s", strerror(pci_err));
      rc = pci_err;
      goto detach_pci;
    }
    pci_err = pci_device_cfg_cap_enable(pdev->pci, pci_reqType_e_MANDATORY,
                                        pdev->msix);
    if (pci_err != PCI_ERR_OK) {
      log_err("failed to enable msix capability: %s", strerror(pci_err));
      rc = pci_err;
      goto detach_pci;
    }
    int irqcount = 1;
    pci_err = pci_device_read_irq(pdev->pci, &irqcount, &dev->irq);
    if (pci_err != PCI_ERR_OK) {
      log_err("failed to read irq: %s", strerror(pci_err));
      rc = pci_err;
      goto detach_pci;
    }

    pci_err = cap_msix_unmask_irq_entry(pdev->pci, pdev->msix, 0);
    if (pci_err != PCI_ERR_OK) {
      log_err("failed to unmask msix irq: %s", strerror(pci_err));
      rc = pci_err;
      goto detach_pci;
    }
  }

  dev->ops.read_device_features = virtio_pci_read_device_features;
  dev->ops.write_driver_features = virtio_pci_write_driver_features;
  dev->ops.set_device_status = virtio_pci_set_device_status;
  dev->ops.reset_device = virtio_pci_reset_device;
  dev->ops.get_device_status = virtio_pci_get_device_status;
  dev->ops.read_device_config = virtio_pci_read_device_config;
  dev->ops.set_queue_addr = virtio_pci_set_queue_addr;
  dev->ops.select_queue = virtio_pci_select_queue;
  dev->ops.notify_queue = virtio_pci_notify_queue;
  dev->ops.enable_queue = virtio_pci_enable_queue;
  dev->ops.set_queue_reset = virtio_pci_set_queue_reset;
  dev->ops.get_queue_reset = virtio_pci_get_queue_reset;
  dev->ops.max_queue_size = virtio_pci_queue_size;
  dev->ops.set_queue_size = virtio_pci_set_queue_size;
  dev->ops.virtq_callback = virtio_pci_virtq_callback;

  log_info("virtio pci device initialized successfully");
  return EOK;

detach_pci:
  pci_device_detach(pdev->pci);
free_pdev:
  free(pdev);
free_vdev:
  virtio_destroy(dev);

  return rc;
}
