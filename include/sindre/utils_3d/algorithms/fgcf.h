#pragma once

/// @file
/// @brief 快速测地曲率流（FGCF）曲线算法。

#include "../types.h"

namespace sindre::utils_3d {

/// @brief 在三角网格表面上优化曲线的 FGCF 参数。
struct FgcfOptions : AlgorithmOptions {
    bool closed = true;
    unsigned iterations = 30;
    /// 每一步的无量纲稳定系数，越小越保守。
    double time_step = 0.25;
    /// 单步位移不超过局部相邻边长的该比例。
    double max_step_fraction = 0.25;
    /// 每轮按弧长均匀重采样，避免顶点聚集。
    bool resample = true;
    double convergence_tolerance = 1e-6;
    /// 仅在闭合曲线中生效；保留曲线点数。
    bool preserve_point_count = true;
};

struct FgcfResult {
    Vertices curve;
    double initial_length = 0.0;
    double final_length = 0.0;
    unsigned iterations = 0;
    bool converged = false;
};

/// @brief 将曲线投影到网格并沿表面测地曲率流稳定收敛。
///
/// 该函数只优化曲线，不改变网格拓扑；可作为后续图切裁剪的输入。
Result<AlgorithmResult<FgcfResult>> smooth_curve_by_fgcf(
    const Mesh &, const Vertices &, const FgcfOptions & = {});

} // namespace sindre::utils_3d
