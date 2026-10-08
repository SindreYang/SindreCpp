#pragma once

/// @file
/// @brief 独立的特征边保持和吸附平滑算法。

#include "../types.h"

namespace sindre::utils_3d {

enum class FeatureSmoothingMethod { windowed_sinc, laplacian };

/// @brief VTK 特征保持平滑参数。
struct FeatureSmoothingOptions : AlgorithmOptions {
    FeatureSmoothingMethod method = FeatureSmoothingMethod::windowed_sinc;
    int iterations = 30;
    /// Windowed-sinc 的 pass band，范围 [0, 2]。
    double pass_band = 0.1;
    /// Laplacian 的松弛系数，范围 (0, 1]。
    double relaxation = 0.1;
    bool preserve_features = true;
    bool preserve_boundary = true;
    bool preserve_non_manifold = true;
    double feature_angle = 45.0;
    double edge_angle = 15.0;
    /// 将平滑后的特征顶点投影回输入特征边，降低漂移和圆角化。
    bool snap_to_features = true;
    double snap_strength = 1.0;
    /// 0 表示按相邻特征边长度自动限制，避免错误吸附到远处边。
    double max_snap_distance = 0.0;
    bool normalize_coordinates = true;
};

/// @brief 使用 VTK 执行特征边保持、边界保护和可选特征吸附平滑。
Result<AlgorithmResult<Mesh>> smooth_mesh_features(
    const Mesh &, const FeatureSmoothingOptions & = {});

} // namespace sindre::utils_3d
