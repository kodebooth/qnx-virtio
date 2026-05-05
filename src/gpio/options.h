/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */

/**
 * @file options.h
 * @brief Command-line argument parsing for VirtIO GPIO driver
 *
 * This file provides structures and functions for parsing command-line
 * arguments specific to the VirtIO GPIO device driver, including device
 * index, memory address, interrupt number, and queue size configuration.
 */

#ifndef GPIO_OPTIONS_H
#define GPIO_OPTIONS_H

#include <stdint.h>

/**
 * @brief VirtIO device arguments structure
 *
 * Contains parsed command-line arguments for initializing a VirtIO device.
 * These parameters specify the hardware configuration for device attachment.
 */
struct virtio_args {
  /** @brief Device index (for multiple VirtIO devices) */
  uint32_t idx;

  /** @brief Physical memory address of VirtIO device registers */
  uint64_t mem;

  /** @brief Interrupt request (IRQ) number */
  uint32_t irq;

  /** @brief Virtqueue size (number of descriptors, must be power of 2) */
  uint32_t qsize;
};

/**
 * @brief Parse command-line arguments
 *
 * Parses command-line arguments and populates the virtio_args structure.
 * Supports standard VirtIO device configuration options including device
 * index (-i), memory address (-m), interrupt (-I), and queue size (-q).
 *
 * @param argc Argument count from main()
 * @param argv Argument vector from main()
 * @param args Output structure to receive parsed arguments
 * @return 0 on success, negative error code on failure
 */
int parse_options(int argc, char **argv, struct virtio_args *args);

#endif // GPIO_OPTIONS_H
