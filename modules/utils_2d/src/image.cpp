#include <sindre/utils_2d/image.h>
#include <sindre/utils_2d/algorithms.h>
#include "../private/native.h"

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <exception>
#include <ostream>

namespace sindre::utils_2d {
namespace {

template <class Function>
auto capture_result(const char *context, Function &&function) -> decltype(function()) {
    using Return = decltype(function());
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        return function();
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const cv::Exception &error) {
        return Return::failure(std::make_error_code(std::errc::invalid_argument), error.what(), context);
    } catch (const std::exception &error) {
        return Return::failure(std::make_error_code(std::errc::io_error), error.what(), context);
    } catch (...) {
        return Return::failure(std::make_error_code(std::errc::io_error), "Image operation failed", context);
    }
#endif
}

template <class Operation>
Result<void> replace_image(SindreImage &image, const char *context, Operation &&operation) {
    return capture_result(context, [&] {
        auto result = operation(image.get_image());
        if (!result) return Result<void>::failure(result.error().with_context(context));
        image.get_image() = std::move(result.value());
        return Result<void>::success();
    });
}

cv::Mat matrix_to_native(const Matrix &matrix) {
    cv::Mat result(matrix.rows, matrix.columns, CV_64F);
    for (int row = 0; row < matrix.rows; ++row)
        for (int column = 0; column < matrix.columns; ++column)
            result.at<double>(row, column) = matrix.at(row, column);
    return result;
}

} // namespace

Result<SindreImage> SindreImage::load(const std::filesystem::path &path,
                                       int flags) noexcept {
    return capture_result("utils_2d.sindre_image.load", [&] {
        auto result = load_image(path, flags);
        if (!result) return Result<SindreImage>::failure(result.error());
        return Result<SindreImage>::success(SindreImage(std::move(result.value())));
    });
}

Result<SindreImage> SindreImage::decode(const std::vector<std::uint8_t> &data,
                                        int flags) noexcept {
    return capture_result("utils_2d.sindre_image.decode", [&] {
        if (data.empty())
            return Result<SindreImage>::failure(std::make_error_code(std::errc::invalid_argument),
                                                "Encoded image data must not be empty",
                                                "utils_2d.sindre_image.decode");
        auto native = cv::imdecode(data, flags);
        if (native.empty())
            return Result<SindreImage>::failure(std::make_error_code(std::errc::invalid_argument),
                                                "Cannot decode image data",
                                                "utils_2d.sindre_image.decode");
        return Result<SindreImage>::success(SindreImage(detail::from_native(native)));
    });
}

Result<SindreImage> SindreImage::from_image(const Image &image) noexcept {
    auto valid = validate_image(image);
    if (!valid) return Result<SindreImage>::failure(valid.error());
    return Result<SindreImage>::success(SindreImage(image));
}

Result<void> SindreImage::save(const std::filesystem::path &path,
                               const std::vector<int> &options) const noexcept {
    return save_image(image_, path, options);
}

Result<std::vector<std::uint8_t>> SindreImage::encode(
    std::string_view extension, const std::vector<int> &options) const noexcept {
    return capture_result("utils_2d.sindre_image.encode", [&] {
        auto valid = validate_image(image_);
        if (!valid) return Result<std::vector<std::uint8_t>>::failure(valid.error());
        if (extension.empty())
            return Result<std::vector<std::uint8_t>>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Image extension must not be empty", "utils_2d.sindre_image.encode");
        std::string suffix(extension);
        if (suffix.front() != '.') suffix.insert(suffix.begin(), '.');
        std::vector<std::uint8_t> output;
        if (!cv::imencode(suffix, detail::to_native(image_), output, options))
            return Result<std::vector<std::uint8_t>>::failure(
                std::make_error_code(std::errc::io_error), "Cannot encode image",
                "utils_2d.sindre_image.encode");
        return Result<std::vector<std::uint8_t>>::success(std::move(output));
    });
}

Result<SindreImage> SindreImage::clone() const noexcept {
    auto valid = validate_image(image_);
    if (!valid) return Result<SindreImage>::failure(valid.error());
    return Result<SindreImage>::success(SindreImage(image_));
}

Result<void> SindreImage::resize(Size size, int interpolation) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.resize",
                         [&](const Image &image) { return resize_image(image, size, interpolation); });
}

