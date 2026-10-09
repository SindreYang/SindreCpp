# Mesh 与 PointCloud

`utils_3d` 的公共 API 只暴露 `Mesh`、`PointCloud`、`SindreMesh` 和稳定的算法契约。
VTK、CGAL、PCL 只作为内部实现；简化算法仅通过受控的
`SimplifyBackend` 和 `SimplifyAlgorithm` 选择策略，不暴露第三方对象、头文件或异常。

## 构建

```cmake
set(SINDRE_WITH_UTILS_3D ON CACHE BOOL "")
target_link_libraries(my_app PRIVATE sindre::utils_3d)
```

后端 SDK 由构建配置决定。简化默认使用 VTK，也可以在调用时选择已经编译的
CGAL；功能不可用时返回 `Result` 错误。

## 基础数据

```cpp
#include <sindre/utils_3d.h>

namespace u3 = sindre::utils_3d;

u3::Vertices vertices(4, 3);
vertices << 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1;

u3::Faces faces(4, 3);
faces << 0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3;

u3::Mesh mesh(vertices, faces);
auto report = mesh.check();
```

`Mesh` 使用 Eigen 双精度坐标和 `int64_t` 面索引。点云使用独立值类型：

```cpp
u3::PointCloud cloud;
cloud.points = vertices;
cloud.normals = u3::Vertices::Zero(vertices.rows(), 3);
cloud.normals->col(2).setOnes();

auto valid = cloud.validate();
if (!valid) {
    std::cerr << valid.error().describe();
}
```

点云可以附带颜色、强度、标签、有效性掩码、坐标系名称和元数据；属性长度必须与点数一致。

## 算法调用

算法输入不会被修改，失败统一通过 `Result<T>` 返回：

```cpp
u3::DecimateOptions decimate_options;
decimate_options.target_faces = 10000;

auto result = u3::decimate_mesh(mesh, decimate_options);
if (!result) {
    std::cerr << result.error().describe();
    return 1;
}

const auto &simplified = result.value().value;
const auto &quality = result.value().report;
```

当前公共算法包括：

| 领域 | 算法 |
| --- | --- |
| 网格质量 | 清理、修复、孔洞、法线、自交检查、质量报告 |
| 网格处理 | 简化、平滑、重网格、布尔、细分、平面裁剪、翻面 |
| 网格分析 | 曲率、投影、符号距离、边界、连通关系、表面采样 |
| 高级独立算法 | CGAL SDF/图切分割、VTK 特征保持平滑、FGCF 测地曲率流 |
| 点云处理 | 体素降采样、统计/半径离群点过滤、法线估计 |
| 点云分割 | RANSAC 平面分割、欧式聚类、标签输出 |
| 配准 | ICP、残差、RMSE、收敛状态 |
| 重建 | Poisson 点云重建 |

复杂算法 Options 只包含算法参数、进度回调、取消回调和点数上限；没有后端字段。

```cpp
u3::PointCloudFilterOptions filter_options;
filter_options.method = u3::PointCloudFilter::voxel;
filter_options.voxel_size = 0.02;
auto filtered = u3::filter_point_cloud(cloud, filter_options);

u3::NormalEstimationOptions normal_options;
normal_options.k_neighbors = 30;
auto with_normals = u3::estimate_point_normals(cloud, normal_options);

auto plane = u3::segment_point_cloud_plane(cloud);
auto clusters = u3::cluster_point_cloud_euclidean(cloud);
```

## SindreMesh 快速入口

`SindreMesh` 是常用网格操作的高层 façade，内部转发到同一套自由函数：

```cpp
auto loaded = u3::SindreMesh::load("scan.ply");
if (!loaded) return 1;

auto processed = loaded.and_then([](const u3::SindreMesh &mesh) {
    return mesh.clean();
}).and_then([](const u3::SindreMesh &mesh) {
    u3::SmoothOptions options;
    options.iterations = 10;
    return mesh.smooth(options);
});

if (processed) {
    auto saved = processed.value().save("cleaned.ply");
    if (!saved) return 1;
}
```

