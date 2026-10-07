#!/usr/bin/env sh
set -eu

# Quick Release configure, build, and test for Linux, macOS, and Git Bash.
# Set OPENBLAS_ROOT when the official OpenBLAS package is not already in
# CMAKE_PREFIX_PATH, for example:
#   OPENBLAS_ROOT=/opt/OpenBLAS ./scripts/build.sh

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

if [ -n "${OPENBLAS_ROOT:-}" ]; then
    set -- "$@" "-DCMAKE_PREFIX_PATH=$OPENBLAS_ROOT"
fi

cmake "$@"

if [ -n "${CMAKE_BUILD_PARALLEL_LEVEL:-}" ]; then
    cmake --build "$BUILD_PATH" --parallel "$CMAKE_BUILD_PARALLEL_LEVEL"
else
    cmake --build "$BUILD_PATH" --parallel
fi

cmake --build "$BUILD_PATH" --target test