Result<void> SindreImage::resize_keep_aspect(Size bounds, bool allow_upscale,
                                             int interpolation) noexcept {
    auto valid = validate_image(image_);
    if (!valid) return Result<void>::failure(valid.error());
    if (bounds.width <= 0 || bounds.height <= 0)
        return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                     "Resize bounds must be positive", "utils_2d.sindre_image.resize_keep_aspect");
    double scale = std::min(static_cast<double>(bounds.width) / image_.width,
                            static_cast<double>(bounds.height) / image_.height);
    if (!allow_upscale) scale = std::min(1.0, scale);
    if (scale >= 1.0) return Result<void>::success();
    return resize({std::max(1, static_cast<int>(std::round(image_.width * scale))),
                   std::max(1, static_cast<int>(std::round(image_.height * scale)))}, interpolation);
}

Result<void> SindreImage::crop(Rect region) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.crop",
                         [&](const Image &image) { return crop_image(image, region); });
}

Result<void> SindreImage::crop_center(Size size) noexcept {
    if (size.width <= 0 || size.height <= 0 || size.width > image_.width || size.height > image_.height)
        return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                     "Center crop size must fit inside image", "utils_2d.sindre_image.crop_center");
    return crop({(image_.width - size.width) / 2, (image_.height - size.height) / 2,
                 size.width, size.height});
}

Result<void> SindreImage::add_border(int top, int bottom, int left, int right,
                                     Scalar color) noexcept {
    return capture_result("utils_2d.sindre_image.add_border", [&] {
        auto valid = validate_image(image_);
        if (!valid) return valid;
        if (top < 0 || bottom < 0 || left < 0 || right < 0)
            return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                         "Border sizes must be non-negative", "utils_2d.sindre_image.add_border");
        cv::Mat result;
        cv::copyMakeBorder(detail::to_native(image_), result, top, bottom, left, right,
                           cv::BORDER_CONSTANT, detail::to_native(color));
        image_ = detail::from_native(result);
        return Result<void>::success();
    });
}

Result<void> SindreImage::convert_color(int conversion) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.convert_color",
                         [&](const Image &image) { return ::sindre::utils_2d::convert_color(image, conversion); });
}

Result<void> SindreImage::gray() noexcept {
    if (image_.channels == 1) return Result<void>::success();
    return convert_color(image_.channels == 4 ? cv::COLOR_BGRA2GRAY : cv::COLOR_BGR2GRAY);
}
Result<void> SindreImage::bgr() noexcept {
    if (image_.channels == 3) return Result<void>::success();
    return convert_color(image_.channels == 1 ? cv::COLOR_GRAY2BGR : cv::COLOR_BGRA2BGR);
}
Result<void> SindreImage::rgb() noexcept {
    if (image_.channels == 3) return convert_color(cv::COLOR_BGR2RGB);
    return convert_color(image_.channels == 1 ? cv::COLOR_GRAY2RGB : cv::COLOR_BGRA2RGB);
}
Result<void> SindreImage::normalize(double scale, double offset) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.normalize",
                         [&](const Image &image) { return normalize_image(image, scale, offset); });
}

Result<void> SindreImage::brightness_contrast(double alpha, double beta) noexcept {
    return capture_result("utils_2d.sindre_image.brightness_contrast", [&] {
        if (!std::isfinite(alpha) || !std::isfinite(beta) || alpha < 0.0)
            return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                         "Contrast parameters are invalid", "utils_2d.sindre_image.brightness_contrast");
        cv::Mat result;
        detail::to_native(image_).convertTo(result, CV_8U, alpha, beta);
        image_ = detail::from_native(result);
        return Result<void>::success();
    });
}

Result<void> SindreImage::gamma(double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0)
        return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                     "Gamma must be finite and positive", "utils_2d.sindre_image.gamma");
    cv::Mat normalized, result;
    detail::to_native(image_).convertTo(normalized, CV_32F, 1.0 / 255.0);
    cv::pow(normalized, value, normalized);
    normalized.convertTo(result, CV_8U, 255.0);
    image_ = detail::from_native(result);
    return Result<void>::success();
}

Result<void> SindreImage::flip(int code) noexcept {
    if (code != -1 && code != 0 && code != 1)
        return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                     "Flip code must be -1, 0 or 1", "utils_2d.sindre_image.flip");
    cv::Mat result;
    cv::flip(detail::to_native(image_), result, code);
    image_ = detail::from_native(result);
    return Result<void>::success();
}

Result<void> SindreImage::rotate(int code) noexcept {
    cv::Mat result;
    cv::rotate(detail::to_native(image_), result, code);
    image_ = detail::from_native(result);
    return Result<void>::success();
}

