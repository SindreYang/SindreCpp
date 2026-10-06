# Third-party dependencies

`thirds/` 是 SindreCpp 的第三方依赖登记区。每个公共模块在这里维护自己的
依赖来源、版本和接入方式；模块实现只负责选择功能，不再散落 Git 地址和版本号。

第三方源码默认由 CMake FetchContent 下载到构建目录的 `_deps/`，不提交到
SindreCpp 源码树。这样既能锁定可复现版本，也能允许宿主项目优先提供已经安装的
CMake target 或 SDK。

## 目录

| 目录 | 依赖范围 |
| --- | --- |
| [general](general/README.md) | CsPointer、CsString、spdlog、cpp-httplib、simdjson、argparse、pybind11 |
| [ai](ai/README.md) | ONNX Runtime、TensorRT、CUDA、cuDNN |
| [gui](gui/README.md) | Dear ImGui、GLFW、OpenGL |
| [utils2d](utils2d/README.md) | OpenCV |
| [utils3d](utils3d/README.md) | Eigen、VTK，以及可选几何后端 |

## 来源规则

- Git 依赖必须填写固定 tag 或 commit，不允许默认跟随主分支。
- URL 下载必须填写固定版本和校验值；当前仓库暂未需要二进制 URL 依赖。
- 大型 SDK、商业/系统库和 GPU 运行时不自动下载，使用现有的 `*_ROOT`、
  `CMAKE_PREFIX_PATH` 或 CMake package target。
- 已安装的 target 优先于 FetchContent；只有找不到 target 时才下载开源依赖。
- 第三方测试、示例和文档默认关闭，避免污染 SindreCpp 的构建目标。
- 不把第三方头文件复制到 `include/`，对外只暴露 SindreCpp 的模块头文件。

## 在宿主项目中替换依赖

依赖登记文件中的仓库和版本都可以通过同名 CMake cache 变量覆盖。例如：

```cmake
set(SINDRECPP_THIRD_GENERAL_SIMDJSON_TAG v4.6.11 CACHE STRING "")
set(CMAKE_PREFIX_PATH "C:/sdk;/another/sdk" CACHE PATH "")
add_subdirectory(SindreCpp)
```

对于 OpenCV、VTK、GLFW、ONNX Runtime 和 TensorRT，应优先使用对应 SDK 的
CMake package；不要把 SDK 二进制提交到本仓库。
