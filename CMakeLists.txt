cmake_minimum_required(VERSION 3.20)

project(SindreCpp VERSION 0.1.0 LANGUAGES C CXX)

set(SINDRECPP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(SINDRECPP_THIRDS_DIR "${SINDRECPP_SOURCE_DIR}/thirds" CACHE PATH
    "SindreCpp third-party dependency registry")
list(APPEND CMAKE_MODULE_PATH "${SINDRECPP_SOURCE_DIR}/cmake")

include(CTest)
include(Options)
include(OutputLayout)
include(Common)
include("${SINDRECPP_THIRDS_DIR}/Dependencies.cmake")
include(GNUInstallDirs)

if(SINDRECPP_WITH_GENERAL)
    add_subdirectory(modules/general)
endif()
if(SINDRECPP_WITH_AI)
    add_subdirectory(modules/ai)
endif()
if(SINDRECPP_WITH_GUI)
    add_subdirectory(modules/gui)
endif()
if(SINDRECPP_WITH_UTILS2D)
    add_subdirectory(modules/utils2d)
endif()
if(SINDRECPP_WITH_UTILS3D)
    add_subdirectory(modules/utils3d)
endif()

add_library(SindreCpp_SindreCpp INTERFACE)
add_library(SindreCpp::SindreCpp ALIAS SindreCpp_SindreCpp)
target_compile_features(SindreCpp_SindreCpp INTERFACE cxx_std_17)
target_include_directories(SindreCpp_SindreCpp INTERFACE
    $<BUILD_INTERFACE:${SINDRECPP_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

foreach(module IN ITEMS General Ai Gui Utils2d Utils3d)
    if(TARGET SindreCpp::${module})
        target_link_libraries(SindreCpp_SindreCpp INTERFACE SindreCpp::${module})
        get_target_property(_module_runtime_dirs SindreCpp_${module} SINDRECPP_RUNTIME_DIRS)
        if(_module_runtime_dirs AND NOT _module_runtime_dirs STREQUAL "_module_runtime_dirs-NOTFOUND")
            list(APPEND _sindrecpp_runtime_dirs ${_module_runtime_dirs})
        endif()
    endif()
endforeach()
if(_sindrecpp_runtime_dirs)
    list(REMOVE_DUPLICATES _sindrecpp_runtime_dirs)
    set_property(TARGET SindreCpp_SindreCpp PROPERTY
        SINDRECPP_RUNTIME_DIRS "${_sindrecpp_runtime_dirs}")
endif()

if(SINDRECPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(SINDRECPP_BUILD_TESTS)
    enable_testing()
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
set_target_properties(sindrecpp_base PROPERTIES EXPORT_NAME base)
install(TARGETS sindrecpp_base EXPORT SindreCppTargets)
set_target_properties(SindreCpp_SindreCpp PROPERTIES EXPORT_NAME SindreCpp)
foreach(module IN ITEMS General Ai Gui Utils2d Utils3d)
    if(TARGET SindreCpp::${module})
        install(TARGETS SindreCpp_${module} EXPORT SindreCppTargets)
    endif()
endforeach()
install(TARGETS SindreCpp_SindreCpp EXPORT SindreCppTargets)
install(EXPORT SindreCppTargets
    FILE SindreCppTargets.cmake
    NAMESPACE SindreCpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SindreCpp)
