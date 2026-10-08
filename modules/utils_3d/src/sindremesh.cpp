#include <sindre/utils_3d/sindremesh.h>

namespace sindre::utils_3d {
namespace {
template <class T>
Result<SindreMesh> to_facade(Result<AlgorithmResult<T>> result) {
    if (!result)
        return Result<SindreMesh>::failure(result.error());
    return Result<SindreMesh>::success(SindreMesh(std::move(result.value().value)));
}
}

Result<SindreMesh> SindreMesh::load(const std::filesystem::path &path) {
    auto result = load_mesh(path);
    if (!result)
        return Result<SindreMesh>::failure(result.error());
    return Result<SindreMesh>::success(SindreMesh(std::move(result.value())));
}

Result<void> SindreMesh::save(const std::filesystem::path &path) const {
    return save_mesh(mesh_, path);
}

Result<std::vector<SindreMesh>> SindreMesh::split(bool max_area) const {
    auto result = split_mesh_components(mesh_, max_area);
    if (!result)
        return Result<std::vector<SindreMesh>>::failure(result.error());
    std::vector<SindreMesh> output;
    output.reserve(result.value().size());
    for (auto &part : result.value())
        output.emplace_back(std::move(part));
    return Result<std::vector<SindreMesh>>::success(std::move(output));
}

Result<std::vector<BoundaryLoop>> SindreMesh::boundary(
    bool max_boundary, bool ordered) const {
    return find_mesh_boundaries(mesh_, max_boundary, ordered);
}

Result<SindreMesh> SindreMesh::join_with_strips(
    const SindreMesh &other, const JoinStripsOptions &options) const {
    return to_facade(join_mesh_strips(mesh_, other.mesh_, options));
}

Result<SindreMesh> SindreMesh::simplify(
    std::size_t target_faces, const SimplifyOptions &options) const {
    return to_facade(simplify_mesh(mesh_, target_faces, options));
}
Result<BooleanPreflight> SindreMesh::check_boolean(
    const SindreMesh &other, BooleanOperation operation,
    const AlgorithmOptions &options) const {
    return check_boolean_mesh(mesh_, other.mesh_, operation, options);
}
Result<SindreMesh> SindreMesh::boolean(
    const SindreMesh &other, BooleanOperation operation,
    const AlgorithmOptions &options) const {
    return to_facade(boolean_mesh(mesh_, other.mesh_, operation, options));
}
Result<SindreMesh> SindreMesh::clip_curve(
    const Vertices &curve, const CurveClipOptions &options) const {
    return to_facade(clip_mesh_by_curve(mesh_, curve, options));
}

Result<SindreMesh> SindreMesh::clean(const CleanOptions &options) const {
    return to_facade(clean_mesh(mesh_, options));
}
Result<SindreMesh> SindreMesh::fix_mesh(const FixOptions &options) const {
    return to_facade(sindre::utils_3d::fix_mesh(mesh_, options));
}
Result<SindreMesh> SindreMesh::repair(const RepairOptions &options) const {
    return to_facade(repair_mesh(mesh_, options));
}
Result<SindreMesh> SindreMesh::smooth(const SmoothOptions &options) const {
    return to_facade(smooth_mesh(mesh_, options));
}
Result<SindreMesh> SindreMesh::deform(
    const Vertices &source_points, const Vertices &target_points,
    const DeformationOptions &options) const {
    return to_facade(deform_mesh(mesh_, source_points, target_points, options));
}
Result<GraphCutResult> SindreMesh::optimize_labels(
    const Labels &labels, const GraphCutOptions &options) const {
    return optimize_mesh_labels(mesh_, labels, options);
}
Result<GraphCutResult> SindreMesh::optimize_labels(
    const Matrix &probabilities, const GraphCutOptions &options) const {
    return optimize_mesh_labels(mesh_, probabilities, options);
}
Result<SindreMesh> SindreMesh::decimate(const DecimateOptions &options) const {
    return to_facade(decimate_mesh(mesh_, options));
}
Result<SindreMesh> SindreMesh::remesh(const RemeshOptions &options) const {
    return to_facade(remesh_surface(mesh_, options));
}
Result<SindreMesh> SindreMesh::uniformize(const UniformizeOptions &options) const {
    return to_facade(uniformize_mesh(mesh_, options));
}
Result<SindreMesh> SindreMesh::fill_holes(const FillHolesOptions &options) const {
    return to_facade(fill_mesh_holes(mesh_, options));
}
Result<SindreMesh> SindreMesh::fill_hole(const FillHolesOptions &options) const {
    return to_facade(fill_mesh_holes(mesh_, options));
}
Result<SindreMesh> SindreMesh::fill_holes_by_cgal(
    const CgalFillHolesOptions &options) const {
    return to_facade(sindre::utils_3d::fill_holes_by_cgal(mesh_, options));
}
Result<::sindre::math::VectorXd> SindreMesh::get_curvature(
    CurvatureType type, const CurvatureOptions &options) const {
    return calculate_mesh_curvature(mesh_, type, options);
}
Result<::sindre::math::VectorXd> SindreMesh::get_curvature_by_cgal(
    CurvatureType type, const CurvatureOptions &options) const {
    return sindre::utils_3d::get_curvature_by_cgal(mesh_, type, options);
}
Result<Vertices> SindreMesh::sample(std::size_t count, const SampleOptions &options) const {
    return sample_mesh_surface(mesh_, count, options);
}
Result<Vertices> SindreMesh::project_line(const Vertices &ordered_points,
                                          const AlgorithmOptions &options) const {
    return project_mesh_line(mesh_, ordered_points, options);
}
Result<PathResult> SindreMesh::find_path(std::int64_t start_vertex,
                                         std::int64_t end_vertex,
                                         const PathOptions &options) const {
    return find_mesh_path(mesh_, start_vertex, end_vertex, options);
}
Result<SindreMesh> SindreMesh::compute_normals(const MeshNormals &normal_options,
                                               const AlgorithmOptions &options) const {
    return to_facade(compute_mesh_normals(mesh_, normal_options, options));
}
Result<SindreMesh> SindreMesh::subdivide(int iterations,
                                         const AlgorithmOptions &options) const {
    return to_facade(subdivide_mesh(mesh_, iterations, options));
}
Result<SindreMesh> SindreMesh::subdivide_faces(
    const std::vector<std::int64_t> &face_indices, int iterations,
    const AlgorithmOptions &options) const {
    return to_facade(subdivide_mesh_faces(mesh_, face_indices, iterations, options));
}
Result<SindreMesh> SindreMesh::cut_plane(const ::sindre::math::Vector3 &origin,
                                         const ::sindre::math::Vector3 &normal,
                                         bool keep_negative,
                                         const AlgorithmOptions &options) const {
    return to_facade(cut_mesh_plane(mesh_, origin, normal, keep_negative, options));
}
Result<SindreMesh> SindreMesh::reverse_faces(const AlgorithmOptions &options) const {
    return to_facade(reverse_mesh_faces(mesh_, options));
}

} // namespace sindre::utils_3d
