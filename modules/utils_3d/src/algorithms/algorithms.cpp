#include <sindre/utils_3d/algorithms/mesh.h>
#include <sindre/utils_3d/algorithms/point_cloud.h>

#include "core/mesh.h"
#include "log.h"

#include <Eigen/LU>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <vtkAppendPolyData.h>
#include <vtkAdaptiveSubdivisionFilter.h>
#include <vtkCellArray.h>
#include <vtkBox.h>
#include <vtkClipPolyData.h>
#include <vtkCleanPolyData.h>
#include <vtkCutter.h>
#include <vtkDecimatePro.h>
#include <vtkDijkstraGraphGeodesicPath.h>
#include <vtkFillHolesFilter.h>
#include <vtkIdList.h>
#include <vtkImplicitPolyDataDistance.h>
#include <vtkLoopSubdivisionFilter.h>
#include <vtkNew.h>
#include <vtkPlane.h>
#include <vtkPolygon.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkReverseSense.h>
#include <vtkSelectPolyData.h>
#include <vtkSphere.h>
#include <vtkStaticCellLocator.h>
#include <vtkStaticCleanPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkStripper.h>
#include <vtkQuadricClustering.h>
#include <vtkQuadricDecimation.h>
#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkVersionMacros.h>

#if defined(SINDRE_UTILS_3D_CGAL)
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#if defined(SINDRE_UTILS_3D_HAS_CGAL_CURVATURE)
#include <CGAL/Polygon_mesh_processing/interpolated_corrected_curvatures.h>
#endif
#include <CGAL/Polygon_mesh_processing/remesh.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>
#include <CGAL/Polygon_mesh_processing/border.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Face_count_stop_predicate.h>
#include <CGAL/Surface_mesh_simplification/edge_collapse.h>
#endif

#if defined(SINDRE_UTILS_3D_PCL)
#include <pcl/PolygonMesh.h>
#include <pcl/conversions.h>
#include <pcl/features/normal_3d.h>
#include <pcl/filters/radius_outlier_removal.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/registration/icp.h>
#include <pcl/search/kdtree.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/surface/poisson.h>
#endif

namespace sindre::utils_3d {

class MeshPathCache::Impl {
  public:
    Vertices vertices;
    std::vector<std::vector<std::pair<std::int64_t, double>>> adjacency;
    std::uint64_t fingerprint = 0;
};

namespace {

using LegacyMesh = detail::legacy::Mesh;

struct cancelled_error final : std::runtime_error {
    cancelled_error() : std::runtime_error("Algorithm cancelled") {}
};
struct limit_error final : std::length_error {
    explicit limit_error(const char *message) : std::length_error(message) {}
};
struct unsupported_error final : std::runtime_error {
    explicit unsupported_error(const char *message) : std::runtime_error(message) {}
};

template <class T>
T unsupported_value(const char *message) {
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
        return Result<T>::failure(Error::make(std::errc::operation_canceled, error.what(), context));
    } catch (const limit_error &error) {
        detail::logging::warning(context, error.what());
        return Result<T>::failure(Error::make(std::errc::value_too_large, error.what(), context));
    } catch (const unsupported_error &error) {
        detail::logging::warning(context, error.what());
        return Result<T>::failure(
            Error::make(std::errc::function_not_supported, error.what(), context));
    } catch (const std::invalid_argument &error) {
        detail::logging::error(context, error.what());
        return Result<T>::failure(Error::make(std::errc::invalid_argument, error.what(), context));
    } catch (const std::out_of_range &error) {
        detail::logging::error(context, error.what());
        return Result<T>::failure(Error::make(std::errc::result_out_of_range, error.what(), context));
    } catch (const std::overflow_error &error) {
        detail::logging::error(context, error.what());
        return Result<T>::failure(Error::make(std::errc::value_too_large, error.what(), context));
    } catch (const std::length_error &error) {
        detail::logging::error(context, error.what());
        return Result<T>::failure(Error::make(std::errc::value_too_large, error.what(), context));
    } catch (const std::exception &error) {
        detail::logging::error(context, error.what());
        return Result<T>::failure(Error::make(std::errc::io_error, error.what(), context));
    } catch (...) {
        detail::logging::error(context, "Unknown algorithm failure");
        return Result<T>::failure(Error::make(std::errc::io_error, "Unknown algorithm failure", context));
    }
#endif
}

void check_cancel(const AlgorithmOptions &options) {
    if (options.cancellation && options.cancellation())
        throw cancelled_error();
}

void check_limit(const AlgorithmOptions &options, std::size_t count) {
    if (options.max_points && count > options.max_points)
        throw limit_error("Algorithm point limit exceeded");
}

void progress(const AlgorithmOptions &options, double value) {
    check_cancel(options);
    if (options.progress)
        options.progress(std::clamp(value, 0.0, 1.0));
}

void require_surface(const Mesh &mesh) {
    if (mesh.npoints() == 0 || mesh.nfaces() == 0)
        throw std::invalid_argument("Operation requires a nonempty triangle surface");
}

void positive(double value, const char *name) {
    if (!std::isfinite(value) || value <= 0)
        throw std::invalid_argument(std::string(name) + " must be finite and positive");
}

// The public API deliberately does not expose a graph-cut dependency.  This
// small Dinic implementation is used for the binary moves of alpha-expansion
// and alpha-beta swap.  Capacities are doubles because the unary terms are
// derived from probabilities and the geometric pairwise terms are distances.
class BinaryCutGraph {
    struct Edge {
        int to;
        int reverse;
        double capacity;
    };

    std::vector<std::vector<Edge>> graph_;
    std::vector<int> level_;
    std::vector<std::size_t> next_;

    bool build_level(int source, int sink) {
        std::fill(level_.begin(), level_.end(), -1);
        std::queue<int> queue;
        level_[source] = 0;
        queue.push(source);
        while (!queue.empty()) {
            const int node = queue.front();
            queue.pop();
            for (const auto &edge : graph_[static_cast<std::size_t>(node)]) {
                if (edge.capacity > 1e-12 && level_[static_cast<std::size_t>(edge.to)] < 0) {
                    level_[static_cast<std::size_t>(edge.to)] =
                        level_[static_cast<std::size_t>(node)] + 1;
                    queue.push(edge.to);
                }
            }
        }
        return level_[sink] >= 0;
    }

    double send_flow(int node, int sink, double flow) {
        if (node == sink)
            return flow;
        auto &edges = graph_[static_cast<std::size_t>(node)];
        for (std::size_t &index = next_[static_cast<std::size_t>(node)];
             index < edges.size(); ++index) {
            auto &edge = edges[index];
            if (edge.capacity <= 1e-12 ||
                level_[static_cast<std::size_t>(edge.to)] !=
                    level_[static_cast<std::size_t>(node)] + 1)
                continue;
            const double pushed = send_flow(edge.to, sink, std::min(flow, edge.capacity));
            if (pushed <= 1e-12)
                continue;
            edge.capacity -= pushed;
            graph_[static_cast<std::size_t>(edge.to)][static_cast<std::size_t>(edge.reverse)]
                .capacity += pushed;
            return pushed;
        }
        return 0.0;
    }

  public:
    explicit BinaryCutGraph(std::size_t node_count)
        : graph_(node_count), level_(node_count, -1), next_(node_count, 0) {}

    void add_edge(int from, int to, double capacity) {
        if (!std::isfinite(capacity) || capacity < 0.0)
            throw std::invalid_argument("Graph-cut capacity must be finite and nonnegative");
        auto &from_edges = graph_[static_cast<std::size_t>(from)];
        auto &to_edges = graph_[static_cast<std::size_t>(to)];
        from_edges.push_back({to, static_cast<int>(to_edges.size()), capacity});
        to_edges.push_back({from, static_cast<int>(from_edges.size() - 1), 0.0});
    }

    void add_undirected_edge(int first, int second, double capacity) {
        add_edge(first, second, capacity);
        add_edge(second, first, capacity);
    }

    double max_flow(int source, int sink) {
        double result = 0.0;
        while (build_level(source, sink)) {
            std::fill(next_.begin(), next_.end(), 0);
            while (const double pushed = send_flow(source, sink,
                                                   std::numeric_limits<double>::infinity())) {
                result += pushed;
            }
        }
        return result;
    }

    [[nodiscard]] std::vector<bool> source_side(int source) const {
        std::vector<bool> visited(graph_.size(), false);
        std::queue<int> queue;
        visited[static_cast<std::size_t>(source)] = true;
        queue.push(source);
        while (!queue.empty()) {
            const int node = queue.front();
            queue.pop();
            for (const auto &edge : graph_[static_cast<std::size_t>(node)]) {
                if (edge.capacity > 1e-12 && !visited[static_cast<std::size_t>(edge.to)]) {
                    visited[static_cast<std::size_t>(edge.to)] = true;
                    queue.push(edge.to);
                }
            }
        }
        return visited;
    }
};

struct GraphCutEdge {
    std::size_t first = 0;
    std::size_t second = 0;
    double weight = 0.0;
};

void add_binary_pairwise(BinaryCutGraph &graph, int first, int second,
                         double e00, double e01, double e10, double e11,
                         std::vector<double> &unary_one) {
    const double pair_capacity = 0.5 * (e01 + e10 - e00 - e11);
    if (!std::isfinite(pair_capacity) || pair_capacity < -1e-9)
        throw std::invalid_argument("Graph-cut pairwise term is not submodular");
    const double q = std::max(0.0, pair_capacity);
    unary_one[static_cast<std::size_t>(first)] += e10 - e00 - q;
    unary_one[static_cast<std::size_t>(second)] += e01 - e00 - q;
    if (q > 0.0)
        graph.add_undirected_edge(first, second, q);
}

std::vector<bool> solve_binary_move(const std::vector<double> &unary_zero,
                                    const std::vector<double> &unary_one,
                                    const std::function<void(BinaryCutGraph &,
                                                              std::vector<double> &,
                                                              std::vector<double> &)> &pairs) {
    const std::size_t count = unary_zero.size();
    if (unary_one.size() != count)
        throw std::invalid_argument("Graph-cut unary terms have inconsistent sizes");
    const int source = static_cast<int>(count);
    const int sink = source + 1;
    BinaryCutGraph graph(count + 2);
    std::vector<double> zero = unary_zero;
    std::vector<double> one = unary_one;
    pairs(graph, zero, one);
    for (std::size_t i = 0; i < count; ++i) {
        if (!std::isfinite(zero[i]) || !std::isfinite(one[i]))
            throw std::invalid_argument("Graph-cut unary term is not finite");
        const double offset = std::min(zero[i], one[i]);
        zero[i] -= offset;
        one[i] -= offset;
        if (zero[i] < -1e-8 || one[i] < -1e-8)
            throw std::invalid_argument("Graph-cut unary term is invalid");
        if (one[i] > 0.0)
            graph.add_edge(source, static_cast<int>(i), one[i]);
        if (zero[i] > 0.0)
            graph.add_edge(static_cast<int>(i), sink, zero[i]);
    }
    (void)graph.max_flow(source, sink);
    const auto reachable = graph.source_side(source);
    std::vector<bool> result(count, false);
    for (std::size_t i = 0; i < count; ++i)
        result[i] = reachable[i];
    return result;
}

std::vector<bool> alpha_expansion_move(const std::vector<std::size_t> &labels,
                                       std::size_t alpha,
                                       const std::vector<double> &unary,
                                       std::size_t class_count,
                                       const std::vector<GraphCutEdge> &edges) {
    const std::size_t count = labels.size();
    std::vector<double> zero(count), one(count);
    for (std::size_t i = 0; i < count; ++i) {
        zero[i] = unary[i * class_count + labels[i]];
        one[i] = unary[i * class_count + alpha];
    }
    return solve_binary_move(zero, one, [&](BinaryCutGraph &graph,
                                             std::vector<double> &move_zero,
                                             std::vector<double> &move_one) {
        (void)move_zero;
        for (const auto &edge : edges) {
            const auto i = edge.first;
            const auto j = edge.second;
            add_binary_pairwise(
                graph, static_cast<int>(i), static_cast<int>(j),
                edge.weight * (labels[i] != labels[j]),
                edge.weight * (labels[i] != alpha),
                edge.weight * (alpha != labels[j]), 0.0,
                move_one);
        }
    });
}

std::vector<bool> alpha_beta_swap_move(const std::vector<std::size_t> &labels,
                                       std::size_t alpha, std::size_t beta,
                                       const std::vector<double> &unary,
                                       std::size_t class_count,
                                       const std::vector<GraphCutEdge> &edges,
                                       std::vector<std::size_t> &active_nodes) {
    active_nodes.clear();
    std::vector<int> active_index(labels.size(), -1);
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (labels[i] == alpha || labels[i] == beta) {
            active_index[i] = static_cast<int>(active_nodes.size());
            active_nodes.push_back(i);
        }
    }
    if (active_nodes.empty())
        return {};
    std::vector<double> zero(active_nodes.size()), one(active_nodes.size());
    for (std::size_t local = 0; local < active_nodes.size(); ++local) {
        const auto i = active_nodes[local];
        zero[local] = unary[i * class_count + alpha];
        one[local] = unary[i * class_count + beta];
    }
    return solve_binary_move(zero, one, [&](BinaryCutGraph &graph,
                                             std::vector<double> &move_zero,
                                             std::vector<double> &move_one) {
        for (const auto &edge : edges) {
            const int local_i = active_index[edge.first];
            const int local_j = active_index[edge.second];
            if (local_i >= 0 && local_j >= 0) {
                add_binary_pairwise(graph, local_i, local_j, 0.0, edge.weight,
                                    edge.weight, 0.0, move_one);
            } else if (local_i >= 0 || local_j >= 0) {
                const auto active = local_i >= 0 ? edge.first : edge.second;
                const auto fixed = local_i >= 0 ? edge.second : edge.first;
                const auto local = static_cast<std::size_t>(
                    local_i >= 0 ? local_i : local_j);
                move_zero[local] += edge.weight * (alpha != labels[fixed]);
                move_one[local] += edge.weight * (beta != labels[fixed]);
                (void)active;
            }
        }
    });
}

double graph_cut_energy(const std::vector<std::size_t> &labels,
                        const std::vector<double> &unary, std::size_t class_count,
                        const std::vector<GraphCutEdge> &edges) {
    double result = 0.0;
    for (std::size_t i = 0; i < labels.size(); ++i)
        result += unary[i * class_count + labels[i]];
    for (const auto &edge : edges)
        result += edge.weight * (labels[edge.first] != labels[edge.second]);
    return result;
}

std::size_t unique_label_count(const std::vector<std::size_t> &labels) {
    std::set<std::size_t> unique(labels.begin(), labels.end());
    return unique.size();
}

double median_value(std::vector<double> values) {
    if (values.empty())
        return 0.0;
    const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
    std::nth_element(values.begin(), middle, values.end());
    if (values.size() % 2 != 0)
        return *middle;
    const auto lower = std::max_element(values.begin(), middle);
    return (*lower + *middle) * 0.5;
}

LegacyMesh to_legacy(const Mesh &mesh) {
    return LegacyMesh(mesh.vertices(), mesh.faces());
}

