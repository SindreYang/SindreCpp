cmake_minimum_required(VERSION 3.20)
project(SindreCppFetchContentConsumer LANGUAGES CXX)

include(FetchContent)
foreach(module IN ITEMS POINTER STRING LOG GUI PYTHON HTTP JSON CLI UTILS3D)
    set(SINDRECPP_WITH_${module} OFF CACHE BOOL "" FORCE)
endforeach()
set(SINDRECPP_WITH_JSON ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    SindreCpp
    GIT_REPOSITORY https://github.com/SindreYang/SindreCpp.git
    GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(SindreCpp)

add_executable(fetchcontent_consumer main.cpp)
target_link_libraries(fetchcontent_consumer PRIVATE SindreCpp::SindreCpp)
