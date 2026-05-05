#!/usr/bin/env bash

# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

set -euo pipefail

QNX_SDP_INSTALL_DIR="${QNX_SDP_INSTALL_DIR:-/opt/qnx800}"

# S3 object paths for QNX packages
QNX_SDP_PACKAGES=(
  "s3://kodebooth-qnx/com.qnx.qnx800.host.linux.x86_64.binutils_2.43.0.00101T202411221147L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.host.linux.x86_64.gcc_12.2.0.00301T202506091801L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.host.linux.x86_64.liblicense_0.0.1.00016T202507171532L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.host.linux.x86_64.mkifs_1.2.0.00011T202511171438L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.host.linux.x86_64.qcc_8.0.0.00121T202510081309L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.host.mkqnximage_0.3.0.00600T202507302003L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.host.qnxsdp_env_0.0.1.00135T202311191043L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.osr.toybox_0.8.11.00018T202507211733L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.base.bootfiles_0.1.0.00021T202411230047L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.base.compression_0.3.0.00600T202507302003L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.base.cpp_18.0.0.01305T202512161844L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.base.libm_0.0.2.02005T202411230033L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.base.shutdown_0.0.1.00135T202311191043L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.base.startup_0.0.1.00443T202311201012L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.bsp.spi_0.1.2.00015T202501271455L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.connectivity.devc_0.2.0.00011T202411230100L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.drivers.dma_0.0.1.00443T202311201012L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.fs.notify_0.0.2.02005T202411230033L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.microkernel.core_2.4.0.00111T202602271127L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.microkernel.libslog2_0.2.2.00600T202507302003L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.microkernel.slogger2_0.2.1.00600T202507302003L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.microkernel.tools_0.0.3.00600T202507302003L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.os_services.smmu.api_0.0.2.02005T202411230033L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.pci.capabilities_3.0.1.02005T202411230033L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.pci.compat_3.0.0.02005T202411230033L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.pci.core.group_3.0.0.02005T202411230033L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.pci.debug_3.0.0.02005T202411230033L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.pci.hw.x86_3.1.0.02005T202411230033L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.pci.lib_3.0.3.02005T202411230033L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.pci.server_3.1.1.02005T202411230033L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.pci.uisupport_3.0.0.02005T202411230033L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.pci.utils_3.0.0.02005T202411230033L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.qemuvirt_0.2.1.00087T202507241124L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.security.secpol_0.3.0.00600T202507302003L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.tools.cpp_12.2.0.00301T202506091801L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.utils.base.k_0.0.1.00282T202404151301L.qpkg"
  "s3://kodebooth-qnx/com.qnx.qnx800.target.utils.system_0.0.2.02005T202411230033L.qpkg"
)

QNX_SDP_LICENSE="s3://kodebooth-qnx/licenses"

curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip" &&
  unzip awscliv2.zip &&
  ./aws/install &&
  rm awscliv2.zip

mkdir -p "${QNX_SDP_INSTALL_DIR}"

for package_path in "${QNX_SDP_PACKAGES[@]}"; do
  echo "Processing: ${package_path}"

  filename=$(basename "${package_path}")

  echo "Downloading ${filename}..."
  aws s3 cp "${package_path}" "/tmp/${filename}"

  echo "Extracting ${filename} to ${QNX_SDP_INSTALL_DIR}..."
  tar -xf "/tmp/${filename}" -C "${QNX_SDP_INSTALL_DIR}"

  rm -f "/tmp/${filename}"

  echo "Successfully installed ${filename}"
done

# Install license file
mkdir -p "${QNX_SDP_INSTALL_DIR}/license"
aws s3 cp "$QNX_SDP_LICENSE" "${QNX_SDP_INSTALL_DIR}/license/licenses"

echo "All QNX packages installed to ${QNX_SDP_INSTALL_DIR}"