Mesh from_legacy(const LegacyMesh &mesh) {
    Mesh result(mesh.vertices(), mesh.faces());
    for (const bool point : {true, false}) {
        for (const auto &name : mesh.data_names(point)) {
            if (name == "Labels")
                continue;
            try {
                const auto data = mesh.get_data(name, point);
                (void)result.set_data(name, data, point);
            } catch (...) {
            }
        }
        try {
            if (mesh.has_data("Labels", point))
                (void)result.set_labels(mesh.get_labels(point), point);
        } catch (...) {
        }
    }
    return result;
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

bool boolean_geometry_ready(const Mesh &mesh, MeshCheckReport &quality,
                            std::string &reason) {
    if (mesh.npoints() == 0 || mesh.nfaces() == 0) {
        reason = "Boolean input must contain a nonempty triangle surface";
        return false;
    }
    const auto vertices = mesh.vertices();
    const auto faces = mesh.faces();
    if (vertices.cols() != 3 || faces.cols() != 3 || !vertices.allFinite()) {
        reason = "Boolean input geometry must be finite N x 3/M x 3 data";
        return false;
    }
    for (Eigen::Index i = 0; i < faces.rows(); ++i)
        for (int k = 0; k < 3; ++k)
            if (faces(i, k) < 0 || faces(i, k) >= vertices.rows()) {
                reason = "Boolean input contains an out-of-range face index";
                return false;
            }
    quality = mesh.check();
    if (quality.degenerate_faces != 0) {
        reason = "Boolean input contains degenerate faces";
        return false;
    }
    if (quality.duplicate_vertices != 0) {
        reason = "Boolean input contains duplicate vertices; clean it first";
        return false;
    }
    if (quality.unused_vertices != 0) {
        reason = "Boolean input contains unused vertices; clean it first";
        return false;
    }
    if (quality.non_manifold_edges != 0) {
        reason = "Boolean input contains non-manifold edges";
        return false;
    }
    if (quality.boundary_edges != 0 || !quality.edge_closed) {
        reason = "Boolean input must be a closed watertight surface";
        return false;
    }
    return true;
}

bool mesh_bounds_overlap(const Mesh &left, const Mesh &right) {
    const auto left_bounds = left.bounds();
    const auto right_bounds = right.bounds();
    for (int axis = 0; axis < 3; ++axis)
        if (left_bounds(1, axis) < right_bounds(0, axis) ||
            right_bounds(1, axis) < left_bounds(0, axis))
            return false;
    return true;
}

Mesh concatenate_meshes(const Mesh &left, const Mesh &right) {
    const auto left_vertices = left.vertices();
    const auto right_vertices = right.vertices();
    const auto left_faces = left.faces();
    const auto right_faces = right.faces();
    const auto point_count = static_cast<std::size_t>(left_vertices.rows()) +
                             static_cast<std::size_t>(right_vertices.rows());
    const auto face_count = static_cast<std::size_t>(left_faces.rows()) +
                            static_cast<std::size_t>(right_faces.rows());
    if (point_count > static_cast<std::size_t>(std::numeric_limits<Index>::max()) ||
        face_count > static_cast<std::size_t>(std::numeric_limits<Index>::max()))
        throw std::length_error("Boolean result exceeds the Eigen index range");

    Vertices vertices(static_cast<Index>(point_count), 3);
    vertices.topRows(left_vertices.rows()) = left_vertices;
    vertices.bottomRows(right_vertices.rows()) = right_vertices;
    Faces faces(static_cast<Index>(face_count), 3);
    faces.topRows(left_faces.rows()) = left_faces;
    const auto point_offset = static_cast<std::int64_t>(left_vertices.rows());
    for (Eigen::Index i = 0; i < right_faces.rows(); ++i)
        for (int k = 0; k < 3; ++k)
            faces(left_faces.rows() + i, k) = right_faces(i, k) + point_offset;
    return Mesh(vertices, faces);
}

std::uint64_t mesh_fingerprint(const Mesh &mesh) {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto mix = [&hash](std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    const auto vertices = mesh.vertices();
    const auto faces = mesh.faces();
    mix(static_cast<std::uint64_t>(vertices.rows()));
    mix(static_cast<std::uint64_t>(faces.rows()));
    for (Eigen::Index i = 0; i < vertices.rows(); ++i)
        for (int k = 0; k < 3; ++k) {
            std::uint64_t bits = 0;
            static_assert(sizeof(bits) == sizeof(vertices(i, k)));
            std::memcpy(&bits, &vertices(i, k), sizeof(bits));
            mix(bits);
        }
    for (Eigen::Index i = 0; i < faces.rows(); ++i)
        for (int k = 0; k < 3; ++k)
            mix(static_cast<std::uint64_t>(faces(i, k)));
    return hash;
}

Mesh compact_unused_vertices(const Mesh &mesh) {
    const auto vertices = mesh.vertices();
    const auto faces = mesh.faces();
    std::vector<bool> used(static_cast<std::size_t>(vertices.rows()), false);
    for (Eigen::Index i = 0; i < faces.rows(); ++i)
        for (int k = 0; k < 3; ++k) {
            const auto id = faces(i, k);
            if (id < 0 || id >= vertices.rows())
                throw std::out_of_range("Mesh face index is outside the vertex range");
            used[static_cast<std::size_t>(id)] = true;
        }

    std::vector<std::int64_t> remap(static_cast<std::size_t>(vertices.rows()), -1);
    std::size_t count = 0;
    for (std::size_t i = 0; i < used.size(); ++i)
        if (used[i])
            remap[i] = static_cast<std::int64_t>(count++);
    Vertices compacted(static_cast<Index>(count), 3);
    for (std::size_t i = 0; i < used.size(); ++i)
        if (used[i])
            compacted.row(remap[i]) = vertices.row(static_cast<Index>(i));
    Faces remapped(faces.rows(), 3);
    for (Eigen::Index i = 0; i < faces.rows(); ++i)
        for (int k = 0; k < 3; ++k)
            remapped(i, k) = remap[static_cast<std::size_t>(faces(i, k))];
    return Mesh(compacted, remapped);
}

Mesh filter_invalid_faces(const Mesh &mesh, double area_tolerance,
                          bool remove_degenerate, bool remove_duplicate) {
    const auto vertices = mesh.vertices();
    const auto faces = mesh.faces();
    const double area_limit = std::max(0.0, area_tolerance);
    std::vector<std::array<std::int64_t, 3>> kept;
    kept.reserve(static_cast<std::size_t>(faces.rows()));
    std::set<std::array<std::int64_t, 3>> seen;
    for (Eigen::Index i = 0; i < faces.rows(); ++i) {
        std::array<std::int64_t, 3> face{faces(i, 0), faces(i, 1), faces(i, 2)};
        for (const auto id : face)
            if (id < 0 || id >= vertices.rows())
                throw std::out_of_range("Mesh face index is outside the vertex range");
        const bool repeated = face[0] == face[1] || face[1] == face[2] || face[0] == face[2];
        const auto cross = (vertices.row(face[1]) - vertices.row(face[0])).cross(
            vertices.row(face[2]) - vertices.row(face[0]));
        const bool degenerate = cross.squaredNorm() <= 4.0 * area_limit * area_limit;
        if (remove_degenerate && (repeated || degenerate))
            continue;
        auto key = face;
        std::sort(key.begin(), key.end());
        if (remove_duplicate && !seen.insert(key).second)
            continue;
        kept.push_back(face);
    }
    Faces output(static_cast<Index>(kept.size()), 3);
    for (std::size_t i = 0; i < kept.size(); ++i)
        for (int k = 0; k < 3; ++k)
            output(static_cast<Index>(i), k) = kept[i][static_cast<std::size_t>(k)];
    return Mesh(vertices, output);
}

BoundaryLoop normalize_boundary_loop(const BoundaryLoop &input) {
    if (input.size() >= 2 && input.front() == input.back())
        return BoundaryLoop(input.begin(), input.end() - 1);
    return input;
}

bool same_boundary_cycle(const BoundaryLoop &left, const BoundaryLoop &right) {
    if (left.size() != right.size() || left.empty())
        return false;
    for (std::size_t offset = 0; offset < right.size(); ++offset) {
        bool forward = true;
        bool reverse = true;
        for (std::size_t i = 0; i < left.size(); ++i) {
            forward = forward && left[i] == right[(offset + i) % right.size()];
            const auto reverse_index = (offset + right.size() - i) % right.size();
            reverse = reverse && left[i] == right[reverse_index];
        }
        if (forward || reverse)
            return true;
    }
    return false;
}

double boundary_diameter(const Mesh &mesh, const BoundaryLoop &loop) {
    const auto vertices = mesh.vertices();
    double diameter = 0.0;
    for (const auto left : loop) {
        if (left < 0 || left >= vertices.rows())
            throw std::out_of_range("Hole boundary contains an invalid vertex index");
        for (const auto right : loop) {
            if (right < 0 || right >= vertices.rows())
                throw std::out_of_range("Hole boundary contains an invalid vertex index");
            diameter = std::max(diameter,
                                (vertices.row(left) - vertices.row(right)).norm());
        }
    }
    return diameter;
}

std::vector<std::array<std::int64_t, 3>> triangulate_boundary_loop(
    const Mesh &mesh, const BoundaryLoop &loop) {
    if (loop.size() < 3)
        throw std::invalid_argument("A hole boundary requires at least three vertices");
    const auto vertices = mesh.vertices();
    vtkNew<vtkPoints> points;
    vtkNew<vtkIdList> point_ids;
    points->SetNumberOfPoints(static_cast<vtkIdType>(loop.size()));
    point_ids->SetNumberOfIds(static_cast<vtkIdType>(loop.size()));
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const auto id = loop[i];
        if (id < 0 || id >= vertices.rows())
            throw std::out_of_range("Hole boundary contains an invalid vertex index");
        points->SetPoint(static_cast<vtkIdType>(i), vertices.row(id).data());
        point_ids->SetId(static_cast<vtkIdType>(i), static_cast<vtkIdType>(i));
    }

    vtkNew<vtkPolygon> polygon;
    polygon->GetPoints()->DeepCopy(points);
    polygon->GetPointIds()->DeepCopy(point_ids);
    vtkNew<vtkIdList> triangle_ids;
    if (!polygon->Triangulate(0, triangle_ids, points) ||
        triangle_ids->GetNumberOfIds() < 3 || triangle_ids->GetNumberOfIds() % 3 != 0)
        throw std::invalid_argument("The selected hole boundary cannot be triangulated");

    std::vector<std::array<std::int64_t, 3>> triangles;
    triangles.reserve(static_cast<std::size_t>(triangle_ids->GetNumberOfIds() / 3));
    for (vtkIdType i = 0; i < triangle_ids->GetNumberOfIds(); i += 3) {
        std::array<std::int64_t, 3> triangle{};
        for (int k = 0; k < 3; ++k) {
            const auto local = triangle_ids->GetId(i + k);
            if (local < 0 || local >= static_cast<vtkIdType>(loop.size()))
                throw std::runtime_error("VTK returned an invalid hole triangle");
            triangle[static_cast<std::size_t>(k)] = loop[static_cast<std::size_t>(local)];
        }
        triangles.push_back(triangle);
    }
    return triangles;
}

Mesh fill_boundary_loops_by_triangulation(const Mesh &mesh,
                                          const std::vector<BoundaryLoop> &loops) {
    if (loops.empty())
        return mesh.clone();
    LegacyMesh input = to_legacy(mesh);
    vtkNew<vtkPolyData> output;
    output->DeepCopy(input.get_native());
    vtkNew<vtkCellArray> polygons;
    if (input.get_native()->GetPolys())
        polygons->DeepCopy(input.get_native()->GetPolys());
    for (const auto &loop : loops) {
        for (const auto &triangle : triangulate_boundary_loop(mesh, loop)) {
            vtkIdType ids[3] = {static_cast<vtkIdType>(triangle[0]),
                                static_cast<vtkIdType>(triangle[1]),
                                static_cast<vtkIdType>(triangle[2])};
            polygons->InsertNextCell(3, ids);
        }
    }
    output->SetPolys(polygons);
    output->BuildCells();
    // New cells have no meaningful values for existing cell attributes.
    output->GetCellData()->Initialize();
    return from_legacy(LegacyMesh(output));
}

struct LocalSubdivisionStep {
    Mesh mesh;
    std::vector<std::int64_t> selected_faces;
};

LocalSubdivisionStep subdivide_selected_faces_once(
    const Mesh &mesh, const std::vector<std::int64_t> &selected_faces,
    const AlgorithmOptions &options) {
    const auto vertices = mesh.vertices();
    const auto faces = mesh.faces();
    std::vector<bool> selected(static_cast<std::size_t>(faces.rows()), false);
    for (const auto face_id : selected_faces) {
        if (face_id < 0 || face_id >= faces.rows())
            throw std::out_of_range("Selected face index is outside the mesh range");
        selected[static_cast<std::size_t>(face_id)] = true;
    }

    std::set<Mesh::Edge> split_edges;
    for (const auto face_id : selected_faces) {
        const auto a = faces(face_id, 0);
        const auto b = faces(face_id, 1);
        const auto c = faces(face_id, 2);
        split_edges.insert({std::min(a, b), std::max(a, b)});
        split_edges.insert({std::min(b, c), std::max(b, c)});
        split_edges.insert({std::min(c, a), std::max(c, a)});
    }

    std::map<Mesh::Edge, std::int64_t> midpoints;
    std::vector<std::array<double, 3>> added_vertices;
    auto midpoint = [&](std::int64_t left, std::int64_t right) {
        const Mesh::Edge edge{std::min(left, right), std::max(left, right)};
        const auto existing = midpoints.find(edge);
        if (existing != midpoints.end())
            return existing->second;
        if (left < 0 || right < 0 || left >= vertices.rows() || right >= vertices.rows())
            throw std::out_of_range("Mesh edge contains an invalid vertex index");
        const auto point = (vertices.row(left) + vertices.row(right)) * 0.5;
        const auto id = static_cast<std::int64_t>(vertices.rows() + added_vertices.size());
        added_vertices.push_back({point[0], point[1], point[2]});
        midpoints.emplace(edge, id);
        return id;
    };

    std::vector<std::array<std::int64_t, 3>> output_faces;
    output_faces.reserve(static_cast<std::size_t>(faces.rows()) +
                         selected_faces.size() * 3);
    std::vector<std::int64_t> next_selected;
    auto append_face = [&](std::array<std::int64_t, 3> face, bool selected_face) {
        const auto output_id = static_cast<std::int64_t>(output_faces.size());
        output_faces.push_back(face);
        if (selected_face)
            next_selected.push_back(output_id);
    };

    for (Eigen::Index i = 0; i < faces.rows(); ++i) {
        check_cancel(options);
        const auto a = faces(i, 0);
        const auto b = faces(i, 1);
        const auto c = faces(i, 2);
        for (const auto id : {a, b, c})
            if (id < 0 || id >= vertices.rows())
                throw std::out_of_range("Mesh face index is outside the vertex range");

        const Mesh::Edge edge0{std::min(a, b), std::max(a, b)};
        const Mesh::Edge edge1{std::min(b, c), std::max(b, c)};
        const Mesh::Edge edge2{std::min(c, a), std::max(c, a)};
        const bool split0 = split_edges.find(edge0) != split_edges.end();
        const bool split1 = split_edges.find(edge1) != split_edges.end();
        const bool split2 = split_edges.find(edge2) != split_edges.end();
        const int split_count = static_cast<int>(split0) + static_cast<int>(split1) +
                                static_cast<int>(split2);
        const bool parent_selected = selected[static_cast<std::size_t>(i)];
        if (split_count == 0) {
            append_face({a, b, c}, parent_selected);
            continue;
        }

        const auto m0 = split0 ? midpoint(a, b) : -1;
        const auto m1 = split1 ? midpoint(b, c) : -1;
        const auto m2 = split2 ? midpoint(c, a) : -1;
        if (split_count == 1) {
            if (split0) {
                append_face({a, m0, c}, parent_selected);
                append_face({m0, b, c}, parent_selected);
            } else if (split1) {
                append_face({b, m1, a}, parent_selected);
                append_face({m1, c, a}, parent_selected);
            } else {
                append_face({c, m2, b}, parent_selected);
                append_face({m2, a, b}, parent_selected);
            }
        } else if (split_count == 2) {
            if (split0 && split1) {
                append_face({b, m1, m0}, parent_selected);
                append_face({m0, m1, c}, parent_selected);
                append_face({a, m0, c}, parent_selected);
            } else if (split1 && split2) {
                append_face({c, m2, m1}, parent_selected);
                append_face({m1, m2, a}, parent_selected);
                append_face({b, m1, a}, parent_selected);
            } else {
                append_face({a, m0, m2}, parent_selected);
                append_face({m2, m0, b}, parent_selected);
                append_face({c, m2, b}, parent_selected);
            }
        } else {
            append_face({a, m0, m2}, parent_selected);
            append_face({m0, b, m1}, parent_selected);
            append_face({m2, m1, c}, parent_selected);
            append_face({m0, m1, m2}, parent_selected);
        }
    }

    const auto point_count = static_cast<std::size_t>(vertices.rows()) + added_vertices.size();
    check_limit(options, point_count);
    Vertices output_vertices(static_cast<Index>(point_count), 3);
    output_vertices.topRows(vertices.rows()) = vertices;
    for (std::size_t i = 0; i < added_vertices.size(); ++i)
        for (int k = 0; k < 3; ++k)
            output_vertices(vertices.rows() + static_cast<Index>(i), k) =
                added_vertices[i][static_cast<std::size_t>(k)];
    Faces output_faces_matrix(static_cast<Index>(output_faces.size()), 3);
    for (std::size_t i = 0; i < output_faces.size(); ++i)
        for (int k = 0; k < 3; ++k)
            output_faces_matrix(static_cast<Index>(i), k) =
                output_faces[i][static_cast<std::size_t>(k)];
    return {Mesh(output_vertices, output_faces_matrix), std::move(next_selected)};
}

#if defined(SINDRE_UTILS_3D_CGAL)
using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using CgalMesh = CGAL::Surface_mesh<Kernel::Point_3>;

CgalMesh to_cgal(const Mesh &mesh) {
    CgalMesh output;
    const auto vertices = mesh.vertices();
    const auto faces = mesh.faces();
    std::vector<CgalMesh::Vertex_index> ids;
    ids.reserve(static_cast<std::size_t>(vertices.rows()));
    for (Eigen::Index i = 0; i < vertices.rows(); ++i)
        ids.push_back(output.add_vertex({vertices(i, 0), vertices(i, 1), vertices(i, 2)}));
    for (Eigen::Index i = 0; i < faces.rows(); ++i) {
        if (output.add_face(ids[faces(i, 0)], ids[faces(i, 1)], ids[faces(i, 2)]) ==
            CgalMesh::null_face())
            throw std::invalid_argument("Mesh orientation is not accepted by the topology backend");
    }
    return output;
}

Mesh from_cgal(CgalMesh mesh) {
    mesh.collect_garbage();
    Vertices vertices(static_cast<Eigen::Index>(mesh.number_of_vertices()), 3);
    Faces faces(static_cast<Eigen::Index>(mesh.number_of_faces()), 3);
    std::map<CgalMesh::Vertex_index, std::int64_t> ids;
    Eigen::Index i = 0;
    for (const auto vertex : mesh.vertices()) {
        const auto point = mesh.point(vertex);
        vertices.row(i) << CGAL::to_double(point.x()), CGAL::to_double(point.y()),
            CGAL::to_double(point.z());
        ids.emplace(vertex, i++);
    }
    i = 0;
    for (const auto face : mesh.faces()) {
        int k = 0;
        for (const auto vertex : CGAL::vertices_around_face(mesh.halfedge(face), mesh)) {
            if (k >= 3)
                throw std::runtime_error("Topology backend returned a nontriangle");
            faces(i, k++) = ids.at(vertex);
        }
        if (k != 3)
            throw std::runtime_error("Topology backend returned a nontriangle");
        ++i;
    }
    return Mesh(vertices, faces);
}

struct CgalHoleCycle {
    CgalMesh::Halfedge_index border;
    BoundaryLoop vertices;
    std::size_t edge_count = 0;
    double diameter = 0.0;
};

std::vector<CgalHoleCycle> collect_cgal_holes(const CgalMesh &mesh) {
    std::vector<CgalMesh::Halfedge_index> borders;
    CGAL::Polygon_mesh_processing::extract_boundary_cycles(
        mesh, std::back_inserter(borders));
    std::vector<CgalHoleCycle> holes;
    holes.reserve(borders.size());
    for (const auto border : borders) {
        CgalHoleCycle hole;
        hole.border = border;
        double minimum[3] = {std::numeric_limits<double>::max(),
                             std::numeric_limits<double>::max(),
                             std::numeric_limits<double>::max()};
        double maximum[3] = {std::numeric_limits<double>::lowest(),
                             std::numeric_limits<double>::lowest(),
                             std::numeric_limits<double>::lowest()};
        for (const auto halfedge : CGAL::halfedges_around_face(border, mesh)) {
            const auto vertex = target(halfedge, mesh);
            const auto point = mesh.point(vertex);
            hole.vertices.push_back(static_cast<std::int64_t>(vertex.idx()));
            minimum[0] = std::min(minimum[0], CGAL::to_double(point.x()));
            minimum[1] = std::min(minimum[1], CGAL::to_double(point.y()));
            minimum[2] = std::min(minimum[2], CGAL::to_double(point.z()));
            maximum[0] = std::max(maximum[0], CGAL::to_double(point.x()));
            maximum[1] = std::max(maximum[1], CGAL::to_double(point.y()));
            maximum[2] = std::max(maximum[2], CGAL::to_double(point.z()));
            ++hole.edge_count;
        }
        const auto dx = maximum[0] - minimum[0];
        const auto dy = maximum[1] - minimum[1];
        const auto dz = maximum[2] - minimum[2];
        hole.diameter = std::sqrt(dx * dx + dy * dy + dz * dz);
        holes.push_back(std::move(hole));
    }
    return holes;
}
#endif

#if defined(SINDRE_UTILS_3D_PCL)
using PclPoint = pcl::PointXYZ;
using PclCloud = pcl::PointCloud<PclPoint>;

PclCloud::Ptr to_pcl(const PointCloud &cloud) {
    auto valid = cloud.validate();
    if (!valid)
        throw std::invalid_argument(valid.error().describe());
    if (cloud.empty())
        throw std::invalid_argument("PointCloud must not be empty");
    if (static_cast<std::uint64_t>(cloud.points.rows()) >
        std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error("PointCloud exceeds the backend index range");
    auto output = PclCloud::Ptr(new PclCloud);
    output->points.reserve(cloud.size());
    for (Eigen::Index i = 0; i < cloud.points.rows(); ++i) {
        for (int k = 0; k < 3; ++k)
            if (std::abs(cloud.points(i, k)) > std::numeric_limits<float>::max())
                throw std::overflow_error("Point coordinate cannot be represented by float32");
        output->points.emplace_back(static_cast<float>(cloud.points(i, 0)),
                                    static_cast<float>(cloud.points(i, 1)),
                                    static_cast<float>(cloud.points(i, 2)));
    }
    output->width = static_cast<std::uint32_t>(output->points.size());
    output->height = 1;
    output->is_dense = true;
    return output;
}

PclCloud::Ptr to_pcl(const Vertices &vertices) {
    PointCloud cloud;
    cloud.points = vertices;
    return to_pcl(cloud);
}

PointCloud from_pcl_points(const PclCloud &value) {
    PointCloud output;
    output.points.resize(static_cast<Eigen::Index>(value.size()), 3);
    for (Eigen::Index i = 0; i < output.points.rows(); ++i) {
        const auto &point = value[static_cast<std::size_t>(i)];
        output.points.row(i) << point.x, point.y, point.z;
    }
    return output;
}

pcl::PointCloud<pcl::PointNormal>::Ptr to_pcl_normals(const PointCloud &cloud) {
    if (!cloud.normals)
        throw std::invalid_argument("Reconstruction requires point normals");
    auto valid = cloud.validate();
    if (!valid)
        throw std::invalid_argument(valid.error().describe());
    auto output = pcl::PointCloud<pcl::PointNormal>::Ptr(new pcl::PointCloud<pcl::PointNormal>);
    output->points.reserve(cloud.size());
    for (Eigen::Index i = 0; i < cloud.points.rows(); ++i) {
        const auto normal = cloud.normals->row(i).normalized();
        if (!normal.allFinite() || normal.norm() == 0)
            throw std::invalid_argument("Point normals must be finite and nonzero");
        pcl::PointNormal point;
        point.x = static_cast<float>(cloud.points(i, 0));
        point.y = static_cast<float>(cloud.points(i, 1));
        point.z = static_cast<float>(cloud.points(i, 2));
        point.normal_x = static_cast<float>(normal.x());
        point.normal_y = static_cast<float>(normal.y());
        point.normal_z = static_cast<float>(normal.z());
        output->points.push_back(point);
    }
    output->width = static_cast<std::uint32_t>(output->points.size());
    output->height = 1;
    output->is_dense = true;
    return output;
}

Mesh from_pcl(const pcl::PolygonMesh &value) {
    PclCloud cloud;
    pcl::fromPCLPointCloud2(value.cloud, cloud);
    if (cloud.empty() || value.polygons.empty())
        throw std::runtime_error("Reconstruction returned an empty mesh");
    Vertices vertices(static_cast<Eigen::Index>(cloud.size()), 3);
    for (Eigen::Index i = 0; i < vertices.rows(); ++i) {
        const auto &point = cloud[static_cast<std::size_t>(i)];
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
            throw std::runtime_error("Reconstruction returned nonfinite vertices");
        vertices.row(i) << point.x, point.y, point.z;
    }
    Faces faces(static_cast<Eigen::Index>(value.polygons.size()), 3);
    for (Eigen::Index i = 0; i < faces.rows(); ++i) {
        const auto &polygon = value.polygons[static_cast<std::size_t>(i)].vertices;
        if (polygon.size() != 3)
            throw std::runtime_error("Reconstruction returned a nontriangle polygon");
        for (int k = 0; k < 3; ++k) {
            if (polygon[static_cast<std::size_t>(k)] >= cloud.size())
                throw std::runtime_error("Reconstruction returned an invalid index");
            faces(i, k) = static_cast<std::int64_t>(polygon[static_cast<std::size_t>(k)]);
        }
    }
    return Mesh(vertices, faces);
}
#endif

struct PreparedGraphCut {
    GraphCutLabelLevel label_level = GraphCutLabelLevel::vertex;
    std::size_t class_count = 0;
    std::size_t node_count = 0;
    std::vector<double> unary;
    std::vector<std::size_t> initial_labels;
    std::vector<GraphCutEdge> edges;
    double smooth_factor = 0.0;
};

GraphCutLabelLevel resolve_graph_cut_level(const Mesh &mesh, Eigen::Index rows,
                                           GraphCutLabelLevel requested) {
    const auto vertex_count = mesh.npoints();
    const auto face_count = mesh.nfaces();
    if (requested == GraphCutLabelLevel::vertex) {
        if (rows != vertex_count)
            throw std::invalid_argument("Vertex labels must have one row per mesh vertex");
        return requested;
    }
    if (requested == GraphCutLabelLevel::face) {
        if (rows != face_count)
            throw std::invalid_argument("Face labels must have one row per mesh face");
        return requested;
    }
    if (rows == vertex_count)
        return GraphCutLabelLevel::vertex;
    if (rows == face_count)
        return GraphCutLabelLevel::face;
    throw std::invalid_argument(
        "Graph-cut labels must match either the mesh vertex count or face count");
}

PreparedGraphCut prepare_graph_cut(const Mesh &mesh, const Matrix &probabilities,
                                   const GraphCutOptions &options) {
    require_surface(mesh);
    if (probabilities.rows() == 0 || probabilities.cols() == 0)
        throw std::invalid_argument("Graph-cut probabilities must be a nonempty matrix");
    if (!std::isfinite(options.temperature) || options.temperature <= 0.0)
        throw std::invalid_argument("Graph-cut temperature must be finite and positive");
    if (!std::isfinite(options.smooth_factor))
        throw std::invalid_argument(
            "Graph-cut smooth_factor must be finite; negative values enable auto estimation");
    if (options.max_iterations == 0)
        throw std::invalid_argument("Graph-cut max_iterations must be positive");

    PreparedGraphCut prepared;
    prepared.label_level =
        resolve_graph_cut_level(mesh, probabilities.rows(), options.label_level);
    prepared.node_count = static_cast<std::size_t>(probabilities.rows());
    prepared.class_count = static_cast<std::size_t>(probabilities.cols());
    if (options.class_count != 0 && options.class_count != prepared.class_count)
        throw std::invalid_argument("Graph-cut class_count does not match probability columns");
    check_limit(options, prepared.node_count);
    if (prepared.node_count > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::length_error("Graph-cut mesh is too large for the internal solver");

    prepared.unary.resize(prepared.node_count * prepared.class_count);
    prepared.initial_labels.resize(prepared.node_count);
    constexpr double minimum_probability = 1e-12;
    for (Eigen::Index row = 0; row < probabilities.rows(); ++row) {
        double sum = 0.0;
        for (Eigen::Index column = 0; column < probabilities.cols(); ++column) {
            const double value = probabilities(row, column);
            if (!std::isfinite(value) || value < 0.0)
                throw std::invalid_argument(
                    "Graph-cut probabilities must be finite and nonnegative");
            sum += value;
        }
        if (!std::isfinite(sum) || sum <= 0.0)
            throw std::invalid_argument("Each graph-cut probability row must have positive mass");

        double transformed_sum = 0.0;
        std::size_t best = 0;
        double best_probability = -1.0;
        for (Eigen::Index column = 0; column < probabilities.cols(); ++column) {
            double value = std::max(probabilities(row, column) / sum,
                                    minimum_probability);
            if (options.temperature != 1.0)
                value = std::pow(value, 1.0 / options.temperature);
            transformed_sum += value;
            if (value > best_probability) {
                best_probability = value;
                best = static_cast<std::size_t>(column);
            }
            prepared.unary[static_cast<std::size_t>(row) * prepared.class_count +
                           static_cast<std::size_t>(column)] = value;
        }
        if (!std::isfinite(transformed_sum) || transformed_sum <= 0.0)
            throw std::invalid_argument("Graph-cut probability normalization failed");
        double minimum_unary = std::numeric_limits<double>::infinity();
        for (Eigen::Index column = 0; column < probabilities.cols(); ++column) {
            auto &value = prepared.unary[static_cast<std::size_t>(row) * prepared.class_count +
                                         static_cast<std::size_t>(column)];
            value = -100.0 * std::log10(value / transformed_sum);
            minimum_unary = std::min(minimum_unary, value);
        }
        for (Eigen::Index column = 0; column < probabilities.cols(); ++column)
            prepared.unary[static_cast<std::size_t>(row) * prepared.class_count +
                           static_cast<std::size_t>(column)] -= minimum_unary;
        prepared.initial_labels[static_cast<std::size_t>(row)] = best;
    }

    const auto positions = prepared.label_level == GraphCutLabelLevel::vertex
        ? mesh.vertices()
        : mesh.faces_barycentre();
    const auto normals = prepared.label_level == GraphCutLabelLevel::vertex
        ? mesh.vertex_normals()
        : mesh.face_normals();
    const auto adjacency = prepared.label_level == GraphCutLabelLevel::vertex
        ? mesh.get_vertex_adj_list()
        : mesh.get_face_adj_list();
    if (positions.rows() != static_cast<Eigen::Index>(prepared.node_count) ||
        positions.cols() != 3 || normals.rows() != positions.rows() || normals.cols() != 3 ||
        adjacency.size() != prepared.node_count)
        throw std::runtime_error("Mesh geometry caches have inconsistent graph-cut dimensions");
    if (!positions.allFinite())
        throw std::invalid_argument("Graph-cut mesh positions must be finite");

    std::set<std::pair<std::size_t, std::size_t>> seen_edges;
    std::vector<double> raw_weights;
    raw_weights.reserve(prepared.node_count * 3);
    for (std::size_t i = 0; i < prepared.node_count; ++i) {
        for (const auto neighbor_value : adjacency[i]) {
            if (neighbor_value < 0 ||
                neighbor_value >= static_cast<std::int64_t>(prepared.node_count))
                throw std::out_of_range("Graph-cut adjacency contains an invalid node index");
            const auto neighbor = static_cast<std::size_t>(neighbor_value);
            if (neighbor <= i || !seen_edges.emplace(i, neighbor).second)
                continue;
            const auto normal_i = normals.row(static_cast<Eigen::Index>(i));
            const auto normal_j = normals.row(static_cast<Eigen::Index>(neighbor));
            const double norm_i = normal_i.norm();
            const double norm_j = normal_j.norm();
            double dot = 0.0;
            if (std::isfinite(norm_i) && std::isfinite(norm_j) && norm_i > 0.0 && norm_j > 0.0)
                dot = normal_i.dot(normal_j) / (norm_i * norm_j);
            const double theta = std::max(
                std::acos(std::clamp(dot, -1.0, 1.0)), 1e-6);
            const double distance =
                (positions.row(static_cast<Eigen::Index>(i)) -
                 positions.row(static_cast<Eigen::Index>(neighbor))).norm();
            double weight = -std::log10(theta / std::acos(-1.0)) * distance;
            if (theta < std::acos(-1.0) / 2.0)
                weight *= 10.0;
            if (!std::isfinite(weight) || weight < 0.0)
                throw std::runtime_error("Graph-cut geometry produced an invalid edge weight");
            if (weight > 0.0) {
                prepared.edges.push_back({i, neighbor, weight});
                raw_weights.push_back(weight);
            }
        }
    }

    if (options.smooth_factor < 0.0) {
        if (raw_weights.empty()) {
            prepared.smooth_factor = 0.0;
        } else {
            std::vector<double> unary_values = prepared.unary;
            const double unary_median = median_value(std::move(unary_values));
            const double weight_median = median_value(std::move(raw_weights));
            prepared.smooth_factor = weight_median > 1e-12
                ? std::clamp(unary_median / weight_median * 0.8, 0.0, 1e4)
                : 0.0;
        }
    } else {
        prepared.smooth_factor = options.smooth_factor;
    }
    for (auto &edge : prepared.edges)
        edge.weight *= prepared.smooth_factor;
    return prepared;
}

GraphCutResult run_graph_cut(PreparedGraphCut prepared, const GraphCutOptions &options) {
    std::vector<std::size_t> current = prepared.initial_labels;
    const double before = graph_cut_energy(
        current, prepared.unary, prepared.class_count, prepared.edges);
    double current_energy = before;
    const auto required_labels = unique_label_count(current);
    if (prepared.class_count == 0)
        throw std::invalid_argument("Graph-cut requires at least one class");
    const std::size_t move_count = options.algorithm == GraphCutAlgorithm::expansion
        ? prepared.class_count
        : prepared.class_count * (prepared.class_count - 1) / 2;
    const std::size_t progress_move_count = std::max<std::size_t>(move_count, 1);

    bool converged = false;
    std::size_t completed_iterations = 0;
    for (unsigned iteration = 0; iteration < options.max_iterations; ++iteration) {
        check_cancel(options);
        bool changed = false;
        std::size_t move_index = 0;
        if (options.algorithm == GraphCutAlgorithm::expansion) {
            for (std::size_t alpha = 0; alpha < prepared.class_count; ++alpha) {
                check_cancel(options);
                auto move = alpha_expansion_move(current, alpha, prepared.unary,
                                                 prepared.class_count, prepared.edges);
                auto candidate = current;
                for (std::size_t i = 0; i < candidate.size(); ++i)
                    if (move[i])
                        candidate[i] = alpha;
                const double energy = graph_cut_energy(
                    candidate, prepared.unary, prepared.class_count, prepared.edges);
                if (energy + 1e-9 < current_energy &&
                    (!options.keep_label || unique_label_count(candidate) >= required_labels)) {
                    current = std::move(candidate);
                    current_energy = energy;
                    changed = true;
                }
                ++move_index;
                progress(options, (static_cast<double>(iteration) * progress_move_count +
                                   move_index) /
                                     (static_cast<double>(options.max_iterations) *
                                      progress_move_count));
            }
        } else if (options.algorithm == GraphCutAlgorithm::swap) {
            std::vector<std::size_t> active_nodes;
            for (std::size_t alpha = 0; alpha < prepared.class_count; ++alpha) {
                for (std::size_t beta = alpha + 1; beta < prepared.class_count; ++beta) {
                    check_cancel(options);
                    auto move = alpha_beta_swap_move(
                        current, alpha, beta, prepared.unary, prepared.class_count,
                        prepared.edges, active_nodes);
                    auto candidate = current;
                    for (std::size_t i = 0; i < move.size(); ++i)
                        candidate[active_nodes[i]] = move[i] ? beta : alpha;
                    const double energy = graph_cut_energy(
                        candidate, prepared.unary, prepared.class_count, prepared.edges);
                    if (energy + 1e-9 < current_energy &&
                        (!options.keep_label || unique_label_count(candidate) >= required_labels)) {
                        current = std::move(candidate);
                        current_energy = energy;
                        changed = true;
                    }
                    ++move_index;
                    progress(options, (static_cast<double>(iteration) * progress_move_count +
                                       move_index) /
                                         (static_cast<double>(options.max_iterations) *
                                          progress_move_count));
                }
            }
        } else {
            throw std::invalid_argument("Unknown graph-cut algorithm");
        }
        completed_iterations = iteration + 1;
        if (!changed) {
            converged = true;
            break;
        }
    }
    progress(options, 1.0);
    GraphCutResult result;
    result.labels.resize(static_cast<Eigen::Index>(current.size()));
    for (std::size_t i = 0; i < current.size(); ++i)
        result.labels(static_cast<Eigen::Index>(i)) = static_cast<std::int64_t>(current[i]);
    result.label_level = prepared.label_level;
    result.class_count = prepared.class_count;
    result.energy_before = before;
    result.energy_after = current_energy;
    result.smooth_factor = prepared.smooth_factor;
    result.iterations = completed_iterations;
    result.converged = converged;
    return result;
}

AlgorithmResult<Mesh> join_mesh_strips_impl(
    const std::vector<Vertices> &first, const std::vector<Vertices> &second,
    const JoinStripsOptions &options) {
    if (first.empty() || second.empty())
        throw std::invalid_argument("join_with_strips requires at least one polyline");
    if (first.size() != second.size())
        throw std::invalid_argument(
            "join_with_strips requires the same number of polylines on both sides");

    std::size_t input_point_count = 0;
    for (std::size_t line_index = 0; line_index < first.size(); ++line_index) {
        const auto &left = first[line_index];
        const auto &right = second[line_index];
        const auto minimum_points = options.closed ? 3 : 2;
        if (left.cols() != 3 || right.cols() != 3 ||
            left.rows() < minimum_points || right.rows() < minimum_points ||
            left.rows() != right.rows() || !left.allFinite() || !right.allFinite())
            throw std::invalid_argument(
                "join_with_strips requires matching finite N x 3 polylines");
        if (input_point_count > std::numeric_limits<std::size_t>::max() -
                                    static_cast<std::size_t>(left.rows() + right.rows()))
            throw std::overflow_error("join_with_strips point count overflow");
        input_point_count += static_cast<std::size_t>(left.rows() + right.rows());
    }
    check_limit(options, input_point_count);
    if (input_point_count > static_cast<std::size_t>(std::numeric_limits<vtkIdType>::max()))
        throw std::length_error("join_with_strips input is too large for VTK");

    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> strips;
    for (std::size_t line_index = 0; line_index < first.size(); ++line_index) {
        check_cancel(options);
        const auto &left = first[line_index];
        const auto &right = second[line_index];
        const auto count = static_cast<std::size_t>(left.rows());
        const auto strip_count = count + (options.closed ? 1u : 0u);
        std::vector<vtkIdType> ids;
        ids.reserve(strip_count * 2);
        for (std::size_t i = 0; i < strip_count; ++i) {
            const auto source_index = options.closed && i == count ? 0 : i;
            double left_point[3] = {
                left(static_cast<Eigen::Index>(source_index), 0),
                left(static_cast<Eigen::Index>(source_index), 1),
                left(static_cast<Eigen::Index>(source_index), 2)};
            double right_point[3] = {
                right(static_cast<Eigen::Index>(source_index), 0),
                right(static_cast<Eigen::Index>(source_index), 1),
                right(static_cast<Eigen::Index>(source_index), 2)};
            ids.push_back(points->InsertNextPoint(left_point));
            ids.push_back(points->InsertNextPoint(right_point));
        }
        strips->InsertNextCell(static_cast<vtkIdType>(ids.size()), ids.data());
        progress(options, static_cast<double>(line_index + 1) /
                             static_cast<double>(first.size()));
    }

    vtkNew<vtkPolyData> strip_mesh;
    strip_mesh->SetPoints(points);
    strip_mesh->SetStrips(strips);

    vtkNew<vtkTriangleFilter> triangulate;
    triangulate->SetInputData(strip_mesh);
    triangulate->PassLinesOff();
    triangulate->PassVertsOff();
    triangulate->Update();
    if (!triangulate->GetOutput() || triangulate->GetErrorCode() ||
        triangulate->GetOutput()->GetNumberOfPolys() == 0)
        throw std::runtime_error("VTK triangle-strip conversion returned no faces");

    Mesh output = from_legacy(LegacyMesh(triangulate->GetOutput()));
    AlgorithmReport report;
    report.input_points = input_point_count;
    report.output_points = static_cast<std::size_t>(output.npoints());
    report.input_faces = 0;
    report.output_faces = static_cast<std::size_t>(output.nfaces());
    report.changed_elements = report.output_faces;
    report.converged = true;
    return {std::move(output), std::move(report)};
}

} // namespace

Result<void> validate_mesh(const Mesh &mesh, double area_tolerance) {
#if defined(SINDRE_NO_EXCEPTIONS)
    (void)mesh;
    (void)area_tolerance;
    return Result<void>::failure(Error::make(
        std::errc::operation_not_supported,
        "Mesh validation requires the exception-enabled validation adapter",
        "utils_3d.validate_mesh"));
#else
    try {
        const auto vertices = mesh.vertices();
        const auto faces = mesh.faces();
        if (vertices.cols() != 3 || faces.cols() != 3 || !vertices.allFinite())
            throw std::invalid_argument("Mesh geometry must be finite N x 3/M x 3 data");
        if (!std::isfinite(area_tolerance) || area_tolerance < 0)
            throw std::invalid_argument("Mesh area tolerance must be finite and nonnegative");
        for (Eigen::Index i = 0; i < faces.rows(); ++i)
            for (int k = 0; k < 3; ++k)
                if (faces(i, k) < 0 || faces(i, k) >= vertices.rows())
                    throw std::out_of_range("Mesh face index is outside the vertex range");
        (void)mesh.check(area_tolerance);
        return Result<void>::success();
    } catch (const std::invalid_argument &error) {
        detail::logging::error("utils_3d.validate_mesh", error.what());
        return Result<void>::failure(Error::make(std::errc::invalid_argument, error.what(),
                                                 "utils_3d.validate_mesh"));
    } catch (const std::out_of_range &error) {
        detail::logging::error("utils_3d.validate_mesh", error.what());
        return Result<void>::failure(Error::make(std::errc::result_out_of_range, error.what(),
                                                 "utils_3d.validate_mesh"));
    } catch (const std::exception &error) {
        detail::logging::error("utils_3d.validate_mesh", error.what());
        return Result<void>::failure(Error::make(std::errc::io_error, error.what(),
                                                 "utils_3d.validate_mesh"));
    } catch (...) {
        detail::logging::error("utils_3d.validate_mesh", "Unknown mesh validation failure");
        return Result<void>::failure(Error::make(std::errc::io_error,
                                                 "Unknown mesh validation failure",
                                                 "utils_3d.validate_mesh"));
    }
#endif
}

Result<AlgorithmResult<Mesh>> clean_mesh(const Mesh &mesh, const CleanOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        check_limit(options, static_cast<std::size_t>(mesh.npoints()));
        check_cancel(options);
        if (!std::isfinite(options.tolerance) || options.tolerance < 0)
            throw std::invalid_argument("Clean tolerance must be finite and nonnegative");
        LegacyMesh input = to_legacy(mesh);
        vtkSmartPointer<vtkPolyData> native_output;
        if (options.merge_duplicate_vertices) {
            vtkNew<vtkStaticCleanPolyData> filter;
            filter->SetInputData(input.get_native());
            filter->ToleranceIsAbsoluteOn();
            filter->SetAbsoluteTolerance(options.tolerance);
#if VTK_VERSION_NUMBER >= VTK_VERSION_CHECK(9, 2, 0)
            filter->SetRemoveUnusedPoints(options.remove_unused_vertices);
#else
            (void)options.remove_unused_vertices;
#endif
            filter->ConvertPolysToLinesOff();
            filter->ConvertLinesToPointsOff();
            filter->Update();
            native_output = filter->GetOutput();
        } else {
            vtkNew<vtkCleanPolyData> filter;
            filter->SetInputData(input.get_native());
            filter->PointMergingOff();
            filter->ConvertPolysToLinesOff();
            filter->ConvertLinesToPointsOff();
            filter->Update();
            native_output = filter->GetOutput();
        }
        if (!native_output)
            throw std::runtime_error("VTK clean returned no mesh");
        auto output = from_legacy(LegacyMesh(native_output));
        if (options.remove_unused_vertices)
            output = compact_unused_vertices(output);
        progress(options, 1.0);
        return result_mesh(mesh, std::move(output));
    }, "utils_3d.clean_mesh");
}

