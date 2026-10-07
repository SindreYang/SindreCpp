#include <sindre/utils_3d/sindremesh.h>

#include <fstream>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <stdexcept>
#include <utility>

namespace sindre::utils_3d {

namespace {
std::string extension(const std::filesystem::path &path) {
    auto value = path.extension().string();
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
void json_string(std::ostream &out, const std::string &value) {
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
void json_matrix(std::ostream &out, const Matrix &values) {
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
void save_json(const SindreMesh &mesh, const std::filesystem::path &path) {
    std::ofstream out(path);
    if (!out)
        throw std::runtime_error("Cannot open JSON output: " + core::path_to_utf8(path));
    const auto v = mesh.vertices();
    const auto f = mesh.faces();
    out << "{\n  \"type\":\"vtkPolyData\",\n  \"vertices\":[";
    for (Eigen::Index i = 0; i < v.rows(); ++i) {
        if (i)
            out << ',';
        out << '[' << std::setprecision(17) << v(i, 0) << ',' << v(i, 1) << ',' << v(i, 2) << ']';
    }
    out << "],\n  \"faces\":[";
    for (Eigen::Index i = 0; i < f.rows(); ++i) {
        if (i)
            out << ',';
        out << '[' << f(i, 0) << ',' << f(i, 1) << ',' << f(i, 2) << ']';
    }
    out << "],\n  \"point_data\":{\n";
    const auto point_names = mesh.data_names(true);
    for (std::size_t i = 0; i < point_names.size(); ++i) {
        if (i)
            out << ",\n";
        out << "    ";
        json_string(out, point_names[i]);
        out << ':';
        json_matrix(out, mesh.get_pointdata(point_names[i]));
    }
    out << "\n  },\n  \"cell_data\":{\n";
    const auto cell_names = mesh.data_names(false);
    for (std::size_t i = 0; i < cell_names.size(); ++i) {
        if (i)
            out << ",\n";
        out << "    ";
        json_string(out, cell_names[i]);
        out << ':';
        json_matrix(out, mesh.get_celldata(cell_names[i]));
    }
    out << "\n  }\n}\n";
    if (!out)
        throw std::runtime_error("Cannot write JSON output: " + core::path_to_utf8(path));
}
}

SindreMesh::SindreMesh() = default;
SindreMesh::SindreMesh(const Vertices &vertices, const Faces &faces) : Base(vertices, faces) {}
SindreMesh::SindreMesh(const std::filesystem::path &path) : Base(path) {}
SindreMesh::SindreMesh(vtkPolyData *data) : Base(data) {}
#if defined(SINDRE_UTILS_3D_VTK_DATA)
SindreMesh::SindreMesh(const Data &data) : Base(data.surface()) {}
SindreMesh::SindreMesh(vtkDataObject *data) : SindreMesh(Data(data)) {}
#endif
SindreMesh::SindreMesh(const Base &mesh) : Base(mesh) {}
SindreMesh::SindreMesh(Base &&mesh) noexcept : Base(std::move(mesh)) {}
SindreMesh &SindreMesh::operator=(const Base &mesh) { Base::operator=(mesh); return *this; }
SindreMesh &SindreMesh::operator=(Base &&mesh) noexcept { Base::operator=(std::move(mesh)); return *this; }
SindreMesh SindreMesh::clone() const { return SindreMesh(static_cast<const Base &>(*this)); }
SindreMesh &SindreMesh::load(const std::filesystem::path &path) { Base::load(path); return *this; }
void SindreMesh::save(const std::filesystem::path &path) const {
    if (extension(path) == ".json") {
        save_json(*this, path);
        return;
    }
    Base::save(path);
}
SindreMesh &SindreMesh::compute_normals(const MeshNormals &options) {
    Base::compute_normals(options); return *this;
}
SindreMesh &SindreMesh::apply_transform(const ::sindre::math::Matrix4 &transform) {
    Base::apply_transform(transform); return *this;
}
SindreMesh &SindreMesh::apply_transform(const ::sindre::math::Matrix3 &transform) {
    Base::apply_transform(transform); return *this;
}
SindreMesh &SindreMesh::apply_inv_transform(const ::sindre::math::Matrix4 &transform) {
    Base::apply_inv_transform(transform); return *this;
}
SindreMesh &SindreMesh::shift_xyz(const ::sindre::math::Vector3 &offset) { Base::shift_xyz(offset); return *this; }
SindreMesh &SindreMesh::scale_xyz(const ::sindre::math::Vector3 &scale) { Base::scale_xyz(scale); return *this; }
SindreMesh &SindreMesh::scale_xyz(double scale) { Base::scale_xyz(scale); return *this; }
SindreMesh &SindreMesh::rotate_xyz(const ::sindre::math::Vector3 &degrees) { Base::rotate_xyz(degrees); return *this; }
SindreMesh &SindreMesh::clean(double tolerance) { Base::operator=(Base::clean(tolerance)); return *this; }
SindreMesh &SindreMesh::smooth(const SmoothOptions &options) {
    Base::operator=(utils_3d::smooth(static_cast<const Base &>(*this), options)); return *this;
}
SindreMesh &SindreMesh::decimate(const DecimateOptions &options) {
    Base::operator=(utils_3d::decimate(static_cast<const Base &>(*this), options)); return *this;
}
SindreMesh &SindreMesh::remesh(const RemeshOptions &options) {
    Base::operator=(utils_3d::remesh(static_cast<const Base &>(*this), options)); return *this;
}
SindreMesh &SindreMesh::fill_holes(Backend backend) {
    Base::operator=(utils_3d::fill_holes(static_cast<const Base &>(*this), backend)); return *this;
}
SindreMesh &SindreMesh::fix_mesh(bool close_holes, Backend backend) {
    Base::operator=(utils_3d::fix_mesh(static_cast<const Base &>(*this), close_holes, backend)); return *this;
}
SindreMesh &SindreMesh::subdivide(int iterations) {
    Base::operator=(utils_3d::subdivide(static_cast<const Base &>(*this), iterations)); return *this;
}
SindreMesh &SindreMesh::cut_plane(const ::sindre::math::Vector3 &origin,
                                  const ::sindre::math::Vector3 &normal,
                                  bool keep_negative) {
    Base::operator=(utils_3d::cut_plane(static_cast<const Base &>(*this), origin, normal, keep_negative));
    return *this;
}
SindreMesh &SindreMesh::reverse_faces() {
    Base::operator=(utils_3d::reverse_faces(static_cast<const Base &>(*this))); return *this;
}
SindreMesh SindreMesh::filtered(vtkPolyDataAlgorithm *filter) const { return SindreMesh(Base::filtered(filter)); }
SindreMesh SindreMesh::extract_faces(const std::vector<bool> &keep, bool compact) const {
    return SindreMesh(Base::extract_faces(keep, compact));
}
SindreMesh SindreMesh::extract_region(const std::string &name, double lower, double upper,
                                      bool point, bool all_vertices) const {
    return SindreMesh(Base::extract_region(name, lower, upper, point, all_vertices));
}
SindreMesh SindreMesh::largest_component() const { return SindreMesh(Base::largest_component()); }
std::vector<SindreMesh> SindreMesh::split_component_by_faces() const {
    std::vector<SindreMesh> result;
    for (auto &part : Base::split_component_by_faces())
        result.emplace_back(std::move(part));
    return result;
}
#if defined(SINDRE_UTILS_3D_SHOW)
ShowMesh SindreMesh::show(const ShowOptions &options) const { return core::show_mesh(get_native(), options); }
#endif

} // namespace sindre::utils_3d
