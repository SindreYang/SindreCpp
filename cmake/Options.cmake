if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(sindrecpp_build_extras_default ON)
else()
    set(sindrecpp_build_extras_default OFF)
endif()

option(SINDRECPP_BUILD_TESTS "Build SindreCpp tests" ${sindrecpp_build_extras_default})
option(SINDRECPP_BUILD_EXAMPLES "Build SindreCpp examples" ${sindrecpp_build_extras_default})
option(SINDRECPP_NO_EXCEPTIONS "Build SindreCpp core with compiler exception support disabled" OFF)

# Clang/clang-cl is the primary supported compiler profile. CMake still uses
# the compiler selected by the toolchain or generator; these options only
# control the common warning and runtime policy applied to SindreCpp targets.
option(SINDRECPP_ENABLE_WARNINGS "Enable SindreCpp compiler warnings" ON)
option(SINDRECPP_WARNINGS_AS_ERRORS "Treat SindreCpp warnings as errors" OFF)
option(SINDRECPP_MSVC_STATIC_RUNTIME "Use the static MSVC runtime" OFF)

option(SINDRECPP_WITH_GENERAL "Build the General module" ON)
option(SINDRECPP_WITH_AI "Build the AI module" OFF)
option(SINDRECPP_WITH_GUI "Build the GUI module" OFF)
option(SINDRECPP_WITH_UTILS2D "Build OpenCV 2D utilities" OFF)
option(SINDRECPP_WITH_UTILS3D "Build VTK 3D utilities" OFF)

# General integrations. They remain implementation options of SindreCpp::General.
option(SINDRECPP_WITH_POINTER "Enable the CsPointer integration" OFF)
option(SINDRECPP_WITH_STRING "Enable the CsString integration" OFF)
option(SINDRECPP_WITH_LOG "Enable the spdlog integration" OFF)
option(SINDRECPP_WITH_UTILS_PY "Enable the pybind11 integration" OFF)
option(SINDRECPP_WITH_HTTP "Enable the cpp-httplib integration" OFF)
option(SINDRECPP_HTTP_OPENSSL "Enable HTTPS support in cpp-httplib when OpenSSL is available" ON)
option(SINDRECPP_WITH_JSON "Enable the simdjson integration" OFF)
option(SINDRECPP_WITH_CLI "Enable the argparse integration" OFF)
option(SINDRECPP_WITH_RE2 "Enable the RE2 regular-expression integration" OFF)
option(SINDRECPP_WITH_CRASHPAD "Enable the Crashpad integration" OFF)
option(SINDRECPP_WITH_ZLIB "Enable zlib compression helpers" OFF)
set(SINDRECPP_CRASHPAD_TARGET "" CACHE STRING "Existing Crashpad client target")

# GUI options.
option(SINDRECPP_GUI_GLFW_OPENGL3 "Enable the GLFW/OpenGL3 GUI backend" ON)
option(SINDRECPP_BUILD_GUI_RUNTIME_TESTS "Run a real GUI window test" OFF)
set(SINDRECPP_IMGUI_VERSION "1.92.9b" CACHE STRING "Dear ImGui version")
set(SINDRECPP_IMGUI_SOURCE_DIR "" CACHE PATH "Existing Dear ImGui source tree")

# AI options.
option(SINDRECPP_AI_ONNXRUNTIME "Enable ONNX Runtime inside AI" ON)
option(SINDRECPP_AI_TRT "Enable TensorRT inside AI" OFF)
option(SINDRECPP_AI_CUDA "Compile ONNX Runtime CUDA support" OFF)
option(SINDRECPP_BUILD_GPU_TESTS "Run TensorRT tests on a real GPU" OFF)
set(SINDRECPP_TENSORRT_ROOT "" CACHE PATH "TensorRT 10.x SDK root")
set(SINDRECPP_ONNXRUNTIME_ROOT "" CACHE PATH "ONNX Runtime C/C++ SDK root")
set(SINDRECPP_CUDNN_ROOT "" CACHE PATH "cuDNN runtime root")

# Utils3d options.
option(SINDRECPP_UTILS3D_SHOW "Enable standalone show_mesh rendering" OFF)
option(SINDRECPP_UTILS3D_VTK_DATA "Enable VTK datasets and image processing" OFF)
foreach(backend IN ITEMS MESHLIB CGAL OPEN3D IGL VCG)
    option(SINDRECPP_UTILS3D_${backend} "Enable optional ${backend} geometry algorithms" OFF)
endforeach()
set(SINDRECPP_VCG_ROOT "" CACHE PATH "VCGlib source root")
set(SINDRECPP_IGL_ROOT "" CACHE PATH "libigl source root")
set(SINDRECPP_MESHLIB_ROOT "" CACHE PATH "MeshLib SDK root")
option(SINDRECPP_UTILS3D_NATIVE_ARCH "Optimize utils3d for the build machine" OFF)
set(SINDRECPP_UTILS3D_BLAS_BACKEND "AUTO" CACHE STRING "utils3d BLAS backend: AUTO, EIGEN, or BLAS")
set_property(CACHE SINDRECPP_UTILS3D_BLAS_BACKEND PROPERTY STRINGS AUTO EIGEN BLAS)
option(SINDRECPP_BUILD_UTILS3D_BENCHMARKS "Build the utils3d benchmark" OFF)

if(NOT SINDRECPP_WITH_GENERAL)
    foreach(module IN ITEMS POINTER STRING LOG HTTP JSON CLI RE2 CRASHPAD ZLIB UTILS_PY)
        if(SINDRECPP_WITH_${module})
            message(FATAL_ERROR "${module} requires SINDRECPP_WITH_GENERAL=ON")
        endif()
    endforeach()
    if(SINDRECPP_WITH_GUI)
        message(FATAL_ERROR "SINDRECPP_WITH_GUI requires SINDRECPP_WITH_GENERAL=ON")
    endif()
    if(SINDRECPP_WITH_AI)
        message(FATAL_ERROR "SINDRECPP_WITH_AI requires SINDRECPP_WITH_GENERAL=ON")
    endif()
endif()
