# stb is a small header-only dependency. Pin the source repository to an
# immutable commit and verify the archive before exposing stb_image.h.
include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(SINDRE_THIRD_GUI_STB_VERSION "2c980bb59875b0d32144a71867fbdebb2f77cd20")
set(SINDRE_THIRD_GUI_STB_URL
    "https://github.com/nothings/stb/archive/2c980bb59875b0d32144a71867fbdebb2f77cd20.zip")
set(SINDRE_THIRD_GUI_STB_SHA256
    "8e59f72b0780690cda64726804269f638a3be77b9d1506ea95f443f7964bccf0")

function(sindre_3rdparty_setup_stb)
    if(DEFINED SINDRE_THIRD_GUI_STB_SOURCE_DIR AND
       EXISTS "${SINDRE_THIRD_GUI_STB_SOURCE_DIR}/stb_image.h")
        return()
    endif()
    set(FETCHCONTENT_BASE_DIR
        "${SINDRE_THIRD_PARTY_CACHE_DIR}/fetchcontent"
        CACHE PATH "FetchContent source cache for source-only dependencies" FORCE)
    file(TO_CMAKE_PATH
        "${SINDRE_THIRD_PARTY_CACHE_DIR}/stb/${SINDRE_THIRD_GUI_STB_VERSION}"
        _sindre_stb_source_dir)
    FetchContent_Declare(sindre_stb_source
        URL "${SINDRE_THIRD_GUI_STB_URL}"
        URL_HASH "SHA256=${SINDRE_THIRD_GUI_STB_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_DIR
            "${_sindre_stb_source_dir}")
    FetchContent_MakeAvailable(sindre_stb_source)
    if(NOT EXISTS "${sindre_stb_source_SOURCE_DIR}/stb_image.h")
        message(FATAL_ERROR "Pinned stb source does not contain stb_image.h")
    endif()
    set(SINDRE_THIRD_GUI_STB_SOURCE_DIR
        "${sindre_stb_source_SOURCE_DIR}" PARENT_SCOPE)
endfunction()
