#include <sindre/utils_2d.h>
#include <cmath>
#include <filesystem>
#include <iostream>

int main() {
    using namespace sindre::utils_2d;
    try {
        Image image(2, 3, CV_8UC3, cv::Scalar(10, 20, 30));
        auto cropped = crop(image, {1, 0, 2, 2});
        cropped.at<cv::Vec3b>(0, 0)[0] = 99;
        if (image.at<cv::Vec3b>(0, 1)[0] != 10) return 1;
        auto box = letterbox(image, {8, 8});
        if (box.image.cols != 8 || box.image.rows != 8) return 2;
        auto tensor = to_tensor(image);
        if (tensor.shape != std::vector<std::int64_t>{1, 3, 2, 3}) return 3;
        if (std::abs(tensor.data[0] - 30.f / 255.f) > 1e-6) return 4;
        auto roi_tensor = to_tensor(image(cv::Rect(1, 0, 1, 2)), false, 1.f);
        if (roi_tensor.data != std::vector<float>{10, 10, 20, 20, 30, 30}) return 5;
        auto gray = change_color(image, cv::COLOR_BGR2GRAY);
        if (to_tensor(gray).shape[1] != 1) return 6;
        const auto unicode_path = std::filesystem::temp_directory_path() / L"sindre-中文图像.png";
        save(image, unicode_path);
        const auto loaded = load(unicode_path);
        if (loaded.rows != image.rows || loaded.cols != image.cols || loaded.channels() != image.channels()) return 7;
        std::filesystem::remove(unicode_path);
        bool rejected = false;
        try { (void)crop(image, {-1, 0, 2, 2}); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) return 8;
        rejected = false;
        try { (void)resize(image, {0, 2}); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) return 9;
        rejected = false;
        try { (void)to_tensor(image, true, 1.f, {}, cv::Scalar(0, 1, 1)); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) return 10;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 10;
    }
}
