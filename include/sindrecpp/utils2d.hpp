#pragma once

#if !defined(SINDRECPP_WITH_UTILS2D)
#error "Enable SINDRECPP_WITH_UTILS2D and link SindreCpp::Utils2d."
#endif

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace sindrecpp::utils2d {

using Image = cv::Mat;
namespace native = cv;

inline void validate(const Image& image) {
    if (image.empty() || image.dims != 2) throw std::invalid_argument("Expected a non-empty 2D image");
}
inline Image load(const std::string& path, int flags = cv::IMREAD_COLOR) {
    auto image = cv::imread(path, flags);
    if (image.empty()) throw std::runtime_error("Cannot load image: " + path);
    return image;
}
inline void save(const Image& image, const std::string& path, const std::vector<int>& options = {}) {
    validate(image);
    if (!cv::imwrite(path, image, options)) throw std::runtime_error("Cannot save image: " + path);
}
inline Image resize(const Image& image, cv::Size size, int interpolation = cv::INTER_LINEAR) {
    validate(image);
    if (size.width <= 0 || size.height <= 0) throw std::invalid_argument("Resize size must be positive");
    Image result;
    cv::resize(image, result, size, 0, 0, interpolation);
    return result;
}
inline Image crop(const Image& image, cv::Rect region) {
    validate(image);
    const cv::Rect bounds(0, 0, image.cols, image.rows);
    if (region.width <= 0 || region.height <= 0 || (region & bounds) != region)
        throw std::invalid_argument("Crop region must lie inside image");
    return image(region).clone(); // Does not retain the full source image.
}
inline Image change_color(const Image& image, int conversion) {
    validate(image);
    Image result;
    cv::cvtColor(image, result, conversion);
    return result;
}
inline Image normalize(const Image& image, double scale = 1.0 / 255.0, double offset = 0.0) {
    validate(image);
    if (!std::isfinite(scale) || !std::isfinite(offset))
        throw std::invalid_argument("Normalization parameters must be finite");
    Image result;
    image.convertTo(result, CV_32F, scale, offset);
    return result;
}
struct Letterbox {
    Image image;
    float scale;
    int left;
    int top;
};
inline Letterbox letterbox(const Image& image, cv::Size size,
                           cv::Scalar color = cv::Scalar(114, 114, 114)) {
    validate(image);
    if (size.width <= 0 || size.height <= 0) throw std::invalid_argument("Letterbox size must be positive");
    const double scale = std::min(double(size.width) / image.cols, double(size.height) / image.rows);
    const int width = std::max(1, std::min(size.width, int(std::round(image.cols * scale))));
    const int height = std::max(1, std::min(size.height, int(std::round(image.rows * scale))));
    const int left = (size.width - width) / 2, top = (size.height - height) / 2;
    auto resized = resize(image, {width, height});
    Image result;
    cv::copyMakeBorder(resized, result, top, size.height - height - top,
                       left, size.width - width - left, cv::BORDER_CONSTANT, color);
    return {result, static_cast<float>(scale), left, top};
}
struct ImageTensor {
    std::vector<std::int64_t> shape; // NCHW, batch=1.
    std::vector<float> data;
};
// Explicitly BGR input -> RGB by default. Normalization is (pixel * scale - mean) / std.
// mean/std are in output channel order. No implicit resize or letterbox.
inline ImageTensor to_tensor(const Image& image, bool rgb = true, float scale = 1.f / 255.f,
                             cv::Scalar mean = {}, cv::Scalar deviation = cv::Scalar(1, 1, 1, 1)) {
    validate(image);
    if (image.channels() != 1 && image.channels() != 3)
        throw std::invalid_argument("Tensor conversion supports grayscale or BGR images");
    if (!std::isfinite(scale)) throw std::invalid_argument("Scale must be finite");
    const int channels = image.channels();
    for (int c = 0; c < channels; ++c)
        if (!std::isfinite(mean[c]) || !std::isfinite(deviation[c]) || deviation[c] <= 0)
            throw std::invalid_argument("Mean must be finite and standard deviation positive");
    auto source = rgb && channels == 3 ? change_color(image, cv::COLOR_BGR2RGB) : image;
    auto pixels = normalize(source, scale);
    const auto plane = static_cast<std::size_t>(image.rows) * image.cols;
    if (plane > std::numeric_limits<std::size_t>::max() / channels)
        throw std::overflow_error("Image tensor is too large");
    ImageTensor result{{1, channels, image.rows, image.cols}, std::vector<float>(plane * channels)};
    for (int row = 0; row < image.rows; ++row) {
        const auto* values = pixels.ptr<float>(row);
        for (int col = 0; col < image.cols; ++col)
            for (int c = 0; c < channels; ++c)
                result.data[plane * c + static_cast<std::size_t>(row) * image.cols + col] =
                    static_cast<float>((values[col * channels + c] - mean[c]) / deviation[c]);
    }
    return result;
}

} // namespace sindrecpp::utils2d
