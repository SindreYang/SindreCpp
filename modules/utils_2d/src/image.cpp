#include <sindre/utils_2d/image.h>

#if defined(SINDRE_WITH_LOG)
#include <sindre/general/diag.h>
#endif

#include <opencv2/highgui.hpp>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace sindre::utils_2d {
namespace {

#if defined(SINDRE_WITH_LOG)
constexpr std::string_view image_logger_name = "sindre.utils_2d.image";

::sindre::general::log::LoggerPtr get_image_logger() noexcept {
    static std::mutex mutex;
    static ::sindre::general::log::LoggerPtr logger;
    std::lock_guard lock(mutex);
    if (logger) return logger;

    auto result = ::sindre::general::log::create_logger(std::string(image_logger_name));
    if (result) logger = std::move(result).value();
    return logger;
}

void log_image_event(::sindre::general::log::Level level, const char* message) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        if (auto logger = get_image_logger())
            logger->log(level, "{}", message);
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (...) {
    }
#endif
}

void log_image_failure(const char* message) noexcept {
    log_image_event(::sindre::general::log::Level::warn, message);
}

void log_image_exception(const char* message) noexcept {
    log_image_event(::sindre::general::log::Level::err, message);
}
#else
void log_image_event(...) noexcept {}
void log_image_failure(const char*) noexcept {}
void log_image_exception(const char*) noexcept {}
#endif

template <class Function>
auto capture_result(const char* context, Function&& function) -> decltype(function()) {
    using Return = decltype(function());
    log_image_event(
#if defined(SINDRE_WITH_LOG)
        ::sindre::general::log::Level::debug,
#endif
        context);
#if defined(SINDRE_NO_EXCEPTIONS)
    auto result = function();
    if (!result) log_image_event(
#if defined(SINDRE_WITH_LOG)
        log::Level::warn,
#endif
        context);
    return result;
#else
    try {
        auto result = function();
        if (!result) log_image_failure(context);
        return result;
    } catch (const cv::Exception& error) {
        log_image_exception(context);
        return Return::failure(std::make_error_code(std::errc::invalid_argument),
                               error.what(), context);
    } catch (const std::invalid_argument& error) {
        log_image_exception(context);
        return Return::failure(std::make_error_code(std::errc::invalid_argument),
                               error.what(), context);
    } catch (const std::overflow_error& error) {
        log_image_exception(context);
        return Return::failure(std::make_error_code(std::errc::value_too_large),
                               error.what(), context);
    } catch (const std::exception& error) {
        log_image_exception(context);
        return Return::failure(std::make_error_code(std::errc::io_error),
                               error.what(), context);
    } catch (...) {
        log_image_exception(context);
        return Return::failure(std::make_error_code(std::errc::io_error),
                               "Unknown image facade failure", context);
    }
#endif
}

template <class Operation>
Result<void> replace_image(SindreImage& image, const char* context, Operation&& operation) {
    return capture_result(context, [&] {
        auto result = operation(image.get_native());
        if (!result)
            return Result<void>::failure(result.error().with_context(context));
        image.get_native() = std::move(result.value());
        return Result<void>::success();
    });
}

Result<void> unsupported_channels(const char* context) {
    return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                 "Image channel count is not supported", context);
}

const char* depth_name(int depth) noexcept {
    switch (depth) {
    case CV_8U: return "CV_8U";
    case CV_8S: return "CV_8S";
    case CV_16U: return "CV_16U";
    case CV_16S: return "CV_16S";
    case CV_32S: return "CV_32S";
    case CV_32F: return "CV_32F";
    case CV_64F: return "CV_64F";
    default: return "unknown";
    }
}

std::string type_name(const Image& image) {
    if (image.empty())
        return "empty";
    std::string result = depth_name(image.depth());
    result += "C" + std::to_string(image.channels());
    return result;
}

} // namespace

Result<SindreImage> SindreImage::load(
    const std::filesystem::path& path, int flags) noexcept {
    return capture_result("utils_2d.sindre_image.load", [&] {
        auto result = load_image(path, flags);
        if (!result)
            return Result<SindreImage>::failure(
                result.error().with_context("utils_2d.sindre_image.load"));
        return Result<SindreImage>::success(SindreImage(std::move(result.value())));
    });
}

