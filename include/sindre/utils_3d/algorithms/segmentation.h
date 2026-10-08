#pragma once

/// @file
/// @brief 后端隔离的高级网格分割算法。

#include "../types.h"

namespace sindre::utils_3d {

/// @brief CGAL SDF/图切网格分割参数。
struct CgalSegmentationOptions : AlgorithmOptions {
    /// SDF 射线锥角，单位为弧度。
    double cone_angle = 2.0 * 3.14159265358979323846 / 3.0;
    std::size_t number_of_rays = 25;
    std::size_t number_of_clusters = 5;
    /// CGAL 推荐范围为 [0, 1]；值越大越偏向保持区域连续性。
    double smoothing_lambda = 0.26;
    /// false 返回连通 segment id，true 返回 soft-cluster id。
    bool output_cluster_ids = false;
    /// 默认拒绝开边界网格，避免 SDF/图切结果不可预测。
    bool require_closed = true;
};

struct CgalSegmentationResult {
    Labels face_labels;
    std::size_t segment_count = 0;
    double sdf_minimum = 0.0;
    double sdf_maximum = 0.0;
};

/// @brief 使用 CGAL 的 SDF 和 alpha-expansion 图切对面片进行分割。
Result<AlgorithmResult<CgalSegmentationResult>> segment_mesh_by_cgal(
    const Mesh &, const CgalSegmentationOptions & = {});

} // namespace sindre::utils_3d
