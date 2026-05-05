# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

"""
Pytest fixtures for QEMU utilities
"""

import os
import time
import subprocess
import pytest
from pathlib import Path

from utils.qemu.qemu import Qemu, Architecture
from utils.qemu.serial import SerialConnection
from utils.vhost_device.vhost_device_gpio import VhostDeviceGpio


@pytest.fixture(scope="module")
def qemu_with_gpio(request):
    """
    Fixture that provides a running QEMU instance with GPIO device

    Returns:
        tuple: (Qemu instance, SerialConnection)
    """
    arch = Architecture(request.config.getoption("--arch"))

    # Always build to ensure image is up to date
    print(f"Building IFS image for {arch}...")
    subprocess.run(
        ["cmake", "--preset", arch],
        check=True
    )
    subprocess.run(
        ["cmake", "--build", "--preset", arch],
        check=True
    )

    build_dir = Path("build") / arch
    ifs_image = build_dir / "ifs.img"

    if not ifs_image.exists():
        raise FileNotFoundError(
            f"IFS image not found after build at {ifs_image}. "
            f"Please configure first: cmake --preset {arch}"
        )

    # Create vhost-device-gpio backend
    vhost_gpio = VhostDeviceGpio(num_gpios=8)

    # Create QEMU instance
    qemu = Qemu(
        kernel_image=ifs_image,
        architecture=arch,
        vhost_devices=[vhost_gpio]
    )

    serial = None

    try:
        # Start vhost device backend
        vhost_gpio.start()

        # Start QEMU
        qemu.start()
        print(f"Started QEMU (PID: {qemu.process.pid})")

        # Wait for serial socket to be created
        for _ in range(50):
            if os.path.exists(qemu.serial_socket):
                break
            time.sleep(0.1)
        else:
            raise RuntimeError("QEMU failed to create serial socket")

        # Connect to serial console
        serial = SerialConnection(qemu.serial_socket)
        serial.connect()

        # Wait for boot
        serial.wait_for_boot()

        # Give system a moment to stabilize
        time.sleep(2)

        yield qemu, serial

    finally:
        if serial:
            serial.close()
        qemu.stop()
        vhost_gpio.stop()
