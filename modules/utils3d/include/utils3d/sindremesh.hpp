#pragma once

// Public user-facing facade. The core namespace contains the individual VTK
// wrappers; SindreMesh composes their common mesh workflow and keeps the
// fluent API stable.
#include "core/vtk.hpp"
#include "algorithms.hpp"
#include <fstream>
#include <iomanip>

namespace sindrecpp::utils3d {

using Vertices = core::Vertices;
using Faces = core::Faces;
using Matrix = core::Matrix;
using Labels = core::Labels;
using MeshNormals = core::MeshNormals;
#if defined(SINDRECPP_UTILS3D_SHOW)
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
#if defined(SINDRECPP_UTILS3D_VTK_DATA)
using Data = core::Data;
using Image = core::Image;
using ImageInterpolation = core::ImageInterpolation;
using SindreData = Data;
using SindreImage = Image;
#if defined(SINDRECPP_UTILS3D_SHOW)
using VolumeStop = core::VolumeStop;
#endif
#endif
#if defined(SINDRECPP_UTILS3D_SHOW) && defined(SINDRECPP_UTILS3D_VTK_DATA)
using Plot = core::ShowPlot;
using PlotKind = core::PlotKind;
using ShowPlot = Plot;
#endif

class SindreMesh : public core::Mesh {
    using Base = core::Mesh;

    static std::string extension(const std::filesystem::path &path) {
        auto value = path.extension().string();
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }
    static void json_string(std::ostream &out, const std::string &value) {
        out << '"';
        constexpr char hex[] = "0123456789abcdef";
        for (const auto byte : value) {
            const auto c = static_cast<unsigned char>(byte);
            switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20)
                    out << "\\u00" << hex[c >> 4] << hex[c & 0x0f];
                else
                    out << static_cast<char>(c);
            }
        }
        out << '"';
    }
    static void json_matrix(std::ostream &out, const Matrix &values) {
        out << '[';
        for (Eigen::Index i = 0; i < values.rows(); ++i) {
            if (i)
                out << ',';
            out << '[';
            for (Eigen::Index j = 0; j < values.cols(); ++j) {
                if (j)
                    out << ',';
                out << std::setprecision(17) << values(i, j);
            }
            out << ']';
        }
        out << ']';
    }
    void save_json(const std::filesystem::path &path) const {
        std::ofstream out(path);
        if (!out)
            throw std::runtime_error("Cannot open JSON output: " + core::path_to_utf8(path));
        const auto v = vertices();
        const auto f = faces();
        out << "{\n  \"type\":\"vtkPolyData\",\n  \"vertices\":[";
        for (Eigen::Index i = 0; i < v.rows(); ++i) {
            if (i)
                out << ',';
            out << '[' << std::setprecision(17) << v(i, 0) << ',' << v(i, 1) << ',' << v(i, 2)
                << ']';
        }
        out << "],\n  \"faces\":[";
        for (Eigen::Index i = 0; i < f.rows(); ++i) {
            if (i)
                out << ',';
            out << '[' << f(i, 0) << ',' << f(i, 1) << ',' << f(i, 2) << ']';
        }
        out << "],\n  \"point_data\":{\n";
        const auto point_names = data_names(true);
        for (std::size_t i = 0; i < point_names.size(); ++i) {
            if (i)
                out << ",\n";
            out << "    ";
            json_string(out, point_names[i]);
            out << ':';
            json_matrix(out, get_pointdata(point_names[i]));
        }
        out << "\n  },\n  \"cell_data\":{\n";
        const auto cell_names = data_names(false);
        for (std::size_t i = 0; i < cell_names.size(); ++i) {
            if (i)
                out << ",\n";
            out << "    ";
            json_string(out, cell_names[i]);
            out << ':';
            json_matrix(out, get_celldata(cell_names[i]));
        }
        out << "\n  }\n}\n";
        if (!out)
            throw std::runtime_error("Cannot write JSON output: " + core::path_to_utf8(path));
    }

  public:
    SindreMesh() = default;
    SindreMesh(const Vertices &vertices, const Faces &faces) : Base(vertices, faces) {}
    explicit SindreMesh(const std::filesystem::path &path) : Base(path) {}
    explicit SindreMesh(vtkPolyData *data) : Base(data) {}
#if defined(SINDRECPP_UTILS3D_VTK_DATA)
    explicit SindreMesh(const Data &data) : Base(data.surface()) {}
    explicit SindreMesh(vtkDataObject *data) : SindreMesh(Data(data)) {}
