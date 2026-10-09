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
if(NOT DEFINED SINDRE_THIRD_PARTY_CACHE_DIR OR
   SINDRE_THIRD_PARTY_CACHE_DIR STREQUAL "")
    set(SINDRE_THIRD_PARTY_CACHE_DIR "${CMAKE_BINARY_DIR}/_third_party_cache"
        CACHE PATH "Local cache for downloaded third-party sources and generated build data")
endif()
if(NOT DEFINED SINDRE_THIRD_PARTY_DOWNLOAD_DIR OR
   SINDRE_THIRD_PARTY_DOWNLOAD_DIR STREQUAL "")
    set(SINDRE_THIRD_PARTY_DOWNLOAD_DIR "${SINDRE_THIRD_PARTY_CACHE_DIR}/downloads"
        CACHE PATH "ExternalProject download cache for sindre third-party dependencies")
endif()

# Some clang toolchains report CMAKE_AR as NOTFOUND, or CMake finds an old
# PATH-level MSVC lib.exe that cannot run outside its original developer
# prompt. Prefer LLVM's archiver for clang-cl, including the LLVM tool shipped
# with Visual Studio, so every ExternalProject uses a self-contained archiver.
if(MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    get_filename_component(_sindre_clang_tool_dir
        "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_program(_sindre_llvm_lib
        NAMES llvm-lib llvm-lib.exe
        HINTS "${_sindre_clang_tool_dir}")
    if(NOT _sindre_llvm_lib)
        file(GLOB _sindre_vs_llvm_lib_candidates
            "C:/Program Files/Microsoft Visual Studio/*/*/VC/Tools/Llvm/x64/bin/llvm-lib.exe"
            "C:/Program Files (x86)/Microsoft Visual Studio/*/*/VC/Tools/Llvm/x64/bin/llvm-lib.exe")
        list(LENGTH _sindre_vs_llvm_lib_candidates _sindre_vs_llvm_lib_count)
        if(_sindre_vs_llvm_lib_count GREATER 0)
            list(GET _sindre_vs_llvm_lib_candidates 0 _sindre_llvm_lib)
        endif()
    endif()
    if(_sindre_llvm_lib AND EXISTS "${_sindre_llvm_lib}")
        set(CMAKE_AR "${_sindre_llvm_lib}" CACHE FILEPATH
            "LLVM archiver used by sindre and fixed dependencies" FORCE)
        # clang-cl's compiler information can otherwise reintroduce the
        # archiver selected from PATH in each ExternalProject child.
        set(CMAKE_C_COMPILER_AR "${_sindre_llvm_lib}" CACHE FILEPATH
            "LLVM C archiver used by sindre and fixed dependencies" FORCE)
        set(CMAKE_CXX_COMPILER_AR "${_sindre_llvm_lib}" CACHE FILEPATH
            "LLVM C++ archiver used by sindre and fixed dependencies" FORCE)
    endif()
endif()

# Promote the selected archiver to the project cache so both Sindre and
# ExternalProject static targets use the same tool.
if(NOT DEFINED CMAKE_AR OR "${CMAKE_AR}" STREQUAL "" OR
   "${CMAKE_AR}" MATCHES "-NOTFOUND$")
    if(MSVC)
        get_filename_component(_sindre_clang_tool_dir
            "${CMAKE_CXX_COMPILER}" DIRECTORY)
        if(EXISTS "${_sindre_clang_tool_dir}/llvm-lib.exe")
            set(CMAKE_AR "${_sindre_clang_tool_dir}/llvm-lib.exe"
                CACHE FILEPATH "MSVC-compatible archiver used by sindre" FORCE)
        endif()
    elseif(DEFINED CMAKE_CXX_COMPILER_AR AND
           NOT "${CMAKE_CXX_COMPILER_AR}" STREQUAL "" AND
           NOT "${CMAKE_CXX_COMPILER_AR}" MATCHES "-NOTFOUND$")
        set(CMAKE_AR "${CMAKE_CXX_COMPILER_AR}" CACHE FILEPATH
            "Archiver used by sindre and fixed dependencies" FORCE)
    endif()
endif()
if(NOT MSVC AND
   (NOT DEFINED CMAKE_RANLIB OR "${CMAKE_RANLIB}" STREQUAL "" OR
    "${CMAKE_RANLIB}" MATCHES "-NOTFOUND$") AND
   DEFINED CMAKE_CXX_COMPILER_RANLIB AND
   NOT "${CMAKE_CXX_COMPILER_RANLIB}" STREQUAL "" AND
   NOT "${CMAKE_CXX_COMPILER_RANLIB}" MATCHES "-NOTFOUND$")
    set(CMAKE_RANLIB "${CMAKE_CXX_COMPILER_RANLIB}" CACHE FILEPATH
        "Ranlib used by sindre and fixed dependencies" FORCE)
endif()

# Keep source archives/downloads shared, but never share an ExternalProject
# build/install tree between compiler, generator, CRT, or exception profiles.
# This fingerprint must be computed *after* the archiver selection above.
# CMake often initializes CMAKE_AR/CMAKE_*_COMPILER_AR during the first
# configure; hashing before that would change the profile directory on the
# second configure and leave install rules pointing at an unbuilt cache.
set(_sindre_external_profile
    "sindre-external-profile-v8|${CMAKE_CXX_COMPILER}|${CMAKE_CXX_COMPILER_ID}|${CMAKE_CXX_COMPILER_VERSION}|${CMAKE_GENERATOR}|${CMAKE_MSVC_RUNTIME_LIBRARY}|${SINDRE_NO_EXCEPTIONS}|${CMAKE_AR}|${CMAKE_C_COMPILER_AR}|${CMAKE_CXX_COMPILER_AR}")
string(MD5 _sindre_external_profile_hash "${_sindre_external_profile}")
set(SINDRE_THIRD_PARTY_BUILD_CACHE_DIR
    "${SINDRE_THIRD_PARTY_CACHE_DIR}/build/${_sindre_external_profile_hash}"
    CACHE PATH "Isolated build/install cache for this compiler profile" FORCE)

# Common ExternalProject arguments, following Open3D's isolated third-party
# build model. Every external dependency is built with the project compiler and
# Release ABI, then consumed through an imported target.
set(SINDRE_EXTERNAL_PROJECT_CMAKE_ARGS
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_BUILD_TYPE=Release
    # ExternalProject does not inherit the top-level configuration selected by
    # a multi-config generator.  Restrict its generated configurations so an
    # unqualified build/install step cannot silently fall back to Debug.
    -DCMAKE_CONFIGURATION_TYPES=Release
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DCMAKE_POLICY_DEFAULT_CMP0091=NEW)
foreach(_sindre_tool IN ITEMS CMAKE_AR CMAKE_LINKER CMAKE_MT CMAKE_RC_COMPILER)
    if(DEFINED ${_sindre_tool} AND NOT "${${_sindre_tool}}" STREQUAL "" AND
       NOT "${${_sindre_tool}}" MATCHES "-NOTFOUND$")
        list(APPEND SINDRE_EXTERNAL_PROJECT_CMAKE_ARGS
            "-D${_sindre_tool}=${${_sindre_tool}}")
    endif()
endforeach()
foreach(_sindre_pair IN ITEMS CMAKE_CXX_COMPILER_AR CMAKE_CXX_COMPILER_RANLIB)
    if(DEFINED ${_sindre_pair} AND NOT "${${_sindre_pair}}" STREQUAL "" AND
       NOT "${${_sindre_pair}}" MATCHES "-NOTFOUND$")
        if(_sindre_pair STREQUAL "CMAKE_CXX_COMPILER_AR" AND NOT MSVC)
            list(APPEND SINDRE_EXTERNAL_PROJECT_CMAKE_ARGS
                "-DCMAKE_AR=${${_sindre_pair}}")
        elseif(_sindre_pair STREQUAL "CMAKE_CXX_COMPILER_RANLIB" AND NOT MSVC)
            list(APPEND SINDRE_EXTERNAL_PROJECT_CMAKE_ARGS
                "-DCMAKE_RANLIB=${${_sindre_pair}}")
        endif()
    endif()
endforeach()
# ExternalProject is a separate CMake invocation.  Pass the same compiler
# flags as the parent project so clang-cl keeps the selected Windows SDK and
# runtime configuration in every fixed source dependency (not only Sindre's
# own targets).
foreach(_sindre_flag IN ITEMS CMAKE_C_FLAGS)
    if(DEFINED ${_sindre_flag} AND NOT "${${_sindre_flag}}" STREQUAL "")
        list(APPEND SINDRE_EXTERNAL_PROJECT_CMAKE_ARGS
            "-D${_sindre_flag}=${${_sindre_flag}}")
    endif()
endforeach()
# The target-level compiler defaults do not cross an ExternalProject boundary.
# Keep the exception model identical for fixed source dependencies, otherwise
# clang-cl children silently use exception-disabled defaults even in a normal
# Sindre build. Preserve explicitly supplied CXX flags and append only the
# mode required by the parent profile.
set(_sindre_external_cxx_flags "${CMAKE_CXX_FLAGS}")
if(MSVC)
    if(SINDRE_NO_EXCEPTIONS)
        string(APPEND _sindre_external_cxx_flags " /EHs-c-")
    else()
        string(APPEND _sindre_external_cxx_flags " /EHsc")
    endif()
endif()
# Fixed third-party libraries are built with their upstream exception model.
# SINDRE_NO_EXCEPTIONS applies to Sindre's own targets; forcing -fno-exceptions
# into every ExternalProject breaks dependencies such as CsString, whose
# upstream headers intentionally contain throw expressions.  General catches
# third-party exceptions at its public Result boundaries.  spdlog is the one
# exception: its dedicated no-exception define is safe and is kept below.
if(SINDRE_NO_EXCEPTIONS)
    string(APPEND _sindre_external_cxx_flags " -DSPDLOG_NO_EXCEPTIONS")
endif()
if(NOT _sindre_external_cxx_flags STREQUAL "")
    list(APPEND SINDRE_EXTERNAL_PROJECT_CMAKE_ARGS
        "-DCMAKE_CXX_FLAGS=${_sindre_external_cxx_flags}")
endif()
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
include("${SINDRE_3RDPARTY_DIR}/abseil/abseil.cmake")
include("${SINDRE_3RDPARTY_DIR}/re2/re2.cmake")
include("${SINDRE_3RDPARTY_DIR}/openssl/openssl.cmake")
include("${SINDRE_3RDPARTY_DIR}/zlib/zlib.cmake")
include("${SINDRE_3RDPARTY_DIR}/crashpad/crashpad.cmake")
include("${SINDRE_3RDPARTY_DIR}/eigen/eigen.cmake")
include("${SINDRE_3RDPARTY_DIR}/openblas/openblas.cmake")
include("${SINDRE_3RDPARTY_DIR}/onnxruntime/onnxruntime.cmake")
include("${SINDRE_3RDPARTY_DIR}/imgui/imgui.cmake")
include("${SINDRE_3RDPARTY_DIR}/glfw/glfw.cmake")
include("${SINDRE_3RDPARTY_DIR}/stb/stb.cmake")
include("${SINDRE_3RDPARTY_DIR}/opencv/opencv.cmake")
include("${SINDRE_3RDPARTY_DIR}/vtk/vtk.cmake")
include("${SINDRE_3RDPARTY_DIR}/cgal/cgal.cmake")
include("${SINDRE_3RDPARTY_DIR}/pcl/pcl.cmake")
include("${SINDRE_3RDPARTY_DIR}/pybind11/pybind11.cmake")
include("${SINDRE_3RDPARTY_DIR}/nanoflann/nanoflann.cmake")
