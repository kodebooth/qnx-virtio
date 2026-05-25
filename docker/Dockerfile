# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

ARG VARIANT="resolute"
# tmux
ARG TMUX_VERSION="3.6a"
# neovim
ARG NEOVIM_VERSION="0.12.1"
# qnx8 sdp
ARG QNX_SDP_INSTALL_DIR="/opt/qnx800"
# qemu
ARG QEMU_VERSION="10.2.2"
ARG QEMU_INSTALL_DIR="/opt/qemu"
# cmake
ARG CMAKE_VERSION="4.3.1"
ARG CMAKE_INSTALL_DIR="/opt/cmake"
# rust
ARG RUST_VERSION="1.94.1"
# node
ARG NODE_VERSION="24.14.1"

FROM buildpack-deps:${VARIANT}-curl AS builder

ARG VARIANT
# qemu
ARG QEMU_VERSION
ARG QEMU_INSTALL_DIR

RUN apt-get update && apt-get install -y --no-install-recommends \
  autoconf \
  autoconf-archive \
  automake \
  build-essential \
  catch2 \
  git \
  libfdt-dev \
  libglib2.0-dev \
  libtool \
  make \
  meson \
  ninja-build \
  pkg-config \
  python3 \
  python3-pip \
  python3-venv \
  xz-utils \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /build

RUN mkdir /build/qemu && \
    cd /build/qemu && \
    curl -L https://download.qemu.org/qemu-${QEMU_VERSION}.tar.xz -o qemu.tar.xz && \
    tar -xf qemu.tar.xz --strip-components=1 && \
    rm qemu.tar.xz && \
    ./configure \
    --prefix=${QEMU_INSTALL_DIR} \
    --target-list="x86_64-softmmu,aarch64-softmmu"\
    --enable-kvm \
    --disable-docs && \
    make -j$(nproc) && \
    make DESTDIR=/install/qemu install

#### qnx-sdp image

FROM ghcr.io/kodebooth/qnx-sdp:latest AS qnx-sdp

#### runtime image

FROM buildpack-deps:${VARIANT}-curl AS runtime

# tmux
ARG TMUX_VERSION
# neovim
ARG NEOVIM_VERSION
# qnx8 sdp
ARG QNX_SDP_INSTALL_DIR
# qemu
ARG QEMU_INSTALL_DIR
# cmake
ARG CMAKE_VERSION
ARG CMAKE_INSTALL_DIR
# rust
ARG RUST_VERSION
# node
ARG NODE_VERSION

ENV LANG=en_US.UTF-8

RUN apt-get update && apt-get upgrade -y && apt-get install -y --no-install-recommends \
  build-essential \
  clang \
  device-tree-compiler \
  git \
  libevent-2.1-7t64 \
  libfdt1 \
  libglib2.0-0t64 \
  libgpiod-dev\
  libncurses6 \
  make \
  ninja-build \
  npm \
  pkg-config \
  python3 \
  python3-pytest \
  python3-venv \
  ripgrep \
  telnet \
  unzip \
  && rm -rf /var/lib/apt/lists/*

# tmux
RUN curl -LO https://github.com/tmux/tmux-builds/releases/download/v${TMUX_VERSION}/tmux-${TMUX_VERSION}-linux-x86_64.tar.gz && \
  mkdir -p /opt/tmux && \
  tar -C /opt/tmux -xvf tmux-${TMUX_VERSION}-linux-x86_64.tar.gz && \
  rm -rf tmux-${TMUX_VERSION}-linux-x86_64.tar.gz
ENV PATH="/opt/tmux:${PATH}"

# neovim
RUN curl -LO https://github.com/neovim/neovim/releases/download/v${NEOVIM_VERSION}/nvim-linux-x86_64.tar.gz && \
    rm -rf /opt/nvim-linux-x86_64 && \
    tar -C /opt -xzf nvim-linux-x86_64.tar.gz && \
    rm -rf nvim-linux-x86_64.tar.gz
ENV PATH="/opt/nvim-linux-x86_64/bin:${PATH}"

# qnx sdp
COPY --from=qnx-sdp ${QNX_SDP_INSTALL_DIR} ${QNX_SDP_INSTALL_DIR}
ENV QNX_SDP_INSTALL_DIR="${QNX_SDP_INSTALL_DIR}"
ENV QNX_CONFIGURATION_EXCLUSIVE=${QNX_SDP_INSTALL_DIR}

# cmake
RUN curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.sh" -o cmake.sh && \
    mkdir -p /opt/cmake && \
    chmod +x cmake.sh && \
    ./cmake.sh --skip-license --exclude-subdir --prefix=/opt/cmake && \
    rm -rf cmake.sh
ENV PATH="${CMAKE_INSTALL_DIR}/bin:${PATH}"

# rust
RUN curl -LO https://static.rust-lang.org/dist/rust-${RUST_VERSION}-x86_64-unknown-linux-gnu.tar.xz && \
  tar -xf rust-${RUST_VERSION}-x86_64-unknown-linux-gnu.tar.xz && \
  cd rust-${RUST_VERSION}-x86_64-unknown-linux-gnu && \
  ./install.sh && \
  rm -rf rust-${RUST_VERSION}-x86_64-unknown-linux-gnu*

# node
RUN curl -LO https://nodejs.org/dist/v${NODE_VERSION}/node-v${NODE_VERSION}-linux-x64.tar.xz && \
  tar -C /opt -xf node-v${NODE_VERSION}-linux-x64.tar.xz && \
  rm -rf node-v${NODE_VERSION}-linux-x64.tar.xz
ENV PATH="/opt/node-v${NODE_VERSION}-linux-x64/bin:${PATH}"

COPY --from=builder /install/qemu/${QEMU_INSTALL_DIR} ${QEMU_INSTALL_DIR}
ENV PATH="${QEMU_INSTALL_DIR}/bin:${PATH}"
