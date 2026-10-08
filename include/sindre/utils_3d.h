#pragma once

/// @file
/// @brief Utils_3d 统一入口，导出网格、点云和后端无关算法。

#if !defined(SINDRE_WITH_UTILS_3D)
#error "Enable SINDRE_WITH_UTILS_3D and link sindre::utils_3d before including this header."
#endif

#include <sindre/math.h>
#include <sindre/utils_3d/types.h>
#include <sindre/utils_3d/algorithms/mesh.h>
#include <sindre/utils_3d/algorithms/point_cloud.h>
#include <sindre/utils_3d/algorithms/segmentation.h>
#include <sindre/utils_3d/algorithms/feature_smoothing.h>
#include <sindre/utils_3d/algorithms/fgcf.h>
#include <sindre/utils_3d/algorithms/nearest_neighbors.h>
#include <sindre/utils_3d/sindremesh.h>

namespace sindre::utils_3d {

using Vector2 = ::sindre::math::Vector2;
using Vector3 = ::sindre::math::Vector3;
using Matrix3 = ::sindre::math::Matrix3;
using Matrix4 = ::sindre::math::Matrix4;

} // namespace sindre::utils_3d
