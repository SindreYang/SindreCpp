#pragma once

/// @file
/// @brief 可选的 Python/NumPy C++ 互操作入口。

#if !defined(SINDRE_WITH_UTILS_PY)
#error "Enable SINDRE_WITH_UTILS_PY and link sindre::utils_py before including this header."
#endif

#include <sindre/math.h>
#include <sindre/utils_py/python.h>

// utils_py intentionally provides a convenient public pybind11 umbrella. These
// are the normal headers needed by embedded applications; implementation-only
// pybind11 internals are not part of this entry point.
#include <pybind11/embed.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

namespace py = ::pybind11;
