if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(sindre_build_extras_default ON)
else()
    set(sindre_build_extras_default OFF)
endif()

option(SINDRE_BUILD_TESTS "Build sindre tests" ${sindre_build_extras_default})
option(SINDRE_BUILD_BENCHMARKS "Build performance benchmarks" OFF)
option(SINDRE_BUILD_EXAMPLES "Build sindre examples" ${sindre_build_extras_default})
option(SINDRE_NO_EXCEPTIONS "Build sindre core with compiler exception support disabled" OFF)

# Clang/clang-cl is the primary supported compiler profile. CMake still uses
# the compiler selected by the toolchain or generator; these options only
# control the common warning and runtime policy applied to sindre targets.
option(SINDRE_ENABLE_WARNINGS "Enable sindre compiler warnings" ON)
option(SINDRE_WARNINGS_AS_ERRORS "Treat sindre warnings as errors" OFF)
option(SINDRE_MSVC_STATIC_RUNTIME "Use the static MSVC runtime" OFF)

# General is the mandatory foundation. Its public integrations and dependency
# profile are fixed; they are deliberately not user-selectable feature flags.
set(SINDRE_WITH_GENERAL ON)
set(SINDRE_WITH_EIGEN ON)
option(SINDRE_GENERAL_BUILD_LIBRARY "Build the compiled General runtime library" ON)
option(SINDRE_GENERAL_SHARED "Build General as a shared library" OFF)
set(SINDRE_EIGEN_BLAS_BACKEND "OPENBLAS" CACHE STRING
    "Eigen BLAS backend: AUTO, MKL, OPENBLAS, BLAS, or EIGEN")
set_property(CACHE SINDRE_EIGEN_BLAS_BACKEND PROPERTY STRINGS AUTO MKL OPENBLAS BLAS EIGEN)
option(SINDRE_EIGEN_NATIVE_ARCH "Optimize Eigen for the local CPU" ON)
set(SINDRE_OPENBLAS_ROOT "${SINDRE_THIRDS_DIR}/general/openblas")
if(NOT IS_DIRECTORY "${SINDRE_OPENBLAS_ROOT}")
    message(FATAL_ERROR
        "Fixed General dependency is missing: OpenBLAS at ${SINDRE_OPENBLAS_ROOT}")
endif()
list(PREPEND CMAKE_PREFIX_PATH "${SINDRE_OPENBLAS_ROOT}")
if(EXISTS "${SINDRE_OPENBLAS_ROOT}/bin")
    set(SINDRE_OPENBLAS_RUNTIME_DIR "${SINDRE_OPENBLAS_ROOT}/bin"
        CACHE INTERNAL "OpenBLAS runtime directory for Windows test targets")
endif()
if(EXISTS "${SINDRE_OPENBLAS_ROOT}/lib/cmake/openblas")
    set(OpenBLAS_DIR "${SINDRE_OPENBLAS_ROOT}/lib/cmake/openblas"
        CACHE PATH "OpenBLAS CMake package directory" FORCE)
endif()
set(SINDRE_GENERAL_PACKAGE_ROOT
    "${SINDRE_THIRDS_DIR}/general/packages/general-x64-windows/installed/x64-windows")
if(NOT IS_DIRECTORY "${SINDRE_GENERAL_PACKAGE_ROOT}/share")
    message(FATAL_ERROR
        "Fixed General dependency profile is missing: ${SINDRE_GENERAL_PACKAGE_ROOT}")
endif()
list(PREPEND CMAKE_PREFIX_PATH "${SINDRE_GENERAL_PACKAGE_ROOT}")
# The fixed Windows Crashpad profile is built with the release STL iterator
# ABI. Apply the same ABI to every project and bundled dependency target so
# Debug builds do not mix iterator-debug levels at link time.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
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
set(SINDRE_UTILS_3D_BLAS_BACKEND "AUTO" CACHE STRING "utils_3d BLAS backend: AUTO, EIGEN, or BLAS")
set_property(CACHE SINDRE_UTILS_3D_BLAS_BACKEND PROPERTY STRINGS AUTO EIGEN BLAS)
option(SINDRE_BUILD_UTILS_3D_BENCHMARKS "Build the utils_3d benchmark" OFF)

if(SINDRE_WITH_UTILS_2D OR SINDRE_WITH_UTILS_3D OR SINDRE_WITH_UTILS_PY)
    message(STATUS "General and Eigen are mandatory foundations for all utility modules")
endif()
