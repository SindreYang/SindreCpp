#include <sindre/utils_3d/show.h>

#include "core/show.h"

namespace sindre::utils_3d::core {

namespace {
detail::legacy::MeshStyle to_legacy(const MeshStyle &value) {
    detail::legacy::MeshStyle result;
    result.color = value.color;
    result.edge_color = value.edge_color;
    result.opacity = value.opacity;
    result.line_width = value.line_width;
    result.point_size = value.point_size;
    result.representation = static_cast<detail::legacy::Representation>(value.representation);
    result.edges = value.edges;
    result.lighting = value.lighting;
    result.smooth_shading = value.smooth_shading;
    result.backface_culling = value.backface_culling;
    result.ambient = value.ambient;
    result.diffuse = value.diffuse;
    result.specular = value.specular;
    result.specular_power = value.specular_power;
    result.color_mode = static_cast<detail::legacy::MeshColorMode>(value.color_mode);
    result.array_name = value.array_name;
    result.point_data = value.point_data;
    result.scalar_bar = value.scalar_bar;
    result.component = value.component;
    result.colormap = static_cast<detail::legacy::ColorMap>(value.colormap);
    result.automatic_range = value.automatic_range;
    result.range = value.range;
    return result;
}

detail::legacy::ShowOptions to_legacy(const ShowOptions &value) {
    detail::legacy::ShowOptions result;
    result.width = value.width;
    result.height = value.height;
    result.title = value.title;
    result.background = value.background;
    result.background_top = value.background_top;
    result.gradient = value.gradient;
    result.offscreen = value.offscreen;
    result.interactive = value.interactive;
    result.axes = value.axes;
    result.style = to_legacy(value.style);
    return result;
}

MeshPick from_legacy(const detail::legacy::MeshPick &value) {
    return {value.hit, value.mesh, value.face, value.vertex, value.position};
}
}

class ShowMesh::Impl {
  public:
    explicit Impl(const ShowOptions &options) : value(to_legacy(options)) {}
    detail::legacy::ShowMesh value;
};

ShowMesh::ShowMesh(const ShowOptions &options) : impl_(std::make_shared<Impl>(options)) {}
ShowMesh::ShowMesh(ShowMesh &&other) noexcept = default;
ShowMesh &ShowMesh::operator=(ShowMesh &&other) noexcept = default;
ShowMesh::~ShowMesh() = default;
std::size_t ShowMesh::add(vtkPolyData *data, const MeshStyle &style) {
    return impl_->value.add(data, to_legacy(style));
}
#if defined(SINDRE_UTILS_3D_VTK_DATA)
std::size_t ShowMesh::add(vtkDataObject *data, const MeshStyle &style) {
    return impl_->value.add(data, to_legacy(style));
}
ShowMesh &ShowMesh::volume(vtkImageData *image, const std::vector<VolumeStop> &stops) {
    std::vector<detail::legacy::VolumeStop> converted;
    converted.reserve(stops.size());
    for (const auto &stop : stops)
        converted.push_back({stop.value, stop.color, stop.opacity});
    impl_->value.volume(image, converted);
    return *this;
}
ShowMesh &ShowMesh::image_slice(vtkImageData *image, int axis, int index) {
    impl_->value.image_slice(image, axis, index);
    return *this;
}
#endif
ShowMesh &ShowMesh::style(std::size_t id, const MeshStyle &value) {
    impl_->value.style(id, to_legacy(value));
    return *this;
}
ShowMesh &ShowMesh::visible(std::size_t id, bool value) { impl_->value.visible(id, value); return *this; }
ShowMesh &ShowMesh::remove(std::size_t id) { impl_->value.remove(id); return *this; }
ShowMesh &ShowMesh::axes(bool value, double length) { impl_->value.axes(value, length); return *this; }
ShowMesh &ShowMesh::bounds(std::size_t id, Color color) { impl_->value.bounds(id, color); return *this; }
ShowMesh &ShowMesh::text(const std::string &message, int x, int y, int size, Color color) {
    impl_->value.text(message, x, y, size, color);
    return *this;
}
ShowMesh &ShowMesh::normals(std::size_t id, bool point, double length, vtkIdType stride) {
    impl_->value.normals(id, point, length, stride);
    return *this;
}
ShowMesh &ShowMesh::texture(std::size_t id, const std::filesystem::path &path) {
    impl_->value.texture(id, path);
    return *this;
}
ShowMesh &ShowMesh::clip_plane(std::size_t id, Position3 origin, Position3 normal) {
    impl_->value.clip_plane(id, origin, normal);
    return *this;
}
ShowMesh &ShowMesh::clear_clipping(std::size_t id) { impl_->value.clear_clipping(id); return *this; }
ShowMesh &ShowMesh::camera(const MeshCamera &value) {
    detail::legacy::MeshCamera converted{value.position, value.focal_point, value.view_up,
                                         value.parallel, value.parallel_scale, value.view_angle};
    impl_->value.camera(converted);
    return *this;
}
ShowMesh &ShowMesh::reset_camera() { impl_->value.reset_camera(); return *this; }
ShowMesh &ShowMesh::rotate_camera(double azimuth, double elevation, double roll) {
    impl_->value.rotate_camera(azimuth, elevation, roll);
    return *this;
}
ShowMesh &ShowMesh::zoom(double factor) { impl_->value.zoom(factor); return *this; }
ShowMesh &ShowMesh::light(Position3 position, Position3 focal_point, Color color, double intensity) {
    impl_->value.light(position, focal_point, color, intensity);
    return *this;
}
ShowMesh &ShowMesh::on_pick(std::function<void(const MeshPick &)> callback) {
    impl_->value.on_pick([callback = std::move(callback)](const detail::legacy::MeshPick &value) {
        if (callback)
            callback(from_legacy(value));
    });
    return *this;
}
ShowMesh &ShowMesh::on_key(std::function<void(const std::string &)> callback) {
    impl_->value.on_key(std::move(callback));
    return *this;
}
MeshPick ShowMesh::pick(double x, double y) { return from_legacy(impl_->value.pick(x, y)); }
ShowMesh &ShowMesh::render() { impl_->value.render(); return *this; }
ShowMesh &ShowMesh::show(bool interactive) { impl_->value.show(interactive); return *this; }
ShowMesh &ShowMesh::screenshot(const std::filesystem::path &path, int scale) {
    impl_->value.screenshot(path, scale);
    return *this;
}
vtkRenderer *ShowMesh::get_renderer() const { return impl_->value.get_renderer(); }
vtkRenderWindow *ShowMesh::get_window() const { return impl_->value.get_window(); }
vtkRenderWindowInteractor *ShowMesh::get_interactor() const { return impl_->value.get_interactor(); }
vtkActor *ShowMesh::get_actor(std::size_t id) { return impl_->value.get_actor(id); }
vtkPolyData *ShowMesh::get_data(std::size_t id) { return impl_->value.get_data(id); }

ShowMesh show_mesh(vtkPolyData *mesh, const ShowOptions &options) {
    ShowMesh viewer(options);
    viewer.add(mesh, options.style);
    viewer.reset_camera().show(options.interactive);
    return viewer;
}

} // namespace sindre::utils_3d::core
