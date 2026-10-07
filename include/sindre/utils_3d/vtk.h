#pragma once

/// @file
/// @brief VTK 相关公共类型的统一入口；按需启用数据、渲染和绘图扩展。
// Individual headers remain available for projects that need a smaller include surface.
#include "mesh.h"

#if defined(SINDRE_UTILS_3D_VTK_DATA)
#include "data.h"
#include "image.h"
#endif

#if defined(SINDRE_UTILS_3D_SHOW)
#include "show.h"
#if defined(SINDRE_UTILS_3D_VTK_DATA)
#include "plot.h"
#endif
#endif
