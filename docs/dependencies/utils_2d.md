# utils_2d dependencies

本文说明 Utils_2d 的 OpenCV 和 opencv_contrib 要求以及 CMake 查找方式。

Utils_2d uses a matching OpenCV 4 and opencv_contrib package and requires the
`core`, `imgproc`, `imgcodecs`, `highgui`, `features2d`, `calib3d`, `video`, `videoio`,
`objdetect`, `aruco`, `optflow`, `tracking` and `xfeatures2d` components. OpenCV is not downloaded automatically because it is
normally supplied by the application, operating system or an imaging SDK.

The required profile is OpenCV 4.8 or newer with a matching opencv_contrib source
tree. OpenCV 4.12.0 is a suitable target when the matching contrib modules are
included; the current plain OpenCV 4.12.0 SDK without contrib is not sufficient. Its import libraries are found through
`OpenCV_DIR`, while the runtime DLLs are under the SDK `bin` directory. A
consumer installation must ship those DLLs or provide the SDK `bin` directory
on `PATH`; static linking of `sindre::utils_2d` does not make OpenCV itself
static.

Configure with `OpenCV_DIR` or `CMAKE_PREFIX_PATH` when CMake cannot discover it.
The build uses config-mode discovery and checks `OpenCV_VERSION` after loading
the package, so SDKs that omit a separate `OpenCVConfigVersion.cmake` remain
supported.

```powershell
cmake --preset windows-clang-cl `
  -DOpenCV_DIR="D:/software/OpenCV/opencv-4.12.0/opencv/build/x64/vc16/lib" `
  -DSINDRE_WITH_UTILS_2D=ON -DSINDRE_BUILD_TESTS=ON
```

`OpenCV_DIR` 必须指向带 contrib 组件的 `OpenCVConfig.cmake` 所在目录；不能把
普通 OpenCV 构建与另一版本的 contrib 头文件或库混用。

`highgui` 用于 `sindre::utils_2d::SindreImage::show()`。服务器、CI 和无桌面
环境仍然可以使用加载、保存、预处理和 tensor 转换，但不应调用 `show()`；窗口
后端不可用时该函数返回 `Result` 错误。Windows 部署时除了 OpenCV 核心 DLL，还要
确保 HighGUI 所需的 OpenCV DLL 位于程序目录或 `PATH`。
