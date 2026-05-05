/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include "resmgr.h"

struct gpio_iofunc_attr {
  /* must be the first field */
  iofunc_attr_t attr;

  struct virtio_gpio_device *dev;
  uint16_t index;
};

struct gpio_resmgr {
  resmgr_attr_t resmgr_attr;
  resmgr_connect_funcs_t connect_funcs;
  resmgr_io_funcs_t io_funcs;
  struct gpio_iofunc_attr *attr;
  int *ids;
};

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

static int resmgr_read(resmgr_context_t *ctp, io_read_t *msg,
                       RESMGR_OCB_T *ocb) {
  int rc;
  uint8_t value;
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
    rc = virtio_gpio_get_value(ocb->attr->dev, ocb->attr->index, &value);
    if (rc != EOK) {
      return rc;
    }

    value += '0';

    SETIOV(ctp->iov, &value, nbytes);
    _IO_SET_READ_NBYTES(ctp, nbytes);

    ocb->offset += nbytes;
    nparts = 1;
  } else {
    _IO_SET_READ_NBYTES(ctp, 0);
    nparts = 0;
  }

  return _RESMGR_NPARTS(nparts);
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
    return rc;
  }

  dpp = dispatch_create();
  if (dpp == NULL) {
    return errno;
  }

  resmgr = calloc(1, sizeof(struct gpio_resmgr));
  if (resmgr == NULL) {
    rc = errno;
    goto free_dpp;
  }
  resmgr->resmgr_attr.nparts_max = 1;
  resmgr->resmgr_attr.msg_max_size = 2048;

  attrs = calloc(ngpio, sizeof(struct gpio_iofunc_attr));
  if (attrs == NULL) {
    rc = errno;
    goto free_resmgr;
  }

  ids = calloc(ngpio, sizeof(int));
  if (ids == NULL) {
    rc = errno;
    goto free_attrs;
  }
  memset(ids, -1, ngpio * sizeof(int));

  iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &resmgr->connect_funcs,
                   _RESMGR_IO_NFUNCS, &resmgr->io_funcs);

  resmgr->io_funcs.write = resmgr_write;
  resmgr->io_funcs.read = resmgr_read;

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
      rc = errno;
      goto detach;
    }
  }

  ctp = dispatch_context_alloc(dpp);
  if (ctp == NULL) {
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
