cmake_minimum_required(VERSION 3.20)

project(SindreCpp VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(sindrecpp_build_extras_default ON)
else()
    set(sindrecpp_build_extras_default OFF)
endif()
option(SINDRECPP_BUILD_TESTS "Build SindreCpp tests" ${sindrecpp_build_extras_default})
option(SINDRECPP_BUILD_EXAMPLES "Build SindreCpp examples" ${sindrecpp_build_extras_default})
option(SINDRECPP_WITH_GENERAL "Enable general-purpose utilities" ON)
option(SINDRECPP_WITH_POINTER "Enable the CsPointer integration" ON)
option(SINDRECPP_WITH_STRING "Enable the CsString integration" ON)
option(SINDRECPP_WITH_LOG "Enable the spdlog integration" ON)
option(SINDRECPP_WITH_GUI "Enable the Dear ImGui integration" ON)
option(SINDRECPP_WITH_PYTHON "Enable the pybind11 integration" ON)
option(SINDRECPP_WITH_HTTP "Enable the cpp-httplib integration" ON)
option(SINDRECPP_HTTP_OPENSSL "Enable HTTPS support in cpp-httplib when OpenSSL is available" ON)
option(SINDRECPP_WITH_JSON "Enable the simdjson integration" ON)
option(SINDRECPP_WITH_CLI "Enable the argparse integration" ON)
option(SINDRECPP_WITH_UTILS3D "Enable the utils3d module backed by Eigen" ON)
option(SINDRECPP_UTILS3D_NATIVE_ARCH "Optimize utils3d for the build machine CPU (reduces binary portability)" ON)
set(SINDRECPP_UTILS3D_BLAS_BACKEND "AUTO" CACHE STRING "utils3d BLAS backend: AUTO, EIGEN, or BLAS")
set_property(CACHE SINDRECPP_UTILS3D_BLAS_BACKEND PROPERTY STRINGS AUTO EIGEN BLAS)
option(SINDRECPP_BUILD_UTILS3D_BENCHMARKS "Build the optional utils3d vs BLAS benchmark" OFF)

if(NOT SINDRECPP_WITH_GENERAL)
    foreach(module IN ITEMS POINTER STRING LOG HTTP JSON CLI)
        set(SINDRECPP_WITH_${module} OFF CACHE BOOL "" FORCE)
    endforeach()
endif()

# CsPointer and CsString have broken top-level CMake configure_file paths; only their sources are populated.
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

add_library(SindreCpp::Core INTERFACE IMPORTED GLOBAL)
set_target_properties(SindreCpp::Core PROPERTIES
    INTERFACE_COMPILE_FEATURES cxx_std_17
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include
)

function(sindrecpp_add_module module)
    add_library(SindreCpp::${module} INTERFACE IMPORTED GLOBAL)
    string(TOUPPER "${module}" module_upper)
    set_target_properties(SindreCpp::${module} PROPERTIES
        INTERFACE_LINK_LIBRARIES "SindreCpp::Core;${ARGN}"
        INTERFACE_COMPILE_DEFINITIONS SINDRECPP_WITH_${module_upper}=1
    )
endfunction()

# Apply upstream defaults only within the dependency's subdirectory scope.
# Existing parent-project variables and cache options always take precedence.
function(sindrecpp_make_available dependency)
    foreach(default IN LISTS ARGN)
        string(FIND "${default}" "=" separator)
        if(separator LESS 1)
            message(FATAL_ERROR "Invalid dependency default '${default}'; expected NAME=VALUE")
        endif()
        string(SUBSTRING "${default}" 0 ${separator} variable)
        math(EXPR value_start "${separator} + 1")
        string(SUBSTRING "${default}" ${value_start} -1 value)
        if(NOT DEFINED ${variable})
            set(${variable} "${value}")
        endif()
    endforeach()
    FetchContent_MakeAvailable(${dependency})
endfunction()

if(SINDRECPP_WITH_POINTER)
    FetchContent_Declare(cppointer
        GIT_REPOSITORY https://github.com/copperspice/cs_pointer.git
        GIT_TAG pointer-1.0.1
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(cppointer)
    if(NOT cppointer_POPULATED)
        FetchContent_Populate(cppointer)
    endif()
    add_library(sindrecpp_cs_pointer INTERFACE)
    target_compile_features(sindrecpp_cs_pointer INTERFACE cxx_std_17)
    target_include_directories(sindrecpp_cs_pointer SYSTEM INTERFACE ${cppointer_SOURCE_DIR}/src)
    sindrecpp_add_module(Pointer sindrecpp_cs_pointer)
endif()

if(SINDRECPP_WITH_STRING)
    FetchContent_Declare(csstring
        GIT_REPOSITORY https://github.com/copperspice/cs_string.git
        GIT_TAG string-1.4.1
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(csstring)
    if(NOT csstring_POPULATED)
        FetchContent_Populate(csstring)
    endif()
    add_library(sindrecpp_cs_string STATIC ${csstring_SOURCE_DIR}/src/cs_string.cpp)
    target_compile_features(sindrecpp_cs_string PUBLIC cxx_std_17)
    target_include_directories(sindrecpp_cs_string SYSTEM PUBLIC ${csstring_SOURCE_DIR}/src)
    sindrecpp_add_module(String sindrecpp_cs_string)
endif()

if(SINDRECPP_WITH_LOG)
    if(NOT TARGET spdlog::spdlog)
        find_package(spdlog CONFIG QUIET)
    endif()
    if(NOT TARGET spdlog::spdlog)
        FetchContent_Declare(spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG v1.17.0
            GIT_SHALLOW TRUE
        )
        sindrecpp_make_available(spdlog
            SPDLOG_BUILD_TESTS=OFF
            SPDLOG_BUILD_EXAMPLE=OFF
        )
    endif()
    sindrecpp_add_module(Log spdlog::spdlog)
endif()

if(SINDRECPP_WITH_GUI)
    if(NOT TARGET imgui::imgui AND NOT TARGET imgui)
        find_package(imgui CONFIG QUIET)
    endif()
    if(TARGET imgui::imgui)
        sindrecpp_add_module(Gui imgui::imgui)
    elseif(TARGET imgui)
        sindrecpp_add_module(Gui imgui)
    else()
        FetchContent_Declare(imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG v1.92.9b
            GIT_SHALLOW TRUE
        )
        FetchContent_GetProperties(imgui)
        if(NOT imgui_POPULATED)
            FetchContent_Populate(imgui)
        endif()
        add_library(sindrecpp_imgui STATIC
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        )
        target_include_directories(sindrecpp_imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR})
        target_compile_features(sindrecpp_imgui PUBLIC cxx_std_17)
        sindrecpp_add_module(Gui sindrecpp_imgui)
    endif()
endif()

if(SINDRECPP_WITH_PYTHON)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
    if(NOT TARGET pybind11::embed)
        find_package(pybind11 CONFIG QUIET)
    endif()
    if(NOT TARGET pybind11::embed)
        FetchContent_Declare(pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG v3.1.0
            GIT_SHALLOW TRUE
        )
        sindrecpp_make_available(pybind11 PYBIND11_TEST=OFF)
    endif()
    sindrecpp_add_module(Python pybind11::embed)
endif()

if(SINDRECPP_WITH_HTTP)
    if(NOT TARGET httplib::httplib)
        find_package(httplib CONFIG QUIET)
    endif()
    if(NOT TARGET httplib::httplib)
        FetchContent_Declare(cpp_httplib
            GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
            GIT_TAG v0.56.0
            GIT_SHALLOW TRUE
        )
        sindrecpp_make_available(cpp_httplib
            HTTPLIB_TEST=OFF
            HTTPLIB_USE_OPENSSL_IF_AVAILABLE=${SINDRECPP_HTTP_OPENSSL}
        )
    endif()
    sindrecpp_add_module(Http httplib::httplib)
endif()

if(SINDRECPP_WITH_JSON)
    if(NOT TARGET simdjson::simdjson)
        find_package(simdjson CONFIG QUIET)
    endif()
    if(NOT TARGET simdjson::simdjson)
        FetchContent_Declare(simdjson
            GIT_REPOSITORY https://github.com/simdjson/simdjson.git
            GIT_TAG v4.6.11
            GIT_SHALLOW TRUE
        )
        sindrecpp_make_available(simdjson SIMDJSON_DEVELOPER_MODE=OFF)
    endif()
    sindrecpp_add_module(Json simdjson::simdjson)
endif()

if(SINDRECPP_WITH_CLI)
    if(NOT TARGET argparse::argparse)
        find_package(argparse CONFIG QUIET)
    endif()
    if(NOT TARGET argparse::argparse)
        FetchContent_Declare(argparse
            GIT_REPOSITORY https://github.com/p-ranav/argparse.git
            GIT_TAG v3.2
            GIT_SHALLOW TRUE
        )
        sindrecpp_make_available(argparse ARGPARSE_BUILD_TESTS=OFF)
    endif()
    sindrecpp_add_module(Cli argparse::argparse)
endif()

if(SINDRECPP_WITH_UTILS3D)
    if(NOT TARGET Eigen3::Eigen)
        find_package(Eigen3 CONFIG QUIET NO_MODULE)
    endif()
    if(NOT TARGET Eigen3::Eigen)
        FetchContent_Declare(eigen
            GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
            GIT_TAG 5.0.1
            GIT_SHALLOW TRUE
        )
        sindrecpp_make_available(eigen
            EIGEN_BUILD_DOC=OFF
            EIGEN_BUILD_PKGCONFIG=OFF
        )
    endif()

    string(TOUPPER "${SINDRECPP_UTILS3D_BLAS_BACKEND}" sindrecpp_utils3d_blas_backend)
    if(NOT sindrecpp_utils3d_blas_backend MATCHES "^(AUTO|EIGEN|BLAS)$")
        message(FATAL_ERROR "SINDRECPP_UTILS3D_BLAS_BACKEND must be AUTO, EIGEN, or BLAS")
    endif()
    if(NOT TARGET BLAS::BLAS)
        if(sindrecpp_utils3d_blas_backend STREQUAL "BLAS")
            find_package(BLAS REQUIRED)
        else()
            find_package(BLAS QUIET)
        endif()
    endif()

    set(sindrecpp_utils3d_use_blas OFF)
    if(sindrecpp_utils3d_blas_backend STREQUAL "BLAS")
        if(NOT TARGET BLAS::BLAS)
            message(FATAL_ERROR "BLAS backend requested, but no BLAS::BLAS target is available")
        endif()
        set(sindrecpp_utils3d_use_blas ON)
    elseif(sindrecpp_utils3d_blas_backend STREQUAL "AUTO" AND TARGET BLAS::BLAS)
        set(sindrecpp_utils3d_use_blas ON)
    endif()

    set(sindrecpp_utils3d_links Eigen3::Eigen)
    if(sindrecpp_utils3d_use_blas)
        list(APPEND sindrecpp_utils3d_links BLAS::BLAS)
        message(STATUS "SindreCpp Eigen backend: external BLAS (BLA_VENDOR='${BLA_VENDOR}')")
    else()
        message(STATUS "SindreCpp Eigen backend: Eigen built-in kernels")
    endif()
    if(SINDRECPP_UTILS3D_NATIVE_ARCH)
        add_library(sindrecpp_utils3d_native_arch INTERFACE)
        if(MSVC AND (CMAKE_GENERATOR_PLATFORM MATCHES "^(x64|Win32)$" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|x64|i.86)$" OR CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "^(x64|X86|Win32)$" OR MSVC_CXX_ARCHITECTURE_ID MATCHES "^(x64|X86|Win32)$"))
            target_compile_options(sindrecpp_utils3d_native_arch INTERFACE /arch:AVX2)
        elseif(MSVC)
            message(STATUS "MSVC target architecture has no SindreCpp native Eigen flag; using compiler defaults")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
            target_compile_options(sindrecpp_utils3d_native_arch INTERFACE -march=native)
        else()
            message(FATAL_ERROR "SINDRECPP_UTILS3D_NATIVE_ARCH is unsupported by this compiler")
        endif()
        list(APPEND sindrecpp_utils3d_links sindrecpp_utils3d_native_arch)
        sindrecpp_add_module(Utils3d ${sindrecpp_utils3d_links})
    else()
        sindrecpp_add_module(Utils3d ${sindrecpp_utils3d_links})
    endif()
    if(sindrecpp_utils3d_use_blas)
        set_property(TARGET SindreCpp::Utils3d APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS EIGEN_USE_BLAS=1)
    endif()
endif()

if(SINDRECPP_WITH_GENERAL)
    add_library(SindreCpp::General INTERFACE IMPORTED GLOBAL)
    set(sindrecpp_general_targets SindreCpp::Core)
    foreach(module IN ITEMS Pointer String Log Http Json Cli)
        string(TOUPPER "${module}" module_upper)
        if(SINDRECPP_WITH_${module_upper})
            list(APPEND sindrecpp_general_targets SindreCpp::${module})
        endif()
    endforeach()
    set_target_properties(SindreCpp::General PROPERTIES
        INTERFACE_LINK_LIBRARIES "${sindrecpp_general_targets}"
        INTERFACE_COMPILE_DEFINITIONS SINDRECPP_WITH_GENERAL=1
    )
endif()

# One target carries every enabled module. Consumers can still link an individual
# module target when they want to keep their dependency surface small.
add_library(SindreCpp::SindreCpp INTERFACE IMPORTED GLOBAL)
set(sindrecpp_all_targets SindreCpp::Core)
if(SINDRECPP_WITH_GENERAL)
    list(APPEND sindrecpp_all_targets SindreCpp::General)
endif()
if(SINDRECPP_WITH_UTILS3D)
    list(APPEND sindrecpp_all_targets SindreCpp::Utils3d)
endif()
if(SINDRECPP_WITH_GUI)
    list(APPEND sindrecpp_all_targets SindreCpp::Gui)
endif()
if(SINDRECPP_WITH_PYTHON)
    list(APPEND sindrecpp_all_targets SindreCpp::Python)
endif()
set_target_properties(SindreCpp::SindreCpp PROPERTIES
    INTERFACE_LINK_LIBRARIES "${sindrecpp_all_targets}"
)

if(SINDRECPP_BUILD_TESTS)
    enable_testing()
    add_executable(sindrecpp_core_tests tests/core_tests.cpp)
    target_link_libraries(sindrecpp_core_tests PRIVATE SindreCpp::Core)
    add_test(NAME sindrecpp.core COMMAND sindrecpp_core_tests)
    if(SINDRECPP_WITH_UTILS3D)
        add_executable(sindrecpp_utils3d_tests tests/utils3d_tests.cpp)
        target_link_libraries(sindrecpp_utils3d_tests PRIVATE SindreCpp::Utils3d)
        add_test(NAME sindrecpp.utils3d COMMAND sindrecpp_utils3d_tests)
    endif()
endif()

if(SINDRECPP_BUILD_UTILS3D_BENCHMARKS)
    if(NOT SINDRECPP_WITH_UTILS3D)
        message(FATAL_ERROR "SINDRECPP_BUILD_UTILS3D_BENCHMARKS requires SINDRECPP_WITH_UTILS3D=ON")
    endif()
    if(NOT TARGET BLAS::BLAS)
        find_package(BLAS QUIET)
    endif()
    if(TARGET BLAS::BLAS)
        add_executable(sindrecpp_utils3d_benchmark tests/utils3d_benchmark.cpp)
        target_link_libraries(sindrecpp_utils3d_benchmark PRIVATE Eigen3::Eigen BLAS::BLAS)
        if(BLA_VENDOR)
            target_compile_definitions(sindrecpp_utils3d_benchmark PRIVATE SINDRECPP_BENCHMARK_BLAS_VENDOR="${BLA_VENDOR}")
        else()
            target_compile_definitions(sindrecpp_utils3d_benchmark PRIVATE SINDRECPP_BENCHMARK_BLAS_VENDOR="detected BLAS")
        endif()
        if(SINDRECPP_UTILS3D_NATIVE_ARCH)
            target_link_libraries(sindrecpp_utils3d_benchmark PRIVATE sindrecpp_utils3d_native_arch)
        endif()
    else()
        message(WARNING "SINDRECPP_BUILD_UTILS3D_BENCHMARKS requested, but BLAS was not found; benchmark target is skipped")
    endif()
endif()

if(SINDRECPP_BUILD_EXAMPLES)
    add_executable(sindrecpp_basic examples/basic_usage.cpp)
    target_link_libraries(sindrecpp_basic PRIVATE SindreCpp::SindreCpp)
    if(SINDRECPP_BUILD_TESTS)
        if(WIN32 AND SINDRECPP_WITH_PYTHON)
            add_test(NAME sindrecpp.modules COMMAND ${CMAKE_COMMAND} -E env
                "PATH=${Python3_RUNTIME_LIBRARY_DIRS};$ENV{PATH}" $<TARGET_FILE:sindrecpp_basic>)
        else()
            add_test(NAME sindrecpp.modules COMMAND sindrecpp_basic)
        endif()
    endif()
endif()
