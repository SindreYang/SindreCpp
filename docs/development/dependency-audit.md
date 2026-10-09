# 依赖与完整性审计

本文记录当前仓库依赖的来源策略和实际验证边界。它描述已验证事实，不能替代
模块文档中的安装步骤。

## 来源策略

| 模块 | 依赖 | 来源策略 | 固定规则 |
| --- | --- | --- | --- |
| General | CsString、spdlog、cpp-httplib、simdjson、argparse、Abseil、RE2、zlib | 项目第三方缓存中的源码构建 | 固定 URL、版本和 SHA256；不从 vcpkg/系统替换 |
| General | OpenSSL、Crashpad | 固定平台二进制 profile | 允许 vcpkg profile；版本和 triplet 固定 |
| Math | Eigen、OpenBLAS | 项目第三方缓存中的源码构建 | 固定 URL、版本和 SHA256；默认 OpenBLAS |
| GUI | ImGui、GLFW、stb_image | 项目第三方缓存中的源码 | 固定 URL、版本/提交和 SHA256；不使用 vcpkg |
| GUI | OpenGL | 操作系统 SDK | 由平台提供，不能由项目替换 |
| Utils_2d | OpenCV 4.12.0 + contrib (Windows); 4.6+ (Linux) | 外部 SDK / 系统包 | Windows `EXACT CONFIG`，Linux minimum config，不自动下载；Linux 缺少 xfeatures2d 时 BRIEF/FREAK 明确返回不支持 |
| Utils_3d | nanoflann | 项目第三方缓存中的源码 | 固定版本和 SHA256；不使用 vcpkg |
| Utils_3d | VTK 9.7.1 (Windows); 9.1+ (Linux) | 官方外部 SDK / Linux 系统包 | Windows exact，Linux minimum；不自动下载 |
| Utils_3d | CGAL 6.2.1 (Windows); 5.6+ (Linux) | 官方独立 SDK / Linux 系统包 | CGAL 禁止使用 vcpkg；Windows 根目录显式指定，Linux 可由系统发现 |
| Utils_3d | PCL 1.15.1 (Windows); 1.14+ (Linux) | 外部 SDK / vcpkg / 系统包 | 可选后端；Windows exact，Linux minimum |
| AI | ONNX Runtime 1.22+、TensorRT 10.11+、CUDA 12+、cuDNN | 外部 SDK / 系统安装 | 大型二进制不由项目源码构建；版本检查和路径显式化 |
| Utils_Py | pybind11 | 项目第三方缓存中的源码 | 固定 URL、版本和 SHA256 |
| Utils_Py | CPython 3.12+、NumPy | Python 安装或虚拟环境 | 运行时由用户/宿主提供，CMake 检查版本和 Embed 开发组件 |

## 强制检查

- 小型源码依赖必须由 `3rdparty/<name>/*.cmake` 注册，并包含固定 SHA256。
- `CGAL_DIR`、Boost、GMP、MPFR 的解析路径含有 vcpkg 时，配置立即失败。
- Windows Utils_3d 的 CGAL 配置必须使用独立 `CGALConfig.cmake`，不能依赖隐式
  `CMAKE_PREFIX_PATH` 回退；Linux 可以使用系统 CGAL，但仍拒绝 vcpkg CGAL。
- 安装后的 Math、GUI、Utils2d、Utils3d、UtilsPy 和 AI target 不应要求消费者
  重新发现项目已经携带的源码小依赖；大型 SDK 仍按各模块文档提供。
- 公共 `include/sindre/` 不应包含 VTK、CGAL、PCL、OpenCV 或 ONNX Runtime
  类型。Math 和 Utils_Py 的 Eigen/pybind11 入口是有意提供的互操作边界。
- GUI 的 ImGui、GLFW、stb_image 以及 Utils_3d 的 nanoflann 不接受自定义
  源码目录、系统安装或 vcpkg 替换；旧的覆盖变量会在配置阶段报错。

## 固定源码归档核验

在 Windows `build_win/all-modules-static-audit` 的第三方下载缓存中，以下
归档已用 `Get-FileHash -Algorithm SHA256` 重新计算，并与各自配方的
`URL_HASH` 完全一致：