Result<AlgorithmResult<Mesh>> fix_mesh(const Mesh &mesh, const FixOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        check_limit(options, static_cast<std::size_t>(mesh.npoints()));
        check_cancel(options);
        if (!std::isfinite(options.tolerance) || options.tolerance < 0 ||
            !std::isfinite(options.degenerate_area_tolerance) ||
            options.degenerate_area_tolerance < 0)
            throw std::invalid_argument("Invalid mesh repair tolerance");

        CleanOptions clean_options;
        clean_options.tolerance = options.tolerance;
        clean_options.merge_duplicate_vertices = options.merge_duplicate_vertices;
        clean_options.remove_unused_vertices = options.remove_unused_vertices;
        clean_options.progress = options.progress;
        clean_options.cancellation = options.cancellation;
        clean_options.max_points = options.max_points;
        auto cleaned = clean_mesh(mesh, clean_options);
        if (!cleaned)
            throw std::runtime_error(cleaned.error().describe());

        Mesh output = std::move(cleaned.value().value);
        if (options.remove_degenerate_faces || options.remove_duplicate_faces)
            output = filter_invalid_faces(output, options.degenerate_area_tolerance,
                                          options.remove_degenerate_faces,
                                          options.remove_duplicate_faces);
        if (options.remove_unused_vertices)
            output = compact_unused_vertices(output);
        if (options.fill_holes) {
            FillHolesOptions hole_options;
            hole_options.max_hole_size = options.max_hole_size;
            hole_options.progress = options.progress;
            hole_options.cancellation = options.cancellation;
            hole_options.max_points = options.max_points;
            auto filled = fill_mesh_holes(output, hole_options);
            if (!filled)
                throw std::runtime_error(filled.error().describe());
            output = std::move(filled.value().value);
        }
        progress(options, 1.0);
        return result_mesh(mesh, std::move(output));
    }, "utils_3d.fix_mesh");
}

