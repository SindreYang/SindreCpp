cmake_minimum_required(VERSION 3.20)
project(sindreExampleUtils_3dMesh LANGUAGES CXX)

# VTK/Eigen are discovered by the Utils_3d module. This block also makes the
# example usable as a standalone CMake source directory.
if(NOT TARGET sindre::utils_3d)
    set(SINDRE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SINDRE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SINDRE_WITH_UTILS_3D ON CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." "${CMAKE_BINARY_DIR}/sindre")
endif()

add_executable(sindre_example_utils_3d_mesh main.cpp)
target_link_libraries(sindre_example_utils_3d_mesh PRIVATE sindre::utils_3d)
sindre_copy_target_runtime_dlls(sindre_example_utils_3d_mesh)
get_target_property(_sindre_utils_3d_runtime_dirs sindre_utils_3d SINDRE_RUNTIME_DIRS)
if(_sindre_utils_3d_runtime_dirs AND
   NOT _sindre_utils_3d_runtime_dirs STREQUAL "_sindre_utils_3d_runtime_dirs-NOTFOUND")
    sindre_copy_runtime_dirs(sindre_example_utils_3d_mesh
                                 ${_sindre_utils_3d_runtime_dirs})
endif()

if(SINDRE_BUILD_TESTS)
    add_test(NAME sindre.example.utils_3d_mesh
             COMMAND sindre_example_utils_3d_mesh)
endif()