| 依赖 | 版本归档 | SHA256 |
| --- | --- | --- |
| Abseil | 20240116.2 | `69909dd729932cbbabb9eeaff56179e8d124515f5d3ac906663d573d700b4c7d` |
| argparse | v3.2 | `14c1a0e975d6877dfeaf52a1e79e54f70169a847e29c7e13aa7fe68a3d0ecbf1` |
| cpp-httplib | v0.56.0 | `a8c0ed8e198b71eed9ef55e9a69de31be13b9519959fac4d20abbe60a828fa45` |
| CsString | string-1.4.1 | `69b2cf7f848eb42038bf0c892a4aa18af9baa11ce2d67377d78a80d9248e202e` |
| Eigen | 3.4.1 | `b93c667d1b69265cdb4d9f30ec21f8facbbe8b307cf34c0b9942834c6d4fdbe2` |
| OpenBLAS | 0.3.34 | `cd7e129868320cc2d033afa920e31202dfe0b8066a5b66661900ccc0f197dfed` |
| pybind11 | v3.1.0 | `affea1ada7b39fe1d835559fcb78800c7927d514bd480ed01b71b88205a5e536` |
| RE2 | 2024-04-01 | `3dbed752e5c699508b0f88d700a47ad7072cd42cb2d504369f8e3ee01f32a09d` |
| simdjson | v4.6.11 | `5a3bc470d53b0324dfcbef952fdbd004655d7a4f7a6558fb94122cb9f233f2d0` |
| spdlog | v1.17.0 | `b11912a82d149792fef33fabd0503b13d54aeac25c1464755461d4108ea71fc2` |
| zlib | v1.3.1 | `17e88863f3600672ab49182f217281b6fc4d3c762bde361935e436a95214d05c` |

GLFW、ImGui、stb 和 nanoflann 使用 FetchContent 的固定归档/提交和
`URL_HASH`，同一构建缓存中的实际归档核验结果如下：

| 依赖 | SHA256 |
| --- | --- |
| GLFW 3.4 | `a133ddc3d3c66143eba9035621db8e0bcf34dba1ee9514a9e23e96afd39fd57a` |
| ImGui v1.92.9b | `e1c46d676c2bcb7ced847ba27f50553e33a19db97b3cadaec7f8be64449139f8` |
| nanoflann v1.8.0 | `da72953234936c0dde0b02b2edea1cbbdbb69b72de3f808dbc8ea0b515eacccd` |
| stb pinned commit | `8e59f72b0780690cda64726804269f638a3be77b9d1506ea95f443f7964bccf0` |

这些依赖不走 vcpkg，也不从默认系统搜索路径选择。

## 已完成的实测矩阵

