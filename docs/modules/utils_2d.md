# utils_2d module

本文面向需要 OpenCV 图像读取、裁剪、缩放、颜色转换和 Tensor 预处理的使用者，
说明模块 target 和启用方式；依赖要求见 [Utils_2d 依赖说明](../dependencies/utils_2d.md)。

`sindre::utils_2d` is a static library target. The public image API remains stable,
while the module now has a dedicated compiled translation unit. OpenCV remains
available through `sindre::utils_2d::native`, so the wrapper does not hide the
native `cv::Mat` API.

Enable with `SINDRE_WITH_UTILS_2D=ON`. CMake discovers an installed OpenCV 4
SDK and registers `sindre.utils_2d` when tests are enabled. The module is
independent from the AI and Utils_3d backends.

On this Windows machine OpenCV 4.12.0 was verified with clang-cl. For the
official archive, `OpenCV_DIR` must point to the directory containing
`OpenCVConfig.cmake`, usually `.../build/x64/vc16/lib`, not its parent. The
OpenCV DLL directory must also be on `PATH` when an executable starts; the
module copies it beside its own tests when the directory can be inferred.

The test suite covers Unicode paths, load/save, color conversion, letterbox
and NCHW tensor conversion. It passed with the OpenCV 4.12.0 SDK on Windows.
