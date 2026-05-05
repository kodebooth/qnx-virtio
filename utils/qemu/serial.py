# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

"""Serial communication utilities for QEMU"""

import socket
import time
from pathlib import Path


class SerialConnection:
    """Manages serial connection to QEMU"""

    def __init__(self, socket_path: Path, timeout: float = 1.0):
        self.socket_path = socket_path
        self.timeout = timeout
        self._socket = None

    def connect(self, retry_timeout: int = 30) -> None:
        """Connect to the QEMU serial console"""
        start_time = time.time()

        while time.time() - start_time < retry_timeout:
            try:
                self._socket = socket.socket(
                    socket.AF_UNIX, socket.SOCK_STREAM)
                self._socket.connect(str(self.socket_path))
                self._socket.settimeout(self.timeout)
                return
            except (FileNotFoundError, ConnectionRefusedError):
                if self._socket:
                    self._socket.close()
                    self._socket = None
                time.sleep(0.5)

        raise RuntimeError("Failed to connect to QEMU serial console")

    def wait_for_boot(self, boot_marker: str = "Startup complete", timeout: int = 60) -> bool:
        """Wait for QNX to boot and show the startup complete message"""
        start_time = time.time()
        buffer = b""

        while time.time() - start_time < timeout:
            try:
                data = self._socket.recv(1024)
                if not data:
                    time.sleep(0.1)
                    continue

                buffer += data
                print(data.decode('utf-8', errors='ignore'), end='')

                if boot_marker.encode() in buffer:
                    return True

            except socket.timeout:
                continue
            except Exception as e:
                print(f"Error reading serial: {e}")
                break

        raise RuntimeError(f"System failed to boot within {timeout}s")

    def send_command(self, command: str, timeout: float = 5.0) -> str:
        """Send a command via serial and return the output"""
        self._socket.sendall(command.encode() + b'\n')
        time.sleep(0.1)

        start_time = time.time()
        buffer = b""

        while time.time() - start_time < timeout:
            try:
                data = self._socket.recv(4096)
                if data:
                    buffer += data
                else:
                    time.sleep(0.1)
            except socket.timeout:
                break

        return buffer.decode('utf-8', errors='ignore')

    def close(self) -> None:
        """Close the serial connection"""
        if self._socket:
            try:
                self._socket.close()
            except:
                pass
            self._socket = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
