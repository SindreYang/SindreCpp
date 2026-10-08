#pragma once

/// @file
/// @brief 后端无关的三角网格算法接口。

#include "../types.h"

namespace sindre::utils_3d {

struct CleanOptions : AlgorithmOptions {
    double tolerance = 0.0;
    bool merge_duplicate_vertices = true;
    bool remove_unused_vertices = true;
};
struct FixOptions : AlgorithmOptions {
    double tolerance = 0.0;
    double degenerate_area_tolerance = 0.0;
    bool merge_duplicate_vertices = true;
    bool remove_unused_vertices = true;
    bool remove_degenerate_faces = true;
    bool remove_duplicate_faces = true;
    bool fill_holes = false;
    std::size_t max_hole_size = 0;
};
struct RepairOptions : AlgorithmOptions {
    bool close_holes = true;
    bool remove_degenerate = true;
    bool remove_duplicate_vertices = true;
};
struct DecimateOptions : AlgorithmOptions { std::size_t target_faces = 10000; };
enum class SimplifyBackend { vtk, cgal };
enum class SimplifyAlgorithm {
    decimate_pro,
    quadric_decimation,
    quadric_clustering,
    cgal_edge_collapse
};
struct SimplifyOptions : AlgorithmOptions {
    SimplifyBackend backend = SimplifyBackend::vtk;
    SimplifyAlgorithm algorithm = SimplifyAlgorithm::decimate_pro;
    bool preserve_topology = true;
    double feature_angle = 30.0;
};
enum class SmoothScope { global, local };
struct SmoothOptions : AlgorithmOptions {
    SmoothScope scope = SmoothScope::global;
    /// 局部平滑时直接指定要移动的顶点；索引可重复，但必须有效。
    std::vector<std::int64_t> vertex_indices;
    /// 局部平滑时指定面片，面片的三个顶点会被移动；可与 vertex_indices 合并。
    std::vector<std::int64_t> face_indices;
    int iterations = 20;
    double strength = 0.1;
    bool preserve_volume = true;
};
enum class DeformationMethod { thin_plate_spline };
enum class DeformationScope { global, local };
struct DeformationOptions : AlgorithmOptions {
    DeformationMethod method = DeformationMethod::thin_plate_spline;
    DeformationScope scope = DeformationScope::global;
    /// 局部变形时直接指定要更新的顶点；索引可重复，但必须有效。
    std::vector<std::int64_t> vertex_indices;
    /// 局部变形时指定面片，面片的三个顶点会被更新；可与 vertex_indices 合并。
    std::vector<std::int64_t> face_indices;
    /// 无量纲正则化系数；0 表示精确通过控制点，正值用于抑制病态输入。
    double regularization = 0.0;
};
/// @brief Connect corresponding boundary polylines with triangle strips.
struct JoinStripsOptions : AlgorithmOptions {
    /// Close each input polyline by connecting its last point back to its first point.
    bool closed = true;
};
enum class GraphCutLabelLevel { auto_detect, vertex, face };
enum class GraphCutAlgorithm { expansion, swap };
struct GraphCutOptions : AlgorithmOptions {
    GraphCutLabelLevel label_level = GraphCutLabelLevel::auto_detect;
    GraphCutAlgorithm algorithm = GraphCutAlgorithm::expansion;
    /// 负数自动根据一元势和几何边权估计；0 表示关闭二元平滑。
    double smooth_factor = -1.0;
    /// 大于 0 的概率温度；1 表示不做温度变换。
    double temperature = 1.0;
    /// 硬标签输入时可指定类别数；0 表示从最大标签推断。
    std::size_t class_count = 0;
    bool keep_label = true;
    unsigned max_iterations = 10;
};

struct GraphCutResult {
    Labels labels;
    GraphCutLabelLevel label_level = GraphCutLabelLevel::vertex;
    std::size_t class_count = 0;
    double energy_before = 0.0;
    double energy_after = 0.0;
    double smooth_factor = 0.0;
    std::size_t iterations = 0;
    bool converged = true;
};
enum class RemeshBackend { vtk, cgal };
struct RemeshOptions : AlgorithmOptions {
    double edge_length = 1.0;
    unsigned iterations = 3;
    RemeshBackend backend = RemeshBackend::vtk;
};
struct UniformizeOptions : AlgorithmOptions {
    double edge_length = 1.0;
    unsigned iterations = 3;
    double pass_band = 0.1;
    bool smooth = true;
    bool preserve_boundary = true;
    double feature_angle = 30.0;
    RemeshBackend backend = RemeshBackend::vtk;
};
using BoundaryLoop = std::vector<std::int64_t>;
enum class FillHoleMethod { vtk, ear_clipping };
struct FillHolesOptions : AlgorithmOptions {
    std::size_t max_hole_size = 0;
    FillHoleMethod method = FillHoleMethod::vtk;
    BoundaryLoop boundary_vertices;
};

enum class CgalHoleFillMethod {
    triangulate,
    triangulate_and_refine,
    triangulate_refine_and_fair
};

/// @brief CGAL 高级补洞参数。
///
/// `max_hole_edges` 和 `max_hole_diameter` 为 0 时不限制。提供
/// `boundary_vertices` 时只处理该完整边界环；不提供时处理所有满足限制的孔洞。
struct CgalFillHolesOptions : AlgorithmOptions {
    CgalHoleFillMethod method = CgalHoleFillMethod::triangulate_refine_and_fair;
    std::size_t max_hole_edges = 0;
    double max_hole_diameter = 0.0;
    BoundaryLoop boundary_vertices;
    bool use_delaunay_triangulation = true;
    bool use_2d_constrained_delaunay_triangulation = true;
    /// 小于 0 使用 CGAL 默认值；否则作为近似平面判定阈值。
    double threshold_distance = -1.0;
    bool do_not_use_cubic_algorithm = false;
    double density_control_factor = 1.4142135623730951;
    unsigned fairing_continuity = 1;
};

enum class SampleAlgorithm { uniform, random, farthest_point };
struct SampleOptions : AlgorithmOptions {
    SampleAlgorithm algorithm = SampleAlgorithm::uniform;
    std::uint64_t seed = 0;
    std::size_t candidate_count = 0;
};

struct PathResult {
    BoundaryLoop vertices;
    Vertices points;
    double length = 0.0;
};

class MeshPathCache;
struct PathOptions : AlgorithmOptions {
    std::shared_ptr<const MeshPathCache> cache;
};

enum class BooleanOperation { unite, intersect, subtract };

enum class CurveClipRegion { inside, outside };
enum class CurveSelectionMode { smallest_region, largest_region };

/// @brief 闭合曲线网格图切裁剪选项。
///
/// 曲线点会先投影到网格表面，然后使用网格边上的 Dijkstra 最短路径连接
/// 相邻曲线点。曲线不要求重复首点；算法会自动把最后一个点和第一个点闭合。
struct CurveClipOptions : AlgorithmOptions {
    CurveClipRegion region = CurveClipRegion::inside;
    CurveSelectionMode selection = CurveSelectionMode::smallest_region;
    /// 设为正数时限制曲线点到网格表面的最大投影距离；0 表示不限制。
    double max_projection_distance = 0.0;
};

struct BooleanPreflight {
    bool supported = false;
    bool can_execute = false;
    bool bounds_overlap = false;
    bool disjoint_shortcut = false;
    bool left_self_intersections_checked = false;
    bool right_self_intersections_checked = false;
    bool left_self_intersecting = false;
    bool right_self_intersecting = false;
    MeshCheckReport left_quality;
    MeshCheckReport right_quality;
    std::string reason;
};

Result<void> validate_mesh(const Mesh &, double area_tolerance = 0.0);
Result<AlgorithmResult<Mesh>> clean_mesh(const Mesh &, const CleanOptions & = {});
Result<AlgorithmResult<Mesh>> fix_mesh(const Mesh &, const FixOptions & = {});
Result<AlgorithmResult<Mesh>> simplify_mesh(
    const Mesh &, std::size_t target_faces, const SimplifyOptions & = {});
Result<std::vector<Mesh>> split_mesh_components(
    const Mesh &, bool max_area = true, const AlgorithmOptions & = {});
Result<std::vector<BoundaryLoop>> find_mesh_boundaries(
    const Mesh &, bool max_boundary = true, bool ordered = true,
    const AlgorithmOptions & = {});
Result<AlgorithmResult<Mesh>> repair_mesh(const Mesh &, const RepairOptions & = {});
Result<AlgorithmResult<Mesh>> decimate_mesh(const Mesh &, const DecimateOptions & = {});
Result<AlgorithmResult<Mesh>> smooth_mesh(const Mesh &, const SmoothOptions & = {});
Result<AlgorithmResult<Mesh>> deform_mesh(
    const Mesh &, const Vertices &source_points, const Vertices &target_points,
    const DeformationOptions & = {});
Result<AlgorithmResult<Mesh>> join_mesh_strips(
    const std::vector<Vertices> &, const std::vector<Vertices> &,
    const JoinStripsOptions & = {});
Result<AlgorithmResult<Mesh>> join_mesh_strips(
    const Mesh &, const Mesh &, const JoinStripsOptions & = {});
Result<GraphCutResult> optimize_mesh_labels(
    const Mesh &, const Labels &, const GraphCutOptions & = {});
Result<GraphCutResult> optimize_mesh_labels(
    const Mesh &, const Matrix &, const GraphCutOptions & = {});
Result<AlgorithmResult<Mesh>> remesh_surface(const Mesh &, const RemeshOptions & = {});
Result<AlgorithmResult<Mesh>> uniformize_mesh(
    const Mesh &, const UniformizeOptions & = {});
Result<AlgorithmResult<Mesh>> fill_mesh_holes(const Mesh &, const FillHolesOptions & = {});
Result<AlgorithmResult<Mesh>> fill_holes_by_cgal(
    const Mesh &, const CgalFillHolesOptions & = {});
Result<BooleanPreflight> check_boolean_mesh(
    const Mesh &, const Mesh &, BooleanOperation, const AlgorithmOptions & = {});
Result<AlgorithmResult<Mesh>> boolean_mesh(
    const Mesh &, const Mesh &, BooleanOperation, const AlgorithmOptions & = {});
Result<AlgorithmResult<Mesh>> clip_mesh_by_curve(
    const Mesh &, const Vertices &, const CurveClipOptions & = {});
Result<bool> has_mesh_self_intersections(const Mesh &, const AlgorithmOptions & = {});
Result<AlgorithmResult<Mesh>> subdivide_mesh(
    const Mesh &, int iterations = 1, const AlgorithmOptions & = {});
Result<AlgorithmResult<Mesh>> subdivide_mesh_faces(
    const Mesh &, const std::vector<std::int64_t> &face_indices,
    int iterations = 1, const AlgorithmOptions & = {});
Result<AlgorithmResult<Mesh>> cut_mesh_plane(
    const Mesh &, const ::sindre::math::Vector3 &, const ::sindre::math::Vector3 &,
    bool keep_negative = false, const AlgorithmOptions & = {});
Result<AlgorithmResult<Mesh>> reverse_mesh_faces(const Mesh &, const AlgorithmOptions & = {});
Result<AlgorithmResult<Mesh>> compute_mesh_normals(
    const Mesh &, const MeshNormals & = {}, const AlgorithmOptions & = {});

struct ProjectionResult {
    Vertices points;
    ::sindre::math::VectorXd distances;
    Labels face_ids;
};
Result<ProjectionResult> project_mesh_points(
    const Mesh &, const Vertices &, const AlgorithmOptions & = {});
Result<Vertices> project_mesh_line(
    const Mesh &, const Vertices &, const AlgorithmOptions & = {});
Result<PathResult> find_mesh_path(
    const Mesh &, std::int64_t start_vertex, std::int64_t end_vertex,
    const PathOptions & = {});
Result<::sindre::math::VectorXd> calculate_signed_distances(
    const Mesh &, const Vertices &, const AlgorithmOptions & = {});
Result<::sindre::math::VectorXd> calculate_mesh_curvature(
    const Mesh &, CurvatureType = CurvatureType::mean,
    const CurvatureOptions & = {});
Result<::sindre::math::VectorXd> get_curvature_by_cgal(
    const Mesh &, CurvatureType = CurvatureType::mean,
    const CurvatureOptions & = {});
Result<Vertices> sample_mesh_surface(
    const Mesh &, std::size_t count, const SampleOptions & = {});

class MeshPathCache {
  public:
    MeshPathCache();
    MeshPathCache(const MeshPathCache &);
    MeshPathCache &operator=(const MeshPathCache &);
    MeshPathCache(MeshPathCache &&) noexcept;
    MeshPathCache &operator=(MeshPathCache &&) noexcept;
    ~MeshPathCache();

    [[nodiscard]] static Result<MeshPathCache> create(const Mesh &);
    Result<void> reset(const Mesh &);
    [[nodiscard]] Result<PathResult> find_path(
        std::int64_t start_vertex, std::int64_t end_vertex,
        const AlgorithmOptions & = {}) const;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool matches(const Mesh &) const;

  private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace sindre::utils_3d
