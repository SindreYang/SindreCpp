cmake_minimum_required(VERSION 3.20)

project(sindreInstalledGuiConsumer LANGUAGES CXX)

find_package(sindre CONFIG REQUIRED COMPONENTS gui)

add_executable(sindre_installed_gui_consumer main.cpp)
target_link_libraries(sindre_installed_gui_consumer PRIVATE sindre::gui)

enable_testing()
add_test(NAME sindre.installed.gui COMMAND sindre_installed_gui_consumer)
get_filename_component(_sindre_install_root
    "${sindre_DIR}/../../.." REALPATH)
set_tests_properties(sindre.installed.gui PROPERTIES
    ENVIRONMENT "PATH=${_sindre_install_root}/bin;$ENV{PATH}")
