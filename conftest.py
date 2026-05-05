# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

"""
Pytest configuration for qnx-virtio project
"""

import pytest


def pytest_addoption(parser):
    """Add custom command line options"""
    parser.addoption(
        "--arch",
        action="store",
        default="x86_64",
        help="Architecture to test: x86_64 or aarch64le"
    )