平滑默认作用于整个网格；局部平滑可以按顶点或面片选择区域。面片选择会自动
平滑这些面片的三个顶点，未选中的顶点保持不变：

```cpp
u3::SmoothOptions local;
local.scope = u3::SmoothScope::local;
local.vertex_indices = {10, 11, 12};
auto locally_smoothed = mesh.smooth(local);

u3::SmoothOptions local_faces;
local_faces.scope = u3::SmoothScope::local;
local_faces.face_indices = {4, 5};
local_faces.preserve_volume = false; // 使用普通 Laplacian 平滑
auto face_smoothed = mesh.smooth(local_faces);
```

`preserve_volume=true` 使用 VTK Windowed Sinc 平滑；设为 `false` 使用 VTK
Laplacian 平滑。全局模式不能同时传入局部顶点或面片索引。

### 高级独立算法

CGAL SDF 图切分割、VTK 特征边吸附平滑和 FGCF 曲线流不作为 `SindreMesh` 成员，
而是保持为独立算法，便于明确区分输入网格和算法结果：

```cpp
u3::CgalSegmentationOptions segmentation_options;
segmentation_options.number_of_clusters = 6;
auto segmented = u3::segment_mesh_by_cgal(mesh, segmentation_options);

u3::FeatureSmoothingOptions feature_options;
feature_options.preserve_features = true;
feature_options.preserve_boundary = true;
feature_options.snap_to_features = true;
auto smoothed = u3::smooth_mesh_features(mesh, feature_options);

u3::FgcfOptions fgcf_options;
fgcf_options.iterations = 30;
auto curve_result = u3::smooth_curve_by_fgcf(mesh, input_curve, fgcf_options);
```

`segment_mesh_by_cgal()` 对不满足 CGAL 几何前提的网格返回错误，不尝试静默修复；
`smooth_mesh_features()` 保持点数和面片拓扑不变，并限制特征吸附距离；
`smooth_curve_by_fgcf()` 每轮将曲线重新投影到网格，使用步长上限和收敛判定，适合
作为后续曲线裁剪的稳定前处理。

薄板变形使用源控制点到目标控制点的 3D Thin-Plate Spline 映射。控制点至少需要
4 个不共面的三维点：

```cpp
u3::Vertices source = mesh.vertices();
u3::Vertices target = source;
for (Eigen::Index i = 0; i < target.rows(); ++i)
    target(i, 0) += 0.25;

auto deformed = mesh.deform(source, target); // 全局变形

u3::DeformationOptions local_deformation;
local_deformation.scope = u3::DeformationScope::local;
local_deformation.vertex_indices = {0, 1, 2};
local_deformation.regularization = 1e-8; // 可选，抑制病态控制点
auto locally_deformed = mesh.deform(source, target, local_deformation);
```

局部变形也支持 `face_indices`；它会更新选中面片的三个顶点，其他顶点保持不变。
控制点重复、共面、维度不一致或产生奇异系统时返回 `Result` 错误。

## 多标签图切优化

`optimize_labels()` 提供类似 pygco 的多标签 Potts 图模型优化，但不要求用户安装或
暴露 pygco。输入可以是每个顶点/面片的硬标签，也可以是每个节点到各类别的概率矩阵；
支持 alpha-expansion 和 alpha-beta swap。负的 `smooth_factor`（默认值）根据一元概率
代价和网格几何边权自动估计，设为 `0` 可关闭邻域平滑。

