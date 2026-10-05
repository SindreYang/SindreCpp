#pragma once

#if !defined(SINDRECPP_WITH_UTILS3D)
#error "Enable SINDRECPP_WITH_UTILS3D and link SindreCpp::Utils3d before including this header."
#endif

#include <Eigen/Core>
#include <Eigen/Geometry>
#include "utils3d/sindremesh.hpp"
#include "utils3d/algorithm.hpp"
#if defined(SINDRECPP_UTILS3D_VTK_DATA)
#include "utils3d/sindredata.hpp"
#include "utils3d/sindreimage.hpp"
#if defined(SINDRECPP_UTILS3D_SHOW)
#include "utils3d/show_plot.hpp"
#endif
#endif

namespace sindrecpp::utils3d {

using Vector2 = Eigen::Vector2d;
using Vector3 = Eigen::Vector3d;
using Matrix3 = Eigen::Matrix3d;
using Matrix4 = Eigen::Matrix4d;
namespace native = Eigen;

} // namespace sindrecpp::utils3d
