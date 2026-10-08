cmake_minimum_required(VERSION 3.20)

# 已安装 Utils_3d 消费者测试：验证 VTK/后端导出和公共头文件可被外部项目使用。
project(sindreInstalledUtils3dConsumer LANGUAGES CXX)

find_package(sindre CONFIG REQUIRED)

add_executable(sindre_installed_utils3d_consumer main.cpp)
target_link_libraries(sindre_installed_utils3d_consumer PRIVATE sindre::utils_3d)

enable_testing()
add_test(NAME sindre.installed.utils_3d
    COMMAND sindre_installed_utils3d_consumer)
