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

All modules are enabled by default and are available through the unified `SindreCpp::SindreCpp` target. Each integration exposes common SindreCpp names and a `native` namespace for advanced use of its underlying library. Dear ImGui's windowing and rendering backends remain the responsibility of the host application. Eigen does not require MKL or OpenMP. The Python module requires Python development files; NumPy conversion helpers also require NumPy at runtime.

The dependency projects retain their own licenses; enabling an integration downloads the upstream project without copying or relicensing it.

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

All third-party dependencies are pinned to release tags and fetched by CMake. To reduce the dependency set, turn off individual `SINDRECPP_WITH_*` options before `FetchContent_MakeAvailable`; `SindreCpp::SindreCpp` then contains only the enabled modules. You can also continue linking an individual target such as `SindreCpp::Json`. The GUI module provides ImGui core/context support; the host application supplies rendering and window backends.

When SindreCpp is included with FetchContent, its own tests and examples default to OFF. When building SindreCpp directly, they default to ON.

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

See `examples/basic_usage.cpp` for the module APIs. Disable an integration at configure time with options such as `-DSINDRECPP_WITH_PYTHON=OFF`.

## License

MIT. See [LICENSE](LICENSE).
