#pragma once

/// @file
/// @brief 可选的 Python/NumPy C++ 互操作入口。

#if !defined(SINDRE_WITH_UTILS_PY)
#error "Enable SINDRE_WITH_UTILS_PY and link sindre::utils_py before including this header."
#endif

#include <sindre/utils_py/python.h>