Result<SindreImage> SindreImage::decode(
    const std::vector<std::uint8_t>& data, int flags) noexcept {
    return capture_result("utils_2d.sindre_image.decode", [&] {
        if (data.empty())
            return Result<SindreImage>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Encoded image data must not be empty",
                "utils_2d.sindre_image.decode");
        auto image = cv::imdecode(data, flags);
        if (image.empty())
            return Result<SindreImage>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Cannot decode image data",
                "utils_2d.sindre_image.decode");
        return Result<SindreImage>::success(SindreImage(std::move(image)));
    });
}

Result<SindreImage> SindreImage::from_native(const Image& image) noexcept {
    return capture_result("utils_2d.sindre_image.from_native", [&] {
        auto valid = validate_image(image);
        if (!valid)
            return Result<SindreImage>::failure(
                valid.error().with_context("utils_2d.sindre_image.from_native"));
        return Result<SindreImage>::success(SindreImage(image.clone()));
    });
}

Result<void> SindreImage::save(
    const std::filesystem::path& path, const std::vector<int>& options) const noexcept {
    return capture_result("utils_2d.sindre_image.save", [&] {
        auto result = save_image(image_, path, options);
        if (!result)
            return Result<void>::failure(result.error().with_context(
                "utils_2d.sindre_image.save"));
        return Result<void>::success();
    });
}

Result<std::vector<std::uint8_t>> SindreImage::encode(
    std::string_view extension, const std::vector<int>& options) const noexcept {
    return capture_result("utils_2d.sindre_image.encode", [&] {
        auto valid = validate_image(image_);
        if (!valid)
            return Result<std::vector<std::uint8_t>>::failure(
                valid.error().with_context("utils_2d.sindre_image.encode"));
        if (extension.empty())
            return Result<std::vector<std::uint8_t>>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Image extension must not be empty",
                "utils_2d.sindre_image.encode");
        std::string suffix(extension);
        if (suffix.front() != '.')
            suffix.insert(suffix.begin(), '.');
        std::vector<std::uint8_t> data;
        if (!cv::imencode(suffix, image_, data, options))
            return Result<std::vector<std::uint8_t>>::failure(
                std::make_error_code(std::errc::io_error),
                "Cannot encode image",
                "utils_2d.sindre_image.encode");
        return Result<std::vector<std::uint8_t>>::success(std::move(data));
    });
}

Result<SindreImage> SindreImage::clone() const noexcept {
    return capture_result("utils_2d.sindre_image.clone", [&] {
        auto valid = validate_image(image_);
        if (!valid)
            return Result<SindreImage>::failure(
                valid.error().with_context("utils_2d.sindre_image.clone"));
        return Result<SindreImage>::success(SindreImage(image_.clone()));
    });
}

Result<void> SindreImage::resize(cv::Size size, int interpolation) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.resize",
                         [&](const Image& image) {
                             return resize_image(image, size, interpolation);
                         });
}

Result<void> SindreImage::resize_keep_aspect(
    cv::Size bounds, bool allow_upscale, int interpolation) noexcept {
    return capture_result("utils_2d.sindre_image.resize_keep_aspect", [&] {
        auto valid = validate_image(image_);
        if (!valid)
            return Result<void>::failure(valid.error().with_context(
                "utils_2d.sindre_image.resize_keep_aspect"));
        if (bounds.width <= 0 || bounds.height <= 0)
            return Result<void>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Resize bounds must be positive",
                "utils_2d.sindre_image.resize_keep_aspect");
        double scale = std::min(static_cast<double>(bounds.width) / image_.cols,
                                static_cast<double>(bounds.height) / image_.rows);
        if (!allow_upscale)
            scale = std::min(1.0, scale);
        if (scale >= 1.0 && image_.cols <= bounds.width && image_.rows <= bounds.height)
            return Result<void>::success();
        const auto width = std::max(1, static_cast<int>(std::round(image_.cols * scale)));
        const auto height = std::max(1, static_cast<int>(std::round(image_.rows * scale)));
        return resize({width, height}, interpolation);
    });
}

Result<void> SindreImage::crop(cv::Rect region) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.crop",
                         [&](const Image& image) { return crop_image(image, region); });
}

