#pragma once

#include "mesh.h"
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

namespace sindre::utils_3d {

/// @brief 返回指定算法使用的几何后端及其可用性。
using Vertices = core::Vertices;
using Faces = core::Faces;
using Matrix = core::Matrix;
using Labels = core::Labels;
using CoreMesh = core::Mesh;

/// @brief 使用平面切割网格并返回 VTK 数据对象。
vtkSmartPointer<vtkPolyData> slice_plane(const CoreMesh &mesh,
                                         const ::sindre::math::Vector3 &origin,
                                         const ::sindre::math::Vector3 &normal);
/// @brief 按轴对齐盒裁剪网格。
CoreMesh clip_box(const CoreMesh &mesh, const ::sindre::math::Vector3 &lower,
                  const ::sindre::math::Vector3 &upper, bool inside = true);
CoreMesh clip_sphere(const CoreMesh &mesh, const ::sindre::math::Vector3 &center, double radius,
                     bool inside = true);
CoreMesh append_meshes(const std::vector<CoreMesh> &meshes, bool merge_points = false,
                       double tolerance = 0);

/// @brief 几何算法后端选择。
enum class Backend { automatic, meshlib, cgal, open3d, igl, vcg, vtk };
/// @brief 可查询支持情况的几何操作类别。
enum class Operation {
    decimate, smooth, remesh, boolean_op, fill_holes, self_intersections, clean,
    curvature, uv, registration, reconstruction, sample
};

/// @brief 返回后端的稳定英文名称。
const char *backend_name(Backend backend);
/// @brief 判断当前构建是否包含指定后端。
bool backend_available(Backend backend);
/// @brief 返回当前构建中可用的后端。
std::vector<Backend> get_available_backends();
/// @brief 返回可执行指定操作的后端。
std::vector<Backend> get_supported_backends(Operation operation);
/// @brief 按请求后端选择实际执行后端。
Backend get_backend(Operation operation, Backend requested = Backend::automatic);

struct DecimateOptions {
    std::size_t target_faces = 10000;
    Backend backend = Backend::automatic;
};
/// @brief 简化三角面数量。
CoreMesh decimate(const CoreMesh &mesh, const DecimateOptions &options = {});

struct SmoothOptions {
    int iterations = 20;
    double strength = .1;
    bool preserve_volume = true;
    Backend backend = Backend::automatic;
};
/// @brief 平滑网格并按选项尽量保持体积。
CoreMesh smooth(const CoreMesh &mesh, const SmoothOptions &options = {});

struct RemeshOptions {
    double edge_length = 1;
    unsigned iterations = 3;
    Backend backend = Backend::automatic;
};
/// @brief 按目标边长重新生成三角网格。
CoreMesh remesh(const CoreMesh &mesh, const RemeshOptions &options = {});

enum class BooleanOperation { unite, intersect, subtract };
/// @brief 对两个网格执行布尔运算。
CoreMesh boolean_mesh(const CoreMesh &a, const CoreMesh &b, BooleanOperation operation,
                      Backend requested = Backend::automatic);
bool has_self_intersections(const CoreMesh &mesh, Backend requested = Backend::automatic);
CoreMesh fill_holes(const CoreMesh &mesh, Backend requested = Backend::automatic);
CoreMesh clean(const CoreMesh &mesh, Backend requested = Backend::automatic);
CoreMesh fix_mesh(const CoreMesh &mesh, bool close_holes = true,
                  Backend backend = Backend::automatic);
CoreMesh subdivide(const CoreMesh &mesh, int iterations = 1);
CoreMesh cut_plane(const CoreMesh &mesh, const ::sindre::math::Vector3 &origin,
                   const ::sindre::math::Vector3 &normal, bool keep_negative = false);
CoreMesh reverse_faces(const CoreMesh &mesh);

struct Projection {
    Vertices points;
    ::sindre::math::VectorXd distances;
    Labels face_ids;
};
Projection project_points(const CoreMesh &mesh, const Vertices &query);
::sindre::math::VectorXd signed_distance(const CoreMesh &mesh, const Vertices &query);
Labels labels_mapping(const Vertices &old_vertices, const Vertices &new_vertices,
                      const Labels &old_labels);
Labels vertex_labels_to_face_labels(const Faces &faces, const Labels &labels);
Labels face_labels_to_vertex_labels(const Faces &faces, const Labels &labels,
                                    ::sindre::math::Index n,
                                    std::int64_t unused_label = -1);

struct Normalization {
    ::sindre::math::Vector3 center;
    double scale;
    ::sindre::math::Matrix4 transform;
};
Normalization get_normalize(const CoreMesh &mesh);
Matrix get_gaussian_heatmap(const Vertices &points, const Vertices &keys, double sigma = .5,
                            bool normalize = false);
::sindre::math::VectorXd get_curvature(const CoreMesh &mesh,
                                       Backend requested = Backend::automatic);
Matrix get_uv(const CoreMesh &mesh);

struct Registration {
    ::sindre::math::Matrix4 transform;
    double fitness;
    double rmse;
};
Registration register_icp(const Vertices &source, const Vertices &target, double max_distance,
                          int iterations = 50,
                          const ::sindre::math::Matrix4 &initial =
                              ::sindre::math::Matrix4::Identity());
Vertices sample(const CoreMesh &mesh, std::size_t count);
CoreMesh reconstruct_poisson(const Vertices &points, const Vertices &normals,
                              std::size_t depth = 8);

} // namespace sindre::utils_3d
