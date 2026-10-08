cmake_minimum_required(VERSION 3.20)

# Utils_2d 示例：仅在启用 OpenCV 后端时构建，避免默认配置强制拉取图像依赖。
project(sindreExampleUtils2dImage LANGUAGES CXX)

if(NOT TARGET sindre::utils_2d)
    set(SINDRE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SINDRE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SINDRE_WITH_UTILS_2D ON CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." "${CMAKE_BINARY_DIR}/sindre")
endif()

add_executable(sindre_example_utils_2d_image main.cpp)
target_link_libraries(sindre_example_utils_2d_image PRIVATE sindre::utils_2d)
sindre_copy_target_runtime_dlls(sindre_example_utils_2d_image)
