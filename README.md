# SindreCpp

Reusable C++17 utilities and integrations for everyday projects. SindreCpp provides a small dependency-free core and a one-stop target for common tools.

## Modules

SindreCpp follows the same capability domains as Sindre:

| CMake option | Domain target | Header | Scope |
| --- | --- | --- | --- |
| `SINDRECPP_WITH_GENERAL` | `SindreCpp::General` | `<sindrecpp/general.hpp>` | Common utilities, strings, logging, HTTP, JSON, and CLI |
| `SINDRECPP_WITH_UTILS3D` | `SindreCpp::Utils3d` | `<sindrecpp/utils3d.hpp>` | 3D/math primitives backed by Eigen |
| `SINDRECPP_WITH_GUI` | `SindreCpp::Gui` | `<sindrecpp/gui.hpp>` | Dear ImGui context lifetime helper |
| `SINDRECPP_WITH_PYTHON` | `SindreCpp::Python` | `<sindrecpp/python.hpp>` | Python embedding and NumPy conversion |

The domain targets are the normal public entry points. Focused headers such as `string.hpp`, `log.hpp`, and `json.hpp` remain available inside the enabled `general` domain when a consumer needs a smaller include surface.

All modules are enabled by default and are available through the unified `SindreCpp::SindreCpp` target. Each integration exposes common SindreCpp names and a `native` namespace for advanced use of its underlying library. Dear ImGui's windowing and rendering backends remain the responsibility of the host application. Eigen enables its compiler-supported vectorization by default; use a Release build for optimized code. `SINDRECPP_UTILS3D_NATIVE_ARCH=ON` opts into CPU-specific compiler flags and can make the resulting binary incompatible with other machines. The Python module requires Python development files; NumPy conversion helpers also require NumPy at runtime.

When a supported dependency target is already present in the parent build or installed with CMake, SindreCpp reuses it. Otherwise CMake fetches the pinned upstream release. Upstream test/example options are set only in the dependency's scope, and existing parent settings take precedence. The dependency projects retain their own licenses; SindreCpp does not copy or relicense them.

## Add with FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    SindreCpp
    GIT_REPOSITORY https://github.com/SindreYang/SindreCpp.git
    GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(SindreCpp)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE SindreCpp::SindreCpp)
```

```cpp
#include <sindrecpp/sindrecpp.hpp>

#include <string>

int main() {
    auto label = sindrecpp::string::trim("  daily utility  ");
    sindrecpp::log::info("Starting {}", std::string(label));
    auto document = sindrecpp::json::parse(R"({"ready":true})");
    return document.root()["ready"].get_bool().value() ? 0 : 1;
}
```

To reduce the dependency set, turn off individual `SINDRECPP_WITH_*` options before `FetchContent_MakeAvailable`; `SindreCpp::SindreCpp` then contains only the enabled modules. You can also continue linking an individual target such as `SindreCpp::Json`. The GUI module provides ImGui core/context support; the host application supplies rendering and window backends.

HTTP enables `SINDRECPP_HTTP_OPENSSL` auto-detection by default. If OpenSSL is available, cpp-httplib adds HTTPS support; otherwise it remains HTTP-only. Eigen automatically uses the SIMD features enabled by the compiler target. Build in Release mode for optimized code. `SINDRECPP_UTILS3D_NATIVE_ARCH=ON` is the default and enables CPU-specific compiler flags for maximum build-machine performance; set it to `OFF` when distributing binaries to other CPU models. The setting propagates to code that uses Eigen. Alignment macros remain at Eigen's defaults to avoid ABI mismatches with other libraries.

Eigen's dense operations use `SINDRECPP_UTILS3D_BLAS_BACKEND=AUTO` by default: if CMake finds a host BLAS, SindreCpp links it and enables Eigen's BLAS integration; otherwise Eigen uses its built-in kernels. Set the option to `EIGEN` to force built-in kernels or `BLAS` to require an external BLAS. To select an installed vendor, pass CMake's `BLA_VENDOR`, for example `-DBLA_VENDOR=OpenBLAS` or `-DBLA_VENDOR=Intel10_64lp`. SindreCpp does not download or build a second BLAS implementation. Eigen routes eligible dynamic/large dense products through BLAS; most other Eigen operations still use Eigen's own algorithms.

### Utils3d/BLAS benchmark

The optional benchmark compares Eigen's built-in double-precision matrix multiplication against the selected BLAS `dgemm`, using the same seeded matrices and single-thread settings. It reports average time, GFLOP/s, speedup, maximum absolute error, and relative L2 error. Build it in Release mode with a BLAS installed:

```powershell
$env:OPENBLAS_NUM_THREADS = "1"
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release `
  -DSINDRECPP_BUILD_UTILS3D_BENCHMARKS=ON `
  -DSINDRECPP_UTILS3D_BLAS_BACKEND=EIGEN `
  -DSINDRECPP_UTILS3D_NATIVE_ARCH=ON `
  -DBLA_VENDOR=OpenBLAS
cmake --build build-bench --config Release
.\build-bench\sindrecpp_utils3d_benchmark.exe 1024 5
```

`SINDRECPP_UTILS3D_BLAS_BACKEND=EIGEN` keeps Eigen's built-in path active in the library for this comparison; the benchmark still links the detected BLAS directly. Set `OPENBLAS_NUM_THREADS=1` (or the equivalent vendor thread control) for a single-thread comparison. Results depend on CPU, compiler, BLAS build, and threading configuration.

### Rotating log file

The log module includes a ready-to-use size-rotating logger. Defaults are 10 MiB per file and five retained files:

```cpp
auto file_log = sindrecpp::log::rotating_file("app", "logs/app.log");
file_log->info("Started {}", "MyApp");
```

Pass `max_size_bytes` and `max_files` to change the rotation limits. The returned logger is named and does not replace spdlog's global default logger.

When SindreCpp is included with FetchContent, its own tests and examples default to OFF. When building SindreCpp directly, they default to ON.

```cpp
#include <sindrecpp/core.hpp>
#include <sindrecpp/string.hpp>

auto label = sindrecpp::string::trim("  daily utility  ");
auto config = sindrecpp::Result<std::string>::success(std::string(label));
```

`Result<T>` represents success or an `Error`; `value()` reads the result and `error()` reads its failure details. String helpers operate on UTF-8 byte sequences for ASCII-compatible operations. Use `sindrecpp::string::Utf8String` for CsString's Unicode-aware API when the String module is enabled.

## Project conventions

SindreCpp follows the Sindre naming and organization style while respecting normal C++ conventions:

- namespaces, functions, and variables use lower snake case;
- public types use PascalCase;
- CMake targets use `SindreCpp::Name`;
- feature options use `SINDRECPP_WITH_NAME`;
- modules follow Sindre capability domains such as `general`, `utils2d`, `utils3d`, `utilsav`, `ai`, `deploy`, `platform`, and `apps`;
- modules are named by user-facing capability, not by the underlying dependency;
- advanced access to a wrapped dependency is exposed through a module's `native` namespace;
- workflow states use `start` and `done`, not `complete`;
- functionality stays concentrated in capability-level headers instead of one header per function.

See [docs/development.md](docs/development.md) for the complete development guide and [docs/notes.md](docs/notes.md) for dependency, portability, and API design notes.

## Local build

Requirements: CMake 3.20+, a C++17 compiler, and Git when enabling third-party modules.

```powershell
cmake -S . -B build -DSINDRECPP_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

See `examples/basic_usage.cpp` for the module APIs. Disable an integration at configure time with options such as `-DSINDRECPP_WITH_PYTHON=OFF`.

## License

MIT. See [LICENSE](LICENSE).
