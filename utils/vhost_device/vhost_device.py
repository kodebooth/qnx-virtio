# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

from __future__ import annotations

from abc import ABC
from functools import cached_property
from pathlib import Path

import os
import random
import string
import threading
import logging
from subprocess import Popen, TimeoutExpired

logger = logging.getLogger(__name__)


class VhostDevice(ABC):
    name: str
    suffix: str
    process: Popen | None
    stdout_lines: list[str]
    stderr_lines: list[str]
    stdout_thread: threading.Thread | None
    stderr_thread: threading.Thread | None

    def __init__(self, name: str) -> None:
        self.name = name
        self.suffix = ''.join(random.choices(
            string.ascii_letters + string.digits, k=8))
        self.process = None
        self.stdout_lines = []
        self.stderr_lines = []
        self.stdout_thread = None
        self.stderr_thread = None

    @cached_property
    def socket_path(self) -> Path:
        return Path(f"/tmp/qnx-virtio-vhost-device-socket-{self.name}-{self.suffix}")

    @cached_property
    def created_socket_path(self) -> Path:
        return Path(str(self.socket_path) + "0")

    def _capture_output(self, stream, stream_name: str) -> None:
        """Capture output from a stream in a background thread"""
        try:
            for line in iter(stream.readline, ''):
                if line:
                    logger.info(
                        f"[vhost-{self.name} {stream_name}] {line.rstrip()}")
        except Exception as e:
            logger.error(f"Error capturing {
                         stream_name} for vhost-{self.name}: {e}")

    def start_capture_threads(self) -> None:
        """Start background threads to capture stdout and stderr from the process"""
        if not self.process:
            raise RuntimeError(
                "Process must be started before capturing output")

        self.stdout_thread = threading.Thread(
            target=self._capture_output,
            args=(self.process.stdout, "stdout"),
            daemon=True
        )
        self.stderr_thread = threading.Thread(
            target=self._capture_output,
            args=(self.process.stderr, "stderr"),
            daemon=True
        )
        self.stdout_thread.start()
        self.stderr_thread.start()

    def stop(self) -> None:
        """Stop the vhost-device process"""
        if self.process:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except TimeoutExpired:
                self.process.kill()
                self.process.wait()

        if os.path.exists(self.created_socket_path):
            try:
                os.remove(self.created_socket_path)
            except FileNotFoundError:
                pass

    def qemu_args(self, architecture) -> list[str]:
        ...

    def start(self) -> None:
        ...
