# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

from __future__ import annotations

from dataclasses import dataclass, field
from enum import StrEnum
from pathlib import Path

import random
import string
import os
import subprocess

from utils.vhost_device.vhost_device import VhostDevice


class UnsupportedArchitecture(Exception):
    def __init__(self, architecture: Architecture) -> None:
        super().__init__(f"Unsupported architecture: {architecture.value}")


class Architecture(StrEnum):

    x86_64 = "x86_64"
    aarch64le = "aarch64le"

    @property
    def machine(self) -> str:
        if self.value == Architecture.x86_64.value:
            return "q35"

        if self.value == Architecture.aarch64le.value:
            return "virt"

        raise UnsupportedArchitecture(architecture=self)

    @property
    def cpu(self) -> str:
        if self.value == Architecture.x86_64.value:
            return "max"

        if self.value == Architecture.aarch64le.value:
            return "cortex-a57"

        raise UnsupportedArchitecture(architecture=self)

    @property
    def system(self) -> str:
        if self.value == Architecture.x86_64.value:
            return "qemu-system-x86_64"

        if self.value == Architecture.aarch64le.value:
            return "qemu-system-aarch64"

        raise UnsupportedArchitecture(architecture=self)


@dataclass
class Qemu:
    @staticmethod
    def serial_socket_name() -> Path:
        suffix = ''.join(random.choices(
            string.ascii_letters + string.digits, k=8))
        return Path("/tmp/qnx-virtio-qemu-serial-socket-" + suffix)

    kernel_image: Path
    architecture: Architecture = Architecture.x86_64
    vhost_devices: list[VhostDevice] = field(default_factory=list)
    serial_socket: Path = field(default_factory=serial_socket_name)
    memory_size: int = 4096
    process: subprocess.Popen | None = None

    def start(self) -> None:
        if not os.path.exists(self.kernel_image):
            raise FileNotFoundError(
                f"Kernel image not found at {self.kernel_image}")

        if os.path.exists(self.serial_socket):
            os.remove(self.serial_socket)

        cmd = [
            self.architecture.system,
            "--nographic",
            "-object", "memory-backend-file,id=mem,size=4G,mem-path=/dev/shm,share=on",
            "-numa", "node,memdev=mem",
            "-m", str(self.memory_size),
            "--machine", self.architecture.machine,
            "--cpu", self.architecture.cpu,
            "--kernel", self.kernel_image,
            "-serial", f"unix:{self.serial_socket},server,nowait"
        ]

        for vhost_device in self.vhost_devices:
            cmd.extend(vhost_device.qemu_args(self.architecture))

        self.process = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    def stop(self) -> None:
        """Stop QEMU process"""
        if self.process:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()

        if os.path.exists(self.serial_socket):
            try:
                os.remove(self.serial_socket)
            except FileNotFoundError:
                pass
