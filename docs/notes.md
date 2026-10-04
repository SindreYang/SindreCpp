# 开发注意事项

## 1. 这是功能库，不是第三方库集合

SindreCpp 的目标是提供稳定的使用体验，而不是简单地把 spdlog、Eigen、simdjson 等库重新导出。

对外 API 应该表达功能：

```cpp
sindrecpp::log::info(...);
sindrecpp::string::trim(...);
```

不要让使用者必须先理解底层依赖：

```cpp
// 不建议把第三方类型直接作为所有 API 的唯一入口
spdlog::logger logger;
```

需要高级能力时，通过 `native` 提供逃生口。

## 2. 不要过度拆分

Sindre 的习惯是按功能集中代码。以下拆分方式不推荐：

```text
string_trim.hpp
string_split.hpp
string_replace.hpp
string_prefix.hpp
```

应该集中为：

```text
string.hpp
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

通用功能统一通过 `sindrecpp::general` 领域入口组织，第三方库不直接成为顶层领域名称。

常规使用：

```cpp
#include <sindrecpp/general.hpp>
#include <sindrecpp/utils3d.hpp>
```

`string`、`log`、`json` 等聚焦命名空间属于 `general` 领域内部能力；`utils3d` 负责 3D 和数学能力。

## 5. 模块开关必须清晰

模块开关采用：

```text
SINDRECPP_WITH_<MODULE>
```

例如：

```bash
-DSINDRECPP_WITH_PYTHON=OFF
-DSINDRECPP_WITH_UTILS3D=OFF
```

关闭模块后：

- 对应第三方依赖不应被下载；
- 对应 target 不应被加入总 target；
- 对应头文件不应要求该依赖；
- 核心模块仍应可以独立构建。

## 6. 注意 utils3d 模块的可移植性\n\nMath 模块对外提供用户功能命名，底层实现使用 Eigen。用户代码应使用 `sindrecpp::utils3d`，不要依赖 Eigen 的实现名称。

`SINDRECPP_UTILS3D_NATIVE_ARCH=ON` 会使用构建机器的 CPU 指令集，适合本机性能测试，不适合直接分发给不同 CPU 的用户。

分发二进制时建议：

```bash
-DSINDRECPP_UTILS3D_NATIVE_ARCH=OFF
```

`BLAS` 后端也应明确：

- `AUTO`：检测到可用 BLAS 时使用；
- `EIGEN`：使用 Eigen 内置实现；
- `BLAS`：强制要求外部 BLAS。

## 7. 注意 Python 模块的环境要求

Python 模块需要：

- Python 解释器；
- Python 开发文件；
- pybind11；
- 使用 NumPy 转换时需要 NumPy 运行时。

如果宿主项目不需要 Python，应关闭：

```bash
-DSINDRECPP_WITH_PYTHON=OFF
```

不要让基础 C++ 项目因为默认开启 Python 而强制安装完整 Python 开发环境。

## 8. 注意 GUI 模块的职责边界

GUI 模块只负责 Dear ImGui 核心和上下文生命周期。

窗口、输入和渲染后端由宿主项目负责，例如：

- GLFW；
- SDL；
- Win32；
- OpenGL；
- Vulkan；
- DirectX。

不要在 SindreCpp 中偷偷绑定某一个窗口系统。

## 9. 对外 API 要少而明确

优先提供少量能够长期维护的函数。不要因为底层库存在某个函数，就直接全部转发。

每个新增 API 都应回答：

1. 用户是否真的需要它；
2. 是否属于 SindreCpp 的功能域；
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

- `SindreCpp::Xxx`
- `sindrecpp::xxx`
- `SINDRECPP_WITH_XXX`

## 11. 版本与发布

发布前至少确认：

- CMake 可以独立构建；
- FetchContent 引入不会默认打开测试和示例；
- 关闭可选模块后仍能构建核心；
- Release 构建通过测试；
- README 中的版本和 FetchContent 标签一致；
- 公开 API 没有临时名称。
