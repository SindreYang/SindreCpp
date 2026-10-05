#pragma once
#include "sindremesh.hpp"
#include <functional>
#include <initializer_list>
#include <queue>
#include <vtkAppendPolyData.h>
#include <vtkBox.h>
#include <vtkClipPolyData.h>
#include <vtkCutter.h>
#include <vtkDecimatePro.h>
#include <vtkFillHolesFilter.h>
#include <vtkImplicitPolyDataDistance.h>
#include <vtkLoopSubdivisionFilter.h>
#include <vtkPlane.h>
#include <vtkReverseSense.h>
#include <vtkSphere.h>
#include <vtkStaticCellLocator.h>
#include <vtkWindowedSincPolyDataFilter.h>
#if defined(SINDRECPP_UTILS3D_MESHLIB)
// MeshLib's translation macros conflict with private member names in
// Boost/CGAL. Keep it local to the SDK headers, preserving any caller macro.
#pragma push_macro("_")
#pragma push_macro("_t")
#undef _
#undef _t
#include <MRMesh/MRBitSet.h>
#include <MRMesh/MRMesh.h>
#include <MRMesh/MRMeshBoolean.h>
#include <MRMesh/MRMeshBuilder.h>
#include <MRMesh/MRMeshCollide.h>
#include <MRMesh/MRMeshDecimate.h>
#include <MRMesh/MRMeshFillHole.h>
#include <MRMesh/MRMeshRelax.h>
#pragma pop_macro("_t")
#pragma pop_macro("_")
#endif
#if defined(SINDRECPP_UTILS3D_CGAL)
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/border.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/remesh.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Count_stop_predicate.h>
#include <CGAL/Surface_mesh_simplification/edge_collapse.h>
#endif
#if defined(SINDRECPP_UTILS3D_OPEN3D)
#include <open3d/geometry/PointCloud.h>
#include <open3d/geometry/TriangleMesh.h>
#include <open3d/pipelines/registration/Registration.h>
#include <open3d/pipelines/registration/TransformationEstimation.h>
#endif
#if defined(SINDRECPP_UTILS3D_IGL)
#include <igl/boundary_loop.h>
#include <igl/decimate.h>
#include <igl/harmonic.h>
#include <igl/map_vertices_to_circle.h>
#include <igl/principal_curvature.h>
#endif
#if defined(SINDRECPP_UTILS3D_VCG)
#include <vcg/complex/algorithms/clean.h>
#include <vcg/complex/complex.h>
#endif

