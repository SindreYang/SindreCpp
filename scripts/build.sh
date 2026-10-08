#!/usr/bin/env bash
set -euo pipefail

# 统一的 Linux/WSL Clang RelWithDebInfo 构建入口；所有产物位于 build_linux/bin。
case "$(uname -s)" in
    Linux*) ;;
    *) echo "sindrecpp supports Linux/WSL only from build.sh" >&2; exit 2 ;;
esac

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

cmake --preset linux-clang "$@"
cmake --build --preset linux-clang
ctest --preset linux-clang
