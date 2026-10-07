#pragma once

/// @file
/// @brief 三角网格值语义、I/O、拓扑和 Eigen 互操作接口。

#if !defined(SINDRE_WITH_UTILS_3D)
#error "Enable SINDRE_WITH_UTILS_3D and link sindre::utils_3d."
#endif

#include <sindre/math.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <vtkPolyData.h>
#include <vtkPolyDataAlgorithm.h>
#include <vtkSmartPointer.h>
#include <vtkTrivialProducer.h>

namespace sindre::utils_3d::core {

/// @brief 以行主序保存顶点坐标，形状为 N×3。
using Vertices = ::sindre::math::Matrix<double, ::sindre::math::eigen::Dynamic, 3>;
/// @brief 以行主序保存三角形索引，形状为 M×3。
using Faces = ::sindre::math::Matrix<std::int64_t, ::sindre::math::eigen::Dynamic, 3>;
using Matrix = ::sindre::math::Matrix<double>;
using Labels = ::sindre::math::Vector<std::int64_t>;
using Index = ::sindre::math::Index;

std::string path_to_utf8(const std::filesystem::path &path);

struct MeshNormals {
    bool points = true, faces = true, consistent = true, auto_orient = false;
    bool flip = false, split = false;
    double feature_angle = 30;
};

#if defined(SINDRE_UTILS_3D_SHOW)
struct ShowOptions;
class ShowMesh;
#endif

/// @brief 面向产品代码的 VTK 三角网格值语义封装。
/// @details Mesh 自带深拷贝、文件 I/O、拓扑查询和 Eigen 互操作；原生 VTK
/// 对象可通过 get_native() 访问，但其生命周期仍由 Mesh 管理。
class Mesh {
    class Impl;
    std::shared_ptr<Impl> impl_;

  public:
    using Edge = std::array<std::int64_t, 2>;

    Mesh();
    Mesh(const Vertices &vertices, const Faces &faces);
    explicit Mesh(vtkPolyData *data);
    explicit Mesh(const std::filesystem::path &path);
    Mesh(const Mesh &other);
    Mesh &operator=(const Mesh &other);
    Mesh(Mesh &&other) noexcept;
    Mesh &operator=(Mesh &&other) noexcept;
    ~Mesh();

    /// @brief 创建一个独立的深拷贝。
    Mesh clone() const;
    /// @brief 返回底层 vtkPolyData；仅在 Mesh 仍存活时有效。
    vtkPolyData *get_native() const noexcept;
    /// @brief 返回顶点数量。
    Index npoints() const;
    /// @brief 返回三角形数量。
    Index nfaces() const;
    bool empty() const;
#if defined(SINDRE_UTILS_3D_SHOW)
    ShowMesh show() const;
    ShowMesh show(const ShowOptions &options) const;
#endif
    ::sindre::math::Matrix<double, 2, 3> bounds() const;
    ::sindre::math::Vector3 dimensions() const;
    Mesh filtered(vtkPolyDataAlgorithm *filter) const;
    vtkSmartPointer<vtkTrivialProducer> pipeline_source() const;

    Vertices vertices() const;
    Faces faces() const;
    /// @brief 以顶点和三角形索引替换几何数据。
    void update_geometry(const Vertices &vertices, const Faces &faces);
    void update_geometry(const Vertices &vertices);
    void update_faces(const std::vector<bool> &keep);
    void update_vertex(const std::vector<bool> &keep);
    /// @brief 计算网格表面积。
    double area() const;
    /// @brief 计算有向体积。
    double signed_volume() const;

    struct CheckReport {
        std::size_t duplicate_vertices = 0, degenerate_faces = 0, unused_vertices = 0;
        std::size_t boundary_edges = 0, non_manifold_edges = 0;
        bool edge_closed = false;
    };
    /// @brief 检查退化、重复、边界和非流形拓扑。
    CheckReport check(double area_tolerance = 0) const;

    /// @brief 从 VTK 支持的网格文件加载数据。
    void load(const std::filesystem::path &path);
    /// @brief 将网格保存到由扩展名决定格式的文件。
    void save(const std::filesystem::path &path) const;
    void compute_normals(const MeshNormals &options = {});
    bool has_data(const std::string &name, bool point = true) const;
    std::vector<std::string> data_names(bool point = true) const;
    void remove_data(const std::string &name, bool point = true);
    void rename_data(const std::string &old_name, const std::string &new_name,
                     bool point = true);
    void clear_data(bool point = true);
    void set_uv(const Matrix &uv);
    Matrix get_uv() const;
    Mesh extract_faces(const std::vector<bool> &keep, bool compact = true) const;
    Mesh extract_region(const std::string &name, double lower, double upper,
                        bool point = false, bool all_vertices = true) const;
    Matrix vertex_normals() const;
    Matrix face_normals() const;
    Matrix get_pointdata(const std::string &name) const;
    Matrix get_celldata(const std::string &name) const;
    Matrix get_data(const std::string &name, bool point) const;
    void set_data(const std::string &name, const Matrix &values, bool point = true);
    void set_labels(const Labels &labels, bool point);
    Labels get_labels(bool point) const;
    void set_vertex_labels(const Labels &labels);
    void set_faces_labels(const Labels &labels);
    Labels get_vertex_labels() const;
    Labels get_faces_labels() const;

    /// @brief 原地应用齐次变换并返回自身，便于链式调用。
    Mesh &apply_transform(const ::sindre::math::Matrix4 &transform);
    Mesh &apply_transform(const ::sindre::math::Matrix3 &transform);
    Mesh &apply_inv_transform(const ::sindre::math::Matrix4 &transform);
    Mesh &shift_xyz(const ::sindre::math::Vector3 &offset);
    Mesh &scale_xyz(const ::sindre::math::Vector3 &scale);
    Mesh &scale_xyz(double scale);
    Mesh &rotate_xyz(const ::sindre::math::Vector3 &degrees);
    ::sindre::math::Vector3 center() const;
    double radius() const;
    Vertices faces_barycentre() const;
    ::sindre::math::VectorXd faces_area() const;
    std::map<Edge, std::vector<std::int64_t>> edges_face() const;
    std::vector<Edge> get_edges() const;
    std::vector<Edge> get_boundary() const;
    std::vector<Edge> get_non_manifold_edges() const;
    std::vector<std::vector<std::int64_t>> boundary_loops() const;
    vtkSmartPointer<vtkPolyData> feature_edges(double angle = 30) const;
    std::vector<std::vector<std::int64_t>> get_vertex_adj_list() const;
    std::vector<std::vector<std::int64_t>> get_face_adj_list() const;
    bool is_watertight() const;
    Labels get_near_idx(const Vertices &query) const;
    Mesh clean(double tolerance = 0) const;
    Mesh largest_component() const;
    std::vector<Mesh> split_component_by_faces() const;
    ::sindre::math::VectorXd get_curvature(bool mean = true) const;
};

} // namespace sindre::utils_3d::core
