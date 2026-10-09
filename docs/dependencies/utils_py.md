# utils_py third-party dependencies

本文说明 Python/NumPy 桥接的编译期和运行时依赖。只有需要嵌入 Python 或进行
NumPy/网格交换时才需要启用此模块。

| Dependency | Source | Version | Default |
| --- | --- | --- | --- |
| [pybind11](https://github.com/pybind/pybind11) | fixed source archive with SHA256 | `v3.1.0` | off |

`utils_py` also requires Python 3.12 or newer development headers/libraries and
NumPy in the runtime environment. It is a compiled shared library, not a
header-only module. CMake first uses an installed/configured pybind11 and
falls back to the pinned, SHA256-verified source archive only when necessary.
Python's library is resolved from the `FindPython3` development result first,
then from the interpreter's standard library directories; Linux is therefore
not restricted to a library located beside `python3` in `/usr/bin`.

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

On Windows, CMake resolves `sys.base_prefix` and copies the matching
`python312.dll` beside embedded test and module targets. uv places `python.exe`
under `.venv\\Scripts`, while the interpreter DLL remains in the base CPython
installation. Linux uses the same base-prefix discovery for runtime
configuration and does not require the shared library beside the virtual
environment executable.

The Ubuntu 24.04/Clang 18 profile was built with the system Python 3.12
development package and passed both the embedded-runtime and NumPy tests.
