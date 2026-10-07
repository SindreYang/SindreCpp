#pragma once

/// @file
/// @brief sindrecpp 所有已启用模块的聚合入口。

#if defined(SINDRE_WITH_GENERAL)
#include <sindre/general.h>
#endif
#if defined(SINDRE_WITH_UTILS_PY)
#include <sindre/utils_py.h>
#endif

#if defined(SINDRE_WITH_AI)
#include <sindre/ai.h>
#endif

#if defined(SINDRE_WITH_GUI)
#include <sindre/gui.h>
#endif
#if defined(SINDRE_WITH_UTILS_2D)
#include <sindre/utils_2d.h>
#endif
#if defined(SINDRE_WITH_UTILS_3D)
#include <sindre/utils_3d.h>
#endif
