# VTK 数据、图像、显示与功能清单

VTK 封装分为网格、通用数据、图像、场分析、三维显示和二维绘图。
`sindremesh.hpp` 调用独立 `show_mesh.hpp`，不承担图像和其他数据类型的职责。

```cmake
set(SINDRECPP_WITH_UTILS3D ON CACHE BOOL "")
set(SINDRECPP_UTILS3D_VTK_DATA ON CACHE BOOL "") # 数据集、图像、场分析；默认 OFF
set(SINDRECPP_UTILS3D_SHOW ON CACHE BOOL "")     # 显示、交互、体渲染、绘图；默认 OFF
target_link_libraries(my_app PRIVATE SindreCpp::Utils3d)
```

| 文件 | 用途 |
| --- | --- |
| `utils3d/sindremesh.hpp` | 三角网格、属性、拓扑、VTK 过滤器入口，`mesh.show()` |
| `utils3d/algorithm.hpp` | 网格算法、曲线截面、裁剪和拼接 |
| `utils3d/show_mesh.hpp` | 可独立使用的多对象窗口、相机、拾取、截图；数据开关开启后支持体渲染与图像切片 |
| `utils3d/sindredata.hpp` | 点、线、结构/非结构网格、组合数据、场分析、XML 读写 |
| `utils3d/sindreimage.hpp` | 二/三维规则图像、图像滤波和重采样 |
| `utils3d/show_plot.hpp` | 折线、散点、柱状图和 PNG 输出 |

## 图像与体数据

```cpp
#include <sindrecpp/utils3d/sindreimage.hpp>
namespace u3 = sindrecpp::utils3d;
u3::Matrix voxels(64*64*64, 1); // 填充实际体数据
// x 最快：x + nx*(y + ny*z)，列是通道；spacing/origin 为物理坐标。
u3::SindreImage image(voxels, {64,64,64}, Eigen::Vector3d(.5,.5,1));
auto filtered = image.gaussian().normalize();
auto mask = filtered.threshold(.3,1).morphology();
auto section = image.reslice(Eigen::Matrix4d::Identity(), {64,64,1});
image.save("volume.vti");
auto loaded = u3::SindreImage(u3::SindreData::load("volume.vti"));

u3::ShowOptions options;
options.offscreen = true; options.interactive = false;
u3::ShowMesh window(options);
window.volume(filtered).reset_camera().screenshot("volume.png");
```

图像 Gaussian sigma 使用体素单位。crop/pad 使用 VTK 原始 extent 索引，crop 不重新归零。
reslice 矩阵从输出局部坐标映射到输入物理坐标；输出 spacing/origin/尺寸由调用者指定。
normalize 对全部通道共同求范围；常数输入映射到 lower。cast 有截断/舍入及夹紧行为，
应先 normalize/shift_scale 后转为 uint8。形态学只接受单通道 0/1 图像。
图像连通域提取的前景为值 >= .5，标签不是原图的值，也不是拓扑稳定的永久编号。
直接由原生 vtkImageData 构造时，调用者负责有效 extent、spacing、方向矩阵和标量数组。

## 通用数据与场分析

```cpp
#include <sindrecpp/utils3d/sindredata.hpp>
auto data = u3::SindreData::structured_grid(points, {nx,ny,nz});
data.set_data("temperature", temperatures); // 点标量
data.set_data("velocity", velocities);     // 点三维向量
auto gradients = data.gradient("temperature");
auto derivatives = data.gradient("velocity", true, true, true); // curl/divergence
auto iso = data.contour("temperature", {300,500});
auto flow = data.streamlines("velocity", seeds, 10);
auto arrows = data.glyph_vectors("velocity", .1);
auto sample = u3::SindreData::point_cloud(query).probe(data);
auto grouped = u3::SindreData::blocks({data,iso,flow});
grouped.save("scene.vtm");
```

SindreData 复制/构造深拷贝，移动后源只能重新赋值；`get_native()` 在移动源上报错。
数据过滤器返回独立结果，不修改输入。组合数据须 `block()` 取叶节点后分析或显示。
`surface()` 只返回三角表面；线数据保持 SindreData 或原生 vtkPolyData，不强转为 SindreMesh。
point/cell 数组转换是数值插值/平均，不适用于离散标签；probe 的有效性查看
`vtkValidPointMask`，无效点不能当作实际采样值。场分析需要正确网格和物理单位，
不保证病态单元、退化网格或未知流场的数值精度。Delaunay 不等同于约束四面体网格生成。
streamlines 是静态向量场流线，不是时间积分粒子轨迹。calculate 接受 VTK 表达式，不运行外部脚本。
XML 扩展名必须与数据类型一致；VTM 会生成相邻子文件，需整体移动/分发。

