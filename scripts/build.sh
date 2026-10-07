#!/usr/bin/env sh
set -eu

# Linux/WSL Release 配置、编译和测试快捷入口；macOS 当前不受支持。
# 如需使用内置 profile 之外的固定 OpenBLAS SDK，可设置：
#   SINDRE_MATH_OPENBLAS_ROOT=/opt/OpenBLAS ./scripts/build.sh

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-build}
BUILD_PATH="$ROOT_DIR/$BUILD_DIR"

set -- \
    -S "$ROOT_DIR" \
    -B "$BUILD_PATH" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DSINDRE_BUILD_TESTS=ON \
    -DSINDRE_BUILD_BENCHMARKS=ON \
    -DSINDRE_BUILD_EXAMPLES=OFF \
    "$@"

if [ -n "${SINDRE_MATH_OPENBLAS_ROOT:-}" ]; then
    set -- "$@" "-DSINDRE_MATH_OPENBLAS_ROOT=$SINDRE_MATH_OPENBLAS_ROOT"
fi

cmake "$@"

if [ -n "${CMAKE_BUILD_PARALLEL_LEVEL:-}" ]; then
    cmake --build "$BUILD_PATH" --parallel "$CMAKE_BUILD_PARALLEL_LEVEL"
else
    cmake --build "$BUILD_PATH" --parallel
fi

cmake --build "$BUILD_PATH" --target test
