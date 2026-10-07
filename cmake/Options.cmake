if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(sindre_build_extras_default ON)
else()
    set(sindre_build_extras_default OFF)
endif()

# The supported product platforms are deliberately limited to Windows and
# Linux/WSL. Fail during configuration instead of compiling an untested
# partial backend on macOS or another Unix-like system.
if(APPLE OR NOT (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    message(FATAL_ERROR
        "sindrecpp supports Windows and Linux/WSL only; macOS and other platforms are unsupported")
endif()

# The fixed static dependency profiles are Release ABI packages.  Make a
# single-config generator safe by selecting Release when the caller did not
# choose a build type explicitly; a Debug selection would mix /MTd vcpkg
# archives with the library's fixed /MT profile on Windows.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "sindrecpp single-config build type" FORCE)
endif()

option(SINDRE_BUILD_TESTS "Build sindre tests" ${sindre_build_extras_default})
option(SINDRE_BUILD_BENCHMARKS "Build performance benchmarks" OFF)
option(SINDRE_BUILD_EXAMPLES "Build sindre examples" ${sindre_build_extras_default})
option(SINDRE_NO_EXCEPTIONS "Build sindre core with compiler exception support disabled" OFF)

# Clang/clang-cl 是主要编译器 profile；具体编译器仍由 toolchain 或生成器选择。
# 下列选项只控制 sindre target 的统一警告和运行时策略。
option(SINDRE_ENABLE_WARNINGS "Enable sindre compiler warnings" ON)
option(SINDRE_WARNINGS_AS_ERRORS "Treat sindre warnings as errors" OFF)
# General uses the fixed static Windows package profile and matching static CRT.
set(SINDRE_MSVC_STATIC_RUNTIME ON)

# General is the mandatory foundation. Its public integrations and dependency
# profile are fixed; they are deliberately not user-selectable feature flags.
include("${SINDRE_THIRDS_DIR}/general/Dependencies.cmake")
set(SINDRE_WITH_GENERAL ON)
set(SINDRE_WITH_MATH ON)
option(SINDRE_GENERAL_BUILD_LIBRARY "Build the compiled General runtime library" ON)
option(SINDRE_GENERAL_SHARED "Build General as a shared library" OFF)
set(SINDRE_MATH_BLAS_BACKEND "OPENBLAS" CACHE STRING
    "Math BLAS backend: OPENBLAS or EIGEN")
set_property(CACHE SINDRE_MATH_BLAS_BACKEND PROPERTY STRINGS OPENBLAS EIGEN)
option(SINDRE_MATH_NATIVE_ARCH "Optimize Math for the local CPU" ON)
if(WIN32)
    set(sindre_openblas_default_root "${SINDRE_THIRDS_DIR}/math/openblas")
else()
    set(sindre_openblas_default_root "${SINDRE_THIRDS_DIR}/math/openblas-linux")
endif()
set(SINDRE_MATH_OPENBLAS_ROOT "${sindre_openblas_default_root}" CACHE PATH
    "Fixed platform OpenBLAS root used by Math")
# 仅把当前平台的固定 profile 加入查找路径，绝不混用其他平台二进制包。
list(PREPEND CMAKE_PREFIX_PATH "${SINDRE_GENERAL_PACKAGE_ROOT}")
# 固定 Windows Crashpad profile 使用 Release STL iterator ABI。
# 所有项目和内置依赖统一该 ABI，避免 Debug 链接时混用 iterator 调试级别。
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
endif()
option(SINDRE_WITH_AI "Build the AI module" OFF)
option(SINDRE_WITH_GUI "Build the GUI module" OFF)
option(SINDRE_WITH_UTILS_2D "Build OpenCV 2D utilities" OFF)
option(SINDRE_WITH_UTILS_3D "Build VTK 3D utilities" OFF)

# All General integrations are mandatory and come from the fixed dependency
# profile declared in thirds/general/Dependencies.cmake.
set(SINDRE_WITH_LOG ON)
set(SINDRE_WITH_HTTP ON)
set(SINDRE_HTTP_OPENSSL ON)
set(SINDRE_WITH_JSON ON)
set(SINDRE_WITH_CLI ON)
set(SINDRE_WITH_RE2 ON)
set(SINDRE_WITH_CRASHPAD ON)
set(SINDRE_WITH_ZLIB ON)
set(SINDRE_WITH_CRYPTO ON)

# Python/NumPy utilities. This is a standalone module because it has a
# separate interpreter/extension dependency profile from General.
option(SINDRE_WITH_UTILS_PY "Build the Python/NumPy utilities module" OFF)

# GUI options.
option(SINDRE_GUI_GLFW_OPENGL3 "Enable the GLFW/OpenGL3 GUI backend" ON)
option(SINDRE_GUI_STB_IMAGE "Enable stb_image image loading" ON)
option(SINDRE_BUILD_GUI_RUNTIME_TESTS "Run a real GUI window test" OFF)
set(SINDRE_IMGUI_VERSION "1.92.9b" CACHE STRING "Dear ImGui version")
set(SINDRE_IMGUI_SOURCE_DIR "" CACHE PATH "Existing Dear ImGui source tree")
set(SINDRE_STB_IMAGE_ROOT "" CACHE PATH "Existing stb_image include root")

# AI options.
option(SINDRE_AI_ONNXRUNTIME "Enable ONNX Runtime inside AI" ON)
option(SINDRE_AI_TRT "Enable TensorRT inside AI" OFF)
option(SINDRE_AI_CUDA "Compile ONNX Runtime CUDA support" OFF)
option(SINDRE_BUILD_GPU_TESTS "Run TensorRT tests on a real GPU" OFF)
set(SINDRE_TENSORRT_ROOT "" CACHE PATH "TensorRT 10.x SDK root")
set(SINDRE_AI_TRT_RUNTIME "FULL" CACHE STRING
    "TensorRT runtime link mode: FULL or DISPATCH")
set_property(CACHE SINDRE_AI_TRT_RUNTIME PROPERTY STRINGS FULL DISPATCH)
set(SINDRE_ONNXRUNTIME_ROOT "" CACHE PATH "ONNX Runtime C/C++ SDK root")
set(SINDRE_CUDNN_ROOT "" CACHE PATH "cuDNN runtime root")
set(sindre_bundled_onnxruntime_root
    "${SINDRE_THIRDS_DIR}/ai/onnxruntime/1.22.0/onnxruntime-win-x64-1.22.0")
if(NOT SINDRE_ONNXRUNTIME_ROOT
   AND EXISTS "${sindre_bundled_onnxruntime_root}/include/onnxruntime_cxx_api.h")
    set(SINDRE_ONNXRUNTIME_ROOT "${sindre_bundled_onnxruntime_root}" CACHE PATH
        "ONNX Runtime C/C++ SDK root" FORCE)
endif()

# Utils_3d options.
option(SINDRE_UTILS_3D_SHOW "Enable standalone show_mesh rendering" OFF)
option(SINDRE_UTILS_3D_VTK_DATA "Enable VTK datasets and image processing" OFF)
foreach(backend IN ITEMS MESHLIB CGAL OPEN3D IGL VCG)
    option(SINDRE_UTILS_3D_${backend} "Enable optional ${backend} geometry algorithms" OFF)
endforeach()
set(SINDRE_VCG_ROOT "" CACHE PATH "VCGlib source root")
set(SINDRE_IGL_ROOT "" CACHE PATH "libigl source root")
set(SINDRE_MESHLIB_ROOT "" CACHE PATH "MeshLib SDK root")
set(SINDRE_OPEN3D_ROOT "" CACHE PATH "Open3D SDK root")
foreach(sindre_utils_3d_sdk IN ITEMS MESHLIB OPEN3D)
    string(TOLOWER "${sindre_utils_3d_sdk}" _sindre_utils_3d_sdk_lower)
    set(_sindre_utils_3d_default_root
        "${SINDRE_THIRDS_DIR}/utils_3d/${_sindre_utils_3d_sdk_lower}")
    if(NOT SINDRE_${sindre_utils_3d_sdk}_ROOT
       AND IS_DIRECTORY "${_sindre_utils_3d_default_root}")
        set(SINDRE_${sindre_utils_3d_sdk}_ROOT "${_sindre_utils_3d_default_root}"
            CACHE PATH "${sindre_utils_3d_sdk} SDK root" FORCE)
    endif()
    if(SINDRE_${sindre_utils_3d_sdk}_ROOT)
        list(PREPEND CMAKE_PREFIX_PATH "${SINDRE_${sindre_utils_3d_sdk}_ROOT}")
    endif()
endforeach()
option(SINDRE_UTILS_3D_NATIVE_ARCH "Optimize utils_3d for the build machine" OFF)
set(SINDRE_UTILS_3D_IGL_PACKAGE_FOUND OFF CACHE INTERNAL
    "Whether Utils_3d uses an installed libigl package")
option(SINDRE_BUILD_UTILS_3D_BENCHMARKS "Build the utils_3d benchmark" OFF)

if(SINDRE_WITH_UTILS_2D OR SINDRE_WITH_UTILS_3D OR SINDRE_WITH_UTILS_PY)
    message(STATUS "General and Math are mandatory foundations for all utility modules")
endif()
