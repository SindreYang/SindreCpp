# General dependencies

本文说明基础模块的固定 General 聚合依赖。General 是基础库，以下依赖
全部必需，不能通过 `SINDRE_WITH_*` 关闭或替换。

General is the foundation module. Its integrations below are mandatory.
Eigen and OpenBLAS belong to the separate Math foundation; see
[`Math dependencies`](math.md).

| Feature | Dependency | Source | Version | Required |
| --- | --- | --- | --- | --- |
| string | [CsString](https://github.com/copperspice/cs_string) | fixed ExternalProject shared runtime | `string-1.4.1` | yes |
| logging | [spdlog](https://github.com/gabime/spdlog) | fixed ExternalProject static runtime | `v1.17.0` | yes |
| HTTP/HTTPS and encryption | [cpp-httplib](https://github.com/yhirose/cpp-httplib) + OpenSSL | fixed source + fixed package | `v0.56.0` / OpenSSL 3.3.0 | yes |
| JSON/config | [simdjson](https://github.com/simdjson/simdjson) | fixed ExternalProject static runtime | `v4.6.11` | yes |
| CLI | [argparse](https://github.com/p-ranav/argparse) | fixed ExternalProject header source | `v3.2` | yes |
| regular expressions | [RE2](https://github.com/google/re2) + Abseil | fixed source ExternalProject; Abseil is private RE2 build dependency | `2024-04-01` / `20240116.2` | yes |
| crash reporting | [Crashpad](https://chromium.googlesource.com/crashpad/crashpad/) | fixed platform package under external third-party cache | `2022-09-05#5` | yes |
| compression | [zlib](https://zlib.net/) | fixed source ExternalProject | `1.3.1` | yes |

cpp-httplib and OpenSSL are implementation dependencies of `sindre_general_runtime`. OpenSSL
also backs General's AES-256-GCM/PBKDF2 file and memory encryption APIs. The installed
package does not install `httplib.h` as a public header and consumers should include only
`sindre/general/network.h`. OpenSSL remains a transitive link dependency for the static
General target because HTTPS and cryptographic code are compiled into the runtime.

The General dependency registry is split into a common manifest and platform
profiles:

```text
3rdparty/find_dependencies.cmake
3rdparty/general.cmake
3rdparty/cs_string/cs_string.cmake
3rdparty/spdlog/spdlog.cmake
3rdparty/cpp_httplib/cpp_httplib.cmake
3rdparty/simdjson/simdjson.cmake
3rdparty/argparse/argparse.cmake
3rdparty/abseil/abseil.cmake
3rdparty/re2/re2.cmake
3rdparty/openssl/openssl.cmake
3rdparty/zlib/zlib.cmake
3rdparty/crashpad/crashpad.cmake
```

The fixed General source trees are downloaded and built through isolated
ExternalProject targets under the ignored build/cache directories selected by `SINDRE_THIRD_PARTY_CACHE_DIR`. RE2, its private Abseil dependency,
and zlib are always built from the pinned source archives. The fixed binary package set is limited to Crashpad and OpenSSL and is selected before any host package path. These generated caches are not
part of the repository's core source and must not be committed.
Crashpad is linked through the `crashpad::crashpad` target when available.

ExternalProject recipes force their child projects to Release, including when
the top-level generator is multi-configuration (Visual Studio or Ninja
Multi-Config). This keeps the generated install tree deterministic and avoids
an unqualified child install falling back to Debug or a machine-wide prefix.

On Linux, the fixed Abseil archives are exported through a linker rescan group.
This is required for the static Abseil component graph and prevents consumers
from depending on the incidental archive order produced by a particular
linker. Windows uses the normal static archive targets.

The fixed argparse source is used only by the private CLI backend when compiler exception
support is enabled. A build configured with `SINDRE_NO_EXCEPTIONS=ON` enables the compiler
no-exceptions flags and uses General's internal no-exception CLI parser instead; this does
not change the public CLI API or its parsing semantics. The option applies strictly to
Sindre's own targets. Fixed third-party projects keep their upstream exception ABI because
some upstream headers, notably CsString, contain required `throw` expressions; General
converts their failures at its public `Result` boundaries. spdlog additionally receives
`SPDLOG_NO_EXCEPTIONS` because its upstream build supports that dedicated mode.

The General dependency versions and package metadata are registered in
`3rdparty/general.cmake`. CMake selects the fixed Windows or Linux profile there
with `NO_DEFAULT_PATH`; system/vcpkg installations cannot silently replace it.
The Windows profile is `general-x64-windows-static` with the `x64-windows-static`
triplet. Its static CRT and iterator ABI must match the consumer; clang-cl builds
also pass the parent Windows SDK and CRT flags into every fixed ExternalProject.

| Platform | Profile | Triplet | Fixed Crashpad | Provisioned packages |
| --- | --- | --- | --- | --- |
| Windows/MSVC | `general-x64-windows-static` | `x64-windows-static` | `2022-09-05#5` | Crashpad, OpenSSL |
| Linux/WSL | `general-x64-linux` | `x64-linux` | `2022-09-05#5` | Crashpad, OpenSSL |

The platform profiles are provisioned below the repository-local
`SINDRE_THIRD_PARTY_CACHE_DIR` (the shared migrated cache is under `.sindre_cache/`).
They contain generated binaries and are ignored by Git; they must exist before
CMake configuration. The Linux profile was provisioned with the fixed vcpkg
2024.04.23 baseline; the Windows profile uses the matching fixed package set.
Only Windows and Linux/WSL profiles are distributed; other host platforms are
rejected during configuration.

For a repository-local shared or CI cache, set
`SINDRE_THIRD_GENERAL_PACKAGE_CACHE_ROOT` explicitly to the directory containing
`general-x64-windows-static/installed/x64-windows-static` (or the Linux profile).
This is the only General dependency input that may be supplied from a fixed large
package manager profile; it must not be confused with the small source dependency
cache controlled by `SINDRE_THIRD_PARTY_CACHE_DIR`.

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

安装后的 CMake consumer 仍需要让 OpenSSL 和 Crashpad 对应的固定包可被发现；RE2、Abseil
和 zlib 的静态库由安装包自身携带，不再要求用户安装 vcpkg 或系统开发包。Math 的
OpenBLAS 静态库和头文件也由安装包自身携带；Windows 运行时还需要把固定大包的 `bin` 加入 DLL
搜索路径。静态链接 sindrecpp 本身不等于把这些第三方运行时 DLL 复制进宿主应用。