```cpp
u3::GraphCutOptions graph_cut;
graph_cut.label_level = u3::GraphCutLabelLevel::vertex;
graph_cut.algorithm = u3::GraphCutAlgorithm::expansion;
graph_cut.smooth_factor = -1.0; // 自动估计；0 表示只使用一元概率
graph_cut.temperature = 1.0;
graph_cut.keep_label = true;    // 防止优化过程中类别全部塌缩

u3::Matrix probabilities(mesh.npoint(), 3);
// 每行是一个顶点属于三个类别的概率或非负得分。
probabilities << 0.9, 0.1, 0.0,
                  0.1, 0.8, 0.1,
                  0.0, 0.2, 0.8,
                  0.8, 0.1, 0.1;
auto optimized = mesh.optimize_labels(probabilities, graph_cut);
if (optimized) {
    const auto &labels = optimized.value().labels;
}

u3::Labels hard_labels = /* mesh.nface() 个面片标签 */;
graph_cut.label_level = u3::GraphCutLabelLevel::face;
graph_cut.class_count = 3;
auto optimized_faces = mesh.optimize_labels(hard_labels, graph_cut);
```

`label_level=auto_detect` 会优先选择顶点标签；当顶点数和面片数相同时，建议显式指定
粒度。所有输入校验、取消、进度回调和求解失败均通过 `Result` 处理。

常用法线计算也可直接通过 `mesh.compute_normals()` 调用；复杂分析和点云算法继续使用
同名自由函数，以便明确输入输出类型。

`SindreMesh` 不保存底层后端对象，也不提供底层对象访问接口。每个成员算法返回
`Result<SindreMesh>`，不会隐藏失败状态。

网格属性可以直接读取：

```cpp
if (mesh.has_data("Normals")) {
    auto normals = mesh.get_data("Normals");
    if (normals) {
        // normals.value() 是 Eigen 矩阵
    }
}
auto names = mesh.data_names();
auto face_labels = mesh.get_labels(false);
```

包围体可以直接获取：

```cpp
auto aabb = mesh.get_aabb();
auto obb = mesh.get_obb();
auto sphere = mesh.get_min_sphere();
// aabb.minimum / aabb.maximum
// obb.center / obb.axes / obb.half_extents
// sphere.center / sphere.radius
```

曲率支持平均曲率、高斯曲率和最小/最大主曲率：

```cpp
auto mean = mesh.get_curvature(u3::CurvatureType::mean);
auto gaussian = mesh.get_curvature(u3::CurvatureType::gaussian);
auto kmin = mesh.get_curvature(u3::CurvatureType::minimum_principal);
auto kmax = mesh.get_curvature(u3::CurvatureType::maximum_principal);
```

普通接口使用 VTK；需要 CGAL 的 corrected-curvature 算法时：

```cpp
u3::CurvatureOptions options;
options.ball_radius = 0.5; // 负数使用邻接面，0 使用 CGAL 极小半径
auto cgal_mean = mesh.get_curvature_by_cgal(
    u3::CurvatureType::mean, options);
```

CGAL 接口同样支持四种曲率类型。CGAL 是 `utils_3d` 的固定构建依赖，不能通过
开关关闭或回退到 VTK；SDK 缺失时配置阶段直接失败。

`SindreMesh` 也提供常用快捷属性：

```cpp
auto points = mesh.vertices();
auto triangles = mesh.faces();
auto normals = mesh.normals();
auto vertex_normals = mesh.vertex_normals();
auto face_normals = mesh.face_normals();
auto vertex_labels = mesh.vertices_labels();
auto face_labels = mesh.faces_labels();
```

联通体拆分使用统一返回类型：

```cpp
auto largest = mesh.split(true);   // 结果只有一个元素：最大面积联通体
auto all = mesh.split(false);      // 结果包含全部联通体
```

边界环使用相同语义：

```cpp
auto largest_boundary = mesh.boundary(true);          // 默认返回有序点索引
auto all_boundaries = mesh.boundary(false, true);     // 全部有序边界环
auto unordered = mesh.boundary(false, false);         // 全部边界点，按索引稳定排列
```

`ordered=true` 默认开启，返回的每个边界环中相邻点沿边界连续连接，首尾相连；
不保证顺时针或逆时针方向。

两组具有相同边界数量和对应点数的边界可以连接成三角带：

```cpp
u3::JoinStripsOptions strips;
strips.closed = true; // 默认闭合，连接每条边界的首尾点
auto side = lower_mesh.join_with_strips(upper_mesh, strips);
```

