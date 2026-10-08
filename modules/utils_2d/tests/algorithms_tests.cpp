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
    return EXIT_SUCCESS;
}
