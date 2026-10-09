# 项目选项与平台约束集中定义在这里。模块 CMake 文件只消费这些选项，
# 不在各模块重复声明会改变整个工程行为的全局开关。
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
        "sindre supports Windows and Linux/WSL only; macOS and other platforms are unsupported")
endif()

# 固定静态依赖 profile 使用非 Debug 的 Release ABI。未指定构建类型时默认使用
# RelWithDebInfo；显式选择 Debug 时仍允许构建，但项目会继续使用 /MT 和
# `_ITERATOR_DEBUG_LEVEL=0`，避免把 /MTd 或 iterator-debug ABI 混入固定依赖。
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "sindre single-config build type" FORCE)
elseif(MSVC AND NOT CMAKE_CONFIGURATION_TYPES AND
       CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(WARNING
        "Sindre General uses the fixed Release dependency ABI in Debug configuration: "
        "MSVC runtime is /MT and _ITERATOR_DEBUG_LEVEL=0")
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
set(SINDRE_WITH_MATH ON)
option(SINDRE_GENERAL_BUILD_LIBRARY "Build the compiled General runtime library" ON)
option(SINDRE_GENERAL_SHARED "Build General as a shared library" OFF)
set(SINDRE_MATH_BLAS_BACKEND "OPENBLAS" CACHE STRING
    "Math BLAS backend: OPENBLAS or EIGEN")
set_property(CACHE SINDRE_MATH_BLAS_BACKEND PROPERTY STRINGS OPENBLAS EIGEN)
option(SINDRE_MATH_NATIVE_ARCH "Optimize Math for the local CPU" ON)
# 仅把当前平台的固定 profile 加入查找路径，绝不混用其他平台二进制包。
list(PREPEND CMAKE_PREFIX_PATH "${SINDRE_GENERAL_PACKAGE_ROOT}")
# 固定 Windows Crashpad profile 使用 Release STL iterator ABI。
# 所有项目和内置依赖统一该 ABI，避免 Debug 链接时混用 iterator 调试级别。
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
    # The fixed General profile contains Release static archives.  CMake does
    # not reliably infer RelWithDebInfo -> Release when a package exports only
    # Debug and Release configurations, so make that ABI choice explicit for
    # all dependencies resolved from the source tree.
    foreach(sindre_dependency_config IN ITEMS DEBUG RELWITHDEBINFO MINSIZEREL)
        set(CMAKE_MAP_IMPORTED_CONFIG_${sindre_dependency_config} Release
            CACHE STRING "Map ${sindre_dependency_config} to the fixed Release dependency profile" FORCE)
    endforeach()
endif()
option(SINDRE_WITH_AI "Build the AI module" OFF)
option(SINDRE_WITH_GUI "Build the GUI module" OFF)
option(SINDRE_WITH_UTILS_2D "Build OpenCV 2D utilities" OFF)
option(SINDRE_WITH_UTILS_3D "Build VTK + CGAL 3D utilities" ON)

# Python/NumPy utilities. This is a standalone module because it has a
# separate interpreter/extension dependency profile from General.
option(SINDRE_WITH_UTILS_PY "Build the Python/NumPy utilities module" OFF)

# GUI options.
option(SINDRE_GUI_GLFW_OPENGL3 "Enable the GLFW/OpenGL3 GUI backend" ON)
option(SINDRE_GUI_STB_IMAGE "Enable stb_image image loading" ON)
option(SINDRE_BUILD_GUI_RUNTIME_TESTS "Run a real GUI window test" OFF)
# ImGui and stb_image are small source dependencies. They are deliberately
# not replaceable by a system/vcpkg path; the recipes under 3rdparty pin the
# archive and SHA256 used by every build.
if(DEFINED SINDRE_IMGUI_VERSION AND
   NOT "${SINDRE_IMGUI_VERSION}" STREQUAL "" AND
   NOT "${SINDRE_IMGUI_VERSION}" STREQUAL "1.92.9b")
    message(FATAL_ERROR
        "SINDRE_IMGUI_VERSION is fixed to 1.92.9b; custom GUI dependency "
        "versions are not supported")
endif()
if(DEFINED SINDRE_IMGUI_SOURCE_DIR AND
   NOT "${SINDRE_IMGUI_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR
        "SINDRE_IMGUI_SOURCE_DIR is not supported; GUI uses the fixed pinned "
        "ImGui source recipe")
endif()
if(DEFINED SINDRE_STB_IMAGE_ROOT AND
   NOT "${SINDRE_STB_IMAGE_ROOT}" STREQUAL "")
    message(FATAL_ERROR
        "SINDRE_STB_IMAGE_ROOT is not supported; GUI uses the fixed pinned "
        "stb source recipe")
endif()
set(SINDRE_IMGUI_VERSION "1.92.9b")
set(SINDRE_IMGUI_SOURCE_DIR "")
set(SINDRE_STB_IMAGE_ROOT "")

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
    "${SINDRE_THIRD_PARTY_CACHE_DIR}/ai/onnxruntime/1.22.0/onnxruntime-win-x64-1.22.0")
