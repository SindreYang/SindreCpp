#include <sindre/utils_3d.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace sindre::utils_3d;
namespace math = sindre::math;

namespace {
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

Mesh create_tetrahedron() {
    Vertices vertices(4, 3);
    vertices << 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1;
    Faces faces(4, 3);
    faces << 0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3;
    return Mesh(vertices, faces);
}

Mesh create_open_square() {
    Vertices vertices(4, 3);
    vertices << 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0;
    Faces faces(2, 3);
    faces << 0, 1, 2, 0, 2, 3;
    return Mesh(vertices, faces);
}

Mesh create_grid(std::int64_t side) {
    const auto count = (side + 1) * (side + 1);
    Vertices vertices(count, 3);
    for (std::int64_t y = 0; y <= side; ++y)
        for (std::int64_t x = 0; x <= side; ++x) {
            const auto id = y * (side + 1) + x;
            vertices(id, 0) = static_cast<double>(x);
            vertices(id, 1) = static_cast<double>(y);
            vertices(id, 2) = 0.0;
        }
    Faces faces(2 * side * side, 3);
    std::int64_t face = 0;
    for (std::int64_t y = 0; y < side; ++y)
        for (std::int64_t x = 0; x < side; ++x) {
            const auto a = y * (side + 1) + x;
            const auto b = a + 1;
            const auto c = a + side + 1;
            const auto d = c + 1;
            faces.row(face++) << a, b, d;
            faces.row(face++) << a, d, c;
        }
    return Mesh(vertices, faces);
}
}

