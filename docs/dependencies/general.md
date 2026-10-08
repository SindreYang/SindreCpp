# General dependencies

本文说明基础模块的固定 General 聚合依赖。General 是基础库，以下依赖
全部必需，不能通过 `SINDRE_WITH_*` 关闭或替换。

General is the foundation module. Its integrations below are mandatory.
Eigen and OpenBLAS belong to the separate Math foundation; see
[`Math dependencies`](math.md).

| Feature | Dependency | Source | Version | Required |
| --- | --- | --- | --- | --- |
| string | [CsString](https://github.com/copperspice/cs_string) | fixed source + static implementation | `string-1.4.1` | yes |
| logging | [spdlog](https://github.com/gabime/spdlog) | fixed source under `thirds/general/sources` | `v1.17.0` | yes |
| HTTP/HTTPS and encryption | [cpp-httplib](https://github.com/yhirose/cpp-httplib) + OpenSSL | fixed source + fixed package | `v0.56.0` / OpenSSL 3.3.0 | yes |
| JSON/config | [simdjson](https://github.com/simdjson/simdjson) | fixed source under `thirds/general/sources` | `v4.6.11` | yes |
| CLI | [argparse](https://github.com/p-ranav/argparse) | fixed source under `thirds/general/sources` | `v3.2` | yes |
| regular expressions | [RE2](https://github.com/google/re2) | fixed package under `thirds/general/packages` | `2024-04-01#2` | yes |
| crash reporting | [Crashpad](https://chromium.googlesource.com/crashpad/crashpad/) | fixed platform package under `thirds/general/packages` | `2022-09-05#5` | yes |
| compression | [zlib](https://zlib.net/) | fixed package under `thirds/general/packages` | `1.3.1` | yes |

cpp-httplib and OpenSSL are implementation dependencies of `sindre_general_runtime`. OpenSSL
also backs General's AES-256-GCM/PBKDF2 file and memory encryption APIs. The installed
package does not install `httplib.h` as a public header and consumers should include only
`sindre/general/network.h`. OpenSSL remains a transitive link dependency for the static
General target because HTTPS and cryptographic code are compiled into the runtime.

The General dependency registry is split into a common manifest and platform
profiles:

```text
thirds/general/Dependencies.cmake
thirds/general/common.cmake
thirds/general/win/Dependencies.cmake
thirds/general/linux/Dependencies.cmake
```

The fixed General source trees are provisioned under the ignored
`thirds/general/sources` cache. The fixed binary package set for RE2, Crashpad,
zlib and OpenSSL is provisioned under the ignored `thirds/general/packages`
cache and selected before any host package path. These generated caches are not
part of the repository's core source and must not be committed.
Crashpad is linked through the `crashpad::crashpad` target when available.

The fixed argparse source is used only by the private CLI backend when compiler exception
support is enabled. A build configured with `SINDRE_NO_EXCEPTIONS=ON` enables the compiler
no-exceptions flags and uses General's internal no-exception CLI parser instead; this does
not change the public CLI API or its parsing semantics.

The General dependency versions and package metadata are registered in
`thirds/general/common.cmake`. CMake selects a fixed platform profile from
`thirds/general/win/Dependencies.cmake` or
`thirds/general/linux/Dependencies.cmake` with `NO_DEFAULT_PATH`; system/vcpkg
installations cannot silently replace it.

| Platform | Profile | Triplet | Fixed Crashpad | Provisioned packages |
| --- | --- | --- | --- | --- |
| Windows/MSVC | `general-x64-windows-static` | `x64-windows-static` | `2022-09-05#5` | RE2, Crashpad, OpenSSL, zlib |
| Linux/WSL | `general-x64-linux` | `x64-linux` | `2022-09-05#5` | RE2, Crashpad, OpenSSL, zlib |

The platform profiles are provisioned below `thirds/general/packages/` and are
intentionally ignored by Git because they contain generated binaries. The
profile directory is part of the local build environment and must exist before
CMake configuration. The Linux profile was provisioned with the fixed vcpkg
2024.04.23 baseline; the Windows profile uses the matching fixed package set.
Only Windows and Linux/WSL profiles are distributed; other host platforms are
rejected during configuration.

Desktop helpers do not add a compiled third-party dependency. Windows uses the platform
Win32/COM libraries already linked by General. Linux desktop helpers discover optional
runtime commands such as `notify-send`, `zenity`, `kdialog`, `wl-copy`, `xclip`, `pkexec`
and `yad`; missing commands produce `function_not_supported` rather than a configure-time
failure. These commands are executed with argument vectors, never through a shell.

The fixed Windows package profile uses static libraries and the static MSVC
runtime with `_ITERATOR_DEBUG_LEVEL=0`. General applies the same ABI to Debug
and Release consumers.

The installed Windows static package exports the `/MT` and iterator settings
through `sindre::general`. It also maps Debug, RelWithDebInfo, and MinSizeRel
dependency configurations to the shipped Release profile, because the fixed
third-party archives are distributed as one consistent static ABI. A consumer
does not need to repeat these ABI flags manually.

安装后的 CMake consumer 仍需要让 OpenSSL、RE2、Crashpad、zlib
对应的固定包可被发现；Math consumer 还需要固定 OpenBLAS 的 CMake package；Windows 运行时还需要把
固定 vcpkg 包的 `bin` 加入 DLL 搜索路径。静态链接 sindrecpp 本身不等于把这些
第三方运行时 DLL 复制进宿主应用。
