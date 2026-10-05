#pragma once
#if !defined(SINDRECPP_UTILS3D_SHOW)
#error "Enable SINDRECPP_UTILS3D_SHOW and link SindreCpp::Utils3d."
#endif

// Standalone viewer: no SindreMesh or geometry-backend headers are required.
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <vtkActor.h>
#include <vtkAutoInit.h>
#include <vtkAxesActor.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCellPicker.h>
#include <vtkColorTransferFunction.h>
#include <vtkCommand.h>
#include <vtkDataArray.h>
#include <vtkIdTypeArray.h>
#include <vtkImageData.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkJPEGReader.h>
#include <vtkLight.h>
#include <vtkNew.h>
#include <vtkOutlineFilter.h>
#include <vtkPNGReader.h>
#include <vtkPNGWriter.h>
#include <vtkPlane.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataNormals.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkScalarBarActor.h>
#include <vtkSmartPointer.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkTexture.h>
#include <vtkUnsignedCharArray.h>
#include <vtkVertexGlyphFilter.h>
#include <vtkWindowToImageFilter.h>

// Factory registration also works for downstream header-only consumers.
VTK_MODULE_INIT(vtkRenderingOpenGL2);
VTK_MODULE_INIT(vtkRenderingFreeType);
VTK_MODULE_INIT(vtkInteractionStyle);

