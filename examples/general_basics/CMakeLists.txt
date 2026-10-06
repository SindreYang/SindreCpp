cmake_minimum_required(VERSION 3.20)
project(SindreCppExampleGeneralBasics LANGUAGES CXX)

# Allow this directory to be configured directly:
# cmake -S examples/general_basics -B build_general_basics
if(NOT TARGET SindreCpp::General)
    set(SINDRECPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SINDRECPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SINDRECPP_WITH_GENERAL ON CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." "${CMAKE_BINARY_DIR}/sindrecpp")
endif()

add_executable(sindrecpp_example_general_basics main.cpp)
target_link_libraries(sindrecpp_example_general_basics PRIVATE SindreCpp::General)
sindrecpp_copy_target_runtime_dlls(sindrecpp_example_general_basics)

if(SINDRECPP_BUILD_TESTS)
    add_test(NAME sindrecpp.example.general_basics
             COMMAND sindrecpp_example_general_basics)
endif()
