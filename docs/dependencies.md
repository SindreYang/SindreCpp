# Third-party dependencies

本文是依赖总览，帮助使用者判断某个模块需要哪些 SDK、包管理器 target 或运行时。
具体版本和发现方式按模块拆分在本目录下；如果只是使用 General，可先阅读
[General 依赖](dependencies/general.md)。

`thirds/` 是 sindrecpp 的第三方依赖登记区。每个公共模块在这里维护自己的
依赖来源、版本和接入方式；模块实现只负责选择功能，不再散落 Git 地址和版本号。

General 的固定源码、OpenBLAS 二进制包和 General 所需的固定 vcpkg 二进制包已经登记在
`thirds/general/`，默认直接使用这些版本。其他模块的可选依赖仍可由 CMake
FetchContent 下载到构建目录 `_deps/`；宿主项目也可以按模块规则显式提供 SDK/target。

## 目录

| 目录 | 依赖范围 |
| --- | --- |
| [general](dependencies/general.md) | Eigen、CsString、spdlog、cpp-httplib、simdjson、argparse、RE2、Crashpad、zlib、OpenSSL |
| [utils_py](dependencies/utils_py.md) | pybind11、Python 3 development files、NumPy |
| [ai](dependencies/ai.md) | ONNX Runtime、TensorRT、CUDA、cuDNN |
| [gui](dependencies/gui.md) | Dear ImGui、GLFW、OpenGL |
| [utils_2d](dependencies/utils_2d.md) | OpenCV |
| [utils_3d](dependencies/utils_3d.md) | General 提供的 Eigen、VTK，以及可选几何后端 |

## 来源规则

- Git 依赖必须填写固定 tag 或 commit，不允许默认跟随主分支。
- 版本优先选择长期维护的稳定线；新主版本刚发布时，默认使用上一主版本的最后稳定小版本，
  待生态和 ABI 验证完成后再升级。
- URL 下载必须填写固定版本和校验值；当前仓库暂未需要二进制 URL 依赖。
- 大型 SDK、商业/系统库和 GPU 运行时不自动下载，使用现有的 `*_ROOT`、
  `CMAKE_PREFIX_PATH` 或 CMake package target。
- General 默认优先使用 `thirds/general` 中的固定版本；只有显式设置
  `SINDRE_GENERAL_USE_EXTERNAL_DEPS=ON` 才优先使用宿主 target。其他模块仍优先复用宿主 target。
- 第三方测试、示例和文档默认关闭，避免污染 sindrecpp 的构建目标。
- 不把第三方头文件复制到 `include/`，对外只暴露 sindrecpp 的模块头文件。

## 在宿主项目中替换依赖

依赖登记文件中的仓库和版本都可以通过同名 CMake cache 变量覆盖。例如：

```cmake
set(SINDRE_THIRD_GENERAL_SIMDJSON_TAG v4.6.11 CACHE STRING "")
set(CMAKE_PREFIX_PATH "C:/sdk;/another/sdk" CACHE PATH "")
add_subdirectory(sindrecpp)
```

对于 OpenCV、VTK、GLFW 和 TensorRT，应使用对应 SDK 的 CMake package；不要把大型 SDK
二进制提交到本仓库。General 的 OpenBLAS、RE2、Crashpad、zlib、OpenSSL 固定包例外，
已放入 `thirds/general` 供本地可复现构建使用。
