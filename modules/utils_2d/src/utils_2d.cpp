#include <sindre/utils_2d.h>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace sindre::utils_2d {

void validate(const Image& image) {
    if (image.empty() || image.dims != 2)
        throw std::invalid_argument("Expected a non-empty 2D image");
}

std::string path_to_utf8(const std::filesystem::path& path) {
#if defined(__cpp_char8_t)
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
#else
    return path.u8string();
#endif
}

Image load_image(const std::string& path, int flags) {
    auto image = cv::imread(path, flags);
    if (image.empty())
        throw std::runtime_error("Cannot load image: " + path);
    return image;
}

Image load_image(const std::filesystem::path& path, int flags) {
    return load_image(path_to_utf8(path), flags);
}

void save_image(const Image& image, const std::string& path,
                const std::vector<int>& options) {
    validate(image);
    if (!cv::imwrite(path, image, options))
        throw std::runtime_error("Cannot save image: " + path);
}

void save_image(const Image& image, const std::filesystem::path& path,
                const std::vector<int>& options) {
    save_image(image, path_to_utf8(path), options);
}

Image resize_image(const Image& image, cv::Size size, int interpolation) {
    validate(image);
    if (size.width <= 0 || size.height <= 0)
        throw std::invalid_argument("Resize size must be positive");
    Image result;
    cv::resize(image, result, size, 0, 0, interpolation);
    return result;
}

Image crop_image(const Image& image, cv::Rect region) {
    validate(image);
    const cv::Rect bounds(0, 0, image.cols, image.rows);
    if (region.width <= 0 || region.height <= 0 || (region & bounds) != region)
        throw std::invalid_argument("Crop region must lie inside image");
    return image(region).clone();
}

Image convert_color(const Image& image, int conversion) {
    validate(image);
    Image result;
    cv::cvtColor(image, result, conversion);
    return result;
}

Image normalize_image(const Image& image, double scale, double offset) {
    validate(image);
    if (!std::isfinite(scale) || !std::isfinite(offset))
        throw std::invalid_argument("Normalization parameters must be finite");
    Image result;
    image.convertTo(result, CV_32F, scale, offset);
    return result;
}

Letterbox create_letterbox(const Image& image, cv::Size size, cv::Scalar color) {
    validate(image);
    if (size.width <= 0 || size.height <= 0)
        throw std::invalid_argument("Letterbox size must be positive");
    const double scale = std::min(double(size.width) / image.cols,
                                  double(size.height) / image.rows);
    const int width = std::max(1, std::min(size.width,
                                           int(std::round(image.cols * scale))));
    const int height = std::max(1, std::min(size.height,
                                            int(std::round(image.rows * scale))));
    const int left = (size.width - width) / 2;
    const int top = (size.height - height) / 2;
    auto resized = resize_image(image, {width, height});
    Image result;
    cv::copyMakeBorder(resized, result, top, size.height - height - top,
                       left, size.width - width - left, cv::BORDER_CONSTANT, color);
    return {result, static_cast<float>(scale), left, top};
}

ImageTensor convert_to_tensor(const Image& image, bool rgb, float scale,
                              cv::Scalar mean, cv::Scalar deviation) {
    validate(image);
    if (image.channels() != 1 && image.channels() != 3)
        throw std::invalid_argument("Tensor conversion supports grayscale or BGR images");
    if (!std::isfinite(scale))
        throw std::invalid_argument("Scale must be finite");
    const int channels = image.channels();
    for (int c = 0; c < channels; ++c) {
        if (!std::isfinite(mean[c]) || !std::isfinite(deviation[c]) || deviation[c] <= 0)
            throw std::invalid_argument("Mean must be finite and standard deviation positive");
    }

    auto source = rgb && channels == 3 ? convert_color(image, cv::COLOR_BGR2RGB) : image;
    auto pixels = normalize_image(source, scale);
    const auto plane = static_cast<std::size_t>(image.rows) * image.cols;
    if (plane > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(channels))
        throw std::overflow_error("Image tensor is too large");

    ImageTensor result{{1, channels, image.rows, image.cols},
                       std::vector<float>(plane * static_cast<std::size_t>(channels))};
    for (int row = 0; row < image.rows; ++row) {
        const auto* values = pixels.ptr<float>(row);
        for (int col = 0; col < image.cols; ++col) {
            for (int c = 0; c < channels; ++c) {
                result.data[plane * static_cast<std::size_t>(c) +
                            static_cast<std::size_t>(row) * image.cols + col] =
                    static_cast<float>((values[col * channels + c] - mean[c]) / deviation[c]);
            }
        }
    }
    return result;
}

Image load(const std::string &path, int flags) { return load_image(path, flags); }
Image load(const std::filesystem::path &path, int flags) { return load_image(path, flags); }
void save(const Image &image, const std::string &path, const std::vector<int> &options) {
    save_image(image, path, options);
}
void save(const Image &image, const std::filesystem::path &path,
          const std::vector<int> &options) {
    save_image(image, path, options);
}
Image resize(const Image &image, cv::Size size, int interpolation) {
    return resize_image(image, size, interpolation);
}
Image crop(const Image &image, cv::Rect region) { return crop_image(image, region); }
Image change_color(const Image &image, int conversion) { return convert_color(image, conversion); }
Image normalize(const Image &image, double scale, double offset) {
    return normalize_image(image, scale, offset);
}
Letterbox letterbox(const Image &image, cv::Size size, cv::Scalar color) {
    return create_letterbox(image, size, color);
}
ImageTensor to_tensor(const Image &image, bool rgb, float scale, cv::Scalar mean,
                      cv::Scalar deviation) {
    return convert_to_tensor(image, rgb, scale, mean, deviation);
}

} // namespace sindre::utils_2d