该接口等价于 Vedo 的 `join_with_strips()`：它按边界点的一一对应关系生成 VTK
triangle strips，最后转换为普通三角面 `Mesh`。两个输入网格必须是开放网格，并且
边界环数量、每个对应边界的点数一致；不满足时返回 `Result` 错误。也可以直接调用
`join_mesh_strips()` 传入 `std::vector<Vertices>` 折线组。

网格清理、修复和补洞：

```cpp
auto cleaned = mesh.clean();       // 合并重复点，并移除未被面片引用的顶点
auto fixed = mesh.fix_mesh();      // 清理、去退化面、去重复面
auto filled = mesh.fill_hole();    // VTK 快速补洞；也可使用 fill_holes()

u3::FillHolesOptions selected;
selected.method = u3::FillHoleMethod::ear_clipping;
selected.boundary_vertices = {0, 1, 2, 3, 0}; // 有序点；首尾可重复表示闭合
auto one_hole = mesh.fill_holes(selected);     // 只补这个边界环
```

`fix_mesh()` 默认优先使用 VTK 清理流程，再进行确定性的拓扑过滤；补洞不是默认
修复步骤，可通过 `FixOptions::fill_holes` 显式开启。`FillHoleMethod::vtk` 适合
快速补全部符合尺寸限制的洞；传入 `boundary_vertices` 时会自动切换为指定环的
耳切三角化，不会误补其他边界。指定点必须组成一个真实的完整边界环。

需要 CGAL 高级补洞时使用独立接口：

```cpp
u3::CgalFillHolesOptions options;
options.method = u3::CgalHoleFillMethod::triangulate_refine_and_fair;
options.use_2d_constrained_delaunay_triangulation = true;
options.threshold_distance = 0.01;
options.density_control_factor = 1.41421356237;
options.fairing_continuity = 1; // C0/C1/C2，只允许 0/1/2
options.max_hole_edges = 500;   // 0 表示不限制

auto filled = mesh.fill_holes_by_cgal(options);
```

CGAL 支持纯三角化、三角化加细化、三角化加细化和 fairing 三种模式，也支持
Delaunay、近似平面约束、平面阈值、密度和 fairing 连续性参数。传入
`boundary_vertices` 可只处理一个完整边界环。CGAL 是固定依赖，SDK 缺失时配置阶段
直接失败，不会静默回退到 VTK。

采样、投影和最短路径：

```cpp
u3::SampleOptions sampling;
sampling.algorithm = u3::SampleAlgorithm::farthest_point;
auto points = mesh.sample(1000, sampling);   // 始终返回固定数量

auto line = mesh.project_line(ordered_points); // 保持输入点顺序投影到表面
auto path = mesh.find_path(start_vertex, end_vertex); // Dijkstra 网格最短路径

auto cache = u3::MeshPathCache::create(mesh.mesh());
u3::PathOptions path_options;
path_options.cache = std::make_shared<u3::MeshPathCache>(cache.value());
auto cached_path = mesh.find_path(start_vertex, end_vertex, path_options);
```

采样算法包括 `uniform`、`random` 和 `farthest_point`；随机采样由 `seed` 控制，
因此可以复现。路径缓存只缓存当前网格的顶点邻接和边长，适合在同一网格上重复查询。

面片简化默认使用 VTK 的边坍塌算法，并支持在构建了 CGAL 时显式选择 CGAL：

```cpp
auto simplified = mesh.simplify(10000);  // 默认 SimplifyBackend::vtk
if (simplified) {
    const auto actual_faces = simplified.value().faces().rows();
}

u3::SimplifyOptions options;
options.backend = u3::SimplifyBackend::cgal;
auto cgal_simplified = mesh.simplify(10000, options);
```

`10000` 是目标面片数。VTK/CGAL 的边坍塌会受网格拓扑、边界和合法坍塌步长约束，
因此最终数量可能与目标不同；通过算法报告的 `output_faces`、`residual` 和
`converged` 检查实际结果。选择 CGAL 时直接使用固定的 CGAL 后端，不会静默退回 VTK。

