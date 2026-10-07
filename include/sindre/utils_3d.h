#pragma once

/// @file
/// @brief Utils_3d 统一入口，按编译开关导出网格、数据和显示能力。

#if !defined(SINDRE_WITH_UTILS_3D)
#error "Enable SINDRE_WITH_UTILS_3D and link sindre::utils_3d before including this header."
#endif

#include <sindre/math.h>
#include <sindre/utils_3d/sindremesh.h>

namespace sindre::utils_3d {

using Vector2 = ::sindre::math::Vector2;
using Vector3 = ::sindre::math::Vector3;
using Matrix3 = ::sindre::math::Matrix3;
using Matrix4 = ::sindre::math::Matrix4;
namespace native = ::sindre::math::eigen;

} // namespace sindre::utils_3d
