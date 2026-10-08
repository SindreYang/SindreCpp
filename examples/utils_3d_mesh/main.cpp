#include <sindre/utils_3d.h>

#include <filesystem>
#include <iostream>
#include <utility>

using namespace sindre::utils_3d;

int main(int argc, char **argv) {
    try {
        Vertices vertices(4, 3);
        vertices << 0, 0, 0,
                    1, 0, 0,
                    0, 1, 0,
                    0, 0, 1;
        Faces faces(4, 3);
        faces << 0, 2, 1,
                 0, 1, 3,
                 0, 3, 2,
                 1, 2, 3;

        SindreMesh mesh(vertices, faces);
        auto computed = mesh.compute_normals();
        if (!computed) {
            std::cerr << computed.error().describe() << '\n';
            return 1;
        }
        mesh = std::move(computed.value());
        const auto output = argc > 1
                                ? std::filesystem::path(argv[1])
                                : std::filesystem::current_path() / "sindre-example-mesh.vtp";
        auto saved = mesh.save(output);
        if (!saved) {
            std::cerr << saved.error().describe() << '\n';
            return 1;
        }

        auto loaded = SindreMesh::load(output);
        if (!loaded) {
            std::cerr << loaded.error().describe() << '\n';
            return 1;
        }
        std::cout << "saved: " << output.string() << '\n'
                  << "vertices: " << loaded.value().npoints() << '\n'
                  << "faces: " << loaded.value().nfaces() << '\n'
                  << "dimensions: " << loaded.value().mesh().dimensions().transpose() << '\n';
        std::filesystem::remove(output);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "utils_3d example failed: " << error.what() << '\n';
        return 1;
    }
}
