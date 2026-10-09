# ImGui 没有适合本项目的独立安装库目标，需要由 GUI 模块编译源文件，
# 因此使用 FetchContent，并将源码缓存放入统一第三方缓存目录。
include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

set(SINDRE_THIRD_GUI_IMGUI_URL
    "https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b.zip")
set(SINDRE_THIRD_GUI_IMGUI_SHA256
    "e1c46d676c2bcb7ced847ba27f50553e33a19db97b3cadaec7f8be64449139f8")

function(sindre_3rdparty_setup_imgui version)
    if(NOT version)
        message(FATAL_ERROR "Dear ImGui version must be provided")
    endif()
    if(NOT version STREQUAL "1.92.9b")
        message(FATAL_ERROR
            "This build profile pins Dear ImGui to 1.92.9b; "
            "a different version would invalidate the archive hash")
    endif()
    set(FETCHCONTENT_BASE_DIR
        "${SINDRE_THIRD_PARTY_CACHE_DIR}/fetchcontent"
        CACHE PATH "FetchContent source cache for source-only dependencies" FORCE)
    file(TO_CMAKE_PATH "${SINDRE_THIRD_PARTY_CACHE_DIR}/imgui/${version}"
        _sindre_imgui_source_dir)
    FetchContent_Declare(sindre_imgui_source
        URL "${SINDRE_THIRD_GUI_IMGUI_URL}"
        URL_HASH "SHA256=${SINDRE_THIRD_GUI_IMGUI_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_DIR "${_sindre_imgui_source_dir}")
    FetchContent_GetProperties(sindre_imgui_source)
    if(NOT sindre_imgui_source_POPULATED)
        FetchContent_Populate(sindre_imgui_source)
    endif()
    set(SINDRE_THIRD_GUI_IMGUI_SOURCE_DIR
        "${sindre_imgui_source_SOURCE_DIR}" PARENT_SCOPE)
endfunction()
