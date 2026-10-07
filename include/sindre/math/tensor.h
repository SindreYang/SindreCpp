#pragma once

/// @file
/// @brief Eigen Tensor 的稳定 Math 入口和 PyTorch 风格布局别名。

#if !defined(SINDRE_WITH_MATH)
#error "Enable sindre::math through CMake before including this header."
#endif

#include <sindre/general/core.h>

#include <unsupported/Eigen/CXX11/Tensor>

namespace sindre::math {

/// @brief Eigen Tensor 的维度索引类型。
using TensorIndex = Eigen::Index;

/// @brief 固定 Rank、行主序的 Eigen Tensor。
///
/// Rank 是编译期确定的维度数量，具体维度大小在运行时确定。Math 统一使用
/// Eigen::RowMajor，使最后一个维度连续，便于与 PyTorch 的 CHW/NCHW 数据交换。
template <class Scalar, int Rank>
using Tensor = Eigen::Tensor<Scalar, Rank, Eigen::RowMajor>;

/// @brief 固定 Rank Tensor 的维度数组。
template <int Rank>
using TensorDimensions = Eigen::array<TensorIndex, Rank>;

/// @brief 固定 Rank Tensor 的非拥有内存映射。
template <class Scalar, int Rank>
using TensorMap = Eigen::TensorMap<Tensor<Scalar, Rank>>;

namespace tensor {

/// @brief PyTorch 风格的三维图像/特征图，轴顺序为 C、H、W。
template <class Scalar>
using CHW = ::sindre::math::Tensor<Scalar, 3>;

/// @brief PyTorch 风格的批量图像/特征图，轴顺序为 N、C、H、W。
template <class Scalar>
using NCHW = ::sindre::math::Tensor<Scalar, 4>;

/// @brief PyTorch 风格的五维视频/时序批量，轴顺序为 N、C、T、H、W。
template <class Scalar>
using NCTHW = ::sindre::math::Tensor<Scalar, 5>;

/// @brief 返回 CHW Tensor 的维度数组。
inline TensorDimensions<3> chw_shape(
    TensorIndex channels, TensorIndex height, TensorIndex width) noexcept {
    return {channels, height, width};
}

/// @brief 返回 NCHW Tensor 的维度数组。
inline TensorDimensions<4> nchw_shape(
    TensorIndex batch, TensorIndex channels,
    TensorIndex height, TensorIndex width) noexcept {
    return {batch, channels, height, width};
}

} // namespace tensor

} // namespace sindre::math
