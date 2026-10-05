# SindreMesh 与几何算法

`utils3d` 开启后以 VTK 9 为网格基础，Eigen 为数组交换基础。不开启时不查找任何 3D SDK。
计算不创建渲染窗口，不需要 GUI 或 GPU。当前算法路径为 CPU；AI 的 CUDA 默认不影响此模块。

## 两个文件

| 文件 | 职责 |
| --- | --- |
| `utils3d/sindremesh.hpp` | 单文件 VTK 网格封装，`sindrecpp::utils3d::SindreMesh` |
| `utils3d/algorithm.hpp` | 算法、后端选择及后端网格转换 |

命名参考 Python sindre 的 `SindreMesh`，设计参考 vedo 的简洁调用方式，但不复制 vedo 实现。
这不是完整 VTK/vedo/Python sindre API 的逐项兼容移植，也不是五个第三方库所有函数的重导出。
下表是实际已经提供的功能范围；不支持的操作会报错，不提供空壳接口。

## 开启与依赖

```cmake
set(SINDRECPP_WITH_UTILS3D ON CACHE BOOL "")
# 默认仅 VTK + Eigen；以下均默认 OFF，按需求开启。
set(SINDRECPP_UTILS3D_MESHLIB ON CACHE BOOL "")
set(SINDRECPP_UTILS3D_CGAL ON CACHE BOOL "")
set(SINDRECPP_UTILS3D_OPEN3D ON CACHE BOOL "")
set(SINDRECPP_UTILS3D_IGL ON CACHE BOOL "")
set(SINDRECPP_UTILS3D_VCG ON CACHE BOOL "")
set(SINDRECPP_IGL_ROOT "/path/to/libigl" CACHE PATH "")
set(SINDRECPP_VCG_ROOT "/path/to/vcglib" CACHE PATH "")
# 安装 SDK 的 CMake package 路径通过 CMAKE_PREFIX_PATH / VTK_DIR / Open3D_DIR 等提供。
# FetchContent_MakeAvailable(SindreCpp) 后：
target_link_libraries(my_app PRIVATE SindreCpp::Utils3d)
```

VTK/CGAL/Open3D/MeshLib 使用已安装 SDK，不自动从源码构建大型依赖。
MeshLib 使用 `find_package(meshlib CONFIG)`，链接 `MeshLib::MRMesh` 或 `MRMesh`，开启后要求 C++20。
其他接口要求 C++17。CI 使用 MeshLib v3.1.4.297、libigl v2.5.0、固定 VCGlib 提交，
以及 Ubuntu 24.04 的 VTK/CGAL SDK、Open3D 0.19.0 官方 C++11-ABI SDK。升级 SDK 后应重新运行后端测试。

## 网格使用

```cpp
#include <sindrecpp/utils3d.hpp>
namespace u3 = sindrecpp::utils3d;
u3::SindreMesh mesh("scan.ply");
auto copy = mesh.clone();
copy.shift_xyz(Eigen::Vector3d(1, 2, 3)).scale_xyz(2.0);
auto v = copy.vertices(); // N×3 float64 独立副本
auto f = copy.faces();    // M×3 int64 独立副本
auto centers = copy.faces_barycentre();
copy.compute_normals();
copy.save("scan.vtp");
```

| 能力 | 接口 |
| --- | --- |
| 构造与复制 | 数组、VTK polydata、文件路径；`clone`；复制构造/赋值为深拷贝 |
| 几何读写 | `vertices`, `faces`, `update_geometry`, `npoints`, `nfaces`, `empty` |
| 文件 | `load`, `save`：STL / PLY / OBJ / VTP |
| 变换 | `apply_transform`, `apply_inv_transform`, `shift_xyz`, `scale_xyz`, `rotate_xyz` |
| 属性 | `set_data`, `get_pointdata`, `get_celldata`；顶点/面片 int64 标签 |
| 几何 | 顶点/面法线、重心、面积、中心、半径、曲率 |
| 拓扑 | 唯一边、边对应面、边界边、非流形边、顶点/面邻接列表 |
| 组件 | `largest_component`, `split_component_by_faces` |
| 查询 | `get_near_idx`、`project_points`、`signed_distance` |

`rotate_xyz` 使用角度制，绕原点，次序为 X→Y→Z；`apply_transform` 为列向量约定的仿射矩阵。
`center` 是顶点平均值；`radius` 是到该中心的最大距离。空网格没有中心，零半径不能归一化。
`get_boundary` 返回原网格顶点索引的无向边，不是已经排序的边界环。
`is_watertight` 仅检查每条边有两个相邻面，不能证明没有自相交或顶点非流形。

`get_native` 是借用的 VTK 指针，需在所属网格存活期间使用；直接修改后需遵循 VTK Modified 规则。
同一网格不保证并发修改安全，独立 clone 可独立处理。没有自动后台线程或解释器初始化。

## 算法与选择顺序

每种操作只执行一个后端；`automatic` 在已开启且有实现的后端中按下表选第一个。
用户指定的 MeshLib → CGAL → Open3D → 其他，是选择优先级，不是已通过实验的稳定性排名。
不适合该操作/没有该封装的后端不会为了优先级强行使用；执行失败直接抛异常，不偷偷换算法重试。
`get_backend`、`get_supported_backends`、`get_available_backends` 可检查实际选择。

