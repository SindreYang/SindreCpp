cmake_minimum_required(VERSION 3.20)

project(SindreCpp VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

option(SINDRECPP_BUILD_TESTS "Build SindreCpp tests" ON)
option(SINDRECPP_BUILD_EXAMPLES "Build SindreCpp examples" ON)
option(SINDRECPP_WITH_POINTER "Enable the CsPointer integration" OFF)
option(SINDRECPP_WITH_STRING "Enable the CsString integration" OFF)
option(SINDRECPP_WITH_LOG "Enable the spdlog integration" OFF)
option(SINDRECPP_WITH_GUI "Enable the Dear ImGui integration" OFF)
option(SINDRECPP_WITH_PYTHON "Enable the pybind11 integration" OFF)
option(SINDRECPP_WITH_HTTP "Enable the cpp-httplib integration" OFF)
option(SINDRECPP_WITH_JSON "Enable the simdjson integration" OFF)
option(SINDRECPP_WITH_CLI "Enable the argparse integration" OFF)
option(SINDRECPP_WITH_EIGEN "Enable the Eigen integration" OFF)

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
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.17.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(spdlog)
    sindrecpp_add_module(Log spdlog::spdlog)
endif()

if(SINDRECPP_WITH_GUI)
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

if(SINDRECPP_WITH_PYTHON)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
    set(PYBIND11_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v3.1.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(pybind11)
    sindrecpp_add_module(Python pybind11::embed)
endif()

if(SINDRECPP_WITH_HTTP)
    set(HTTPLIB_TEST OFF CACHE BOOL "" FORCE)
    set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(cpp_httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.56.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(cpp_httplib)
    sindrecpp_add_module(Http httplib::httplib)
endif()

if(SINDRECPP_WITH_JSON)
    set(SIMDJSON_DEVELOPER_MODE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(simdjson
        GIT_REPOSITORY https://github.com/simdjson/simdjson.git
        GIT_TAG v4.6.11
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(simdjson)
    sindrecpp_add_module(Json simdjson::simdjson)
endif()

if(SINDRECPP_WITH_CLI)
    set(ARGPARSE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(argparse
        GIT_REPOSITORY https://github.com/p-ranav/argparse.git
        GIT_TAG v3.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(argparse)
    sindrecpp_add_module(Cli argparse::argparse)
endif()

if(SINDRECPP_WITH_EIGEN)
    set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
    set(EIGEN_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(eigen
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG 5.0.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(eigen)
    sindrecpp_add_module(Eigen Eigen3::Eigen)
endif()

if(SINDRECPP_BUILD_TESTS)
    enable_testing()
    add_executable(sindrecpp_core_tests tests/core_tests.cpp)
    target_link_libraries(sindrecpp_core_tests PRIVATE SindreCpp::Core)
    add_test(NAME sindrecpp.core COMMAND sindrecpp_core_tests)
endif()

if(SINDRECPP_BUILD_EXAMPLES)
    add_executable(sindrecpp_basic examples/basic_usage.cpp)
    target_link_libraries(sindrecpp_basic PRIVATE SindreCpp::Core)
    foreach(module IN ITEMS Pointer String Log Gui Python Http Json Cli Eigen)
        string(TOUPPER "${module}" module_upper)
        if(SINDRECPP_WITH_${module_upper})
            target_link_libraries(sindrecpp_basic PRIVATE SindreCpp::${module})
            target_compile_definitions(sindrecpp_basic PRIVATE SINDRECPP_WITH_${module_upper}=1)
        endif()
    endforeach()
    if(SINDRECPP_BUILD_TESTS)
        if(WIN32 AND SINDRECPP_WITH_PYTHON)
            add_test(NAME sindrecpp.modules COMMAND ${CMAKE_COMMAND} -E env
                "PATH=${Python3_RUNTIME_LIBRARY_DIRS};$ENV{PATH}" $<TARGET_FILE:sindrecpp_basic>)
        else()
            add_test(NAME sindrecpp.modules COMMAND sindrecpp_basic)
        endif()
    endif()
endif()
