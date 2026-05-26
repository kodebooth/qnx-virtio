/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */
#include "gpio.h"
#include "errno.h"
#include "logging.h"
#include "virtio.h"
#include "virtio_mmio.h"
#include "virtio_pci.h"
#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define VIRTIO_DT_GPIO 41

/* Possible values of the status field */
#define VIRTIO_GPIO_STATUS_OK 0x0
#define VIRTIO_GPIO_STATUS_ERR 0x1

/* GPIO message types */
#define VIRTIO_GPIO_MSG_GET_LINE_NAMES 0x0001
#define VIRTIO_GPIO_MSG_GET_DIRECTION 0x0002
#define VIRTIO_GPIO_MSG_SET_DIRECTION 0x0003
#define VIRTIO_GPIO_MSG_GET_VALUE 0x0004
#define VIRTIO_GPIO_MSG_SET_VALUE 0x0005
#define VIRTIO_GPIO_MSG_SET_IRQ_TYPE 0x0006

/* GPIO Direction types */
#define VIRTIO_GPIO_DIRECTION_NONE 0x00
#define VIRTIO_GPIO_DIRECTION_OUT 0x01
#define VIRTIO_GPIO_DIRECTION_IN 0x02

/* GPIO interrupt types */
#define VIRTIO_GPIO_IRQ_TYPE_NONE 0x00
#define VIRTIO_GPIO_IRQ_TYPE_EDGE_RISING 0x01
#define VIRTIO_GPIO_IRQ_TYPE_EDGE_FALLING 0x02
#define VIRTIO_GPIO_IRQ_TYPE_EDGE_BOTH 0x03
#define VIRTIO_GPIO_IRQ_TYPE_LEVEL_HIGH 0x04
#define VIRTIO_GPIO_IRQ_TYPE_LEVEL_LOW 0x08

struct virtio_gpio_config {
  uint16_t ngpio;
  uint8_t padding[2];
  uint32_t gpio_names_size;
};

struct virtio_gpio_response {
  uint8_t status;
  uint8_t value;
};

struct virtio_gpio_response_n {
  uint8_t status;
  uint8_t value[];
};

struct virtio_gpio_request {
  uint16_t type;
  uint16_t gpio;
  uint32_t value;
};

struct virtio_gpio_device {
  struct virtio_device *dev;

  size_t idx;

  struct virtio_gpio_config config;

  /* Virtqueues */
  struct virtq *requestq;

  void *priv;
};

/**
 * @brief Callback function for GPIO request completion
 *
 * @param[in] request GPIO request structure
 * @param[in] response GPIO response structure
 * @param[in] context User context data
 */
static void
virtio_gpio_callback(const struct virtio_gpio_request *const request,
                     const struct virtio_gpio_response *const response,
                     void *const context);

struct virtio_gpio_context_header {
  void (*callback)(const struct virtio_gpio_request *const request,
                   const struct virtio_gpio_response *const response,
                   void *const context);
  sem_t sem;
  uint8_t status;
};

struct virtio_gpio_context {
  struct virtio_gpio_context_header header;
  uint8_t value;
};

struct virtio_gpio_context_n {
  struct virtio_gpio_context_header header;
  uint32_t size;
  uint8_t value[];
};

/**
 * @brief Initialize GPIO context for synchronous operations
 *
 * @param[out] context GPIO context structure to initialize
 * @return 0 on success, error code from sem_init() on failure
 */
static inline int
virtio_gpio_context_init(struct virtio_gpio_context *context) {
  context->header.callback = virtio_gpio_callback;
  context->header.status = 0;

  return sem_init(&context->header.sem, 0, 0);
}

/**
 * @brief Destroy GPIO context and release resources
 *
 * @param[in] context GPIO context structure to destroy
 * @return 0 on success, error code from sem_destroy() on failure
 */
static inline int
virtio_gpio_context_deinit(struct virtio_gpio_context *context) {
  return sem_destroy(&context->header.sem);
}

/**
 * @brief Wait for GPIO operation completion
 *
 * @param[in] context GPIO context structure
 * @return 0 on success, error code on failure
 */
static inline int
virtio_gpio_context_wait(struct virtio_gpio_context *context) {
  int rc;

  do {
    rc = sem_wait(&context->header.sem);
  } while (rc == EINTR);

  return rc;
}

static void
virtio_gpio_callback(const struct virtio_gpio_request *const request,
                     const struct virtio_gpio_response *const response,
                     void *const context) {
  struct virtio_gpio_context *context_ = context;

  struct virtio_gpio_context_n *context_n;
  struct virtio_gpio_response_n *response_n;

  context_->header.status = response->status;

  switch (request->type) {
  case VIRTIO_GPIO_MSG_GET_LINE_NAMES:
    context_n = (struct virtio_gpio_context_n *)context;
    response_n = (struct virtio_gpio_response_n *)response;
    memcpy(context_n->value, (void *)response_n->value, context_n->size);
    break;
  default:
    context_->value = response->value;
    break;
  }

  sem_post(&context_->header.sem);
}