Result<AlgorithmResult<Mesh>> simplify_mesh(
    const Mesh &mesh, std::size_t target_faces, const SimplifyOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        require_surface(mesh);
        check_limit(options, static_cast<std::size_t>(mesh.npoints()));
        check_cancel(options);
        if (target_faces == 0)
            throw std::invalid_argument("target_faces must be positive");

        const auto input_faces = static_cast<std::size_t>(mesh.nfaces());
        if (target_faces >= input_faces) {
            auto result = result_mesh(mesh, mesh.clone());
            result.report.converged = target_faces == input_faces;
            result.report.residual = target_faces > input_faces
                ? static_cast<double>(target_faces - input_faces)
                : 0.0;
            progress(options, 1.0);
            return result;
        }

        if (options.backend == SimplifyBackend::cgal ||
            options.algorithm == SimplifyAlgorithm::cgal_edge_collapse) {
#if defined(SINDRE_UTILS_3D_CGAL)
            auto value = to_cgal(mesh);
            using Stop = CGAL::Surface_mesh_simplification::Face_count_stop_predicate<CgalMesh>;
            const auto collapsed = CGAL::Surface_mesh_simplification::edge_collapse(
                value, Stop(target_faces));
            value.collect_garbage();
            auto output = from_cgal(std::move(value));
            auto result = result_mesh(mesh, std::move(output));
            result.report.iterations = collapsed > 0
                ? static_cast<std::size_t>(collapsed)
                : 0;
            const auto actual = static_cast<std::size_t>(result.value.nfaces());
            result.report.converged = actual == target_faces;
            result.report.residual = actual > target_faces
                ? static_cast<double>(actual - target_faces)
                : static_cast<double>(target_faces - actual);
            result.report.changed_elements = actual > input_faces
                ? actual - input_faces
                : input_faces - actual;
            progress(options, 1.0);
            return result;
#else
            throw unsupported_error(
                "CGAL simplification was requested but the CGAL backend is not enabled");
#endif
        }

        if (options.backend != SimplifyBackend::vtk)
            throw std::invalid_argument("Unknown mesh simplification backend");

        if (!std::isfinite(options.feature_angle) || options.feature_angle < 0 ||
            options.feature_angle > 180)
            throw std::invalid_argument("Simplify feature angle must lie in [0,180]");
        LegacyMesh input = to_legacy(mesh);
        vtkSmartPointer<vtkPolyData> native_output;
        const auto reduction = std::clamp(
            1.0 - static_cast<double>(target_faces) / input_faces, 0.0, 1.0);
        switch (options.algorithm) {
        case SimplifyAlgorithm::decimate_pro: {
            vtkNew<vtkDecimatePro> filter;
            filter->SetInputData(input.get_native());
            filter->SetTargetReduction(reduction);
            filter->SetFeatureAngle(options.feature_angle);
            if (options.preserve_topology)
                filter->PreserveTopologyOn();
            else
                filter->PreserveTopologyOff();
            filter->SplittingOff();
            filter->Update();
            native_output = filter->GetOutput();
            break;
        }
        case SimplifyAlgorithm::quadric_decimation: {
            vtkNew<vtkQuadricDecimation> filter;
            filter->SetInputData(input.get_native());
            filter->SetTargetReduction(reduction);
            filter->SetAttributeErrorMetric(false);
            filter->SetVolumePreservation(options.preserve_topology);
            filter->Update();
            native_output = filter->GetOutput();
            break;
        }
        case SimplifyAlgorithm::quadric_clustering: {
            const double cells = std::max(1.0, static_cast<double>(target_faces) / 2.0);
            const auto divisions = static_cast<int>(std::clamp(
                std::ceil(std::cbrt(cells)), 1.0, static_cast<double>(std::numeric_limits<int>::max())));
            vtkNew<vtkQuadricClustering> filter;
            filter->SetInputData(input.get_native());
            filter->SetNumberOfDivisions(divisions, divisions, divisions);
            filter->UseInputPointsOn();
            filter->UseFeatureEdgesOn();
            filter->UseFeaturePointsOn();
            filter->SetFeaturePointsAngle(options.feature_angle);
            filter->Update();
            native_output = filter->GetOutput();
            break;
        }
        case SimplifyAlgorithm::cgal_edge_collapse:
            throw std::invalid_argument("CGAL edge collapse requires the CGAL backend");
        }
        if (!native_output)
            throw std::runtime_error("VTK simplification returned no mesh");

        auto result = result_mesh(mesh, from_legacy(LegacyMesh(native_output)));
        const auto actual = static_cast<std::size_t>(result.value.nfaces());
        result.report.converged = actual == target_faces;
        result.report.residual = actual > target_faces
            ? static_cast<double>(actual - target_faces)
            : static_cast<double>(target_faces - actual);
        result.report.changed_elements = actual > input_faces
            ? actual - input_faces
            : input_faces - actual;
        progress(options, 1.0);
        return result;
    }, "utils_3d.simplify_mesh");
}

Result<std::vector<Mesh>> split_mesh_components(
    const Mesh &mesh, bool max_area, const AlgorithmOptions &options) {
    return run<std::vector<Mesh>>([&] {
        require_surface(mesh);
        check_cancel(options);
        LegacyMesh input = to_legacy(mesh);
        auto legacy_components = input.split_component_by_faces();
        if (legacy_components.empty())
            throw std::runtime_error("Mesh connectivity split returned no components");

        std::vector<Mesh> components;
        components.reserve(legacy_components.size());
        for (std::size_t i = 0; i < legacy_components.size(); ++i) {
            check_cancel(options);
            components.emplace_back(from_legacy(legacy_components[i]));
            progress(options, static_cast<double>(i + 1) /
                                 static_cast<double>(legacy_components.size()));
        }
        if (max_area) {
            const auto largest = std::max_element(
                components.begin(), components.end(),
                [](const Mesh &left, const Mesh &right) {
                    return left.area() < right.area();
                });
            Mesh result = std::move(*largest);
            components.clear();
            components.emplace_back(std::move(result));
        }
        return components;
    }, "utils_3d.split_mesh_components");
}

Result<std::vector<BoundaryLoop>> find_mesh_boundaries(
    const Mesh &mesh, bool max_boundary, bool ordered, const AlgorithmOptions &options) {
    return run<std::vector<BoundaryLoop>>([&] {
        check_cancel(options);
        auto loops = mesh.boundary_loops();
        if (loops.empty())
            return std::vector<BoundaryLoop>{};

        const auto vertices = mesh.vertices();
        auto perimeter = [&vertices](const BoundaryLoop &loop) {
            double length = 0.0;
            if (loop.size() < 2)
                return length;
            for (std::size_t i = 0; i < loop.size(); ++i) {
                const auto left = loop[i];
                const auto right = loop[(i + 1) % loop.size()];
                if (left < 0 || right < 0 || left >= vertices.rows() ||
                    right >= vertices.rows())
                    throw std::out_of_range("Boundary loop contains an invalid vertex index");
                length += (vertices.row(left) - vertices.row(right)).norm();
            }
            return length;
        };

        if (max_boundary) {
            const auto largest = std::max_element(
                loops.begin(), loops.end(), [&](const BoundaryLoop &left,
                                                const BoundaryLoop &right) {
                    return perimeter(left) < perimeter(right);
                });
            loops = {*largest};
        }

        if (!ordered)
            for (auto &loop : loops)
                std::sort(loop.begin(), loop.end());
        return loops;
    }, "utils_3d.find_mesh_boundaries");
}

Result<AlgorithmResult<Mesh>> repair_mesh(const Mesh &mesh, const RepairOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        check_limit(options, static_cast<std::size_t>(mesh.npoints()));
        check_cancel(options);
        CleanOptions clean_options;
        clean_options.progress = options.progress;
        clean_options.cancellation = options.cancellation;
        clean_options.max_points = options.max_points;
        auto cleaned = options.remove_duplicate_vertices || options.remove_degenerate
            ? clean_mesh(mesh, clean_options)
            : Result<AlgorithmResult<Mesh>>::success(result_mesh(mesh, mesh.clone()));
        if (!cleaned)
            throw std::runtime_error(cleaned.error().describe());
        Mesh output = cleaned.value().value;
        if (options.close_holes) {
            auto filled = fill_mesh_holes(output);
            if (!filled)
                throw std::runtime_error(filled.error().describe());
            output = std::move(filled.value().value);
        }
        progress(options, 1.0);
        return result_mesh(mesh, std::move(output));
    }, "utils_3d.repair_mesh");
}

Result<AlgorithmResult<Mesh>> decimate_mesh(const Mesh &mesh, const DecimateOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        require_surface(mesh);
        check_limit(options, static_cast<std::size_t>(mesh.npoints()));
        if (options.target_faces == 0)
            throw std::invalid_argument("target_faces must be positive");
        if (options.target_faces >= static_cast<std::size_t>(mesh.nfaces()))
            return result_mesh(mesh, mesh.clone());
        LegacyMesh input = to_legacy(mesh);
        vtkNew<vtkDecimatePro> filter;
        filter->SetInputData(input.get_native());
        filter->SetTargetReduction(1.0 - double(options.target_faces) / mesh.nfaces());
        filter->PreserveTopologyOn();
        filter->SplittingOff();
        filter->Update();
        progress(options, 1.0);
        return result_mesh(mesh, from_legacy(LegacyMesh(filter->GetOutput())));
    }, "utils_3d.decimate_mesh");
}

