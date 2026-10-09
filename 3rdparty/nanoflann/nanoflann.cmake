# nanoflann is a header-only KD-tree library used by the lightweight
# nearest-neighbor API in Utils_3d. Keep the upstream target private to the
# module; Sindre exposes its own result and neighbor types.
include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

set(SINDRE_THIRD_UTILS_3D_NANOFLANN_VERSION "1.8.0")
set(SINDRE_THIRD_UTILS_3D_NANOFLANN_URL
    "https://github.com/jlblancoc/nanoflann/archive/refs/tags/v1.8.0.zip")
set(SINDRE_THIRD_UTILS_3D_NANOFLANN_SHA256
    "da72953234936c0dde0b02b2edea1cbbdbb69b72de3f808dbc8ea0b515eacccd")

function(sindre_3rdparty_setup_nanoflann)
    if(TARGET sindre::nanoflann)
        return()
    endif()

    set(FETCHCONTENT_BASE_DIR
        "${SINDRE_THIRD_PARTY_CACHE_DIR}/fetchcontent"
        CACHE PATH "FetchContent source cache for source-only dependencies" FORCE)
    file(TO_CMAKE_PATH
        "${SINDRE_THIRD_PARTY_CACHE_DIR}/nanoflann/${SINDRE_THIRD_UTILS_3D_NANOFLANN_VERSION}"
        _sindre_nanoflann_source_dir)
    FetchContent_Declare(sindre_nanoflann_source
        URL "${SINDRE_THIRD_UTILS_3D_NANOFLANN_URL}"
        URL_HASH "SHA256=${SINDRE_THIRD_UTILS_3D_NANOFLANN_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_DIR "${_sindre_nanoflann_source_dir}")
    FetchContent_GetProperties(sindre_nanoflann_source)
    if(NOT sindre_nanoflann_source_POPULATED)
        FetchContent_Populate(sindre_nanoflann_source)
    endif()

    add_library(sindre_nanoflann INTERFACE)
    add_library(sindre::nanoflann ALIAS sindre_nanoflann)
    target_include_directories(sindre_nanoflann SYSTEM INTERFACE
        "$<BUILD_INTERFACE:${sindre_nanoflann_source_SOURCE_DIR}/include>")
    # nanoflann is needed only while compiling the private Utils_3d sources.
    # Exporting this build-cache include target would make installed static
    # consumers depend on an internal, non-installed target.
    set(SINDRE_NANOFLANN_INCLUDE_DIR
        "${sindre_nanoflann_source_SOURCE_DIR}/include" PARENT_SCOPE)
endfunction()
