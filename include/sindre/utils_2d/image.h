#pragma once

/// @file
/// @brief 面向常用图像处理的 SindreImage 高级封装。

#include <sindre/utils_2d.h>

#include <cstdint>
#include <iosfwd>
#include <string_view>
#include <utility>

namespace sindre::utils_2d {

/// @brief 以 OpenCV cv::Mat 为后端的常用图像操作 facade。
///
/// SindreImage 拥有自己的 cv::Mat 引用。修改操作在成功后替换内部图像，失败时
/// 保持原图不变；需要独立深拷贝时使用 clone()。
class SindreImage {
public:
    SindreImage() = default;
    explicit SindreImage(Image image) noexcept : image_(std::move(image)) {}

    /// @brief 从文件加载图像。
    static Result<SindreImage> load(
        const std::filesystem::path& path,
        int flags = cv::IMREAD_COLOR) noexcept;

    /// @brief 从内存中的常见图像编码创建图像。
    static Result<SindreImage> decode(
        const std::vector<std::uint8_t>& data,
        int flags = cv::IMREAD_COLOR) noexcept;

    /// @brief 从 OpenCV 图像创建独立副本。
    static Result<SindreImage> from_native(const Image& image) noexcept;

    /// @brief 访问底层 OpenCV 图像。
    const Image& get_native() const noexcept { return image_; }
    Image& get_native() noexcept { return image_; }

    bool is_empty() const noexcept { return image_.empty(); }
    int get_width() const noexcept { return image_.cols; }
    int get_height() const noexcept { return image_.rows; }
    int get_channels() const noexcept { return image_.channels(); }
    int get_type() const noexcept { return image_.type(); }

    Result<void> save(
        const std::filesystem::path& path,
        const std::vector<int>& options = {}) const noexcept;

    /// @brief 将图像编码为 PNG/JPEG 等格式的内存字节。
    Result<std::vector<std::uint8_t>> encode(
        std::string_view extension,
        const std::vector<int>& options = {}) const noexcept;

    Result<SindreImage> clone() const noexcept;

    Result<void> resize(
        cv::Size size,
        int interpolation = cv::INTER_LINEAR) noexcept;
    Result<void> resize_keep_aspect(
        cv::Size bounds,
        bool allow_upscale = false,
        int interpolation = cv::INTER_LINEAR) noexcept;
    Result<void> crop(cv::Rect region) noexcept;
    Result<void> crop_center(cv::Size size) noexcept;
    Result<void> add_border(
        int top, int bottom, int left, int right,
        cv::Scalar color = cv::Scalar()) noexcept;
    Result<void> convert_color(int conversion) noexcept;
    Result<void> gray() noexcept;
    Result<void> bgr() noexcept;
    Result<void> rgb() noexcept;
    Result<void> normalize(
        double scale = 1.0 / 255.0,
        double offset = 0.0) noexcept;
    Result<void> brightness_contrast(
        double alpha = 1.0, double beta = 0.0) noexcept;
    Result<void> gamma(double gamma) noexcept;
    Result<void> flip(int code) noexcept;
    Result<void> rotate(cv::RotateFlags code) noexcept;
    Result<void> letterbox(
        cv::Size size,
        cv::Scalar color = cv::Scalar(114, 114, 114)) noexcept;

    Result<void> blur(
        BlurAlgorithm algorithm,
        cv::Size kernel = {5, 5},
        double sigma = 0.0) noexcept;
    Result<void> threshold(
        ThresholdAlgorithm algorithm,
        double threshold = 0.0,
        double max_value = 255.0,
        int block_size = 11,
        double constant = 2.0) noexcept;
    Result<void> morphology(
        MorphologyOperation operation,
        cv::Size kernel = {3, 3},
        int iterations = 1,
        int shape = cv::MORPH_RECT) noexcept;
    Result<void> detect_edges(
        EdgeAlgorithm algorithm,
        double low_threshold = 50.0,
        double high_threshold = 150.0,
        int aperture = 3) noexcept;
    Result<void> warp_affine(
        const cv::Mat& transform,
        cv::Size size,
        int interpolation = cv::INTER_LINEAR) noexcept;
    Result<void> warp_perspective(
        const cv::Mat& transform,
        cv::Size size,
        int interpolation = cv::INTER_LINEAR) noexcept;

    Result<void> apply_mask(const Image& mask) noexcept;

    /// @brief 将多通道图像拆成独立的单通道副本。
    Result<std::vector<Image>> split() const noexcept;
    /// @brief 从多个单通道图像创建 SindreImage。
    static Result<SindreImage> merge(
        const std::vector<Image>& channels) noexcept;
    /// @brief 查找当前二值图像中的轮廓。
    Result<std::vector<ContourInfo>> find_contours(
        int retrieval = cv::RETR_EXTERNAL,
        int approximation = cv::CHAIN_APPROX_SIMPLE) const noexcept;

    Result<ImageTensor> to_tensor(
        bool rgb = true,
        float scale = 1.0f / 255.0f,
        cv::Scalar mean = {},
        cv::Scalar deviation = cv::Scalar(1, 1, 1, 1)) const noexcept;

    /// @brief 显示图像并等待 wait_ms 毫秒；wait_ms=0 表示等待用户关闭窗口。
    Result<void> show(
        std::string_view window_name = "sindre_image",
        int wait_ms = 0) const noexcept;

private:
    Image image_;
};

} // namespace sindre::utils_2d

namespace sindre::utils_2d {

std::ostream& operator<<(std::ostream& stream, const SindreImage& image);

} // namespace sindre::utils_2d
