# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(QNX_SDP_INSTALL_DIR "$ENV{QNX_SDP_INSTALL_DIR}")

set(CMAKE_C_COMPILER "${QNX_SDP_INSTALL_DIR}/qcc_wrapper.sh")
set(CMAKE_CXX_COMPILER "${QNX_SDP_INSTALL_DIR}/qcc_wrapper.sh")
set(CMAKE_ASM_COMPILER "${QNX_SDP_INSTALL_DIR}/qcc_wrapper.sh")

set(CMAKE_C_FLAGS "-Vgcc_nto${CMAKE_SYSTEM_PROCESSOR} -Wall -Werror -Wextra -I${QNX_SDP_INSTALL_DIR}/target/qnx/usr/include")
