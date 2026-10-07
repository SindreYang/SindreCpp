cmake_minimum_required(VERSION 3.20)

project(sindrecpp VERSION 0.1.0 LANGUAGES C CXX)

# The public library contract is C++17.  Keep this fixed at the project
# boundary so an installed build and all bundled dependencies use the same
# language level instead of inheriting a newer host default.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SINDRE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(SINDRE_THIRDS_DIR "${SINDRE_SOURCE_DIR}/thirds" CACHE PATH
    "sindre third-party dependency registry")
list(APPEND CMAKE_MODULE_PATH "${SINDRE_SOURCE_DIR}/cmake")

include(CTest)
include(Options)
include(OutputLayout)
include("${SINDRE_THIRDS_DIR}/Dependencies.cmake")
include(Common)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_subdirectory(modules/general)
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
install(TARGETS sindre_base EXPORT sindrecppTargets)
foreach(module IN ITEMS general ai gui utils_2d utils_3d utils_py)
    if(TARGET sindre::${module})
        install(TARGETS sindre_${module} EXPORT sindrecppTargets)
    endif()
endforeach()
set(SINDRE_EIGEN_BUNDLED OFF)
if(TARGET eigen AND DEFINED eigen_SOURCE_DIR)
    set(SINDRE_EIGEN_BUNDLED ON)
    install(TARGETS eigen EXPORT sindrecppTargets)
endif()
install(EXPORT sindrecppTargets
    FILE sindrecppTargets.cmake
    NAMESPACE sindre::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sindrecpp)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sindrecppConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/sindrecppConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/sindrecpp")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/sindrecppConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/sindrecppConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/sindrecppConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sindrecpp)
