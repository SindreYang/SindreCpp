cmake_minimum_required(VERSION 3.20)
project(SindreCppExampleUtils3dMesh LANGUAGES CXX)

# VTK/Eigen are discovered by the Utils3d module. This block also makes the
# example usable as a standalone CMake source directory.
if(NOT TARGET SindreCpp::Utils3d)
    set(SINDRECPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SINDRECPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SINDRECPP_WITH_GENERAL ON CACHE BOOL "" FORCE)
    set(SINDRECPP_WITH_UTILS3D ON CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." "${CMAKE_BINARY_DIR}/sindrecpp")
endif()

add_executable(sindrecpp_example_utils3d_mesh main.cpp)
target_link_libraries(sindrecpp_example_utils3d_mesh PRIVATE SindreCpp::Utils3d)
sindrecpp_copy_target_runtime_dlls(sindrecpp_example_utils3d_mesh)
get_target_property(_sindrecpp_utils3d_runtime_dirs SindreCpp_Utils3d SINDRECPP_RUNTIME_DIRS)
if(_sindrecpp_utils3d_runtime_dirs AND
   NOT _sindrecpp_utils3d_runtime_dirs STREQUAL "_sindrecpp_utils3d_runtime_dirs-NOTFOUND")
    sindrecpp_copy_runtime_dirs(sindrecpp_example_utils3d_mesh
                                 ${_sindrecpp_utils3d_runtime_dirs})
endif()

if(SINDRECPP_BUILD_TESTS)
    add_test(NAME sindrecpp.example.utils3d_mesh
             COMMAND sindrecpp_example_utils3d_mesh)
endif()