#endif
    SindreMesh(const Base &mesh) : Base(mesh) {}
    SindreMesh(Base &&mesh) noexcept : Base(std::move(mesh)) {}

    SindreMesh &operator=(const Base &mesh) {
        Base::operator=(mesh);
        return *this;
    }
    SindreMesh &operator=(Base &&mesh) noexcept {
        Base::operator=(std::move(mesh));
        return *this;
    }

    SindreMesh clone() const { return SindreMesh(static_cast<const Base &>(*this)); }

    SindreMesh &load(const std::filesystem::path &path) {
        Base::load(path);
        return *this;
    }
    void save(const std::filesystem::path &path) const {
        const auto e = extension(path);
        if (e == ".json") {
            save_json(path);
            return;
        }
        Base::save(path);
    }

    SindreMesh &compute_normals(const MeshNormals &options = {}) {
        Base::compute_normals(options);
        return *this;
    }
    SindreMesh &apply_transform(const Eigen::Matrix4d &transform) {
        Base::apply_transform(transform);
        return *this;
    }
    SindreMesh &apply_transform(const Eigen::Matrix3d &transform) {
        Base::apply_transform(transform);
        return *this;
    }
    SindreMesh &apply_inv_transform(const Eigen::Matrix4d &transform) {
        Base::apply_inv_transform(transform);
        return *this;
    }
    SindreMesh &shift_xyz(const Eigen::Vector3d &offset) {
        Base::shift_xyz(offset);
        return *this;
    }
    SindreMesh &scale_xyz(const Eigen::Vector3d &scale) {
        Base::scale_xyz(scale);
        return *this;
    }
    SindreMesh &scale_xyz(double scale) {
        Base::scale_xyz(scale);
        return *this;
    }
    SindreMesh &rotate_xyz(const Eigen::Vector3d &degrees) {
        Base::rotate_xyz(degrees);
        return *this;
    }
    SindreMesh &clean(double tolerance = 0) {
        Base::operator=(Base::clean(tolerance));
        return *this;
    }
    SindreMesh &smooth(const SmoothOptions &options = {}) {
        Base::operator=(utils3d::smooth(static_cast<const Base &>(*this), options));
        return *this;
    }
    SindreMesh &decimate(const DecimateOptions &options = {}) {
        Base::operator=(utils3d::decimate(static_cast<const Base &>(*this), options));
        return *this;
    }
    SindreMesh &remesh(const RemeshOptions &options = {}) {
        Base::operator=(utils3d::remesh(static_cast<const Base &>(*this), options));
        return *this;
    }
    SindreMesh &fill_holes(Backend backend = Backend::automatic) {
        Base::operator=(utils3d::fill_holes(static_cast<const Base &>(*this), backend));
        return *this;
    }
    SindreMesh &fix_mesh(bool close_holes = true, Backend backend = Backend::automatic) {
        Base::operator=(utils3d::fix_mesh(static_cast<const Base &>(*this), close_holes, backend));
        return *this;
    }
    SindreMesh &subdivide(int iterations = 1) {
        Base::operator=(utils3d::subdivide(static_cast<const Base &>(*this), iterations));
        return *this;
    }
    SindreMesh &cut_plane(const Eigen::Vector3d &origin, const Eigen::Vector3d &normal,
                          bool keep_negative = false) {
        Base::operator=(utils3d::cut_plane(static_cast<const Base &>(*this), origin, normal,
                                           keep_negative));
        return *this;
    }
    SindreMesh &reverse_faces() {
        Base::operator=(utils3d::reverse_faces(static_cast<const Base &>(*this)));
        return *this;
    }

    SindreMesh filtered(vtkPolyDataAlgorithm *filter) const {
        return SindreMesh(Base::filtered(filter));
    }
    SindreMesh extract_faces(const std::vector<bool> &keep, bool compact = true) const {
        return SindreMesh(Base::extract_faces(keep, compact));
    }
    SindreMesh extract_region(const std::string &name, double lower, double upper,
                              bool point = false, bool all_vertices = true) const {
        return SindreMesh(Base::extract_region(name, lower, upper, point, all_vertices));
    }
    SindreMesh largest_component() const { return SindreMesh(Base::largest_component()); }
    std::vector<SindreMesh> split_component_by_faces() const {
        std::vector<SindreMesh> result;
        for (auto &part : Base::split_component_by_faces())
            result.emplace_back(std::move(part));
        return result;
    }

#if defined(SINDRECPP_UTILS3D_SHOW)
    ShowMesh show(const ShowOptions &options = {}) const {
        return core::show_mesh(this->get_native(), options);
    }
#endif
};

} // namespace sindrecpp::utils3d
