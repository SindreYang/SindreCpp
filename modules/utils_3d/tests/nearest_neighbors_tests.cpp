#include <sindre/utils_3d/algorithms/nearest_neighbors.h>

#include <cmath>
#include <iostream>

using namespace sindre::utils_3d;

int main() {
    Vertices points(4, 3);
    points << 0.0, 0.0, 0.0,
              1.0, 0.0, 0.0,
              0.0, 2.0, 0.0,
              0.0, 0.0, 3.0;

    auto index = NearestNeighborIndex::create(points);
    if (!index || index.value().size() != 4) return 1;

    auto nearest = index.value().get_nearest(::sindre::math::Vector3(0.9, 0.1, 0.0));
    if (!nearest || nearest.value().index != 1 ||
        std::abs(nearest.value().squared_distance - 0.02) > 1e-12)
        return 2;

    auto knn = index.value().get_knn(::sindre::math::Vector3::Zero(), 3);
    if (!knn || knn.value().size() != 3 || knn.value()[0].index != 0) return 3;

    auto radius = index.value().get_radius(::sindre::math::Vector3::Zero(), 1.01);
    if (!radius || radius.value().size() != 2) return 4;

    auto invalid = index.value().get_knn(::sindre::math::Vector3::Zero(), 0);
    if (invalid || invalid.error().context != "utils_3d.nearest_neighbors.knn") return 5;

    Vertices invalid_points(2, 2);
    auto invalid_index = NearestNeighborIndex::create(invalid_points);
    if (invalid_index || invalid_index.error().code != std::make_error_code(std::errc::invalid_argument))
        return 6;
    return 0;
}