| 环境 | 配置 | 结果 |
| --- | --- | --- |
| Windows + clang-cl | General、Math、AI、GUI、Utils_2d、Utils_3d、Utils_Py | 构建通过，CTest 23/23 |
| Windows + clang-cl 独立 SDK | Utils_3d + VTK 9.7.1 + CGAL 6.2.1 + Boost 1.86 + GMP/MPFR | 全新源码依赖缓存构建通过，CTest 9/9；CGAL 未使用 vcpkg |
| Windows + clang-cl PCL profile | Utils_3d + PCL 1.15.1（vcpkg 固定 profile）+ VTK 9.7.1 + CGAL 6.2.1 + Boost 1.86 + GMP/MPFR | 全新配置、固定小依赖源码构建和链接通过，CTest 9/9；当前环境未发现 OpenMP，PCL 的 OpenMP 加速功能未启用 |
| Windows + clang-cl OpenCV SDK | Utils_2d + OpenCV 4.12.0（含 contrib 组件） | 全新源码依赖缓存构建通过，CTest 10/10 |
| Windows + clang-cl OpenCV 4.12 isolated vcpkg prefix | Utils_2d + OpenCV 4.12.0/contrib、固定源码 General/Math/OpenBLAS | OpenCV 4.12.0 从历史 vcpkg port overlay 编译安装；显式提供 `Protobuf_PROTOC_EXECUTABLE` 后配置通过，完整构建 119/119，CTest 10/10；BRIEF/FREAK xfeatures2d 测试实际运行通过 |
| Windows + clang-cl ONNX Runtime SDK | AI + ONNX Runtime GPU 1.26.0 | 全新源码依赖缓存构建通过，CTest 12/12；实际 ORT DLL 加载测试通过 |
| Windows + clang-cl TensorRT SDK | AI + TensorRT 10.11.0.33 + CUDA 12.9 | FULL 兼容 engine 构建和真实 RTX 3060 Laptop GPU 推理通过，CTest 1/1；DISPATCH 使用外部 lean runtime 加载同一 engine，CTest 1/1 |
| Windows + clang-cl GUI runtime | GUI + 真实 GLFW/OpenGL3/ImGui 窗口生命周期 | 真实窗口测试通过；重复活动实例返回 `device_or_resource_busy` |
| Windows 安装消费者 | General/AI、Math、GUI、Utils_2d、Utils_3d、Utils_Py | 使用匹配的 clang-cl ABI，编译和实际运行通过；独立 SDK Utils_3d consumer 1/1，PCL profile 安装 consumer 1/1 |
| Linux/WSL + GCC 13 | General、Math、Examples | 构建通过，CTest 8/8 |
| Linux/WSL + Clang 18 | General、Math、Examples | 当前源码依赖全新构建（含 OpenBLAS 0.3.34），CTest 8/8 |
| Linux/WSL Ubuntu 24.04 + Clang 18 | Utils_2d + OpenCV 4.6.0/contrib、Utils_3d + VTK 9.1、CGAL 5.6、系统 Boost/GMP/MPFR、nanoflann | 构建通过，CTest 11/11；系统 OpenCV 未提供 xfeatures2d，BRIEF/FREAK 按约定返回 `function_not_supported` |
| Linux/WSL Ubuntu 24.04 + Clang 18 + PCL | Utils_3d + PCL 1.14、VTK 9.1、CGAL 5.6、系统 Boost/GMP/MPFR、libomp-18 | 配置、构建通过，CTest 12/12；新增 PCL 过滤、平面分割、欧氏聚类实测；安装包消费者配置、编译和运行 1/1 |
| Linux/WSL Ubuntu 24.04 + Clang 18 optional full | GUI、Utils_2d、Utils_3d/PCL、Utils_Py + GLFW/ImGui、OpenCV 4.6、VTK 9.1、CGAL 5.6、PCL 1.14、Python 3.12 | 191/191 个构建步骤通过，CTest 15/15；GUI 模块测试、Utils_2d、Utils_3d/PCL、Utils_Py runtime/NumPy 均运行通过；未开启 Linux 真实窗口测试，AI 未启用 |
| Linux/WSL Ubuntu 24.04 + Clang 18 AI CPU | AI + 官方 ONNX Runtime Linux x64 1.22.0 SDK | 全新 SDK 解压、配置和构建通过，CTest 12/12；安装包 AI consumer 配置、编译和实际推理 1/1；未启用 CUDA/TensorRT |
| Linux/WSL + Clang 18 preset（WSL ext4） | `linux-clang-core` preset、General、Math、Examples | 通过 preset 配置、固定源码依赖下载/构建和运行，CTest 8/8 |
| Linux/WSL 严格无异常 + Clang 18 | General、Math | 当前固定源码依赖全新构建，Eigen 后端，使用 `-fno-exceptions`，CTest 6/6 |
| Windows + clang-cl 严格无异常 | General、Math | 全新 ExternalProject profile（源码下载、校验、构建），CTest 6/6 |

CGAL 的拒绝路径也经过配置级验证：将 `CGALConfig.cmake` 放在带有
`vcpkg` 路径片段的探针目录后，Utils_3d 在 `find_package(CGAL)` 之前直接失败；
不会把 vcpkg CGAL 当作备用包。显式传入 `SINDRE_UTILS_3D_CGAL_ROOT` 时，该
standalone root 会覆盖旧缓存中的 `CGAL_DIR`，避免复用 build 目录时混入旧包。

既有 Windows 全模块 profile 已重新构建固定 OpenBLAS 源码（2,406 个 OpenBLAS
目标）并通过 CTest 23/23。另一个隔离的 OpenCV 4.12 profile 本次重新构建了
2,406 个 OpenBLAS 目标并通过 CTest 10/10；随后修复了 Utils_2d 测试目标没有同步
xfeatures2d 编译宏的问题，BRIEF/FREAK 已实际运行通过。既有安装前缀还验证了
General+AI、Math、GUI、Utils_2d、Utils_3d 和 Utils_Py 六个外部消费者，均为配置、
编译和运行测试 1/1。
本次在同一源码状态下重新生成并增量构建了 Linux optional full profile，使用 Linux
允许的系统依赖版本，CMake 配置耗时约 329 秒，增量构建 19/19，CTest 15/15；
General、Math、GUI、Utils_2d、Utils_3d/PCL 和 Utils_Py 的运行测试全部通过。
Linux AI CPU profile 也在同一源码状态下重新构建了 8/8，CTest 12/12；ONNX Runtime
1.22 SDK 的普通、typed 和执行器测试均通过。当前 Windows 工作区外部 SDK 中，
OpenCV/VTK/CGAL/PCL、ONNX Runtime 1.26、CUDA 和 TensorRT 的原先 `D:\software`
路径均不存在；因此 Windows 全模块重新生成会在缺失 OpenCV 配置处停止，Windows
AI profile 会在缺失 `onnxruntime.lib` 处停止。这是外部 SDK 缺失，不是源码编译
错误；SDK 恢复或提供等价路径后必须重新执行对应 profile 的构建、CTest 和实际运行
验证。
Windows consumer 必须使用与安装包一致的 clang-cl/MSVC ABI；GNU-style `clang++`
不能链接该固定 MSVC ABI 包。

