#include <sindre/utils_2d/algorithms.h>

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    using namespace sindre::utils_2d;
    Image image(32, 32, 1, std::vector<std::uint8_t>(32 * 32, 0));
    for (int y = 8; y < 24; ++y) {
        for (int x = 8; x < 24; ++x) {
            image.pixels[static_cast<std::size_t>(y) * 32 + x] =
                static_cast<std::uint8_t>((x + y) % 2 ? 220 : 30);
        }
    }
    const auto flow = calculate_dense_flow(image, image);
    if (!flow || flow.value().width != 32 || flow.value().height != 32 ||
        flow.value().channels != 3) {
        std::cerr << "installed Utils2d algorithm failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
