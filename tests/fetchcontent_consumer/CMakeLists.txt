cmake_minimum_required(VERSION 3.20)
project(SindreCppFetchContentConsumer LANGUAGES CXX)

include(FetchContent)
set(SINDRECPP_WITH_JSON ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    SindreCpp
    GIT_REPOSITORY https://github.com/SindreYang/SindreCpp.git
    GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(SindreCpp)

add_executable(fetchcontent_consumer main.cpp)
target_link_libraries(fetchcontent_consumer PRIVATE SindreCpp::Json)
