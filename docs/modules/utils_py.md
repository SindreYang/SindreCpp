# utils_py module

本文面向需要在 C++ 中嵌入 Python、调用 NumPy 或交换网格数据的使用者，介绍运行时
初始化、公共入口和编译方式；依赖要求见 [Utils_Py 依赖说明](../dependencies/utils_py.md)。

`utils_py` provides the optional Python/NumPy bridge for sindre. It is kept
outside `General` because it requires a Python development installation and
pybind11. pybind11 and CPython are implementation details: the public header
does not expose pybind11, NumPy, Eigen, or Python object types.

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
auto values = sindre::utils_py::vector_from_array<double>(array);
```

跨线程执行 Python 代码时可以让模块自动管理 GIL，并统一接收错误：

```cpp
auto result = interpreter.value()->run_with_gil([&] {
    // Python-facing work is performed by the private implementation layer.
    return 40 + 2;
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

如果应用确实需要直接使用 pybind11，可以在应用自己的实现层引入它；
`sindre::utils_py` 的公共接口只负责解释器生命周期、GIL 和自有字节数组，
不会把第三方对象跨越 DLL 边界。

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
cmake --preset windows-clang-cl `
  -DPython3_EXECUTABLE="F:/My_Github/SindreCpp/.venv/Scripts/python.exe" `
  -Dpybind11_DIR="F:/My_Github/SindreCpp/.venv/Lib/site-packages/pybind11/share/cmake/pybind11" `
  -DSINDRE_WITH_UTILS_PY=ON -DSINDRE_BUILD_TESTS=ON
cmake --build --preset windows-clang-cl --parallel 4
```

The runtime test does not require VTK or `utils_3d`. The array bridge test also
uses only the public `Array` value type, so it does not require NumPy or
`utils_3d`.

The Windows clang-cl verification used uv-managed Python 3.12.9 and pybind11
3.1.0. The embedded runtime and public byte-array exchange tests passed. A
separate NumPy/mesh exchange API is no longer part of the public contract. This
module remains a shared library because it embeds CPython;
the other C++ modules can stay static without changing this boundary.

The public CMake target is `sindre::utils_py`, enabled with
`SINDRE_WITH_UTILS_PY=ON`. The namespace is `sindre::utils_py`, matching the
module directory and public include path.

NumPy remains an implementation/runtime dependency for applications that
embed NumPy, but the stable public exchange format is `Array` (dtype, shape,
and owned bytes). This keeps the C++ ABI independent of NumPy and Eigen.
