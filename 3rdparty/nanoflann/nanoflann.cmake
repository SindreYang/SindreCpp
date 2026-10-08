# nanoflann is a header-only KD-tree library used by the lightweight
# nearest-neighbor API in Utils_3d. Keep the upstream target private to the
# module; Sindre exposes its own result and neighbor types.
include(FetchContent)

set(SINDRE_THIRD_UTILS_3D_NANOFLANN_VERSION "1.8.0" CACHE STRING
    "Pinned nanoflann version")
set(SINDRE_THIRD_UTILS_3D_NANOFLANN_REPOSITORY
    "https://github.com/jlblancoc/nanoflann.git" CACHE STRING
    "nanoflann repository")

function(sindre_3rdparty_setup_nanoflann)
    if(TARGET sindre::nanoflann)
        return()
    endif()

    set(FETCHCONTENT_BASE_DIR
        "${SINDRE_THIRD_PARTY_CACHE_DIR}/fetchcontent"
        CACHE PATH "FetchContent source cache for source-only dependencies" FORCE)
    FetchContent_Declare(sindre_nanoflann_source
        GIT_REPOSITORY "${SINDRE_THIRD_UTILS_3D_NANOFLANN_REPOSITORY}"
        GIT_TAG "v${SINDRE_THIRD_UTILS_3D_NANOFLANN_VERSION}"
        GIT_SHALLOW TRUE
        SOURCE_DIR "${SINDRE_THIRD_PARTY_CACHE_DIR}/nanoflann/${SINDRE_THIRD_UTILS_3D_NANOFLANN_VERSION}")
    FetchContent_GetProperties(sindre_nanoflann_source)
    if(NOT sindre_nanoflann_source_POPULATED)
        FetchContent_Populate(sindre_nanoflann_source)
    endif()

    add_library(sindre_nanoflann INTERFACE)
    add_library(sindre::nanoflann ALIAS sindre_nanoflann)
    target_include_directories(sindre_nanoflann SYSTEM INTERFACE
        "$<BUILD_INTERFACE:${sindre_nanoflann_source_SOURCE_DIR}/include>")
endfunction()
