#include <sindre/utils_3d/data.h>

#include "core/data.h"
#include <utility>

namespace sindre::utils_3d::core {

class Data::Impl {
  public:
    detail::legacy::Data value;
    explicit Impl(vtkDataObject *object) : value(object) {}
    explicit Impl(detail::legacy::Data object) : value(std::move(object)) {}
};

namespace {
Data wrap(detail::legacy::Data value) { return Data(value.get_native()); }
Mesh wrap_mesh(detail::legacy::Mesh value) { return Mesh(value.get_native()); }
}

Data::Data(vtkDataObject *input) : impl_(std::make_shared<Impl>(input)) {}
Data::Data(const Data &other)
    : impl_(other.impl_ ? std::make_shared<Impl>(other.impl_->value) : nullptr) {}
Data &Data::operator=(const Data &other) {
    if (this != &other)
        impl_ = other.impl_ ? std::make_shared<Impl>(other.impl_->value) : nullptr;
    return *this;
}
Data::Data(Data &&other) noexcept : impl_(std::move(other.impl_)) {}
Data &Data::operator=(Data &&other) noexcept {
    if (this != &other)
        impl_ = std::move(other.impl_);
    return *this;
}
Data::~Data() = default;

vtkDataObject *Data::get_native() const { return impl_ ? impl_->value.get_native() : nullptr; }
vtkDataSet *Data::dataset() const { return impl_->value.dataset(); }
std::string Data::type() const { return impl_->value.type(); }
vtkIdType Data::npoints() const { return impl_->value.npoints(); }
vtkIdType Data::ncells() const { return impl_->value.ncells(); }
Vertices Data::points() const { return impl_->value.points(); }
Data Data::point_cloud(const Vertices &points) { return wrap(detail::legacy::Data::point_cloud(points)); }
Data Data::polyline(const Vertices &points, bool closed) { return wrap(detail::legacy::Data::polyline(points, closed)); }
Data Data::structured_grid(const Vertices &points, std::array<int, 3> dimensions) {
    return wrap(detail::legacy::Data::structured_grid(points, dimensions));
}
Data Data::rectilinear_grid(const Eigen::VectorXd &x, const Eigen::VectorXd &y,
                            const Eigen::VectorXd &z) {
    return wrap(detail::legacy::Data::rectilinear_grid(x, y, z));
}
Data Data::tetrahedra(
    const Vertices &points,
    const Eigen::Matrix<std::int64_t, Eigen::Dynamic, 4, Eigen::RowMajor> &cells) {
    return wrap(detail::legacy::Data::tetrahedra(points, cells));
}
Data Data::blocks(const std::vector<Data> &items) {
    std::vector<detail::legacy::Data> legacy;
    legacy.reserve(items.size());
    for (const auto &item : items)
        legacy.emplace_back(item.impl_->value.get_native());
    return wrap(detail::legacy::Data::blocks(legacy));
}
unsigned Data::nblocks() const { return impl_->value.nblocks(); }
Data Data::block(unsigned index) const { return wrap(impl_->value.block(index)); }
void Data::set_data(const std::string &name, const Matrix &values, bool point) {
    impl_->value.set_data(name, values, point);
}
Matrix Data::get_data(const std::string &name, bool point) const { return impl_->value.get_data(name, point); }
vtkSmartPointer<vtkTrivialProducer> Data::pipeline_source() const {
    return impl_->value.pipeline_source();
}
Mesh Data::surface() const { return wrap_mesh(impl_->value.surface()); }
Data Data::contour(const std::string &scalar, const std::vector<double> &levels) const {
    return wrap(impl_->value.contour(scalar, levels));
}
Data Data::threshold(const std::string &scalar, double lower, double upper, bool point) const {
    return wrap(impl_->value.threshold(scalar, lower, upper, point));
}
Data Data::clip_plane(const Eigen::Vector3d &origin, const Eigen::Vector3d &normal, bool inside) const {
    return wrap(impl_->value.clip_plane(origin, normal, inside));
}
Data Data::gradient(const std::string &name, bool point, bool vorticity, bool divergence) const {
    return wrap(impl_->value.gradient(name, point, vorticity, divergence));
}
Data Data::warp_vector(const std::string &name, double scale) const { return wrap(impl_->value.warp_vector(name, scale)); }
Data Data::warp_scalar(const std::string &name, double scale, Eigen::Vector3d normal) const {
    return wrap(impl_->value.warp_scalar(name, scale, normal));
}
Data Data::point_to_cell_data() const { return wrap(impl_->value.point_to_cell_data()); }
Data Data::cell_to_point_data() const { return wrap(impl_->value.cell_to_point_data()); }
Data Data::probe(const Data &source) const { return wrap(impl_->value.probe(source.impl_->value)); }
Data Data::connected_regions(bool largest) const { return wrap(impl_->value.connected_regions(largest)); }
Data Data::calculate(const std::string &expression, const std::vector<std::string> &variables,
                     const std::string &output, bool point) const {
    return wrap(impl_->value.calculate(expression, variables, output, point));
}
Data Data::delaunay(bool three_dimensional) const { return wrap(impl_->value.delaunay(three_dimensional)); }
Data Data::tube(double radius, int sides) const { return wrap(impl_->value.tube(radius, sides)); }
Data Data::glyph_vectors(const std::string &name, double scale) const {
    return wrap(impl_->value.glyph_vectors(name, scale));
}
Data Data::streamlines(const std::string &vectors, const Vertices &seeds, double length) const {
    return wrap(impl_->value.streamlines(vectors, seeds, length));
}
Data Data::load(const std::filesystem::path &path) { return wrap(detail::legacy::Data::load(path)); }
void Data::save(const std::filesystem::path &path) const { impl_->value.save(path); }

} // namespace sindre::utils_3d::core