| 算法 | 接口 | 后端顺序 |
| --- | --- | --- |
| 网格简化 | `decimate` | MeshLib → CGAL → Open3D → libigl → VTK |
| 平滑 | `smooth` | MeshLib → Open3D → VTK |
| 各向同性重网格 | `remesh` | MeshLib → CGAL |
| 并/交/差 | `boolean_mesh` | MeshLib → CGAL |
| 自相交检测 | `has_self_intersections` | MeshLib → CGAL |
| 补洞 | `fill_holes` | MeshLib → CGAL → VTK |
| 清理 | `clean` | MeshLib → Open3D → VCG → VTK |
| 清理+补洞+法线 | `fix_mesh` | 按各步骤选后端；不是任意缺陷的全自动修复保证 |
| 平均曲率 | `get_curvature` | libigl → VTK |
| 圆边界调和 UV | `get_uv` | libigl；要求一个边界环，调用方需保证盘拓扑 |
| ICP 配准 | `register_icp` | Open3D；点到点，返回变换/fitness/RMSE |
| 面积加权表面采样 | `sample` | Open3D |
| Poisson 重建 | `reconstruct_poisson` | Open3D；输入需有方向一致的非零法线 |
| Loop 细分/平面切割/翻面 | `subdivide`, `cut_plane`, `reverse_faces` | VTK |
| 最近邻标签回映射 | `labels_mapping` | VTK 空间索引 |
| 顶点↔面标签 | `vertex_labels_to_face_labels`, `face_labels_to_vertex_labels` | 多数投票；平票选较小标签，孤立点默认 -1 |
| 归一化/高斯热图 | `get_normalize`, `get_gaussian_heatmap` | Eigen/标准数学 |

```cpp
u3::DecimateOptions options;
options.target_faces = 10000;
auto backend = u3::get_backend(u3::Operation::decimate);
auto result = u3::decimate(mesh, options); // mesh 保持不变
auto normalized = result.clone();
auto normalization = u3::get_normalize(result);
normalized.apply_transform(normalization.transform);
// 恢复原坐标：normalized.apply_inv_transform(normalization.transform);
```

简化的目标面数是目标，不保证严格达到（拓扑保护、误差与可折叠边限制）。
平滑参数并非各后端数值等价：VTK strength 为 windowed-sinc pass band；
MeshLib/Open3D strength 为移动系数。preserve_volume 是减小收缩，不是严格体积约束。
VTK smooth 不接受 preserve_volume=false。MeshLib 使用 float32 坐标，其他路径通常为 float64。
CGAL/MeshLib 转换会拒绝不能表示的面，不静默丢面；应先检查或修复输入拓扑。
CGAL 布尔路径检查自相交，但仍应验证输入是方向正确的有效实体。
补洞默认尝试所有边界，可能封闭本来有意保留的开口；切平面不自动封口。
Poisson/采样/配准目前为 Open3D legacy CPU 路径，不承诺 CUDA。

### 属性与标签

所有自由算法返回新对象，输入不被修改。跨后端几何转换只交换顶点和三角面：
法线、颜色、UV、标签等不会自动跨后端传递。即便顶点数量没有变化，也不要假定索引相同。
拓扑变化后可显式 `labels_mapping` 回映射顶点标签，再按需要计算面标签。
VTK 过滤器可能传播部分数组，但其插值不一定适用于离散标签；调用者必须重新验证。
`update_geometry(v,f)` 丢弃全部旧属性；`update_geometry(v)` 保留属性、移除旧法线。
变换重新计算法线，非均匀缩放不会直接用普通矩阵变换法线。
VTP 支持保留自定义数组；STL/OBJ/PLY 不能保证保存全部属性，不能作为属性备份格式。

## NumPy ↔ Eigen/网格

同时链接 `SindreCpp::Utils_py` 与 `SindreCpp::Utils3d`，Python 解释器由调用者管理。

```cpp
#include <sindrecpp/utils_py.hpp>
sindrecpp::utils_py::Interpreter python;
// pybind11::array vertices, faces 从调用者获取：
auto mesh = sindrecpp::utils_py::mesh_from_arrays(vertices, faces);
auto arrays = sindrecpp::utils_py::arrays_from_mesh(mesh); // (vertices, faces)
auto matrix = sindrecpp::utils_py::matrix_from_array<double>(vertices);
auto array = sindrecpp::utils_py::array_from_matrix(matrix);
```

全部为独立拷贝，不提供借用 NumPy 内存的零拷贝视图。支持切片、转置、负 strides 与 Fortran 布局。
顶点为实数数组且有限；面索引必须为整数 dtype，检查 int64 溢出、负值和越界；不接受浮点索引强转。
空数组必须仍为 `(0,3)`；一般矩阵转换要求二维。Python 调用必须持有 GIL，返回对象不能在解释器关闭后存活。
这里只提供 C++ 数据转换工具，不自动生成完整 Python 扩展包。

## 验证与许可

CI 分别构建 VTK-only 和五个可选后端，执行网格/属性/变换/拓扑/算法及 NumPy 测试。
这是合成小网格的功能验证，不是扫描数据集的稳定性或性能评测；不声称某个后端最稳定。
发布前仍应在真实扫描数据、复杂孔洞、自交、极端尺度和大型模型上验证。
未实现：vedo 窗口/纹理 UI、所有后端的全量 API、曲线切割/曲线偏移、体素 offset、ARAP、
图割分割、CAD/OCC 转换与 Python 私有 `.smesh` 格式。这些不能用本页的“后端支持”替代。

本次不因许可阻止接入，也不删除任何第三方版权声明。后续开源不自动满足依赖许可；
项目 MIT 不覆盖第三方依赖，分发时需核对启用后端及其传递依赖的具体条款。
