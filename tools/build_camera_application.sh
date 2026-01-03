#!/usr/bin/env bash
set -euo pipefail

TOPDIR="$(dirname $(realpath ${BASH_SOURCE[0]}) | rev | cut -d '/' -f2- | rev)"

mkdir -p build-aarch64
cmake -S ${TOPDIR} -B ${TOPDIR}/build-aarch64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=${TOPDIR}/aarch64-toolchain.cmake
cmake --build ${TOPDIR}/build-aarch64