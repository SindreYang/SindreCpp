cmake_minimum_required(VERSION 3.20)
project(sindreInstalledUtils3dConsumer LANGUAGES CXX)

find_package(sindrecpp CONFIG REQUIRED)

add_executable(sindre_installed_utils3d_consumer main.cpp)
target_link_libraries(sindre_installed_utils3d_consumer PRIVATE sindre::utils_3d)

enable_testing()
add_test(NAME sindre.installed.utils_3d
    COMMAND sindre_installed_utils3d_consumer)
