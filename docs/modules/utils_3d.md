# Utils_3d 模块

`sindre::utils_3d` 提供后端无关的网格、点云、几何算法、配准和表面重建能力。

## 公共入口

```cpp
#include <sindre/utils_3d.h>
```

公共接口包括：

- `Mesh`：拥有值语义的三角网格；
- `PointCloud`：点、法线、颜色、强度、标签和有效性属性；
- `SindreMesh`：常用网格操作的快捷 façade；
- 网格清理、修复、简化、平滑、重网格、布尔、采样和空间查询；
- 点云验证、配准和表面重建。

公共头文件收敛为三层：

```text
sindre/utils_3d/types.h
sindre/utils_3d/sindremesh.h
sindre/utils_3d/algorithms/{mesh,point_cloud,segmentation,feature_smoothing,fgcf}.h
```

模块实现对应放在 `modules/utils_3d/src/core/` 和
`modules/utils_3d/src/algorithms/`；`modules/utils_3d/private/core/` 保留
Vedo 风格的 VTK Mesh/Data/Image 基础封装，支撑高层类型和 I/O，但这些头文件不安装。
复杂 VTK/CGAL/PCL 适配只允许出现在私有算法实现中。

所有可失败算法返回 `sindre::general::Result<T>`。

## 底层隔离

VTK、CGAL 和 PCL 只作为模块内部实现。用户代码不能看到：

- 第三方对象类型；
- 底层转换函数；
- 第三方异常和错误码。

简化算法是受控例外：通过 `SimplifyBackend::vtk`（默认）或
`SimplifyBackend::cgal` 选择后端，再通过 `SimplifyAlgorithm` 选择 VTK 的
`decimate_pro`、`quadric_decimation`、`quadric_clustering` 或 CGAL 的
`cgal_edge_collapse`；仍不会暴露 VTK/CGAL 类型。

网格常用处理统一通过 `SindreMesh` 提供：`clean()`、`fix_mesh()`、`fill_hole()`、
`sample()`、`remesh()`、`uniformize()`、`subdivide()`、`subdivide_faces()`、
`clip_curve()`、`project_line()` 和 `find_path()`。`clip_curve()` 接收闭合三维曲线，
将曲线投影到网格后使用网格边图上的 Dijkstra 路径完成区域裁剪，可选择保留内部或
外部区域；裁剪边界会吸附到网格图上。路径重复查询可使用
`MeshPathCache`，缓存会拒绝用于不同网格。

高级后端算法保持为独立自由函数，不加入 `SindreMesh`：

- `segment_mesh_by_cgal()`：CGAL SDF/图切面片分割；默认要求封闭、流形、无自交网格；
- `smooth_mesh_features()`：VTK 特征边保持、边界保护和受限特征吸附平滑；
- `smooth_curve_by_fgcf()`：在网格表面上执行快速测地曲率流，只改变输入曲线，不改变网格。

这些接口只使用 `Mesh`、`Vertices` 和公开结果类型；后端未编译时返回明确的功能不可用错误。

计数可使用完整名称 `npoints()` / `nfaces()`，也可使用快捷名称
`npoint()` / `nface()`。

包围体接口包括 `get_aabb()`、`get_obb()` 和 `get_min_sphere()`。AABB 返回最小/最大
坐标、中心和尺寸；OBB 使用点集 PCA 计算，`axes` 的列是局部正交轴，
`half_extents` 是各轴半尺寸；最小包围球返回球心和半径。空网格调用这些接口会抛出
与其他几何属性一致的参数错误。

`smooth()` 默认执行全局平滑；设置 `SmoothOptions::scope` 为
`SmoothScope::local` 后，可通过 `vertex_indices` 或 `face_indices` 指定局部区域。
局部模式只写回被选中的顶点，未选中的顶点和面片拓扑保持不变。

网格变形通过 `deform(source_points, target_points)` 提供 3D Thin-Plate Spline。
默认作用于全局；设置 `DeformationOptions::scope` 为 `DeformationScope::local` 后，
可按顶点或面片限定更新区域。控制点必须是至少 4 个不共面的有限 `N x 3` 点，
异常输入会通过 `Result` 返回错误。

`SindreMesh::join_with_strips()` 对两个开放网格的对应边界环生成 VTK triangle strips，
并转换为普通三角面。两个网格必须有相同数量的边界环，且对应边界点数一致；底层
`join_mesh_strips()` 也支持直接传入两组 `Vertices` 折线。

多标签后处理通过 `optimize_mesh_labels()` 或 `SindreMesh::optimize_labels()` 提供
类似 pygco 的 alpha-expansion / alpha-beta swap 图切优化。实现使用模块内部的二元
最大流求解器，不向用户暴露 pygco 或其他图切后端。支持顶点/面片硬标签和概率矩阵，
`GraphCutOptions::smooth_factor < 0` 自动估计几何平滑权重，`keep_label` 默认防止
类别在优化过程中塌缩。输入粒度不明确且顶点数等于面片数时，`auto_detect` 优先选择
顶点，因此生产代码应显式设置 `GraphCutLabelLevel`。

曲率接口支持 `CurvatureType::mean`、`gaussian`、`minimum_principal` 和
`maximum_principal`。`get_curvature()` 使用 VTK，`get_curvature_by_cgal()` 使用
CGAL corrected-curvature 算法，并可通过 `CurvatureOptions::ball_radius` 控制 CGAL
局部估计范围。

补洞支持默认 VTK 全局补洞和 `FillHoleMethod::ear_clipping` 指定边界补洞；指定的
`boundary_vertices` 可以是有序开放表示或首尾重复的闭合表示。

`fill_holes_by_cgal()` 提供 CGAL 高级补洞，支持纯三角化、细化、fairing、Delaunay
和近似平面约束等参数；可通过 `max_hole_edges`、`max_hole_diameter` 或指定
`boundary_vertices` 限制处理范围。CGAL 未启用时返回明确的功能不可用错误。

布尔运算通过 `check_boolean_mesh()` 返回 `BooleanPreflight` 预检报告；只有
`can_execute == true` 时才进入 CGAL 布尔内核。预检失败会返回原因，不把明显非法的
网格提交给后端。

算法会自动选择可用实现；能力不可用时返回明确的高级错误。

## 构建

```cmake
set(SINDRE_WITH_UTILS_3D ON CACHE BOOL "")
target_link_libraries(my_app PRIVATE sindre::utils_3d)
```

后端 SDK 由项目构建配置提供，应用只依赖 `sindre::utils_3d`。

## 模块日志

Utils_3d 使用 General 的 `sindre::general::log::create_logger` 创建命名 logger
`sindre.utils_3d`。用户仍通过 General 的 `init_log(...)` 统一配置控制台、轮转文件或
异步队列；Utils_3d 不创建独立 sink，也不把 spdlog 类型放进公共头。算法失败、取消和
资源限制会按错误边界记录，正常路径不输出噪声日志。

## 依赖边界

- 最低语言标准：C++17；
- 数值交换：Eigen/sindre::math；
- 正式实现后端：VTK、CGAL、PCL；
- 后端头文件和库通过 `sindre::utils_3d` 的私有实现使用；
- 不支持 Open3D、VCG、libigl、MeshLib 作为当前实现后端。

详细示例见 [Mesh 与 PointCloud 指南](../guides/mesh.md)。
