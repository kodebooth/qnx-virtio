# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

import subprocess
import os
import time
import logging
from utils.qemu.qemu import Architecture
from utils.vhost_device.vhost_device import VhostDevice

logger = logging.getLogger(__name__)


class VhostDeviceGpio(VhostDevice):

    def __init__(self, num_gpios: int = 8) -> None:
        super().__init__(name="gpio")
        self.num_gpios = num_gpios

    def start(self) -> None:
        """Start the vhost-user-gpio backend (assumes invocation from repo root)"""
        self.process = subprocess.Popen([
            "cargo", "run",
            "--manifest-path=third_party/vhost-device/Cargo.toml",
            "--package=vhost-device-gpio",
            "--features=mock_gpio",
            "--",
            f"--socket-path={self.socket_path}",
            f"--device-list=s{self.num_gpios}"
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)

        # Start capture threads from base class
        self.start_capture_threads()

        # Wait for socket to be created
        # The target may need to be compiled by cargo which can take a long time
        socket_file = f"{self.socket_path}0"
        for _ in range(600):
            if os.path.exists(socket_file):
                break
            time.sleep(0.1)
            # Check if process crashed early
            retcode = self.process.poll()
            if retcode is not None:
                raise RuntimeError(
                    f"vhost-user-gpio backend exited with code {retcode}\n"
                    f"Captured output available via get_logs()"
                )
        else:
            # Check if process crashed
            retcode = self.process.poll()
            if retcode is not None:
                raise RuntimeError(
                    f"vhost-user-gpio backend exited with code {retcode}\n"
                )
            raise RuntimeError(
                f"vhost-user-gpio backend failed to create socket at {
                    socket_file}"
            )

        logger.info(
            f"Started vhost-user-gpio backend (PID: {self.process.pid})")

    def qemu_args(self, architecture: Architecture) -> list[str]:
        device_type = ""
        if architecture == Architecture.x86_64:
            device_type = "pci"
        elif architecture == Architecture.aarch64le:
            device_type = "device"

        return [
            "-chardev", f"socket,path={self.created_socket_path},id=vgpio",
            "-device", f"vhost-user-gpio-{device_type},chardev=vgpio,id=gpio",
        ]
