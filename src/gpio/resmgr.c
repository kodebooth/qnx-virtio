/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */

#include "uapi/gpio.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

struct gpio_iofunc_attr;
#define IOFUNC_ATTR_T struct gpio_iofunc_attr

/* clang-format off */
// iofun.h must come before resmgr.h and both must come before all other sys includes
// to correctly define IOFUNC_ATTR_T and RESMGR_OCB_ATTR
#include <sys/iofunc.h>
#include <sys/resmgr.h>
/* clang-format on */

#include <sys/dispatch.h>
#include <sys/iomsg.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "errno.h"
#include "gpio.h"
#include "logging.h"
#include "resmgr.h"

struct gpio_iofunc_attr {
  /* must be the first field */
  iofunc_attr_t attr;

  struct virtio_gpio_device *dev;
  size_t index;
  uint8_t value;
};

struct gpio_resmgr {
  resmgr_attr_t resmgr_attr;
  resmgr_connect_funcs_t connect_funcs;
  resmgr_io_funcs_t io_funcs;
  struct gpio_iofunc_attr *attr;
  int *ids;
};

/**
 * @brief Handle write requests to GPIO device
 *
 * @param[in] ctp Resource manager context
 * @param[in] msg Write message structure
 * @param[in] ocb Open control block
 * @return EOK on success, error code on failure
 */
static int resmgr_write(resmgr_context_t *ctp, io_write_t *msg,
                        RESMGR_OCB_T *ocb) {
  int rc;
  uint8_t value;

  rc = iofunc_write_verify(ctp, msg, ocb, NULL);
  if (rc != EOK) {
    return rc;
  }

  if (_IO_WRITE_GET_NBYTES(msg) != 1) {
    return EINVAL;
  }

  rc = resmgr_msgget(ctp, &value, 1, sizeof(msg->i));
  if (rc == -1) {
    return errno;
  }

  value = value == '0' ? 0 : 1;

  rc = virtio_gpio_set_value(ocb->attr->dev, ocb->attr->index, value);
  if (rc == EOK) {
    _IO_SET_WRITE_NBYTES(ctp, 1);
  }

  return rc;
}

/**
 * @brief Handle read requests from GPIO device
 *
 * @param[in] ctp Resource manager context
 * @param[in] msg Read message structure
 * @param[in] ocb Open control block
 * @return EOK on success, error code on failure
 */
static int resmgr_read(resmgr_context_t *ctp, io_read_t *msg,
                       RESMGR_OCB_T *ocb) {
  int rc;
  size_t nleft;
  size_t nbytes;
  int nparts;

  rc = iofunc_read_verify(ctp, msg, ocb, NULL);
  if (rc != EOK) {
    return rc;
  }

  nleft = ocb->attr->attr.nbytes - ocb->offset;
  nbytes = min(_IO_READ_GET_NBYTES(msg), nleft);

  if (nbytes > 0) {
    rc = virtio_gpio_get_value(ocb->attr->dev, ocb->attr->index,
                               &ocb->attr->value);
    if (rc != EOK) {
      return rc;
    }

    ocb->attr->value += '0';

    SETIOV(ctp->iov, &ocb->attr->value, nbytes);
    _IO_SET_READ_NBYTES(ctp, nbytes);

    ocb->offset += nbytes;
    nparts = 1;
  } else {
    _IO_SET_READ_NBYTES(ctp, 0);
    nparts = 0;
  }

  return _RESMGR_NPARTS(nparts);
}

static int resmgr_devctl(resmgr_context_t *ctp, io_devctl_t *msg,
                         RESMGR_OCB_T *ocb) {
  int status;
  int nbytes;
  uint16_t lines;

  if ((status = iofunc_devctl_default(ctp, msg, ocb)) != _RESMGR_DEFAULT) {
    return status;
  }

  switch (msg->i.dcmd) {
  case GPIO_GET_CHIPINFO_IOCTL: {
    struct gpiochip_info *info = _IO_OUTPUT_PAYLOAD(msg);
    size_t index;

    if ((status = virtio_gpio_num_gpios(ocb->attr->dev, &lines)) != EOK) {
      return status;
    }

    if ((status = virtio_gpio_get_chip_index(ocb->attr->dev, &index)) != EOK) {
      return status;
    }

    snprintf(info->name, GPIO_MAX_NAME_SIZE, "gpiochip%zu", index);
    strcpy(info->label, "virtio");
    info->lines = lines;

    nbytes = sizeof(struct gpiochip_info);
  } break;

  case GPIO_V2_GET_LINEINFO_IOCTL: {
    uint16_t lines;
    struct gpio_v2_line_info *info = _IO_INPUT_PAYLOAD(msg);
    uint32_t offset = info->offset;
    size_t index;
    uint8_t direction;

    if ((status = virtio_gpio_num_gpios(ocb->attr->dev, &lines)) != EOK) {
      return status;
    }

    if ((status = virtio_gpio_get_chip_index(ocb->attr->dev, &index)) != EOK) {
      return status;
    }

    if (offset >= lines) {
      return ENOENT;
    }

    info = _IO_OUTPUT_PAYLOAD(msg);

    memset(info, 0, sizeof(struct gpio_v2_line_info));

    info->offset = offset;
    snprintf(info->name, GPIO_MAX_NAME_SIZE, "gpio%zu.%u", index, offset);

    if ((status = virtio_gpio_get_direction(ocb->attr->dev, offset,
                                            &direction)) != EOK) {
      return status;
    }

    info->flags |=
        direction == 0 ? GPIO_V2_LINE_FLAG_INPUT : GPIO_V2_LINE_FLAG_OUTPUT;

    nbytes = sizeof(struct gpio_v2_line_info);
  } break;
  case GPIO_GET_LINEINFO_UNWATCH_IOCTL:
  case GPIO_V2_GET_LINEINFO_WATCH_IOCTL:
  case GPIO_V2_GET_LINE_IOCTL:
  case GPIO_V2_LINE_SET_CONFIG_IOCTL:
  case GPIO_V2_LINE_GET_VALUES_IOCTL:
  case GPIO_V2_LINE_SET_VALUES_IOCTL:
  case GPIO_GET_LINEINFO_IOCTL:
  case GPIO_GET_LINEHANDLE_IOCTL:
  case GPIO_GET_LINEEVENT_IOCTL:
  case GPIOHANDLE_GET_LINE_VALUES_IOCTL:
  case GPIOHANDLE_SET_LINE_VALUES_IOCTL:
  case GPIOHANDLE_SET_CONFIG_IOCTL:
  case GPIO_GET_LINEINFO_WATCH_IOCTL:
    return ENOTSUP;
  default:
    return ENOSYS;
  }

  memset(&msg->o, 0, sizeof(msg->o));
  msg->o.ret_val = status;

  return (_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o) + nbytes));
}

