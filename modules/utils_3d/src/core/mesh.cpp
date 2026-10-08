#include <sindre/utils_3d/types.h>

#include "core/mesh.h"
#include "log.h"

#include <stdexcept>
#include <utility>

namespace sindre::utils_3d {

class Mesh::Impl {
  public:
    detail::legacy::Mesh value;
};

namespace {
Mesh make_mesh(const detail::legacy::Mesh &value) {
    Mesh result(value.vertices(), value.faces());
    for (const bool point : {true, false}) {
        for (const auto &name : value.data_names(point)) {
            if (name == "Labels")
                continue;
            try {
                const auto data = value.get_data(name, point);
                (void)result.set_data(name, data, point);
            } catch (...) {
                // Unsupported native array types are intentionally not exposed through
                // the stable matrix-only Mesh API.
            }
        }
        try {
            if (value.has_data("Labels", point))
                (void)result.set_labels(value.get_labels(point), point);
        } catch (...) {
        }
    }
    return result;
}

detail::legacy::Mesh make_legacy(const Mesh &mesh) {
    detail::legacy::Mesh result(mesh.vertices(), mesh.faces());
    for (const bool point : {true, false}) {
        for (const auto &name : mesh.data_names(point)) {
            if (name == "Labels")
                continue;
            const auto data = mesh.get_data(name, point);
            if (!data)
                throw std::runtime_error(data.error().describe());
            result.set_data(name, data.value(), point);
        }
        try {
            if (mesh.has_data("Labels", point)) {
                const auto labels = mesh.get_labels(point);
                if (labels)
                    result.set_labels(labels.value(), point);
            }
        } catch (...) {
        }
    }
    return result;
}

template <class Function>
auto guarded(Function &&function, const char *context)
    -> ::sindre::general::Result<decltype(function())> {
    using Value = decltype(function());
#if defined(SINDRE_NO_EXCEPTIONS)
    (void)context;
    return ::sindre::general::Result<Value>::success(function());
#else
    try {
        return ::sindre::general::Result<Value>::success(function());
    } catch (const std::invalid_argument &error) {
        detail::logging::error(context, error.what());
        return ::sindre::general::Result<Value>::failure(
            ::sindre::general::Error::make(std::errc::invalid_argument, error.what(), context));
    } catch (const std::out_of_range &error) {
        detail::logging::error(context, error.what());
        return ::sindre::general::Result<Value>::failure(
            ::sindre::general::Error::make(std::errc::result_out_of_range, error.what(), context));
    } catch (const std::exception &error) {
        detail::logging::error(context, error.what());
        return ::sindre::general::Result<Value>::failure(
            ::sindre::general::Error::make(std::errc::io_error, error.what(), context));
    } catch (...) {
        detail::logging::error(context, "Unknown mesh operation failure");
        return ::sindre::general::Result<Value>::failure(
            ::sindre::general::Error::make(std::errc::io_error, "Unknown mesh operation failure", context));
    }
#endif
}

template <class Function>
::sindre::general::Result<void> guarded_void(Function &&function, const char *context) {
#if defined(SINDRE_NO_EXCEPTIONS)
    (void)context;
    function();
    return ::sindre::general::Result<void>::success();
#else
    try {
        function();
        return ::sindre::general::Result<void>::success();
    } catch (const std::invalid_argument &error) {
        detail::logging::error(context, error.what());
        return ::sindre::general::Result<void>::failure(
            ::sindre::general::Error::make(std::errc::invalid_argument, error.what(), context));
    } catch (const std::out_of_range &error) {
        detail::logging::error(context, error.what());
        return ::sindre::general::Result<void>::failure(
            ::sindre::general::Error::make(std::errc::result_out_of_range, error.what(), context));
    } catch (const std::exception &error) {
        detail::logging::error(context, error.what());
        return ::sindre::general::Result<void>::failure(
            ::sindre::general::Error::make(std::errc::io_error, error.what(), context));
    } catch (...) {
        detail::logging::error(context, "Unknown mesh attribute failure");
        return ::sindre::general::Result<void>::failure(
            ::sindre::general::Error::make(std::errc::io_error, "Unknown mesh attribute failure", context));
    }
#endif
}
}

Mesh::Mesh() : impl_(std::make_shared<Impl>()) {}
Mesh::Mesh(const Vertices &vertices, const Faces &faces) : Mesh() {
    impl_->value.update_geometry(vertices, faces);
}
Mesh::Mesh(const Mesh &other) : Mesh() {
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
Index Mesh::npoints() const noexcept { return impl_ ? impl_->value.npoints() : 0; }
Index Mesh::nfaces() const noexcept { return impl_ ? impl_->value.nfaces() : 0; }
bool Mesh::empty() const noexcept { return npoints() == 0; }
Vertices Mesh::vertices() const { return impl_->value.vertices(); }
Faces Mesh::faces() const { return impl_->value.faces(); }
MeshCheckReport Mesh::check(double area_tolerance) const {
    const auto report = impl_->value.check(area_tolerance);
    return {report.duplicate_vertices, report.degenerate_faces, report.unused_vertices,
            report.boundary_edges, report.non_manifold_edges, report.edge_closed};
}
void Mesh::update_geometry(const Vertices &vertices, const Faces &faces) {
    impl_->value.update_geometry(vertices, faces);
}
void Mesh::update_geometry(const Vertices &vertices) { impl_->value.update_geometry(vertices); }
double Mesh::area() const { return impl_->value.area(); }
double Mesh::signed_volume() const { return impl_->value.signed_volume(); }
::sindre::math::Matrix<double, 2, 3> Mesh::bounds() const { return impl_->value.bounds(); }
Aabb Mesh::get_aabb() const { return impl_->value.get_aabb(); }
Obb Mesh::get_obb() const { return impl_->value.get_obb(); }
BoundingSphere Mesh::get_min_sphere() const { return impl_->value.get_min_sphere(); }
::sindre::math::Vector3 Mesh::dimensions() const { return impl_->value.dimensions(); }
::sindre::math::Vector3 Mesh::center() const { return impl_->value.center(); }
double Mesh::radius() const { return impl_->value.radius(); }
Vertices Mesh::faces_barycentre() const { return impl_->value.faces_barycentre(); }
::sindre::math::VectorXd Mesh::faces_area() const { return impl_->value.faces_area(); }
std::map<Mesh::Edge, std::vector<std::int64_t>> Mesh::edges_face() const {
    return impl_->value.edges_face();
}
std::vector<Mesh::Edge> Mesh::get_edges() const { return impl_->value.get_edges(); }
std::vector<Mesh::Edge> Mesh::get_boundary() const { return impl_->value.get_boundary(); }
std::vector<Mesh::Edge> Mesh::get_non_manifold_edges() const {
    return impl_->value.get_non_manifold_edges();
}
bool Mesh::is_watertight() const { return impl_->value.is_watertight(); }
Labels Mesh::get_near_idx(const Vertices &query) const { return impl_->value.get_near_idx(query); }
Matrix Mesh::vertex_normals() const { return impl_->value.vertex_normals(); }
Matrix Mesh::face_normals() const { return impl_->value.face_normals(); }
::sindre::math::VectorXd Mesh::get_curvature(CurvatureType type) const {
    return impl_->value.get_curvature(type);
}
std::vector<std::vector<std::int64_t>> Mesh::boundary_loops() const {
    return impl_->value.boundary_loops();
}
std::vector<std::vector<std::int64_t>> Mesh::get_vertex_adj_list() const {
    return impl_->value.get_vertex_adj_list();
}
std::vector<std::vector<std::int64_t>> Mesh::get_face_adj_list() const {
    return impl_->value.get_face_adj_list();
}
bool Mesh::has_data(const std::string &name, bool point) const {
    return impl_->value.has_data(name, point);
}
std::vector<std::string> Mesh::data_names(bool point) const {
    return impl_->value.data_names(point);
}
::sindre::general::Result<Matrix> Mesh::get_data(const std::string &name, bool point) const {
    return guarded([&] { return impl_->value.get_data(name, point); },
                   "utils_3d.mesh.get_data");
}
::sindre::general::Result<void> Mesh::set_data(const std::string &name,
                                               const Matrix &values, bool point) {
    return guarded_void([&] { impl_->value.set_data(name, values, point); },
                        "utils_3d.mesh.set_data");
}
::sindre::general::Result<void> Mesh::remove_data(const std::string &name, bool point) {
    return guarded_void([&] { impl_->value.remove_data(name, point); },
                        "utils_3d.mesh.remove_data");
}
::sindre::general::Result<void> Mesh::set_labels(const Labels &labels, bool point) {
    return guarded_void([&] { impl_->value.set_labels(labels, point); },
                        "utils_3d.mesh.set_labels");
}
::sindre::general::Result<Labels> Mesh::get_labels(bool point) const {
    return guarded([&] { return impl_->value.get_labels(point); },
                   "utils_3d.mesh.get_labels");
}

::sindre::general::Result<Mesh> load_mesh(const std::filesystem::path &path) {
    return guarded([&] {
        detail::legacy::Mesh value(path);
        return make_mesh(value);
    }, "utils_3d.load_mesh");
}

::sindre::general::Result<void> save_mesh(
    const Mesh &mesh, const std::filesystem::path &path) {
#if defined(SINDRE_NO_EXCEPTIONS)
    (void)mesh;
    (void)path;
    return ::sindre::general::Result<void>::failure(::sindre::general::Error::make(
        std::errc::operation_not_supported, "Mesh I/O requires exception-enabled VTK adapters",
        "utils_3d.save_mesh"));
#else
    try {
        detail::legacy::Mesh value = make_legacy(mesh);
        value.save(path);
        return ::sindre::general::Result<void>::success();
    } catch (const std::exception &error) {
        return ::sindre::general::Result<void>::failure(::sindre::general::Error::make(
            std::errc::io_error, error.what(), "utils_3d.save_mesh"));
    } catch (...) {
        return ::sindre::general::Result<void>::failure(::sindre::general::Error::make(
            std::errc::io_error, "Unknown mesh save failure", "utils_3d.save_mesh"));
    }
#endif
}

} // namespace sindre::utils_3d
