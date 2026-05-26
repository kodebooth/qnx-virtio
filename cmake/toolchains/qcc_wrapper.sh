#!/usr/bin/env bash

# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

set -euo pipefail

QNX_SDP_INSTALL_DIR="${QNX_SDP_INSTALL_DIR:-/opt/qnx800}"

source "$QNX_SDP_INSTALL_DIR/qnxsdp-env.sh" >/dev/null

${QNX_HOST}/usr/bin/qcc "$@"