namespace sindrecpp::utils3d {
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

class ShowMesh {
    struct Entry {
        vtkSmartPointer<vtkPolyData> data;
        vtkSmartPointer<vtkPolyDataMapper> mapper;
        vtkSmartPointer<vtkActor> actor;
        vtkSmartPointer<vtkScalarBarActor> bar;
        MeshStyle style;
        bool active = true;
    };
    struct State {
        vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
        vtkSmartPointer<vtkRenderWindow> window = vtkSmartPointer<vtkRenderWindow>::New();
        vtkSmartPointer<vtkRenderWindowInteractor> interactor;
        vtkSmartPointer<vtkAxesActor> axes;
        std::vector<Entry> entries;
        std::vector<vtkSmartPointer<vtkCallbackCommand>> observers;
        std::function<void(const MeshPick &)> pick_callback;
        std::function<void(const std::string &)> key_callback;
        std::exception_ptr event_error;
        ShowOptions options;
    };
    std::shared_ptr<State> state_;
    static void finite(const Position3 &v) {
        for (auto x : v)
            if (!std::isfinite(x))
                throw std::invalid_argument("Nonfinite display coordinate");
    }
    static void color_valid(const Color &c) {
        finite(c);
        for (auto x : c)
            if (x < 0 || x > 1)
                throw std::invalid_argument("Colors must lie in [0,1]");
    }
    static void unit(double x, const char *name) {
        if (!std::isfinite(x) || x < 0 || x > 1)
            throw std::invalid_argument(name);
    }
    static void positive(double x, const char *name) {
        if (!std::isfinite(x) || x <= 0)
            throw std::invalid_argument(name);
    }
    Entry &entry(std::size_t id) {
        if (id >= state_->entries.size() || !state_->entries[id].active)
            throw std::out_of_range("Display mesh id");
        return state_->entries[id];
    }
    static MeshPick pick_state(State &s, double x, double y) {
        vtkNew<vtkCellPicker> picker;
        picker->SetTolerance(.005);
        picker->PickFromListOn();
        for (auto &e : s.entries)
            if (e.active && e.actor->GetVisibility())
                picker->AddPickList(e.actor);
        MeshPick result;
        if (!picker->Pick(x, y, 0, s.renderer))
            return result;
        for (std::size_t i = 0; i < s.entries.size(); ++i) {
            if (s.entries[i].active && picker->GetActor() == s.entries[i].actor) {
                result.hit = true;
                result.mesh = i;
                result.face = picker->GetCellId();
                result.vertex = picker->GetPointId();
                if (!s.entries[i].data->GetNumberOfPolys())
                    result.face = -1;
                picker->GetPickPosition(result.position.data());
                break;
            }
        }
        return result;
    }
    void install_events() {
        auto &s = *state_;
        auto observer = vtkSmartPointer<vtkCallbackCommand>::New();
        observer->SetClientData(&s);
        observer->SetCallback([](vtkObject *, unsigned long event, void *client, void *) {
            auto &s = *static_cast<State *>(client);
            try {
                if (event == vtkCommand::LeftButtonPressEvent && s.pick_callback) {
                    auto *p = s.interactor->GetEventPosition();
                    s.pick_callback(pick_state(s, p[0], p[1]));
                } else if (event == vtkCommand::KeyPressEvent && s.key_callback) {
                    const char *key = s.interactor->GetKeySym();
                    s.key_callback(key ? key : "");
                }
            } catch (...) {
                // Never unwind a C++ exception through a VTK event callback.
                s.event_error = std::current_exception();
                s.interactor->TerminateApp();
            }
        });
        s.interactor->AddObserver(vtkCommand::LeftButtonPressEvent, observer, 1.0f);
        s.interactor->AddObserver(vtkCommand::KeyPressEvent, observer, 1.0f);
        s.observers.push_back(observer);
    }
    static void validate_style(const MeshStyle &o) {
        color_valid(o.color);
        color_valid(o.edge_color);
        unit(o.opacity, "Invalid opacity");
        unit(o.ambient, "Invalid ambient");
        unit(o.diffuse, "Invalid diffuse");
        unit(o.specular, "Invalid specular");
        positive(o.line_width, "Invalid line width");
        positive(o.point_size, "Invalid point size");
        positive(o.specular_power, "Invalid specular power");
        if (!o.automatic_range &&
            (!std::isfinite(o.range[0]) || !std::isfinite(o.range[1]) || o.range[0] >= o.range[1]))
            throw std::invalid_argument("Invalid color range");
    }
    void apply_style(Entry &e, const MeshStyle &o) {
        validate_style(o);
        auto *attributes = o.point_data
                               ? static_cast<vtkDataSetAttributes *>(e.data->GetPointData())
                               : static_cast<vtkDataSetAttributes *>(e.data->GetCellData());
        vtkDataArray *a = nullptr;
        if (o.color_mode != MeshColorMode::solid) {
            a = attributes->GetArray(o.array_name.c_str());
            const auto count =
                o.point_data ? e.data->GetNumberOfPoints() : e.data->GetNumberOfCells();
            if (!a || a->GetNumberOfTuples() != count)
                throw std::invalid_argument("Display array missing or wrong size");
            if (o.color_mode == MeshColorMode::scalar &&
                (o.component < 0 || o.component >= a->GetNumberOfComponents()))
                throw std::invalid_argument("Scalar component out of range");
            if (o.color_mode == MeshColorMode::labels &&
                (!vtkIdTypeArray::SafeDownCast(a) || a->GetNumberOfComponents() != 1))
                throw std::invalid_argument(
                    "Label coloring requires vtkIdTypeArray (SindreMesh Labels)");
            if (o.color_mode == MeshColorMode::direct &&
                (a->GetNumberOfComponents() != 3 && a->GetNumberOfComponents() != 4))
                throw std::invalid_argument("Direct colors require RGB/RGBA");
            if (o.color_mode == MeshColorMode::direct && a->GetDataType() != VTK_UNSIGNED_CHAR &&
                a->GetDataType() != VTK_DOUBLE && a->GetDataType() != VTK_FLOAT)
                throw std::invalid_argument("RGB/RGBA must be uint8 [0,255] or float [0,1]");
            for (vtkIdType i = 0; i < a->GetNumberOfTuples(); ++i)
                for (int k = 0; k < a->GetNumberOfComponents(); ++k) {
                    const auto x = a->GetComponent(i, k);
                    if (!std::isfinite(x))
                        throw std::invalid_argument("Nonfinite display array");
                    if (o.color_mode == MeshColorMode::direct &&
                        (x < 0 || x > (a->GetDataType() == VTK_UNSIGNED_CHAR ? 255 : 1)))
                        throw std::invalid_argument("RGB/RGBA value out of range");
                }
        }
        if (e.bar)
            state_->renderer->RemoveActor2D(e.bar);
        e.bar = nullptr;
        e.style = o;
        auto *p = e.actor->GetProperty();
        p->SetColor(o.color.data());
        p->SetOpacity(o.opacity);
        p->SetEdgeColor(o.edge_color.data());
        p->SetEdgeVisibility(o.edges);
        p->SetLineWidth(o.line_width);
        p->SetPointSize(o.point_size);
        p->SetLighting(o.lighting);
        p->SetBackfaceCulling(o.backface_culling);
        p->SetAmbient(o.ambient);
        p->SetDiffuse(o.diffuse);
        p->SetSpecular(o.specular);
        p->SetSpecularPower(o.specular_power);
        if (o.smooth_shading)
            p->SetInterpolationToPhong();
        else
            p->SetInterpolationToFlat();
        if (o.representation == Representation::wireframe)
            p->SetRepresentationToWireframe();
        else if (o.representation == Representation::points)
            p->SetRepresentationToPoints();
        else
            p->SetRepresentationToSurface();
        auto *mapper = e.mapper.GetPointer();
        mapper->ScalarVisibilityOff();
        if (!a)
            return;
        mapper->ScalarVisibilityOn();
        if (o.point_data)
            mapper->SetScalarModeToUsePointFieldData();
        else
            mapper->SetScalarModeToUseCellFieldData();
        mapper->SelectColorArray(o.array_name.c_str());
        mapper->InterpolateScalarsBeforeMappingOff();
        if (o.color_mode == MeshColorMode::labels) {
            auto *ids = vtkIdTypeArray::SafeDownCast(a);
            vtkNew<vtkUnsignedCharArray> colors;
            colors->SetName("__sindrecpp_label_colors");
            colors->SetNumberOfComponents(3);
            for (vtkIdType i = 0; i < ids->GetNumberOfTuples(); ++i) {
                // Integer hash avoids double precision loss for labels above 2^53.
                auto h = static_cast<std::uint64_t>(ids->GetValue(i));
                h += 0x9e3779b97f4a7c15ULL;
                h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
                h = (h ^ (h >> 27)) * 0x94d049bb133111ebULL;
                h ^= h >> 31;
                unsigned char rgb[3] = {static_cast<unsigned char>(64 + (h & 191)),
                                        static_cast<unsigned char>(64 + ((h >> 8) & 191)),
                                        static_cast<unsigned char>(64 + ((h >> 16) & 191))};
                colors->InsertNextTypedTuple(rgb);
            }
            attributes->AddArray(colors);
            mapper->SelectColorArray(colors->GetName());
            mapper->SetColorModeToDirectScalars();
        } else if (o.color_mode == MeshColorMode::direct)
            mapper->SetColorModeToDirectScalars();
        else {
            double range[2];
            a->GetRange(range, o.component);
            if (!o.automatic_range) {
                range[0] = o.range[0];
                range[1] = o.range[1];
            }
            if (range[0] == range[1]) {
                range[0] -= .5;
                range[1] += .5;
            }
            vtkNew<vtkColorTransferFunction> lut;
            const double middle = range[0] * .5 + range[1] * .5;
            if (o.colormap == ColorMap::gray) {
                lut->AddRGBPoint(range[0], 0, 0, 0);
                lut->AddRGBPoint(range[1], 1, 1, 1);
            } else if (o.colormap == ColorMap::cool_warm) {
                lut->AddRGBPoint(range[0], .23, .30, .75);
                lut->AddRGBPoint(middle, .87, .87, .87);
                lut->AddRGBPoint(range[1], .71, .02, .15);
            } else {
                lut->AddRGBPoint(range[0], .267, .005, .329);
                lut->AddRGBPoint(middle, .128, .567, .551);
                lut->AddRGBPoint(range[1], .993, .906, .144);
            }
            mapper->SetColorModeToMapScalars();
            mapper->SetArrayComponent(o.component);
            mapper->SetLookupTable(lut);
            mapper->SetScalarRange(range);
            mapper->UseLookupTableScalarRangeOn();
            if (o.scalar_bar) {
                e.bar = vtkSmartPointer<vtkScalarBarActor>::New();
                e.bar->SetLookupTable(lut);
                e.bar->SetTitle(o.array_name.c_str());
                e.bar->SetNumberOfLabels(5);
                e.bar->SetWidth(.12);
                e.bar->SetHeight(.6);
                state_->renderer->AddActor2D(e.bar);
            }
        }
    }

