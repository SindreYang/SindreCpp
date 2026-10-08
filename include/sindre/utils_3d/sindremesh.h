#pragma once

/// @file
/// @brief 面向普通用户的高层网格快捷 façade。

#include "types.h"
#include "algorithms/mesh.h"

#include <filesystem>
#include <utility>

namespace sindre::utils_3d {

class SindreMesh {
    Mesh mesh_;

  public:
    SindreMesh() = default;
    SindreMesh(const Vertices &vertices, const Faces &faces) : mesh_(vertices, faces) {}
    explicit SindreMesh(const Mesh &mesh) : mesh_(mesh) {}
    explicit SindreMesh(Mesh &&mesh) noexcept : mesh_(std::move(mesh)) {}

    [[nodiscard]] static Result<SindreMesh> load(const std::filesystem::path &path);
    [[nodiscard]] Result<void> save(const std::filesystem::path &path) const;

    [[nodiscard]] const Mesh &mesh() const noexcept { return mesh_; }
    [[nodiscard]] SindreMesh clone() const { return *this; }
    [[nodiscard]] Mesh clone_mesh() const { return mesh_.clone(); }
    [[nodiscard]] Vertices vertices() const { return mesh_.vertices(); }
    [[nodiscard]] Faces faces() const { return mesh_.faces(); }
    [[nodiscard]] Matrix normals() const { return mesh_.vertex_normals(); }
    [[nodiscard]] Matrix vertex_normals() const { return mesh_.vertex_normals(); }
    [[nodiscard]] Matrix face_normals() const { return mesh_.face_normals(); }
    [[nodiscard]] Result<Labels> vertices_labels() const {
        return mesh_.get_labels(true);
    }
    [[nodiscard]] Result<Labels> faces_labels() const {
        return mesh_.get_labels(false);
    }
    [[nodiscard]] Index npoints() const noexcept { return mesh_.npoints(); }
    [[nodiscard]] Index nfaces() const noexcept { return mesh_.nfaces(); }
    [[nodiscard]] Index npoint() const noexcept { return mesh_.npoint(); }
    [[nodiscard]] Index nface() const noexcept { return mesh_.nface(); }
    [[nodiscard]] bool empty() const noexcept { return mesh_.empty(); }
    [[nodiscard]] Aabb get_aabb() const { return mesh_.get_aabb(); }
    [[nodiscard]] Obb get_obb() const { return mesh_.get_obb(); }
    [[nodiscard]] BoundingSphere get_min_sphere() const {
        return mesh_.get_min_sphere();
    }
    [[nodiscard]] bool has_data(const std::string &name, bool point = true) const {
        return mesh_.has_data(name, point);
    }
    [[nodiscard]] std::vector<std::string> data_names(bool point = true) const {
        return mesh_.data_names(point);
    }
    [[nodiscard]] Result<Matrix> get_data(
        const std::string &name, bool point = true) const {
        return mesh_.get_data(name, point);
    }
    [[nodiscard]] Result<Labels> get_labels(bool point = true) const {
        return mesh_.get_labels(point);
    }
    [[nodiscard]] Result<std::vector<SindreMesh>> split(bool max_area = true) const;
    [[nodiscard]] Result<std::vector<BoundaryLoop>> boundary(
        bool max_boundary = true, bool ordered = true) const;
    [[nodiscard]] Result<SindreMesh> join_with_strips(
        const SindreMesh &, const JoinStripsOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> simplify(
        std::size_t target_faces, const SimplifyOptions & = {}) const;
    [[nodiscard]] Result<BooleanPreflight> check_boolean(
        const SindreMesh &, BooleanOperation,
        const AlgorithmOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> boolean(
        const SindreMesh &, BooleanOperation,
        const AlgorithmOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> clip_curve(
        const Vertices &, const CurveClipOptions & = {}) const;

    [[nodiscard]] Result<SindreMesh> clean(const CleanOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> fix_mesh(const FixOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> repair(const RepairOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> smooth(const SmoothOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> deform(
        const Vertices &source_points, const Vertices &target_points,
        const DeformationOptions & = {}) const;
    [[nodiscard]] Result<GraphCutResult> optimize_labels(
        const Labels &, const GraphCutOptions & = {}) const;
    [[nodiscard]] Result<GraphCutResult> optimize_labels(
        const Matrix &, const GraphCutOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> decimate(const DecimateOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> remesh(const RemeshOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> uniformize(
        const UniformizeOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> fill_holes(const FillHolesOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> fill_hole(const FillHolesOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> fill_holes_by_cgal(
        const CgalFillHolesOptions & = {}) const;
    [[nodiscard]] Result<::sindre::math::VectorXd> get_curvature(
        CurvatureType = CurvatureType::mean, const CurvatureOptions & = {}) const;
    [[nodiscard]] Result<::sindre::math::VectorXd> get_curvature_by_cgal(
        CurvatureType = CurvatureType::mean, const CurvatureOptions & = {}) const;
    [[nodiscard]] Result<Vertices> sample(
        std::size_t count, const SampleOptions & = {}) const;
    [[nodiscard]] Result<Vertices> project_line(
        const Vertices &ordered_points, const AlgorithmOptions & = {}) const;
    [[nodiscard]] Result<PathResult> find_path(
        std::int64_t start_vertex, std::int64_t end_vertex,
        const PathOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> compute_normals(
        const MeshNormals & = {}, const AlgorithmOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> subdivide(
        int iterations = 1, const AlgorithmOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> subdivide_faces(
        const std::vector<std::int64_t> &face_indices,
        int iterations = 1, const AlgorithmOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> cut_plane(
        const ::sindre::math::Vector3 &origin,
        const ::sindre::math::Vector3 &normal,
        bool keep_negative = false,
        const AlgorithmOptions & = {}) const;
    [[nodiscard]] Result<SindreMesh> reverse_faces(
        const AlgorithmOptions & = {}) const;
};

} // namespace sindre::utils_3d
