cmake_minimum_required(VERSION 3.20)
project(SindreCppFetchContentConsumer LANGUAGES CXX)

include(FetchContent)
foreach(module IN ITEMS POINTER STRING LOG UTILS_PY HTTP JSON CLI UTILS3D)
    set(SINDRECPP_WITH_${module} OFF CACHE BOOL "" FORCE)
endforeach()
set(SINDRECPP_WITH_GUI OFF CACHE BOOL "" FORCE)
set(SINDRECPP_WITH_JSON ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    SindreCpp
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.."
)
FetchContent_MakeAvailable(SindreCpp)

add_executable(fetchcontent_consumer main.cpp)
target_link_libraries(fetchcontent_consumer PRIVATE SindreCpp::SindreCpp)

enable_testing()
add_test(NAME fetchcontent.consumer COMMAND fetchcontent_consumer)
