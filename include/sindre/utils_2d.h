#pragma once

/// @file
/// @brief OpenCV 图像、预处理和 2D 算法聚合入口。

#if !defined(SINDRE_WITH_UTILS_2D)
#error "Enable SINDRE_WITH_UTILS_2D and link sindre::utils_2d."
#endif

#include <sindre/general/core.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sindre::utils_2d {

using ::sindre::general::Result;

/// @brief OpenCV 图像类型别名；高级用户可直接使用 native 命名空间。
using Image = cv::Mat;
namespace native = cv;

/// @brief 校验图像非空且确实是二维图像。
Result<void> validate_image(const Image& image);
/// @brief 将平台路径转换为 OpenCV 可接受的 UTF-8 字符串。
std::string path_to_utf8(const std::filesystem::path& path);

/// @brief 从文件加载图像。
Result<Image> load_image(const std::string& path, int flags = cv::IMREAD_COLOR);
Result<Image> load_image(const std::filesystem::path& path, int flags = cv::IMREAD_COLOR);

/// @brief 将图像保存到文件。
Result<void> save_image(const Image& image, const std::string& path,
                        const std::vector<int>& options = {});
Result<void> save_image(const Image& image, const std::filesystem::path& path,
                        const std::vector<int>& options = {});

/// @brief 调整图像尺寸。
Result<Image> resize_image(const Image& image, cv::Size size,
                           int interpolation = cv::INTER_LINEAR);
/// @brief 裁剪矩形区域。
Result<Image> crop_image(const Image& image, cv::Rect region);
/// @brief 执行 OpenCV 颜色空间转换。
Result<Image> convert_color(const Image& image, int conversion);
/// @brief 按 scale 和 offset 对像素执行归一化。
Result<Image> normalize_image(const Image& image, double scale = 1.0 / 255.0,
                              double offset = 0.0);

struct Letterbox {
    Image image;
    float scale;
    int left;
    int top;
};

/// @brief 将图像等比缩放并填充到目标尺寸，记录缩放比例和边距。
Result<Letterbox> create_letterbox(const Image& image, cv::Size size,
                                   cv::Scalar color = cv::Scalar(114, 114, 114));

struct ImageTensor {
    std::vector<std::int64_t> shape; // NCHW, batch=1.
    std::vector<float> data;
};

/// @brief 轮廓及其常用几何属性。
struct ContourInfo {
    std::vector<cv::Point> points;
    double area = 0.0;
    double perimeter = 0.0;
    cv::Rect bounding_box;
    cv::RotatedRect minimum_box;
    bool convex = false;
};

/// @brief 将 BGR 图像转换为 NCHW float tensor。
/// @details 默认转换为 RGB，归一化公式为 (pixel * scale - mean) / deviation。
Result<ImageTensor> convert_to_tensor(const Image& image, bool rgb = true,
                                      float scale = 1.f / 255.f,
                                      cv::Scalar mean = {},
                                      cv::Scalar deviation = cv::Scalar(1, 1, 1, 1));

/// @brief 可用的平滑滤波器。
enum class BlurAlgorithm { gaussian, median, bilateral };
/// @brief 可用的固定、自适应和自动阈值算法。
enum class ThresholdAlgorithm {
    binary,
    binary_inverse,
    trunc,
    to_zero,
    to_zero_inverse,
    otsu,
    triangle,
    adaptive_mean,
    adaptive_gaussian
};
/// @brief 形态学操作类型。
enum class MorphologyOperation { erode, dilate, open, close, gradient, top_hat, black_hat };
/// @brief 边缘检测算法。
enum class EdgeAlgorithm { canny, sobel, scharr, laplacian };

} // namespace sindre::utils_2d

#include <sindre/utils_2d/algorithms.h>
#include <sindre/utils_2d/image.h>
