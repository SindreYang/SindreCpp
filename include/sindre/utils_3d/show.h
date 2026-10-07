#pragma once

/// @file
/// @brief VTK/ImGui 网格显示、交互和拾取接口。

#include "mesh.h"
#include <array>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <vtkType.h>
#if defined(SINDRE_UTILS_3D_VTK_DATA)
#include <vtkImageData.h>
#endif

class vtkActor;
class vtkDataObject;
class vtkImageData;
class vtkPolyData;
class vtkRenderer;
class vtkRenderWindow;
class vtkRenderWindowInteractor;

namespace sindre::utils_3d::core {

/// @brief RGB 颜色，分量范围通常为 [0, 1]。
using Color = std::array<double, 3>;
using Position3 = std::array<double, 3>;
enum class Representation { surface, wireframe, points };
enum class MeshColorMode { solid, scalar, direct, labels };
enum class ColorMap { cool_warm, viridis, gray };

struct MeshStyle {
    Color color{.72, .78, .88}, edge_color{.12, .14, .18};
    double opacity = 1, line_width = 1, point_size = 4;
    Representation representation = Representation::surface;
    bool edges = false, lighting = true, smooth_shading = true, backface_culling = false;
    double ambient = .15, diffuse = .75, specular = .2, specular_power = 30;
    MeshColorMode color_mode = MeshColorMode::solid;
    std::string array_name;
    bool point_data = true, scalar_bar = true;
    int component = 0;
    ColorMap colormap = ColorMap::viridis;
    bool automatic_range = true;
    std::array<double, 2> range{0, 1};
};

/// @brief 网格窗口、背景和交互模式配置。
struct ShowOptions {
    int width = 1000, height = 750;
    std::string title = "SindreMesh";
    Color background{.08, .09, .12}, background_top{.22, .25, .30};
    bool gradient = true, offscreen = false, interactive = true, axes = true;
    MeshStyle style;
};

struct MeshCamera {
    Position3 position{0, 0, 5}, focal_point{0, 0, 0}, view_up{0, 1, 0};
    bool parallel = false;
    double parallel_scale = 1, view_angle = 30;
};
struct MeshPick {
    bool hit = false;
    std::size_t mesh = std::numeric_limits<std::size_t>::max();
    vtkIdType face = -1, vertex = -1;
    Position3 position{0, 0, 0};
};

#if defined(SINDRE_UTILS_3D_VTK_DATA)
struct VolumeStop {
    double value;
    Color color;
    double opacity;
};
#endif

/// @brief 面向网格、体数据和交互事件的 VTK 渲染器封装。
class ShowMesh {
    class Impl;
    std::shared_ptr<Impl> impl_;

  public:
    explicit ShowMesh(const ShowOptions &options = {});
    ShowMesh(const ShowMesh &) = delete;
    ShowMesh &operator=(const ShowMesh &) = delete;
    ShowMesh(ShowMesh &&other) noexcept;
    ShowMesh &operator=(ShowMesh &&other) noexcept;
    ~ShowMesh();

    /// @brief 添加一个网格并返回稳定的对象索引。
    std::size_t add(vtkPolyData *data, const MeshStyle &style = {});
    template <class MeshType>
    auto add(const MeshType &mesh, const MeshStyle &style = {})
        -> decltype(mesh.get_native(), std::size_t{}) {
        return add(mesh.get_native(), style);
    }
#if defined(SINDRE_UTILS_3D_VTK_DATA)
    std::size_t add(vtkDataObject *data, const MeshStyle &style = {});
    ShowMesh &volume(vtkImageData *image, const std::vector<VolumeStop> &stops = {});
    template <class ImageType>
    ShowMesh &volume(const ImageType &image, const std::vector<VolumeStop> &stops = {}) {
        return volume(vtkImageData::SafeDownCast(image.get_native()), stops);
    }
    ShowMesh &image_slice(vtkImageData *image, int axis, int index);
    template <class ImageType>
    ShowMesh &image_slice(const ImageType &image, int axis, int index) {
        return image_slice(vtkImageData::SafeDownCast(image.get_native()), axis, index);
    }
#endif
    ShowMesh &style(std::size_t id, const MeshStyle &value);
    ShowMesh &visible(std::size_t id, bool value);
    ShowMesh &remove(std::size_t id);
    ShowMesh &axes(bool value = true, double length = 1);
    ShowMesh &bounds(std::size_t id, Color color = {1, 1, 1});
    ShowMesh &text(const std::string &message, int x = 15, int y = 15, int size = 18,
                   Color color = {1, 1, 1});
    ShowMesh &normals(std::size_t id, bool point = true, double length = .1,
                      vtkIdType stride = 1);
    ShowMesh &texture(std::size_t id, const std::filesystem::path &path);
    ShowMesh &clip_plane(std::size_t id, Position3 origin, Position3 normal);
    ShowMesh &clear_clipping(std::size_t id);
    ShowMesh &camera(const MeshCamera &value);
    ShowMesh &reset_camera();
    ShowMesh &rotate_camera(double azimuth, double elevation = 0, double roll = 0);
    ShowMesh &zoom(double factor);
    ShowMesh &light(Position3 position, Position3 focal_point = {0, 0, 0},
                    Color color = {1, 1, 1}, double intensity = 1);
    ShowMesh &on_pick(std::function<void(const MeshPick &)> callback);
    ShowMesh &on_key(std::function<void(const std::string &)> callback);
    MeshPick pick(double x, double y);
    /// @brief 渲染当前场景但不进入交互循环。
    ShowMesh &render();
    /// @brief 显示窗口；interactive=false 时只执行一次渲染。
    ShowMesh &show(bool interactive = true);
    /// @brief 将当前场景保存为 PNG 等 VTK 支持的图片格式。
    ShowMesh &screenshot(const std::filesystem::path &path, int scale = 1);
    vtkRenderer *get_renderer() const;
    vtkRenderWindow *get_window() const;
    vtkRenderWindowInteractor *get_interactor() const;
    vtkActor *get_actor(std::size_t id);
    vtkPolyData *get_data(std::size_t id);
};

ShowMesh show_mesh(vtkPolyData *mesh, const ShowOptions &options = {});

template <class MeshType>
auto show_mesh(const MeshType &mesh, const ShowOptions &options = {})
    -> decltype(mesh.get_native(), ShowMesh(options)) {
    return show_mesh(mesh.get_native(), options);
}

} // namespace sindre::utils_3d::core
