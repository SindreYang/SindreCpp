# 开发注意事项

本文记录容易被忽略的设计约束和维护检查项，适合准备修改公共 API、接入第三方
依赖或提交模块变更的开发者。目录和构建流程请看 [开发指南](development.md)。

## 1. 这是功能库，不是第三方库集合

sindrecpp 的目标是提供稳定的使用体验，而不是简单地把 spdlog、Eigen、simdjson 等库重新导出。

对外 API 应该表达功能：

```cpp
sindre::general::log::info(...);
sindre::general::string::trim(...);
```

不要让使用者必须先理解底层依赖：

```cpp
// 不建议把第三方类型直接作为所有 API 的唯一入口
spdlog::logger logger;
```

需要高级能力时，应在对应模块明确记录原生互操作边界；General 的日志、JSON、HTTP
和 CLI 不提供第三方类型逃生口，Math 的 Eigen 互操作是单独记录的基础契约。

## 2. 不要过度拆分

Sindre 的习惯是按功能集中代码。以下拆分方式不推荐：

```text
string_trim.h
string_split.h
string_replace.h
string_prefix.h
```

应该集中为：

```text
string.h
```

只有在依赖、编译成本或职责已经明显不同的情况下才拆分模块。

## 3. 不要为了兼容保留旧 API

当前项目处于早期阶段。重命名或调整接口时，直接采用新规范，不保留：

- 旧函数别名；
- 旧 target 别名；
- 旧宏名称；
- 仅为兼容而存在的转发头文件；
- 同一个功能的多套命名。

这样可以避免项目长期积累重复 API。

## 4. 通用能力统一放在 general

通用功能统一通过 `sindre::general` 领域入口组织，第三方库不直接成为顶层领域名称。

常规使用：

```cpp
#include <sindre/general.h>
#include <sindre/utils_3d.h>
```

`string`、`log`、`json` 等聚焦命名空间属于 `general` 领域内部能力；`utils_3d` 负责 3D 和数学能力。

## 5. 模块开关必须清晰

模块开关采用：

```text
SINDRE_WITH_<MODULE>
```

例如：

```bash
-DSINDRE_WITH_UTILS_PY=OFF
-DSINDRE_WITH_UTILS_3D=OFF
```

关闭模块后：

- 对应第三方依赖不应被下载；
- 对应 target 不应被加入总 target；
- 对应头文件不应要求该依赖；
- General 默认聚合第三方依赖；如需无这些依赖的最小构建，显式关闭对应的
  `SINDRE_WITH_*` 选项。

## 6. 注意 utils_3d 模块的可移植性

Utils_3d 对外提供后端无关的 Mesh、PointCloud 和 SindreMesh；Eigen 用于数组与数学交换。VTK、CGAL 和 PCL 只在模块内部按需实现网格、拓扑、点云配准和 Poisson 重建，用户代码不需要包含它们的头文件。
Utils_3d 的公共接口统一使用 C++17。需要 C++20 的第三方后端不能作为当前统一构建
配置的一部分启用；网格与 NumPy 转换全部独立拷贝。
MeshLib 仅保留在文档中作为历史设计/依赖记录，已从当前实现、CMake 和安装导出中移除。
跨后端算法不自动传递标签/颜色/UV；拓扑变化后需显式回映射。详见 [网格指南](../guides/mesh.md)。
显示、数据/图像处理不属于当前公开的 utils_3d API；历史设计和后端说明见 [VTK 指南](../guides/vtk.md)。

`SINDRE_UTILS_3D_NATIVE_ARCH` 默认关闭；开启 `ON` 后会使用构建机器的 CPU 指令集，适合本机性能测试，不适合直接分发给不同 CPU 的用户。

分发二进制时建议：

```bash
-DSINDRE_UTILS_3D_NATIVE_ARCH=OFF
```

`BLAS` 后端也应明确：

- `AUTO`：检测到可用 BLAS 时使用；
- `EIGEN`：使用 Eigen 内置实现；
- `BLAS`：强制要求外部 BLAS。

## 7. 注意 utils_py 模块的环境要求

utils_py 模块需要：

- Python 3.12 或更高版本的解释器和开发文件；
- pybind11；
- 使用 NumPy 转换时需要 NumPy 运行时。

配置阶段可用标准 CMake 变量指定用于编译和链接的解释器：

