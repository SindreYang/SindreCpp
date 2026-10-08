#include <sindre/utils_2d.h>
#include "../private/native.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <system_error>

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

} // namespace

Image::Image(int image_height, int image_width, int image_channels,
             std::vector<std::uint8_t> image_pixels)
    : width(image_width), height(image_height), channels(image_channels),
      pixels(std::move(image_pixels)) {
    if (pixels.empty() && width > 0 && height > 0 && channels > 0)
        pixels.resize(static_cast<std::size_t>(width) * height * channels);
}

bool Image::empty() const noexcept {
    return width <= 0 || height <= 0 || channels <= 0 ||
           pixels.size() != static_cast<std::size_t>(width) * height * channels;
}

Result<void> validate_image(const Image &image) {
    if (image.empty())
        return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                     "Expected a non-empty 2D image", "validate_image");
    return Result<void>::success();
}

std::string path_to_utf8(const std::filesystem::path &path) {
#if defined(__cpp_char8_t)
    const auto value = path.u8string();
    return {reinterpret_cast<const char *>(value.data()), value.size()};
#else
    return path.u8string();
#endif
}

Result<Image> load_image(const std::string &path, int flags) {
    return capture_result("load_image", [&] {
        if (path.empty())
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Image path must not be empty", "load_image");
        const auto native = cv::imread(path, flags);
        if (native.empty())
            return Result<Image>::failure(std::make_error_code(std::errc::no_such_file_or_directory),
                                          "Cannot load image: " + path, "load_image");
        return Result<Image>::success(detail::from_native(native));
    });
}

Result<Image> load_image(const std::filesystem::path &path, int flags) {
    return load_image(path_to_utf8(path), flags);
}

Result<void> save_image(const Image &image, const std::string &path,
                        const std::vector<int> &options) {
    return capture_result("save_image", [&] {
        auto valid = validate_image(image);
        if (!valid) return valid;
        if (path.empty())
            return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                         "Image path must not be empty", "save_image");
        if (!cv::imwrite(path, detail::to_native(image), options))
            return Result<void>::failure(std::make_error_code(std::errc::io_error),
                                         "Cannot save image: " + path, "save_image");
        return Result<void>::success();
    });
}

Result<void> save_image(const Image &image, const std::filesystem::path &path,
                        const std::vector<int> &options) {
    return save_image(image, path_to_utf8(path), options);
}

Result<Image> resize_image(const Image &image, Size size, int interpolation) {
    return capture_result("resize_image", [&] {
        auto valid = validate_image(image);
        if (!valid) return Result<Image>::failure(valid.error());
        if (size.width <= 0 || size.height <= 0)
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Resize size must be positive", "resize_image");
        cv::Mat result;
        cv::resize(detail::to_native(image), result, detail::to_native(size), 0, 0, interpolation);
        return Result<Image>::success(detail::from_native(result));
    });
}

Result<Image> crop_image(const Image &image, Rect region) {
    return capture_result("crop_image", [&] {
        auto valid = validate_image(image);
        if (!valid) return Result<Image>::failure(valid.error());
        const Rect bounds{0, 0, image.width, image.height};
        if (region.width <= 0 || region.height <= 0 || region.x < bounds.x ||
            region.y < bounds.y || region.x + region.width > bounds.width ||
            region.y + region.height > bounds.height)
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Crop region must lie inside image", "crop_image");
        return Result<Image>::success(detail::from_native(
            detail::to_native(image)(detail::to_native(region)).clone()));
    });
}

Result<Image> convert_color(const Image &image, int conversion) {
    return capture_result("convert_color", [&] {
        auto valid = validate_image(image);
        if (!valid) return Result<Image>::failure(valid.error());
        cv::Mat result;
        cv::cvtColor(detail::to_native(image), result, conversion);
        return Result<Image>::success(detail::from_native(result));
    });
}

