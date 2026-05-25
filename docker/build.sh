#!/usr/bin/env bash

# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Environment variables with default values
IMAGE_NAME="ghcr.io/kodebooth/qnx-virtio"
IMAGE_TAG="0.0.1"

echo "Building Docker image from ${SCRIPT_DIR}..."
docker build \
  -t "${IMAGE_NAME}:${IMAGE_TAG}" \
  -t "${IMAGE_NAME}:latest" \
  "${SCRIPT_DIR}"

echo "✓ Build complete"
