cmake_minimum_required(VERSION 3.20)

project(sindreInstalledUtils2dConsumer LANGUAGES CXX)

find_package(sindre CONFIG REQUIRED COMPONENTS utils_2d)

add_executable(sindre_installed_utils2d_consumer main.cpp)
target_link_libraries(sindre_installed_utils2d_consumer PRIVATE sindre::utils_2d)

enable_testing()
add_test(NAME sindre.installed.utils_2d COMMAND sindre_installed_utils2d_consumer)

get_filename_component(_sindre_install_root
    "${sindre_DIR}/../../.." REALPATH)
get_filename_component(_sindre_opencv_root
    "${OpenCV_DIR}/.." REALPATH)
set_tests_properties(sindre.installed.utils_2d PROPERTIES
    ENVIRONMENT
        "PATH=${_sindre_install_root}/bin;${_sindre_opencv_root}/bin;$ENV{PATH}")
