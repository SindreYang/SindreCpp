#include <sindre/utils_3d/algorithms/point_cloud.h>

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace sindre::utils_3d;

int main() {
    PointCloud cloud;
    cloud.points.resize(8, 3);
    cloud.points << 0.00, 0.00, 0.00,
                    0.01, 0.00, 0.00,
                    0.00, 0.01, 0.00,
                    0.01, 0.01, 0.00,
                    1.00, 0.00, 0.00,
                    1.01, 0.00, 0.00,
                    1.00, 0.01, 0.00,
                    1.01, 0.01, 0.00;

    double last_progress = -1.0;
    PointCloudFilterOptions filter_options;
    filter_options.method = PointCloudFilter::voxel;
    filter_options.voxel_size = 0.1;
    filter_options.progress = [&](double value) { last_progress = value; };
    auto filtered = filter_point_cloud(cloud, filter_options);
    if (!filtered || filtered.value().value.size() != 2 || last_progress != 1.0) {
        std::cerr << "PCL voxel filtering failed\n";
        return EXIT_FAILURE;
    }

    PlaneSegmentationOptions plane_options;
    plane_options.distance_threshold = 1e-6;
    plane_options.max_iterations = 100;
    auto plane = segment_point_cloud_plane(cloud, plane_options);
    if (!plane || plane.value().inliers != cloud.size() ||
        !std::isfinite(plane.value().normal.norm()) ||
        std::abs(plane.value().normal.norm() - 1.0) > 1e-12) {
        std::cerr << "PCL plane segmentation failed\n";
        return EXIT_FAILURE;
    }

    EuclideanClusteringOptions cluster_options;
    cluster_options.tolerance = 0.05;
    cluster_options.minimum_cluster_size = 3;
    auto clusters = cluster_point_cloud_euclidean(cloud, cluster_options);
    if (!clusters || clusters.value().cluster_count != 2 ||
        clusters.value().labels.size() != static_cast<Index>(cloud.size())) {
        std::cerr << "PCL Euclidean clustering failed\n";
        return EXIT_FAILURE;
    }

    NormalEstimationOptions normal_options;
    normal_options.k_neighbors = 3;
    auto normals = estimate_point_normals(cloud, normal_options);
    if (!normals || !normals.value().value.normals ||
        normals.value().value.normals->rows() != static_cast<Index>(cloud.size()) ||
        !normals.value().value.normals->allFinite()) {
        std::cerr << "PCL normal estimation failed\n";
        return EXIT_FAILURE;
    }

    RegistrationOptions registration_options;
    registration_options.max_correspondence_distance = 0.1;
    registration_options.max_iterations = 10;
    auto registration = register_point_clouds(cloud, cloud, registration_options);
    if (!registration || !registration.value().converged ||
        !registration.value().transform.isApprox(
            ::sindre::math::Matrix4::Identity(), 1e-5)) {
        std::cerr << "PCL point-cloud registration failed\n";
        return EXIT_FAILURE;
    }

    cloud.normals = Vertices::Zero(static_cast<Index>(cloud.size()), 3);
    cloud.normals->col(2).setOnes();
    ReconstructionOptions reconstruction_options;
    reconstruction_options.depth = 3;
    auto reconstruction = reconstruct_surface(cloud, reconstruction_options);
    if (!reconstruction || reconstruction.value().value.empty()) {
        std::cerr << "PCL surface reconstruction failed\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
