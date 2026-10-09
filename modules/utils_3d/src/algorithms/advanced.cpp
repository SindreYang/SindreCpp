#include <sindre/utils_3d/algorithms/feature_smoothing.h>
#include <sindre/utils_3d/algorithms/fgcf.h>
#include <sindre/utils_3d/algorithms/mesh.h>
#include <sindre/utils_3d/algorithms/segmentation.h>

#include "core/mesh.h"
#include "log.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include <vtkNew.h>
#include <vtkPolyData.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkWindowedSincPolyDataFilter.h>

#if defined(SINDRE_UTILS_3D_CGAL)
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/mesh_segmentation.h>
#endif

namespace sindre::utils_3d {
namespace {

using Vector3 = ::sindre::math::Vector3;
using LegacyMesh = detail::legacy::Mesh;

struct cancelled_error final : std::runtime_error {
    cancelled_error() : std::runtime_error("Algorithm cancelled") {}
};
struct unsupported_error final : std::runtime_error {
    explicit unsupported_error(const char *message) : std::runtime_error(message) {}
};

template <class T>
[[noreturn]] T unsupported_result(const char *message) {
    throw unsupported_error(message);
}

template <class T, class Function>
Result<T> run(Function &&function, const char *context) noexcept {
#if defined(SINDRE_NO_EXCEPTIONS)
    (void)context;
    if constexpr (std::is_void_v<T>) {
        function();
        return Result<void>::success();
    } else {
        return Result<T>::success(function());
    }
#else
    try {
        if constexpr (std::is_void_v<T>) {
            function();
            return Result<void>::success();
        } else {
            return Result<T>::success(function());
        }
    } catch (const cancelled_error &error) {
        detail::logging::warning(context, error.what());
        return Result<T>::failure(
            Error::make(std::errc::operation_canceled, error.what(), context));
    } catch (const unsupported_error &error) {
        detail::logging::warning(context, error.what());
        return Result<T>::failure(
            Error::make(std::errc::function_not_supported, error.what(), context));
    } catch (const std::invalid_argument &error) {
        detail::logging::error(context, error.what());
        return Result<T>::failure(
            Error::make(std::errc::invalid_argument, error.what(), context));
    } catch (const std::out_of_range &error) {
        detail::logging::error(context, error.what());
        return Result<T>::failure(
            Error::make(std::errc::result_out_of_range, error.what(), context));
    } catch (const std::length_error &error) {
        detail::logging::error(context, error.what());
        return Result<T>::failure(
            Error::make(std::errc::value_too_large, error.what(), context));
    } catch (const std::exception &error) {
        detail::logging::error(context, error.what());
        return Result<T>::failure(Error::make(std::errc::io_error, error.what(), context));
    } catch (...) {
        detail::logging::error(context, "Unknown advanced mesh algorithm failure");
        return Result<T>::failure(Error::make(
            std::errc::io_error, "Unknown advanced mesh algorithm failure", context));
    }
#endif
}

void check_cancel(const AlgorithmOptions &options) {
    if (options.cancellation && options.cancellation())
        throw cancelled_error();
}

void report_progress(const AlgorithmOptions &options, double value) {
    check_cancel(options);
    if (options.progress)
        options.progress(std::clamp(value, 0.0, 1.0));
}

void validate_surface(const Mesh &mesh) {
    if (mesh.npoints() < 3 || mesh.nfaces() < 1)
        throw std::invalid_argument("Mesh must contain a nonempty triangle surface");
    const auto vertices = mesh.vertices();
    const auto faces = mesh.faces();
    if (vertices.cols() != 3 || faces.cols() != 3 || !vertices.allFinite())
        throw std::invalid_argument("Mesh geometry must be finite N x 3/M x 3 data");
    for (Eigen::Index i = 0; i < faces.rows(); ++i) {
        for (int k = 0; k < 3; ++k) {
            const auto index = faces(i, k);
            if (index < 0 || index >= vertices.rows())
                throw std::out_of_range("Mesh face index is out of range");
        }
    }
    const auto quality = mesh.check();
    if (quality.degenerate_faces != 0)
        throw std::invalid_argument("Mesh contains degenerate faces");
}

AlgorithmReport report_for(const Mesh &input, const Mesh &output) {
    AlgorithmReport report;
    report.input_points = static_cast<std::size_t>(input.npoints());
    report.output_points = static_cast<std::size_t>(output.npoints());
    report.input_faces = static_cast<std::size_t>(input.nfaces());
    report.output_faces = static_cast<std::size_t>(output.nfaces());
    report.changed_elements =
        report.input_points > report.output_points ? report.input_points - report.output_points :
                                                       report.output_points - report.input_points;
    return report;
}

AlgorithmResult<Mesh> result_mesh(const Mesh &input, Mesh output) {
    auto report = report_for(input, output);
    return {std::move(output), std::move(report)};
}

LegacyMesh to_legacy(const Mesh &mesh) {
    return LegacyMesh(mesh.vertices(), mesh.faces());
}

Vector3 row_vector(const Vertices &vertices, Eigen::Index index) {
    return vertices.row(index).transpose();
}

void set_row(Vertices &vertices, Eigen::Index index, const Vector3 &value) {
    vertices.row(index) = value.transpose();
}

double segment_length(const Vector3 &a, const Vector3 &b) {
    return (b - a).norm();
}

double curve_length(const Vertices &curve, bool closed) {
    if (curve.rows() < (closed ? 3 : 2))
        throw std::invalid_argument("FGCF curve has too few points");
    double length = 0.0;
    const Eigen::Index count = curve.rows();
    for (Eigen::Index i = 1; i < count; ++i)
        length += segment_length(row_vector(curve, i - 1), row_vector(curve, i));
    if (closed)
        length += segment_length(row_vector(curve, count - 1), row_vector(curve, 0));
    if (!std::isfinite(length) || length <= std::numeric_limits<double>::epsilon())
        throw std::invalid_argument("FGCF curve must have a positive finite length");
    return length;
}

Vertices resample_curve(const Vertices &curve, bool closed) {
    const Eigen::Index count = curve.rows();
    const double total = curve_length(curve, closed);
    const Eigen::Index segment_count = closed ? count : count - 1;
    std::vector<double> cumulative(static_cast<std::size_t>(segment_count + 1), 0.0);
    for (Eigen::Index i = 0; i < segment_count; ++i) {
        const auto a = row_vector(curve, i);
        const auto b = row_vector(curve, (i + 1) % count);
        const double length = segment_length(a, b);
        if (!std::isfinite(length) || length <= std::numeric_limits<double>::epsilon())
            throw std::invalid_argument("FGCF curve contains duplicate consecutive points");
        cumulative[static_cast<std::size_t>(i + 1)] = cumulative[static_cast<std::size_t>(i)] + length;
    }

    Vertices output(count, 3);
    for (Eigen::Index target = 0; target < count; ++target) {
        const double distance = closed ? total * static_cast<double>(target) / count
                                        : total * static_cast<double>(target) / (count - 1);
        auto upper = std::upper_bound(cumulative.begin(), cumulative.end(), distance);
        std::size_t segment = upper == cumulative.begin()
                                  ? 0
                                  : static_cast<std::size_t>(upper - cumulative.begin() - 1);
        if (segment >= static_cast<std::size_t>(segment_count))
            segment = static_cast<std::size_t>(segment_count - 1);
        const double begin = cumulative[segment];
        const double length = cumulative[segment + 1] - begin;
        const double alpha = length > 0.0 ? (distance - begin) / length : 0.0;
        const auto a = row_vector(curve, static_cast<Eigen::Index>(segment));
        const auto b = row_vector(curve, (static_cast<Eigen::Index>(segment) + 1) % count);
        set_row(output, target, a + std::clamp(alpha, 0.0, 1.0) * (b - a));
    }
    if (!closed) {
        output.row(0) = curve.row(0);
        output.row(count - 1) = curve.row(count - 1);
    }
    return output;
}

struct EdgeRecord {
    std::int64_t first = 0;
    std::int64_t second = 0;
    std::vector<std::int64_t> faces;
};

std::map<std::pair<std::int64_t, std::int64_t>, EdgeRecord> collect_edges(
    const Faces &faces) {
    std::map<std::pair<std::int64_t, std::int64_t>, EdgeRecord> result;
    for (Eigen::Index face = 0; face < faces.rows(); ++face) {
        for (int edge = 0; edge < 3; ++edge) {
            const auto a = faces(face, edge);
            const auto b = faces(face, (edge + 1) % 3);
            const auto first = std::min(a, b);
            const auto second = std::max(a, b);
            auto &record = result[{first, second}];
            record.first = first;
            record.second = second;
            record.faces.push_back(static_cast<std::int64_t>(face));
        }
    }
    return result;
}

Vector3 closest_point_on_segment(const Vector3 &point, const Vector3 &a, const Vector3 &b) {
    const Vector3 direction = b - a;
    const double squared_length = direction.squaredNorm();
    if (!std::isfinite(squared_length) || squared_length <= std::numeric_limits<double>::epsilon())
        return a;
    const double alpha = std::clamp((point - a).dot(direction) / squared_length, 0.0, 1.0);
    return a + alpha * direction;
}

} // namespace

Result<AlgorithmResult<Mesh>> smooth_mesh_features(
    const Mesh &mesh, const FeatureSmoothingOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        validate_surface(mesh);
        if (options.iterations < 1 || !std::isfinite(options.feature_angle) ||
            options.feature_angle < 0.0 || options.feature_angle > 180.0 ||
            !std::isfinite(options.edge_angle) || options.edge_angle < 0.0 ||
            options.edge_angle > 180.0 || !std::isfinite(options.pass_band) ||
            options.pass_band < 0.0 || options.pass_band > 2.0 ||
            !std::isfinite(options.relaxation) || options.relaxation <= 0.0 ||
            options.relaxation > 1.0 || !std::isfinite(options.snap_strength) ||
            options.snap_strength < 0.0 || options.snap_strength > 1.0 ||
            !std::isfinite(options.max_snap_distance) || options.max_snap_distance < 0.0)
            throw std::invalid_argument("Invalid feature smoothing options");
        check_cancel(options);
        if (options.max_points != 0 &&
            static_cast<std::size_t>(mesh.npoints()) > options.max_points)
            throw std::length_error("Mesh exceeds the configured point limit");

        const auto vertices = mesh.vertices();
        const auto faces = mesh.faces();
        const auto edges = collect_edges(faces);
        const auto face_normals = mesh.face_normals();
        std::vector<EdgeRecord> preserved_edges;
        std::vector<std::vector<std::pair<std::int64_t, std::int64_t>>> incident(
            static_cast<std::size_t>(mesh.npoints()));
        std::set<std::int64_t> feature_vertices;

        for (const auto &entry : edges) {
            const auto &edge = entry.second;
            bool preserve = false;
            if (edge.faces.size() == 1) {
                preserve = options.preserve_boundary;
            } else if (edge.faces.size() == 2) {
                const auto left = face_normals.row(edge.faces[0]).transpose();
                const auto right = face_normals.row(edge.faces[1]).transpose();
                const double cosine = std::clamp(left.dot(right), -1.0, 1.0);
                const double angle = std::acos(cosine) * 180.0 / 3.14159265358979323846;
                preserve = options.preserve_features && angle >= options.feature_angle;
            } else {
                preserve = options.preserve_non_manifold;
            }
            if (!preserve)
                continue;
            preserved_edges.push_back(edge);
            incident[static_cast<std::size_t>(edge.first)].push_back({edge.first, edge.second});
            incident[static_cast<std::size_t>(edge.second)].push_back({edge.first, edge.second});
            feature_vertices.insert(edge.first);
            feature_vertices.insert(edge.second);
        }

        LegacyMesh input = to_legacy(mesh);
        vtkSmartPointer<vtkPolyData> output_data;
        if (options.method == FeatureSmoothingMethod::windowed_sinc) {
            vtkNew<vtkWindowedSincPolyDataFilter> filter;
            filter->SetInputData(input.get_native());
            filter->SetNumberOfIterations(options.iterations);
            filter->SetPassBand(options.pass_band);
            filter->SetFeatureAngle(options.feature_angle);
            filter->SetEdgeAngle(options.edge_angle);
            filter->SetFeatureEdgeSmoothing(options.preserve_features ? 0 : 1);
            filter->SetBoundarySmoothing(options.preserve_boundary ? 0 : 1);
            filter->SetNonManifoldSmoothing(options.preserve_non_manifold ? 0 : 1);
            filter->SetNormalizeCoordinates(options.normalize_coordinates ? 1 : 0);
            filter->Update();
            output_data = filter->GetOutput();
        } else if (options.method == FeatureSmoothingMethod::laplacian) {
            vtkNew<vtkSmoothPolyDataFilter> filter;
            filter->SetInputData(input.get_native());
            filter->SetNumberOfIterations(options.iterations);
            filter->SetRelaxationFactor(options.relaxation);
            filter->SetFeatureAngle(options.feature_angle);
            filter->SetEdgeAngle(options.edge_angle);
            filter->SetFeatureEdgeSmoothing(options.preserve_features ? 0 : 1);
            filter->SetBoundarySmoothing(options.preserve_boundary ? 0 : 1);
            filter->Update();
            output_data = filter->GetOutput();
        } else {
            throw std::invalid_argument("Unknown feature smoothing method");
        }

        if (!output_data || output_data->GetNumberOfPoints() != mesh.npoints() ||
            output_data->GetNumberOfPolys() != mesh.nfaces())
            throw std::runtime_error("Feature smoothing changed mesh topology");

        Vertices output(mesh.npoints(), 3);
        for (Eigen::Index i = 0; i < output.rows(); ++i) {
            const auto *point = output_data->GetPoint(static_cast<vtkIdType>(i));
            if (!point || !std::isfinite(point[0]) || !std::isfinite(point[1]) ||
                !std::isfinite(point[2]))
                throw std::runtime_error("Feature smoothing returned a nonfinite point");
            output.row(i) << point[0], point[1], point[2];
        }

        if (options.snap_to_features && options.snap_strength > 0.0 &&
            !preserved_edges.empty()) {
            for (const auto index : feature_vertices) {
                check_cancel(options);
                const auto &neighbors = incident[static_cast<std::size_t>(index)];
                if (neighbors.size() != 2) {
                    output.row(index) = vertices.row(index);
                    continue;
                }
                const Vector3 current = row_vector(output, index);
                double minimum_edge = std::numeric_limits<double>::infinity();
                Vector3 closest = current;
                double closest_distance = std::numeric_limits<double>::infinity();
                for (const auto &neighbor : neighbors) {
                    const Vector3 a = row_vector(vertices, neighbor.first);
                    const Vector3 b = row_vector(vertices, neighbor.second);
                    minimum_edge = std::min(minimum_edge, segment_length(a, b));
                    const Vector3 candidate = closest_point_on_segment(current, a, b);
                    const double distance = (candidate - current).norm();
                    if (distance < closest_distance) {
                        closest = candidate;
                        closest_distance = distance;
                    }
                }
                const double maximum = options.max_snap_distance > 0.0
                                           ? options.max_snap_distance
                                           : 0.5 * minimum_edge;
                if (std::isfinite(maximum) && closest_distance <= maximum)
                    set_row(output, index,
                            current + options.snap_strength * (closest - current));
            }
        }
        if (!output.allFinite())
            throw std::runtime_error("Feature smoothing produced nonfinite geometry");
        Mesh result(std::move(output), faces);
        report_progress(options, 1.0);
        auto report = result_mesh(mesh, std::move(result));
        report.report.iterations = static_cast<std::size_t>(options.iterations);
        report.report.changed_elements = 0;
        for (Eigen::Index i = 0; i < mesh.npoints(); ++i) {
            if ((report.value.vertices().row(i) - vertices.row(i)).norm() > 1e-12)
                ++report.report.changed_elements;
        }
        return report;
    }, "utils_3d.smooth_mesh_features");
}