Result<void> SindreImage::letterbox(Size size, Scalar color) noexcept {
    auto result = create_letterbox(image_, size, color);
    if (!result) return Result<void>::failure(result.error());
    image_ = std::move(result.value().image);
    return Result<void>::success();
}
Result<void> SindreImage::blur(BlurAlgorithm algorithm, Size kernel, double sigma) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.blur",
                         [&](const Image &image) { return apply_blur(image, algorithm, kernel, sigma); });
}
Result<void> SindreImage::threshold(ThresholdAlgorithm algorithm, double value,
                                    double max_value, int block_size, double constant) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.threshold",
                         [&](const Image &image) { return threshold_image(image, algorithm, value, max_value, block_size, constant); });
}
Result<void> SindreImage::morphology(MorphologyOperation operation, Size kernel,
                                     int iterations, int shape) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.morphology",
                         [&](const Image &image) { return apply_morphology(image, operation, kernel, iterations, shape); });
}
Result<void> SindreImage::detect_edges(EdgeAlgorithm algorithm, double low,
                                       double high, int aperture) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.detect_edges",
                         [&](const Image &image) { return ::sindre::utils_2d::detect_edges(image, algorithm, low, high, aperture); });
}
Result<void> SindreImage::warp_affine(const Matrix &transform, Size size,
                                      int interpolation) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.warp_affine",
                         [&](const Image &image) { return warp_affine_image(image, transform, size, interpolation); });
}
Result<void> SindreImage::warp_perspective(const Matrix &transform, Size size,
                                           int interpolation) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.warp_perspective",
                         [&](const Image &image) { return warp_perspective_image(image, transform, size, interpolation); });
}
Result<void> SindreImage::apply_mask(const Image &mask) noexcept {
    if (mask.width != image_.width || mask.height != image_.height || mask.channels != 1)
        return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                     "Mask must be a matching single-channel image", "utils_2d.sindre_image.apply_mask");
    cv::Mat result;
    cv::bitwise_and(detail::to_native(image_), detail::to_native(image_), result, detail::to_native(mask));
    image_ = detail::from_native(result);
    return Result<void>::success();
}
Result<std::vector<Image>> SindreImage::split() const noexcept {
    std::vector<cv::Mat> native;
    cv::split(detail::to_native(image_), native);
    std::vector<Image> result;
    result.reserve(native.size());
    for (const auto &part : native) result.push_back(detail::from_native(part));
    return Result<std::vector<Image>>::success(std::move(result));
}
Result<SindreImage> SindreImage::merge(const std::vector<Image> &channels) noexcept {
    if (channels.empty())
        return Result<SindreImage>::failure(std::make_error_code(std::errc::invalid_argument),
                                            "At least one channel is required", "utils_2d.sindre_image.merge");
    std::vector<cv::Mat> native;
    for (const auto &channel : channels) native.push_back(detail::to_native(channel));
    cv::Mat result;
    cv::merge(native, result);
    return Result<SindreImage>::success(SindreImage(detail::from_native(result)));
}
Result<std::vector<ContourInfo>> SindreImage::find_contours(int retrieval,
                                                            int approximation) const noexcept {
    return ::sindre::utils_2d::find_contours(image_, retrieval, approximation);
}
Result<ImageTensor> SindreImage::to_tensor(bool rgb, float scale, Scalar mean,
                                            Scalar deviation) const noexcept {
    return convert_to_tensor(image_, rgb, scale, mean, deviation);
}
Result<void> SindreImage::show(std::string_view window_name, int wait_ms) const noexcept {
    return capture_result("utils_2d.sindre_image.show", [&] {
        auto valid = validate_image(image_);
        if (!valid) return valid;
        if (window_name.empty() || wait_ms < 0)
            return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                         "Window name must not be empty and wait_ms must be non-negative",
                                         "utils_2d.sindre_image.show");
        cv::imshow(std::string(window_name), detail::to_native(image_));
        (void)cv::waitKey(wait_ms);
        return Result<void>::success();
    });
}

std::ostream &operator<<(std::ostream &stream, const SindreImage &image) {
    stream << "SindreImage{empty=" << (image.is_empty() ? "true" : "false")
           << ", width=" << image.get_width() << ", height=" << image.get_height()
           << ", channels=" << image.get_channels()
           << ", bytes=" << image.get_image().pixels.size() << '}';
    return stream;
}

} // namespace sindre::utils_2d
