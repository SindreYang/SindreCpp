#!/usr/bin/env bash
set -euo pipefail

# 统一的 Linux/WSL Release 构建入口；所有产物位于 build/linux/bin。
case "$(uname -s)" in
    Linux*) ;;
    *) echo "sindrecpp supports Linux/WSL only from build.sh" >&2; exit 2 ;;
esac

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/linux"

command -v ninja >/dev/null 2>&1 || {
    echo "Ninja is required. Install Ninja and make it available on PATH." >&2
    exit 2
}

OPENBLAS_ARGS=()
if [[ -n "${SINDRE_MATH_OPENBLAS_ROOT:-}" ]]; then
    OPENBLAS_ARGS+=("-DSINDRE_MATH_OPENBLAS_ROOT=${SINDRE_MATH_OPENBLAS_ROOT}")
fi

cmake -S "${ROOT}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DSINDRE_BUILD_TESTS=ON \
    -DSINDRE_BUILD_EXAMPLES=ON "${OPENBLAS_ARGS[@]}" "$@"
cmake --build "${BUILD_DIR}" --parallel
ctest --test-dir "${BUILD_DIR}" --output-on-failure
