#pragma once

#include "data.h"
#include <array>

#include <vtkImageData.h>
#include <vtkType.h>

namespace sindre::utils_3d::core {

/// @brief 图像插值策略。
enum class ImageInterpolation { nearest, linear, cubic };

/// @brief VTK 体数据及其采样、滤波和重采样操作封装。
class Image : public Data {
  public:
    explicit Image(vtkImageData *image);
    explicit Image(const Data &data);
    Image(const Matrix &values, std::array<int, 3> dimensions,
          ::sindre::math::Vector3 spacing = ::sindre::math::Vector3::Ones(),
          ::sindre::math::Vector3 origin = ::sindre::math::Vector3::Zero());

    vtkImageData *image() const;
    std::array<int, 3> dimensions() const;
    std::array<int, 6> extent() const;
    ::sindre::math::Vector3 spacing() const;
    ::sindre::math::Vector3 origin() const;
    Matrix values() const;
    Image gaussian(::sindre::math::Vector3 sigma =
                       ::sindre::math::Vector3::Ones()) const;
    Image median(std::array<int, 3> kernel = {3, 3, 3}) const;
    Image threshold(double lower, double upper, double inside = 1, double outside = 0) const;
    Image shift_scale(double shift, double scale) const;
    Image normalize(double lower = 0, double upper = 1) const;
    Image cast(int vtk_scalar_type = VTK_UNSIGNED_CHAR) const;
    Image crop(std::array<int, 6> box, std::array<int, 3> stride = {1, 1, 1}) const;
    Image pad(std::array<int, 6> box, double value = 0) const;
    Image flip(int axis) const;
    Image resample(::sindre::math::Vector3 output_spacing,
                   ImageInterpolation interpolation = ImageInterpolation::linear) const;
    Image reslice(const ::sindre::math::Matrix4 &axes, std::array<int, 3> dimensions,
                  ::sindre::math::Vector3 spacing = ::sindre::math::Vector3::Ones(),
                  ::sindre::math::Vector3 origin = ::sindre::math::Vector3::Zero(),
                  ImageInterpolation interpolation = ImageInterpolation::linear) const;
    Image gradient(bool magnitude = false) const;
    Image laplacian() const;
    Image morphology(bool dilate = true, std::array<int, 3> kernel = {3, 3, 3}) const;
    Image connected_regions(bool largest = false) const;
};

} // namespace sindre::utils_3d::core
