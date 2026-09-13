# SindreCpp

Reusable C++17 utilities and optional integrations for everyday projects. SindreCpp provides a small dependency-free core and independent modules for common tasks; enable only the integrations your project uses.

## Modules

| CMake option | Target | Header | Integration |
| --- | --- | --- | --- |
| `SINDRECPP_WITH_POINTER` | `SindreCpp::Pointer` | `<sindrecpp/pointer.hpp>` | CsPointer 1.x and standard smart-pointer helpers |
| `SINDRECPP_WITH_STRING` | `SindreCpp::String` | `<sindrecpp/string.hpp>` | CsString 1.x and common string operations |
| `SINDRECPP_WITH_LOG` | `SindreCpp::Log` | `<sindrecpp/log.hpp>` | spdlog |
| `SINDRECPP_WITH_GUI` | `SindreCpp::Gui` | `<sindrecpp/gui.hpp>` | Dear ImGui context lifetime helper |
| `SINDRECPP_WITH_PYTHON` | `SindreCpp::Python` | `<sindrecpp/python.hpp>` | pybind11 embedding and owned NumPy conversion helpers |
| `SINDRECPP_WITH_HTTP` | `SindreCpp::Http` | `<sindrecpp/http.hpp>` | cpp-httplib |
| `SINDRECPP_WITH_JSON` | `SindreCpp::Json` | `<sindrecpp/json.hpp>` | simdjson |
| `SINDRECPP_WITH_CLI` | `SindreCpp::Cli` | `<sindrecpp/cli.hpp>` | argparse |
| `SINDRECPP_WITH_EIGEN` | `SindreCpp::Eigen` | `<sindrecpp/eigen.hpp>` | Eigen |

The optional modules are disabled by default. Each integration exposes common SindreCpp names and a `native` namespace for advanced use of its underlying library. Dear ImGui's windowing and rendering backends remain the responsibility of the host application. Eigen does not require MKL or OpenMP.

The dependency projects retain their own licenses; enabling an integration downloads the upstream project without copying or relicensing it.

## Add with FetchContent

```cmake
include(FetchContent)

set(SINDRECPP_WITH_JSON ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    SindreCpp
    GIT_REPOSITORY https://github.com/SindreYang/SindreCpp.git
    GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(SindreCpp)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE SindreCpp::Json)
```

SindreCpp fetches only dependencies for enabled modules and pins each dependency to a release tag. Set feature options before `FetchContent_MakeAvailable`.

```cpp
#include <sindrecpp/core.hpp>
#include <sindrecpp/string.hpp>

auto label = sindrecpp::string::trim("  daily utility  ");
auto config = sindrecpp::Result<std::string>::success(std::string(label));
```

`Result<T>` represents success or an `Error`; `value()` reads the result and `error()` reads its failure details. String helpers operate on UTF-8 byte sequences for ASCII-compatible operations. Use `sindrecpp::string::Utf8String` for CsString's Unicode-aware API when the String module is enabled.

## Local build

Requirements: CMake 3.20+, a C++17 compiler, and Git when enabling third-party modules.

```powershell
cmake -S . -B build -DSINDRECPP_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Enable modules with options such as `-DSINDRECPP_WITH_LOG=ON`. Python integration also requires Python development headers and libraries; NumPy must be installed to use the NumPy conversion helpers. See `examples/basic_usage.cpp` for the module APIs.

## License

MIT. See [LICENSE](LICENSE).
