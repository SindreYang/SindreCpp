#include "vtk_coverage.hpp"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <utils3d/index.hpp>

using namespace sindrecpp::utils3d;
static void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> static void rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "Invalid input was accepted");
}
int main() {
    try {
        Vertices v(4, 3);
        v << 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1;
        Faces f(4, 3);
        f << 0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3;
        SindreMesh mesh(v, f);
        auto fluent = mesh.clone();
        fluent.clean().compute_normals();
        SmoothOptions fluent_smoothing;
        fluent_smoothing.backend = Backend::vtk;
        fluent.smooth(fluent_smoothing);
        check(fluent.nfaces() == 4 && fluent.vertex_normals().rows() == 4,
              "SindreMesh fluent workflow");
        SindreMesh empty_mesh;
        empty_mesh.shift_xyz(Eigen::Vector3d::Ones());
        check(empty_mesh.empty() && empty_mesh.vertex_normals().rows() == 0,
              "Empty mesh transformations must be safe");
        check(mesh.dimensions().isApprox(Eigen::Vector3d::Ones()), "Mesh bounds");
        auto movable = mesh.clone();
        auto moved = std::move(movable);
        check(movable.empty() && moved.vertices().isApprox(v),
              "Mesh move transfers geometry and empties source");
        Matrix property(4, 1);
        property << 0, 1, 2, 3;
        moved.set_data("quality", property, false);
        Matrix updated_property(4, 1);
        updated_property << 10, 11, 12, 13;
        moved.set_data("quality", updated_property, false);
        check(moved.get_celldata("quality").isApprox(updated_property),
              "Replacing an existing mesh attribute");
        moved.set_data("quality", property, false);
        moved.rename_data("quality", "score", false);
        check(moved.has_data("score", false), "Rename attribute");
        check(moved.extract_region("score", 1, 2).nfaces() == 2, "Scalar region extraction");
        moved.remove_data("score", false);
        check(!moved.has_data("score", false), "Remove attribute");
        Matrix uv_values(4, 2);
        uv_values << 0, 0, 1, 0, 0, 1, 1, 1;
        moved.set_uv(uv_values);
        check(moved.get_uv().isApprox(uv_values), "Texture coordinates");
        vtkNew<vtkTriangleFilter> filter;
        check(moved.filtered(filter).nfaces() == 4, "Generic VTK filter");
        auto pipeline = moved.pipeline_source();
        filter->SetInputConnection(pipeline->GetOutputPort());
        filter->Update();
        moved.shift_xyz(Eigen::Vector3d::Ones());
        check(filter->GetOutput()->GetNumberOfPoints() == 4, "Caller-owned pipeline source");
        check(append_meshes({mesh, mesh}).nfaces() == 8, "Append meshes");
        rejects([&] { (void)append_meshes({mesh}, true, -1); });
        check(slice_plane(mesh, Eigen::Vector3d(.2, 0, 0), Eigen::Vector3d::UnitX())
                      ->GetNumberOfLines() > 0,
              "Plane section curves");
        check(clip_box(mesh, Eigen::Vector3d(-1, -1, -1), Eigen::Vector3d(2, 2, 2)).nfaces() == 4,
              "Box clipping");
        check(clip_sphere(mesh, Eigen::Vector3d::Zero(), 2).nfaces() == 4, "Sphere clipping");
        check(mesh.feature_edges(20)->GetNumberOfLines() > 0, "Sharp feature edges");
        check(mesh.npoints() == 4 && mesh.nfaces() == 4, "Mesh size");
        check(mesh.is_watertight() && mesh.get_boundary().empty(), "Closed tetrahedron");
        check(mesh.get_edges().size() == 6, "Unique edges");
        check(mesh.get_curvature().allFinite() && mesh.get_curvature(false).allFinite(),
              "Mean/Gaussian curvature");
        check(mesh.get_face_adj_list()[0].size() == 3, "Face adjacency");
        check((mesh.faces_barycentre().row(0) - Eigen::RowVector3d(1. / 3, 1. / 3, 0)).norm() <
                  1e-12,
              "Face center");
        check(std::abs(mesh.faces_area()[0] - .5) < 1e-12, "Area");
        mesh.compute_normals();
        auto flipped_normals = mesh.clone();
        MeshNormals normal_options;
        normal_options.flip = true;
        flipped_normals.compute_normals(normal_options);
        check(flipped_normals.get_celldata("Normals").isApprox(-mesh.get_celldata("Normals")),
              "Normal flip option");
        check(mesh.vertex_normals().rows() == 4, "Normals");
        Labels labels(4);
        labels << 1, 2, 3, (std::int64_t{1} << 54) + 3;
        mesh.set_vertex_labels(labels);
        mesh.set_faces_labels(labels);
        check(mesh.get_vertex_labels() == labels, "Integer labels preserve 64 bits");
        auto selection = mesh.clone();
        selection.update_faces({true, false, true, false});
        check(selection.nfaces() == 2 && selection.get_faces_labels()[1] == labels[2],
              "Face selection preserves original labels");
        selection = mesh.clone();
        selection.update_vertex({true, true, true, false});
        check(selection.npoints() == 3 && selection.nfaces() == 1 &&
                  selection.get_vertex_labels()[2] == labels[2] &&
                  selection.get_faces_labels()[0] == labels[0],
              "Vertex selection remaps labels");
        check(mesh.check().edge_closed && mesh.check().unused_vertices == 0, "Geometry report");
        check(std::abs(mesh.signed_volume() - 1. / 6) < 1e-12, "Signed volume");
        auto copy = mesh.clone();
        copy.shift_xyz(Eigen::Vector3d(2, 3, 4));
        check(mesh.vertices().isApprox(v), "Clone must not mutate source");
        check(copy.get_vertex_labels() == labels, "Transforms preserve labels");
        auto transform = get_normalize(mesh).transform;
        copy = mesh.clone();
        copy.apply_transform(transform).apply_inv_transform(transform);
        check(copy.vertices().isApprox(v), "Normalization round trip");
        Vertices query(2, 3);
        query << .1, .1, -1, 0, 0, 0;
        auto projection = project_points(mesh, query);
        check(std::abs(projection.distances[0] - 1) < 1e-10, "Surface projection");
        check(labels_mapping(v, v, labels) == labels, "Label transfer");
        auto face_labels = vertex_labels_to_face_labels(f, labels);
        check(face_labels.size() == 4, "Vertex/face labels");
        check(face_labels_to_vertex_labels(f, face_labels, 4).size() == 4, "Face/vertex labels");
        auto heat = get_gaussian_heatmap(v, v, .5, true);
        check(std::abs(heat(0, 0) - 1) < 1e-12, "Heatmap");
        check(subdivide(mesh, 1).nfaces() == 16, "Subdivision");
        SmoothOptions smoothing;
        smoothing.backend = Backend::vtk;
        check(smooth(mesh, smoothing).npoints() == 4, "VTK smoothing");
        check(reverse_faces(mesh).nfaces() == 4, "Reverse winding");
        check(cut_plane(mesh, Eigen::Vector3d(.2, 0, 0), Eigen::Vector3d::UnitX()).nfaces() > 0,
              "Plane clipping");
        Faces open_faces = f.topRows(3);
        SindreMesh open(v, open_faces);
        check(open.boundary_loops().size() == 1 && open.boundary_loops()[0].size() == 3,
              "Ordered boundary loop");
        check(!open.get_boundary().empty(), "Open boundary");
        check(fill_holes(open, Backend::vtk).is_watertight(), "VTK hole fill");
        check(mesh.split_component_by_faces().size() == 1, "Components");
        auto path =
            std::filesystem::temp_directory_path() /
            ("sindrecpp-mesh-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".vtp");
        mesh.save(path);
        SindreMesh loaded(path);
        check(loaded.vertices().isApprox(v) && loaded.faces() == f, "VTP round trip");
        check(loaded.get_vertex_labels() == labels, "VTP integer attributes");
        std::filesystem::remove(path);
        const auto unicode_path = std::filesystem::temp_directory_path() / L"sindrecpp-中文网格.vtp";
        mesh.save(unicode_path);
        SindreMesh unicode_loaded(unicode_path);
        check(unicode_loaded.vertices().isApprox(v) && unicode_loaded.faces() == f,
              "Unicode VTP path round trip");
        std::filesystem::remove(unicode_path);
        auto json_path = path;
        json_path.replace_extension(".json");
        mesh.set_data("质量\"\\", property, true);
        mesh.save(json_path);
        check(std::filesystem::is_regular_file(json_path), "JSON mesh export");
        std::filesystem::remove(json_path);
        const auto unicode_json_path = std::filesystem::temp_directory_path() / L"sindrecpp-中文网格.json";
        mesh.save(unicode_json_path);
        check(std::filesystem::is_regular_file(unicode_json_path), "Unicode JSON path export");
        std::filesystem::remove(unicode_json_path);
        for (const char *extension : {".stl", ".ply", ".obj"}) {
            auto interchange = path;
            interchange.replace_extension(extension);
            mesh.save(interchange);
            SindreMesh roundtrip(interchange);
            check(roundtrip.nfaces() == 4 &&
                      roundtrip.dimensions().isApprox(Eigen::Vector3d::Ones()),
                  "Mesh interchange geometry");
            std::filesystem::remove(interchange);
        }
        Faces bad = f;
        bad(0, 0) = -1;
        rejects([&] { SindreMesh x(v, bad); });
        rejects([&] { mesh.set_vertex_labels(Labels(2)); });
        rejects([&] { get_gaussian_heatmap(v, v, 0); });
        rejects([&] { mesh.apply_inv_transform(Eigen::Matrix4d::Zero()); });
        rejects([&] {
            SindreMesh x;
            x.center();
        });
        rejects([&] { get_backend(Operation::uv, Backend::vtk); });
        for (auto backend : get_supported_backends(Operation::decimate)) {
            DecimateOptions o;
            o.target_faces = 3;
            o.backend = backend;
            auto out = decimate(mesh, o);
            check(out.nfaces() <= mesh.nfaces(), "Decimation grew faces");
        }
        for (auto backend : get_supported_backends(Operation::smooth)) {
            SmoothOptions o;
            o.backend = backend;
            auto out = smooth(mesh, o);
            check(out.npoints() == 4, "Smoothing vertex count");
        }
        for (auto backend : get_supported_backends(Operation::fill_holes))
            check(fill_holes(open, backend).is_watertight(), "Backend hole fill");
        for (auto backend : get_supported_backends(Operation::self_intersections))
            check(!has_self_intersections(mesh, backend), "Tetrahedron self intersection");
        for (auto backend : get_supported_backends(Operation::clean))
            check(clean(mesh, backend).nfaces() == 4, "Backend clean");
        if (!get_supported_backends(Operation::boolean_op).empty()) {
            auto other = mesh.clone();
            other.shift_xyz(Eigen::Vector3d(.2, .2, .2));
            for (auto backend : get_supported_backends(Operation::boolean_op)) {
                auto result = boolean_mesh(mesh, other, BooleanOperation::intersect, backend);
                check(result.nfaces() > 0 && result.is_watertight(), "Boolean intersection");
            }
        }
        for (auto backend : get_supported_backends(Operation::remesh)) {
            RemeshOptions o;
            o.backend = backend;
            o.edge_length = .5;
            o.iterations = 1;
            check(remesh(mesh, o).nfaces() > 0, "Remesh");
        }
#if defined(SINDRECPP_UTILS3D_OPEN3D)
        auto icp = register_icp(v, v, .5);
        check(icp.transform.isApprox(Eigen::Matrix4d::Identity(), 1e-6) && icp.fitness > .99,
              "ICP identity");
        Vertices open3d_vertices(8, 3);
        open3d_vertices << 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0,
                           0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1;
        Faces open3d_faces(12, 3);
        open3d_faces << 0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6,
                        0, 4, 5, 0, 5, 1, 1, 5, 6, 1, 6, 2,
                        2, 6, 7, 2, 7, 3, 3, 7, 4, 3, 4, 0;
        SindreMesh open3d_mesh(open3d_vertices, open3d_faces);
        DecimateOptions open3d_decimate;
        open3d_decimate.target_faces = 8;
        open3d_decimate.backend = Backend::open3d;
        check(decimate(open3d_mesh, open3d_decimate).nfaces() <= 12, "Open3D decimation");
        SmoothOptions open3d_smooth;
        open3d_smooth.backend = Backend::open3d;
        check(smooth(open3d_mesh, open3d_smooth).npoints() == 8, "Open3D smoothing");
        check(clean(open3d_mesh, Backend::open3d).nfaces() == 12, "Open3D cleaning");
        check(sample(open3d_mesh, 10).rows() == 10, "Open3D sampling");
#endif
#if defined(SINDRECPP_UTILS3D_IGL)
        Vertices square(4, 3);
        square << 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0;
        Faces sf(2, 3);
        sf << 0, 1, 2, 0, 2, 3;
        auto uv = get_uv(SindreMesh(square, sf));
        check(uv.rows() == 4 && uv.cols() == 2 && uv.allFinite(), "UV");
        Vertices disk(5, 3);
        disk.topRows(4) = square;
        disk.row(4) << .5, .5, 0;
        Faces df(4, 3);
        df << 0, 1, 4, 1, 2, 4, 2, 3, 4, 3, 0, 4;
        auto interior_uv = get_uv(SindreMesh(disk, df));
        check(interior_uv.rows() == 5 && interior_uv.allFinite() &&
                  interior_uv.row(4).norm() < 1e-8,
              "UV interior harmonic solve");
#endif
        check(mesh.vertices().isApprox(v) && mesh.get_vertex_labels() == labels,
              "Algorithms must retain input");
        std::cout << "SindreMesh tests passed\n";
        vtk_coverage("vtk_coverage_mesh.json",
                     {1,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 26, 27, 28, 29,
                      30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45});
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