Result<AlgorithmResult<Mesh>> smooth_mesh(const Mesh &mesh, const SmoothOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        require_surface(mesh);
        if (options.iterations < 1 || !std::isfinite(options.strength) ||
            options.strength <= 0 || options.strength > 1)
            throw std::invalid_argument("Invalid smoothing options");

        std::vector<unsigned char> selected;
        if (options.scope == SmoothScope::local) {
            selected.assign(static_cast<std::size_t>(mesh.npoints()), 0);
            const auto select_vertex = [&](std::int64_t index) {
                if (index < 0 || index >= mesh.npoints())
                    throw std::out_of_range("Local smoothing vertex index is out of range");
                selected[static_cast<std::size_t>(index)] = 1;
            };
            for (const auto index : options.vertex_indices)
                select_vertex(index);

            const auto faces = mesh.faces();
            for (const auto face_index : options.face_indices) {
                if (face_index < 0 || face_index >= faces.rows())
                    throw std::out_of_range("Local smoothing face index is out of range");
                for (int k = 0; k < 3; ++k)
                    select_vertex(faces(static_cast<Eigen::Index>(face_index), k));
            }
            if (std::none_of(selected.begin(), selected.end(), [](unsigned char value) {
                    return value != 0;
                }))
                throw std::invalid_argument(
                    "Local smoothing requires vertex_indices or face_indices");
        } else if (options.scope == SmoothScope::global) {
            if (!options.vertex_indices.empty() || !options.face_indices.empty())
                throw std::invalid_argument(
                    "Global smoothing cannot receive local vertex or face indices");
        } else {
            throw std::invalid_argument("Unknown smoothing scope");
        }

        LegacyMesh input = to_legacy(mesh);
        auto make_output = [&](vtkPolyData *polydata) {
            if (!polydata || polydata->GetNumberOfPoints() != mesh.npoints())
                throw std::runtime_error("Smoothing returned an invalid point set");
            if (options.scope == SmoothScope::global)
                return from_legacy(LegacyMesh(polydata));

            auto vertices = mesh.vertices();
            for (Eigen::Index i = 0; i < vertices.rows(); ++i) {
                if (!selected[static_cast<std::size_t>(i)])
                    continue;
                const auto *point = polydata->GetPoint(static_cast<vtkIdType>(i));
                if (!point || !std::isfinite(point[0]) || !std::isfinite(point[1]) ||
                    !std::isfinite(point[2]))
                    throw std::runtime_error("Smoothing returned a nonfinite point");
                vertices.row(i) << point[0], point[1], point[2];
            }
            return Mesh(std::move(vertices), mesh.faces());
        };

        Mesh output;
        if (options.preserve_volume) {
            vtkNew<vtkWindowedSincPolyDataFilter> filter;
            filter->SetInputData(input.get_native());
            filter->SetNumberOfIterations(options.iterations);
            filter->SetPassBand(options.strength);
            filter->NormalizeCoordinatesOn();
            filter->Update();
            output = make_output(filter->GetOutput());
        } else {
            vtkNew<vtkSmoothPolyDataFilter> filter;
            filter->SetInputData(input.get_native());
            filter->SetNumberOfIterations(options.iterations);
            filter->SetRelaxationFactor(options.strength);
            filter->FeatureEdgeSmoothingOff();
            filter->BoundarySmoothingOff();
            filter->Update();
            output = make_output(filter->GetOutput());
        }

        progress(options, 1.0);
        auto result = result_mesh(mesh, std::move(output));
        result.report.iterations = static_cast<std::size_t>(options.iterations);
        return result;
    }, "utils_3d.smooth_mesh");
}

Result<AlgorithmResult<Mesh>> deform_mesh(
    const Mesh &mesh, const Vertices &source_points, const Vertices &target_points,
    const DeformationOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        require_surface(mesh);
        if (options.method != DeformationMethod::thin_plate_spline)
            throw std::invalid_argument("Unknown deformation method");
        if (!std::isfinite(options.regularization) || options.regularization < 0)
            throw std::invalid_argument("regularization must be finite and nonnegative");
        if (source_points.cols() != 3 || target_points.cols() != 3 ||
            source_points.rows() != target_points.rows() || source_points.rows() < 4 ||
            !source_points.allFinite() || !target_points.allFinite())
            throw std::invalid_argument(
                "Thin-plate deformation requires matching finite N x 3 control points (N >= 4)");

        std::vector<unsigned char> selected;
        if (options.scope == DeformationScope::local) {
            selected.assign(static_cast<std::size_t>(mesh.npoints()), 0);
            const auto select_vertex = [&](std::int64_t index) {
                if (index < 0 || index >= mesh.npoints())
                    throw std::out_of_range("Local deformation vertex index is out of range");
                selected[static_cast<std::size_t>(index)] = 1;
            };
            for (const auto index : options.vertex_indices)
                select_vertex(index);

            const auto faces = mesh.faces();
            for (const auto face_index : options.face_indices) {
                if (face_index < 0 || face_index >= faces.rows())
                    throw std::out_of_range("Local deformation face index is out of range");
                for (int k = 0; k < 3; ++k)
                    select_vertex(faces(static_cast<Eigen::Index>(face_index), k));
            }
            if (std::none_of(selected.begin(), selected.end(), [](unsigned char value) {
                    return value != 0;
                }))
                throw std::invalid_argument(
                    "Local deformation requires vertex_indices or face_indices");
        } else if (options.scope == DeformationScope::global) {
            if (!options.vertex_indices.empty() || !options.face_indices.empty())
                throw std::invalid_argument(
                    "Global deformation cannot receive local vertex or face indices");
        } else {
            throw std::invalid_argument("Unknown deformation scope");
        }

        check_limit(options, static_cast<std::size_t>(mesh.npoints()));
        check_limit(options, static_cast<std::size_t>(source_points.rows()));

        const Eigen::Index control_count = source_points.rows();
        Eigen::MatrixXd affine_points(control_count, 4);
        affine_points.col(0).setOnes();
        affine_points.block(0, 1, control_count, 3) = source_points;

        Eigen::FullPivLU<Eigen::MatrixXd> affine_solver(affine_points);
        if (affine_solver.rank() < 4)
            throw std::invalid_argument(
                "Thin-plate control points must span three-dimensional space");

        const auto source_min = source_points.colwise().minCoeff();
        const auto source_max = source_points.colwise().maxCoeff();
        const double scale = std::max(1.0, (source_max - source_min).norm());
        const double duplicate_tolerance = 1e-12 * scale;
        for (Eigen::Index i = 0; i < control_count; ++i)
            for (Eigen::Index j = i + 1; j < control_count; ++j)
                if ((source_points.row(i) - source_points.row(j)).norm() <=
                    duplicate_tolerance)
                    throw std::invalid_argument(
                        "Thin-plate source control points must be distinct");

        Eigen::MatrixXd system = Eigen::MatrixXd::Zero(control_count + 4, control_count + 4);
        for (Eigen::Index i = 0; i < control_count; ++i) {
            for (Eigen::Index j = i + 1; j < control_count; ++j) {
                const double distance =
                    (source_points.row(i) - source_points.row(j)).norm();
                system(i, j) = distance;
                system(j, i) = distance;
            }
        }
        system.topRightCorner(control_count, 4) = affine_points;
        system.bottomLeftCorner(4, control_count) = affine_points.transpose();
        if (options.regularization > 0.0)
            system.diagonal().head(control_count).array() +=
                options.regularization * scale;

        Eigen::MatrixXd right_hand_side = Eigen::MatrixXd::Zero(control_count + 4, 3);
        right_hand_side.topRows(control_count) = target_points;
        Eigen::FullPivLU<Eigen::MatrixXd> solver(system);
        if (solver.rank() < system.rows())
            throw std::invalid_argument(
                "Thin-plate control points produce a singular deformation system");
        const auto coefficients = solver.solve(right_hand_side);
        if (!coefficients.allFinite())
            throw std::runtime_error("Thin-plate deformation produced nonfinite coefficients");

        auto vertices = mesh.vertices();
        for (Eigen::Index i = 0; i < vertices.rows(); ++i) {
            if (options.scope == DeformationScope::local &&
                !selected[static_cast<std::size_t>(i)])
                continue;
            Eigen::VectorXd radial(control_count);
            for (Eigen::Index j = 0; j < control_count; ++j)
                radial[j] = (vertices.row(i) - source_points.row(j)).norm();
            Eigen::Vector4d affine;
            affine << 1.0, vertices(i, 0), vertices(i, 1), vertices(i, 2);
            const auto value = radial.transpose() * coefficients.topRows(control_count) +
                affine.transpose() * coefficients.bottomRows(4);
            if (!value.allFinite())
                throw std::runtime_error("Thin-plate deformation produced a nonfinite point");
            vertices.row(i) = value;
        }

        progress(options, 1.0);
        auto output = Mesh(std::move(vertices), mesh.faces());
        return result_mesh(mesh, std::move(output));
    }, "utils_3d.deform_mesh");
}

Result<AlgorithmResult<Mesh>> join_mesh_strips(
    const std::vector<Vertices> &first, const std::vector<Vertices> &second,
    const JoinStripsOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        return join_mesh_strips_impl(first, second, options);
    }, "utils_3d.join_mesh_strips");
}

Result<AlgorithmResult<Mesh>> join_mesh_strips(
    const Mesh &first, const Mesh &second, const JoinStripsOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        require_surface(first);
        require_surface(second);
        const auto first_loops = first.boundary_loops();
        const auto second_loops = second.boundary_loops();
        if (first_loops.empty() || second_loops.empty())
            throw std::invalid_argument(
                "join_with_strips requires open meshes with boundary loops");
        if (first_loops.size() != second_loops.size())
            throw std::invalid_argument(
                "join_with_strips requires the same number of boundary loops");

        const auto make_polylines = [](const Mesh &mesh,
                                       const std::vector<BoundaryLoop> &loops) {
            const auto vertices = mesh.vertices();
            std::vector<Vertices> result;
            result.reserve(loops.size());
            for (const auto &loop : loops) {
                if (loop.empty())
                    throw std::invalid_argument("Boundary loop is empty");
                Vertices polyline(static_cast<Eigen::Index>(loop.size()), 3);
                for (Eigen::Index i = 0; i < polyline.rows(); ++i) {
                    const auto index = loop[static_cast<std::size_t>(i)];
                    if (index < 0 || index >= vertices.rows())
                        throw std::out_of_range("Boundary loop contains an invalid vertex");
                    polyline.row(i) = vertices.row(static_cast<Eigen::Index>(index));
                }
                result.push_back(std::move(polyline));
            }
            return result;
        };

        auto first_lines = make_polylines(first, first_loops);
        auto second_lines = make_polylines(second, second_loops);
        return join_mesh_strips_impl(first_lines, second_lines, options);
    }, "utils_3d.join_mesh_strips");
}

Result<GraphCutResult> optimize_mesh_labels(
    const Mesh &mesh, const Matrix &probabilities, const GraphCutOptions &options) {
    return run<GraphCutResult>([&] {
        auto prepared = prepare_graph_cut(mesh, probabilities, options);
        return run_graph_cut(std::move(prepared), options);
    }, "utils_3d.optimize_mesh_labels");
}

Result<GraphCutResult> optimize_mesh_labels(
    const Mesh &mesh, const Labels &labels, const GraphCutOptions &options) {
    return run<GraphCutResult>([&] {
        require_surface(mesh);
        if (labels.size() == 0)
            throw std::invalid_argument("Graph-cut labels must be nonempty");
        if (labels.size() > static_cast<Eigen::Index>(std::numeric_limits<int>::max()))
            throw std::length_error("Graph-cut label array is too large for the internal solver");

        std::int64_t maximum_label = -1;
        for (Eigen::Index i = 0; i < labels.size(); ++i) {
            if (labels(i) < 0)
                throw std::invalid_argument("Graph-cut hard labels must be nonnegative");
            maximum_label = std::max(maximum_label, labels(i));
        }
        if (maximum_label == std::numeric_limits<std::int64_t>::max())
            throw std::invalid_argument("Graph-cut hard label is too large");
        const std::size_t inferred_count = static_cast<std::size_t>(maximum_label) + 1;
        const std::size_t class_count = options.class_count == 0
            ? inferred_count
            : options.class_count;
        if (class_count == 0 || class_count > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
            class_count > static_cast<std::size_t>(std::numeric_limits<Eigen::Index>::max()))
            throw std::invalid_argument("Graph-cut class_count is outside the supported range");
        for (Eigen::Index i = 0; i < labels.size(); ++i)
            if (static_cast<std::size_t>(labels(i)) >= class_count)
                throw std::invalid_argument("Graph-cut hard label exceeds class_count");

        Matrix probabilities(labels.size(), static_cast<Eigen::Index>(class_count));
        probabilities.setZero();
        for (Eigen::Index i = 0; i < labels.size(); ++i)
            probabilities(i, labels(i)) = 1.0;
        auto prepared = prepare_graph_cut(mesh, probabilities, options);
        return run_graph_cut(std::move(prepared), options);
    }, "utils_3d.optimize_mesh_labels");
}

Result<AlgorithmResult<Mesh>> remesh_surface(const Mesh &mesh, const RemeshOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        require_surface(mesh);
        positive(options.edge_length, "edge_length");
        if (options.iterations == 0)
            throw std::invalid_argument("iterations must be positive");
        if (options.backend == RemeshBackend::cgal) {
#if defined(SINDRE_UTILS_3D_CGAL)
            auto value = to_cgal(mesh);
            CGAL::Polygon_mesh_processing::isotropic_remeshing(
                value.faces(), options.edge_length, value,
                CGAL::parameters::number_of_iterations(options.iterations));
            auto output = from_cgal(std::move(value));
            progress(options, 1.0);
            auto result = result_mesh(mesh, std::move(output));
            result.report.iterations = options.iterations;
            return result;
#else
            return unsupported_value<AlgorithmResult<Mesh>>(
                "CGAL remeshing was requested but the CGAL backend is not enabled");
#endif
        }
        if (options.backend != RemeshBackend::vtk)
            throw std::invalid_argument("Unknown mesh remeshing backend");

        LegacyMesh input = to_legacy(mesh);
        vtkNew<vtkAdaptiveSubdivisionFilter> filter;
        filter->SetInputData(input.get_native());
        filter->SetMaximumEdgeLength(options.edge_length);
        filter->SetMaximumNumberOfPasses(static_cast<vtkIdType>(options.iterations));
        filter->SetMaximumNumberOfTriangles(std::numeric_limits<vtkIdType>::max());
        filter->Update();
        if (!filter->GetOutput())
            throw std::runtime_error("VTK remeshing returned no mesh");
        auto result = result_mesh(mesh, from_legacy(LegacyMesh(filter->GetOutput())));
        result.report.iterations = options.iterations;
        progress(options, 1.0);
        return result;
    }, "utils_3d.remesh_surface");
}

Result<AlgorithmResult<Mesh>> uniformize_mesh(const Mesh &mesh,
                                              const UniformizeOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        require_surface(mesh);
        positive(options.edge_length, "edge_length");
        if (options.iterations == 0 || !std::isfinite(options.pass_band) ||
            options.pass_band <= 0 || options.pass_band > 2 ||
            !std::isfinite(options.feature_angle) || options.feature_angle < 0 ||
            options.feature_angle > 180)
            throw std::invalid_argument("Invalid mesh uniformization options");

        if (options.backend == RemeshBackend::cgal) {
#if defined(SINDRE_UTILS_3D_CGAL)
            auto value = to_cgal(mesh);
            CGAL::Polygon_mesh_processing::isotropic_remeshing(
                value.faces(), options.edge_length, value,
                CGAL::parameters::number_of_iterations(options.iterations));
            auto output = from_cgal(std::move(value));
            auto result = result_mesh(mesh, std::move(output));
            result.report.iterations = options.iterations;
            progress(options, 1.0);
            return result;
#else
            return unsupported_value<AlgorithmResult<Mesh>>(
                "CGAL uniformization was requested but the CGAL backend is not enabled");
#endif
        }
        if (options.backend != RemeshBackend::vtk)
            throw std::invalid_argument("Unknown mesh uniformization backend");

        LegacyMesh input = to_legacy(mesh);
        vtkNew<vtkAdaptiveSubdivisionFilter> subdivide;
        subdivide->SetInputData(input.get_native());
        subdivide->SetMaximumEdgeLength(options.edge_length);
        subdivide->SetMaximumNumberOfPasses(static_cast<vtkIdType>(options.iterations));
        subdivide->SetMaximumNumberOfTriangles(std::numeric_limits<vtkIdType>::max());
        subdivide->Update();
        if (!subdivide->GetOutput())
            throw std::runtime_error("VTK uniformization subdivision returned no mesh");
        check_limit(options, static_cast<std::size_t>(
                                subdivide->GetOutput()->GetNumberOfPoints()));

        vtkSmartPointer<vtkPolyData> native_output = subdivide->GetOutput();
        if (options.smooth) {
            vtkNew<vtkWindowedSincPolyDataFilter> smooth;
            smooth->SetInputData(native_output);
            smooth->SetNumberOfIterations(static_cast<int>(options.iterations));
            smooth->SetPassBand(options.pass_band);
            smooth->SetFeatureAngle(options.feature_angle);
            smooth->FeatureEdgeSmoothingOff();
            smooth->NonManifoldSmoothingOff();
            if (options.preserve_boundary)
                smooth->BoundarySmoothingOff();
            else
                smooth->BoundarySmoothingOn();
            smooth->NormalizeCoordinatesOn();
            smooth->Update();
            if (!smooth->GetOutput())
                throw std::runtime_error("VTK uniformization smoothing returned no mesh");
            native_output = smooth->GetOutput();
        }

        auto result = result_mesh(mesh, from_legacy(LegacyMesh(native_output)));
        result.report.iterations = options.iterations;
        progress(options, 1.0);
        return result;
    }, "utils_3d.uniformize_mesh");
}