```bash
cmake -S . -B build -G Ninja -DSINDRE_WITH_UTILS_PY=ON \
  -DPython3_EXECUTABLE="D:/运行时/Python312/python.exe"
```

使用 uv 虚拟环境时，还应指定该环境中的 pybind11 CMake 包：

```powershell
uv venv --python 3.12 .venv
uv pip install --python .venv\Scripts\python.exe numpy pybind11
cmake -S . -B build_win -G Ninja `
  -DPython3_EXECUTABLE="F:/My_Github/SindreCpp/.venv/Scripts/python.exe" `
  -Dpybind11_DIR="F:/My_Github/SindreCpp/.venv/Lib/site-packages/pybind11/share/cmake/pybind11" `
  -DSINDRE_WITH_UTILS_PY=ON
```

运行时使用 `sindre::utils_py::InterpreterConfig` 传入
`std::filesystem::path`。库内部转换为 CPython `PyConfig`，不要求用户
直接处理 `wchar_t**`、`PYTHONHOME` 或环境变量；中文 Windows 路径不会
经过 ANSI 转换。隔离嵌入时应同时提供 `python_executable` 和
`python_home`；后者应指向包含标准库的 CPython 基础安装目录，uv 虚拟环境
通常使用 `sys.base_prefix`，而不是 `.venv` 本身。应用程序和 DLL 插件优先调用
`sindre::utils_py::get_runtime()`，由库保证进程内只初始化或附加一次；
`Interpreter::create()` 适合由宿主明确负责解释器生命周期的场景。每个
进程必须独立拥有自己的 Python 解释器，Python 对象不能跨进程传递。

如果宿主项目不需要 Python，应关闭：

```bash
-DSINDRE_WITH_UTILS_PY=OFF
```

utils_py 默认关闭，只有启用后才要求 Python 开发环境。

## 8. utils_gui 的后端和职责边界

`utils_gui` 默认使用匹配版本的 Dear ImGui core + GLFW + OpenGL3 backend，
由 `GuiApplication::create()` 统一完成窗口、显示器、DPI、输入和渲染初始化。
默认主题为暗色，圆角、间距和字体缩放保持一致；字体搜索支持系统目录和显式路径，
图片加载支持 UTF-8/中文路径，并通过 `TextureUploader` 与宿主 GPU 纹理对象衔接。

宿主若已经有 SDL、Win32、Vulkan 或 DirectX 生命周期，可将
`SINDRE_GUI_GLFW_OPENGL3` 关闭，仅使用 ImGui 核心 `Context`、图片 CPU 解码
和通用控件封装，不会强行绑定 GLFW。

## 9. 对外 API 要少而明确

优先提供少量能够长期维护的函数。不要因为底层库存在某个函数，就直接全部转发。

每个新增 API 都应回答：

1. 用户是否真的需要它；
2. 是否属于 sindrecpp 的功能域；
3. 是否有清晰的错误行为；
4. 是否能在不暴露底层细节的情况下使用；
5. 是否值得长期维护。

## 10. 文档与代码必须同步

新增模块必须同步更新：

- README 的模块说明；
- 本目录的开发规范或注意事项；
- 一个最小示例；
- 测试说明；
- 依赖和构建选项。

文档中的名称必须与代码完全一致，尤其注意：

- `sindre::<module>`
- `sindre::xxx`
- `SINDRE_WITH_XXX`

## 11. 版本与发布

发布前至少确认：

- CMake 可以独立构建；
- FetchContent 引入不会默认打开测试和示例；
- 关闭可选模块后仍能构建核心；
- Release 构建通过测试；
- README 中的版本和 FetchContent 标签一致；
- 公开 API 没有临时名称。

## 12. 异步与独立 TensorRT

AI 的 TRT 后端不依赖 ORT，转换与 engine 执行使用原生 TensorRT。
跨 GPU 兼容由用户指定硬件级别，不意味着任意 GPU 都能加载。
future 的 get 传播失败；有界队列满时拒绝新提交，close 排空旧任务。
Pipeline 在预处理和推理之间实际重叠工作，数据所有权必须清楚。
Pending 析构会等待 CUDA stream；设备缓冲由用户保持到完成。
详细参数、API 和 GPU 验证边界见 [推理指南](../guides/inference.md)。
