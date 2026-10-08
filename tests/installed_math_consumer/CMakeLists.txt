cmake_minimum_required(VERSION 3.20)

# 已安装 Math 消费者测试：验证 Eigen 头文件和 Math 导出 target 不依赖源码树。
project(sindreInstalledMathConsumer LANGUAGES CXX)

find_package(sindre CONFIG REQUIRED)

add_executable(sindre_installed_math_consumer main.cpp)
target_link_libraries(sindre_installed_math_consumer PRIVATE sindre::math)

enable_testing()
add_test(NAME sindre.installed.math
    COMMAND sindre_installed_math_consumer)