VTK 简化算法可通过 `SimplifyOptions::algorithm` 选择：
`decimate_pro`（默认，支持拓扑保护）、`quadric_decimation`（二次误差度量）和
`quadric_clustering`（空间网格聚类）。三者都是目标面数的近似结果，不承诺精确面数。

`remesh()` 默认使用 VTK 自适应细分，以最大边长为约束并保持原有拓扑；它适合稳定地
细化和重构表面。需要 CGAL 各向同性重网格时，可将 `RemeshOptions::backend` 设为
`RemeshBackend::cgal`，但构建时必须启用 CGAL。

网格均匀化和细分：

```cpp
auto uniform = mesh.uniformize();       // VTK：长边自适应细分 + 特征保护平滑
auto global = mesh.subdivide(2);        // 整体 Loop 细分两轮

auto local = mesh.subdivide_faces({12, 18, 19}, 1); // 只细分指定面片
```

`subdivide_faces()` 会自动共享边中点，并同步细分相邻面片的共享边，避免局部细分
产生 T-junction 裂缝。指定面片的子面会继续参与下一轮迭代；邻接面只为保持拓扑
一致而细分，不会被当作下一轮目标。`UniformizeOptions::backend` 设为
`RemeshBackend::cgal` 时使用固定的 CGAL 各向同性重网格；SDK 缺失时配置阶段失败。

布尔运算必须先经过可执行性预检：

```cpp
auto preflight = u3::check_boolean_mesh(a, b, u3::BooleanOperation::intersect);
if (!preflight || !preflight.value().can_execute) {
    // 查看 preflight.value().reason，不要强行调用后端
}

auto result = u3::boolean_mesh(a, b, u3::BooleanOperation::intersect);
```

预检会拒绝空网格、退化面、重复/未使用顶点、非流形边、开放边界和自交网格；
CGAL 是固定依赖。包围盒完全分离的闭合网格会使用确定性快捷路径，
避免把不可能相交的输入送入布尔内核。

闭合曲线图切裁剪：

```cpp
u3::Vertices curve(4, 3);
curve << 2, 2, 0, 8, 2, 0, 8, 8, 0, 2, 8, 0;

u3::CurveClipOptions options;
options.region = u3::CurveClipRegion::inside;
options.max_projection_distance = 0.5; // 0 表示不限制
auto cropped = mesh.clip_curve(curve, options);
```

曲线点会先投影到网格表面，随后使用网格边图上的 Dijkstra 最短路径连接相邻点，
再选择闭合回路的最小区域；因此它适合在三角网格上执行类似 MeshLib 的闭合曲线
裁剪。曲线不需要重复首点，算法会自动闭合。`region` 可改为 `outside` 保留回路外部，
`selection` 可改为 `largest_region`。曲线不能自交，且应位于同一连通表面附近；设置
`max_projection_distance` 可以防止错误投影到网格的另一处。

## 处理主线

```text
load_mesh
→ validate/check
→ clean_mesh / repair_mesh
→ estimate normals
→ segment or sample
→ register point clouds
→ reconstruct_surface
→ save_mesh
```

点云到网格的重建要求点云包含方向一致、有限且非零的法线。

## 文件读写

```cpp
auto loaded = u3::load_mesh("input.vtp");
if (!loaded) return 1;

auto saved = u3::save_mesh(loaded.value(), "output.ply");
if (!saved) return 1;
```

支持的网格格式由当前内部 I/O 实现提供，读写失败通过 `Result` 返回，不把第三方异常传给用户。

## 设计边界

- 不公开 VTK、CGAL、PCL 类型；
- 只在简化算法中提供受控的 `SimplifyBackend` 选择，不暴露任何第三方对象；
- 不提供 `to_pcl()` 或其他底层转换接口；
- 不承诺第三方库的全量 API 映射；
- 后端 SDK 未启用时返回明确的功能不可用错误；
- 真实扫描数据、大模型和极端拓扑仍需在安装对应 SDK 的环境中验证。