/**
 * @brief Calculate response buffer size for a GPIO message type
 *
 * @param[in] dev GPIO device structure
 * @param[in] type GPIO message type
 * @return Size in bytes required for response buffer
 */
static size_t virtio_gpio_response_size(struct virtio_gpio_device *dev,
                                        uint16_t type) {
  switch (type) {
  case VIRTIO_GPIO_MSG_GET_LINE_NAMES:
    return sizeof(struct virtio_gpio_response_n) + dev->config.gpio_names_size;
  default:
    return sizeof(struct virtio_gpio_response);
  }
}

/**
 * @brief Virtqueue interrupt callback for GPIO device
 *
 * @param[in] dev VirtIO device structure
 * @param[in] vq Virtqueue that triggered the interrupt
 * @return EOK on success, error code on failure
 */
static int virtq_callback(struct virtio_device *dev, struct virtq *vq) {
  int rc;
  uint16_t idxs[2];
  size_t count = 2;
  struct virtio_gpio_request *request;
  struct virtio_gpio_response *response;
  struct virtio_gpio_context_header *context;

  (void)dev;

  count = 2;
  rc = virtq_get_desc_chain(vq, &count, idxs);
  while (rc == 0) {
    virtq_get_desc_vaddr(vq, idxs[0], (uint64_t *)&request);
    virtq_get_desc_vaddr(vq, idxs[1], (uint64_t *)&response);

    virtq_get_desc_context(vq, idxs[0], (void **)&context);

    context->callback(request, response, context);

    virtq_free_desc(vq, idxs[0]);
    virtq_free_desc(vq, idxs[1]);

    count = 2;
    rc = virtq_get_desc_chain(vq, &count, idxs);
  }

  return rc;
}

/**
 * @brief Send a GPIO request to the device
 *
 * @param[in] dev GPIO device structure
 * @param[in] type GPIO message type
 * @param[in] gpio GPIO pin number
 * @param[in] value Value for the request
 * @param[in] context User context data
 * @return EOK on success, error code on failure
 */
static int virtio_gpio_request(struct virtio_gpio_device *dev, uint16_t type,
                               uint16_t gpio, uint16_t value, void *context) {
  uint16_t idxs[2];
  struct virtio_gpio_request *request;
  int rc;

  rc = virtq_alloc_desc(dev->requestq, sizeof(struct virtio_gpio_request),
                        false, &idxs[0]);
  if (rc != EOK) {
    return rc;
  }

  rc = virtq_alloc_desc(dev->requestq, virtio_gpio_response_size(dev, type),
                        true, &idxs[1]);
  if (rc != EOK) {
    goto free_desc0;
  }

  rc = virtq_get_desc_vaddr(dev->requestq, idxs[0], (uint64_t *)&request);
  if (rc != EOK) {
    goto free_desc1;
  }

  request->type = type;
  request->gpio = gpio;
  request->value = value;

  virtq_set_desc_context(dev->requestq, idxs[0], context);

  virtq_put_desc_chain(dev->requestq, 2, idxs);
  virtio_notify_queue(dev->dev, 0);

  return 0;

free_desc1:
  virtq_free_desc(dev->requestq, idxs[1]);
free_desc0:
  virtq_free_desc(dev->requestq, idxs[0]);
  return rc;
}

int virtio_gpio_get_chip_index(struct virtio_gpio_device *dev, size_t *index) {
  if (dev == NULL || index == NULL) {
    return EINVAL;
  }

  *index = dev->idx;

  return EOK;
}

int virtio_gpio_num_gpios(struct virtio_gpio_device *dev, uint16_t *ngpio) {
  if (dev == NULL || ngpio == NULL) {
    return EINVAL;
  }

  *ngpio = dev->config.ngpio;

  return 0;
}

int virtio_gpio_names_size(struct virtio_gpio_device *dev, uint32_t *size) {
  if (dev == NULL || size == NULL) {
    return EINVAL;
  }

  *size = dev->config.gpio_names_size;

  return 0;
}

int virtio_gpio_set_value(struct virtio_gpio_device *dev, uint16_t gpio,
                          uint8_t value) {
  int rc;
  struct virtio_gpio_context context;
  const uint16_t type = VIRTIO_GPIO_MSG_SET_VALUE;

  rc = virtio_gpio_context_init(&context);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_gpio_request(dev, type, gpio, value, &context);
  if (rc != EOK) {
    goto deinit;
  }

  rc = virtio_gpio_context_wait(&context);
  if (rc != EOK) {
    goto deinit;
  }

  rc = context.header.status == VIRTIO_GPIO_STATUS_OK ? EOK : EIO;

deinit:
  virtio_gpio_context_deinit(&context);

  return rc;
}

