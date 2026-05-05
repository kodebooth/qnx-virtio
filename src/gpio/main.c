/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <string.h>
#include <sys/procmgr.h>
#include <syslog.h>

#include "gpio.h"
#include "options.h"
#include "resmgr.h"
#include <sys/mman.h>
#include <sys/neutrino.h>

int main(int argc, char **argv) {
  struct virtio_gpio_device *dev;
  struct virtio_args virtio_args = {0};
  int rc;

  rc = parse_options(argc, argv, &virtio_args);
  if (rc != 0) {
    return -1;
  }

  ThreadCtl(_NTO_TCTL_IO, 0);

  openlog("dev-virtio-gpio", LOG_PID | LOG_CONS | LOG_PERROR, LOG_USER);

  syslog(LOG_INFO,
         "initializing virtio gpio device with idx=%u, mem=0x%lx, "
         "irq=%u, qsize=%u",
         virtio_args.idx, virtio_args.mem, virtio_args.irq, virtio_args.qsize);

  rc = virtio_gpio_init(virtio_args.idx, virtio_args.mem, virtio_args.irq,
                        virtio_args.qsize, &dev);
  if (rc != 0) {
    syslog(LOG_ERR, "failed to initialize virtio gpio device: %s",
           strerror(rc));
    return -1;
  }

  return resmgr_run(dev, 0);
}