Result<BooleanPreflight> check_boolean_mesh(const Mesh &left, const Mesh &right,
                                            BooleanOperation operation,
                                            const AlgorithmOptions &options) {
    return run<BooleanPreflight>([&] {
        BooleanPreflight report;
        check_cancel(options);
        if (static_cast<int>(operation) < static_cast<int>(BooleanOperation::unite) ||
            static_cast<int>(operation) > static_cast<int>(BooleanOperation::subtract)) {
            report.reason = "Unknown boolean operation";
            return report;
        }

        std::string reason;
        if (!boolean_geometry_ready(left, report.left_quality, reason)) {
            report.reason = "Left mesh is not boolean-ready: " + reason;
            return report;
        }
        if (!boolean_geometry_ready(right, report.right_quality, reason)) {
            report.reason = "Right mesh is not boolean-ready: " + reason;
            return report;
        }
        report.bounds_overlap = mesh_bounds_overlap(left, right);

#if defined(SINDRE_UTILS_3D_CGAL)
        report.supported = true;
        try {
            auto left_native = to_cgal(left);
            report.left_self_intersections_checked = true;
            report.left_self_intersecting =
                CGAL::Polygon_mesh_processing::does_self_intersect(left_native);
            if (report.left_self_intersecting) {
                report.reason = "Left mesh contains self intersections";
                return report;
            }

            auto right_native = to_cgal(right);
            report.right_self_intersections_checked = true;
            report.right_self_intersecting =
                CGAL::Polygon_mesh_processing::does_self_intersect(right_native);
            if (report.right_self_intersecting) {
                report.reason = "Right mesh contains self intersections";
                return report;
            }
        } catch (const std::exception &error) {
            report.reason = std::string("Boolean backend rejected the input topology: ") +
                            error.what();
            return report;
        }
#else
        report.reason = "Boolean operations require the CGAL backend in this build";
        return report;
#endif

        report.disjoint_shortcut = !report.bounds_overlap;
        report.can_execute = true;
        report.reason = report.disjoint_shortcut
            ? "Boolean preflight passed; disjoint bounds will use a deterministic shortcut"
            : "Boolean preflight passed";
        progress(options, 1.0);
        return report;
    }, "utils_3d.check_boolean_mesh");
}

Result<AlgorithmResult<Mesh>> boolean_mesh(const Mesh &a, const Mesh &b,
                                           BooleanOperation operation,
                                           const AlgorithmOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        auto preflight = check_boolean_mesh(a, b, operation, options);
        if (!preflight)
            throw std::runtime_error(preflight.error().describe());
        const auto &check = preflight.value();
        if (!check.supported)
            throw unsupported_error(check.reason.c_str());
        if (!check.can_execute)
            throw std::invalid_argument(check.reason);

        auto complete_report = [&](Mesh output) {
            auto result = result_mesh(a, std::move(output));
            result.report.input_points = static_cast<std::size_t>(a.npoints()) +
                                         static_cast<std::size_t>(b.npoints());
            result.report.input_faces = static_cast<std::size_t>(a.nfaces()) +
                                        static_cast<std::size_t>(b.nfaces());
            return result;
        };
        if (check.disjoint_shortcut) {
            if (operation == BooleanOperation::unite)
                return complete_report(concatenate_meshes(a, b));
            if (operation == BooleanOperation::intersect)
                return complete_report(Mesh());
            return complete_report(a.clone());
        }
#if defined(SINDRE_UTILS_3D_CGAL)
        auto left = to_cgal(a);
        auto right = to_cgal(b);
        CgalMesh output;
        namespace pmp = CGAL::Polygon_mesh_processing;
        bool ok = false;
        if (operation == BooleanOperation::unite)
            ok = pmp::corefine_and_compute_union(left, right, output);
        else if (operation == BooleanOperation::intersect)
            ok = pmp::corefine_and_compute_intersection(left, right, output);
        else
            ok = pmp::corefine_and_compute_difference(left, right, output);
        if (!ok)
            throw std::runtime_error("Boolean operation failed");
        auto mesh = from_cgal(std::move(output));
        progress(options, 1.0);
        return complete_report(std::move(mesh));
#else
        return unsupported_value<AlgorithmResult<Mesh>>(
            "Boolean operations are not enabled in this build");
#endif
    }, "utils_3d.boolean_mesh");
}

Result<AlgorithmResult<Mesh>> clip_mesh_by_curve(
    const Mesh &mesh, const Vertices &curve, const CurveClipOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        require_surface(mesh);
        if (curve.rows() < 3 || curve.cols() != 3)
            throw std::invalid_argument(
                "A closed clipping curve requires at least three 3D points");
        if (!curve.allFinite())
            throw std::invalid_argument("Clipping curve must contain finite points");
        if (!std::isfinite(options.max_projection_distance) ||
            options.max_projection_distance < 0.0)
            throw std::invalid_argument(
                "max_projection_distance must be finite and non-negative");
        if (options.region != CurveClipRegion::inside &&
            options.region != CurveClipRegion::outside)
            throw std::invalid_argument("Unknown curve clipping region");
        if (options.selection != CurveSelectionMode::smallest_region &&
            options.selection != CurveSelectionMode::largest_region)
            throw std::invalid_argument("Unknown curve selection mode");

        auto projected = project_mesh_points(mesh, curve, options);
        if (!projected)
            throw std::invalid_argument(projected.error().describe());
        for (Eigen::Index i = 0; i < projected.value().distances.size(); ++i) {
            if (options.max_projection_distance > 0.0 &&
                projected.value().distances[i] > options.max_projection_distance)
                throw std::invalid_argument(
                    "Clipping curve is farther from the mesh than the configured projection limit");
        }

        // vtkSelectPolyData closes the loop between the last and first point.
        // Remove an explicitly repeated endpoint and consecutive projected
        // duplicates to avoid zero-length graph edges on coarse meshes.
        const double point_tolerance =
            std::numeric_limits<double>::epsilon() * 1024.0 *
            std::max(1.0, mesh.radius());
        std::vector<::sindre::math::Vector3> unique_loop_points;
        unique_loop_points.reserve(static_cast<std::size_t>(projected.value().points.rows()));
        for (Eigen::Index i = 0; i < projected.value().points.rows(); ++i) {
            const auto point = projected.value().points.row(i).transpose();
            if (unique_loop_points.empty() ||
                (point - unique_loop_points.back()).norm() > point_tolerance)
                unique_loop_points.push_back(point);
        }
        if (unique_loop_points.size() >= 2 &&
            (unique_loop_points.front() - unique_loop_points.back()).norm() <=
                point_tolerance)
            unique_loop_points.pop_back();
        if (unique_loop_points.size() < 3)
            throw std::invalid_argument("A closed clipping curve requires three distinct points");
        Vertices loop_points(static_cast<Index>(unique_loop_points.size()), 3);
        for (std::size_t i = 0; i < unique_loop_points.size(); ++i)
            loop_points.row(static_cast<Index>(i)) = unique_loop_points[i].transpose();

        LegacyMesh input = to_legacy(mesh);
        vtkNew<vtkPoints> loop;
        loop->SetDataTypeToDouble();
        loop->SetNumberOfPoints(static_cast<vtkIdType>(loop_points.rows()));
        for (Eigen::Index i = 0; i < loop_points.rows(); ++i)
            loop->SetPoint(static_cast<vtkIdType>(i), loop_points.row(i).data());

        vtkNew<vtkSelectPolyData> selector;
        selector->SetInputData(input.get_native());
        selector->SetLoop(loop);
#if VTK_VERSION_NUMBER >= VTK_VERSION_CHECK(9, 2, 0)
        selector->SetEdgeSearchModeToDijkstra();
#endif
        if (options.selection == CurveSelectionMode::largest_region)
            selector->SetSelectionModeToLargestRegion();
        else
            selector->SetSelectionModeToSmallestRegion();
        // Keep the graph selection itself in the canonical inside direction.
        // For an outside crop use vtkSelectPolyData's second output instead of
        // InsideOut: this preserves the selected/unselected partition for open
        // surfaces as well as closed ones.
        selector->SetInsideOut(false);
        selector->GenerateUnselectedOutputOn();
        selector->GenerateSelectionScalarsOff();
        selector->Update();
        if (selector->GetErrorCode() || !selector->GetOutput())
            throw std::runtime_error("VTK graph-cut curve selection failed");
        auto *selected = options.region == CurveClipRegion::outside
            ? selector->GetUnselectedOutput()
            : selector->GetOutput();
        if (selected->GetNumberOfPoints() == 0 || selected->GetNumberOfPolys() == 0)
            throw std::invalid_argument(
                "The closed clipping curve does not select a nonempty mesh region");

        LegacyMesh output(selected);
        auto result = from_legacy(output);
        check_limit(options, static_cast<std::size_t>(result.npoints()));
        progress(options, 1.0);
        return result_mesh(mesh, std::move(result));
    }, "utils_3d.clip_mesh_by_curve");
}

Result<AlgorithmResult<Mesh>> fill_mesh_holes(const Mesh &mesh,
                                              const FillHolesOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        require_surface(mesh);
        check_limit(options, static_cast<std::size_t>(mesh.npoints()));
        check_cancel(options);
        if (options.method == FillHoleMethod::vtk && options.boundary_vertices.empty()) {
            LegacyMesh input = to_legacy(mesh);
            vtkNew<vtkFillHolesFilter> filter;
            filter->SetInputData(input.get_native());
            filter->SetHoleSize(options.max_hole_size == 0
                                    ? std::numeric_limits<double>::max()
                                    : static_cast<double>(options.max_hole_size));
            filter->Update();
            if (!filter->GetOutput())
                throw std::runtime_error("VTK hole filling returned no mesh");
            progress(options, 1.0);
            return result_mesh(mesh, from_legacy(LegacyMesh(filter->GetOutput())));
        }

        if (options.method != FillHoleMethod::vtk &&
            options.method != FillHoleMethod::ear_clipping)
            throw std::invalid_argument("Unknown hole filling method");

        // A supplied boundary always means targeted filling. Even when method is
        // vtk, use the deterministic VTK ear-clipping cell only for that loop;
        // vtkFillHolesFilter itself cannot target a selected boundary.
        auto available = mesh.boundary_loops();
        std::vector<BoundaryLoop> selected;
        if (!options.boundary_vertices.empty()) {
            auto requested = normalize_boundary_loop(options.boundary_vertices);
            if (requested.size() < 3)
                throw std::invalid_argument(
                    "A selected hole boundary requires at least three vertices");
            std::set<std::int64_t> unique(requested.begin(), requested.end());
            if (unique.size() != requested.size())
                throw std::invalid_argument("A selected hole boundary contains duplicate vertices");
            const auto match = std::find_if(
                available.begin(), available.end(),
                [&](const BoundaryLoop &loop) { return same_boundary_cycle(requested, loop); });
            if (match == available.end())
                throw std::invalid_argument(
                    "The selected vertices do not describe one complete boundary loop");
            if (options.max_hole_size != 0 &&
                boundary_diameter(mesh, requested) >
                    static_cast<double>(options.max_hole_size))
                throw std::invalid_argument("The selected hole exceeds max_hole_size");
            selected.push_back(std::move(requested));
        } else {
            for (const auto &loop : available) {
                if (options.max_hole_size != 0 &&
                    boundary_diameter(mesh, loop) >
                        static_cast<double>(options.max_hole_size))
                    continue;
                selected.push_back(loop);
            }
        }

        auto output = fill_boundary_loops_by_triangulation(mesh, selected);
        progress(options, 1.0);
        return result_mesh(mesh, std::move(output));
    }, "utils_3d.fill_mesh_holes");
}

Result<AlgorithmResult<Mesh>> fill_holes_by_cgal(
    const Mesh &mesh, const CgalFillHolesOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        require_surface(mesh);
#if !defined(SINDRE_UTILS_3D_CGAL)
        (void)options;
        return unsupported_value<AlgorithmResult<Mesh>>(
            "CGAL hole filling was requested but the CGAL backend is not enabled");
#else
        if (options.max_hole_diameter < 0.0 ||
            !std::isfinite(options.max_hole_diameter))
            throw std::invalid_argument(
                "max_hole_diameter must be finite and non-negative");
        if (!std::isfinite(options.threshold_distance))
            throw std::invalid_argument("threshold_distance must be finite");
        if (!std::isfinite(options.density_control_factor) ||
            options.density_control_factor <= 0.0)
            throw std::invalid_argument(
                "density_control_factor must be finite and positive");
        if (options.fairing_continuity > 2)
            throw std::invalid_argument("fairing_continuity must be 0, 1 or 2");
        if (options.method != CgalHoleFillMethod::triangulate &&
            options.method != CgalHoleFillMethod::triangulate_and_refine &&
            options.method != CgalHoleFillMethod::triangulate_refine_and_fair)
            throw std::invalid_argument("Unknown CGAL hole filling method");

        const auto quality = mesh.check();
        if (quality.degenerate_faces != 0 || quality.duplicate_vertices != 0 ||
            quality.unused_vertices != 0 || quality.non_manifold_edges != 0)
            throw std::invalid_argument(
                "CGAL hole filling requires a clean manifold triangle mesh");

        BoundaryLoop requested_boundary;
        if (!options.boundary_vertices.empty()) {
            requested_boundary = normalize_boundary_loop(options.boundary_vertices);
            if (requested_boundary.size() < 3)
                throw std::invalid_argument(
                    "A selected CGAL hole boundary requires at least three vertices");
            std::set<std::int64_t> unique(requested_boundary.begin(), requested_boundary.end());
            if (unique.size() != requested_boundary.size())
                throw std::invalid_argument(
                    "A selected CGAL hole boundary contains duplicate vertices");
            const auto available = mesh.boundary_loops();
            const auto match = std::find_if(
                available.begin(), available.end(), [&](const BoundaryLoop &loop) {
                    return same_boundary_cycle(requested_boundary, loop);
                });
            if (match == available.end())
                throw std::invalid_argument(
                    "The selected vertices do not describe one complete mesh boundary");
        }

        auto native = to_cgal(mesh);
        const auto holes = collect_cgal_holes(native);
        if (holes.empty()) {
            progress(options, 1.0);
            return result_mesh(mesh, mesh.clone());
        }

        std::size_t eligible_holes = 0;
        for (const auto &hole : holes) {
            if (!requested_boundary.empty() &&
                !same_boundary_cycle(requested_boundary, hole.vertices))
                continue;
            if (options.max_hole_edges != 0 &&
                hole.edge_count > options.max_hole_edges)
                continue;
            if (options.max_hole_diameter > 0.0 &&
                hole.diameter > options.max_hole_diameter)
                continue;
            ++eligible_holes;
        }
        if (!requested_boundary.empty() && eligible_holes == 0)
            throw std::invalid_argument(
                "The selected CGAL hole exceeds the configured limits");
        if (eligible_holes == 0) {
            progress(options, 1.0);
            return result_mesh(mesh, mesh.clone());
        }

        std::size_t filled_holes = 0;
        for (const auto &hole : holes) {
            check_cancel(options);
            if (!requested_boundary.empty() &&
                !same_boundary_cycle(requested_boundary, hole.vertices))
                continue;
            if (options.max_hole_edges != 0 &&
                hole.edge_count > options.max_hole_edges)
                continue;
            if (options.max_hole_diameter > 0.0 &&
                hole.diameter > options.max_hole_diameter)
                continue;

            std::vector<CgalMesh::Face_index> patch_faces;
            std::vector<CgalMesh::Vertex_index> patch_vertices;
            bool successful = false;
            auto fill_with_parameters = [&](const auto &geometry_parameters) {
                if (options.method == CgalHoleFillMethod::triangulate) {
                    const auto parameters = geometry_parameters.face_output_iterator(
                        std::back_inserter(patch_faces));
                    (void)CGAL::Polygon_mesh_processing::triangulate_hole(
                        native, hole.border, parameters);
                    successful = !patch_faces.empty();
                } else if (options.method == CgalHoleFillMethod::triangulate_and_refine) {
                    const auto parameters = geometry_parameters
                        .face_output_iterator(std::back_inserter(patch_faces))
                        .vertex_output_iterator(std::back_inserter(patch_vertices))
                        .density_control_factor(options.density_control_factor);
                    (void)CGAL::Polygon_mesh_processing::triangulate_and_refine_hole(
                        native, hole.border, parameters);
                    successful = !patch_faces.empty();
                } else {
                    const auto parameters = geometry_parameters
                        .face_output_iterator(std::back_inserter(patch_faces))
                        .vertex_output_iterator(std::back_inserter(patch_vertices))
                        .density_control_factor(options.density_control_factor)
                        .fairing_continuity(options.fairing_continuity);
                    const auto result =
                        CGAL::Polygon_mesh_processing::triangulate_refine_and_fair_hole(
                            native, hole.border, parameters);
                    successful = std::get<0>(result) && !patch_faces.empty();
                }
            };

            auto geometry_parameters = CGAL::parameters::use_delaunay_triangulation(
                options.use_delaunay_triangulation)
                .use_2d_constrained_delaunay_triangulation(
                    options.use_2d_constrained_delaunay_triangulation)
                .do_not_use_cubic_algorithm(options.do_not_use_cubic_algorithm);
            if (options.threshold_distance >= 0.0)
                fill_with_parameters(
                    geometry_parameters.threshold_distance(options.threshold_distance));
            else
                fill_with_parameters(geometry_parameters);

            if (!successful)
                throw std::runtime_error("CGAL could not fill the selected mesh hole");
            ++filled_holes;
            check_limit(options, static_cast<std::size_t>(native.number_of_vertices()));
            progress(options, static_cast<double>(filled_holes) /
                                 static_cast<double>(eligible_holes));
        }

        auto output = from_cgal(std::move(native));
        progress(options, 1.0);
        return result_mesh(mesh, std::move(output));
#endif
    }, "utils_3d.fill_holes_by_cgal");
}

