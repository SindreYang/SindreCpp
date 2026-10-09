#include <sindre/utils_2d/algorithms.h>

#include <cstdlib>
#include <iostream>

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "Check failed at " << __FILE__ << ':' << __LINE__ << "\n"; \
    return EXIT_FAILURE; \
} } while (false)

int main() {
    using namespace sindre::utils_2d;
    Image image(64, 64, 1, std::vector<std::uint8_t>(64 * 64, 0));
    auto contours = find_contours(image);
    CHECK(contours && contours.value().empty());

    const Matrix affine{2, 3, {1, 0, 0, 0, 1, 0}};
    const auto warped = warp_affine_image(image, affine, {32, 32});
    CHECK(warped && warped.value().width == 32 && warped.value().height == 32);

    const Matrix perspective{3, 3, {1, 0, 0, 0, 1, 0, 0, 0, 1}};
    const auto projected = warp_perspective_image(image, perspective, {32, 32});
    CHECK(projected && projected.value().width == 32);

    const auto features = detect_features(image, FeatureAlgorithm::orb);
    CHECK(features);

    Image textured(64, 64, 1, std::vector<std::uint8_t>(64 * 64, 0));
    for (int y = 8; y < 56; ++y) {
        for (int x = 8; x < 56; ++x) {
            textured.pixels[static_cast<std::size_t>(y) * 64 + x] =
                static_cast<std::uint8_t>(((x / 4) + (y / 4)) % 2 ? 220 : 30);
        }
    }
    Image shifted(64, 64, 1, std::vector<std::uint8_t>(64 * 64, 0));
    for (int y = 8; y < 56; ++y) {
        for (int x = 10; x < 58; ++x) {
            shifted.pixels[static_cast<std::size_t>(y) * 64 + x] =
                textured.pixels[static_cast<std::size_t>(y) * 64 + x - 2];
        }
    }
    const auto dense_flow = calculate_dense_flow(textured, shifted);
    CHECK(dense_flow && dense_flow.value().width == 64 &&
          dense_flow.value().height == 64 && dense_flow.value().channels == 3 &&
          dense_flow.value().byte_size() == 64u * 64u * 3u);
    const auto invalid_flow = calculate_dense_flow(textured, shifted, 1.0);
    CHECK(!invalid_flow &&
          invalid_flow.error().code == std::make_error_code(std::errc::invalid_argument));

    Image feature_image(128, 128, 1, std::vector<std::uint8_t>(128 * 128, 0));
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            feature_image.pixels[static_cast<std::size_t>(y) * 128 + x] =
                static_cast<std::uint8_t>((x * 37 + y * 17 + x * y) & 0xff);
        }
    }
    const auto brief = detect_features(feature_image, FeatureAlgorithm::brief, 200);
    const auto freak = detect_features(feature_image, FeatureAlgorithm::freak, 200);
#if defined(SINDRE_OPENCV_HAS_XFEATURES2D)
    CHECK(brief && freak && !brief.value().descriptors.empty() &&
          !freak.value().descriptors.empty());
    const auto flann = match_features(brief.value(), brief.value(),
                                      MatcherAlgorithm::flann);
    CHECK(flann);
#else
    CHECK(!brief && !freak &&
          brief.error().code == std::make_error_code(std::errc::function_not_supported) &&
          freak.error().code == std::make_error_code(std::errc::function_not_supported));
#endif

    const std::vector<Point2f> tracked_points{{20.0f, 20.0f}, {32.0f, 32.0f}};
    const auto farneback = track_points(textured, shifted, tracked_points,
                                        OpticalFlowAlgorithm::farneback);
    CHECK(farneback && farneback.value().points.size() == tracked_points.size());
    const auto rlof = track_points(feature_image, feature_image,
                                   std::vector<Point2f>{{64.0f, 64.0f}},
                                   OpticalFlowAlgorithm::rlof);
    CHECK(!rlof && rlof.error().code ==
          std::make_error_code(std::errc::function_not_supported));
    return EXIT_SUCCESS;
}
