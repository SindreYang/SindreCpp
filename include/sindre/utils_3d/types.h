#pragma once

/// @file
/// @brief Utils_3d 的后端无关公共数据类型和通用算法契约。

#if !defined(SINDRE_WITH_UTILS_3D)
#error "Enable SINDRE_WITH_UTILS_3D and link sindre::utils_3d."
#endif

#include <sindre/general/core.h>
#include <sindre/math.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sindre::utils_3d {

using Vertices = ::sindre::math::Matrix<double, ::sindre::math::eigen::Dynamic, 3>;
using Faces = ::sindre::math::Matrix<std::int64_t, ::sindre::math::eigen::Dynamic, 3>;
using Matrix = ::sindre::math::Matrix<double>;
using Labels = ::sindre::math::Vector<std::int64_t>;
using Index = ::sindre::math::Index;

using Error = ::sindre::general::Error;
template <class T>
using Result = ::sindre::general::Result<T>;

using ProgressCallback = std::function<void(double)>;
using CancellationCallback = std::function<bool()>;

struct AlgorithmOptions {
    ProgressCallback progress;
    CancellationCallback cancellation;
    std::size_t max_points = 0;
};

struct AlgorithmReport {
    std::size_t input_points = 0;
    std::size_t output_points = 0;
    std::size_t input_faces = 0;
    std::size_t output_faces = 0;
    std::size_t iterations = 0;
    std::size_t changed_elements = 0;
    double residual = 0.0;
    bool converged = true;
};

template <class T>
struct AlgorithmResult {
    T value;
    AlgorithmReport report;
};

enum class CurvatureType {
    mean,
    gaussian,
    minimum_principal,
    maximum_principal
};

struct CurvatureOptions : AlgorithmOptions {
    /// CGAL 扩展球半径；负数使用邻接面，0 使用 CGAL 的极小半径。
    double ball_radius = -1.0;
};

struct MeshNormals {
    bool points = true;
    bool faces = true;
    bool consistent = true;
    bool auto_orient = false;
    bool flip = false;
    bool split = false;
    double feature_angle = 30.0;
};

struct MeshCheckReport {
    std::size_t duplicate_vertices = 0;
    std::size_t degenerate_faces = 0;
    std::size_t unused_vertices = 0;
    std::size_t boundary_edges = 0;
    std::size_t non_manifold_edges = 0;
    bool edge_closed = false;
};

/// @brief Axis-aligned bounding box of a mesh.
struct Aabb {
    ::sindre::math::Vector3 minimum = ::sindre::math::Vector3::Zero();
    ::sindre::math::Vector3 maximum = ::sindre::math::Vector3::Zero();
    ::sindre::math::Vector3 center = ::sindre::math::Vector3::Zero();
    ::sindre::math::Vector3 dimensions = ::sindre::math::Vector3::Zero();
};

/// @brief PCA-oriented bounding box of a mesh.
///
/// `axes` stores the orthonormal local axes in its columns. `half_extents`
/// stores the positive half length along each local axis.
struct Obb {
    ::sindre::math::Vector3 center = ::sindre::math::Vector3::Zero();
    ::sindre::math::Matrix3 axes = ::sindre::math::Matrix3::Identity();
    ::sindre::math::Vector3 half_extents = ::sindre::math::Vector3::Zero();
};

/// @brief Smallest enclosing sphere calculated from mesh vertices.
struct BoundingSphere {
    ::sindre::math::Vector3 center = ::sindre::math::Vector3::Zero();
    double radius = 0.0;
};

/// @brief 后端无关、拥有值语义的三角网格。
///
/// 公共接口只使用 Eigen、标准库和 General 的 Result。VTK、CGAL 和 PCL
/// 均隐藏在 utils_3d 的私有实现中。
class Mesh {
    class Impl;
    std::shared_ptr<Impl> impl_;

  public:
    using Edge = std::array<std::int64_t, 2>;

    Mesh();
    Mesh(const Vertices &vertices, const Faces &faces);
    Mesh(const Mesh &other);
    Mesh &operator=(const Mesh &other);
    Mesh(Mesh &&other) noexcept;
    Mesh &operator=(Mesh &&other) noexcept;
    ~Mesh();

