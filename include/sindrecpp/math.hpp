#pragma once

#if !defined(SINDRECPP_WITH_MATH)
#error "Enable SINDRECPP_WITH_MATH and link SindreCpp::Math before including this header."
#endif

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace sindrecpp::math {

using Vector2 = Eigen::Vector2d;
using Vector3 = Eigen::Vector3d;
using Matrix3 = Eigen::Matrix3d;
using Matrix4 = Eigen::Matrix4d;
namespace native = Eigen;

} // namespace sindrecpp::math