安装包消费者必须使用与构建包相匹配的 Windows ABI（当前验证为
`clang-cl`/MSVC ABI），并为大型外部依赖显式提供 SDK 配置路径。例如 General/AI
需要 OpenSSL 和 Crashpad 的配置前缀，Utils_2d 需要 `OpenCV_DIR`，Utils_3d
需要独立的 `CGAL_DIR`、Boost、GMP/MPFR 和 `VTK_DIR`。安装包不会把这些大型
SDK 偷渡为源码依赖，也不会从安装包目录猜测开发机路径。

Windows Utils_3d 使用独立 CGAL/Boost/GMP/MPFR SDK；Linux Utils_3d 已用系统
CGAL/Boost/GMP/MPFR、VTK 9.1、OpenCV 4.6 和 PCL 1.14 完成 Clang 实测。PCL 仅在明确的
固定 profile 中使用。当前 Windows 已完成 Utils_3d 的独立 SDK 配置，以及 PCL
profile 的配置、编译、链接和 CTest 9/9；Windows profile 中未检测到 OpenMP，因此不能
把该次验证表述为启用 OpenMP 加速的验证。Linux 使用系统 `libomp-18` 并完成了 PCL
后端运行测试。Windows 还完成了 Utils_2d、ONNX Runtime、TensorRT
FULL 和 DISPATCH 后端的独立配置、编译和运行测试。Linux AI CPU 已使用官方
ONNX Runtime 1.22.0 SDK 完成源码与安装消费者验证；当前仍未提供 Linux
CUDA/TensorRT SDK，因此没有 Linux GPU 实测，不能把 Windows AI 结果外推到
Linux。安装包消费者必须使用固定 General 依赖 profile；不应把系统 OpenSSL
3.0 当作固定 3.3.0 ABI 的替代品。

PCL 的 Windows 安装 consumer 还必须把同一 vcpkg profile 的前缀加入
`CMAKE_PREFIX_PATH`，因为 PCL 的 `PCLConfig.cmake` 会重新查找其 Eigen、FLANN 和
Qhull 配置；这不代表 General 或 Math 改用 vcpkg。当前该安装 consumer 已使用
固定 General profile、独立 CGAL/Boost/GMP/MPFR/VTK SDK 和 PCL vcpkg profile，完成
配置、编译和运行测试 1/1。Linux 安装 consumer 使用系统 PCL/VTK/CGAL/Boost/GMP/MPFR
和 MPI，并完成配置、编译和运行测试 1/1；由于 Ubuntu 的 VTK/PCL 导出依赖
`MPI::MPI_C`，该 consumer 显式启用 C 和 C++。

Windows clang-cl 的 ExternalProject profile 会把 `CMAKE_AR`、C/C++ compiler
archiver 一并纳入缓存指纹，并优先使用 Visual Studio LLVM 的 `llvm-lib.exe`。
这样不同编译器或开发者命令行中的旧 `lib.exe` 不会污染源码依赖的静态归档。
指纹在 archiver 选择完成后计算，连续重新配置不会把安装规则切换到另一个未构建的
缓存目录。
多个构建目录可以通过同一个 `SINDRE_THIRD_GENERAL_PACKAGE_CACHE_ROOT` 复用固定的
General 大依赖 profile；该路径必须由宿主或 CI 明确提供，不能依赖当前构建目录
中碰巧存在的包。

当前 WSL 系统 CMake/CTest 可执行文件仍依赖缺失的 `libssl.so.1.1`；本次使用隔离的
Python CMake 4.4.4 环境完成了 Linux/Clang 核心及 Utils_2d/Utils_3d 构建和正式
CTest。Linux AI 后端仍需要单独的 GPU SDK 环境。

