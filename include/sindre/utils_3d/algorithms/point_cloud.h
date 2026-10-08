#pragma once

/// @file
/// @brief 后端无关的点云过滤、法线估计和分割接口。

#include "../types.h"

namespace sindre::utils_3d {

enum class PointCloudFilter { voxel, statistical_outlier, radius_outlier };

struct PointCloudFilterOptions : AlgorithmOptions {
    PointCloudFilter method = PointCloudFilter::voxel;
    double voxel_size = 0.01;
    int mean_k = 30;
    double standard_deviation = 1.0;
    double radius = 0.05;
    int minimum_neighbors = 2;
};
Result<AlgorithmResult<PointCloud>> filter_point_cloud(
    const PointCloud &, const PointCloudFilterOptions & = {});

struct NormalEstimationOptions : AlgorithmOptions {
    int k_neighbors = 30;
    double radius = 0.0;
    bool orient_toward_viewpoint = false;
    ::sindre::math::Vector3 viewpoint = ::sindre::math::Vector3::Zero();
};
Result<AlgorithmResult<PointCloud>> estimate_point_normals(
    const PointCloud &, const NormalEstimationOptions & = {});

struct PlaneSegmentationOptions : AlgorithmOptions {
    double distance_threshold = 0.01;
    int max_iterations = 100;
};
struct PlaneSegmentationResult {
    Labels labels;
    ::sindre::math::Vector3 normal = ::sindre::math::Vector3::Zero();
    double offset = 0.0;
    std::size_t inliers = 0;
};
Result<PlaneSegmentationResult> segment_point_cloud_plane(
    const PointCloud &, const PlaneSegmentationOptions & = {});

struct EuclideanClusteringOptions : AlgorithmOptions {
    double tolerance = 0.02;
    std::size_t minimum_cluster_size = 10;
    std::size_t maximum_cluster_size = 0;
};
struct ClusteringResult {
    Labels labels;
    std::size_t cluster_count = 0;
};
Result<ClusteringResult> cluster_point_cloud_euclidean(
    const PointCloud &, const EuclideanClusteringOptions & = {});

struct RegistrationOptions : AlgorithmOptions {
    double max_correspondence_distance = 1.0;
    int max_iterations = 50;
    double transformation_epsilon = 1e-6;
    ::sindre::math::Matrix4 initial = ::sindre::math::Matrix4::Identity();
};
struct RegistrationResult {
    ::sindre::math::Matrix4 transform = ::sindre::math::Matrix4::Identity();
    double fitness = 0.0;
    double rmse = 0.0;
    bool converged = false;
};
Result<RegistrationResult> register_point_clouds(
    const PointCloud &, const PointCloud &, const RegistrationOptions & = {});

struct ReconstructionOptions : AlgorithmOptions { std::size_t depth = 8; };
Result<AlgorithmResult<Mesh>> reconstruct_surface(
    const PointCloud &, const ReconstructionOptions & = {});

} // namespace sindre::utils_3d