Result<AlgorithmResult<FgcfResult>> smooth_curve_by_fgcf(
    const Mesh &mesh, const Vertices &input_curve, const FgcfOptions &options) {
    return run<AlgorithmResult<FgcfResult>>([&] {
        validate_surface(mesh);
        const Eigen::Index minimum_points = options.closed ? 3 : 2;
        if (input_curve.cols() != 3 || input_curve.rows() < minimum_points ||
            !input_curve.allFinite())
            throw std::invalid_argument("FGCF curve must be finite N x 3 data");
        if (!std::isfinite(options.time_step) || options.time_step <= 0.0 ||
            options.time_step > 1.0 || !std::isfinite(options.max_step_fraction) ||
            options.max_step_fraction <= 0.0 || options.max_step_fraction > 1.0 ||
            !std::isfinite(options.convergence_tolerance) ||
            options.convergence_tolerance < 0.0)
            throw std::invalid_argument("Invalid FGCF options");
        if (options.max_points != 0 &&
            static_cast<std::size_t>(input_curve.rows()) > options.max_points)
            throw std::length_error("FGCF curve exceeds the configured point limit");

        const double initial_length = curve_length(input_curve, options.closed);
        auto projected = project_mesh_points(mesh, input_curve, options);
        if (!projected)
            throw std::runtime_error(projected.error().describe());
        Vertices current = std::move(projected.value().points);
        if (options.resample && options.preserve_point_count)
            current = resample_curve(current, options.closed);
        auto projected_again = project_mesh_points(mesh, current, options);
        if (!projected_again)
            throw std::runtime_error(projected_again.error().describe());
        current = std::move(projected_again.value().points);
        const auto normals = mesh.face_normals();
        const Eigen::Index count = current.rows();
        FgcfResult value;
        value.curve = current;
        value.initial_length = initial_length;
        value.iterations = 0;
        value.converged = options.iterations == 0;

        for (unsigned iteration = 0; iteration < options.iterations; ++iteration) {
            check_cancel(options);
            const auto projection = project_mesh_points(mesh, current, options);
            if (!projection)
                throw std::runtime_error(projection.error().describe());
            current = projection.value().points;
            const auto &face_ids = projection.value().face_ids;
            Vertices candidates = current;
            double maximum_displacement = 0.0;

            const Eigen::Index begin = options.closed ? 0 : 1;
            const Eigen::Index end = options.closed ? count : count - 1;
            for (Eigen::Index i = begin; i < end; ++i) {
                const Eigen::Index previous = i == 0 ? count - 1 : i - 1;
                const Eigen::Index next = i + 1 == count ? 0 : i + 1;
                const Vector3 point = row_vector(current, i);
                const Vector3 previous_point = row_vector(current, previous);
                const Vector3 next_point = row_vector(current, next);
                const double previous_length = segment_length(point, previous_point);
                const double next_length = segment_length(point, next_point);
                if (previous_length <= std::numeric_limits<double>::epsilon() ||
                    next_length <= std::numeric_limits<double>::epsilon())
                    continue;

                const auto face_id = face_ids(i);
                if (face_id < 0 || face_id >= normals.rows())
                    throw std::runtime_error("FGCF projection returned an invalid face id");
                Vector3 surface_normal = normals.row(face_id).transpose();
                const double normal_length = surface_normal.norm();
                if (!std::isfinite(normal_length) || normal_length <= 1e-15)
                    continue;
                surface_normal /= normal_length;
                Vector3 tangent = next_point - previous_point;
                const double tangent_length = tangent.norm();
                if (tangent_length <= 1e-15)
                    continue;
                tangent /= tangent_length;

                Vector3 curvature = previous_point + next_point - 2.0 * point;
                curvature -= surface_normal * curvature.dot(surface_normal);
                curvature -= tangent * curvature.dot(tangent);
                curvature /= std::max(0.5 * (previous_length + next_length), 1e-15);
                Vector3 displacement = options.time_step * curvature;
                const double maximum_step = options.max_step_fraction *
                                             std::min(previous_length, next_length);
                const double displacement_length = displacement.norm();
                if (displacement_length > maximum_step && displacement_length > 0.0)
                    displacement *= maximum_step / displacement_length;
                set_row(candidates, i, point + displacement);
                maximum_displacement = std::max(maximum_displacement, displacement.norm());
            }

            auto next_projection = project_mesh_points(mesh, candidates, options);
            if (!next_projection)
                throw std::runtime_error(next_projection.error().describe());
            current = std::move(next_projection.value().points);
            if (options.resample && options.preserve_point_count) {
                current = resample_curve(current, options.closed);
                auto resampled_projection = project_mesh_points(mesh, current, options);
                if (!resampled_projection)
                    throw std::runtime_error(resampled_projection.error().describe());
                current = std::move(resampled_projection.value().points);
            }

            value.iterations = iteration + 1;
            value.curve = current;
            const double scale = curve_length(current, options.closed) / static_cast<double>(count);
            value.converged = maximum_displacement <=
                              options.convergence_tolerance * std::max(scale, 1e-15);
            report_progress(options, static_cast<double>(iteration + 1) /
                                         std::max(options.iterations, 1u));
            if (value.converged)
                break;
        }

        value.final_length = curve_length(value.curve, options.closed);
        AlgorithmReport report;
        report.input_points = static_cast<std::size_t>(input_curve.rows());
        report.output_points = static_cast<std::size_t>(value.curve.rows());
        report.iterations = value.iterations;
        report.residual = std::abs(value.final_length - value.initial_length);
        report.converged = value.converged;
        report.changed_elements = report.output_points;
        report_progress(options, 1.0);
        return AlgorithmResult<FgcfResult>{std::move(value), std::move(report)};
    }, "utils_3d.smooth_curve_by_fgcf");
}

