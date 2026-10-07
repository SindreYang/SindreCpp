#pragma once

/// @file
/// @brief Utils_3d 统一入口，按编译开关导出网格、数据和显示能力。

#if !defined(SINDRE_WITH_UTILS_3D)
#error "Enable SINDRE_WITH_UTILS_3D and link sindre::utils_3d before including this header."
#endif

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <sindre/utils_3d/sindremesh.h>

namespace sindre::utils_3d {

using Vector2 = Eigen::Vector2d;
using Vector3 = Eigen::Vector3d;
using Matrix3 = Eigen::Matrix3d;
using Matrix4 = Eigen::Matrix4d;
namespace native = Eigen;

} // namespace sindre::utils_3d
