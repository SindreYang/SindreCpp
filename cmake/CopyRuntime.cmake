# 安装或构建后复制动态库的脚本入口。运行时文件不存在时跳过，
# 这样可兼容只包含静态库或跨平台路径不同的依赖配置。
if(NOT DEFINED SINDRE_RUNTIME_DEST OR NOT DEFINED SINDRE_RUNTIME_FILES)
    return()
endif()

foreach(runtime_file IN LISTS SINDRE_RUNTIME_FILES)
    if(EXISTS "${runtime_file}")
        get_filename_component(runtime_name "${runtime_file}" NAME)
        set(runtime_destination "${SINDRE_RUNTIME_DEST}/${runtime_name}")
        file(LOCK "${runtime_destination}.lock" GUARD PROCESS TIMEOUT 300)
        file(COPY_FILE "${runtime_file}" "${runtime_destination}" ONLY_IF_DIFFERENT)
        file(REMOVE "${runtime_destination}.lock")
    endif()
endforeach()
