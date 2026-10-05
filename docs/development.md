# 开发指南

## 项目定位

SindreCpp 是 Sindre 生态的 C++17 基础库，提供稳定、直接、面向使用者的通用能力。

项目按“功能域”组织，不按第三方依赖组织。第三方库只是实现方式，不应成为对外 API 的核心概念。

## 目录结构

```text
SindreCpp/
├── include/sindrecpp/   # 对外头文件
├── examples/            # 最小可运行示例
├── tests/               # 单元测试
├── docs/                # 开发规范和注意事项
├── CMakeLists.txt       # CMake 配置
├── README.md            # 项目入口文档
└── LICENSE
```

新增功能时，优先按用户看到的功能放置：

- 字符串处理放在 `sindrecpp/general.hpp` 的 `general::string` 能力中
- 网格对象放在 `sindrecpp/utils3d/sindremesh.hpp`（VTK），几何算法放在 `utils3d/algorithm.hpp`；数学交换使用 Eigen
- 显示独立放在 `utils3d/show_mesh.hpp`，由网格调用；数据集/图像/场分析放在 `sindredata.hpp` / `sindreimage.hpp`，二维绘图放在 `show_plot.hpp`
- `SINDRECPP_UTILS3D_SHOW` 与 `SINDRECPP_UTILS3D_VTK_DATA` 分别按需开启；不要把原生 VTK 可访问性当作封装完成，覆盖清单见 [VTK 指南](vtk.md)
- 日志能力放在 `sindrecpp/general.hpp` 的 `general::log` 能力中
- 媒体能力应放在 `sindrecpp/utilsav.hpp`
- 图像能力应放在 `sindrecpp/utils2d.hpp`
- 底层库别名放入对应模块的 `native` 命名空间

不要为每一个函数单独创建头文件。

## 功能域

SindreCpp 的顶层组织跟随 Sindre：

- `general`：通用工具、字符串、日志、HTTP、JSON、CLI；
- `utils2d`：OpenCV 图像和推理预处理；
- `utils3d`：VTK SindreMesh、几何算法和 Eigen 数学交换；后端独立按需开启，详见 [网格指南](mesh.md)；
- `utilsav`：音视频能力，预留；
- `ai`：ONNX Runtime CPU/CUDA 和独立 TensorRT 推理；
- `deploy`：部署能力，预留；
- `platform`：平台相关能力，预留；
- `apps`：应用级能力，预留。

第三方库只能作为域内实现，不直接决定 SindreCpp 的顶层模块名称。

## 命名规则

### CMake

```text
项目名：SindreCpp
目标名：SindreCpp::General 或 SindreCpp::Utils3d
选项名：SINDRECPP_WITH_STRING 或 SINDRECPP_WITH_UTILS3D
```

### C++

```cpp
namespace sindrecpp::general::string {}
namespace sindrecpp::utils3d {}

namespace sindrecpp::general {
class Result {};
struct Error {};
}

std::string replace_all(...);
bool starts_with(...);
```

规则：

- 命名空间、函数、变量使用小写下划线；
- 类型使用 PascalCase；
- 常量使用 `UPPER_SNAKE_CASE`；
- 公开动作优先使用 `get_`、`set_`、`change_`、`show_`、`load_`、`save_`；
- 状态流程使用 `start`、`done`，不使用 `complete`；
- 不使用 `do_`、`handle_`、`process_` 这类无法表达具体动作的名称。

## 模块设计

每个模块应同时提供：

1. 简单的 SindreCpp API；
2. 必要时提供 `native` 命名空间访问底层库；
3. 独立的 CMake 目标；
4. 最小测试或示例；
5. 文档中的使用方式。

示例：

```cpp
namespace sindrecpp::general::log {
using Logger = spdlog::logger;
namespace native = spdlog;
}
```

调用者默认使用 `sindrecpp::general::log`，只有需要底层高级能力时才使用 `sindrecpp::general::log::native`。

## 错误处理

可预期的失败使用 `Result<T>` 和 `Error`：

```cpp
auto result = load_config(path);
if (!result) {
    // result.error()
}
```

约定：

- 成功使用 `Result::success(...)`；
- 失败使用 `Result::failure(...)`；
- 不混用 `ok`、`fail`、`complete` 等替代命名；
- 错误信息应能说明操作、对象和原因；
- 不要用静默返回空值掩盖失败。

## 依赖管理

- 优先复用父项目中已经存在的 CMake target；
- 没有现成 target 时才使用 FetchContent；
- 第三方库的测试、示例和文档默认关闭；
- 每个依赖必须固定版本；
- 不把第三方头文件复制进 SindreCpp；
- 不通过全局宏污染宿主项目。

## 测试

本地构建：

```bash
cmake -S . -B build -DSINDRECPP_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

新增模块至少应覆盖：

- 正常输入；
- 空输入；
- 边界输入；
- 失败路径；
- 关闭模块选项后的构建行为。

## 提交前检查

提交前确认：

- 公开名称符合本规范；
- 示例可以独立理解；
- 关闭无关模块后仍能构建；
- 没有引入不必要的兼容别名；
- 没有把内部实现名称暴露给用户；
- README 或对应文档已经更新。

## 按需模块

只有无第三方依赖的 general 默认开启；其他功能域和第三方集成默认关闭。
Result/Error/version 属于 general，无独立 Core 目标。
GUI/Python 域为 utils_gui/utils_py，不保留旧名称。
AI 默认选择 ONNX Runtime+CUDA；独立 TensorRT 显式开启，并可关闭 ORT。
提供 infer、infer_async 和两阶段 Pipeline，错误经 future 传播，关闭排空任务。
未启用 AI 不查找 ORT/GPU SDK。
详见 inference.md。
