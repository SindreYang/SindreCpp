#pragma once

#include "vtk.h"
#include "algorithms.h"
#include <filesystem>
#include <vector>

namespace sindre::utils_3d {

using Vertices = core::Vertices;
using Faces = core::Faces;
using Matrix = core::Matrix;
using Labels = core::Labels;
using MeshNormals = core::MeshNormals;
#if defined(SINDRE_UTILS_3D_SHOW)
using Color = core::Color;
using Position3 = core::Position3;
using Representation = core::Representation;
using MeshColorMode = core::MeshColorMode;
using ColorMap = core::ColorMap;
using MeshStyle = core::MeshStyle;
using ShowOptions = core::ShowOptions;
using MeshCamera = core::MeshCamera;
using MeshPick = core::MeshPick;
using ShowMesh = core::ShowMesh;
#endif
#if defined(SINDRE_UTILS_3D_VTK_DATA)
using Data = core::Data;
using Image = core::Image;
using ImageInterpolation = core::ImageInterpolation;
using SindreData = Data;
using SindreImage = Image;
#if defined(SINDRE_UTILS_3D_SHOW)
using VolumeStop = core::VolumeStop;
#endif
#endif
#if defined(SINDRE_UTILS_3D_SHOW) && defined(SINDRE_UTILS_3D_VTK_DATA)
using Plot = core::ShowPlot;
using PlotKind = core::PlotKind;
using ShowPlot = Plot;
#endif

/// @brief 面向用户的链式网格 facade，继承 CoreMesh 并返回自身引用。
class SindreMesh : public core::Mesh {
    using Base = core::Mesh;

  public:
    SindreMesh();
    SindreMesh(const Vertices &vertices, const Faces &faces);
    explicit SindreMesh(const std::filesystem::path &path);
    explicit SindreMesh(vtkPolyData *data);
#if defined(SINDRE_UTILS_3D_VTK_DATA)
    explicit SindreMesh(const Data &data);
    explicit SindreMesh(vtkDataObject *data);
#endif
    SindreMesh(const Base &mesh);
    SindreMesh(Base &&mesh) noexcept;

    SindreMesh &operator=(const Base &mesh);
    SindreMesh &operator=(Base &&mesh) noexcept;
    SindreMesh clone() const;
    SindreMesh &load(const std::filesystem::path &path);
    void save(const std::filesystem::path &path) const;
    SindreMesh &compute_normals(const MeshNormals &options = {});
    SindreMesh &apply_transform(const Eigen::Matrix4d &transform);
    SindreMesh &apply_transform(const Eigen::Matrix3d &transform);
    SindreMesh &apply_inv_transform(const Eigen::Matrix4d &transform);
    SindreMesh &shift_xyz(const Eigen::Vector3d &offset);
    SindreMesh &scale_xyz(const Eigen::Vector3d &scale);
    SindreMesh &scale_xyz(double scale);
    SindreMesh &rotate_xyz(const Eigen::Vector3d &degrees);
    SindreMesh &clean(double tolerance = 0);
    SindreMesh &smooth(const SmoothOptions &options = {});
    SindreMesh &decimate(const DecimateOptions &options = {});
    SindreMesh &remesh(const RemeshOptions &options = {});
    SindreMesh &fill_holes(Backend backend = Backend::automatic);
    SindreMesh &fix_mesh(bool close_holes = true, Backend backend = Backend::automatic);
    SindreMesh &subdivide(int iterations = 1);
    SindreMesh &cut_plane(const Eigen::Vector3d &origin, const Eigen::Vector3d &normal,
                          bool keep_negative = false);
    SindreMesh &reverse_faces();
    SindreMesh filtered(vtkPolyDataAlgorithm *filter) const;
    SindreMesh extract_faces(const std::vector<bool> &keep, bool compact = true) const;
    SindreMesh extract_region(const std::string &name, double lower, double upper,
                              bool point = false, bool all_vertices = true) const;
    SindreMesh largest_component() const;
    std::vector<SindreMesh> split_component_by_faces() const;
#if defined(SINDRE_UTILS_3D_SHOW)
    ShowMesh show(const ShowOptions &options = {}) const;
#endif
};

} // namespace sindre::utils_3d