Result<bool> has_mesh_self_intersections(const Mesh &mesh, const AlgorithmOptions &options) {
    return run<bool>([&] {
        require_surface(mesh);
#if defined(SINDRE_UTILS_3D_CGAL)
        const auto value = to_cgal(mesh);
        progress(options, 1.0);
        return CGAL::Polygon_mesh_processing::does_self_intersect(value);
#else
        (void)options;
        return unsupported_value<bool>(
            "Self-intersection detection is not enabled in this build");
#endif
    }, "utils_3d.has_mesh_self_intersections");
}

Result<AlgorithmResult<Mesh>> subdivide_mesh(const Mesh &mesh, int iterations,
                                             const AlgorithmOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        require_surface(mesh);
        if (iterations < 1)
            throw std::invalid_argument("Subdivision iterations must be positive");
        LegacyMesh input = to_legacy(mesh);
        vtkNew<vtkLoopSubdivisionFilter> filter;
        filter->SetInputData(input.get_native());
        filter->SetNumberOfSubdivisions(iterations);
        filter->Update();
        if (!filter->GetOutput())
            throw std::runtime_error("VTK subdivision returned no mesh");
        check_limit(options, static_cast<std::size_t>(
                                filter->GetOutput()->GetNumberOfPoints()));
        progress(options, 1.0);
        auto result = result_mesh(mesh, from_legacy(LegacyMesh(filter->GetOutput())));
        result.report.iterations = static_cast<std::size_t>(iterations);
        return result;
    }, "utils_3d.subdivide_mesh");
}

Result<AlgorithmResult<Mesh>> subdivide_mesh_faces(
    const Mesh &mesh, const std::vector<std::int64_t> &face_indices,
    int iterations, const AlgorithmOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        require_surface(mesh);
        if (iterations < 1 || iterations > 10)
            throw std::invalid_argument("Local subdivision iterations must lie in [1,10]");
        if (face_indices.empty())
            throw std::invalid_argument("At least one face must be selected");

        std::vector<std::int64_t> selected = face_indices;
        std::sort(selected.begin(), selected.end());
        if (std::adjacent_find(selected.begin(), selected.end()) != selected.end())
            throw std::invalid_argument("Selected face indices must be unique");
        for (const auto face_id : selected)
            if (face_id < 0 || face_id >= mesh.nfaces())
                throw std::out_of_range("Selected face index is outside the mesh range");

        Mesh current = mesh.clone();
        for (int iteration = 0; iteration < iterations; ++iteration) {
            auto step = subdivide_selected_faces_once(current, selected, options);
            current = std::move(step.mesh);
            selected = std::move(step.selected_faces);
            progress(options, static_cast<double>(iteration + 1) /
                                 static_cast<double>(iterations));
            if (selected.empty())
                break;
        }
        auto result = result_mesh(mesh, std::move(current));
        result.report.iterations = static_cast<std::size_t>(iterations);
        return result;
    }, "utils_3d.subdivide_mesh_faces");
}

Result<AlgorithmResult<Mesh>> cut_mesh_plane(const Mesh &mesh,
                                             const ::sindre::math::Vector3 &origin,
                                             const ::sindre::math::Vector3 &normal,
                                             bool keep_negative,
                                             const AlgorithmOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        require_surface(mesh);
        if (!origin.allFinite() || !normal.allFinite() || normal.norm() == 0)
            throw std::invalid_argument("Invalid cutting plane");
        LegacyMesh input = to_legacy(mesh);
        vtkNew<vtkPlane> plane;
        plane->SetOrigin(origin.data());
        plane->SetNormal(normal.normalized().eval().data());
        vtkNew<vtkClipPolyData> filter;
        filter->SetInputData(input.get_native());
        filter->SetClipFunction(plane);
        filter->SetInsideOut(keep_negative);
        filter->Update();
        progress(options, 1.0);
        return result_mesh(mesh, from_legacy(LegacyMesh(filter->GetOutput())));
    }, "utils_3d.cut_mesh_plane");
}

Result<AlgorithmResult<Mesh>> reverse_mesh_faces(const Mesh &mesh,
                                                 const AlgorithmOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        require_surface(mesh);
        LegacyMesh input = to_legacy(mesh);
        vtkNew<vtkReverseSense> filter;
        filter->SetInputData(input.get_native());
        filter->ReverseCellsOn();
        filter->ReverseNormalsOn();
        filter->Update();
        progress(options, 1.0);
        return result_mesh(mesh, from_legacy(LegacyMesh(filter->GetOutput())));
    }, "utils_3d.reverse_mesh_faces");
}

Result<AlgorithmResult<Mesh>> compute_mesh_normals(const Mesh &mesh,
                                                   const MeshNormals &normal_options,
                                                   const AlgorithmOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        require_surface(mesh);
        if (!std::isfinite(normal_options.feature_angle) ||
            normal_options.feature_angle < 0 || normal_options.feature_angle > 180)
            throw std::invalid_argument("Normal feature angle must lie in [0,180]");
        LegacyMesh output = to_legacy(mesh);
        output.compute_normals(normal_options);
        progress(options, 1.0);
        return result_mesh(mesh, from_legacy(output));
    }, "utils_3d.compute_mesh_normals");
}

Result<ProjectionResult> project_mesh_points(const Mesh &mesh, const Vertices &query,
                                             const AlgorithmOptions &options) {
    return run<ProjectionResult>([&] {
        require_surface(mesh);
        if (!query.allFinite())
            throw std::invalid_argument("Projection query must be finite");
        LegacyMesh input = to_legacy(mesh);
        vtkNew<vtkStaticCellLocator> locator;
        locator->SetDataSet(input.get_native());
        locator->BuildLocator();
        ProjectionResult output;
        output.points.resize(query.rows(), 3);
        output.distances.resize(query.rows());
        output.face_ids.resize(query.rows());
        for (Eigen::Index i = 0; i < query.rows(); ++i) {
            double closest[3], pcoords[3], weights[3];
            vtkIdType cell = -1;
            int sub = 0;
            double distance = 0;
            locator->FindClosestPoint(query.row(i).data(), closest, cell, sub, distance);
            output.points.row(i) << closest[0], closest[1], closest[2];
            output.distances[i] = std::sqrt(std::max(0.0, distance));
            output.face_ids[i] = cell;
            (void)pcoords;
            (void)weights;
        }
        progress(options, 1.0);
        return output;
    }, "utils_3d.project_mesh_points");
}

Result<Vertices> project_mesh_line(const Mesh &mesh, const Vertices &ordered_points,
                                   const AlgorithmOptions &options) {
    return run<Vertices>([&] {
        if (ordered_points.rows() < 2)
            throw std::invalid_argument("A projected line requires at least two points");
        auto projected = project_mesh_points(mesh, ordered_points, options);
        if (!projected)
            throw std::runtime_error(projected.error().describe());
        return std::move(projected.value().points);
    }, "utils_3d.project_mesh_line");
}

Result<PathResult> find_mesh_path(const Mesh &mesh, std::int64_t start_vertex,
                                  std::int64_t end_vertex, const PathOptions &options) {
    return run<PathResult>([&] {
        require_surface(mesh);
        if (start_vertex < 0 || end_vertex < 0 || start_vertex >= mesh.npoints() ||
            end_vertex >= mesh.npoints())
            throw std::out_of_range("Path vertex index is outside the vertex range");
        if (options.cache) {
            if (!options.cache->matches(mesh))
                throw std::invalid_argument("Path cache belongs to a different mesh");
            auto cached = options.cache->find_path(start_vertex, end_vertex, options);
            if (!cached)
                throw std::runtime_error(cached.error().describe());
            return cached.value();
        }

        LegacyMesh input = to_legacy(mesh);
        vtkNew<vtkDijkstraGraphGeodesicPath> dijkstra;
        dijkstra->SetInputData(input.get_native());
        dijkstra->SetStartVertex(static_cast<vtkIdType>(start_vertex));
        dijkstra->SetEndVertex(static_cast<vtkIdType>(end_vertex));
        dijkstra->StopWhenEndReachedOn();
        dijkstra->Update();
        auto ids = dijkstra->GetIdList();
        if (!ids || ids->GetNumberOfIds() == 0)
            throw std::runtime_error("No path exists between the requested vertices");

        const auto vertices = mesh.vertices();
        std::vector<vtkIdType> path_ids;
        path_ids.reserve(static_cast<std::size_t>(ids->GetNumberOfIds()));
        for (vtkIdType i = 0; i < ids->GetNumberOfIds(); ++i)
            path_ids.push_back(ids->GetId(i));
        if (path_ids.front() == end_vertex && path_ids.back() == start_vertex)
            std::reverse(path_ids.begin(), path_ids.end());
        if (path_ids.front() != start_vertex || path_ids.back() != end_vertex)
            throw std::runtime_error("VTK path endpoints do not match the request");
        PathResult output;
        output.vertices.reserve(path_ids.size());
        output.points.resize(static_cast<Index>(path_ids.size()), 3);
        for (std::size_t i = 0; i < path_ids.size(); ++i) {
            const auto id = path_ids[i];
            if (id < 0 || id >= vertices.rows())
                throw std::runtime_error("VTK path returned an invalid vertex index");
            output.vertices.push_back(static_cast<std::int64_t>(id));
            output.points.row(static_cast<Index>(i)) = vertices.row(id);
            if (i > 0)
                output.length += (vertices.row(id) - vertices.row(path_ids[i - 1])).norm();
        }
        progress(options, 1.0);
        return output;
    }, "utils_3d.find_mesh_path");
}

Result<::sindre::math::VectorXd> calculate_signed_distances(
    const Mesh &mesh, const Vertices &query, const AlgorithmOptions &options) {
    return run<::sindre::math::VectorXd>([&] {
        require_surface(mesh);
        if (!query.allFinite())
            throw std::invalid_argument("Distance query must be finite");
        LegacyMesh input = to_legacy(mesh);
        vtkNew<vtkImplicitPolyDataDistance> distance;
        distance->SetInput(input.get_native());
        ::sindre::math::VectorXd output(query.rows());
        for (Eigen::Index i = 0; i < query.rows(); ++i) {
            double point[3] = {query(i, 0), query(i, 1), query(i, 2)};
            output[i] = distance->EvaluateFunction(point);
        }
        progress(options, 1.0);
        return output;
    }, "utils_3d.calculate_signed_distances");
}

Result<::sindre::math::VectorXd> calculate_mesh_curvature(
    const Mesh &mesh, CurvatureType type, const CurvatureOptions &options) {
    return run<::sindre::math::VectorXd>([&] {
        require_surface(mesh);
        check_limit(options, static_cast<std::size_t>(mesh.npoints()));
        LegacyMesh input = to_legacy(mesh);
        auto output = input.get_curvature(type);
        progress(options, 1.0);
        return output;
    }, "utils_3d.calculate_mesh_curvature");
}

Result<::sindre::math::VectorXd> get_curvature_by_cgal(
    const Mesh &mesh, CurvatureType type, const CurvatureOptions &options) {
    return run<::sindre::math::VectorXd>([&] {
        require_surface(mesh);
#if !defined(SINDRE_UTILS_3D_CGAL)
        (void)type;
        (void)options;
        return unsupported_value<::sindre::math::VectorXd>(
            "CGAL curvature computation was requested but the CGAL backend is not enabled");
#else
#if defined(SINDRE_UTILS_3D_HAS_CGAL_CURVATURE)
        if (!std::isfinite(options.ball_radius))
            throw std::invalid_argument("ball_radius must be finite");
        switch (type) {
        case CurvatureType::mean:
        case CurvatureType::gaussian:
        case CurvatureType::minimum_principal:
        case CurvatureType::maximum_principal:
            break;
        default:
            throw std::invalid_argument("Unknown curvature type");
        }
        const auto quality = mesh.check();
        if (quality.degenerate_faces != 0 || quality.duplicate_vertices != 0 ||
            quality.unused_vertices != 0 || quality.non_manifold_edges != 0)
            throw std::invalid_argument(
                "CGAL curvature requires a clean manifold triangle mesh");
        check_limit(options, static_cast<std::size_t>(mesh.npoints()));

        auto native = to_cgal(mesh);
        namespace pmp = CGAL::Polygon_mesh_processing;
        using Principal = pmp::Principal_curvatures_and_directions<Kernel>;
        const Principal default_principal(
            Kernel::FT(0), Kernel::FT(0), Kernel::Vector_3(0, 0, 0),
            Kernel::Vector_3(0, 0, 0));
        auto mean_map = native.add_property_map<CgalMesh::Vertex_index, Kernel::FT>(
            "sindre_mean_curvature", Kernel::FT(0)).first;
        auto gaussian_map = native.add_property_map<CgalMesh::Vertex_index, Kernel::FT>(
            "sindre_gaussian_curvature", Kernel::FT(0)).first;
        auto principal_map = native.add_property_map<CgalMesh::Vertex_index, Principal>(
            "sindre_principal_curvatures", default_principal).first;

        auto compute = [&](const auto &parameters) {
            pmp::interpolated_corrected_curvatures(native, parameters);
        };
        auto parameters = CGAL::parameters::vertex_mean_curvature_map(mean_map)
            .vertex_Gaussian_curvature_map(gaussian_map)
            .vertex_principal_curvatures_and_directions_map(principal_map);
        if (options.ball_radius >= 0.0)
            compute(parameters.ball_radius(options.ball_radius));
        else
            compute(parameters);

        ::sindre::math::VectorXd output(mesh.npoints());
        for (const auto vertex : native.vertices()) {
            const auto id = static_cast<Eigen::Index>(vertex.idx());
            const auto principal = get(principal_map, vertex);
            switch (type) {
            case CurvatureType::mean:
                output[id] = CGAL::to_double(get(mean_map, vertex));
                break;
            case CurvatureType::gaussian:
                output[id] = CGAL::to_double(get(gaussian_map, vertex));
                break;
            case CurvatureType::minimum_principal:
                output[id] = CGAL::to_double(principal.min_curvature);
                break;
            case CurvatureType::maximum_principal:
                output[id] = CGAL::to_double(principal.max_curvature);
                break;
            default:
                throw std::invalid_argument("Unknown curvature type");
            }
        }
        progress(options, 1.0);
        return output;
#else
        (void)type;
        (void)options;
        return unsupported_value<::sindre::math::VectorXd>(
            "This CGAL version does not provide polygon mesh curvature support");
#endif
#endif
    }, "utils_3d.get_curvature_by_cgal");
}

Result<Vertices> sample_mesh_surface(const Mesh &mesh, std::size_t count,
                                     const SampleOptions &options) {
    return run<Vertices>([&] {
        require_surface(mesh);
        if (count == 0 || count > static_cast<std::size_t>(std::numeric_limits<Index>::max()))
            throw std::invalid_argument("Sample count must be positive and representable");
        check_limit(options, count);
        const auto vertices = mesh.vertices();
        const auto faces = mesh.faces();
        const auto areas = mesh.faces_area();
        std::vector<double> cumulative(static_cast<std::size_t>(areas.size()));
        double total_area = 0.0;
        for (Eigen::Index i = 0; i < areas.size(); ++i) {
            if (!std::isfinite(areas[i]) || areas[i] < 0)
                throw std::invalid_argument("Mesh contains invalid face areas");
            total_area += areas[i];
            cumulative[static_cast<std::size_t>(i)] = total_area;
        }
        if (!std::isfinite(total_area) || total_area <= 0)
            throw std::invalid_argument("Mesh surface area must be positive");

        auto sample_one = [&](double area_unit, double bary_unit) {
            const auto it = std::lower_bound(cumulative.begin(), cumulative.end(),
                                             area_unit * total_area);
            const auto face_id = static_cast<std::size_t>(
                std::min<std::ptrdiff_t>(std::distance(cumulative.begin(), it),
                                         cumulative.size() - 1));
            const auto face = faces.row(static_cast<Index>(face_id));
            const double u = std::sqrt(std::clamp(bary_unit, 0.0, 1.0));
            const double v = std::fmod(bary_unit * 0.7548776662466927 + 0.1732050807568877, 1.0);
            return (1 - u) * vertices.row(face[0]) +
                   u * ((1 - v) * vertices.row(face[1]) + v * vertices.row(face[2]));
        };

        auto unit = [](std::size_t index, std::size_t denominator) {
            return std::fmod((static_cast<double>(index) + 0.5) /
                                 static_cast<double>(denominator) * 0.6180339887498949,
                             1.0);
        };
        std::mt19937_64 generator(options.seed);
        std::uniform_real_distribution<double> random_unit(0.0, 1.0);
        auto make_candidate = [&](std::size_t index, std::size_t denominator) {
            if (options.algorithm == SampleAlgorithm::random)
                return sample_one(random_unit(generator), random_unit(generator));
            return sample_one(unit(index, denominator), unit(index + 1, denominator));
        };

        Vertices output(static_cast<Index>(count), 3);
        if (options.algorithm == SampleAlgorithm::uniform ||
            options.algorithm == SampleAlgorithm::random) {
            for (std::size_t i = 0; i < count; ++i)
                output.row(static_cast<Index>(i)) = make_candidate(i, count);
        } else if (options.algorithm == SampleAlgorithm::farthest_point) {
            std::size_t candidates = options.candidate_count;
            if (candidates == 0)
                candidates = count > std::numeric_limits<std::size_t>::max() / 4
                    ? count : count * 4;
            if (candidates < count || candidates > 2000000)
                throw std::invalid_argument("Invalid farthest-point candidate_count");
            Vertices pool(static_cast<Index>(candidates), 3);
            for (std::size_t i = 0; i < candidates; ++i)
                pool.row(static_cast<Index>(i)) = make_candidate(i, candidates);
            std::vector<double> nearest(candidates, std::numeric_limits<double>::infinity());
            std::size_t selected = 0;
            for (std::size_t output_id = 0; output_id < count; ++output_id) {
                output.row(static_cast<Index>(output_id)) = pool.row(static_cast<Index>(selected));
                for (std::size_t candidate = 0; candidate < candidates; ++candidate) {
                    const double distance =
                        (pool.row(static_cast<Index>(candidate)) -
                         output.row(static_cast<Index>(output_id))).squaredNorm();
                    nearest[candidate] = std::min(nearest[candidate], distance);
                }
                selected = static_cast<std::size_t>(std::distance(
                    nearest.begin(), std::max_element(nearest.begin(), nearest.end())));
            }
        } else {
            throw std::invalid_argument("Unknown surface sampling algorithm");
        }
        progress(options, 1.0);
        return output;
    }, "utils_3d.sample_mesh_surface");
}

