# utils_py module

本文面向需要在 C++ 中嵌入 Python、调用 NumPy 或交换网格数据的使用者，介绍运行时
初始化、公共入口和编译方式；依赖要求见 [Utils_Py 依赖说明](../dependencies/utils_py.md)。

`utils_py` provides the optional Python/NumPy bridge for sindre. It is kept
outside `General` because it requires a Python development installation and
pybind11.

The module is a compiled shared library. It is disabled by default and is
enabled with `SINDRE_WITH_UTILS_PY=ON`. The supported runtime is Python 3.12+
with matching development headers and libraries. NumPy is required only when
using the NumPy conversion helpers.

## Public entry point

```cpp
#include <sindre/utils_py.h>

auto interpreter = sindre::utils_py::Interpreter::create();
if (!interpreter) {
    // interpreter.error() is sindre::general::Error
    return 1;
}
auto gil = interpreter.value()->get_gil();
auto array = sindre::utils_py::array_from_vector(std::vector<double>{1.0, 2.0});
```

跨线程执行 Python 代码时可以让模块自动管理 GIL，并统一接收错误：

```cpp
auto result = interpreter.value()->run_with_gil([&] {
    return pybind11::module_::import("numpy").attr("__version__").cast<std::string>();
});
if (!result) {
    // result.error() is sindre::general::Error
    return 1;
}
```

需要让出 GIL 执行原生计算时使用 RAII 释放守卫；守卫析构时会自动重新
获取 GIL：

```cpp
auto gil = interpreter.value()->get_gil();
{
    auto released = interpreter.value()->get_gil_release();
    do_native_work();
}
use_python_objects_again();
```

`pybind11` 对象的创建、使用和销毁都应位于 `GilGuard` 作用域内。

`utils_py` is a compiled shared library. `get_runtime()` uses CPython's
`PyPreConfig` and `PyConfig` internally and initializes or attaches to the
process-wide interpreter exactly once. `Interpreter::create()` is available
when the host explicitly owns initialization.
For a deterministic embedded runtime, pass only the paths that differ from
the host default; the wrapper performs normalization and Unicode-safe path
conversion:

```cpp
sindre::utils_py::InterpreterConfig config;
config.python_executable = LR"(D:\运行时\Python312\python.exe)";
config.python_home = LR"(D:\运行时\Python312)";

auto interpreter = sindre::utils_py::Interpreter::create(config);
if (!interpreter) {
    // No CPython exception crosses this initialization boundary.
    return 1;
}
```

For applications and DLL plugins, use the automatic process-wide entry
point. The first caller initializes Python; later callers attach to the same
runtime without finalizing it when a DLL is unloaded:

```cpp
auto runtime = sindre::utils_py::get_runtime(config);
if (!runtime) return 1;
auto gil = runtime.value()->get_gil();
```

For isolated embedding, pass both `python_executable` and `python_home` when
the host does not already own an initialized interpreter. `python_home` should
identify the CPython installation containing the standard library; for a uv
virtual environment this is normally the environment's `sys.base_prefix`, not
the `.venv` directory itself. `module_search_paths` is optional, but when it
is supplied it becomes the complete CPython search list and must include the
standard library and any required site-packages directories. If the host has
already initialized CPython, `get_runtime()` can attach without these paths.
Each process must own its own Python interpreter; Python objects must not be
passed between processes.

## Build and test

Create the virtual environment and install the runtime packages with uv:

```powershell
uv venv --python 3.12 .venv
uv pip install --python .venv\Scripts\python.exe numpy pybind11
```

Configure the module with the same Python used by the virtual environment.
The explicit `pybind11_DIR` is needed when pybind11 was installed by uv:

```powershell
cmake -S . -B build_win -G Ninja `
  -DPython3_EXECUTABLE="F:/My_Github/SindreCpp/.venv/Scripts/python.exe" `
  -Dpybind11_DIR="F:/My_Github/SindreCpp/.venv/Lib/site-packages/pybind11/share/cmake/pybind11" `
  -DSINDRE_WITH_UTILS_PY=ON -DSINDRE_BUILD_TESTS=ON
cmake --build build_win --parallel 4
```

The runtime test does not require VTK or `utils_3d`. If `SINDRE_WITH_UTILS_3D=ON`
is also enabled, the additional NumPy/mesh conversion test is built.

The Windows clang-cl verification used uv-managed Python 3.12.9, NumPy 2.5.3
and pybind11 3.1.0. Both the embedded runtime test and the NumPy/mesh exchange
test passed. This module remains a shared library because it embeds CPython;
the other C++ modules can stay static without changing this boundary.

The public CMake target is `sindre::utils_py`, enabled with
`SINDRE_WITH_UTILS_PY=ON`. The namespace is `sindre::utils_py`, matching the
module directory and public include path.

NumPy is imported by the caller's Python environment at runtime. Mesh/Math
conversion helpers are available when `SINDRE_WITH_UTILS_3D=ON` is enabled
for the consuming target as well.
