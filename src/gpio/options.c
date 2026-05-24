/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */

#include "options.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

enum {
  VIRTIO_OPT_IDX = 0,
  VIRTIO_OPT_MEM,
  VIRTIO_OPT_IRQ,
  VIRTIO_OPT_QSIZE,
};

/**
 * @brief Print usage information to stdout
 *
 * @param[in] prog Program name (typically argv[0])
 */
static void print_usage(const char *prog) {
  printf("Usage: %s [options]\n", prog);
  printf("Options:\n");
  printf("  --virtio idx=<num>,mem=<addr>,irq=<num>,qsize=<num>\n");
  printf("           Configure VirtIO device parameters\n");
  printf("           idx    - Index number\n");
  printf("           mem    - Memory address\n");
  printf("           irq    - IRQ number\n");
  printf("           qsize  - Queue size\n");
  printf("  -h, --help  Show this help message\n");
}

/**
 * @brief Parse VirtIO-specific suboptions
 *
 * @param[in] optarg Option argument string containing suboptions
 * @param[out] args Structure to store parsed arguments
 * @return 0 on success, -1 on error
 */
static int parse_virtio_args(char *optarg, struct virtio_args *args) {
  char *const token[] = {[VIRTIO_OPT_IDX] = "idx",
                         [VIRTIO_OPT_MEM] = "mem",
                         [VIRTIO_OPT_IRQ] = "irq",
                         [VIRTIO_OPT_QSIZE] = "qsize",
                         NULL};

  char *subopts = optarg;
  char *value;

  while (*subopts != '\0') {
    switch (getsubopt(&subopts, token, &value)) {
    case VIRTIO_OPT_IDX:
      if (value == NULL) {
        fprintf(stderr, "Missing value for pci option\n");
        return -1;
      }
      args->idx = (uint32_t)strtoul(value, NULL, 0);
      break;

    case VIRTIO_OPT_MEM:
      if (value == NULL) {
        fprintf(stderr, "Missing value for mem option\n");
        return -1;
      }
      args->mem = (uint64_t)strtoull(value, NULL, 0);
      break;

    case VIRTIO_OPT_IRQ:
      if (value == NULL) {
        fprintf(stderr, "Missing value for irq option\n");
        return -1;
      }
      args->irq = (uint32_t)strtoul(value, NULL, 0);
      break;

    case VIRTIO_OPT_QSIZE:
      if (value == NULL) {
        fprintf(stderr, "Missing value for qsize option\n");
        return -1;
      }
      args->qsize = (uint32_t)strtoul(value, NULL, 0);
      break;

    default:
      fprintf(stderr, "Unknown virtio suboption: %s\n", value);
      return -1;
    }
  }

  return 0;
}

int parse_options(int argc, char **argv, struct virtio_args *args) {
  int opt;

  static struct option long_options[] = {{"virtio", required_argument, 0, 'v'},
                                         {"help", no_argument, 0, 'h'},
                                         {0, 0, 0, 0}};

  while ((opt = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
    switch (opt) {
    case 'v':
      if (parse_virtio_args(optarg, args) != 0) {
        fprintf(stderr, "Error parsing virtio arguments\n");
        print_usage(argv[0]);
        return -1;
      }
      break;

    case 'h':
      print_usage(argv[0]);
      return 1;

    default:
      print_usage(argv[0]);
      return -1;
    }
  }

  return 0;
}
