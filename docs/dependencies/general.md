# General dependencies

本文说明基础模块的 Eigen 数据桥和 General 聚合依赖。General 是基础库，以下依赖
全部必需，不能通过 `SINDRE_WITH_*` 关闭或替换。

General is the foundation module. Eigen is its project-wide data bridge and all
integrations below are mandatory. Configuration fails when the fixed profile is
missing or a host package would be selected.

| Feature | Dependency | Source | Version | Required |
| --- | --- | --- | --- | --- |
| data bridge | [Eigen](https://gitlab.com/libeigen/eigen) | fixed source under `thirds/general/sources` | `3.4.1` | yes |
| string | [CsString](https://github.com/copperspice/cs_string) | fixed source + static implementation | `string-1.4.1` | yes |
| logging | [spdlog](https://github.com/gabime/spdlog) | fixed source under `thirds/general/sources` | `v1.17.0` | yes |
| HTTP/HTTPS | [cpp-httplib](https://github.com/yhirose/cpp-httplib) + OpenSSL | fixed source + fixed package | `v0.56.0` / OpenSSL 3.3.0 | yes |
| JSON/config | [simdjson](https://github.com/simdjson/simdjson) | fixed source under `thirds/general/sources` | `v4.6.11` | yes |
| CLI | [argparse](https://github.com/p-ranav/argparse) | fixed source under `thirds/general/sources` | `v3.2` | yes |
| regular expressions | [RE2](https://github.com/google/re2) | fixed package under `thirds/general/packages` | `2024-04-01#2` | yes |
| crash reporting | [Crashpad](https://chromium.googlesource.com/crashpad/crashpad/) | fixed package under `thirds/general/packages` | `2024-04-11` | yes |
| compression | [zlib](https://zlib.net/) | fixed package under `thirds/general/packages` | `1.3.1` | yes |

OpenBLAS and the fixed General source trees are stored under `thirds/general`.
The fixed binary package set for RE2, Crashpad, zlib and OpenSSL is kept under
`thirds/general/packages` and selected before any host package path.
Crashpad is linked through the `crashpad::crashpad` target when available.

The General dependency versions and package metadata are registered in
`thirds/general/Dependencies.cmake`. CMake uses the fixed source trees and
the fixed `general-x64-windows` package profile with `NO_DEFAULT_PATH`;
system/vcpkg installations cannot silently replace them.

The fixed Windows package profile uses the dynamic MSVC runtime and
`_ITERATOR_DEBUG_LEVEL=0`. General applies the same ABI to Debug and Release
consumers.

安装后的 CMake consumer 仍需要让 `OpenBLAS_DIR`、OpenSSL、RE2、Crashpad、zlib
对应的固定包可被发现；Windows 运行时还需要把 `thirds/general/openblas/bin` 和
固定 vcpkg 包的 `bin` 加入 DLL 搜索路径。静态链接 sindrecpp 本身不等于把这些
第三方运行时 DLL 复制进宿主应用。
