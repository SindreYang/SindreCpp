#pragma once

/// @file
/// @brief 不暴露 OpenCV 的 2D 图像和预处理 facade。

#if !defined(SINDRE_WITH_UTILS_2D)
#error "Enable SINDRE_WITH_UTILS_2D and link sindre::utils_2d."
#endif

#include <sindre/general/core.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sindre::utils_2d {

using ::sindre::general::Result;

struct Size {
    int width = 0;
    int height = 0;
};

struct Point {
    int x = 0;
    int y = 0;
};

struct Point2f {
    float x = 0.0f;
    float y = 0.0f;
};

struct Point3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct Rect2f {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct Scalar {
    double values[4] = {0.0, 0.0, 0.0, 0.0};

    constexpr Scalar() = default;
    constexpr Scalar(double value) : values{value, value, value, value} {}
    constexpr Scalar(double first, double second, double third,
                     double fourth = 0.0)
        : values{first, second, third, fourth} {}
    constexpr double operator[](std::size_t index) const { return values[index]; }
};

/// @brief 由模块拥有的连续 8-bit 图像数据。
struct Image {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<std::uint8_t> pixels;

    Image() = default;
    Image(int image_height, int image_width, int image_channels,
          std::vector<std::uint8_t> image_pixels = {});

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] Size size() const noexcept { return {width, height}; }
    [[nodiscard]] std::size_t byte_size() const noexcept { return pixels.size(); }
    [[nodiscard]] const std::uint8_t *data() const noexcept { return pixels.data(); }
    [[nodiscard]] std::uint8_t *data() noexcept { return pixels.data(); }
};

// Stable values used by the public API; the implementation maps them to its
// selected image backend rather than requiring backend headers from callers.
inline constexpr int read_grayscale = 0;
inline constexpr int read_color = 1;
inline constexpr int read_unchanged = -1;
inline constexpr int interpolation_nearest = 0;
inline constexpr int interpolation_linear = 1;
inline constexpr int interpolation_cubic = 2;
inline constexpr int interpolation_area = 3;
inline constexpr int interpolation_lanczos4 = 4;
inline constexpr int morphology_rect = 0;
inline constexpr int retrieval_external = 0;
inline constexpr int chain_approx_simple = 2;

Result<void> validate_image(const Image &image);
std::string path_to_utf8(const std::filesystem::path &path);
Result<Image> load_image(const std::string &path, int flags = read_color);
Result<Image> load_image(const std::filesystem::path &path, int flags = read_color);
Result<void> save_image(const Image &image, const std::string &path,
                        const std::vector<int> &options = {});
Result<void> save_image(const Image &image, const std::filesystem::path &path,
                        const std::vector<int> &options = {});
Result<Image> resize_image(const Image &image, Size size,
                           int interpolation = interpolation_linear);
Result<Image> crop_image(const Image &image, Rect region);
Result<Image> convert_color(const Image &image, int conversion);
Result<Image> normalize_image(const Image &image, double scale = 1.0 / 255.0,
                              double offset = 0.0);

struct Letterbox {
    Image image;
    float scale = 1.0f;
    int left = 0;
    int top = 0;
};

Result<Letterbox> create_letterbox(const Image &image, Size size,
                                   Scalar color = Scalar(114.0));

struct ImageTensor {
    std::vector<std::int64_t> shape;
    std::vector<float> data;
};

struct ContourInfo {
    std::vector<Point> points;
    double area = 0.0;
    double perimeter = 0.0;
    Rect bounding_box;
    Rect2f minimum_box;
    bool convex = false;
};

Result<ImageTensor> convert_to_tensor(const Image &image, bool rgb = true,
                                      float scale = 1.0f / 255.0f,
                                      Scalar mean = {},
                                      Scalar deviation = Scalar(1.0));

enum class BlurAlgorithm { gaussian, median, bilateral };
enum class ThresholdAlgorithm {
    binary, binary_inverse, trunc, to_zero, to_zero_inverse, otsu, triangle,
    adaptive_mean, adaptive_gaussian
};
enum class MorphologyOperation { erode, dilate, open, close, gradient, top_hat, black_hat };
enum class EdgeAlgorithm { canny, sobel, scharr, laplacian };

} // namespace sindre::utils_2d

#include <sindre/utils_2d/algorithms.h>
#include <sindre/utils_2d/image.h>