int resmgr_run(struct virtio_gpio_device *dev, uint16_t index) {
  dispatch_t *dpp;
  struct gpio_iofunc_attr *attrs;
  int rc;
  dispatch_context_t *ctp;
  char path[32];
  struct gpio_resmgr *resmgr;
  int *ids;
  uint16_t ngpio;

  if (dev == NULL) {
    return EINVAL;
  }

  rc = virtio_gpio_num_gpios(dev, &ngpio);
  if (rc != EOK) {
    log_err("failed to get gpio count: %s", strerror(rc));
    return rc;
  }

  log_info("starting resource manager for %u gpio lines", ngpio);

  dpp = dispatch_create();
  if (dpp == NULL) {
    log_err("failed to create dispatch: %s", strerror(errno));
    return errno;
  }

  resmgr = calloc(1, sizeof(struct gpio_resmgr));
  if (resmgr == NULL) {
    log_err("failed to allocate resmgr structure: %s", strerror(errno));
    rc = errno;
    goto free_dpp;
  }
  resmgr->resmgr_attr.nparts_max = 1;
  resmgr->resmgr_attr.msg_max_size = 2048;

  attrs = calloc(ngpio + 1, sizeof(struct gpio_iofunc_attr));
  if (attrs == NULL) {
    log_err("failed to allocate gpio attributes: %s", strerror(errno));
    rc = errno;
    goto free_resmgr;
  }

  ids = calloc(ngpio + 1, sizeof(int));
  if (ids == NULL) {
    log_err("failed to allocate resource ids: %s", strerror(errno));
    rc = errno;
    goto free_attrs;
  }
  memset(ids, -1, ngpio * sizeof(int));

  iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &resmgr->connect_funcs,
                   _RESMGR_IO_NFUNCS, &resmgr->io_funcs);

  resmgr->io_funcs.write = resmgr_write;
  resmgr->io_funcs.read = resmgr_read;
  resmgr->io_funcs.devctl = resmgr_devctl;

  iofunc_attr_init(&attrs[ngpio].attr, S_IFCHR | 0666, 0, 0);
  attrs[ngpio].dev = dev;
  attrs[ngpio].index = -1;
  attrs[ngpio].attr.nbytes = 0;
  snprintf(path, sizeof(path), "/dev/gpiochip%u", index);
  ids[ngpio] =
      resmgr_attach(dpp, &resmgr->resmgr_attr, path, _FTYPE_ANY, 0,
                    &resmgr->connect_funcs, &resmgr->io_funcs, &attrs[ngpio]);
  if (ids[ngpio] == -1) {
    log_err("failed to attach %s: %s", path, strerror(errno));
    rc = errno;
    goto detach;
  }

  for (uint16_t n = 0; n < ngpio; n++) {
    iofunc_attr_init(&attrs[n].attr, S_IFCHR | 0666, 0, 0);
    attrs[n].dev = dev;
    attrs[n].index = n;
    attrs[n].attr.nbytes = 1;
    snprintf(path, sizeof(path), "/dev/gpio%u.%u", index, n);
    ids[n] =
        resmgr_attach(dpp, &resmgr->resmgr_attr, path, _FTYPE_ANY, 0,
                      &resmgr->connect_funcs, &resmgr->io_funcs, &attrs[n]);
    if (ids[n] == -1) {
      log_err("failed to attach %s: %s", path, strerror(errno));
      rc = errno;
      goto detach;
    }
  }

  log_info("resource manager ready, entering dispatch loop");

  ctp = dispatch_context_alloc(dpp);
  if (ctp == NULL) {
    log_err("failed to allocate dispatch context: %s", strerror(errno));
    rc = errno;
    goto detach;
  }

  while (1) {
    ctp = dispatch_block(ctp);
    if (ctp == NULL) {
      return errno;
    }
    dispatch_handler(ctp);
  }

  return EOK;

detach:
  for (uint16_t n = 0; n < ngpio; n++) {
    if (ids[n] != -1) {
      resmgr_detach(dpp, ids[n], 0);
    }
  }
  free(ids);

free_attrs:
  free(attrs);

free_resmgr:
  free(resmgr);

free_dpp:
  dispatch_destroy(dpp);

  return rc;
}