int virtio_gpio_get_value(struct virtio_gpio_device *dev, uint16_t gpio,
                          uint8_t *value) {
  int rc;
  struct virtio_gpio_context context;
  const uint16_t type = VIRTIO_GPIO_MSG_GET_VALUE;

  rc = virtio_gpio_context_init(&context);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_gpio_request(dev, type, gpio, 0, &context);
  if (rc != EOK) {
    goto deinit;
  }

  rc = virtio_gpio_context_wait(&context);
  if (rc != EOK) {
    goto deinit;
  }

  *value = context.value;
  rc = context.header.status == VIRTIO_GPIO_STATUS_OK ? EOK : EIO;

deinit:
  virtio_gpio_context_deinit(&context);

  return rc;
}

int virtio_gpio_get_direction(struct virtio_gpio_device *dev, uint16_t gpio,
                              uint8_t *direction) {
  int rc;
  struct virtio_gpio_context context;
  const uint16_t type = VIRTIO_GPIO_MSG_GET_DIRECTION;

  rc = virtio_gpio_context_init(&context);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_gpio_request(dev, type, gpio, 0, &context);
  if (rc != EOK) {
    goto deinit;
  }

  rc = virtio_gpio_context_wait(&context);
  if (rc != EOK) {
    goto deinit;
  }

  *direction = context.value;
  rc = context.header.status == VIRTIO_GPIO_STATUS_OK ? EOK : EIO;

deinit:
  virtio_gpio_context_deinit(&context);

  return rc;
}

int virtio_gpio_set_direction(struct virtio_gpio_device *dev, uint16_t gpio,
                              uint8_t direction) {
  int rc;
  struct virtio_gpio_context context;
  const uint16_t type = VIRTIO_GPIO_MSG_SET_DIRECTION;

  rc = virtio_gpio_context_init(&context);
  if (rc != EOK) {
    return rc;
  }

  rc = virtio_gpio_request(dev, type, gpio, direction, &context);
  if (rc != EOK) {
    goto deinit;
  }

  rc = virtio_gpio_context_wait(&context);
  if (rc != EOK) {
    goto deinit;
  }

  rc = context.header.status == VIRTIO_GPIO_STATUS_OK ? EOK : EIO;

deinit:
  virtio_gpio_context_deinit(&context);

  return rc;
}

int virtio_gpio_init(size_t idx, uint64_t mem, uint32_t irq, uint16_t qsize,
                     struct virtio_gpio_device **dev) {
  struct virtio_device *vdev;
  struct virtio_gpio_device *pdev;
  int rc;
  const uint16_t msix_vector_count = 1;
  const uint16_t qindex = 0;

  if (dev == NULL) {
    return EINVAL;
  }

  log_debug("initializing virtio gpio device");

  pdev = calloc(1, sizeof(struct virtio_gpio_device));
  if (pdev == NULL) {
    rc = errno;
    log_err("failed to allocate gpio device structure: %s", strerror(rc));
    return rc;
  }

  pdev->idx = idx;

  rc = virtio_init(&vdev);
  if (rc != EOK) {
    return rc;
  }

  rc = mem ? virtio_mmio_init(vdev, mem, VIRTIO_DT_GPIO, irq)
           : virtio_pci_init(vdev, VIRTIO_DT_GPIO, idx, msix_vector_count);
  if (rc != EOK) {
    log_err("failed to initialize virtio device: %s", strerror(rc));
    goto free_vdev;
  }

  virtio_reset_device(vdev);
  virtio_add_device_status(vdev, VIRTIO_DEVICE_STATUS_ACKNOWLEDGE);
  virtio_add_device_status(vdev, VIRTIO_DEVICE_STATUS_DRIVER);

  virtio_read_device_config(vdev, &pdev->config, sizeof(pdev->config), 0);
  log_info("gpio device has %u lines", pdev->config.ngpio);

  virtio_add_device_status(vdev, VIRTIO_DEVICE_STATUS_FEATURES_OK);

  rc = virtio_create_queue(vdev, qindex, qsize, vdev->irq, virtq_callback,
                           &pdev->requestq);
  if (rc != EOK) {
    log_err("failed to create request queue: %s", strerror(rc));
    goto free_vdev;
  }

  virtio_enable_queue(vdev, qindex, true);
  virtio_add_device_status(vdev, VIRTIO_DEVICE_STATUS_DRIVER_OK);

  pdev->dev = vdev;
  *dev = pdev;

  log_info("virtio gpio device initialized successfully");
  return EOK;

free_vdev:
  virtio_destroy(vdev);

  *dev = NULL;

  return rc;
}
