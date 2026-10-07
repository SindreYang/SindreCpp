# utils_2d dependencies

本文说明 Utils_2d 的 OpenCV 要求和 CMake 查找方式。它适合已经有 OpenCV SDK、
只需要把图像预处理接入 sindrecpp 的项目。

Utils_2d uses the installed OpenCV 4 package and requires the `core`, `imgproc`
and `imgcodecs` components. OpenCV is not downloaded automatically because it is
normally supplied by the application, operating system or an imaging SDK.

The verified local SDK is OpenCV 4.12.0. Its import libraries are found through
`OpenCV_DIR`, while the runtime DLLs are under the SDK `bin` directory. A
consumer installation must ship those DLLs or provide the SDK `bin` directory
on `PATH`; static linking of `sindre::utils_2d` does not make OpenCV itself
static.

Configure with `OpenCV_DIR` or `CMAKE_PREFIX_PATH` when CMake cannot discover it.

```powershell
cmake -S . -B build/windows-utils2d -G Ninja `
  -DOpenCV_DIR="D:/software/OpenCV/opencv-4.12.0/opencv/build/x64/vc16/lib" `
  -DSINDRE_WITH_UTILS_2D=ON -DSINDRE_BUILD_TESTS=ON
```
