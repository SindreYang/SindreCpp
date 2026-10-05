#include <iostream>
#include <sindrecpp/utils3d/sindremesh.hpp>
#include <vtkCamera.h>
#include <vtkPNGReader.h>

using namespace sindrecpp::utils3d;
static void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> static void rejects(F f) {
    bool value = false;
    try {
        f();
    } catch (const std::exception &) {
        value = true;
    }
    check(value, "Invalid display input was accepted");
}
int main() {
    try {
        Vertices v(4, 3);
        v << 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1;
        Faces f(4, 3);
        f << 0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3;
        SindreMesh mesh(v, f);
        Matrix uv(4, 2);
        uv << 0, 0, 1, 0, 0, 1, 1, 1;
        mesh.set_uv(uv);
        Matrix values(4, 1);
        values << 0, .3, .7, 1;
        mesh.set_data("height", values);
        Labels ids(4);
        ids << 0, 1, (std::int64_t{1} << 54), (std::int64_t{1} << 54) + 1;
        mesh.set_vertex_labels(ids);
        ShowOptions options;
        options.width = 320;
        options.height = 240;
        options.offscreen = true;
        options.interactive = false;
        options.axes = false;
        options.background = {0, 0, 0};
        options.gradient = false;
        auto viewer = mesh.show(options);
        check(viewer.get_interactor() == nullptr,
              "Offscreen view must not initialize an interactor");
        check(viewer.get_data(0) != mesh.get_native(), "Viewer owns an independent snapshot");
        MeshStyle scalar;
        scalar.color_mode = MeshColorMode::scalar;
        scalar.array_name = "height";
        viewer.style(0, scalar).bounds(0).normals(0).text("SindreMesh", 10, 10, 16);
        viewer.axes(true, .5).reset_camera().light({2, 3, 4}).render();
        const auto path = std::filesystem::current_path() / "show_mesh_preview.png";
        viewer.screenshot(path);
        vtkNew<vtkPNGReader> image;
        image->SetFileName(path.string().c_str());
        image->Update();
        int dims[3];
        image->GetOutput()->GetDimensions(dims);
        check(dims[0] == 320 && dims[1] == 240, "Screenshot dimensions");
        auto *pixels = image->GetOutput()->GetPointData()->GetScalars();
        double range[2];
        pixels->GetRange(range);
        check(range[1] > range[0], "Screenshot must contain visible content");
        viewer.texture(0, path);
        check(viewer.get_actor(0)->GetTexture() != nullptr, "Texture attached");
        viewer.get_actor(0)->SetTexture(nullptr);
        for (auto mode :
             {Representation::wireframe, Representation::points, Representation::surface}) {
            MeshStyle appearance;
            appearance.representation = mode;
            appearance.opacity = .6;
            appearance.edges = true;
            appearance.smooth_shading = false;
            appearance.backface_culling = true;
            viewer.style(0, appearance).render();
        }
        MeshStyle labels;
        labels.color_mode = MeshColorMode::labels;
        labels.array_name = "Labels";
        viewer.style(0, labels);
        auto *colors = vtkUnsignedCharArray::SafeDownCast(
            viewer.get_data(0)->GetPointData()->GetArray("__sindrecpp_label_colors"));
        check(colors && colors->GetNumberOfTuples() == 4, "Integer label colors");
        unsigned char a[3], b[3];
        colors->GetTypedTuple(2, a);
        colors->GetTypedTuple(3, b);
        check(a[0] != b[0] || a[1] != b[1] || a[2] != b[2],
              "Large adjacent integer labels must not collapse through double conversion");
        MeshStyle direct;
        direct.color_mode = MeshColorMode::direct;
        direct.array_name = "rgb";
        Matrix rgb(4, 3);
        rgb << 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0;
        auto second = mesh.clone();
        second.set_data("rgb", rgb);
        const auto id = viewer.add(second, direct);
        viewer.render();
        Matrix rgba(4, 4);
        rgba.leftCols(3) = rgb;
        rgba.col(3).setConstant(.5);
        second.set_data("rgba", rgba);
        MeshStyle alpha = direct;
        alpha.array_name = "rgba";
        const auto rgba_id = viewer.add(second, alpha);
        viewer.render().remove(rgba_id);
        viewer.visible(id, false).visible(id, true).remove(id);
        rejects([&] { viewer.get_actor(id); });
        viewer.clip_plane(0, {.2, 0, 0}, {1, 0, 0}).render().clear_clipping(0);
        MeshCamera camera;
        camera.position = {0, 0, 3};
        camera.focal_point = {0, 0, 0};
        camera.parallel = true;
        viewer.camera(camera).render();
        // Test actual hit ids and snapshot lifetime on one isolated triangle.
        Vertices tv(3, 3);
        tv << -1, -1, 0, 1, -1, 0, 0, 1, 0;
        Faces tf(1, 3);
        tf << 0, 1, 2;
        SindreMesh triangle(tv, tf);
        auto picking = triangle.show(options);
        picking.camera(camera).render();
        auto hit = picking.pick(160, 120);
        check(hit.hit && hit.mesh == 0 && hit.face == 0, "Center cell picking");
        bool called = false;
        ShowOptions events = options;
        events.offscreen = false;
        ShowMesh event_view(events);
        event_view.add(triangle);
        event_view.camera(camera).render();
        event_view.on_pick([&](const MeshPick &p) { called = p.hit; });
        event_view.get_interactor()->SetEventPosition(160, 120);
        event_view.get_interactor()->InvokeEvent(vtkCommand::LeftButtonPressEvent);
        check(called, "Mouse pick callback");
        event_view.on_key([&](const std::string &key) { called = key == "x"; });
        called = false;
        event_view.get_interactor()->SetKeySym("x");
        event_view.get_interactor()->InvokeEvent(vtkCommand::KeyPressEvent);
        check(called, "Key callback");
        auto moved = std::move(event_view);
        called = false;
        moved.get_interactor()->InvokeEvent(vtkCommand::KeyPressEvent);
        check(called, "Observers remain valid after viewer move");
        auto cloud = SindreMesh(tv, Faces(0, 3));
        const auto cloud_id = viewer.add(cloud);
        check(viewer.get_data(cloud_id)->GetNumberOfVerts() == 3, "Point cloud glyphs");
        rejects([&] {
            MeshStyle invalid;
            invalid.opacity = 2;
            viewer.style(0, invalid);
        });
        rejects([&] {
            MeshStyle missing = scalar;
            missing.array_name = "missing";
            viewer.style(0, missing);
        });
        rejects([&] { viewer.clip_plane(0, {0, 0, 0}, {0, 0, 0}); });
        rejects([&] {
            MeshCamera invalid;
            invalid.view_up = {0, 0, 1};
            viewer.camera(invalid);
        });
        rejects([&] { viewer.screenshot("bad.jpg"); });
        check(mesh.vertices().isApprox(v) && mesh.get_vertex_labels() == ids,
              "Display must retain source geometry and attributes");
        std::cout << "show_mesh rendering tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
