cmake_minimum_required(VERSION 3.20)

# 安装包消费者测试：只通过 find_package(sindre CONFIG) 使用已安装导出，
# 用于发现安装包 target、头文件和运行时布局问题。
project(sindreInstalledConsumer LANGUAGES CXX)

find_package(sindre CONFIG REQUIRED)

add_executable(sindre_installed_consumer main.cpp)
target_link_libraries(sindre_installed_consumer PRIVATE sindre::general)

if(TARGET sindre::ai)
    add_executable(sindre_installed_ai_consumer ai.cpp)
    target_link_libraries(sindre_installed_ai_consumer PRIVATE sindre::ai)
    if(WIN32)
        get_filename_component(_sindre_installed_prefix
            "${sindre_DIR}/../../.." ABSOLUTE)
        if(EXISTS "${_sindre_installed_prefix}/bin/onnxruntime.dll")
            add_custom_command(TARGET sindre_installed_ai_consumer POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_sindre_installed_prefix}/bin/onnxruntime.dll"
                    "$<TARGET_FILE_DIR:sindre_installed_ai_consumer>/onnxruntime.dll")
        endif()
    endif()
    enable_testing()
    add_test(NAME sindre.installed.ai
        COMMAND sindre_installed_ai_consumer
            "${CMAKE_CURRENT_LIST_DIR}/../../modules/ai/tests/models/identity.onnx")
endif()
