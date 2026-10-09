#include <sindre/utils_3d/algorithms/nearest_neighbors.h>

#include <nanoflann.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string_view>
#include <system_error>
#include <utility>

namespace sindre::utils_3d {
namespace {

Result<void> validate_points(const Vertices &points,
                             const NearestNeighborOptions &options) {
    if (points.cols() != 3)
        return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                     "Nearest-neighbor points must have exactly 3 columns",
                                     "utils_3d.nearest_neighbors.points");
    if (options.leaf_size == 0)
        return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                     "Nearest-neighbor leaf_size must be positive",
                                     "utils_3d.nearest_neighbors.leaf_size");
    for (Index row = 0; row < points.rows(); ++row)
        for (Index col = 0; col < 3; ++col)
            if (!std::isfinite(points(row, col)))
                return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                             "Nearest-neighbor points must be finite",
                                             "utils_3d.nearest_neighbors.points");
    return Result<void>::success();
}

Result<void> validate_query(const ::sindre::math::Vector3 &query,
                            std::string_view context) {
    for (Index i = 0; i < 3; ++i)
        if (!std::isfinite(query(i)))
            return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                         "Nearest-neighbor query must be finite",
                                         std::string(context));
    return Result<void>::success();
}

} // namespace

class NearestNeighborIndex::Impl {
public:
    struct Dataset {
        Vertices points;

        std::size_t kdtree_get_point_count() const noexcept {
            return static_cast<std::size_t>(points.rows());
        }
        double kdtree_get_pt(std::size_t index, std::size_t dimension) const noexcept {
            return points(static_cast<Index>(index), static_cast<Index>(dimension));
        }
        template <class BoundingBox>
        bool kdtree_get_bbox(BoundingBox &) const noexcept {
            return false;
        }
    };

    using Tree = nanoflann::KDTreeSingleIndexAdaptor<
        nanoflann::L2_Simple_Adaptor<double, Dataset>, Dataset, 3, std::size_t>;

    Dataset dataset;
    std::unique_ptr<Tree> tree;
};

NearestNeighborIndex::NearestNeighborIndex() = default;
NearestNeighborIndex::~NearestNeighborIndex() = default;
NearestNeighborIndex::NearestNeighborIndex(NearestNeighborIndex &&other) noexcept = default;
NearestNeighborIndex &NearestNeighborIndex::operator=(NearestNeighborIndex &&other) noexcept = default;

Result<NearestNeighborIndex> NearestNeighborIndex::create(
    const Vertices &points, const NearestNeighborOptions &options) {
    NearestNeighborIndex result;
    auto status = result.rebuild(points, options);
    if (!status) return Result<NearestNeighborIndex>::failure(status.error());
    return Result<NearestNeighborIndex>::success(std::move(result));
}

Result<void> NearestNeighborIndex::rebuild(
    const Vertices &points, const NearestNeighborOptions &options) {
    auto valid = validate_points(points, options);
    if (!valid) return valid;

    auto next = std::make_unique<Impl>();
    next->dataset.points = points;
    next->tree = std::make_unique<Impl::Tree>(
        3, next->dataset,
        nanoflann::KDTreeSingleIndexAdaptorParams(options.leaf_size));
    next->tree->buildIndex();
    impl_ = std::move(next);
    return Result<void>::success();
}

std::size_t NearestNeighborIndex::size() const noexcept {
    return impl_ ? impl_->dataset.kdtree_get_point_count() : 0;
}

Result<Neighbor> NearestNeighborIndex::get_nearest(
    const ::sindre::math::Vector3 &query) const {
    auto valid = validate_query(query, "utils_3d.nearest_neighbors.nearest");
    if (!valid) return Result<Neighbor>::failure(valid.error());
    if (!impl_ || impl_->dataset.points.rows() == 0)
        return Result<Neighbor>::failure(std::make_error_code(std::errc::no_such_file_or_directory),
                                         "Nearest-neighbor index is empty",
                                         "utils_3d.nearest_neighbors.nearest");

    std::size_t index = 0;
    double distance = 0.0;
    const auto found = impl_->tree->knnSearch(query.data(), 1, &index, &distance);
    if (found != 1)
        return Result<Neighbor>::failure(std::make_error_code(std::errc::io_error),
                                         "Nearest-neighbor query returned no result",
                                         "utils_3d.nearest_neighbors.nearest");
    return Result<Neighbor>::success({static_cast<std::int64_t>(index), distance});
}

Result<std::vector<Neighbor>> NearestNeighborIndex::get_knn(
    const ::sindre::math::Vector3 &query, std::size_t count) const {
    auto valid = validate_query(query, "utils_3d.nearest_neighbors.knn");
    if (!valid) return Result<std::vector<Neighbor>>::failure(valid.error());
    if (count == 0)
        return Result<std::vector<Neighbor>>::failure(std::make_error_code(std::errc::invalid_argument),
                                                     "KNN count must be positive",
                                                     "utils_3d.nearest_neighbors.knn");
    if (!impl_ || impl_->dataset.points.rows() == 0)
        return Result<std::vector<Neighbor>>::failure(
            std::make_error_code(std::errc::no_such_file_or_directory), "Nearest-neighbor index is empty",
            "utils_3d.nearest_neighbors.knn");

    count = std::min(count, size());
    std::vector<std::size_t> indices(count);
    std::vector<double> distances(count);
    const auto found = impl_->tree->knnSearch(query.data(), count, indices.data(), distances.data());
    std::vector<Neighbor> result;
    result.reserve(found);
    for (std::size_t i = 0; i < found; ++i)
        result.push_back({static_cast<std::int64_t>(indices[i]), distances[i]});
    return Result<std::vector<Neighbor>>::success(std::move(result));
}

Result<std::vector<Neighbor>> NearestNeighborIndex::get_radius(
    const ::sindre::math::Vector3 &query, double radius, std::size_t max_count) const {
    auto valid = validate_query(query, "utils_3d.nearest_neighbors.radius");
    if (!valid) return Result<std::vector<Neighbor>>::failure(valid.error());
    if (!std::isfinite(radius) || radius < 0.0)
        return Result<std::vector<Neighbor>>::failure(
            std::make_error_code(std::errc::invalid_argument), "Radius must be finite and non-negative",
            "utils_3d.nearest_neighbors.radius");
    if (!impl_ || impl_->dataset.points.rows() == 0)
        return Result<std::vector<Neighbor>>::failure(
            std::make_error_code(std::errc::no_such_file_or_directory), "Nearest-neighbor index is empty",
            "utils_3d.nearest_neighbors.radius");

    using Match = nanoflann::ResultItem<std::size_t, double>;
    std::vector<Match> matches;
    impl_->tree->radiusSearch(query.data(), radius * radius, matches,
                              nanoflann::SearchParameters());
    if (max_count > 0 && matches.size() > max_count) matches.resize(max_count);
    std::vector<Neighbor> result;
    result.reserve(matches.size());
    for (const auto &[index, distance] : matches)
        result.push_back({static_cast<std::int64_t>(index), distance});
    return Result<std::vector<Neighbor>>::success(std::move(result));
}

} // namespace sindre::utils_3d
