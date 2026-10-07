# utils_py third-party dependencies

本文说明 Python/NumPy 桥接的编译期和运行时依赖。只有需要嵌入 Python 或进行
NumPy/网格交换时才需要启用此模块。

| Dependency | Source | Version | Default |
| --- | --- | --- | --- |
| [pybind11](https://github.com/pybind/pybind11) | Git | `v3.1.0` | off |

`utils_py` also requires Python 3.12 or newer development headers/libraries and
NumPy in the runtime environment. It is a compiled shared library, not a
header-only module. CMake first uses an installed/configured pybind11 and
falls back to the pinned Git source only when necessary.

The public target is `sindre::utils_py`. Applications and DLL plugins should
use `sindre::utils_py::get_runtime()` so initialization and attachment are
process-wide. Runtime configuration accepts filesystem paths for the Python
executable, home and module search paths.

The supported local workflow is to create the environment with uv, then pass
the same interpreter and its pybind11 CMake directory to CMake. Do not use the
WindowsApps `python` alias as evidence that a Python development installation
exists. For an embedded interpreter, `python_home` should point to the CPython
base installation; a uv virtual environment's `.venv` directory is not normally
the base home.
