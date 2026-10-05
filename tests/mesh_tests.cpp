#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <sindrecpp/utils3d.hpp>

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
        check(mesh.npoints() == 4 && mesh.nfaces() == 4, "Mesh size");
        check(mesh.is_watertight() && mesh.get_boundary().empty(), "Closed tetrahedron");
        check(mesh.get_edges().size() == 6, "Unique edges");
        check(mesh.get_face_adj_list()[0].size() == 3, "Face adjacency");
        check((mesh.faces_barycentre().row(0) - Eigen::RowVector3d(1. / 3, 1. / 3, 0)).norm() <
                  1e-12,
              "Face center");
        check(std::abs(mesh.faces_area()[0] - .5) < 1e-12, "Area");
        mesh.compute_normals();
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
        check(sample(mesh, 10).rows() == 10, "Sampling");
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
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
