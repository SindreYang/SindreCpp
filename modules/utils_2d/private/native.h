#pragma once

#include <sindre/utils_2d.h>

#include <opencv2/core.hpp>

namespace sindre::utils_2d::detail {

inline int native_type(const Image &image) {
    return CV_MAKETYPE(CV_8U, image.channels);
}

inline cv::Mat to_native(const Image &image) {
    if (image.empty()) return {};
    return cv::Mat(image.height, image.width, native_type(image),
                   const_cast<std::uint8_t *>(image.pixels.data())).clone();
}

inline Image from_native(const cv::Mat &image) {
    if (image.empty() || image.dims != 2 || image.depth() != CV_8U) return {};
    Image result(image.rows, image.cols, image.channels());
    result.pixels.assign(image.data,
                         image.data + image.total() * image.elemSize());
    return result;
}

inline cv::Size to_native(Size value) { return {value.width, value.height}; }
inline cv::Rect to_native(Rect value) { return {value.x, value.y, value.width, value.height}; }
inline cv::Scalar to_native(Scalar value) {
    return {value.values[0], value.values[1], value.values[2], value.values[3]};
}
inline cv::Point2f to_native(Point2f value) { return {value.x, value.y}; }
inline cv::Point3f to_native(Point3f value) { return {value.x, value.y, value.z}; }
inline cv::Point to_native(Point value) { return {value.x, value.y}; }

} // namespace sindre::utils_2d::detail
