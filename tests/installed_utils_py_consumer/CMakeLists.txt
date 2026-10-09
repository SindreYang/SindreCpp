cmake_minimum_required(VERSION 3.20)

project(sindreInstalledUtilsPyConsumer LANGUAGES CXX)

find_package(sindre CONFIG REQUIRED COMPONENTS utils_py)
find_package(Python3 3.12 REQUIRED COMPONENTS Interpreter)
execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c "import sys; print(sys.base_prefix)"
    OUTPUT_VARIABLE _sindre_test_python_home
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
file(TO_CMAKE_PATH "${_sindre_test_python_home}" _sindre_test_python_home)

add_executable(sindre_installed_utils_py_consumer main.cpp)
target_compile_definitions(sindre_installed_utils_py_consumer PRIVATE
    SINDRE_TEST_PYTHON_EXECUTABLE="${Python3_EXECUTABLE}"
    SINDRE_TEST_PYTHON_HOME="${_sindre_test_python_home}")
target_link_libraries(sindre_installed_utils_py_consumer PRIVATE sindre::utils_py)

enable_testing()
add_test(NAME sindre.installed.utils_py
    COMMAND sindre_installed_utils_py_consumer)
if(WIN32)
    get_filename_component(_sindre_install_prefix
        "${sindre_DIR}/../../.." ABSOLUTE)
    set_tests_properties(sindre.installed.utils_py PROPERTIES
        ENVIRONMENT "PATH=${_sindre_install_prefix}/bin;$ENV{PATH}")
endif()
