# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

"""
Pytest configuration for GPIO tests
"""

# Import fixtures from utils (pytest will auto-discover)
from utils.qemu.conftest import qemu_with_gpio

__all__ = ["qemu_with_gpio"]
