/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */

/**
 * @file gpio.h
 * @brief VirtIO GPIO device driver interface
 *
 * This file provides the main interface for the VirtIO GPIO device driver,
 * implementing the VirtIO GPIO specification. It supports GPIO direction
 * control, value reading/writing, and device initialization.
 *
 * The driver communicates with the VirtIO GPIO device through virtqueues
 * and exposes GPIO pins through the QNX resource manager interface.
 */

#ifndef VIRTIO_QNX_GPIO_H_INCLUDED
#define VIRTIO_QNX_GPIO_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

/** @brief Forward declaration of VirtIO GPIO device structure */
struct virtio_gpio_device;

/**
 * @brief Get the number of GPIO pins
 *
 * Queries the VirtIO GPIO device to determine how many GPIO pins are
 * available. This value is read from the device configuration space.
 *
 * @param dev VirtIO GPIO device instance
 * @param ngpio Output buffer to receive number of GPIOs
 * @return 0 on success, negative error code on failure
 */
int virtio_gpio_num_gpios(struct virtio_gpio_device *dev, uint16_t *ngpio);

/**
 * @brief Get the size of GPIO names buffer
 *
 * Queries the size of the buffer needed to store GPIO pin names.
 * The names buffer contains null-terminated strings for each GPIO pin.
 *
 * @param dev VirtIO GPIO device instance
 * @param size Output buffer to receive buffer size in bytes
 * @return 0 on success, negative error code on failure
 */
int virtio_gpio_names_size(struct virtio_gpio_device *dev, uint32_t *size);

/**
 * @brief Set GPIO pin direction
 *
 * Configures a GPIO pin as input or output. The direction must be set
 * before reading from or writing to the pin.
 *
 * @param dev VirtIO GPIO device instance
 * @param gpio GPIO pin number (0-based index)
 * @param direction Direction value (0 = input, 1 = output)
 * @return 0 on success, negative error code on failure
 */
int virtio_gpio_set_direction(struct virtio_gpio_device *dev, uint16_t gpio,
                              uint8_t direction);

/**
 * @brief Get GPIO pin direction
 *
 * Queries the current direction configuration of a GPIO pin.
 *
 * @param dev VirtIO GPIO device instance
 * @param gpio GPIO pin number (0-based index)
 * @param direction Output buffer to receive direction (0 = input, 1 = output)
 * @return 0 on success, negative error code on failure
 */
int virtio_gpio_get_direction(struct virtio_gpio_device *dev, uint16_t gpio,
                              uint8_t *direction);

/**
 * @brief Set GPIO pin value
 *
 * Sets the output value of a GPIO pin. The pin must be configured as
 * an output before calling this function.
 *
 * @param dev VirtIO GPIO device instance
 * @param gpio GPIO pin number (0-based index)
 * @param value Value to set (0 = low, 1 = high)
 * @return 0 on success, negative error code on failure
 */
int virtio_gpio_set_value(struct virtio_gpio_device *dev, uint16_t gpio,
                          uint8_t value);

/**
 * @brief Get GPIO pin value
 *
 * Reads the current value of a GPIO pin. For input pins, this reads the
 * external signal. For output pins, this reads the last written value.
 *
 * @param dev VirtIO GPIO device instance
 * @param gpio GPIO pin number (0-based index)
 * @param value Output buffer to receive value (0 = low, 1 = high)
 * @return 0 on success, negative error code on failure
 */
int virtio_gpio_get_value(struct virtio_gpio_device *dev, uint16_t gpio,
                          uint8_t *value);

/**
 * @brief Initialize VirtIO GPIO device
 *
 * Initializes a VirtIO GPIO device instance by discovering the device at
 * the specified memory address, negotiating features, setting up virtqueues,
 * and preparing the device for operation.
 *
 * The initialization sequence follows the VirtIO specification:
 * 1. Reset device
 * 2. Acknowledge device
 * 3. Read and negotiate features
 * 4. Set up virtqueues
 * 5. Set DRIVER_OK status
 *
 * @param idx Device index (for multiple instances)
 * @param mem Physical memory address of VirtIO device registers
 * @param irq Interrupt request number
 * @param qsize Virtqueue size (must be power of 2)
 * @param dev Output pointer to receive initialized device instance
 * @return 0 on success, negative error code on failure
 */
int virtio_gpio_init(size_t idx, uint64_t mem, uint32_t irq, uint16_t qsize,
                     struct virtio_gpio_device **dev);

#endif
