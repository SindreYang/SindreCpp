# ImGui 没有适合本项目的独立安装库目标，需要由 GUI 模块编译源文件，
# 因此使用 FetchContent，并将源码缓存放入统一第三方缓存目录。
include(FetchContent)

set(SINDRE_THIRD_GUI_IMGUI_REPOSITORY
    "https://github.com/ocornut/imgui.git" CACHE STRING "Dear ImGui repository")

function(sindre_3rdparty_setup_imgui version)
    if(NOT version)
        message(FATAL_ERROR "Dear ImGui version must be provided")
    endif()
    set(FETCHCONTENT_BASE_DIR
        "${SINDRE_THIRD_PARTY_CACHE_DIR}/fetchcontent"
        CACHE PATH "FetchContent source cache for source-only dependencies" FORCE)
    FetchContent_Declare(sindre_imgui_source
        GIT_REPOSITORY "${SINDRE_THIRD_GUI_IMGUI_REPOSITORY}"
        GIT_TAG "v${version}"
        GIT_SHALLOW TRUE
        SOURCE_DIR "${SINDRE_THIRD_PARTY_CACHE_DIR}/imgui/${version}")
    FetchContent_GetProperties(sindre_imgui_source)
    if(NOT sindre_imgui_source_POPULATED)
        FetchContent_Populate(sindre_imgui_source)
    endif()
    set(SINDRE_THIRD_GUI_IMGUI_SOURCE_DIR
        "${sindre_imgui_source_SOURCE_DIR}" PARENT_SCOPE)
endfunction()
