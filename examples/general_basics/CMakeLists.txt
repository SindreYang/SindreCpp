cmake_minimum_required(VERSION 3.20)
project(sindreExampleGeneralBasics LANGUAGES CXX)

# Allow this directory to be configured directly:
# cmake -S examples/general_basics -B build_general_basics -G Ninja
if(NOT TARGET sindre::general)
    set(SINDRE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SINDRE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." "${CMAKE_BINARY_DIR}/sindre")
endif()

add_executable(sindre_example_general_basics main.cpp)
target_link_libraries(sindre_example_general_basics PRIVATE sindre::general)
sindre_copy_target_runtime_dlls(sindre_example_general_basics)
if(WIN32 AND IS_DIRECTORY "${SINDRE_GENERAL_PACKAGE_ROOT}/bin")
    sindre_copy_runtime_dirs(sindre_example_general_basics
        "${SINDRE_GENERAL_PACKAGE_ROOT}/bin")
endif()

if(SINDRE_BUILD_TESTS)
    add_test(NAME sindre.example.general_basics
             COMMAND sindre_example_general_basics)
endif()
