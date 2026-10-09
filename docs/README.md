# sindrecpp 文档

这里是 sindrecpp 的文档入口。sindrecpp 是一个以 C++17 为基础、以 `General`
和 `Math` 为公共基础层的跨平台能力库；其他模块可以依赖或反哺这两个基础模块，但不会把第三方库
直接暴露成项目的核心概念。

如果你是第一次使用，先看下面的“按目的选择”；如果你已经确定模块，再直接进入
对应的模块说明。每篇文档开头都给出适用范围，避免为了找一个开关阅读整套文档。

## 按目的选择

| 目的 | 建议阅读顺序 |
| --- | --- |
| 快速构建、运行示例 | [示例与构建指南](guides/examples.md) |
| 使用通用能力 | [General 模块](modules/general.md) → [General 依赖](dependencies/general.md) |
| 使用矩阵、数组或 Eigen 互操作 | [Math 模块](modules/math.md) → [Math 依赖](dependencies/math.md) |
| 接入 ONNX Runtime / TensorRT | [AI 模块](modules/ai.md) → [推理指南](guides/inference.md) → [AI 依赖](dependencies/ai.md) |
| 处理图像和传统视觉 | [Utils_2d 模块](modules/utils_2d.md) → [Utils_2d 依赖](dependencies/utils_2d.md) |
| 处理网格和点云 | [Utils_3d 模块](modules/utils_3d.md) → [网格指南](guides/mesh.md) → [VTK 指南](guides/vtk.md) |
| 使用 GUI、Python | [GUI 模块](modules/gui.md) / [Utils_Py 模块](modules/utils_py.md) |
| 修改库或增加模块 | [开发指南](development/development.md) → [开发注意事项](development/notes.md) |

## 模块总览

| CMake 选项 | Target | 公共入口 | 能力范围 |
| --- | --- | --- | --- |
| 固定启用 | `sindre::general` | `sindre/general.h` | Result/Error、字符串、JSON、配置、文件、网络、运行时和系统能力 |
| 固定启用 | `sindre::math` | `sindre/math.h` | Eigen 行主序类型、Quaternion、Transform3 和固定 OpenBLAS 后端 |
| `SINDRE_WITH_UTILS_PY` | `sindre::utils_py` | `sindre/utils_py.h` | Python、NumPy、网格数据交换 |
| `SINDRE_WITH_GUI` | `sindre::gui` | `sindre/gui.h` | ImGui、GLFW/OpenGL3、字体、图片和 GUI 辅助 |
| `SINDRE_WITH_UTILS_2D` | `sindre::utils_2d` | `sindre/utils_2d.h` | OpenCV 图像、传统视觉算法、预处理和 Tensor 转换 |
| `SINDRE_WITH_UTILS_3D` | `sindre::utils_3d` | `sindre/utils_3d.h` | Mesh、PointCloud、SindreMesh、Math 和私有几何后端 |
| `SINDRE_WITH_AI` | `sindre::ai` | `sindre/ai.h` | Tensor、执行流水线、ONNX Runtime、TensorRT |

General、Math 和 Utils_3d 默认启用；AI、GUI、Utils_2d、Utils_Py 默认关闭。
Utils_3d 的默认配置要求本机提供 VTK、独立 CGAL、Boost、GMP、MPFR SDK；若只构建
General/Math，应显式设置 `-DSINDRE_WITH_UTILS_3D=OFF`。各模块的 target、开关、
公共头文件和测试入口见 [`modules/`](modules/)；第三方 SDK、版本和发现方式见
[`dependencies.md`](dependencies.md)。

## 最小接入

```cmake
include(FetchContent)
FetchContent_Declare(sindre
    GIT_REPOSITORY https://github.com/SindreYang/SindreCpp.git
    GIT_TAG main) # 生产环境请固定到经过验证的提交
FetchContent_MakeAvailable(sindre)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE sindre::general)
```

代码中只包含需要的公共入口：

```cpp
#include <sindre/general.h>
```

项目自己的公共头文件统一使用 `.h` 后缀，并位于 `include/sindre/`；第三方头文件
保留其上游名称。各模块均可单独链接，没有隐式的“全模块”聚合 target。

## 文档分区

- [guides/](guides/examples.md)：面向使用者的构建、示例、推理和 2D/3D 指南。
- [modules/](modules/general.md)：模块 API、CMake 选项和使用边界。
- [dependencies/](dependencies.md)：第三方依赖、SDK、版本和发现方式。
- [development/](development/development.md)：目录约定、实现规范、测试和维护注意事项；
  [依赖与完整性审计](development/dependency-audit.md)记录依赖来源和实际验证范围。

MIT；第三方依赖遵循各自许可证。
