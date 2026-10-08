# utils_2d module

本文面向需要 OpenCV 图像处理、传统视觉算法和 Tensor 预处理的使用者，
说明模块 target 和启用方式；依赖要求见 [Utils_2d 依赖说明](../dependencies/utils_2d.md)。

`sindre::utils_2d` is a static library target. 图像 I/O、预处理和算法接口统一使用
`sindre::general::Result<T>` 表达失败。OpenCV remains
available through `sindre::utils_2d::native`, so the wrapper does not hide the
native `cv::Mat` API.

Enable with `SINDRE_WITH_UTILS_2D=ON`. CMake discovers an installed OpenCV 4
SDK with matching `opencv_contrib` modules and registers `sindre.utils_2d` when tests are enabled. The module is
independent from the AI and Utils_3d backends.

On this Windows machine the official OpenCV 4.12.0 SDK was found, but it is a
plain build without contrib and is therefore rejected by the full module. For
an official archive, `OpenCV_DIR` must point to the directory containing
`OpenCVConfig.cmake`, usually `.../build/x64/vc16/lib`, not its parent. The
OpenCV DLL directory must also be on `PATH` when an executable starts; the
module copies it beside its own tests when the directory can be inferred.

算法封装覆盖滤波、阈值、形态学、边缘、轮廓、连通域、Hough、NMS、特征匹配、
单应性、光流、去畸变、相机标定和 ArUco。基础测试覆盖 Unicode 路径、读写、颜色转换、
letterbox 和 NCHW tensor 转换。OpenCV 4.8 或更高版本必须是带匹配版本 contrib
的构建，缺少 contrib 模块时配置阶段直接失败。当前仓库代码和算法测试已用本机
OpenCV 4.9 + contrib 头文件完成 clang-cl 语法编译；完整运行测试仍需要可运行的
CPU 或 CUDA DLL 集合。

## SindreImage 高级封装

`Image` 仍然是 OpenCV `cv::Mat` 的别名，适合需要直接使用 OpenCV 的高级用户。
普通图像处理可以使用 `SindreImage`，它复用同一组 OpenCV 后端和自由函数：

```cpp
#include <sindre/utils_2d.h>
#include <iostream>

auto loaded = sindre::utils_2d::SindreImage::load("1.png");
if (!loaded) {
    // loaded.error().code/message/context
    return;
}

if (auto status = loaded->resize({400, 400}); !status)
    return;

auto copy = loaded->clone();
if (!copy)
    return;

if (auto status = copy->show(); !status)
    return;

std::cout << *copy;
```

常用成员包括文件和内存编解码、深拷贝、普通/等比例 resize、普通/中心 crop、边框、
颜色转换、灰度/RGB/BGR、归一化、亮度对比度、Gamma、翻转、旋转、letterbox、仿射和
透视变换、mask、通道 split/merge、滤波、阈值、形态学、边缘检测、轮廓查询和 NCHW
tensor 转换。修改操作成功后才替换内部图像，失败时保留原图。需要 `scale/left/top`
等 letterbox 元数据时继续使用 `create_letterbox()`。

`operator<<` 只输出图像信息，例如 `empty`、`width`、`height`、`channels`、类型、字节数
和连续性，不会输出像素数据。

内存编解码适合网络或 AI 推理管线，不需要中间临时文件：

```cpp
auto bytes = loaded->encode(".jpg");
if (!bytes)
    return;
auto decoded = sindre::utils_2d::SindreImage::decode(*bytes);
if (!decoded)
    return;
```

`show()` 使用 OpenCV HighGUI，默认窗口名为 `sindre_image`，默认等待窗口关闭。
无桌面环境或窗口后端不可用时返回结构化错误；因此服务器和 CI 应优先使用保存或
tensor API，不要在后台任务中调用 `show()`。

启用日志时，`SindreImage` 会按需创建固定名称的 logger
`sindre.utils_2d.image`。它继承 General 默认 logger 的输出目标和格式，不会调用
`init_log()`，也不会替换宿主程序的全局默认 logger；图像操作失败会保留 `Result` 错误，
日志失败不会改变图像操作结果。

独立示例位于 [`examples/utils_2d_image`](../../examples/utils_2d_image)，构建命令：

```powershell
cmake -S examples/utils_2d_image -B build_utils_2d_image -G Ninja
cmake --build build_utils_2d_image --parallel
build_utils_2d_image/sindre_example_utils_2d_image.exe 1.png resized.png
```

```cpp
#include <sindre/utils_2d.h>

sindre::general::Result<void> detect_contours(const std::string& path) {
    auto image = sindre::utils_2d::load_image(path);
    if (!image) return sindre::general::Result<void>::failure(image.error());

    auto edges = sindre::utils_2d::detect_edges(
        image.value(), sindre::utils_2d::EdgeAlgorithm::canny);
    if (!edges) return sindre::general::Result<void>::failure(edges.error());

    auto contours = sindre::utils_2d::find_contours(edges.value());
    if (!contours) return sindre::general::Result<void>::failure(contours.error());
    return sindre::general::Result<void>::success();
}
```
