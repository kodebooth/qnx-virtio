/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */
#include "virtio_pci.h"
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

int virtio_pci_get_device_status(struct virtio_device *const dev,
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

int virtio_pci_set_device_status(struct virtio_device *const dev,
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

int virtio_pci_add_device_status(struct virtio_device *const dev,
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

  rc = virtio_pci_get_device_status(dev, &current_status);
  if (rc != EOK) {
    goto unlock;
  }

  rc = virtio_pci_set_device_status(dev, current_status | status);
  if (rc != EOK) {
    goto unlock;
  }

unlock:
  pthread_spin_unlock(&dev->lock);
  return rc;
}

int virtio_pci_reset_device(struct virtio_device *dev) {
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

int virtio_pci_select_queue(struct virtio_device *dev, uint16_t index) {
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

int virtio_pci_notify_queue(struct virtio_device *dev, uint16_t index) {
  int rc;
  struct virtio_pci_device *pdev;

  if (dev == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;

  if (pdev == NULL || pdev->common_cfg == NULL || pdev->notify_cfg == NULL) {
    return EINVAL;
  }

  rc = pthread_spin_lock(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_pci_select_queue(dev, index);
  if (rc != EOK) {
    goto unlock;
  }

  // TODO: (#4) queue notification
  ((uint8_t *)pdev->notify_cfg)[pdev->common_cfg->queue_notify_off *
                                pdev->notify_cfg_cap.notify_off_multiplier] =
      index;

unlock:
  pthread_spin_unlock(&dev->lock);
  return rc;
}

int virtio_pci_enable_queue(struct virtio_device *dev, uint16_t index,
                            bool enable) {
  int rc;
  struct virtio_pci_device *pdev;

  if (dev == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;
  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  rc = pthread_spin_lock(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_pci_select_queue(dev, index);
  if (rc != EOK) {
    goto unlock;
  }

  pdev->common_cfg->queue_enable = enable ? 1 : 0;

unlock:
  pthread_spin_unlock(&dev->lock);
  return rc;
}

int virtio_pci_reset_queue(struct virtio_device *dev, uint16_t index) {
  int rc;
  struct virtio_pci_device *pdev;

  if (dev == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;

  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  rc = pthread_spin_lock(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_pci_select_queue(dev, index);
  if (rc != EOK) {
    goto unlock;
  }

  pdev->common_cfg->queue_reset = true;
  while (pdev->common_cfg->queue_reset) {
    /* wait for reset to complete */
  }

unlock:
  pthread_spin_unlock(&dev->lock);
  return rc;
}

int virtio_pci_queue_size(struct virtio_device *dev, uint16_t index,
                          uint16_t *size) {
  int rc;
  struct virtio_pci_device *pdev;

  if (dev == NULL || size == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;
  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  rc = pthread_spin_lock(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_pci_select_queue(dev, index);
  if (rc != EOK) {
    goto unlock;
  }

  *size = pdev->common_cfg->queue_size;

unlock:
  pthread_spin_unlock(&dev->lock);
  return rc;
}

int virtio_pci_set_queue_size(struct virtio_device *dev, uint16_t index,
                              uint16_t size) {
  int rc;
  struct virtio_pci_device *pdev;

  if (dev == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;
  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  rc = pthread_spin_lock(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_pci_select_queue(dev, index);
  if (rc != EOK) {
    goto unlock;
  }

  pdev->common_cfg->queue_size = size;

unlock:
  pthread_spin_unlock(&dev->lock);
  return rc;
}

int virtio_pci_read_device_features(struct virtio_device *dev,
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

int virtio_pci_write_driver_features(struct virtio_device *dev,
                                     const uint32_t *features, size_t len) {
  int rc;
  uint32_t select;
  struct virtio_pci_device *pdev;

  if (dev == NULL || features == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;

  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  rc = pthread_spin_lock(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  for (select = 0; select < len; select++) {
    pdev->common_cfg->driver_feature_select = select;
    pdev->common_cfg->driver_feature = features[select];
  }

  return pthread_spin_unlock(&dev->lock);
}

int virtio_pci_read_device_config(struct virtio_device *dev, void *dst,
                                  size_t len, size_t offset) {
  int rc;
  struct virtio_pci_device *pdev;

  if (dev == NULL || dst == NULL) {
    return EINVAL;
  }

  pdev = dev->priv;

  rc = pthread_spin_lock(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  for (size_t i = 0; i < len; i += sizeof(uint32_t)) {
    ((uint32_t *)dst)[i] = ((uint32_t *)pdev->device_cfg)[offset + i];
  }

  return pthread_spin_unlock(&dev->lock);
}

int virtio_pci_read_msix_vector_count(struct virtio_pci_device *dev,
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

int virtio_pci_create_queue(struct virtio_device *dev, struct virtq *vq,
                            const uint16_t index) {
  uint16_t num;
  int rc;
  struct virtio_pci_device *pdev = dev->priv;

  if (dev == NULL || vq == NULL) {
    return EINVAL;
  }

  rc = virtq_size(vq, &num);
  if (rc != EOK) {
    return rc;
  }

  pdev = dev->priv;
  if (pdev == NULL || pdev->common_cfg == NULL) {
    return EINVAL;
  }

  rc = virtio_pci_reset_queue(dev, index);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_pci_set_queue_size(dev, index, num);
  if (rc != EOK) {
    return rc;
  }

  rc = pthread_spin_lock(&dev->lock);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_pci_select_queue(dev, index);
  if (rc != EOK) {
    goto unlock;
  }

  rc = virtq_desc_paddr(vq, (intptr_t *)&pdev->common_cfg->queue_desc);
  if (rc != EOK) {
    goto unlock;
  }

  rc = virtq_avail_paddr(vq, (intptr_t *)&pdev->common_cfg->queue_driver);
  if (rc != EOK) {
    goto unlock;
  }

  rc = virtq_used_paddr(vq, (intptr_t *)&pdev->common_cfg->queue_device);
  if (rc != EOK) {
    goto unlock;
  }

  // TODO: (#3) add support for MSI-X vectors per queue
  pdev->common_cfg->queue_msix_vector = 0;

unlock:
  pthread_spin_unlock(&dev->lock);
  return rc;
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

int virtio_pci_init(struct virtio_device **dev, uint16_t type, size_t index,
                    uint16_t msix_vector_count) {
  pci_err_t pci_err;
  int rc;
  struct virtio_pci_device *pdev;
  struct virtio_device *vdev;

  rc = virtio_init(dev);
  if (rc != EOK) {
    return rc;
  }

  vdev = *dev;

  pdev = calloc(1, sizeof(struct virtio_pci_device));
  if (pdev == NULL) {
    rc = ENOMEM;
    goto free_vdev;
  }

  vdev->priv = pdev;
  vdev->leagcy = false;

  pdev->bdf = virtio_pci_find(index, type);
  if (pdev->bdf == PCI_BDF_NONE) {
    rc = ENODEV;
    goto free_pdev;
  }

  pdev->pci = pci_device_attach(pdev->bdf, pci_attachFlags_DEFAULT, &pci_err);
  if (pci_err != PCI_ERR_OK) {
    rc = pci_err;
    goto free_pdev;
  }

  rc = virtio_pci_find_caps(pdev);
  if (rc != 0) {
    goto detach_pci;
  }

  rc = virtio_pci_map_bars(pdev);
  if (rc != 0) {
    goto detach_pci;
  }

  pdev->common_cfg =
      pdev->mapped_bars[pdev->common_cfg_cap.bar] + pdev->common_cfg_cap.offset;
  pdev->device_cfg =
      pdev->mapped_bars[pdev->device_cfg_cap.bar] + pdev->device_cfg_cap.offset;
  pdev->notify_cfg = pdev->mapped_bars[pdev->notify_cfg_cap.cap.bar] +
                     pdev->notify_cfg_cap.cap.offset;

  if (msix_vector_count > 0 && !pdev->msix) {
    rc = EINVAL;
    goto detach_pci;
  }

  if (pdev->msix) {
    virtio_pci_read_msix_vector_count(pdev, &pdev->msix_vector_count);
    if (msix_vector_count > pdev->msix_vector_count) {
      rc = EINVAL;
      goto detach_pci;
    }
    pdev->msix_vector_count = msix_vector_count;

    pci_err = cap_msix_set_nirq(pdev->pci, pdev->msix, pdev->msix_vector_count);
    if (pci_err != PCI_ERR_OK) {
      rc = pci_err;
      goto detach_pci;
    }
    pci_err = cap_msix_set_irq_entry(pdev->pci, pdev->msix, 0, 0);
    if (pci_err != PCI_ERR_OK) {
      rc = pci_err;
      goto detach_pci;
    }
    pci_err = pci_device_cfg_cap_enable(pdev->pci, pci_reqType_e_MANDATORY,
                                        pdev->msix);
    if (pci_err != PCI_ERR_OK) {
      rc = pci_err;
      goto detach_pci;
    }
    int irqcount = 1;
    pci_err = pci_device_read_irq(pdev->pci, &irqcount, &vdev->irq);
    if (pci_err != PCI_ERR_OK) {
      rc = pci_err;
      goto detach_pci;
    }

    pci_err = cap_msix_unmask_irq_entry(pdev->pci, pdev->msix, 0);
    if (pci_err != PCI_ERR_OK) {
      rc = pci_err;
      goto detach_pci;
    }
  }

  vdev->ops.read_device_features = virtio_pci_read_device_features;
  vdev->ops.write_driver_features = virtio_pci_write_driver_features;
  vdev->ops.set_device_status = virtio_pci_set_device_status;
  vdev->ops.add_device_status = virtio_pci_add_device_status;
  vdev->ops.reset_device = virtio_pci_reset_device;
  vdev->ops.get_device_status = virtio_pci_get_device_status;
  vdev->ops.read_device_config = virtio_pci_read_device_config;
  vdev->ops.create_queue = virtio_pci_create_queue;
  vdev->ops.select_queue = virtio_pci_select_queue;
  vdev->ops.notify_queue = virtio_pci_notify_queue;
  vdev->ops.enable_queue = virtio_pci_enable_queue;
  vdev->ops.reset_queue = virtio_pci_reset_queue;
  vdev->ops.max_queue_size = virtio_pci_queue_size;
  vdev->ops.get_queue_size = virtio_pci_queue_size;
  vdev->ops.set_queue_size = virtio_pci_set_queue_size;
  vdev->ops.virtq_callback = virtio_pci_virtq_callback;

  return EOK;

detach_pci:
  pci_device_detach(pdev->pci);
free_pdev:
  free(pdev);
free_vdev:
  virtio_destroy(vdev);

  return rc;
}
