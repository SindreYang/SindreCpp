cmake_minimum_required(VERSION 3.20)
project(SindreCppExampleGeneralJsonConfig LANGUAGES CXX)

# This example needs simdjson. Enabling JSON here makes the directory directly
# configurable without requiring users to edit the root project first.
if(NOT TARGET SindreCpp::General)
    set(SINDRECPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SINDRECPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SINDRECPP_WITH_GENERAL ON CACHE BOOL "" FORCE)
    set(SINDRECPP_WITH_JSON ON CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." "${CMAKE_BINARY_DIR}/sindrecpp")
elseif(NOT SINDRECPP_WITH_JSON)
    message(FATAL_ERROR "general_json_config requires SINDRECPP_WITH_JSON=ON")
endif()

add_executable(sindrecpp_example_general_json_config main.cpp)
target_link_libraries(sindrecpp_example_general_json_config PRIVATE SindreCpp::General)
sindrecpp_copy_target_runtime_dlls(sindrecpp_example_general_json_config)

if(SINDRECPP_BUILD_TESTS)
    add_test(NAME sindrecpp.example.general_json_config
             COMMAND sindrecpp_example_general_json_config)
endif()
