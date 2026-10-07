cmake_minimum_required(VERSION 3.20)
project(sindreInstalledMathConsumer LANGUAGES CXX)

find_package(sindrecpp CONFIG REQUIRED)

add_executable(sindre_installed_math_consumer main.cpp)
target_link_libraries(sindre_installed_math_consumer PRIVATE sindre::math)

enable_testing()
add_test(NAME sindre.installed.math
    COMMAND sindre_installed_math_consumer)
