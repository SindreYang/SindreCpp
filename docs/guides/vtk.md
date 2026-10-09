# 3D 后端说明

VTK 是 `utils_3d` 的内部实现依赖，不属于应用层 API。用户代码应使用：

```cpp
#include <sindre/utils_3d.h>
```

公共类型为 `Mesh`、`PointCloud` 和 `SindreMesh`。网格算法通过 `Result<T>` 返回，
应用不需要包含 VTK 头文件、连接 VTK 对象或选择后端。

VTK、CGAL、PCL 的版本、CMake 查找和运行时 DLL 配置只记录在
[utils_3d 依赖说明](../dependencies/utils_3d.md)中，供库维护者使用。

## Windows 固定依赖安装

Windows 的 `utils_3d` 固定使用 VTK `9.7.1`、CGAL `6.2.1` 和 nanoflann `1.8.0`，
版本不匹配时 CMake 会直接失败。Linux 使用 VTK `9.1+`、CGAL `5.6+` 的系统或
外部 SDK，CGAL 仍禁止使用 vcpkg；nanoflann 在所有平台继续使用固定源码版本。

## Linux 系统依赖

Ubuntu 24.04 可以直接安装大依赖的开发包：

```bash
sudo apt install libvtk9-dev libcgal-dev libboost-all-dev libgmp-dev libmpfr-dev \
  libopencv-dev libopencv-contrib-dev
```

然后使用系统 CMake 配置：

```bash
cmake -S . -B build_linux/utils3d-system -G Ninja \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DSINDRE_WITH_UTILS_3D=ON \
  -DSINDRE_BUILD_TESTS=ON
cmake --build build_linux/utils3d-system --parallel
ctest --test-dir build_linux/utils3d-system --output-on-failure
```

如果发行版的 VTK、CGAL 或 OpenCV 配置文件不在默认路径，使用对应的
`VTK_DIR`、`CGAL_DIR` 或 `OpenCV_DIR`。Linux 配置使用最低版本检查，不要求
与 Windows SDK 完全相同的版本；但缺少 `opencv_contrib` 组件仍会在配置阶段失败。

### VTK 9.7.1

使用已验证的 VTK Windows SDK，并把 SDK 的 `cmake` 目录传给 CMake：

```powershell
$vtkRoot = 'D:\software\VTK\vtk_sdk-9.7.1-cp312\vtk_sdk'
$env:PATH = "$vtkRoot\content\bin;$env:PATH"
```

确认以下文件存在：

```text
<vtkRoot>/cmake/vtk-config.cmake
<vtkRoot>/content/bin/*.dll
```

### CGAL 6.2.1

使用 CGAL 官方 `CGAL-6.2.1-library.zip` 和官方 Windows x64 GMP/MPFR 依赖包，
再使用项目固定的 ClangCL 工具链安装到独立前缀。CGAL 本身主要是 header-only，
GMP/MPFR 是运行时依赖。CGAL 配置不得使用 vcpkg 的 CGAL 包；Boost 也建议使用
完整的独立 Boost 头文件 SDK。CMake 4.4 下可通过 `cmake/BoostStandalone/`
提供的最小 `BoostConfig.cmake` 将独立 Boost 根目录交给 CGAL。

```powershell
$cgalSource = 'D:\software\CGAL\CGAL-6.2.1'
$cgalInstall = 'D:\software\CGAL\cgal-6.2.1-clang'
$boostRoot = 'D:\software\Boost\boost_1_86_0'
$gmpRoot = 'D:\software\CGAL\auxiliary-6.2.1\auxiliary\gmp'

cmake -S $cgalSource -B "$cgalSource\build\release" -G Ninja `
  -DCMAKE_C_COMPILER='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe' `
  -DCMAKE_CXX_COMPILER='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe' `
  -DCMAKE_BUILD_TYPE=Release `
  -DBOOST_ROOT="$boostRoot" `
  -DGMP_ROOT="$gmpRoot" `
  -DMPFR_ROOT="$gmpRoot" `
  -DCMAKE_INSTALL_PREFIX="$cgalInstall" `
  -DWITH_examples=OFF -DWITH_demos=OFF -DWITH_tests=OFF
cmake --build "$cgalSource\build\release" --target install --parallel 4
```

确认以下文件存在：

```text
<cgalInstall>/lib/cmake/CGAL/CGALConfig.cmake
<cgalInstall>/include/CGAL/version.h
```

### 构建 Utils_3d

```powershell
cmake -S . -B build_win/utils3d-cgal -G Ninja `
  -DCMAKE_C_COMPILER='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe' `
  -DCMAKE_CXX_COMPILER='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe' `
  -DVTK_DIR="$vtkRoot\cmake" `
  -DCGAL_DIR="$cgalInstall\lib\cmake\CGAL" `
  -DBoost_DIR="$PWD\cmake\BoostStandalone" `
  -DBOOST_ROOT="$boostRoot" `
  -DSINDRE_UTILS_3D_BOOST_ROOT="$boostRoot" `
  -DSINDRE_UTILS_3D_GMP_ROOT="$gmpRoot" `
  -DSINDRE_UTILS_3D_MPFR_ROOT="$gmpRoot" `
  -DGMP_ROOT="$gmpRoot" `
  -DMPFR_ROOT="$gmpRoot" `
  -DSINDRE_WITH_UTILS_3D=ON `
  -DSINDRE_UTILS_3D_PCL=OFF `
  -DSINDRE_BUILD_TESTS=ON
cmake --build build_win/utils3d-cgal --parallel 4
ctest --test-dir build_win/utils3d-cgal --output-on-failure
```

如果同时启用固定的 PCL 点云后端，Windows clang-cl 工具链还必须提供
`libomp.lib` 和 `libomp.dll`。CMake 会自动定位 LLVM 自带的 OpenMP runtime，
并在安装时复制 `libomp.dll` 到包的 `bin/` 目录。

仓库内部仍保留参考 vedo 设计的 VTK Mesh/Data/Image 封装，位置是
`modules/utils_3d/private/core/`；它用于实现 `Mesh`、`SindreMesh` 和后续算法，
不属于安装包的公共接口。

旧的 `utils_3d/vtk.h`、`data.h`、`image.h`、`show.h` 和 `plot.h` 接口属于历史实现，
不再作为稳定公共 API。需要新增数据集、显示或图像能力时，应先设计后端无关的高层类型，
再将第三方调用放入模块私有实现。
