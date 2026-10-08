#pragma once

/// @file
/// @brief 面向常用图像处理的拥有型 SindreImage 封装。

#include <sindre/utils_2d.h>

#include <cstdint>
#include <iosfwd>
#include <string_view>
#include <utility>

namespace sindre::utils_2d {

struct Matrix;

class SindreImage {
public:
    SindreImage() = default;
    explicit SindreImage(Image image) noexcept : image_(std::move(image)) {}

    static Result<SindreImage> load(const std::filesystem::path &path,
                                    int flags = read_color) noexcept;
    static Result<SindreImage> decode(const std::vector<std::uint8_t> &data,
                                      int flags = read_color) noexcept;
    static Result<SindreImage> from_image(const Image &image) noexcept;

    [[nodiscard]] const Image &get_image() const noexcept { return image_; }
    [[nodiscard]] Image &get_image() noexcept { return image_; }
    [[nodiscard]] bool is_empty() const noexcept { return image_.empty(); }
    [[nodiscard]] int get_width() const noexcept { return image_.width; }
    [[nodiscard]] int get_height() const noexcept { return image_.height; }
    [[nodiscard]] int get_channels() const noexcept { return image_.channels; }

    Result<void> save(const std::filesystem::path &path,
                      const std::vector<int> &options = {}) const noexcept;
    Result<std::vector<std::uint8_t>> encode(
        std::string_view extension,
        const std::vector<int> &options = {}) const noexcept;
    Result<SindreImage> clone() const noexcept;
    Result<void> resize(Size size, int interpolation = interpolation_linear) noexcept;
    Result<void> resize_keep_aspect(Size bounds, bool allow_upscale = false,
                                    int interpolation = interpolation_linear) noexcept;
    Result<void> crop(Rect region) noexcept;
    Result<void> crop_center(Size size) noexcept;
    Result<void> add_border(int top, int bottom, int left, int right,
                            Scalar color = {}) noexcept;
    Result<void> convert_color(int conversion) noexcept;
    Result<void> gray() noexcept;
    Result<void> bgr() noexcept;
    Result<void> rgb() noexcept;
    Result<void> normalize(double scale = 1.0 / 255.0,
                           double offset = 0.0) noexcept;
    Result<void> brightness_contrast(double alpha = 1.0,
                                     double beta = 0.0) noexcept;
    Result<void> gamma(double value) noexcept;
    Result<void> flip(int code) noexcept;
    Result<void> rotate(int code) noexcept;
    Result<void> letterbox(Size size, Scalar color = Scalar(114.0)) noexcept;
    Result<void> blur(BlurAlgorithm algorithm, Size kernel = {5, 5},
                      double sigma = 0.0) noexcept;
    Result<void> threshold(ThresholdAlgorithm algorithm, double threshold = 0.0,
                           double max_value = 255.0, int block_size = 11,
                           double constant = 2.0) noexcept;
    Result<void> morphology(MorphologyOperation operation, Size kernel = {3, 3},
                            int iterations = 1,
                            int shape = morphology_rect) noexcept;
    Result<void> detect_edges(EdgeAlgorithm algorithm, double low_threshold = 50.0,
                              double high_threshold = 150.0,
                              int aperture = 3) noexcept;
    Result<void> warp_affine(const Matrix &transform, Size size,
                             int interpolation = interpolation_linear) noexcept;
    Result<void> warp_perspective(const Matrix &transform, Size size,
                                  int interpolation = interpolation_linear) noexcept;
    Result<void> apply_mask(const Image &mask) noexcept;
    Result<std::vector<Image>> split() const noexcept;
    static Result<SindreImage> merge(const std::vector<Image> &channels) noexcept;
    Result<std::vector<ContourInfo>> find_contours(
        int retrieval = retrieval_external,
        int approximation = chain_approx_simple) const noexcept;
    Result<ImageTensor> to_tensor(bool rgb = true,
                                  float scale = 1.0f / 255.0f,
                                  Scalar mean = {},
                                  Scalar deviation = Scalar(1.0)) const noexcept;
    Result<void> show(std::string_view window_name = "sindre_image",
                      int wait_ms = 0) const noexcept;

private:
    Image image_;
};

} // namespace sindre::utils_2d

namespace sindre::utils_2d {
std::ostream &operator<<(std::ostream &stream, const SindreImage &image);
} // namespace sindre::utils_2d