Windows 是 ABI 和交付基线，继续使用项目锁定的 exact profile：OpenCV 4.12.0
(contrib)、VTK 9.7.1、CGAL 6.2.1 和 PCL 1.15.1。Windows 的 CGAL 必须来自
官方独立 SDK，不能用 vcpkg CGAL。

Linux/WSL 不要求与 Windows 使用同一版本。Linux 使用 minimum 规则，允许发行版或
用户提供的其他版本，只要满足最低版本、CMake package 能被发现，并通过该版本的
模块构建和 CTest：OpenCV >= 4.6、VTK >= 9.1、CGAL >= 5.6、PCL >= 1.14。
CGAL 在 Linux 可以使用系统包，但仍拒绝 vcpkg CGAL。当前已经实测 VTK 9.1、CGAL
5.6、PCL 1.14 和 OpenCV 4.6。PCL 实测需要 `SINDRE_MATH_NATIVE_ARCH=OFF` 和
系统 LLVM OpenMP，避免预编译 PCL/Eigen 与本机 SIMD 选项产生 ABI 不匹配。

本次审查时原先 `D:\software` 路径中的 Windows VTK 9.7.1、CGAL 6.2.1 和
PCL 1.15.1 SDK 已不存在，因此没有重新声称 Windows 的 VTK/CGAL/PCL 全量构建；
上表中这几项 Windows 结果是 SDK 尚存在时的已完成证据。OpenCV 4.12.0 则已通过
隔离的 vcpkg 历史 port overlay 前缀重新构建，并完成 Utils_2d 的 119/119 构建与
CTest 10/10。恢复其余 SDK，或提供等价的固定配置路径后，必须重新执行对应的
Windows 配置、构建、CTest 和安装消费者验证。

本次再次检查 Windows vcpkg：`pcl:x64-windows-static` 1.15.1 已安装并符合
项目要求；当前 vcpkg 的 OpenCV 4.14 和 VTK 9.3 不符合 Windows 的固定版本，
不能直接替代 OpenCV 4.12 和 VTK 9.7.1。vcpkg 的 CGAL 6.2.1 虽然版本匹配，
但项目明确禁止使用 vcpkg CGAL，仍必须使用独立 CGAL SDK。

随后使用同一隔离 CMake 环境完成了当前源码的 Linux/Clang 严格无异常核心构建：
`SINDRE_NO_EXCEPTIONS=ON`、`-fno-exceptions`、Math 的 Eigen 后端，General 和 Math
共 6/6 CTest 通过。该模式明确只覆盖 General/Math；AI 和 Utils_3d 仍因现有公开
抛异常便利 API 在配置阶段拒绝无异常组合。

在 WSL 的 `/mnt/f` Windows 挂载盘上，ExternalProject 并行解压偶发出现临时目录
重命名的 `Permission denied`；这属于 WSL 9P 文件系统限制，不是源码包校验或版本
错误。旧记录中曾把 Linux 构建目录临时放在 WSL ext4；当前规则要求验证完成后迁回
仓库 `build_linux/<profile>/`，不得把构建树长期留在 `/home`、容器卷或其他仓库外
位置。若发生该问题，可以在 ext4 临时构建后迁回，并在迁回后重新配置 CMake，因为
缓存中的绝对路径不会随目录移动自动修正。Windows clang-cl 和 WSL 中已完成的核心
构建不受此限制影响。

公共 API 完备性扫描未发现旧 `sindrecpp` 命名，也未发现 VTK、CGAL、OpenCV、PCL、
ONNX Runtime 或 TensorRT 头文件泄漏到非后端公共头。Math 的 Eigen 和 Utils_Py 的
pybind11 属于其明确的公共互操作 API。AI 与 Utils_3d 仍保留公开便利接口及内部
后端的直接异常路径；在这些路径迁移到 `Result` 之前，不应宣称全库已达到严格的
无异常公共边界。

## 运行时边界

静态链接 Sindre target 不代表第三方 DLL 自动进入应用目录。Windows 应用必须
按模块复制 OpenCV、VTK、GMP/MPFR、Python、ONNX Runtime、TensorRT/CUDA 等
大型运行时；安装消费者测试会显式设置其运行时路径并实际启动程序。
多个目标并行链接时，项目的运行库复制脚本使用进程锁保护同一个 DLL 目标，
避免 `CsString.dll` 等运行库发生并发覆盖。