Result<AlgorithmResult<CgalSegmentationResult>> segment_mesh_by_cgal(
    const Mesh &mesh, const CgalSegmentationOptions &options) {
    return run<AlgorithmResult<CgalSegmentationResult>>([&] {
#if !defined(SINDRE_UTILS_3D_CGAL)
        (void)mesh;
        (void)options;
        return unsupported_result<AlgorithmResult<CgalSegmentationResult>>(
            "CGAL segmentation was requested but the CGAL backend is not enabled");
#else
        validate_surface(mesh);
        if (options.number_of_rays == 0 || options.number_of_clusters == 0 ||
            !std::isfinite(options.cone_angle) || options.cone_angle <= 0.0 ||
            options.cone_angle >= 3.14159265358979323846 ||
            !std::isfinite(options.smoothing_lambda) || options.smoothing_lambda < 0.0 ||
            options.smoothing_lambda > 1.0)
            throw std::invalid_argument("Invalid CGAL segmentation options");
        if (static_cast<std::size_t>(mesh.nfaces()) < options.number_of_clusters)
            throw std::invalid_argument(
                "CGAL segmentation cluster count cannot exceed face count");
        if (options.require_closed && !mesh.is_watertight())
            throw std::invalid_argument(
                "CGAL SDF segmentation requires a closed, manifold triangle mesh");
        if (options.max_points != 0 &&
            static_cast<std::size_t>(mesh.npoints()) > options.max_points)
            throw std::length_error("Mesh exceeds the configured point limit");

        using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
        using CgalMesh = CGAL::Surface_mesh<Kernel::Point_3>;
        CgalMesh native;
        std::vector<CgalMesh::Vertex_index> vertex_ids;
        vertex_ids.reserve(static_cast<std::size_t>(mesh.npoints()));
        const auto vertices = mesh.vertices();
        for (Eigen::Index i = 0; i < vertices.rows(); ++i)
            vertex_ids.push_back(native.add_vertex(Kernel::Point_3(
                vertices(i, 0), vertices(i, 1), vertices(i, 2))));

        const auto faces = mesh.faces();
        std::vector<CgalMesh::Face_index> face_ids;
        face_ids.reserve(static_cast<std::size_t>(faces.rows()));
        for (Eigen::Index i = 0; i < faces.rows(); ++i) {
            const auto face = native.add_face(
                vertex_ids[static_cast<std::size_t>(faces(i, 0))],
                vertex_ids[static_cast<std::size_t>(faces(i, 1))],
                vertex_ids[static_cast<std::size_t>(faces(i, 2))]);
            if (face == CgalMesh::null_face())
                throw std::invalid_argument("CGAL rejected the mesh face orientation");
            face_ids.push_back(face);
        }
        if (!native.is_valid())
            throw std::invalid_argument("CGAL rejected the mesh as an invalid surface mesh");
        if (CGAL::Polygon_mesh_processing::does_self_intersect(native))
            throw std::invalid_argument("CGAL SDF segmentation requires an intersection-free mesh");

        auto sdf = native.add_property_map<CgalMesh::Face_index, double>(
            "sindre_sdf", 0.0).first;
        auto labels = native.add_property_map<CgalMesh::Face_index, std::size_t>(
            "sindre_segment", 0).first;
        const auto sdf_range = CGAL::sdf_values(
            native, sdf, options.cone_angle, options.number_of_rays, true);
        const std::size_t segment_count = CGAL::segmentation_from_sdf_values(
            native, sdf, labels, options.number_of_clusters, options.smoothing_lambda,
            options.output_cluster_ids);

        CgalSegmentationResult value;
        value.face_labels.resize(faces.rows());
        for (Eigen::Index i = 0; i < faces.rows(); ++i)
            value.face_labels(i) = static_cast<std::int64_t>(get(labels, face_ids[static_cast<std::size_t>(i)]));
        value.segment_count = segment_count;
        value.sdf_minimum = sdf_range.first;
        value.sdf_maximum = sdf_range.second;
        AlgorithmReport report;
        report.input_faces = static_cast<std::size_t>(mesh.nfaces());
        report.output_faces = report.input_faces;
        report.changed_elements = report.output_faces;
        report.converged = true;
        report_progress(options, 1.0);
        return AlgorithmResult<CgalSegmentationResult>{std::move(value), std::move(report)};
#endif
    }, "utils_3d.segment_mesh_by_cgal");
}

} // namespace sindre::utils_3d
