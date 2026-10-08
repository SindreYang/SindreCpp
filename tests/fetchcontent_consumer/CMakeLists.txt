cmake_minimum_required(VERSION 3.20)

# FetchContent 消费者测试：验证项目作为源码依赖加入时，最小 General target 可用。
project(sindreFetchContentConsumer LANGUAGES CXX)

include(FetchContent)
foreach(module IN ITEMS UTILS_3D UTILS_PY)
    set(SINDRE_WITH_${module} OFF CACHE BOOL "" FORCE)
endforeach()
set(SINDRE_WITH_GUI OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    sindre
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.."
)
FetchContent_MakeAvailable(sindre)

add_executable(fetchcontent_consumer main.cpp)
target_link_libraries(fetchcontent_consumer PRIVATE sindre::general)

enable_testing()
add_test(NAME fetchcontent.consumer COMMAND fetchcontent_consumer)
