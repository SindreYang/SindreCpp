#include <sindre/gui.h>

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    sindre::gui::Context context;
    if (!context.is_valid()) {
        std::cerr << "installed GUI context is invalid\n";
        return EXIT_FAILURE;
    }

    const std::vector<std::uint8_t> ppm{
        'P', '6', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\n', 255, 0, 0};
    const auto image = sindre::gui::ImageAsset::load_memory(ppm);
    if (!image || image.value().width != 1 || image.value().height != 1) {
        std::cerr << "installed GUI image backend failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
