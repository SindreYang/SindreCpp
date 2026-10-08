# utils_2d module

本文面向需要 OpenCV 图像处理、传统视觉算法和 Tensor 预处理的使用者，
说明模块 target 和启用方式；依赖要求见 [Utils_2d 依赖说明](../dependencies/utils_2d.md)。

`sindre::utils_2d` is a static library target. 图像 I/O、预处理和算法接口统一使用
`sindre::general::Result<T>` 表达失败。公共头只使用 Sindre 自有的 `Image`、几何
结构和 `Matrix`；OpenCV 只在静态库实现中加载，不会进入消费者的编译接口。

Enable with `SINDRE_WITH_UTILS_2D=ON`. CMake discovers an installed OpenCV 4
SDK with matching `opencv_contrib` modules and registers `sindre.utils_2d` when tests are enabled. The module is
independent from the AI and Utils_3d backends.

On this Windows machine the official OpenCV 4.12.0 SDK was found, but it is a
plain build without contrib and is therefore rejected by the full module. For
an official archive, `OpenCV_DIR` must point to the directory containing
`OpenCVConfig.cmake`, usually `.../build/x64/vc16/lib`, not its parent. The
OpenCV DLL directory must also be on `PATH` when an executable starts; the
module copies it beside its own tests when the directory can be inferred.

稳定 facade 当前覆盖滤波、阈值、形态学、边缘、轮廓、连通域、霍夫线/圆、NMS、
仿射/透视变换、ORB/SIFT/AKAZE/FAST/GFTT 特征、BF/FLANN 匹配、单应性、
Lucas-Kanade 稀疏跟踪、去畸变、相机标定、ArUco 和 NCHW tensor；BRIEF/FREAK、
Farneback/RLOF 及 dense flow 因为公共 `Image` 只承载拥有的 8-bit 像素，当前明确
返回 `function_not_supported`。不会把 OpenCV 对象泄漏给调用方。基础测试覆盖 Unicode
路径、读写、letterbox、NCHW tensor 和核心算法调用。OpenCV 4.8 或更高版本必须是带匹配
版本 contrib 的构建，缺少 contrib 模块时配置阶段直接失败。完整运行测试仍需要可运行的
CPU 或 CUDA DLL 集合。

## SindreImage 高级封装

`Image` 是由模块拥有的连续 8-bit 图像数据结构，包含宽、高、通道数和像素字节。
普通图像处理可以使用 `SindreImage`，它复用同一组私有 OpenCV 后端和自由函数：

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

`operator<<` 只输出图像信息，不会输出像素数据。

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

独立示例位于 [`examples/utils_2d_image`](../../examples/utils_2d_image)，构建命令：

```powershell
cmake -S examples/utils_2d_image -B build_win/examples/utils_2d_image -G "Visual Studio 17 2022" -A x64 -T ClangCL
cmake --build build_win/examples/utils_2d_image --config RelWithDebInfo --parallel
build_win/examples/utils_2d_image/bin/sindre_example_utils_2d_image.exe 1.png resized.png
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
