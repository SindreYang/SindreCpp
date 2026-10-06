#include <utils3d/index.hpp>

#include <filesystem>
#include <iostream>

using namespace sindrecpp::utils3d;

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
        mesh.compute_normals();
        const auto output = argc > 1
                                ? std::filesystem::path(argv[1])
                                : std::filesystem::current_path() / "sindrecpp-example-mesh.vtp";
        mesh.save(output);

        SindreMesh loaded(output);
        std::cout << "saved: " << output.string() << '\n'
                  << "vertices: " << loaded.npoints() << '\n'
                  << "faces: " << loaded.nfaces() << '\n'
                  << "dimensions: " << loaded.dimensions().transpose() << '\n';
        std::filesystem::remove(output);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "utils3d example failed: " << error.what() << '\n';
        return 1;
    }
}
