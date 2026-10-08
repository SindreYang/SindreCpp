#include <sindre/utils_2d.h>

#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: sindre_example_utils_2d_image <input> [output]\n";
        return 2;
    }

    auto loaded = sindre::utils_2d::SindreImage::load(std::filesystem::path(argv[1]));
    if (!loaded) {
        std::cerr << "load failed: " << loaded.error().describe() << '\n';
        return 1;
    }

    if (auto status = loaded->resize({400, 400}); !status) {
        std::cerr << "resize failed: " << status.error().describe() << '\n';
        return 1;
    }

    auto clone = loaded->clone();
    if (!clone) {
        std::cerr << "clone failed: " << clone.error().describe() << '\n';
        return 1;
    }

    if (argc == 3) {
        if (auto status = clone->save(std::filesystem::path(argv[2])); !status) {
            std::cerr << "save failed: " << status.error().describe() << '\n';
            return 1;
        }
    }

    if (auto status = clone->show(); !status) {
        std::cerr << "show failed: " << status.error().describe() << '\n';
        return 1;
    }
    return 0;
}
