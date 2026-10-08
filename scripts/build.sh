#!/usr/bin/env bash
set -euo pipefail

# 统一的 Linux/WSL Clang RelWithDebInfo 构建入口；所有产物位于 build_linux/bin。
case "$(uname -s)" in
    Linux*) ;;
    *) echo "sindrecpp supports Linux/WSL only from build.sh" >&2; exit 2 ;;
esac

# 在调用 CMake 前检查生成器和编译器，避免把环境问题误报成项目配置错误。
for tool in cmake ninja clang clang++; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "[sindre] Error: required tool '${tool}' was not found in PATH." >&2
        echo "[sindre] Install CMake, Ninja and Clang, or activate the Linux/WSL toolchain first." >&2
        exit 2
    fi
done

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

cmake --preset linux-clang "$@"
cmake --build --preset linux-clang
ctest --preset linux-clang
