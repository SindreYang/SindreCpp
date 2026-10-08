#pragma once

/// @file
/// @brief Lightweight KD-tree nearest-neighbor queries for 3D point clouds.

#include "../types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sindre::utils_3d {

struct Neighbor {
    std::int64_t index = -1;
    double squared_distance = 0.0;
};

/// @brief Options used when constructing a nearest-neighbor index.
struct NearestNeighborOptions {
    std::size_t leaf_size = 10;
};

/// @brief An owning, reusable KD-tree over an N x 3 point matrix.
///
/// The implementation uses nanoflann privately. The public API only exposes
/// Sindre types, so the backend can be changed without changing consumers.
class NearestNeighborIndex {
public:
    NearestNeighborIndex();
    ~NearestNeighborIndex();

    NearestNeighborIndex(NearestNeighborIndex &&other) noexcept;
    NearestNeighborIndex &operator=(NearestNeighborIndex &&other) noexcept;
    NearestNeighborIndex(const NearestNeighborIndex &) = delete;
    NearestNeighborIndex &operator=(const NearestNeighborIndex &) = delete;

    static Result<NearestNeighborIndex> create(
        const Vertices &points, const NearestNeighborOptions &options = {});

    Result<void> rebuild(const Vertices &points,
                         const NearestNeighborOptions &options = {});
    [[nodiscard]] std::size_t size() const noexcept;

    Result<Neighbor> get_nearest(const ::sindre::math::Vector3 &query) const;
    Result<std::vector<Neighbor>> get_knn(
        const ::sindre::math::Vector3 &query, std::size_t count) const;
    Result<std::vector<Neighbor>> get_radius(
        const ::sindre::math::Vector3 &query, double radius,
        std::size_t max_count = 0) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sindre::utils_3d