  public:
    explicit ShowMesh(const ShowOptions &options = {}) : state_(std::make_shared<State>()) {
        if (options.width < 1 || options.height < 1)
            throw std::invalid_argument("Invalid display size");
        color_valid(options.background);
        color_valid(options.background_top);
        validate_style(options.style);
        auto &s = *state_;
        s.options = options;
        s.renderer->SetBackground(options.background.data());
        s.renderer->SetBackground2(options.background_top.data());
        s.renderer->SetGradientBackground(options.gradient);
        s.window->AddRenderer(s.renderer);
        s.window->SetSize(options.width, options.height);
        s.window->SetWindowName(options.title.c_str());
        s.window->SetOffScreenRendering(options.offscreen);
        s.window->SetMultiSamples(0);
        s.window->SetAlphaBitPlanes(1);
        s.renderer->SetUseDepthPeeling(1);
        s.renderer->SetMaximumNumberOfPeels(100);
        s.renderer->SetOcclusionRatio(.1);
        if (!options.offscreen) {
            s.interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
            s.interactor->SetRenderWindow(s.window);
            vtkNew<vtkInteractorStyleTrackballCamera> style;
            s.interactor->SetInteractorStyle(style);
            install_events();
        }
        if (options.axes)
            axes();
    }
    ShowMesh(const ShowMesh &) = delete;
    ShowMesh &operator=(const ShowMesh &) = delete;
    ShowMesh(ShowMesh &&) noexcept = default;
    ShowMesh &operator=(ShowMesh &&) noexcept = default;
    std::size_t add(vtkPolyData *data, const MeshStyle &style = {}) {
        if (!data || !data->GetNumberOfPoints())
            throw std::invalid_argument("Cannot display an empty mesh");
        Entry e;
        e.data = vtkSmartPointer<vtkPolyData>::New();
        e.data->DeepCopy(data);
        for (vtkIdType i = 0; i < e.data->GetNumberOfPoints(); ++i) {
            double p[3];
            e.data->GetPoint(i, p);
            finite({p[0], p[1], p[2]});
        }
        if (!e.data->GetNumberOfCells()) {
            vtkNew<vtkVertexGlyphFilter> verts;
            verts->SetInputData(e.data);
            verts->Update();
            e.data->DeepCopy(verts->GetOutput());
        }
        if (e.data->GetNumberOfPolys()) {
            vtkNew<vtkPolyDataNormals> normals;
            normals->SetInputData(e.data);
            normals->SplittingOff();
            normals->ConsistencyOff();
            normals->ComputePointNormalsOn();
            normals->ComputeCellNormalsOn();
            normals->Update();
            e.data->DeepCopy(normals->GetOutput());
        }
        e.mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        e.mapper->SetInputData(e.data);
        e.actor = vtkSmartPointer<vtkActor>::New();
        e.actor->SetMapper(e.mapper);
        apply_style(e, style);
        state_->renderer->AddActor(e.actor);
        state_->entries.push_back(std::move(e));
        return state_->entries.size() - 1;
    }
    template <class Mesh> std::size_t add(const Mesh &mesh, const MeshStyle &style = {}) {
        return add(mesh.get_native(), style);
    }
    ShowMesh &style(std::size_t id, const MeshStyle &value) {
        apply_style(entry(id), value);
        return *this;
    }
    ShowMesh &visible(std::size_t id, bool value) {
        auto &e = entry(id);
        e.actor->SetVisibility(value);
        if (e.bar)
            e.bar->SetVisibility(value);
        return *this;
    }
    ShowMesh &remove(std::size_t id) {
        auto &e = entry(id);
        state_->renderer->RemoveActor(e.actor);
        if (e.bar)
            state_->renderer->RemoveActor2D(e.bar);
        e.active = false;
        return *this;
    }
    ShowMesh &axes(bool value = true, double length = 1) {
        positive(length, "Invalid axis length");
        if (!state_->axes) {
            state_->axes = vtkSmartPointer<vtkAxesActor>::New();
            state_->renderer->AddActor(state_->axes);
        }
        state_->axes->SetTotalLength(length, length, length);
        state_->axes->SetVisibility(value);
        return *this;
    }
    ShowMesh &bounds(std::size_t id, Color color = {1, 1, 1}) {
        color_valid(color);
        vtkNew<vtkOutlineFilter> outline;
        outline->SetInputData(entry(id).data);
        outline->Update();
        vtkNew<vtkPolyDataMapper> mapper;
        mapper->SetInputData(outline->GetOutput());
        vtkNew<vtkActor> actor;
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(color.data());
        actor->PickableOff();
        state_->renderer->AddActor(actor);
        return *this;
    }
    ShowMesh &text(const std::string &message, int x = 15, int y = 15, int size = 18,
                   Color color = {1, 1, 1}) {
        color_valid(color);
        if (size < 1)
            throw std::invalid_argument("Invalid text size");
        vtkNew<vtkTextActor> actor;
        actor->SetInput(message.c_str());
        actor->SetDisplayPosition(x, y);
        actor->GetTextProperty()->SetFontSize(size);
        actor->GetTextProperty()->SetColor(color.data());
        state_->renderer->AddActor2D(actor);
        return *this;
    }
    ShowMesh &normals(std::size_t id, bool point = true, double length = .1, vtkIdType stride = 1) {
        positive(length, "Invalid normal length");
        if (stride < 1)
            throw std::invalid_argument("Invalid normal stride");
        auto &e = entry(id);
        vtkNew<vtkPolyDataNormals> filter;
        filter->SetInputData(e.data);
        filter->SplittingOff();
        filter->ComputePointNormalsOn();
        filter->ComputeCellNormalsOn();
        filter->Update();
        auto *data = filter->GetOutput();
        auto *n = point ? data->GetPointData()->GetNormals() : data->GetCellData()->GetNormals();
        if (!n)
            throw std::invalid_argument("Normal display requires surface faces");
        vtkNew<vtkPoints> points;
        points->SetDataTypeToDouble();
        vtkNew<vtkCellArray> lines;
        for (vtkIdType i = 0; i < n->GetNumberOfTuples(); i += stride) {
            double p[3] = {0, 0, 0};
            if (point)
                data->GetPoint(i, p);
            else {
                vtkIdType count;
                const vtkIdType *ids;
                data->GetCellPoints(i, count, ids);
                if (!count)
                    continue;
                for (vtkIdType j = 0; j < count; ++j) {
                    double v[3];
                    data->GetPoint(ids[j], v);
                    for (int k = 0; k < 3; ++k)
                        p[k] += v[k] / count;
                }
            }
            double end[3];
            for (int k = 0; k < 3; ++k)
                end[k] = p[k] + length * n->GetComponent(i, k);
            vtkIdType ids[2] = {points->InsertNextPoint(p), points->InsertNextPoint(end)};
            lines->InsertNextCell(2, ids);
        }
        vtkNew<vtkPolyData> output;
        output->SetPoints(points);
        output->SetLines(lines);
        vtkNew<vtkPolyDataMapper> mapper;
        mapper->SetInputData(output);
        vtkNew<vtkActor> actor;
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(1, .65, .15);
        actor->PickableOff();
        state_->renderer->AddActor(actor);
        return *this;
    }
    ShowMesh &texture(std::size_t id, const std::filesystem::path &path) {
        auto &e = entry(id);
        auto *uv = e.data->GetPointData()->GetTCoords();
        if (!uv || uv->GetNumberOfComponents() != 2 ||
            uv->GetNumberOfTuples() != e.data->GetNumberOfPoints())
            throw std::invalid_argument("Texture requires Nx2 texture coordinates");
        if (!std::filesystem::is_regular_file(path))
            throw std::invalid_argument("Texture file missing");
        vtkSmartPointer<vtkImageData> image;
        const auto ext = path.extension().string();
        if (ext == ".png") {
            vtkNew<vtkPNGReader> r;
            r->SetFileName(path.string().c_str());
            r->Update();
            if (r->GetErrorCode())
                throw std::runtime_error("PNG texture read failed");
            image = r->GetOutput();
        } else if (ext == ".jpg" || ext == ".jpeg") {
            vtkNew<vtkJPEGReader> r;
            r->SetFileName(path.string().c_str());
            r->Update();
            if (r->GetErrorCode())
                throw std::runtime_error("JPEG texture read failed");
            image = r->GetOutput();
        } else
            throw std::invalid_argument("Texture supports PNG/JPEG");
        if (!image || !image->GetNumberOfPoints())
            throw std::runtime_error("Empty texture image");
        vtkNew<vtkTexture> t;
        t->SetInputData(image);
        t->InterpolateOn();
        e.actor->SetTexture(t);
        e.mapper->ScalarVisibilityOff();
        return *this;
    }
    ShowMesh &clip_plane(std::size_t id, Position3 origin, Position3 normal) {
        finite(origin);
        finite(normal);
        double norm = 0;
        for (auto v : normal)
            norm += v * v;
        positive(norm, "Invalid clipping normal");
        vtkNew<vtkPlane> plane;
        plane->SetOrigin(origin.data());
        plane->SetNormal(normal.data());
        entry(id).mapper->AddClippingPlane(plane);
        return *this;
    }
    ShowMesh &clear_clipping(std::size_t id) {
        entry(id).mapper->RemoveAllClippingPlanes();
        return *this;
    }
    ShowMesh &camera(const MeshCamera &value) {
        finite(value.position);
        finite(value.focal_point);
        finite(value.view_up);
        positive(value.parallel_scale, "Invalid parallel scale");
        positive(value.view_angle, "Invalid view angle");
        if (value.view_angle >= 180)
            throw std::invalid_argument("View angle must be below 180");
        double d[3], cross[3];
        for (int k = 0; k < 3; ++k)
            d[k] = value.focal_point[k] - value.position[k];
        for (int k = 0; k < 3; ++k)
            cross[k] = d[(k + 1) % 3] * value.view_up[(k + 2) % 3] -
                       d[(k + 2) % 3] * value.view_up[(k + 1) % 3];
        positive(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2],
                 "Camera direction and up must be independent");
        auto *c = state_->renderer->GetActiveCamera();
        c->SetPosition(value.position.data());
        c->SetFocalPoint(value.focal_point.data());
        c->SetViewUp(value.view_up.data());
        c->SetParallelProjection(value.parallel);
        c->SetParallelScale(value.parallel_scale);
        c->SetViewAngle(value.view_angle);
        state_->renderer->ResetCameraClippingRange();
        return *this;
    }
    ShowMesh &reset_camera() {
        state_->renderer->ResetCamera();
        state_->renderer->ResetCameraClippingRange();
        return *this;
    }
    ShowMesh &rotate_camera(double azimuth, double elevation = 0, double roll = 0) {
        finite({azimuth, elevation, roll});
        auto *c = state_->renderer->GetActiveCamera();
        c->Azimuth(azimuth);
        c->Elevation(elevation);
        c->Roll(roll);
        c->OrthogonalizeViewUp();
        state_->renderer->ResetCameraClippingRange();
        return *this;
    }
    ShowMesh &zoom(double factor) {
        positive(factor, "Invalid zoom factor");
        state_->renderer->GetActiveCamera()->Zoom(factor);
        return *this;
    }
    ShowMesh &light(Position3 position, Position3 focal_point = {0, 0, 0}, Color color = {1, 1, 1},
                    double intensity = 1) {
        finite(position);
        finite(focal_point);
        color_valid(color);
        positive(intensity, "Invalid light intensity");
        vtkNew<vtkLight> light;
        light->SetLightTypeToSceneLight();
        light->SetPosition(position.data());
        light->SetFocalPoint(focal_point.data());
        light->SetColor(color.data());
        light->SetIntensity(intensity);
        state_->renderer->AddLight(light);
        return *this;
    }
    ShowMesh &on_pick(std::function<void(const MeshPick &)> callback) {
        state_->pick_callback = std::move(callback);
        return *this;
    }
    ShowMesh &on_key(std::function<void(const std::string &)> callback) {
        state_->key_callback = std::move(callback);
        return *this;
    }
    MeshPick pick(double x, double y) {
        if (!std::isfinite(x) || !std::isfinite(y))
            throw std::invalid_argument("Invalid display position");
        return pick_state(*state_, x, y);
    }
    ShowMesh &render() {
        state_->window->Render();
        return *this;
    }
    ShowMesh &show(bool interactive = true) {
        render();
        if (interactive && !state_->options.offscreen) {
            state_->event_error = nullptr;
            state_->interactor->Initialize();
            state_->interactor->Start();
            if (state_->event_error)
                std::rethrow_exception(state_->event_error);
        }
        return *this;
    }
    ShowMesh &screenshot(const std::filesystem::path &path, int scale = 1) {
        if (path.extension() != ".png" || scale < 1 || scale > 8)
            throw std::invalid_argument("Screenshot requires PNG and scale 1..8");
        render();
        vtkNew<vtkWindowToImageFilter> image;
        image->SetInput(state_->window);
        image->SetScale(scale);
        image->SetInputBufferTypeToRGB();
        image->ReadFrontBufferOff();
        image->Update();
        vtkNew<vtkPNGWriter> writer;
        writer->SetFileName(path.string().c_str());
        writer->SetInputConnection(image->GetOutputPort());
        writer->Write();
        if (writer->GetErrorCode() || !std::filesystem::is_regular_file(path))
            throw std::runtime_error("Screenshot write failed");
        return *this;
    }
    // Borrowed handles; native modifications remain the caller's responsibility.
    vtkRenderer *get_renderer() const { return state_->renderer; }
    vtkRenderWindow *get_window() const { return state_->window; }
    vtkRenderWindowInteractor *get_interactor() const { return state_->interactor; }
    vtkActor *get_actor(std::size_t id) { return entry(id).actor; }
    vtkPolyData *get_data(std::size_t id) { return entry(id).data; }
};

inline ShowMesh show_mesh(vtkPolyData *mesh, const ShowOptions &options = {}) {
    ShowMesh viewer(options);
    viewer.add(mesh, options.style);
    viewer.reset_camera().show(options.interactive);
    return viewer;
}
template <class Mesh> ShowMesh show_mesh(const Mesh &mesh, const ShowOptions &options = {}) {
    return show_mesh(mesh.get_native(), options);
}
} // namespace sindrecpp::utils3d
