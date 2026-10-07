#include <sindre/utils_3d/plot.h>

#include "core/plot.h"

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
}

class ShowPlot::Impl {
  public:
    explicit Impl(const ShowOptions &options) : value(to_legacy(options)) {}
    detail::legacy::ShowPlot value;
};

ShowPlot::ShowPlot(const ShowOptions &options) : impl_(std::make_shared<Impl>(options)) {}
ShowPlot::~ShowPlot() = default;
ShowPlot::ShowPlot(ShowPlot &&other) noexcept = default;
ShowPlot &ShowPlot::operator=(ShowPlot &&other) noexcept = default;
ShowPlot &ShowPlot::add_values(const std::vector<double> &x, const std::vector<double> &y,
                               const std::string &name, PlotKind kind, Color color) {
    impl_->value.add(x, y, name, static_cast<detail::legacy::PlotKind>(kind), color);
    return *this;
}
ShowPlot &ShowPlot::title(const std::string &value) { impl_->value.title(value); return *this; }
ShowPlot &ShowPlot::axis_titles(const std::string &x, const std::string &y) {
    impl_->value.axis_titles(x, y);
    return *this;
}
ShowPlot &ShowPlot::show(bool interactive) { impl_->value.show(interactive); return *this; }
ShowPlot &ShowPlot::screenshot(const std::filesystem::path &path) {
    impl_->value.screenshot(path);
    return *this;
}
vtkChartXY *ShowPlot::get_chart() const { return impl_->value.get_chart(); }

} // namespace sindre::utils_3d::core
