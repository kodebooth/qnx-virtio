# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

"""
Tests for QNX VirtIO GPIO driver
"""

import time


def test_gpio_device_exists(qemu_with_gpio):
    """Test that GPIO devices are created"""
    _, serial = qemu_with_gpio

    # List /dev to see GPIO devices
    output = serial.send_command("ls -l /dev/gpio*")
    print(f"GPIO devices:\n{output}")

    # Check that at least gpio0.0 exists
    assert "gpio0.0" in output, "GPIO device gpio0.0 not found"


def test_gpio_write_read(qemu_with_gpio):
    """Test writing and reading GPIO values"""
    _, serial = qemu_with_gpio

    # Test GPIO 0
    gpio_dev = "/dev/gpio0.0"

    # Write '1' to GPIO
    print(f"\nWriting '1' to {gpio_dev}")
    serial.send_command(f"echo -n '1' > {gpio_dev}")
    time.sleep(0.2)

    # Read back value
    output = serial.send_command(f"cat {gpio_dev}")
    print(f"Read value: {repr(output)}")

    # The output should contain '1'
    assert '1' in output, f"Expected to read '1' but got: {output}"

    # Write '0' to GPIO
    print(f"\nWriting '0' to {gpio_dev}")
    serial.send_command(f"echo -n '0' > {gpio_dev}")
    time.sleep(0.2)

    # Read back value
    output = serial.send_command(f"cat {gpio_dev}")
    print(f"Read value: {repr(output)}")

    # The output should contain '0'
    assert '0' in output, f"Expected to read '0' but got: {output}"


def test_gpio_toggle_multiple(qemu_with_gpio):
    """Test toggling multiple GPIO pins"""
    _, serial = qemu_with_gpio

    # Test multiple GPIOs
    for gpio_num in range(3):
        gpio_dev = f"/dev/gpio0.{gpio_num}"

        print(f"\nTesting {gpio_dev}")

        # Toggle: 0 -> 1 -> 0
        for value in ['0', '1', '0']:
            serial.send_command(f"echo -n '{value}' > {gpio_dev}")
            time.sleep(0.1)

            output = serial.send_command(f"cat {gpio_dev}")
            assert value in output, f"GPIO {
                gpio_num}: Expected '{value}' but got: {output}"
            print(f"  {gpio_dev} = {value} ✓")