## 显示与绘图

```cpp
#include <sindrecpp/utils3d/show_plot.hpp>
u3::ShowPlot plot;
plot.add(x,y,"curve").add(x,y,"samples",u3::PlotKind::points);
plot.title("Result").axis_titles("x","y").show();
```

三维显示细节见 [网格指南](mesh.md)。volume 用排序的 VolumeStop 设置颜色/透明度曲线，
不指定时根据标量范围给出默认曲线。image_slice 接受 axis=0/1/2 和原始 extent 索引；
直接显示原始图像时需先准备合适的显示标量范围（例如 normalize(0,255).cast()）。
通用数据的拾取编号属于提取出的表面，不能直接映射回原体网格单元。
图像/体 actor 暂不参与 MeshPick。CPU 算法不要求 CUDA；显示需要 OpenGL，
软件 Mesa 可渲染，旧 X11 VTK 离屏时仍需 DISPLAY/xvfb。体渲染不承诺跨显卡相同像素或性能。

## “80%”的验收口径

下方建立跨 VTK 能力域的 **100 项项目功能清单**，每项按一个使用场景等权统计。
这是供本项目验收的功能清单，不是 VTK 官方完整功能目录、公共类/方法覆盖率或性能发挥比例。
高级能力同样列入分母；不能用原生对象可访问、通用过滤器可配置来宣称它们已经有高层封装。
清单项目可继续增加，增加时必须重算比例，不以删减缺项维持 80%。

状态：封装=接口实现；验证=有对应执行测试。仅声明接口/仅编译不计入执行验证。
常用场景单元测试不代表真实医疗、仿真或超大数据项目已经验证。

| ID | 使用场景 | 接口/状态 |
| --- | --- | --- |
| 01–08 | 三角表面、点云、折线、规则图像、结构网格、直角网格、四面体网格、组合数据 | SindreMesh / SindreData / SindreImage |
| 09–15 | 点属性、面/单元属性、属性增删改名、独立复制、纹理坐标、通用过滤器、流水线快照 | set/get_data、data_names/remove/rename、clone、set_uv、filtered、pipeline_source |
| 16–25 | STL、PLY、OBJ、VTP、VTI、VTS、VTR、VTU、VTM 读写，PNG 截图 | load/save、screenshot |
| 26–35 | 包围盒、坐标变换、面积、体积、法线控制、曲率、边界环、特征边、邻接、连通表面 | SindreMesh |
| 36–45 | 最近点、表面距离、清理、标量区域、VTK 简化、VTK 平滑、VTK 补洞、细分、平面裁剪、盒/球裁剪 | SindreMesh / algorithm |
| 46–61 | 数据表面、等值线/面、阈值、数据裁剪、梯度、散度/旋度、向量形变、标量形变、点到单元、单元到点、场采样、表达式、Delaunay、流线、管线、箭头 | SindreData |
| 62–76 | Gaussian、中值、阈值、shift/scale、归一化、类型转换、裁取、补边、翻转、重采样、重切片、梯度/幅值、Laplacian、二值形态学、连通域 | SindreImage |
| 77–90 | 多对象显示、表面/线框/点、材质/透明度、标量色标、RGB/RGBA、整数标签、纹理、相机/灯光、辅助标注、鼠标/键盘事件、显示裁剪、体渲染、图像切片、二维图表 | ShowMesh / ShowPlot |
| 91 | VR/AR/OpenXR | 未封装 |
| 92 | OSPRay/光线追踪与高级 PBR 环境照明 | 未封装 |
| 93 | MPI 分布式分析/渲染 | 未封装 |
| 94 | 分布式读写、超大数据分块流式处理 | 未封装 |
| 95 | AMR 自适应层级网格 | 未封装 |
| 96 | 分子、图、树与信息可视化专用流程 | 未封装 |
| 97 | DICOM/NIfTI 专用读写与医疗流程 | 未封装 |
| 98 | HDF/数据库/地理与仿真专用读写器 | 未封装 |
| 99 | 可编辑 box/plane/测量等交互 widgets | 未封装 |
| 100 | 动画时间轴、视频录制、整场景导出 | 未封装 |

当前 01–90 有封装，91–100 未封装，即本清单接口覆盖 90/100。
执行验证比例须结合 tests 和 CI 结果，不能把这 90% 写成“整个 VTK 已覆盖 90%”。
STL/PLY/OBJ、曲率等已有接口仍有真实文件/边界场景验证缺项；全库80%验收必须另列测试证据。

VTK 范围参考：[官方介绍](https://vtk.org/about/)、[官方模块目录](https://docs.vtk.org/en/latest/modules/index.html)。