int main() {
    try {
        auto mesh = create_tetrahedron();
        check(static_cast<bool>(validate_mesh(mesh)), "Mesh validation");
        check(mesh.npoints() == 4 && mesh.nfaces() == 4 && mesh.npoint() == 4 &&
                  mesh.nface() == 4,
              "Mesh dimensions");
        const auto aabb = mesh.get_aabb();
        check(aabb.minimum.isApprox(math::Vector3::Zero()) &&
                  aabb.maximum.isApprox(math::Vector3::Ones()) &&
                  aabb.dimensions.isApprox(math::Vector3::Ones()),
              "Axis-aligned bounding box");
        const auto obb = mesh.get_obb();
        for (Eigen::Index i = 0; i < mesh.npoints(); ++i) {
            const auto local = obb.axes.transpose() *
                (mesh.vertices().row(i).transpose() - obb.center);
            check((local.array().abs() <=
                   (obb.half_extents.array() + 1e-10)).all(),
                  "Oriented bounding box must contain vertices");
        }
        const auto sphere = mesh.get_min_sphere();
        check(std::abs(sphere.radius - std::sqrt(2.0 / 3.0)) < 1e-10,
              "Minimum bounding sphere radius");
        for (Eigen::Index i = 0; i < mesh.npoints(); ++i)
            check((mesh.vertices().row(i).transpose() - sphere.center).norm() <=
                      sphere.radius + 1e-10,
                  "Minimum bounding sphere must contain vertices");
        check(mesh.is_watertight(), "Tetrahedron must be closed");
        check(mesh.get_boundary().empty(), "Closed mesh boundary");
        check(mesh.get_edges().size() == 6, "Unique mesh edges");
        check(std::abs(mesh.area() - (std::sqrt(3.0) + 3.0) / 2.0) < 1e-12,
              "Tetrahedron area");
        check(std::abs(mesh.signed_volume() - 1.0 / 6.0) < 1e-12,
              "Tetrahedron volume");

        auto open_square = SindreMesh(create_open_square());
        auto ordered_boundary = open_square.boundary();
        check(ordered_boundary && ordered_boundary.value().size() == 1,
              "Ordered boundary loop count");
        check(ordered_boundary.value()[0].size() == 4,
              "Ordered boundary loop vertices");
        for (std::size_t i = 0; i < ordered_boundary.value()[0].size(); ++i) {
            const auto left = ordered_boundary.value()[0][i];
            const auto right = ordered_boundary.value()[0][(i + 1) % 4];
            check((left == 0 && (right == 1 || right == 3)) ||
                      (left == 1 && (right == 0 || right == 2)) ||
                      (left == 2 && (right == 1 || right == 3)) ||
                      (left == 3 && (right == 0 || right == 2)),
                  "Boundary points must be connected in order");
        }
        auto unordered_boundary = open_square.boundary(false, false);
        check(unordered_boundary && unordered_boundary.value().size() == 1 &&
                  std::is_sorted(unordered_boundary.value()[0].begin(),
                                 unordered_boundary.value()[0].end()),
              "Unordered boundary loop must be stable");

        auto elevated_square_vertices = create_open_square().vertices();
        elevated_square_vertices.col(2).setConstant(1.0);
        auto elevated_square = Mesh(elevated_square_vertices, create_open_square().faces());
        JoinStripsOptions strip_options;
        auto joined_strips = join_mesh_strips(open_square.mesh(), elevated_square, strip_options);
        check(joined_strips && joined_strips.value().value.npoints() == 10 &&
                  joined_strips.value().value.nfaces() == 8,
              "VTK triangle strips between matching boundaries");
        auto facade_strips = open_square.join_with_strips(SindreMesh(elevated_square));
        check(facade_strips && facade_strips.value().nfaces() == 8,
              "SindreMesh join_with_strips facade");

        std::vector<Vertices> first_lines{open_square.vertices()};
        std::vector<Vertices> second_lines{elevated_square.vertices()};
        strip_options.closed = false;
        auto open_strips = join_mesh_strips(first_lines, second_lines, strip_options);
        check(open_strips && open_strips.value().value.nfaces() == 6,
              "Open VTK triangle strip");
        auto invalid_strips = join_mesh_strips(
            first_lines, std::vector<Vertices>{elevated_square.vertices(), elevated_square.vertices()});
        check(!invalid_strips && invalid_strips.error().code ==
                  std::make_error_code(std::errc::invalid_argument),
              "Mismatched strip counts must fail");

        auto clean = clean_mesh(mesh);
        check(clean && clean.value().value.nfaces() == 4, "Mesh cleaning");

        Vertices unused_vertices(5, 3);
        unused_vertices << 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 99, 99, 99;
        auto cleaned_unused = clean_mesh(Mesh(unused_vertices, mesh.faces()));
        check(cleaned_unused && cleaned_unused.value().value.npoints() == 4,
              "Cleaning must remove unused vertices");

        Faces broken_faces(6, 3);
        broken_faces << 0, 2, 1, 0, 2, 1, 0, 0, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3;
        auto fixed = fix_mesh(Mesh(mesh.vertices(), broken_faces));
        check(fixed && fixed.value().value.nfaces() == 4,
              "Mesh fix must remove duplicate and degenerate faces");

        auto filled = open_square.fill_hole();
        check(filled && filled.value().nfaces() >= open_square.nfaces(),
              "Fast hole filling");
        FillHolesOptions targeted_hole;
        targeted_hole.method = FillHoleMethod::ear_clipping;
        targeted_hole.boundary_vertices = {0, 1, 2, 3, 0};
        auto targeted_fill = fill_mesh_holes(open_square.mesh(), targeted_hole);
        check(targeted_fill && targeted_fill.value().value.nfaces() == 4,
              "Targeted ordered hole filling");
        CgalFillHolesOptions cgal_options;
#if defined(SINDRE_UTILS_3D_CGAL)
        cgal_options.method = CgalHoleFillMethod::triangulate;
        auto cgal_fill = open_square.fill_holes_by_cgal(cgal_options);
        check(cgal_fill && cgal_fill.value().nfaces() == 4,
              "CGAL hole filling");
#else
        auto cgal_fill = open_square.fill_holes_by_cgal(cgal_options);
        check(!cgal_fill &&
                  cgal_fill.error().code ==
                      std::make_error_code(std::errc::function_not_supported),
              "CGAL hole filling must report an unavailable backend");
#endif

        RemeshOptions remesh_options;
        remesh_options.edge_length = 1.0;
        remesh_options.iterations = 1;
        auto remeshed = SindreMesh(mesh).remesh(remesh_options);
        check(remeshed && remeshed.value().nfaces() >= mesh.nfaces(),
              "VTK feature-preserving remesh");

        auto uniformized = SindreMesh(mesh).uniformize();
        check(uniformized && uniformized.value().nfaces() >= mesh.nfaces(),
              "VTK mesh uniformization");
        auto global_subdivision = SindreMesh(mesh).subdivide(1);
        check(global_subdivision && global_subdivision.value().nfaces() == 16,
              "Global mesh subdivision");
        auto local_subdivision = SindreMesh(mesh).subdivide_faces({0});
        check(local_subdivision && local_subdivision.value().nfaces() == 10,
              "Conforming local mesh subdivision");

        SmoothOptions smooth_options;
        smooth_options.iterations = 1;
        auto smooth = smooth_mesh(mesh, smooth_options);
        check(smooth && smooth.value().value.npoints() == 4, "Mesh smoothing");

        FeatureSmoothingOptions feature_smoothing_options;
        feature_smoothing_options.iterations = 2;
        feature_smoothing_options.feature_angle = 30.0;
        auto feature_smoothing = smooth_mesh_features(create_grid(2), feature_smoothing_options);
        check(feature_smoothing && feature_smoothing.value().value.npoints() == 9 &&
                  feature_smoothing.value().value.nfaces() == 8 &&
                  feature_smoothing.value().value.vertices().allFinite(),
              "Feature-preserving VTK smoothing");

        Vertices fgcf_curve(4, 3);
        fgcf_curve << 0.1, 0.1, 0.0,
            0.7, 0.1, 0.0,
            0.7, 0.2, 0.0,
            0.1, 0.7, 0.0;
        FgcfOptions fgcf_options;
        fgcf_options.iterations = 3;
        auto fgcf = smooth_curve_by_fgcf(mesh, fgcf_curve, fgcf_options);
        check(fgcf && fgcf.value().value.curve.rows() == fgcf_curve.rows() &&
                  fgcf.value().value.curve.allFinite() &&
                  fgcf.value().value.final_length > 0.0,
              "FGCF curve flow");
        auto fgcf_projection = project_mesh_points(mesh, fgcf.value().value.curve);
        check(fgcf_projection && fgcf_projection.value().distances.maxCoeff() < 1e-10,
              "FGCF output must remain on the mesh");

        CgalSegmentationOptions segmentation_options;
#if defined(SINDRE_UTILS_3D_CGAL)
        auto segmentation = segment_mesh_by_cgal(mesh, segmentation_options);
        check(segmentation &&
                  segmentation.value().value.face_labels.rows() == mesh.nfaces() &&
                  segmentation.value().value.segment_count > 0,
              "CGAL SDF graph-cut segmentation");
#else
        auto segmentation = segment_mesh_by_cgal(mesh, segmentation_options);
        check(!segmentation && segmentation.error().code ==
                  std::make_error_code(std::errc::function_not_supported),
              "CGAL segmentation must report an unavailable backend");
#endif

        SmoothOptions local_smooth_options;
        local_smooth_options.scope = SmoothScope::local;
        local_smooth_options.vertex_indices = {0};
        local_smooth_options.iterations = 2;
        auto local_smooth = smooth_mesh(mesh, local_smooth_options);
        check(local_smooth && local_smooth.value().value.npoints() == mesh.npoints() &&
                  local_smooth.value().value.nfaces() == mesh.nfaces() &&
                  (local_smooth.value().value.vertices().row(1) -
                       mesh.vertices().row(1)).norm() == 0.0,
              "Local vertex smoothing");

        SmoothOptions local_face_smooth_options;
        local_face_smooth_options.scope = SmoothScope::local;
        local_face_smooth_options.face_indices = {0};
        local_face_smooth_options.preserve_volume = false;
        auto local_face_smooth = smooth_mesh(mesh, local_face_smooth_options);
        check(local_face_smooth && local_face_smooth.value().value.npoints() == mesh.npoints() &&
                  (local_face_smooth.value().value.vertices().row(3) -
                       mesh.vertices().row(3)).norm() == 0.0,
              "Local face smoothing");

        SmoothOptions invalid_local_smooth;
        invalid_local_smooth.scope = SmoothScope::local;
        auto missing_local_selection = smooth_mesh(mesh, invalid_local_smooth);
        check(!missing_local_selection &&
                  missing_local_selection.error().code ==
                      std::make_error_code(std::errc::invalid_argument),
              "Local smoothing requires a selection");

        auto deformation_source = mesh.vertices();
        auto deformation_target = deformation_source;
        for (Eigen::Index i = 0; i < deformation_target.rows(); ++i)
            deformation_target(i, 0) += 0.25;
        auto deformed = deform_mesh(mesh, deformation_source, deformation_target);
        check(deformed &&
                  (deformed.value().value.vertices() - deformation_target).norm() < 1e-8,
              "Global thin-plate deformation");

        DeformationOptions local_deformation_options;
        local_deformation_options.scope = DeformationScope::local;
        local_deformation_options.vertex_indices = {0, 1, 2};
        auto locally_deformed = deform_mesh(
            mesh, deformation_source, deformation_target, local_deformation_options);
        check(locally_deformed &&
                  (locally_deformed.value().value.vertices().row(3) -
                       mesh.vertices().row(3)).norm() == 0.0 &&
                  (locally_deformed.value().value.vertices().row(0) -
                       deformation_target.row(0)).norm() < 1e-8,
              "Local thin-plate deformation");

        auto facade_deformed = SindreMesh(mesh).deform(
            deformation_source, deformation_target);
        check(facade_deformed && facade_deformed.value().npoints() == mesh.npoints(),
              "SindreMesh thin-plate deformation facade");

        Matrix vertex_probabilities = Matrix::Zero(mesh.npoints(), 3);
        vertex_probabilities(0, 0) = 1.0;
        vertex_probabilities(1, 1) = 1.0;
        vertex_probabilities(2, 2) = 1.0;
        vertex_probabilities(3, 1) = 1.0;
        GraphCutOptions expansion_options;
        expansion_options.label_level = GraphCutLabelLevel::vertex;
        expansion_options.algorithm = GraphCutAlgorithm::expansion;
        expansion_options.smooth_factor = 0.0;
        expansion_options.keep_label = false;
        auto expansion_labels = optimize_mesh_labels(mesh, vertex_probabilities,
                                                     expansion_options);
        check(expansion_labels && expansion_labels.value().label_level ==
                  GraphCutLabelLevel::vertex && expansion_labels.value().labels.size() ==
                  mesh.npoints() && expansion_labels.value().class_count == 3 &&
                  expansion_labels.value().labels(0) == 0 &&
                  expansion_labels.value().labels(1) == 1 &&
                  expansion_labels.value().labels(2) == 2 &&
                  expansion_labels.value().labels(3) == 1,
              "Alpha-expansion must preserve zero-smoothing unary labels");

        GraphCutOptions smoothed_options = expansion_options;
        smoothed_options.smooth_factor = -1.0;
        auto smoothed_labels = optimize_mesh_labels(mesh, vertex_probabilities,
                                                    smoothed_options);
        check(smoothed_labels && std::isfinite(smoothed_labels.value().energy_before) &&
                  std::isfinite(smoothed_labels.value().energy_after) &&
                  smoothed_labels.value().energy_after <=
                      smoothed_labels.value().energy_before + 1e-8,
              "Alpha-expansion must not increase the graph-cut energy");

        GraphCutOptions swap_options = expansion_options;
        swap_options.algorithm = GraphCutAlgorithm::swap;
        auto swap_labels = optimize_mesh_labels(mesh, vertex_probabilities, swap_options);
        check(swap_labels && swap_labels.value().labels == expansion_labels.value().labels,
              "Alpha-beta swap must preserve zero-smoothing unary labels");

        GraphCutOptions face_options;
        face_options.label_level = GraphCutLabelLevel::face;
        face_options.class_count = 2;
        face_options.smooth_factor = 0.0;
        Labels face_labels(mesh.nfaces());
        face_labels << 0, 1, 0, 1;
        auto optimized_face_labels = optimize_mesh_labels(mesh, face_labels, face_options);
        check(optimized_face_labels && optimized_face_labels.value().label_level ==
                  GraphCutLabelLevel::face && optimized_face_labels.value().labels == face_labels,
              "Face hard-label graph cut");
        auto facade_labels = SindreMesh(mesh).optimize_labels(
            vertex_probabilities, expansion_options);
        check(facade_labels && facade_labels.value().labels == expansion_labels.value().labels,
              "SindreMesh graph-cut facade");

        Matrix invalid_probabilities = vertex_probabilities;
        invalid_probabilities(0, 0) = -1.0;
        auto invalid_graph_cut = optimize_mesh_labels(mesh, invalid_probabilities);
        check(!invalid_graph_cut && invalid_graph_cut.error().code ==
                  std::make_error_code(std::errc::invalid_argument),
              "Graph-cut must reject negative probabilities");
        GraphCutOptions invalid_temperature;
        invalid_temperature.temperature = 0.0;
        auto invalid_temperature_result = optimize_mesh_labels(
            mesh, vertex_probabilities, invalid_temperature);
        check(!invalid_temperature_result && invalid_temperature_result.error().code ==
                  std::make_error_code(std::errc::invalid_argument),
              "Graph-cut must reject invalid temperature");

        DecimateOptions decimate_options;
        decimate_options.target_faces = 3;
        auto decimated = decimate_mesh(mesh, decimate_options);
        check(decimated && decimated.value().value.nfaces() <= 4, "Mesh decimation");

        SimplifyOptions simplify_options;
        auto simplified = simplify_mesh(mesh, 3, simplify_options);
        check(simplified && simplified.value().value.nfaces() <= 4,
              "Default VTK mesh simplification");
        check(simplified.value().report.output_faces ==
                  static_cast<std::size_t>(simplified.value().value.nfaces()),
              "Simplification face report");

        simplify_options.algorithm = SimplifyAlgorithm::quadric_decimation;
        auto quadric = simplify_mesh(mesh, 3, simplify_options);
        check(quadric && quadric.value().value.nfaces() <= 4,
              "Quadric mesh simplification");
        simplify_options.algorithm = SimplifyAlgorithm::quadric_clustering;
        auto clustered = simplify_mesh(mesh, 3, simplify_options);
        check(clustered && clustered.value().value.nfaces() <= 4,
              "Quadric clustering simplification");

        auto sampled = sample_mesh_surface(mesh, 32);
        check(sampled && sampled.value().rows() == 32 && sampled.value().allFinite(),
              "Surface sampling");
        SampleOptions random_sampling;
        random_sampling.algorithm = SampleAlgorithm::random;
        random_sampling.seed = 42;
        auto random_samples = SindreMesh(mesh).sample(12, random_sampling);
        check(random_samples && random_samples.value().rows() == 12,
              "Fixed-count random sampling");
        SampleOptions farthest_sampling;
        farthest_sampling.algorithm = SampleAlgorithm::farthest_point;
        farthest_sampling.candidate_count = 32;
        auto farthest_samples = sample_mesh_surface(mesh, 8, farthest_sampling);
        check(farthest_samples && farthest_samples.value().rows() == 8,
              "Farthest-point sampling");

        Vertices query(2, 3);
        query << .1, .1, -1, 0, 0, 0;
        auto projection = project_mesh_points(mesh, query);
        check(projection && projection.value().points.rows() == 2,
              "Mesh projection");
        Vertices line(3, 3);
        line << .1, .1, 2, .5, .2, -1, .9, .8, 3;
        auto projected_line = SindreMesh(mesh).project_line(line);
        check(projected_line && projected_line.value().rows() == 3,
              "Ordered mesh line projection");

        auto path = SindreMesh(mesh).find_path(0, 3);
        check(path && path.value().vertices.front() == 0 &&
                  path.value().vertices.back() == 3 && path.value().length > 0,
              "VTK Dijkstra path");
        auto path_cache = MeshPathCache::create(mesh);
        check(static_cast<bool>(path_cache), "Path cache creation");
        auto cached_options = PathOptions{};
        cached_options.cache = std::make_shared<MeshPathCache>(path_cache.value());
        auto cached_path = find_mesh_path(mesh, 0, 3, cached_options);
        check(cached_path && cached_path.value().vertices == path.value().vertices,
              "Cached Dijkstra path");
        auto distances = calculate_signed_distances(mesh, query);
        check(distances && distances.value().size() == 2, "Signed distances");

        for (const auto curvature_type : {CurvatureType::mean,
                                          CurvatureType::gaussian,
                                          CurvatureType::minimum_principal,
                                          CurvatureType::maximum_principal}) {
            auto curvature = calculate_mesh_curvature(mesh, curvature_type);
            check(curvature && curvature.value().size() == mesh.npoints() &&
                      curvature.value().allFinite(),
                  "VTK curvature type");
        }
        auto facade_curvature = SindreMesh(mesh).get_curvature(
            CurvatureType::maximum_principal);
        check(facade_curvature && facade_curvature.value().size() == mesh.npoints(),
              "SindreMesh curvature facade");
#if defined(SINDRE_UTILS_3D_CGAL)
        for (const auto curvature_type : {CurvatureType::mean,
                                          CurvatureType::gaussian,
                                          CurvatureType::minimum_principal,
                                          CurvatureType::maximum_principal}) {
            auto cgal_curvature = get_curvature_by_cgal(mesh, curvature_type);
            check(cgal_curvature && cgal_curvature.value().size() == mesh.npoints() &&
                      cgal_curvature.value().allFinite(),
                  "CGAL curvature type");
        }
#else
        auto cgal_curvature = SindreMesh(mesh).get_curvature_by_cgal();
        check(!cgal_curvature &&
                  cgal_curvature.error().code ==
                      std::make_error_code(std::errc::function_not_supported),
              "CGAL curvature must report an unavailable backend");
#endif

        auto invalid_boolean = check_boolean_mesh(mesh, open_square.mesh(),
                                                   BooleanOperation::intersect);
        check(invalid_boolean && !invalid_boolean.value().can_execute &&
                  invalid_boolean.value().reason.find("closed") != std::string::npos,
              "Boolean preflight must reject open surfaces");
        auto member_preflight = SindreMesh(mesh).check_boolean(
            open_square, BooleanOperation::intersect);
        check(member_preflight && !member_preflight.value().can_execute,
              "SindreMesh boolean preflight");
        auto boolean_result = SindreMesh(mesh).boolean(
            open_square, BooleanOperation::intersect);
        check(!boolean_result, "Unsafe boolean input must not reach the backend");

        auto grid = SindreMesh(create_grid(10));
        Vertices curve(4, 3);
        curve << 2.0, 2.0, 0.0, 8.0, 2.0, 0.0, 8.0, 8.0, 0.0, 2.0, 8.0, 0.0;
        CurveClipOptions curve_options;
        curve_options.max_projection_distance = 0.01;
        auto clipped_inside = grid.clip_curve(curve, curve_options);
        check(clipped_inside && clipped_inside.value().nfaces() > 0 &&
                  clipped_inside.value().nfaces() < grid.nfaces(),
              "Graph-cut curve clipping must select the inside region");
        curve_options.region = CurveClipRegion::outside;
        auto clipped_outside = clip_mesh_by_curve(grid.mesh(), curve, curve_options);
        check(clipped_outside && clipped_outside.value().value.nfaces() > 0 &&
                  clipped_outside.value().value.nfaces() < grid.nfaces(),
              "Graph-cut curve clipping must select the outside region");
        Vertices open_curve(2, 3);
        open_curve << 2.0, 2.0, 0.0, 8.0, 2.0, 0.0;
        auto invalid_curve = clip_mesh_by_curve(grid.mesh(), open_curve);
        check(!invalid_curve, "Curve clipping must reject fewer than three points");

        PointCloud cloud;
        cloud.points = mesh.vertices();
        cloud.normals = Vertices::Zero(4, 3);
        cloud.normals->col(2).setOnes();
        check(static_cast<bool>(cloud.validate()), "Point cloud validation");
        cloud.labels = Labels::Zero(3);
        check(!cloud.validate(), "Point cloud attribute mismatch must fail");

        auto facade = SindreMesh(mesh).clean();
        check(facade && facade.value().nfaces() == 4, "SindreMesh facade");
        check(SindreMesh(mesh).npoint() == 4 && SindreMesh(mesh).nface() == 4,
              "SindreMesh count shortcuts");
        check(SindreMesh(mesh).get_aabb().maximum.isApprox(math::Vector3::Ones()) &&
                  SindreMesh(mesh).get_obb().half_extents.minCoeff() >= 0.0 &&
                  SindreMesh(mesh).get_min_sphere().radius > 0.0,
              "SindreMesh bounding volume shortcuts");

        auto chained = SindreMesh(mesh).clean().and_then([](const SindreMesh &value) {
            return value.smooth();
        });
        check(chained && chained.value().nfaces() == 4, "SindreMesh Result chain");

        std::cout << "utils_3d public API tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
