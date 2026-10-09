#include <sindre/utils_2d.h>

#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "Check failed at " << __FILE__ << ':' << __LINE__ << "\n"; \
    return EXIT_FAILURE; \
} } while (false)

int main() {
    using namespace sindre::utils_2d;
    Image image(3, 4, 3, std::vector<std::uint8_t>(36, 10));
    CHECK(!image.empty() && image.width == 4 && image.height == 3 && image.channels == 3);
    CHECK(validate_image(image));

    const auto resized = resize_image(image, {8, 6});
    CHECK(resized && resized.value().width == 8 && resized.value().height == 6);
    const auto cropped = crop_image(resized.value(), {1, 1, 3, 2});
    CHECK(cropped && cropped.value().width == 3 && cropped.value().height == 2);
    const auto boxed = create_letterbox(image, {10, 10});
    CHECK(boxed && boxed.value().image.width == 10 && boxed.value().image.height == 10);
    const auto tensor = convert_to_tensor(image, true);
    CHECK(tensor && tensor.value().shape == std::vector<std::int64_t>({1, 3, 3, 4}) &&
          tensor.value().data.size() == 36);

    SindreImage facade(image);
    CHECK(facade.get_width() == 4 && facade.get_height() == 3);
    CHECK(facade.resize({8, 6}));
    CHECK(facade.crop_center({4, 4}));
    CHECK(facade.get_width() == 4 && facade.get_height() == 4);
    CHECK(facade.clone());
    std::ostringstream description;
    description << facade;
    CHECK(description.str().find("SindreImage") != std::string::npos);

    const auto unique_id = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
        ("sindre-中文图像-" + std::to_string(unique_id) + ".png");
    CHECK(facade.save(path));
    auto loaded = SindreImage::load(path);
    CHECK(loaded && loaded.value().get_width() == 4);
    auto encoded = facade.encode(".png");
    CHECK(encoded && !encoded.value().empty());
    auto decoded = SindreImage::decode(encoded.value());
    CHECK(decoded && decoded.value().get_width() == 4);
    std::filesystem::remove(path);

    const auto blurred = apply_blur(image, BlurAlgorithm::gaussian);
    const auto thresholded = threshold_image(image, ThresholdAlgorithm::binary);
    const auto edges = detect_edges(image, EdgeAlgorithm::canny);
    CHECK(blurred && thresholded && edges);

    std::vector<BoundingBox> boxes;
    boxes.push_back(BoundingBox{{0, 0, 10, 10}, 0.9f, 1});
    boxes.push_back(BoundingBox{{1, 1, 10, 10}, 0.8f, 1});
    boxes.push_back(BoundingBox{{1, 1, 10, 10}, 0.7f, 2});
    const auto nms = non_maximum_suppression(boxes);
    CHECK(nms && nms.value().size() == 2);
    return EXIT_SUCCESS;
}
