cmake_minimum_required(VERSION 3.20)

# 项目根配置：统一 C++ 标准、依赖注册、模块开关、安装导出和测试入口。
# 具体模块的编译逻辑放在 modules/<module>/CMakeLists.txt，避免根文件过度膨胀。

project(sindrecpp VERSION 0.1.0 LANGUAGES C CXX)

# The public library contract is C++17.  Keep this fixed at the project
# boundary so an installed build and all bundled dependencies use the same
# language level instead of inheriting a newer host default.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SINDRE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(SINDRE_3RDPARTY_DIR "${SINDRE_SOURCE_DIR}/3rdparty" CACHE PATH
    "sindre third-party dependency registry")
list(APPEND CMAKE_MODULE_PATH "${SINDRE_SOURCE_DIR}/cmake")

include(CTest)
include("${SINDRE_3RDPARTY_DIR}/find_dependencies.cmake")
include(Options)
include(OutputLayout)
include(Common)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_subdirectory(modules/general)
add_subdirectory(modules/math)
if(SINDRE_WITH_AI)
    add_subdirectory(modules/ai)
endif()
if(SINDRE_WITH_GUI)
    add_subdirectory(modules/gui)
endif()
if(SINDRE_WITH_UTILS_2D)
    add_subdirectory(modules/utils_2d)
endif()
if(SINDRE_WITH_UTILS_3D)
    add_subdirectory(modules/utils_3d)
endif()
if(SINDRE_WITH_UTILS_PY)
    add_subdirectory(modules/utils_py)
endif()

if(SINDRE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(SINDRE_BUILD_TESTS)
    enable_testing()
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
set_target_properties(sindre_base PROPERTIES EXPORT_NAME base)
install(TARGETS sindre_base EXPORT sindreTargets)
foreach(module IN ITEMS general math ai gui utils_2d utils_3d utils_py)
    if(TARGET sindre::${module})
        install(TARGETS sindre_${module} EXPORT sindreTargets)
    endif()
endforeach()
install(EXPORT sindreTargets
    FILE sindreTargets.cmake
    NAMESPACE sindre::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sindre)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sindreConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/sindreConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/sindre")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/sindreConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/sindreConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/sindreConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sindre)
