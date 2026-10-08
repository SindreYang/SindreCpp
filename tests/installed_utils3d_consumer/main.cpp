#include <sindre/utils_3d.h>

int main() {
    sindre::utils_3d::Vertices vertices(4, 3);
    vertices << 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1;
    sindre::utils_3d::Faces faces(4, 3);
    faces << 0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3;
    sindre::utils_3d::Mesh mesh(vertices, faces);
    auto result = sindre::utils_3d::clean_mesh(mesh)
                      .and_then([](const auto &output) {
                          return sindre::general::Result<sindre::utils_3d::Mesh>::success(
                              output.value);
                      });
    return result && result.value().npoints() == 4 &&
                   result.value().nfaces() == 4
               ? 0
               : 1;
}
