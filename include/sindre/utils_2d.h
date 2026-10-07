#pragma once

#if !defined(SINDRE_WITH_UTILS_2D)
#error "Enable SINDRE_WITH_UTILS_2D and link sindre::utils_2d."
#endif

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace sindre::utils_2d {

/// @brief OpenCV 图像类型别名；高级用户可直接使用 native 命名空间。
using Image = cv::Mat;
namespace native = cv;

/// @brief 校验图像非空并抛出包含上下文的异常。
void validate(const Image& image);
/// @brief 将平台路径转换为 OpenCV 可接受的 UTF-8 字符串。
std::string path_to_utf8(const std::filesystem::path& path);

/// @brief 从文件加载图像。
Image load_image(const std::string& path, int flags = cv::IMREAD_COLOR);
Image load_image(const std::filesystem::path& path, int flags = cv::IMREAD_COLOR);

/// @brief 将图像保存到文件。
void save_image(const Image& image, const std::string& path,
                const std::vector<int>& options = {});
void save_image(const Image& image, const std::filesystem::path& path,
                const std::vector<int>& options = {});

/// @brief 调整图像尺寸。
Image resize_image(const Image& image, cv::Size size,
                   int interpolation = cv::INTER_LINEAR);
/// @brief 裁剪矩形区域。
Image crop_image(const Image& image, cv::Rect region);
/// @brief 执行 OpenCV 颜色空间转换。
Image convert_color(const Image& image, int conversion);
/// @brief 按 scale 和 offset 对像素执行归一化。
Image normalize_image(const Image& image, double scale = 1.0 / 255.0,
                      double offset = 0.0);

struct Letterbox {
    Image image;
    float scale;
    int left;
    int top;
};

/// @brief 将图像等比缩放并填充到目标尺寸，记录缩放比例和边距。
Letterbox create_letterbox(const Image& image, cv::Size size,
                           cv::Scalar color = cv::Scalar(114, 114, 114));

struct ImageTensor {
    std::vector<std::int64_t> shape; // NCHW, batch=1.
    std::vector<float> data;
};

/// @brief 将 BGR 图像转换为 NCHW float tensor。
/// @details 默认转换为 RGB，归一化公式为 (pixel * scale - mean) / deviation。
ImageTensor convert_to_tensor(const Image& image, bool rgb = true,
                              float scale = 1.f / 255.f,
                              cv::Scalar mean = {},
                              cv::Scalar deviation = cv::Scalar(1, 1, 1, 1));

// Transitional names kept for source compatibility with the current API.
Image load(const std::string& path, int flags = cv::IMREAD_COLOR);
Image load(const std::filesystem::path& path, int flags = cv::IMREAD_COLOR);
void save(const Image& image, const std::string& path, const std::vector<int>& options = {});
void save(const Image& image, const std::filesystem::path& path,
          const std::vector<int>& options = {});
Image resize(const Image& image, cv::Size size, int interpolation = cv::INTER_LINEAR);
Image crop(const Image& image, cv::Rect region);
Image change_color(const Image& image, int conversion);
Image normalize(const Image& image, double scale = 1.0 / 255.0, double offset = 0.0);
Letterbox letterbox(const Image& image, cv::Size size,
                    cv::Scalar color = cv::Scalar(114, 114, 114));
ImageTensor to_tensor(const Image& image, bool rgb = true, float scale = 1.f / 255.f,
                      cv::Scalar mean = {},
                      cv::Scalar deviation = cv::Scalar(1, 1, 1, 1));

} // namespace sindre::utils_2d
