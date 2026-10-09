# GLFW is a small source dependency. Keep it out of vcpkg and build the
# pinned archive as part of the GUI module's dependency graph.
include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(SINDRE_THIRD_GUI_GLFW_VERSION "3.4")
set(SINDRE_THIRD_GUI_GLFW_URL
    "https://github.com/glfw/glfw/archive/refs/tags/3.4.zip")
set(SINDRE_THIRD_GUI_GLFW_SHA256
    "a133ddc3d3c66143eba9035621db8e0bcf34dba1ee9514a9e23e96afd39fd57a")

function(sindre_3rdparty_setup_glfw)
    if(TARGET glfw OR TARGET glfw3::glfw)
        return()
    endif()

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    set(GLFW_LIBRARY_TYPE STATIC CACHE STRING "" FORCE)
    set(FETCHCONTENT_BASE_DIR
        "${SINDRE_THIRD_PARTY_CACHE_DIR}/fetchcontent"
        CACHE PATH "FetchContent source cache for source-only dependencies" FORCE)
    file(TO_CMAKE_PATH
        "${SINDRE_THIRD_PARTY_CACHE_DIR}/glfw/${SINDRE_THIRD_GUI_GLFW_VERSION}"
        _sindre_glfw_source_dir)
    FetchContent_Declare(sindre_glfw_source
        URL "${SINDRE_THIRD_GUI_GLFW_URL}"
        URL_HASH "SHA256=${SINDRE_THIRD_GUI_GLFW_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_DIR
            "${_sindre_glfw_source_dir}")
    FetchContent_MakeAvailable(sindre_glfw_source)
    if(NOT TARGET glfw AND NOT TARGET glfw3::glfw)
        message(FATAL_ERROR "Pinned GLFW did not create the expected glfw target")
    endif()
endfunction()