Result<void> SindreImage::crop_center(cv::Size size) noexcept {
    return capture_result("utils_2d.sindre_image.crop_center", [&] {
        auto valid = validate_image(image_);
        if (!valid)
            return Result<void>::failure(valid.error().with_context(
                "utils_2d.sindre_image.crop_center"));
        if (size.width <= 0 || size.height <= 0 || size.width > image_.cols ||
            size.height > image_.rows)
            return Result<void>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Center crop size must fit inside image",
                "utils_2d.sindre_image.crop_center");
        return crop({(image_.cols - size.width) / 2,
                     (image_.rows - size.height) / 2,
                     size.width, size.height});
    });
}

Result<void> SindreImage::add_border(
    int top, int bottom, int left, int right, cv::Scalar color) noexcept {
    return capture_result("utils_2d.sindre_image.add_border", [&] {
        auto valid = validate_image(image_);
        if (!valid)
            return Result<void>::failure(valid.error().with_context(
                "utils_2d.sindre_image.add_border"));
        if (top < 0 || bottom < 0 || left < 0 || right < 0)
            return Result<void>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Border sizes must be non-negative",
                "utils_2d.sindre_image.add_border");
        Image result;
        cv::copyMakeBorder(image_, result, top, bottom, left, right,
                           cv::BORDER_CONSTANT, color);
        image_ = std::move(result);
        return Result<void>::success();
    });
}

Result<void> SindreImage::convert_color(int conversion) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.convert_color",
                         [&](const Image& image) {
                             return ::sindre::utils_2d::convert_color(image, conversion);
                         });
}

Result<void> SindreImage::gray() noexcept {
    auto valid = validate_image(image_);
    if (!valid)
        return Result<void>::failure(
            valid.error().with_context("utils_2d.sindre_image.gray"));
    if (image_.channels() == 1)
        return Result<void>::success();
    if (image_.channels() == 3)
        return convert_color(cv::COLOR_BGR2GRAY);
    if (image_.channels() == 4)
        return convert_color(cv::COLOR_BGRA2GRAY);
    return unsupported_channels("utils_2d.sindre_image.gray");
}

Result<void> SindreImage::bgr() noexcept {
    auto valid = validate_image(image_);
    if (!valid)
        return Result<void>::failure(
            valid.error().with_context("utils_2d.sindre_image.bgr"));
    if (image_.channels() == 3)
        return Result<void>::success();
    if (image_.channels() == 1)
        return convert_color(cv::COLOR_GRAY2BGR);
    if (image_.channels() == 4)
        return convert_color(cv::COLOR_BGRA2BGR);
    return unsupported_channels("utils_2d.sindre_image.bgr");
}

Result<void> SindreImage::rgb() noexcept {
    auto valid = validate_image(image_);
    if (!valid)
        return Result<void>::failure(
            valid.error().with_context("utils_2d.sindre_image.rgb"));
    if (image_.channels() == 3)
        return convert_color(cv::COLOR_BGR2RGB);
    if (image_.channels() == 1)
        return convert_color(cv::COLOR_GRAY2RGB);
    if (image_.channels() == 4)
        return convert_color(cv::COLOR_BGRA2RGB);
    return unsupported_channels("utils_2d.sindre_image.rgb");
}

Result<void> SindreImage::normalize(double scale, double offset) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.normalize",
                         [&](const Image& image) {
                             return normalize_image(image, scale, offset);
                         });
}

Result<void> SindreImage::brightness_contrast(
    double alpha, double beta) noexcept {
    return capture_result("utils_2d.sindre_image.brightness_contrast", [&] {
        auto valid = validate_image(image_);
        if (!valid)
            return Result<void>::failure(valid.error().with_context(
                "utils_2d.sindre_image.brightness_contrast"));
        if (!std::isfinite(alpha) || !std::isfinite(beta) || alpha < 0.0)
            return Result<void>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Contrast alpha must be finite and non-negative; beta must be finite",
                "utils_2d.sindre_image.brightness_contrast");
        Image result;
        image_.convertTo(result, image_.type(), alpha, beta);
        image_ = std::move(result);
        return Result<void>::success();
    });
}

