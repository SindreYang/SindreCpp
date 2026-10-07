cmake_minimum_required(VERSION 3.20)
project(sindreExampleGeneralJsonConfig LANGUAGES CXX)

# This example needs simdjson. Enabling JSON here makes the directory directly
# configurable without requiring users to edit the root project first.
if(NOT TARGET sindre::general)
    set(SINDRE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SINDRE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SINDRE_WITH_GENERAL ON CACHE BOOL "" FORCE)
    set(SINDRE_WITH_JSON ON CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." "${CMAKE_BINARY_DIR}/sindre")
elseif(NOT SINDRE_WITH_JSON)
    message(FATAL_ERROR "general_json_config requires SINDRE_WITH_JSON=ON")
endif()

add_executable(sindre_example_general_json_config main.cpp)
target_link_libraries(sindre_example_general_json_config PRIVATE sindre::general)
sindre_copy_target_runtime_dlls(sindre_example_general_json_config)
if(WIN32 AND IS_DIRECTORY "${SINDRE_GENERAL_PACKAGE_ROOT}/bin")
    sindre_copy_runtime_dirs(sindre_example_general_json_config
        "${SINDRE_GENERAL_PACKAGE_ROOT}/bin")
endif()

if(SINDRE_BUILD_TESTS)
    add_test(NAME sindre.example.general_json_config
             COMMAND sindre_example_general_json_config)
endif()
