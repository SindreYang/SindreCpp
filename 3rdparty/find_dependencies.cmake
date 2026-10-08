include_guard(GLOBAL)

# 第三方依赖总入口：只负责注册各依赖配方和公共 ExternalProject 参数。
# 依赖源码、下载包与构建结果均放在缓存目录，不进入项目源码和安装导出。

include(ExternalProject)

# This file is included by the root project. The registry path also remains
# available when a standalone example embeds sindre with add_subdirectory().
set(SINDRE_3RDPARTY_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH
    "sindre third-party dependency registry")

# Keep downloaded sources and generated external build trees out of the source
# tree. A caller may point this cache at a shared local/CI location; the
# default remains inside the build directory and is never installed or
# exported as part of sindre.
set(SINDRE_THIRD_PARTY_CACHE_DIR "${CMAKE_BINARY_DIR}/_third_party_cache" CACHE PATH
    "Local cache for downloaded third-party sources and generated build data")
set(SINDRE_THIRD_PARTY_DOWNLOAD_DIR "${SINDRE_THIRD_PARTY_CACHE_DIR}/downloads" CACHE PATH
    "ExternalProject download cache for sindre third-party dependencies")

# Common ExternalProject arguments, following Open3D's isolated third-party
# build model. Every external dependency is built with the project compiler and
# Release ABI, then consumed through an imported target.
set(SINDRE_EXTERNAL_PROJECT_CMAKE_ARGS
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DCMAKE_POLICY_DEFAULT_CMP0091=NEW)
if(MSVC)
    # find_dependencies.cmake is included before Options.cmake by the root
    # project.  Establish the fixed General ABI here as well, otherwise
    # ExternalProject dependencies silently default to /MD while Sindre's
    # public targets use /MT.
    if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY OR
       CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    endif()
    list(APPEND SINDRE_EXTERNAL_PROJECT_CMAKE_ARGS
        -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY})
endif()

# Flat registry: each dependency owns one small recipe file under 3rdparty.
# This root file is the only inclusion entry point.
include("${SINDRE_3RDPARTY_DIR}/general.cmake")
include("${SINDRE_3RDPARTY_DIR}/cs_string/cs_string.cmake")
include("${SINDRE_3RDPARTY_DIR}/spdlog/spdlog.cmake")
include("${SINDRE_3RDPARTY_DIR}/cpp_httplib/cpp_httplib.cmake")
include("${SINDRE_3RDPARTY_DIR}/simdjson/simdjson.cmake")
include("${SINDRE_3RDPARTY_DIR}/argparse/argparse.cmake")
include("${SINDRE_3RDPARTY_DIR}/re2/re2.cmake")
include("${SINDRE_3RDPARTY_DIR}/openssl/openssl.cmake")
include("${SINDRE_3RDPARTY_DIR}/zlib/zlib.cmake")
include("${SINDRE_3RDPARTY_DIR}/crashpad/crashpad.cmake")
include("${SINDRE_3RDPARTY_DIR}/eigen/eigen.cmake")
include("${SINDRE_3RDPARTY_DIR}/openblas/openblas.cmake")
include("${SINDRE_3RDPARTY_DIR}/onnxruntime/onnxruntime.cmake")
include("${SINDRE_3RDPARTY_DIR}/imgui/imgui.cmake")
include("${SINDRE_3RDPARTY_DIR}/opencv/opencv.cmake")
include("${SINDRE_3RDPARTY_DIR}/vtk/vtk.cmake")
include("${SINDRE_3RDPARTY_DIR}/cgal/cgal.cmake")
include("${SINDRE_3RDPARTY_DIR}/pcl/pcl.cmake")
include("${SINDRE_3RDPARTY_DIR}/pybind11/pybind11.cmake")
include("${SINDRE_3RDPARTY_DIR}/nanoflann/nanoflann.cmake")