if(NOT SINDRE_ONNXRUNTIME_ROOT
   AND EXISTS "${sindre_bundled_onnxruntime_root}/include/onnxruntime_cxx_api.h")
    set(SINDRE_ONNXRUNTIME_ROOT "${sindre_bundled_onnxruntime_root}" CACHE PATH
        "ONNX Runtime C/C++ SDK root" FORCE)
endif()

# Utils_3d options. VTK, CGAL and nanoflann are fixed dependencies whenever
# Utils_3d is built. CGAL is deliberately not a feature switch: a reduced
# build would make the module's behavior depend on an accidental cache value.
if(DEFINED SINDRE_UTILS_3D_CGAL AND NOT SINDRE_UTILS_3D_CGAL)
    message(FATAL_ERROR
        "SINDRE_UTILS_3D_CGAL=OFF is no longer supported. Utils_3d requires "
        "the fixed standalone CGAL SDK; see docs/guides/vtk.md for installation.")
endif()
set(SINDRE_UTILS_3D_CGAL ON)
option(SINDRE_UTILS_3D_PCL "Enable PCL point-cloud algorithms" OFF)
set(SINDRE_UTILS_3D_PCL_ROOT "" CACHE PATH
    "PCL SDK/package root used by the optional point-cloud backend")
set(SINDRE_UTILS_3D_CGAL_ROOT "" CACHE PATH
    "Official standalone CGAL SDK root used by the CGAL backend")
set(SINDRE_UTILS_3D_BOOST_ROOT "" CACHE PATH
    "Official standalone Boost root used by the CGAL backend")
set(SINDRE_UTILS_3D_GMP_ROOT "" CACHE PATH
    "Official standalone GMP root used by the CGAL backend")
set(SINDRE_UTILS_3D_MPFR_ROOT "" CACHE PATH
    "Official standalone MPFR root used by the CGAL backend")
set(_sindre_utils_3d_default_pcl_root
    "${SINDRE_THIRD_PARTY_CACHE_DIR}/utils_3d/pcl")
if(NOT SINDRE_UTILS_3D_PCL_ROOT
   AND IS_DIRECTORY "${_sindre_utils_3d_default_pcl_root}")
    set(SINDRE_UTILS_3D_PCL_ROOT "${_sindre_utils_3d_default_pcl_root}"
        CACHE PATH "PCL SDK/package root" FORCE)
endif()
if(SINDRE_UTILS_3D_PCL_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${SINDRE_UTILS_3D_PCL_ROOT}")
endif()
option(SINDRE_UTILS_3D_NATIVE_ARCH "Optimize utils_3d for the build machine" OFF)
option(SINDRE_BUILD_UTILS_3D_BENCHMARKS "Build the utils_3d benchmark" OFF)

# These modules still expose legacy throwing convenience APIs and private
# backends that use exceptions. Reject a misleading partial no-exception build
# until those APIs are migrated to Result-returning entry points.
if(SINDRE_NO_EXCEPTIONS AND (SINDRE_WITH_AI OR SINDRE_WITH_UTILS_3D))
    message(FATAL_ERROR
        "SINDRE_NO_EXCEPTIONS currently supports General and Math only. "
        "Disable SINDRE_WITH_AI and SINDRE_WITH_UTILS_3D, or build those "
        "modules with exception support until their throwing APIs are migrated.")
endif()

if(SINDRE_WITH_UTILS_2D OR SINDRE_WITH_UTILS_3D OR SINDRE_WITH_UTILS_PY)
    message(STATUS "General and Math are mandatory foundations for all utility modules")
endif()
