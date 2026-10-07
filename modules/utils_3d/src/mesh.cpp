#include <sindre/utils_3d/mesh.h>
#if defined(SINDRE_UTILS_3D_SHOW)
#include <sindre/utils_3d/show.h>
#endif

#include "core/mesh.h"
#include <utility>

namespace sindre::utils_3d::core {

std::string path_to_utf8(const std::filesystem::path &path) {
#if defined(__cpp_char8_t)
    const auto value = path.u8string();
    return {reinterpret_cast<const char *>(value.data()), value.size()};
#else
    return path.u8string();
#endif
}

class Mesh::Impl {
  public:
    detail::legacy::Mesh value;
};

namespace {
Mesh wrap(detail::legacy::Mesh value) { return Mesh(value.get_native()); }
}

Mesh::Mesh() : impl_(std::make_shared<Impl>()) {}
Mesh::Mesh(const Vertices &vertices, const Faces &faces) : Mesh() {
    impl_->value.update_geometry(vertices, faces);
}
Mesh::Mesh(vtkPolyData *data) : impl_(std::make_shared<Impl>()) { impl_->value = detail::legacy::Mesh(data); }
Mesh::Mesh(const std::filesystem::path &path) : impl_(std::make_shared<Impl>()) { impl_->value.load(path); }
Mesh::Mesh(const Mesh &other) : impl_(std::make_shared<Impl>()) {
    if (other.impl_)
        impl_->value = other.impl_->value;
}
Mesh &Mesh::operator=(const Mesh &other) {
    if (this != &other) {
        if (!impl_)
            impl_ = std::make_shared<Impl>();
        impl_->value = other.impl_ ? other.impl_->value : detail::legacy::Mesh{};
    }
    return *this;
}
Mesh::Mesh(Mesh &&other) noexcept : impl_(std::move(other.impl_)) {
    // Keep a moved-from mesh valid and empty.  Callers commonly reuse a
    // moved-from value for state checks or assignment.
    if (!other.impl_)
        other.impl_ = std::make_shared<Impl>();
}
Mesh &Mesh::operator=(Mesh &&other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
        if (!other.impl_)
            other.impl_ = std::make_shared<Impl>();
    }
    return *this;
}
Mesh::~Mesh() = default;