Result<void> SindreImage::gamma(double gamma) noexcept {
    return capture_result("utils_2d.sindre_image.gamma", [&] {
        auto valid = validate_image(image_);
        if (!valid)
            return Result<void>::failure(valid.error().with_context(
                "utils_2d.sindre_image.gamma"));
        if (!std::isfinite(gamma) || gamma <= 0.0)
            return Result<void>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Gamma must be finite and positive",
                "utils_2d.sindre_image.gamma");
        if (image_.depth() != CV_8U)
            return Result<void>::failure(
                std::make_error_code(std::errc::operation_not_supported),
                "Gamma correction currently supports CV_8U images only",
                "utils_2d.sindre_image.gamma");
        // OpenCV 4.8 requires the lookup table to be a continuous 256x1
        // vector for all channel counts.  The 1x256 form is accepted by some
        // newer builds but fails the same public API on the supported floor.
        cv::Mat normalized;
        image_.convertTo(normalized, CV_32F, 1.0 / 255.0);
        cv::pow(normalized, gamma, normalized);
        Image result;
        normalized.convertTo(result, image_.type(), 255.0);
        image_ = std::move(result);
        return Result<void>::success();
    });
}

Result<void> SindreImage::flip(int code) noexcept {
    return capture_result("utils_2d.sindre_image.flip", [&] {
        auto valid = validate_image(image_);
        if (!valid)
            return Result<void>::failure(
                valid.error().with_context("utils_2d.sindre_image.flip"));
        if (code != -1 && code != 0 && code != 1)
            return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                         "Flip code must be -1, 0 or 1",
                                         "utils_2d.sindre_image.flip");
        Image result;
        cv::flip(image_, result, code);
        image_ = std::move(result);
        return Result<void>::success();
    });
}

Result<void> SindreImage::rotate(cv::RotateFlags code) noexcept {
    return capture_result("utils_2d.sindre_image.rotate", [&] {
        auto valid = validate_image(image_);
        if (!valid)
            return Result<void>::failure(
                valid.error().with_context("utils_2d.sindre_image.rotate"));
        if (code != cv::ROTATE_90_CLOCKWISE &&
            code != cv::ROTATE_180 &&
            code != cv::ROTATE_90_COUNTERCLOCKWISE)
            return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                         "Unsupported rotate code",
                                         "utils_2d.sindre_image.rotate");
        Image result;
        cv::rotate(image_, result, code);
        image_ = std::move(result);
        return Result<void>::success();
    });
}

Result<void> SindreImage::letterbox(cv::Size size, cv::Scalar color) noexcept {
    return capture_result("utils_2d.sindre_image.letterbox", [&] {
        auto result = create_letterbox(image_, size, color);
        if (!result)
            return Result<void>::failure(result.error().with_context(
                "utils_2d.sindre_image.letterbox"));
        image_ = std::move(result.value().image);
        return Result<void>::success();
    });
}

Result<void> SindreImage::blur(
    BlurAlgorithm algorithm, cv::Size kernel, double sigma) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.blur",
                         [&](const Image& image) {
                             return apply_blur(image, algorithm, kernel, sigma);
                         });
}

Result<void> SindreImage::threshold(
    ThresholdAlgorithm algorithm, double threshold, double max_value,
    int block_size, double constant) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.threshold",
                         [&](const Image& image) {
                             return threshold_image(image, algorithm, threshold, max_value,
                                                    block_size, constant);
                         });
}

Result<void> SindreImage::morphology(
    MorphologyOperation operation, cv::Size kernel, int iterations, int shape) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.morphology",
                         [&](const Image& image) {
                             return apply_morphology(image, operation, kernel, iterations, shape);
                         });
}

Result<void> SindreImage::detect_edges(
    EdgeAlgorithm algorithm, double low_threshold, double high_threshold,
    int aperture) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.detect_edges",
                         [&](const Image& image) {
                             return ::sindre::utils_2d::detect_edges(
                                 image, algorithm, low_threshold, high_threshold, aperture);
                         });
}

Result<void> SindreImage::warp_affine(
    const cv::Mat& transform, cv::Size size, int interpolation) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.warp_affine",
                         [&](const Image& image) {
                             return warp_affine_image(image, transform, size, interpolation);
                         });
}

