# sindrecpp examples

本文是最快的上手路径：先用最小示例验证编译环境，再按需要打开 AI、图像或 3D
模块。它不讲完整 API；模块细节请回到 [文档入口](../README.md) 选择对应页面。

每个示例都是一个可以单独配置的 CMake 项目，目录中包含自己的
`CMakeLists.txt` 和 `main.cpp`。从仓库根目录配置时，示例会随
`SINDRE_BUILD_EXAMPLES=ON` 自动加入；带可选依赖的示例只有在对应模块
启用后才会加入。

## 根工程构建

```bash
cmake --preset windows-clang-cl
cmake --build --preset windows-clang-cl --target sindre_example_general_basics
build_win/bin/sindre_example_general_basics.exe
```

默认构建的 `general_basics` 展示字符串、Result、版本、Base64、scope guard、
临时文件和 UTF-8 文件读写。

启用 JSON 示例：

```bash
cmake --preset windows-clang-cl
cmake --build --preset windows-clang-cl --target sindre_example_general_json_config
```

启用 utils_3d 示例时需要本机可用的 VTK 9.7.1 或兼容的 VTK 9 SDK；官方
SDK 的 `VTK_DIR` 应指向包含 `vtk-config.cmake` 的 `cmake` 目录：

```bash
cmake --preset windows-clang-cl \
  -DVTK_DIR="D:/software/VTK/vtk_sdk-9.7.1-cp312/vtk_sdk/cmake" \
  -DSINDRE_BUILD_EXAMPLES=ON -DSINDRE_WITH_UTILS_3D=ON
cmake --build --preset windows-clang-cl --target sindre_example_utils_3d_mesh
```

Windows 运行时还需要把 VTK `content/bin`、OpenBLAS `bin` 和 General
二进制依赖目录加入 `PATH`。示例会创建并读取一个临时四面体网格，完成
后删除示例文件。

## 单独配置一个示例

```bash
cmake -S examples/general_basics -B build_win/examples/general_basics -G "Visual Studio 17 2022" -A x64 -T ClangCL
cmake --build build_win/examples/general_basics --config RelWithDebInfo
build_win/examples/general_basics/bin/sindre_example_general_basics.exe
```

JSON 和 utils_3d 示例也可以用相同方式从自己的目录配置；它们会自动启用
需要的 sindrecpp 模块，并在依赖缺失时由 CMake 给出明确错误。

## Utils_2d 图像 facade

`utils_2d_image` 展示基于 OpenCV 的 `SindreImage` 高级封装。它不隐藏 OpenCV
核心类型，失败仍通过 `Result` 返回：

```powershell
cmake -S examples/utils_2d_image -B build_win/examples/utils_2d_image -G "Visual Studio 17 2022" -A x64 -T ClangCL
cmake --build build_win/examples/utils_2d_image --config RelWithDebInfo --parallel
build_win/examples/utils_2d_image/bin/sindre_example_utils_2d_image.exe input.png output.png
```

示例会加载图片、调整到 `400x400`、深拷贝、保存并显示窗口。服务器或无桌面环境
可以省略窗口调用，使用 `save()` 或 `to_tensor()`。

## TensorRT 生产路径

`examples/ai_tensorrt_segmentation` 是需要真实 CUDA/TensorRT SDK 的独立示例，
展示 ONNX 构建、engine 复用、`Result` 错误处理、固定 profile、分割后处理以及
Windows DLL 部署。完整的模型来源、下载脚本和兼容性选项见其
[TensorRT 语义分割示例](ai_tensorrt_segmentation.md)。

从仓库根目录可以使用预设快速生成两种构建：

```powershell
$env:SINDRE_TENSORRT_ROOT = "C:\Program Files\NVIDIA\TensorRT-10.11.0.33"
$env:CUDAToolkit_ROOT = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9"
cmake --preset ai-trt-full
cmake --build --preset ai-trt-full
cmake --preset ai-trt-dispatch
cmake --build --preset ai-trt-dispatch
```

`ai-trt-full` 用于构建 engine，`ai-trt-dispatch` 用于部署已有 engine。也可以
继续使用下面的独立示例命令；独立示例不会使用根目录 preset。

```powershell
powershell -ExecutionPolicy Bypass -File .\examples\ai_tensorrt_segmentation\download_model.ps1
cmake --preset ai-trt-full
cmake --build --preset ai-trt-full
```

该示例不能在没有 CUDA、TensorRT、Ninja 和 C++ 编译器的机器上完成运行验证；
配置成功也不等于目标 GPU 上的 engine 和 DLL 已通过 smoke test。