Mesh Mesh::clone() const { return Mesh(*this); }
vtkPolyData *Mesh::get_native() const noexcept { return impl_ ? impl_->value.get_native() : nullptr; }
::sindre::math::Index Mesh::npoints() const { return impl_->value.npoints(); }
::sindre::math::Index Mesh::nfaces() const { return impl_->value.nfaces(); }
bool Mesh::empty() const { return impl_->value.empty(); }
#if defined(SINDRE_UTILS_3D_SHOW)
ShowMesh Mesh::show() const { return show_mesh(get_native(), {}); }
ShowMesh Mesh::show(const ShowOptions &options) const { return show_mesh(get_native(), options); }
#endif
::sindre::math::Matrix<double, 2, 3> Mesh::bounds() const { return impl_->value.bounds(); }
::sindre::math::Vector3 Mesh::dimensions() const { return impl_->value.dimensions(); }
Mesh Mesh::filtered(vtkPolyDataAlgorithm *filter) const { return wrap(impl_->value.filtered(filter)); }
vtkSmartPointer<vtkTrivialProducer> Mesh::pipeline_source() const { return impl_->value.pipeline_source(); }
Vertices Mesh::vertices() const { return impl_->value.vertices(); }
Faces Mesh::faces() const { return impl_->value.faces(); }
void Mesh::update_geometry(const Vertices &vertices, const Faces &faces) { impl_->value.update_geometry(vertices, faces); }
void Mesh::update_geometry(const Vertices &vertices) { impl_->value.update_geometry(vertices); }
void Mesh::update_faces(const std::vector<bool> &keep) { impl_->value.update_faces(keep); }
void Mesh::update_vertex(const std::vector<bool> &keep) { impl_->value.update_vertex(keep); }
double Mesh::area() const { return impl_->value.area(); }
double Mesh::signed_volume() const { return impl_->value.signed_volume(); }
Mesh::CheckReport Mesh::check(double area_tolerance) const {
    const auto report = impl_->value.check(area_tolerance);
    return {report.duplicate_vertices, report.degenerate_faces, report.unused_vertices,
            report.boundary_edges, report.non_manifold_edges, report.edge_closed};
}
void Mesh::load(const std::filesystem::path &path) { impl_->value.load(path); }
void Mesh::save(const std::filesystem::path &path) const { impl_->value.save(path); }
void Mesh::compute_normals(const MeshNormals &options) { impl_->value.compute_normals(options); }
bool Mesh::has_data(const std::string &name, bool point) const { return impl_->value.has_data(name, point); }
std::vector<std::string> Mesh::data_names(bool point) const { return impl_->value.data_names(point); }
void Mesh::remove_data(const std::string &name, bool point) { impl_->value.remove_data(name, point); }
void Mesh::rename_data(const std::string &old_name, const std::string &new_name, bool point) {
    impl_->value.rename_data(old_name, new_name, point);
}
void Mesh::clear_data(bool point) { impl_->value.clear_data(point); }
void Mesh::set_uv(const Matrix &uv) { impl_->value.set_uv(uv); }
Matrix Mesh::get_uv() const { return impl_->value.get_uv(); }
Mesh Mesh::extract_faces(const std::vector<bool> &keep, bool compact) const {
    return wrap(impl_->value.extract_faces(keep, compact));
}
Mesh Mesh::extract_region(const std::string &name, double lower, double upper, bool point,
                          bool all_vertices) const {
    return wrap(impl_->value.extract_region(name, lower, upper, point, all_vertices));
}
Matrix Mesh::vertex_normals() const { return impl_->value.vertex_normals(); }
Matrix Mesh::face_normals() const { return impl_->value.face_normals(); }
Matrix Mesh::get_pointdata(const std::string &name) const { return impl_->value.get_pointdata(name); }
Matrix Mesh::get_celldata(const std::string &name) const { return impl_->value.get_celldata(name); }
Matrix Mesh::get_data(const std::string &name, bool point) const { return impl_->value.get_data(name, point); }
void Mesh::set_data(const std::string &name, const Matrix &values, bool point) {
    impl_->value.set_data(name, values, point);
}
void Mesh::set_labels(const Labels &labels, bool point) { impl_->value.set_labels(labels, point); }
Labels Mesh::get_labels(bool point) const { return impl_->value.get_labels(point); }
void Mesh::set_vertex_labels(const Labels &labels) { impl_->value.set_vertex_labels(labels); }
void Mesh::set_faces_labels(const Labels &labels) { impl_->value.set_faces_labels(labels); }
Labels Mesh::get_vertex_labels() const { return impl_->value.get_vertex_labels(); }
Labels Mesh::get_faces_labels() const { return impl_->value.get_faces_labels(); }
Mesh &Mesh::apply_transform(const ::sindre::math::Matrix4 &transform) {
    // Math matrices are row-major aliases; convert explicitly to Eigen's
    // native Matrix4d so the legacy overload set cannot choose ambiguously.
    impl_->value.apply_transform(Eigen::Matrix4d(transform));
    return *this;
}
Mesh &Mesh::apply_transform(const ::sindre::math::Matrix3 &transform) {
    impl_->value.apply_transform(Eigen::Matrix3d(transform));
    return *this;
}
Mesh &Mesh::apply_inv_transform(const ::sindre::math::Matrix4 &transform) {
    impl_->value.apply_inv_transform(Eigen::Matrix4d(transform));
    return *this;
}
Mesh &Mesh::shift_xyz(const ::sindre::math::Vector3 &offset) {
    impl_->value.shift_xyz(offset);
    return *this;
}
Mesh &Mesh::scale_xyz(const ::sindre::math::Vector3 &scale) {
    impl_->value.scale_xyz(scale);
    return *this;
}
Mesh &Mesh::scale_xyz(double scale) {
    impl_->value.scale_xyz(scale);
    return *this;
}
Mesh &Mesh::rotate_xyz(const ::sindre::math::Vector3 &degrees) {
    impl_->value.rotate_xyz(degrees);
    return *this;
}
::sindre::math::Vector3 Mesh::center() const { return impl_->value.center(); }
double Mesh::radius() const { return impl_->value.radius(); }
Vertices Mesh::faces_barycentre() const { return impl_->value.faces_barycentre(); }
::sindre::math::VectorXd Mesh::faces_area() const { return impl_->value.faces_area(); }
std::map<Mesh::Edge, std::vector<std::int64_t>> Mesh::edges_face() const { return impl_->value.edges_face(); }
std::vector<Mesh::Edge> Mesh::get_edges() const { return impl_->value.get_edges(); }
std::vector<Mesh::Edge> Mesh::get_boundary() const { return impl_->value.get_boundary(); }
std::vector<Mesh::Edge> Mesh::get_non_manifold_edges() const { return impl_->value.get_non_manifold_edges(); }
std::vector<std::vector<std::int64_t>> Mesh::boundary_loops() const { return impl_->value.boundary_loops(); }
vtkSmartPointer<vtkPolyData> Mesh::feature_edges(double angle) const { return impl_->value.feature_edges(angle); }
std::vector<std::vector<std::int64_t>> Mesh::get_vertex_adj_list() const { return impl_->value.get_vertex_adj_list(); }
std::vector<std::vector<std::int64_t>> Mesh::get_face_adj_list() const { return impl_->value.get_face_adj_list(); }
bool Mesh::is_watertight() const { return impl_->value.is_watertight(); }
Labels Mesh::get_near_idx(const Vertices &query) const { return impl_->value.get_near_idx(query); }
Mesh Mesh::clean(double tolerance) const { return wrap(impl_->value.clean(tolerance)); }
Mesh Mesh::largest_component() const { return wrap(impl_->value.largest_component()); }
std::vector<Mesh> Mesh::split_component_by_faces() const {
    std::vector<Mesh> result;
    for (auto &part : impl_->value.split_component_by_faces())
        result.emplace_back(part.get_native());
    return result;
}
::sindre::math::VectorXd Mesh::get_curvature(bool mean) const {
    return impl_->value.get_curvature(mean);
}

} // namespace sindre::utils_3d::core