    [[nodiscard]] Mesh clone() const;
    [[nodiscard]] Index npoints() const noexcept;
    [[nodiscard]] Index nfaces() const noexcept;
    [[nodiscard]] Index npoint() const noexcept { return npoints(); }
    [[nodiscard]] Index nface() const noexcept { return nfaces(); }
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] Vertices vertices() const;
    [[nodiscard]] Faces faces() const;
    [[nodiscard]] MeshCheckReport check(double area_tolerance = 0.0) const;

    void update_geometry(const Vertices &vertices, const Faces &faces);
    void update_geometry(const Vertices &vertices);

    [[nodiscard]] double area() const;
    [[nodiscard]] double signed_volume() const;
    [[nodiscard]] ::sindre::math::Matrix<double, 2, 3> bounds() const;
    [[nodiscard]] Aabb get_aabb() const;
    [[nodiscard]] Obb get_obb() const;
    [[nodiscard]] BoundingSphere get_min_sphere() const;
    [[nodiscard]] ::sindre::math::Vector3 dimensions() const;
    [[nodiscard]] ::sindre::math::Vector3 center() const;
    [[nodiscard]] double radius() const;
    [[nodiscard]] Vertices faces_barycentre() const;
    [[nodiscard]] ::sindre::math::VectorXd faces_area() const;
    [[nodiscard]] std::map<Edge, std::vector<std::int64_t>> edges_face() const;
    [[nodiscard]] std::vector<Edge> get_edges() const;
    [[nodiscard]] std::vector<Edge> get_boundary() const;
    [[nodiscard]] std::vector<Edge> get_non_manifold_edges() const;
    [[nodiscard]] bool is_watertight() const;
    [[nodiscard]] Labels get_near_idx(const Vertices &query) const;

    [[nodiscard]] Matrix vertex_normals() const;
    [[nodiscard]] Matrix face_normals() const;
    [[nodiscard]] ::sindre::math::VectorXd get_curvature(
        CurvatureType type = CurvatureType::mean) const;
    [[nodiscard]] std::vector<std::vector<std::int64_t>> boundary_loops() const;
    [[nodiscard]] std::vector<std::vector<std::int64_t>> get_vertex_adj_list() const;
    [[nodiscard]] std::vector<std::vector<std::int64_t>> get_face_adj_list() const;

    [[nodiscard]] bool has_data(const std::string &name, bool point = true) const;
    [[nodiscard]] std::vector<std::string> data_names(bool point = true) const;
    [[nodiscard]] Result<Matrix> get_data(const std::string &name, bool point = true) const;
    Result<void> set_data(const std::string &name, const Matrix &values, bool point = true);
    Result<void> remove_data(const std::string &name, bool point = true);
    Result<void> set_labels(const Labels &labels, bool point = true);
    [[nodiscard]] Result<Labels> get_labels(bool point = true) const;
};

Result<Mesh> load_mesh(const std::filesystem::path &path);
Result<void> save_mesh(const Mesh &mesh, const std::filesystem::path &path);

/// @brief 后端无关的点云值类型。
class PointCloud {
  public:
    using Colors = ::sindre::math::Matrix<std::uint8_t,
                                          ::sindre::math::eigen::Dynamic, 3>;
    using Intensity = ::sindre::math::Vector<float>;
    using Validity = ::sindre::math::Vector<std::uint8_t>;

    Vertices points;
    std::optional<Vertices> normals;
    std::optional<Colors> colors;
    std::optional<Intensity> intensity;
    std::optional<Labels> labels;
    std::optional<Validity> validity;
    std::string frame_id;
    std::unordered_map<std::string, std::string> metadata;

    [[nodiscard]] std::size_t size() const noexcept {
        return static_cast<std::size_t>(points.rows());
    }
    [[nodiscard]] bool empty() const noexcept { return points.rows() == 0; }
    [[nodiscard]] Result<void> validate() const;
};

} // namespace sindre::utils_3d
