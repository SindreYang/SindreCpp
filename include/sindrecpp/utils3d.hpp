#pragma once

#if !defined(SINDRECPP_WITH_UTILS3D)
#error "Enable SINDRECPP_WITH_UTILS3D and link SindreCpp::Utils3d before including this header."
#endif

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace sindrecpp::utils3d {

using Vector2 = Eigen::Vector2d;
using Vector3 = Eigen::Vector3d;
using Matrix3 = Eigen::Matrix3d;
using Matrix4 = Eigen::Matrix4d;
namespace native = Eigen;

} // namespace sindrecpp::utils3d