MeshPathCache::MeshPathCache() = default;
MeshPathCache::MeshPathCache(const MeshPathCache &) = default;
MeshPathCache &MeshPathCache::operator=(const MeshPathCache &) = default;
MeshPathCache::MeshPathCache(MeshPathCache &&) noexcept = default;
MeshPathCache &MeshPathCache::operator=(MeshPathCache &&) noexcept = default;
MeshPathCache::~MeshPathCache() = default;

Result<MeshPathCache> MeshPathCache::create(const Mesh &mesh) {
    return run<MeshPathCache>([&] {
        MeshPathCache cache;
        auto result = cache.reset(mesh);
        if (!result)
            throw std::runtime_error(result.error().describe());
        return cache;
    }, "utils_3d.MeshPathCache.create");
}

Result<void> MeshPathCache::reset(const Mesh &mesh) {
    return run<void>([&] {
        require_surface(mesh);
        const auto vertices = mesh.vertices();
        const auto adjacency = mesh.get_vertex_adj_list();
        if (adjacency.size() != static_cast<std::size_t>(vertices.rows()))
            throw std::runtime_error("Mesh adjacency cache has an invalid vertex count");

        auto next = std::make_shared<Impl>();
        next->vertices = vertices;
        next->fingerprint = mesh_fingerprint(mesh);
        next->adjacency.resize(adjacency.size());
        for (std::size_t i = 0; i < adjacency.size(); ++i) {
            auto &edges = next->adjacency[i];
            edges.reserve(adjacency[i].size());
            for (const auto neighbor : adjacency[i]) {
                if (neighbor < 0 || neighbor >= vertices.rows())
                    throw std::out_of_range("Mesh adjacency contains an invalid vertex index");
                const auto distance =
                    (vertices.row(static_cast<Index>(i)) -
                     vertices.row(static_cast<Index>(neighbor))).norm();
                if (!std::isfinite(distance))
                    throw std::invalid_argument("Mesh adjacency contains a nonfinite edge");
                edges.emplace_back(neighbor, distance);
            }
        }
        impl_ = std::move(next);
        return;
    }, "utils_3d.MeshPathCache.reset");
}

Result<PathResult> MeshPathCache::find_path(std::int64_t start_vertex,
                                            std::int64_t end_vertex,
                                            const AlgorithmOptions &options) const {
    return run<PathResult>([&] {
        if (!impl_ || impl_->vertices.rows() == 0)
            throw std::invalid_argument("Mesh path cache is empty");
        const auto count = static_cast<std::size_t>(impl_->vertices.rows());
        if (start_vertex < 0 || end_vertex < 0 || start_vertex >= impl_->vertices.rows() ||
            end_vertex >= impl_->vertices.rows())
            throw std::out_of_range("Path vertex index is outside the cache range");

        const double infinity = std::numeric_limits<double>::infinity();
        std::vector<double> distances(count, infinity);
        std::vector<std::int64_t> previous(count, -1);
        using QueueItem = std::pair<double, std::int64_t>;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;
        distances[static_cast<std::size_t>(start_vertex)] = 0.0;
        queue.emplace(0.0, start_vertex);
        while (!queue.empty()) {
            check_cancel(options);
            const auto [distance, vertex] = queue.top();
            queue.pop();
            if (distance != distances[static_cast<std::size_t>(vertex)])
                continue;
            if (vertex == end_vertex)
                break;
            for (const auto [neighbor, weight] :
                 impl_->adjacency[static_cast<std::size_t>(vertex)]) {
                const auto candidate = distance + weight;
                auto &known = distances[static_cast<std::size_t>(neighbor)];
                if (candidate < known) {
                    known = candidate;
                    previous[static_cast<std::size_t>(neighbor)] = vertex;
                    queue.emplace(candidate, neighbor);
                }
            }
        }
        if (!std::isfinite(distances[static_cast<std::size_t>(end_vertex)]))
            throw std::runtime_error("No path exists between the requested vertices");

        PathResult output;
        for (auto vertex = end_vertex; vertex >= 0; vertex = previous[static_cast<std::size_t>(vertex)]) {
            output.vertices.push_back(vertex);
            if (vertex == start_vertex)
                break;
        }
        if (output.vertices.empty() || output.vertices.back() != start_vertex)
            throw std::runtime_error("Path reconstruction failed");
        std::reverse(output.vertices.begin(), output.vertices.end());
        output.points.resize(static_cast<Index>(output.vertices.size()), 3);
        for (std::size_t i = 0; i < output.vertices.size(); ++i)
            output.points.row(static_cast<Index>(i)) =
                impl_->vertices.row(static_cast<Index>(output.vertices[i]));
        output.length = distances[static_cast<std::size_t>(end_vertex)];
        progress(options, 1.0);
        return output;
    }, "utils_3d.MeshPathCache.find_path");
}

bool MeshPathCache::empty() const noexcept {
    return !impl_ || impl_->vertices.rows() == 0;
}

bool MeshPathCache::matches(const Mesh &mesh) const {
    return impl_ && impl_->fingerprint == mesh_fingerprint(mesh);
}

Result<RegistrationResult> register_point_clouds(
    const PointCloud &source, const PointCloud &target,
    const RegistrationOptions &options) {
    return run<RegistrationResult>([&] {
        auto source_valid = source.validate();
        auto target_valid = target.validate();
        if (!source_valid)
            throw std::invalid_argument(source_valid.error().describe());
        if (!target_valid)
            throw std::invalid_argument(target_valid.error().describe());
        if (source.size() < 3 || target.size() < 3)
            throw std::invalid_argument("Registration requires at least three points");
        positive(options.max_correspondence_distance, "max_correspondence_distance");
        if (options.max_iterations < 1 || !options.initial.allFinite())
            throw std::invalid_argument("Invalid registration options");
#if defined(SINDRE_UTILS_3D_PCL)
        auto source_native = to_pcl(source);
        auto target_native = to_pcl(target);
        pcl::IterativeClosestPoint<PclPoint, PclPoint> icp;
        icp.setInputSource(source_native);
        icp.setInputTarget(target_native);
        icp.setMaxCorrespondenceDistance(options.max_correspondence_distance);
        icp.setMaximumIterations(options.max_iterations);
        pcl::PointCloud<PclPoint> aligned;
        icp.align(aligned, options.initial.cast<float>());
        RegistrationResult output;
        output.converged = icp.hasConverged();
        if (!output.converged)
            throw std::runtime_error("Point-cloud registration did not converge");
        output.fitness = icp.getFitnessScore();
        output.rmse = std::sqrt(std::max(0.0, output.fitness));
        output.transform = icp.getFinalTransformation().template cast<double>();
        progress(options, 1.0);
        return output;
#else
        return unsupported_value<RegistrationResult>(
            "Point-cloud registration is not enabled in this build");
#endif
    }, "utils_3d.register_point_clouds");
}

Result<AlgorithmResult<Mesh>> reconstruct_surface(
    const PointCloud &cloud, const ReconstructionOptions &options) {
    return run<AlgorithmResult<Mesh>>([&] {
        auto valid = cloud.validate();
        if (!valid)
            throw std::invalid_argument(valid.error().describe());
        if (cloud.size() < 3 || options.depth < 2 || options.depth > 16)
            throw std::invalid_argument("Invalid surface reconstruction input");
#if defined(SINDRE_UTILS_3D_PCL)
        auto native = to_pcl_normals(cloud);
        pcl::Poisson<pcl::PointNormal> poisson;
        poisson.setDepth(static_cast<int>(options.depth));
        poisson.setInputCloud(native);
        pcl::PolygonMesh output;
        poisson.reconstruct(output);
        auto mesh = from_pcl(output);
        progress(options, 1.0);
        AlgorithmResult<Mesh> result = result_mesh(Mesh(), std::move(mesh));
        result.report.input_points = cloud.size();
        result.report.output_points = result.value.npoints();
        result.report.changed_elements = result.report.output_points > result.report.input_points
            ? result.report.output_points - result.report.input_points
            : result.report.input_points - result.report.output_points;
        return result;
#else
        return unsupported_value<AlgorithmResult<Mesh>>(
            "Surface reconstruction is not enabled in this build");
#endif
    }, "utils_3d.reconstruct_surface");
}

Result<AlgorithmResult<PointCloud>> filter_point_cloud(
    const PointCloud &cloud, const PointCloudFilterOptions &options) {
    return run<AlgorithmResult<PointCloud>>([&] {
        auto valid = cloud.validate();
        if (!valid)
            throw std::invalid_argument(valid.error().describe());
        if (cloud.empty())
            throw std::invalid_argument("PointCloud filtering requires nonempty input");
        check_limit(options, cloud.size());
#if defined(SINDRE_UTILS_3D_PCL)
        auto input = to_pcl(cloud);
        PclCloud output_native;
        if (options.method == PointCloudFilter::voxel) {
            positive(options.voxel_size, "voxel_size");
            pcl::VoxelGrid<PclPoint> filter;
            filter.setInputCloud(input);
            const auto leaf = static_cast<float>(options.voxel_size);
            filter.setLeafSize(leaf, leaf, leaf);
            filter.filter(output_native);
        } else if (options.method == PointCloudFilter::statistical_outlier) {
            if (options.mean_k < 1 || !std::isfinite(options.standard_deviation) ||
                options.standard_deviation < 0)
                throw std::invalid_argument("Invalid statistical outlier options");
            pcl::StatisticalOutlierRemoval<PclPoint> filter;
            filter.setInputCloud(input);
            filter.setMeanK(options.mean_k);
            filter.setStddevMulThresh(options.standard_deviation);
            filter.filter(output_native);
        } else {
            positive(options.radius, "radius");
            if (options.minimum_neighbors < 1)
                throw std::invalid_argument("minimum_neighbors must be positive");
            pcl::RadiusOutlierRemoval<PclPoint> filter;
            filter.setInputCloud(input);
            filter.setRadiusSearch(options.radius);
            filter.setMinNeighborsInRadius(options.minimum_neighbors);
            filter.filter(output_native);
        }
        progress(options, 1.0);
        auto output = from_pcl_points(output_native);
        const auto output_size = output_native.size();
        return AlgorithmResult<PointCloud>{std::move(output),
                                           {cloud.size(), output_size, 0, 0, 1,
                                            cloud.size() > output_size
                                                ? cloud.size() - output_size
                                                : output_size - cloud.size(),
                                            0.0, true}};
#else
        return unsupported_value<AlgorithmResult<PointCloud>>(
            "Point-cloud filtering is not enabled in this build");
#endif
    }, "utils_3d.filter_point_cloud");
}

Result<AlgorithmResult<PointCloud>> estimate_point_normals(
    const PointCloud &cloud, const NormalEstimationOptions &options) {
    return run<AlgorithmResult<PointCloud>>([&] {
        auto valid = cloud.validate();
        if (!valid)
            throw std::invalid_argument(valid.error().describe());
        if (cloud.size() < 3)
            throw std::invalid_argument("Normal estimation requires at least three points");
        if (options.k_neighbors < 3 || !std::isfinite(options.radius) || options.radius < 0 ||
            !options.viewpoint.allFinite())
            throw std::invalid_argument("Invalid normal estimation options");
#if defined(SINDRE_UTILS_3D_PCL)
        auto input = to_pcl(cloud);
        pcl::NormalEstimation<PclPoint, pcl::Normal> estimator;
        estimator.setInputCloud(input);
        auto tree = pcl::search::KdTree<PclPoint>::Ptr(new pcl::search::KdTree<PclPoint>);
        estimator.setSearchMethod(tree);
        if (options.radius > 0) {
            if (!std::isfinite(options.radius))
                throw std::invalid_argument("radius must be finite");
            estimator.setRadiusSearch(options.radius);
        } else {
            estimator.setKSearch(options.k_neighbors);
        }
        if (options.orient_toward_viewpoint)
            estimator.setViewPoint(static_cast<float>(options.viewpoint.x()),
                                   static_cast<float>(options.viewpoint.y()),
                                   static_cast<float>(options.viewpoint.z()));
        pcl::PointCloud<pcl::Normal> native_normals;
        estimator.compute(native_normals);
        PointCloud output = cloud;
        output.normals = Vertices::Zero(static_cast<Index>(native_normals.size()), 3);
        for (Eigen::Index i = 0; i < output.normals->rows(); ++i) {
            const auto &normal = native_normals[static_cast<std::size_t>(i)];
            if (!std::isfinite(normal.normal_x) || !std::isfinite(normal.normal_y) ||
                !std::isfinite(normal.normal_z))
                throw std::runtime_error("Normal estimation returned a nonfinite normal");
            output.normals->row(i) << normal.normal_x, normal.normal_y, normal.normal_z;
        }
        progress(options, 1.0);
        return AlgorithmResult<PointCloud>{std::move(output),
                                           {cloud.size(), cloud.size(), 0, 0, 1, 0, 0.0, true}};
#else
        return unsupported_value<AlgorithmResult<PointCloud>>(
            "Normal estimation is not enabled in this build");
#endif
    }, "utils_3d.estimate_point_normals");
}

Result<PlaneSegmentationResult> segment_point_cloud_plane(
    const PointCloud &cloud, const PlaneSegmentationOptions &options) {
    return run<PlaneSegmentationResult>([&] {
        auto valid = cloud.validate();
        if (!valid)
            throw std::invalid_argument(valid.error().describe());
        if (cloud.size() < 3 || !std::isfinite(options.distance_threshold) ||
            options.distance_threshold <= 0 || options.max_iterations < 1)
            throw std::invalid_argument("Invalid plane segmentation options");
#if defined(SINDRE_UTILS_3D_PCL)
        auto input = to_pcl(cloud);
        pcl::SACSegmentation<PclPoint> segmentation;
        segmentation.setOptimizeCoefficients(true);
        segmentation.setModelType(pcl::SACMODEL_PLANE);
        segmentation.setMethodType(pcl::SAC_RANSAC);
        segmentation.setDistanceThreshold(options.distance_threshold);
        segmentation.setMaxIterations(options.max_iterations);
        segmentation.setInputCloud(input);
        pcl::PointIndices inliers;
        pcl::ModelCoefficients coefficients;
        segmentation.segment(inliers, coefficients);
        if (inliers.indices.empty() || coefficients.values.size() < 4)
            throw std::runtime_error("No plane was found in the point cloud");
        PlaneSegmentationResult output;
        output.labels = Labels::Constant(static_cast<Index>(cloud.size()), -1);
        for (const auto index : inliers.indices) {
            if (index >= 0 && static_cast<std::size_t>(index) < cloud.size())
                output.labels[index] = 0;
        }
        output.normal << coefficients.values[0], coefficients.values[1], coefficients.values[2];
        const auto norm = output.normal.norm();
        if (!std::isfinite(norm) || norm == 0)
            throw std::runtime_error("Plane segmentation returned an invalid normal");
        output.normal /= norm;
        output.offset = coefficients.values[3] / norm;
        output.inliers = inliers.indices.size();
        progress(options, 1.0);
        return output;
#else
        return unsupported_value<PlaneSegmentationResult>(
            "Plane segmentation is not enabled in this build");
#endif
    }, "utils_3d.segment_point_cloud_plane");
}

Result<ClusteringResult> cluster_point_cloud_euclidean(
    const PointCloud &cloud, const EuclideanClusteringOptions &options) {
    return run<ClusteringResult>([&] {
        auto valid = cloud.validate();
        if (!valid)
            throw std::invalid_argument(valid.error().describe());
        if (cloud.empty())
            throw std::invalid_argument("Clustering requires nonempty input");
        positive(options.tolerance, "tolerance");
        if (options.minimum_cluster_size == 0 ||
            (options.maximum_cluster_size != 0 &&
             options.maximum_cluster_size < options.minimum_cluster_size))
            throw std::invalid_argument("Invalid cluster size range");
#if defined(SINDRE_UTILS_3D_PCL)
        auto input = to_pcl(cloud);
        auto tree = pcl::search::KdTree<PclPoint>::Ptr(new pcl::search::KdTree<PclPoint>);
        tree->setInputCloud(input);
        pcl::EuclideanClusterExtraction<PclPoint> extraction;
        extraction.setClusterTolerance(options.tolerance);
        extraction.setMinClusterSize(static_cast<int>(options.minimum_cluster_size));
        extraction.setMaxClusterSize(static_cast<int>(options.maximum_cluster_size == 0
                                                            ? cloud.size()
                                                            : options.maximum_cluster_size));
        extraction.setSearchMethod(tree);
        extraction.setInputCloud(input);
        std::vector<pcl::PointIndices> clusters;
        extraction.extract(clusters);
        ClusteringResult output;
        output.labels = Labels::Constant(static_cast<Index>(cloud.size()), -1);
        output.cluster_count = clusters.size();
        for (std::size_t cluster = 0; cluster < clusters.size(); ++cluster)
            for (const auto index : clusters[cluster].indices)
                if (index >= 0 && static_cast<std::size_t>(index) < cloud.size())
                    output.labels[index] = static_cast<std::int64_t>(cluster);
        progress(options, 1.0);
        return output;
#else
        return unsupported_value<ClusteringResult>(
            "Point-cloud clustering is not enabled in this build");
#endif
    }, "utils_3d.cluster_point_cloud_euclidean");
}

} // namespace sindre::utils_3d