Result<Image> normalize_image(const Image &image, double scale, double offset) {
    return capture_result("normalize_image", [&] {
        auto valid = validate_image(image);
        if (!valid) return Result<Image>::failure(valid.error());
        if (!std::isfinite(scale) || !std::isfinite(offset))
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Normalization parameters must be finite", "normalize_image");
        cv::Mat result;
        detail::to_native(image).convertTo(result, CV_8U, scale, offset);
        return Result<Image>::success(detail::from_native(result));
    });
}

Result<Letterbox> create_letterbox(const Image &image, Size size, Scalar color) {
    return capture_result("create_letterbox", [&] {
        auto valid = validate_image(image);
        if (!valid) return Result<Letterbox>::failure(valid.error());
        if (size.width <= 0 || size.height <= 0)
            return Result<Letterbox>::failure(std::make_error_code(std::errc::invalid_argument),
                                              "Letterbox size must be positive", "create_letterbox");
        const double scale = std::min(double(size.width) / image.width,
                                      double(size.height) / image.height);
        const int width = std::max(1, std::min(size.width, int(std::round(image.width * scale))));
        const int height = std::max(1, std::min(size.height, int(std::round(image.height * scale))));
        const int left = (size.width - width) / 2;
        const int top = (size.height - height) / 2;
        auto resized = resize_image(image, {width, height});
        if (!resized) return Result<Letterbox>::failure(resized.error());
        cv::Mat result;
        cv::copyMakeBorder(detail::to_native(resized.value()), result, top, size.height - height - top,
                           left, size.width - width - left, cv::BORDER_CONSTANT,
                           detail::to_native(color));
        return Result<Letterbox>::success({detail::from_native(result), static_cast<float>(scale), left, top});
    });
}

Result<ImageTensor> convert_to_tensor(const Image &image, bool rgb, float scale,
                                      Scalar mean, Scalar deviation) {
    return capture_result("convert_to_tensor", [&] {
        auto valid = validate_image(image);
        if (!valid) return Result<ImageTensor>::failure(valid.error());
        if (image.channels != 1 && image.channels != 3)
            return Result<ImageTensor>::failure(std::make_error_code(std::errc::invalid_argument),
                                                "Tensor conversion supports grayscale or BGR images",
                                                "convert_to_tensor");
        if (!std::isfinite(scale))
            return Result<ImageTensor>::failure(std::make_error_code(std::errc::invalid_argument),
                                                "Scale must be finite", "convert_to_tensor");
        for (int channel = 0; channel < image.channels; ++channel)
            if (!std::isfinite(mean[channel]) || !std::isfinite(deviation[channel]) ||
                deviation[channel] <= 0.0)
                return Result<ImageTensor>::failure(std::make_error_code(std::errc::invalid_argument),
                                                    "Mean must be finite and standard deviation positive",
                                                    "convert_to_tensor");
        Image source = image;
        if (rgb && image.channels == 3) {
            auto converted = convert_color(image, cv::COLOR_BGR2RGB);
            if (!converted) return Result<ImageTensor>::failure(converted.error());
            source = std::move(converted.value());
        }
        const auto plane = static_cast<std::size_t>(source.width) * source.height;
        ImageTensor result{{1, source.channels, source.height, source.width},
                           std::vector<float>(plane * source.channels)};
        for (int row = 0; row < source.height; ++row) {
            for (int column = 0; column < source.width; ++column) {
                for (int channel = 0; channel < source.channels; ++channel) {
                    const auto index = (static_cast<std::size_t>(row) * source.width + column) * source.channels + channel;
                    result.data[plane * channel + static_cast<std::size_t>(row) * source.width + column] =
                        (static_cast<float>(source.pixels[index]) * scale - static_cast<float>(mean[channel])) /
                        static_cast<float>(deviation[channel]);
                }
            }
        }
        return Result<ImageTensor>::success(std::move(result));
    });
}

} // namespace sindre::utils_2d
