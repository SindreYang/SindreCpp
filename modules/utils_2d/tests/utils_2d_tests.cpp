#include <sindre/utils_2d.h>
#include <sindre/utils_2d/image.h>

#if defined(SINDRE_WITH_LOG)
#include <spdlog/spdlog.h>
#endif

#include <cmath>
#include <filesystem>
#include <iostream>
#include <sstream>

int main() {
    using namespace sindre::utils_2d;
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        Image image(2, 3, CV_8UC3, cv::Scalar(10, 20, 30));
        auto cropped = crop_image(image, {1, 0, 2, 2});
        if (!cropped)
            return 1;
        cropped.value().at<cv::Vec3b>(0, 0)[0] = 99;
        if (image.at<cv::Vec3b>(0, 1)[0] != 10)
            return 2;

        auto box = create_letterbox(image, {8, 8});
        if (!box || box.value().image.cols != 8 || box.value().image.rows != 8)
            return 3;
        auto tensor = convert_to_tensor(image);
        if (!tensor || tensor.value().shape != std::vector<std::int64_t>{1, 3, 2, 3})
            return 4;
        if (std::abs(tensor.value().data[0] - 30.f / 255.f) > 1e-6)
            return 5;
        auto roi_tensor = convert_to_tensor(image(cv::Rect(1, 0, 1, 2)), false, 1.f);
        if (!roi_tensor || roi_tensor.value().data != std::vector<float>{10, 10, 20, 20, 30, 30})
            return 6;
        auto gray = convert_color(image, cv::COLOR_BGR2GRAY);
        if (!gray)
            return 7;
        auto gray_tensor = convert_to_tensor(gray.value());
        if (!gray_tensor || gray_tensor.value().shape[1] != 1)
            return 8;

        const auto unicode_path = std::filesystem::temp_directory_path() / L"sindre-中文图像.png";
        if (!save_image(image, unicode_path))
            return 9;
        const auto loaded = load_image(unicode_path);
        if (!loaded || loaded.value().rows != image.rows || loaded.value().cols != image.cols ||
            loaded.value().channels() != image.channels())
            return 10;
        auto facade_loaded = SindreImage::load(unicode_path);
        if (!facade_loaded || facade_loaded.value().get_width() != image.cols ||
            facade_loaded.value().get_height() != image.rows)
            return 11;
#if defined(SINDRE_WITH_LOG)
        if (!spdlog::get("sindre.utils_2d.image"))
            return 12;
#endif
        std::filesystem::remove(unicode_path);

        auto rejected = crop_image(image, {-1, 0, 2, 2});
        if (rejected)
            return 13;
        rejected = resize_image(image, {0, 2});
        if (rejected)
            return 14;
        auto bad_tensor = convert_to_tensor(image, true, 1.f, {}, cv::Scalar(0, 1, 1));
        if (bad_tensor)
            return 15;

        auto facade_result = SindreImage::from_native(image);
        if (!facade_result)
            return 16;
        auto& facade = facade_result.value();
        if (facade.is_empty() || facade.get_width() != 3 || facade.get_height() != 2 ||
            facade.get_channels() != 3)
            return 17;
        std::ostringstream image_info;
        image_info << facade;
        if (image_info.str().find("SindreImage{") == std::string::npos ||
            image_info.str().find("width=3") == std::string::npos)
            return 18;
        auto clone = facade.clone();
        if (!clone || clone.value().get_native().data == facade.get_native().data)
            return 19;
        if (auto status = facade.resize({6, 4}); !status)
            return 20;
        if (facade.get_width() != 6 || facade.get_height() != 4)
            return 21;
        if (auto status = facade.gray(); !status || facade.get_channels() != 1)
            return 22;
        if (auto status = facade.bgr(); !status || facade.get_channels() != 3)
            return 23;
        auto facade_tensor = facade.to_tensor();
        if (!facade_tensor || facade_tensor.value().shape != std::vector<std::int64_t>{1, 3, 4, 6})
            return 24;
        const auto before_failed_resize = facade.get_native().size();
        if (facade.resize({0, 4}) || facade.get_native().size() != before_failed_resize)
            return 25;
        auto empty = SindreImage::from_native(Image{});
        if (empty)
            return 26;
        auto encoded = facade.encode("png");
        if (!encoded || encoded.value().empty())
            return 27;
        auto decoded = SindreImage::decode(encoded.value());
        if (!decoded || decoded.value().get_width() != facade.get_width() ||
            decoded.value().get_height() != facade.get_height())
            return 28;
        auto channels = facade.split();
        if (!channels || channels.value().size() != 3)
            return 28;
        auto merged = SindreImage::merge(channels.value());
        if (!merged || merged.value().get_channels() != 3)
            return 29;
        if (auto status = facade.resize_keep_aspect({3, 3});
            !status || facade.get_width() != 3 || facade.get_height() != 2)
            return 30;
        if (auto status = facade.crop_center({2, 2});
            !status || facade.get_width() != 2 || facade.get_height() != 2)
            return 31;
        if (auto status = facade.add_border(1, 1, 1, 1);
            !status || facade.get_width() != 4 || facade.get_height() != 4)
            return 32;
        if (auto status = facade.brightness_contrast(1.1, 2.0); !status)
            return 33;
        if (auto status = facade.gamma(1.0); !status) {
            std::cerr << status.error().describe() << '\n';
            return 34;
        }
        Image mask(4, 4, CV_8UC1, cv::Scalar(255));
        if (auto status = facade.apply_mask(mask); !status)
            return 35;
        return 0;
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 36;
    }
#endif
}