Result<void> SindreImage::warp_perspective(
    const cv::Mat& transform, cv::Size size, int interpolation) noexcept {
    return replace_image(*this, "utils_2d.sindre_image.warp_perspective",
                         [&](const Image& image) {
                             return warp_perspective_image(image, transform, size, interpolation);
                         });
}

Result<void> SindreImage::apply_mask(const Image& mask) noexcept {
    return capture_result("utils_2d.sindre_image.apply_mask", [&] {
        auto valid = validate_image(image_);
        if (!valid)
            return Result<void>::failure(valid.error().with_context(
                "utils_2d.sindre_image.apply_mask"));
        if (mask.empty() || mask.dims != 2 || mask.type() != CV_8UC1 ||
            mask.size() != image_.size())
            return Result<void>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Mask must be a non-empty CV_8UC1 image with matching dimensions",
                "utils_2d.sindre_image.apply_mask");
        Image result;
        cv::bitwise_and(image_, image_, result, mask);
        image_ = std::move(result);
        return Result<void>::success();
    });
}

Result<std::vector<Image>> SindreImage::split() const noexcept {
    return capture_result("utils_2d.sindre_image.split", [&] {
        auto valid = validate_image(image_);
        if (!valid)
            return Result<std::vector<Image>>::failure(valid.error().with_context(
                "utils_2d.sindre_image.split"));
        std::vector<Image> channels;
        cv::split(image_, channels);
        return Result<std::vector<Image>>::success(std::move(channels));
    });
}

Result<SindreImage> SindreImage::merge(
    const std::vector<Image>& channels) noexcept {
    return capture_result("utils_2d.sindre_image.merge", [&] {
        if (channels.empty())
            return Result<SindreImage>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "At least one channel is required",
                "utils_2d.sindre_image.merge");
        Image merged;
        cv::merge(channels, merged);
        return Result<SindreImage>::success(SindreImage(std::move(merged)));
    });
}

Result<std::vector<ContourInfo>> SindreImage::find_contours(
    int retrieval, int approximation) const noexcept {
    return capture_result("utils_2d.sindre_image.find_contours", [&] {
        auto result = ::sindre::utils_2d::find_contours(image_, retrieval, approximation);
        if (!result)
            return Result<std::vector<ContourInfo>>::failure(result.error().with_context(
                "utils_2d.sindre_image.find_contours"));
        return Result<std::vector<ContourInfo>>::success(std::move(result.value()));
    });
}

Result<ImageTensor> SindreImage::to_tensor(
    bool rgb, float scale, cv::Scalar mean, cv::Scalar deviation) const noexcept {
    return capture_result("utils_2d.sindre_image.to_tensor", [&] {
        auto result = convert_to_tensor(image_, rgb, scale, mean, deviation);
        if (!result)
            return Result<ImageTensor>::failure(result.error().with_context(
                "utils_2d.sindre_image.to_tensor"));
        return Result<ImageTensor>::success(std::move(result.value()));
    });
}

Result<void> SindreImage::show(std::string_view window_name, int wait_ms) const noexcept {
    return capture_result("utils_2d.sindre_image.show", [&] {
        auto valid = validate_image(image_);
        if (!valid)
            return Result<void>::failure(
                valid.error().with_context("utils_2d.sindre_image.show"));
        if (window_name.empty() || wait_ms < 0)
            return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                         "Window name must not be empty and wait_ms must be non-negative",
                                         "utils_2d.sindre_image.show");
        cv::imshow(std::string(window_name), image_);
        (void)cv::waitKey(wait_ms);
        return Result<void>::success();
    });
}

std::ostream& operator<<(std::ostream& stream, const SindreImage& image) {
    stream << "SindreImage{"
           << "empty=" << (image.is_empty() ? "true" : "false")
           << ", width=" << image.get_width()
           << ", height=" << image.get_height()
           << ", channels=" << image.get_channels()
           << ", type=" << type_name(image.get_native())
           << ", bytes=" << (image.get_native().total() * image.get_native().elemSize())
           << ", continuous="
           << (image.get_native().isContinuous() ? "true" : "false")
           << '}';
    return stream;
}

} // namespace sindre::utils_2d
