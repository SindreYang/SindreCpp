#include <iostream>
#include <sindrecpp/utils3d/sindremesh.hpp>

int main(int argc, char **argv) {
    using namespace sindrecpp::utils3d;
    try {
        if (argc < 2) {
            std::cerr << "Usage: sindrecpp_show_mesh mesh.ply [preview.png]\n";
            return 1;
        }
        SindreMesh mesh{std::filesystem::path(argv[1])};
        ShowOptions options;
        options.style.edges = true;
        options.interactive = argc < 3;
        options.offscreen = argc >= 3;
        auto viewer = mesh.show(options);
        if (argc >= 3)
            viewer.screenshot(argv[2]);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
