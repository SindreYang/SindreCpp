#include <sindre/utils_3d.h>
#include <sindre/utils_3d/algorithms/point_cloud.h>

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
    if (!result || result.value().npoints() != 4 || result.value().nfaces() != 4)
        return 1;

    sindre::utils_3d::PointCloud cloud;
    cloud.points.resize(4, 3);
    cloud.points << 0.00, 0.00, 0.00,
                    0.01, 0.00, 0.00,
                    0.00, 0.01, 0.00,
                    0.01, 0.01, 0.00;
    sindre::utils_3d::PointCloudFilterOptions options;
    options.method = sindre::utils_3d::PointCloudFilter::voxel;
    options.voxel_size = 0.1;
    auto filtered = sindre::utils_3d::filter_point_cloud(cloud, options);
    return filtered && filtered.value().value.size() == 1 ? 0 : 1;
}
