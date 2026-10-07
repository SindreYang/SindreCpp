# General dependencies

本文说明基础模块的 Eigen 数据桥和 General 聚合依赖。默认构建会把这些依赖全部
接入 `sindre::general`；如产品不需要某项能力，可显式关闭对应 `SINDRE_WITH_*`。

General is the foundation module. Eigen is its project-wide data bridge and is
resolved centrally; the integrations below are enabled by default and can be
disabled individually for a smaller build.

| Feature | Dependency | Source | Version | Default |
| --- | --- | --- | --- | --- |
| data bridge | [Eigen](https://gitlab.com/libeigen/eigen) | fixed source under `thirds/general/sources` | `3.4.1` | on |
| string | [CsString](https://github.com/copperspice/cs_string) | fixed source + static implementation | `string-1.4.1` | on |
| logging | [spdlog](https://github.com/gabime/spdlog) | fixed source under `thirds/general/sources` | `v1.17.0` | on |
| HTTP/HTTPS | [cpp-httplib](https://github.com/yhirose/cpp-httplib) + OpenSSL | fixed source + fixed package | `v0.56.0` / OpenSSL 3.3.0 | on |
| JSON/config | [simdjson](https://github.com/simdjson/simdjson) | fixed source under `thirds/general/sources` | `v4.6.11` | on |
| CLI | [argparse](https://github.com/p-ranav/argparse) | fixed source under `thirds/general/sources` | `v3.2` | on |
| regular expressions | [RE2](https://github.com/google/re2) | fixed package under `thirds/general/packages` | `2024-04-01#2` | on |
| crash reporting | [Crashpad](https://chromium.googlesource.com/crashpad/crashpad/) | fixed package under `thirds/general/packages` | `2024-04-11` | on |
| compression | [zlib](https://zlib.net/) | fixed package under `thirds/general/packages` | `1.3.1` | on |

OpenBLAS and the fixed General source trees are stored under `thirds/general`.
The fixed binary package set for RE2, Crashpad, zlib and OpenSSL is kept under
`thirds/general/packages` and selected before any host package path.
Crashpad is linked through the `crashpad::crashpad` target when available.

The General dependency versions and package metadata are registered in
`thirds/general/Dependencies.cmake`; fixed sources are not downloaded into
the build directory.

By default the fixed sources in `thirds/general/sources` are authoritative. Set
`SINDRE_GENERAL_USE_EXTERNAL_DEPS=ON` only when the host application has already
audited compatible CMake targets and wants to override the fixed spdlog,
cpp-httplib, simdjson or argparse source trees.

安装后的 CMake consumer 仍需要让 `OpenBLAS_DIR`、OpenSSL、RE2、Crashpad、zlib
对应的固定包可被发现；Windows 运行时还需要把 `thirds/general/openblas/bin` 和
固定 vcpkg 包的 `bin` 加入 DLL 搜索路径。静态链接 sindrecpp 本身不等于把这些
第三方运行时 DLL 复制进宿主应用。