namespace sindrecpp::utils3d {
// Curves retain vtkPolyData lines rather than being converted to triangle meshes.
inline vtkSmartPointer<vtkPolyData>
slice_plane(const SindreMesh &mesh, const Eigen::Vector3d &origin, const Eigen::Vector3d &normal) {
    if (!origin.allFinite() || !normal.allFinite() || normal.norm() == 0)
        throw std::invalid_argument("Invalid section plane");
    vtkNew<vtkPlane> plane;
    plane->SetOrigin(origin.data());
    plane->SetNormal(normal.normalized().eval().data());
    vtkNew<vtkCutter> cutter;
    cutter->SetInputData(mesh.get_native());
    cutter->SetCutFunction(plane);
    vtkNew<vtkStripper> lines;
    lines->SetInputConnection(cutter->GetOutputPort());
    lines->Update();
    auto result = vtkSmartPointer<vtkPolyData>::New();
    result->DeepCopy(lines->GetOutput());
    return result;
}
inline SindreMesh clip_box(const SindreMesh &mesh, const Eigen::Vector3d &lower,
                           const Eigen::Vector3d &upper, bool inside = true) {
    if (!lower.allFinite() || !upper.allFinite() || (lower.array() >= upper.array()).any())
        throw std::invalid_argument("Invalid clip bounds");
    vtkNew<vtkBox> box;
    box->SetBounds(lower.x(), upper.x(), lower.y(), upper.y(), lower.z(), upper.z());
    vtkNew<vtkClipPolyData> clip;
    clip->SetInputData(mesh.get_native());
    clip->SetClipFunction(box);
    clip->SetInsideOut(inside);
    clip->Update();
    return SindreMesh(clip->GetOutput());
}
inline SindreMesh clip_sphere(const SindreMesh &mesh, const Eigen::Vector3d &center, double radius,
                              bool inside = true) {
    if (!center.allFinite() || !std::isfinite(radius) || radius <= 0)
        throw std::invalid_argument("Invalid clip sphere");
    vtkNew<vtkSphere> sphere;
    sphere->SetCenter(center.data());
    sphere->SetRadius(radius);
    vtkNew<vtkClipPolyData> clip;
    clip->SetInputData(mesh.get_native());
    clip->SetClipFunction(sphere);
    clip->SetInsideOut(inside);
    clip->Update();
    return SindreMesh(clip->GetOutput());
}
inline SindreMesh append_meshes(const std::vector<SindreMesh> &meshes, bool merge_points = false,
                                double tolerance = 0) {
    if (meshes.empty())
        return {};
    vtkNew<vtkAppendPolyData> append;
    for (const auto &mesh : meshes)
        append->AddInputData(mesh.get_native());
    append->Update();
    SindreMesh result(append->GetOutput());
    return merge_points ? result.clean(tolerance) : result;
}
enum class Backend { automatic, meshlib, cgal, open3d, igl, vcg, vtk };
enum class Operation {
    decimate,
    smooth,
    remesh,
    boolean_op,
    fill_holes,
    self_intersections,
    clean,
    curvature,
    uv,
    registration,
    reconstruction,
    sample
};
inline const char *backend_name(Backend b) {
    switch (b) {
    case Backend::meshlib:
        return "meshlib";
    case Backend::cgal:
        return "cgal";
    case Backend::open3d:
        return "open3d";
    case Backend::igl:
        return "igl";
    case Backend::vcg:
        return "vcg";
    case Backend::vtk:
        return "vtk";
    default:
        return "automatic";
    }
}
inline bool backend_available(Backend b) {
    switch (b) {
    case Backend::vtk:
        return true;
#if defined(SINDRECPP_UTILS3D_MESHLIB)
    case Backend::meshlib:
        return true;
#endif
#if defined(SINDRECPP_UTILS3D_CGAL)
    case Backend::cgal:
        return true;
#endif
#if defined(SINDRECPP_UTILS3D_OPEN3D)
    case Backend::open3d:
        return true;
#endif
#if defined(SINDRECPP_UTILS3D_IGL)
    case Backend::igl:
        return true;
#endif
#if defined(SINDRECPP_UTILS3D_VCG)
    case Backend::vcg:
        return true;
#endif
    default:
        return false;
    }
}
inline std::vector<Backend> get_available_backends() {
    std::vector<Backend> x;
    for (auto b : {Backend::meshlib, Backend::cgal, Backend::open3d, Backend::igl, Backend::vcg,
                   Backend::vtk})
        if (backend_available(b))
            x.push_back(b);
    return x;
}
inline std::vector<Backend> get_supported_backends(Operation op) {
    std::vector<Backend> candidates;
    switch (op) {
    case Operation::decimate:
        candidates = {Backend::meshlib, Backend::cgal, Backend::open3d, Backend::igl, Backend::vtk};
        break;
    case Operation::smooth:
        candidates = {Backend::meshlib, Backend::open3d, Backend::vtk};
        break;
    case Operation::remesh:
    case Operation::boolean_op:
    case Operation::self_intersections:
        candidates = {Backend::meshlib, Backend::cgal};
        break;
    case Operation::fill_holes:
        candidates = {Backend::meshlib, Backend::cgal, Backend::vtk};
        break;
    case Operation::clean:
        candidates = {Backend::meshlib, Backend::open3d, Backend::vcg, Backend::vtk};
        break;
    case Operation::curvature:
        candidates = {Backend::igl, Backend::vtk};
        break;
    case Operation::uv:
        candidates = {Backend::igl};
        break;
    default:
        candidates = {Backend::open3d};
        break;
    }
    std::vector<Backend> x;
    for (auto b : candidates)
        if (backend_available(b))
            x.push_back(b);
    return x;
}
inline Backend get_backend(Operation op, Backend requested = Backend::automatic) {
    const auto choices = get_supported_backends(op);
    if (requested == Backend::automatic) {
        if (choices.empty())
            throw std::runtime_error("No enabled backend supports this operation");
        return choices.front();
    }
    if (std::find(choices.begin(), choices.end(), requested) == choices.end())
        throw std::runtime_error(std::string(backend_name(requested)) +
                                 " is disabled or does not implement this operation");
    return requested;
}
namespace detail {
inline void require_surface(const SindreMesh &m) {
    if (!m.nfaces())
        throw std::invalid_argument("Operation requires a nonempty triangle surface");
}
inline void positive(double v, const char *name) {
    if (!std::isfinite(v) || v <= 0)
        throw std::invalid_argument(std::string(name) + " must be finite and positive");
}
#if defined(SINDRECPP_UTILS3D_MESHLIB)
inline MR::Mesh to_mr(const SindreMesh &m) {
    const auto v = m.vertices();
    const auto f = m.faces();
    if (v.rows() > std::numeric_limits<int>::max() || f.rows() > std::numeric_limits<int>::max())
        throw std::overflow_error("MeshLib index limit exceeded");
    MR::VertCoords coords;
    coords.resize(v.rows());
    for (Eigen::Index i = 0; i < v.rows(); ++i) {
        if (v.row(i).cwiseAbs().maxCoeff() > std::numeric_limits<float>::max())
            throw std::overflow_error("MeshLib float coordinate overflow");
        coords[MR::VertId(int(i))] = {float(v(i, 0)), float(v(i, 1)), float(v(i, 2))};
    }
    MR::Triangulation tris;
    tris.reserve(f.rows());
    for (Eigen::Index i = 0; i < f.rows(); ++i)
        tris.push_back(
            {MR::VertId(int(f(i, 0))), MR::VertId(int(f(i, 1))), MR::VertId(int(f(i, 2)))});
    auto result = MR::Mesh::fromTriangles(std::move(coords), tris);
    if (result.topology.numValidFaces() != f.rows())
        throw std::invalid_argument("MeshLib rejected faces: check orientation, "
                                    "degeneracy and manifold topology");
    return result;
}
inline SindreMesh from_mr(MR::Mesh m) {
    m.pack();
    Vertices v(m.topology.numValidVerts(), 3);
    Faces f(m.topology.numValidFaces(), 3);
    for (Eigen::Index i = 0; i < v.rows(); ++i) {
        const auto &p = m.points[MR::VertId(int(i))];
        v.row(i) << p.x, p.y, p.z;
    }
    for (Eigen::Index i = 0; i < f.rows(); ++i) {
        auto t = m.topology.getTriVerts(MR::FaceId(int(i)));
        for (int k = 0; k < 3; ++k)
            f(i, k) = int(t[k]);
    }
    return SindreMesh(v, f);
}
#endif
#if defined(SINDRECPP_UTILS3D_CGAL)
using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using CgalMesh = CGAL::Surface_mesh<Kernel::Point_3>;
inline CgalMesh to_cgal(const SindreMesh &m) {
    CgalMesh out;
    auto v = m.vertices();
    auto f = m.faces();
    std::vector<CgalMesh::Vertex_index> ids;
    for (Eigen::Index i = 0; i < v.rows(); ++i)
        ids.push_back(out.add_vertex({v(i, 0), v(i, 1), v(i, 2)}));
    for (Eigen::Index i = 0; i < f.rows(); ++i)
        if (out.add_face(ids[f(i, 0)], ids[f(i, 1)], ids[f(i, 2)]) == CgalMesh::null_face())
            throw std::invalid_argument("CGAL requires consistently oriented manifold triangles");
    return out;
}
inline SindreMesh from_cgal(CgalMesh m) {
    m.collect_garbage();
    Vertices v(m.number_of_vertices(), 3);
    Faces f(m.number_of_faces(), 3);
    std::map<CgalMesh::Vertex_index, std::int64_t> ids;
    Eigen::Index i = 0;
    for (auto id : m.vertices()) {
        auto p = m.point(id);
        v.row(i) << CGAL::to_double(p.x()), CGAL::to_double(p.y()), CGAL::to_double(p.z());
        ids[id] = i++;
    }
    i = 0;
    for (auto face : m.faces()) {
        int k = 0;
        for (auto id : CGAL::vertices_around_face(m.halfedge(face), m)) {
            if (k >= 3)
                throw std::runtime_error("CGAL returned a nontriangle");
            f(i, k++) = ids.at(id);
        }
        if (k != 3)
            throw std::runtime_error("CGAL returned a nontriangle");
        ++i;
    }
    return SindreMesh(v, f);
}
#endif
#if defined(SINDRECPP_UTILS3D_OPEN3D)
inline open3d::geometry::TriangleMesh to_open3d(const SindreMesh &m) {
    auto v = m.vertices();
    auto f = m.faces();
    if (v.rows() > std::numeric_limits<int>::max())
        throw std::overflow_error("Open3D index limit exceeded");
    open3d::geometry::TriangleMesh out;
    for (Eigen::Index i = 0; i < v.rows(); ++i)
        out.vertices_.emplace_back(v.row(i).transpose());
    for (Eigen::Index i = 0; i < f.rows(); ++i)
        out.triangles_.emplace_back(int(f(i, 0)), int(f(i, 1)), int(f(i, 2)));
    return out;
}
inline SindreMesh from_open3d(const open3d::geometry::TriangleMesh &m) {
    Vertices v(m.vertices_.size(), 3);
    Faces f(m.triangles_.size(), 3);
    for (Eigen::Index i = 0; i < v.rows(); ++i)
        v.row(i) = m.vertices_[i].transpose();
    for (Eigen::Index i = 0; i < f.rows(); ++i)
        f.row(i) = m.triangles_[i].cast<std::int64_t>().transpose();
    return SindreMesh(v, f);
}
inline open3d::geometry::PointCloud pointcloud(const Vertices &v) {
    if (v.rows() == 0 || !v.allFinite())
        throw std::invalid_argument("Expected nonempty finite point cloud");
    open3d::geometry::PointCloud p;
    for (Eigen::Index i = 0; i < v.rows(); ++i)
        p.points_.emplace_back(v.row(i).transpose());
    return p;
}
inline Vertices points(const open3d::geometry::PointCloud &p) {
    Vertices v(p.points_.size(), 3);
    for (Eigen::Index i = 0; i < v.rows(); ++i)
        v.row(i) = p.points_[i].transpose();
    return v;
}
#endif
#if defined(SINDRECPP_UTILS3D_VCG)
class VcgVertex;
class VcgFace;
struct VcgTypes : vcg::UsedTypes<vcg::Use<VcgVertex>::AsVertexType, vcg::Use<VcgFace>::AsFaceType> {
};
class VcgVertex : public vcg::Vertex<VcgTypes, vcg::vertex::Coord3d, vcg::vertex::BitFlags> {};
class VcgFace : public vcg::Face<VcgTypes, vcg::face::VertexRef, vcg::face::BitFlags> {};
class VcgMesh : public vcg::tri::TriMesh<std::vector<VcgVertex>, std::vector<VcgFace>> {};
inline SindreMesh clean_vcg(const SindreMesh &m) {
    VcgMesh out;
    auto v = m.vertices();
    auto f = m.faces();
    vcg::tri::Allocator<VcgMesh>::AddVertices(out, v.rows());
    vcg::tri::Allocator<VcgMesh>::AddFaces(out, f.rows());
    for (Eigen::Index i = 0; i < v.rows(); ++i)
        out.vert[i].P() = vcg::Point3d(v(i, 0), v(i, 1), v(i, 2));
    for (Eigen::Index i = 0; i < f.rows(); ++i)
        for (int k = 0; k < 3; ++k)
            out.face[i].V(k) = &out.vert[f(i, k)];
    vcg::tri::Clean<VcgMesh>::RemoveDuplicateVertex(out);
    vcg::tri::Clean<VcgMesh>::RemoveDuplicateFace(out);
    vcg::tri::Clean<VcgMesh>::RemoveUnreferencedVertex(out);
    vcg::tri::Allocator<VcgMesh>::CompactEveryVector(out);
    Vertices nv(out.vert.size(), 3);
    Faces nf(out.face.size(), 3);
    for (Eigen::Index i = 0; i < nv.rows(); ++i)
        for (int k = 0; k < 3; ++k)
            nv(i, k) = out.vert[i].P()[k];
    for (Eigen::Index i = 0; i < nf.rows(); ++i)
        for (int k = 0; k < 3; ++k)
            nf(i, k) = out.face[i].V(k) - out.vert.data();
    return SindreMesh(nv, nf);
}
#endif
} // namespace detail

struct DecimateOptions {
    std::size_t target_faces = 10000;
    Backend backend = Backend::automatic;
};
inline SindreMesh decimate(const SindreMesh &m, const DecimateOptions &o = {}) {
    detail::require_surface(m);
    if (!o.target_faces)
        throw std::invalid_argument("target_faces must be positive");
    auto b = get_backend(Operation::decimate, o.backend);
    if (o.target_faces >= std::size_t(m.nfaces()))
        return m.clone();
#if defined(SINDRECPP_UTILS3D_MESHLIB)
    if (b == Backend::meshlib) {
        auto x = detail::to_mr(m);
        MR::DecimateSettings s;
        s.maxDeletedFaces = int(m.nfaces() - o.target_faces);
        MR::decimateMesh(x, s);
        return detail::from_mr(std::move(x));
    }
#endif
#if defined(SINDRECPP_UTILS3D_CGAL)
    if (b == Backend::cgal) {
        auto x = detail::to_cgal(m);
        namespace sms = CGAL::Surface_mesh_simplification; // Target is approximate: edge
                                                           // collapses remove
                                                           // topology-dependent faces.
        sms::Count_stop_predicate<detail::CgalMesh> stop(
            std::max<std::size_t>(1, x.number_of_edges() * o.target_faces / m.nfaces()));
        sms::edge_collapse(x, stop);
        return detail::from_cgal(std::move(x));
    }
#endif
#if defined(SINDRECPP_UTILS3D_OPEN3D)
    if (b == Backend::open3d) {
        auto x = detail::to_open3d(m);
        return detail::from_open3d(*x.SimplifyQuadricDecimation(
            int(o.target_faces), std::numeric_limits<double>::infinity(), 1.0));
    }
#endif
#if defined(SINDRECPP_UTILS3D_IGL)
    if (b == Backend::igl) {
        auto v = m.vertices();
        Eigen::MatrixXi f = m.faces().cast<int>();
        Eigen::MatrixXd u;
        Eigen::MatrixXi g;
        Eigen::VectorXi j, i;
        igl::decimate(v, f, o.target_faces, u, g, j, i);
        return SindreMesh(Vertices(u), Faces(g.cast<std::int64_t>()));
    }
#endif
    vtkNew<vtkDecimatePro> d;
    d->SetInputData(m.get_native());
    d->SetTargetReduction(1. - double(o.target_faces) / m.nfaces());
    d->PreserveTopologyOn();
    d->SplittingOff();
    d->Update();
    return SindreMesh(d->GetOutput());
}
struct SmoothOptions {
    int iterations = 20;
    double strength = 0.1;
    bool preserve_volume = true;
    Backend backend = Backend::automatic;
};
inline SindreMesh smooth(const SindreMesh &m, const SmoothOptions &o = {}) {
    detail::require_surface(m);
    if (o.iterations < 1 || !std::isfinite(o.strength) || o.strength <= 0 || o.strength > 1)
        throw std::invalid_argument("Invalid smoothing iterations/strength");
    auto b = get_backend(Operation::smooth, o.backend);
#if defined(SINDRECPP_UTILS3D_MESHLIB)
    if (b == Backend::meshlib) {
        auto x = detail::to_mr(m);
        MR::MeshRelaxParams p;
        p.iterations = o.iterations;
        p.force = float(o.strength);
        if (o.preserve_volume)
            MR::relaxKeepVolume(x, p);
        else
            MR::relax(x, p);
        return detail::from_mr(std::move(x));
    }
#endif
#if defined(SINDRECPP_UTILS3D_OPEN3D)
    if (b == Backend::open3d) {
        auto x = detail::to_open3d(m);
        return detail::from_open3d(
            o.preserve_volume ? *x.FilterSmoothTaubin(o.iterations, o.strength, -o.strength * 1.06)
                              : *x.FilterSmoothLaplacian(o.iterations, o.strength));
    }
#endif
    if (!o.preserve_volume)
        throw std::invalid_argument("VTK smooth uses windowed-sinc; "
                                    "preserve_volume=false needs MeshLib/Open3D");
    vtkNew<vtkWindowedSincPolyDataFilter> s;
    s->SetInputData(m.get_native());
    s->SetNumberOfIterations(o.iterations);
    s->SetPassBand(o.strength);
    s->NormalizeCoordinatesOn();
    s->Update();
    return SindreMesh(s->GetOutput());
}
struct RemeshOptions {
    double edge_length = 1;
    unsigned iterations = 3;
    Backend backend = Backend::automatic;
};
inline SindreMesh remesh(const SindreMesh &m, const RemeshOptions &o = {}) {
    detail::require_surface(m);
    detail::positive(o.edge_length, "edge_length");
    if (!o.iterations)
        throw std::invalid_argument("iterations must be positive");
    auto b = get_backend(Operation::remesh, o.backend);
#if defined(SINDRECPP_UTILS3D_MESHLIB)
    if (b == Backend::meshlib) {
        auto x = detail::to_mr(m);
        MR::RemeshSettings p;
        p.targetEdgeLen = float(o.edge_length);
        for (unsigned i = 0; i < o.iterations; ++i)
            if (!MR::remesh(x, p))
                throw std::runtime_error("MeshLib remesh interrupted");
        return detail::from_mr(std::move(x));
    }
#endif
#if defined(SINDRECPP_UTILS3D_CGAL)
    if (b == Backend::cgal) {
        auto x = detail::to_cgal(m);
        CGAL::Polygon_mesh_processing::isotropic_remeshing(
            x.faces(), o.edge_length, x, CGAL::parameters::number_of_iterations(o.iterations));
        return detail::from_cgal(std::move(x));
    }
#endif
    throw std::runtime_error("Remesh backend unavailable");
}
enum class BooleanOperation { unite, intersect, subtract };
inline SindreMesh boolean_mesh(const SindreMesh &a, const SindreMesh &b, BooleanOperation op,
                               Backend requested = Backend::automatic) {
    detail::require_surface(a);
    detail::require_surface(b);
    if (!a.is_watertight() || !b.is_watertight())
        throw std::invalid_argument("Boolean requires closed surfaces");
    auto backend = get_backend(Operation::boolean_op, requested);
#if defined(SINDRECPP_UTILS3D_MESHLIB)
    if (backend == Backend::meshlib) {
        auto x = detail::to_mr(a), y = detail::to_mr(b);
        auto native_op = op == BooleanOperation::unite       ? MR::BooleanOperation::Union
                         : op == BooleanOperation::intersect ? MR::BooleanOperation::Intersection
                                                             : MR::BooleanOperation::DifferenceAB;
        auto result = MR::boolean(x, y, native_op);
        if (!result)
            throw std::runtime_error("MeshLib boolean: " + result.errorString);
        return detail::from_mr(std::move(result.mesh));
    }
#endif
#if defined(SINDRECPP_UTILS3D_CGAL)
    if (backend == Backend::cgal) {
        auto x = detail::to_cgal(a), y = detail::to_cgal(b);
        detail::CgalMesh out;
        namespace pmp = CGAL::Polygon_mesh_processing;
        if (pmp::does_self_intersect(x) || pmp::does_self_intersect(y))
            throw std::invalid_argument("Boolean input has self intersections");
        bool ok = op == BooleanOperation::unite ? pmp::corefine_and_compute_union(x, y, out)
                  : op == BooleanOperation::intersect
                      ? pmp::corefine_and_compute_intersection(x, y, out)
                      : pmp::corefine_and_compute_difference(x, y, out);
        if (!ok)
            throw std::runtime_error("CGAL boolean failed to produce a manifold result");
        return detail::from_cgal(std::move(out));
    }
#endif
    throw std::runtime_error("Boolean backend unavailable");
}
inline bool has_self_intersections(const SindreMesh &m, Backend requested = Backend::automatic) {
    detail::require_surface(m);
    auto b = get_backend(Operation::self_intersections, requested);
#if defined(SINDRECPP_UTILS3D_MESHLIB)
    if (b == Backend::meshlib) {
        auto x = detail::to_mr(m);
        auto r = MR::findSelfCollidingTriangles(x, nullptr);
        if (!r)
            throw std::runtime_error(r.error());
        return *r;
    }
#endif
#if defined(SINDRECPP_UTILS3D_CGAL)
    if (b == Backend::cgal)
        return CGAL::Polygon_mesh_processing::does_self_intersect(detail::to_cgal(m));
#endif
    throw std::runtime_error("Self-intersection backend unavailable");
}
inline SindreMesh fill_holes(const SindreMesh &m, Backend requested = Backend::automatic) {
    detail::require_surface(m);
    auto b = get_backend(Operation::fill_holes, requested);
#if defined(SINDRECPP_UTILS3D_MESHLIB)
    if (b == Backend::meshlib) {
        auto x = detail::to_mr(m);
        auto holes = x.topology.findHoleRepresentiveEdges();
        MR::FillHoleParams p;
        bool bad = false;
        p.stopBeforeBadTriangulation = &bad;
        for (auto e : holes) {
            MR::fillHole(x, e, p);
            if (bad)
                throw std::runtime_error("MeshLib rejected hole triangulation");
        }
        return detail::from_mr(std::move(x));
    }
#endif
#if defined(SINDRECPP_UTILS3D_CGAL)
    if (b == Backend::cgal) {
        auto x = detail::to_cgal(m);
        std::vector<detail::CgalMesh::Halfedge_index> loops;
        CGAL::Polygon_mesh_processing::extract_boundary_cycles(x, std::back_inserter(loops));
        for (auto h : loops) {
            std::vector<detail::CgalMesh::Face_index> patch;
            CGAL::Polygon_mesh_processing::triangulate_hole(x, h, std::back_inserter(patch));
            if (patch.empty())
                throw std::runtime_error("CGAL could not fill hole");
        }
        return detail::from_cgal(std::move(x));
    }
#endif
    vtkNew<vtkFillHolesFilter> f;
    f->SetInputData(m.get_native());
    f->SetHoleSize(std::numeric_limits<double>::max());
    f->Update();
    return SindreMesh(f->GetOutput());
}
inline SindreMesh clean(const SindreMesh &m, Backend requested = Backend::automatic) {
    auto b = get_backend(Operation::clean, requested);
#if defined(SINDRECPP_UTILS3D_MESHLIB)
    if (b == Backend::meshlib) {
        auto x = detail::to_mr(m);
        MR::MeshBuilder::uniteCloseVertices(x, 0.f, false);
        return detail::from_mr(std::move(x));
    }
#endif
#if defined(SINDRECPP_UTILS3D_OPEN3D)
    if (b == Backend::open3d) {
        auto x = detail::to_open3d(m);
        x.RemoveDuplicatedVertices()
            .RemoveDuplicatedTriangles()
            .RemoveDegenerateTriangles()
            .RemoveUnreferencedVertices();
        return detail::from_open3d(x);
    }
#endif
#if defined(SINDRECPP_UTILS3D_VCG)
    if (b == Backend::vcg)
        return detail::clean_vcg(m);
#endif
    return m.clean();
}
// Explicit limited repair, not a promise to resolve all
// self-intersections/non-manifold inputs.
inline SindreMesh fix_mesh(const SindreMesh &m, bool close_holes = true,
                           Backend b = Backend::automatic) {
    auto x = clean(m, b);
    if (close_holes)
        x = fill_holes(x, b);
    x.compute_normals();
    return x;
}
inline SindreMesh subdivide(const SindreMesh &m, int iterations = 1) {
    detail::require_surface(m);
    if (iterations < 0 || iterations > 8)
        throw std::invalid_argument("Subdivision iterations must be in [0,8]");
    vtkNew<vtkLoopSubdivisionFilter> s;
    s->SetInputData(m.get_native());
    s->SetNumberOfSubdivisions(iterations);
    s->Update();
    return SindreMesh(s->GetOutput());
}
inline SindreMesh cut_plane(const SindreMesh &m, const Eigen::Vector3d &origin,
                            const Eigen::Vector3d &normal, bool keep_negative = false) {
    detail::require_surface(m);
    if (!origin.allFinite() || !normal.allFinite() || normal.norm() == 0)
        throw std::invalid_argument("Invalid plane");
    vtkNew<vtkPlane> p;
    p->SetOrigin(origin.data());
    p->SetNormal(normal.data());
    vtkNew<vtkClipPolyData> c;
    c->SetInputData(m.get_native());
    c->SetClipFunction(p);
    c->SetInsideOut(keep_negative);
    c->Update();
    return SindreMesh(c->GetOutput());
}
inline SindreMesh reverse_faces(const SindreMesh &m) {
    vtkNew<vtkReverseSense> r;
    r->SetInputData(m.get_native());
    r->ReverseCellsOn();
    r->ReverseNormalsOn();
    r->Update();
    return SindreMesh(r->GetOutput());
}
struct Projection {
    Vertices points;
    Eigen::VectorXd distances;
    Labels face_ids;
};
inline Projection project_points(const SindreMesh &m, const Vertices &q) {
    detail::require_surface(m);
    if (!q.allFinite())
        throw std::invalid_argument("Nonfinite query");
    vtkNew<vtkStaticCellLocator> l;
    l->SetDataSet(m.get_native());
    l->BuildLocator();
    Projection r{Vertices(q.rows(), 3), Eigen::VectorXd(q.rows()), Labels(q.rows())};
    for (Eigen::Index i = 0; i < q.rows(); ++i) {
        double p[3], d;
        vtkIdType id;
        int sub;
        l->FindClosestPoint(q.row(i).data(), p, id, sub, d);
        for (int k = 0; k < 3; ++k)
            r.points(i, k) = p[k];
        r.distances[i] = std::sqrt(d);
        r.face_ids[i] = id;
    }
    return r;
}
inline Eigen::VectorXd signed_distance(const SindreMesh &m, const Vertices &q) {
    detail::require_surface(m);
    if (!m.is_watertight() || !q.allFinite())
        throw std::invalid_argument(
            "Signed distance requires closed, oriented surface and finite queries");
    vtkNew<vtkImplicitPolyDataDistance> d;
    d->SetInput(m.get_native());
    Eigen::VectorXd x(q.rows());
    for (Eigen::Index i = 0; i < q.rows(); ++i)
        x[i] = d->EvaluateFunction(const_cast<double *>(q.row(i).data()));
    return x;
}
inline Labels labels_mapping(const Vertices &old_vertices, const Vertices &new_vertices,
                             const Labels &old_labels) {
    if (old_vertices.rows() != old_labels.size())
        throw std::invalid_argument("Source label count mismatch");
    Faces f(0, 3);
    auto ids = SindreMesh(old_vertices, f).get_near_idx(new_vertices);
    Labels out(ids.size());
    for (Eigen::Index i = 0; i < ids.size(); ++i)
        out[i] = old_labels[ids[i]];
    return out;
}
inline Labels vertex_labels_to_face_labels(const Faces &f, const Labels &labels) {
    Labels out(f.rows());
    for (Eigen::Index i = 0; i < f.rows(); ++i) {
        std::map<std::int64_t, int> counts;
        for (int k = 0; k < 3; ++k) {
            if (f(i, k) < 0 || f(i, k) >= labels.size())
                throw std::out_of_range("Label face index");
            ++counts[labels[f(i, k)]];
        }
        int best = 0;
        for (auto p : counts)
            if (p.second > best) {
                out[i] = p.first;
                best = p.second;
            }
    }
    return out;
}
inline Labels face_labels_to_vertex_labels(const Faces &f, const Labels &labels, Eigen::Index n,
                                           std::int64_t unused_label = -1) {
    if (n < 0 || labels.size() != f.rows())
        throw std::invalid_argument("Label shape mismatch");
    std::vector<std::map<std::int64_t, int>> counts(n);
    for (Eigen::Index i = 0; i < f.rows(); ++i)
        for (int k = 0; k < 3; ++k) {
            if (f(i, k) < 0 || f(i, k) >= n)
                throw std::out_of_range("Label face index");
            ++counts[f(i, k)][labels[i]];
        }
    Labels out = Labels::Constant(n, unused_label);
    for (Eigen::Index i = 0; i < n; ++i) {
        int best = 0;
        for (auto p : counts[i])
            if (p.second > best) {
                out[i] = p.first;
                best = p.second;
            }
    }
    return out;
}
struct Normalization {
    Eigen::Vector3d center;
    double scale;
    Eigen::Matrix4d transform;
};
inline Normalization get_normalize(const SindreMesh &m) {
    auto c = m.center();
    auto s = m.radius();
    detail::positive(s, "radius");
    Eigen::Matrix4d t = Eigen::Matrix4d::Identity();
    t.topLeftCorner<3, 3>() /= s;
    t.topRightCorner<3, 1>() = -c / s;
    return {c, s, t};
}
inline Matrix get_gaussian_heatmap(const Vertices &points, const Vertices &keys, double sigma = .5,
                                   bool normalize = false) {
    detail::positive(sigma, "sigma");
    if (!points.allFinite() || !keys.allFinite())
        throw std::invalid_argument("Nonfinite heatmap points");
    Matrix h(points.rows(), keys.rows());
    for (Eigen::Index j = 0; j < keys.rows(); ++j) {
        for (Eigen::Index i = 0; i < points.rows(); ++i)
            h(i, j) = std::exp(-((points.row(i) - keys.row(j)) / sigma).squaredNorm() / 2);
        if (normalize && h.rows()) {
            double mx = h.col(j).maxCoeff();
            if (mx > 0)
                h.col(j) /= mx;
        }
    }
    return h;
}
inline Eigen::VectorXd get_curvature(const SindreMesh &m, Backend requested = Backend::automatic) {
    detail::require_surface(m);
    auto b = get_backend(Operation::curvature, requested);
#if defined(SINDRECPP_UTILS3D_IGL)
    if (b == Backend::igl) {
        Eigen::MatrixXd d1, d2;
        Eigen::VectorXd k1, k2;
        igl::principal_curvature(m.vertices(), m.faces().cast<int>().eval(), d1, d2, k1, k2);
        return (k1 + k2) * .5;
    }
#endif
    return m.get_curvature();
}
inline Matrix get_uv(const SindreMesh &m) {
    detail::require_surface(m);
    get_backend(Operation::uv);
#if defined(SINDRECPP_UTILS3D_IGL)
    if (!m.get_non_manifold_edges().empty() ||
        m.npoints() - Eigen::Index(m.get_edges().size()) + m.nfaces() != 1 ||
        m.split_component_by_faces().size() != 1 || (m.faces_area().array() <= 0).any())
        throw std::invalid_argument("Harmonic UV requires a connected nondegenerate disk mesh");
    const auto v = m.vertices();
    Eigen::MatrixXi f = m.faces().cast<int>();
    std::vector<std::vector<int>> loops;
    igl::boundary_loop(f, loops);
    if (loops.size() != 1)
        throw std::invalid_argument("Harmonic UV requires one boundary loop (disk topology)");
    Eigen::VectorXi b(loops[0].size());
    for (Eigen::Index i = 0; i < b.size(); ++i)
        b[i] = loops[0][i];
    Eigen::MatrixXd bc, uv;
    igl::map_vertices_to_circle(v, b, bc);
    // A single triangle / triangulated polygon may have no interior unknowns.
    // libigl's solver asserts in that case; the boundary map already is the solution.
    if (b.size() == v.rows()) {
        uv.resize(v.rows(), 2);
        for (Eigen::Index i = 0; i < b.size(); ++i)
            uv.row(b[i]) = bc.row(i);
        return uv;
    }
    if (!igl::harmonic(v, f, b, bc, 1, uv))
        throw std::runtime_error("Harmonic UV failed");
    return uv;
#else
    throw std::runtime_error("Enable libigl for UV");
#endif
}
struct Registration {
    Eigen::Matrix4d transform;
    double fitness;
    double rmse;
};
inline Registration register_icp(const Vertices &source, const Vertices &target,
                                 double max_distance, int iterations = 50,
                                 const Eigen::Matrix4d &initial = Eigen::Matrix4d::Identity()) {
    detail::positive(max_distance, "max_distance");
    if (iterations < 1 || !initial.allFinite())
        throw std::invalid_argument("Invalid ICP options");
    get_backend(Operation::registration);
#if defined(SINDRECPP_UTILS3D_OPEN3D)
    auto s = detail::pointcloud(source), t = detail::pointcloud(target);
    namespace reg = open3d::pipelines::registration;
    auto r = reg::RegistrationICP(s, t, max_distance, initial,
                                  reg::TransformationEstimationPointToPoint(false),
                                  reg::ICPConvergenceCriteria(1e-6, 1e-6, iterations));
    return {r.transformation_, r.fitness_, r.inlier_rmse_};
#else
    throw std::runtime_error("Enable Open3D for ICP");
#endif
}
inline Vertices sample(const SindreMesh &m, std::size_t count) {
    detail::require_surface(m);
    if (!count)
        throw std::invalid_argument("Sample count must be positive");
    get_backend(Operation::sample);
#if defined(SINDRECPP_UTILS3D_OPEN3D)
    auto x = detail::to_open3d(m);
    return detail::points(*x.SamplePointsUniformly(count));
#else
    throw std::runtime_error("Enable Open3D for surface sampling");
#endif
}
inline SindreMesh reconstruct_poisson(const Vertices &points, const Vertices &normals,
                                      std::size_t depth = 8) {
    if (points.rows() != normals.rows() || !normals.allFinite() || depth < 2 || depth > 16)
        throw std::invalid_argument("Invalid Poisson points/normals/depth");
    get_backend(Operation::reconstruction);
#if defined(SINDRECPP_UTILS3D_OPEN3D)
    auto p = detail::pointcloud(points);
    for (Eigen::Index i = 0; i < normals.rows(); ++i) {
        if (normals.row(i).norm() == 0)
            throw std::invalid_argument("Zero reconstruction normal");
        p.normals_.emplace_back(normals.row(i).normalized().transpose());
    }
    auto r = open3d::geometry::TriangleMesh::CreateFromPointCloudPoisson(p, depth);
    return detail::from_open3d(*std::get<0>(r));
#else
    throw std::runtime_error("Enable Open3D for Poisson reconstruction");
#endif
}
} // namespace sindrecpp::utils3d
