include_guard(GLOBAL)

include(FetchContent)

# This file is included by the root project. The cache path also remains
# available when a standalone example embeds sindre with add_subdirectory().
set(SINDRE_THIRDS_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH
    "sindre third-party dependency registry")

# Keep downloaded sources and generated FetchContent trees out of the source
# tree.  A caller may point this cache at a shared local/CI location; the
# default remains inside the build directory and is never installed or
# exported as part of sindrecpp.
set(SINDRE_THIRD_PARTY_CACHE_DIR "${CMAKE_BINARY_DIR}/_third_party_cache" CACHE PATH
    "Local cache for downloaded third-party sources and generated build data")
set(FETCHCONTENT_BASE_DIR "${SINDRE_THIRD_PARTY_CACHE_DIR}/fetchcontent" CACHE PATH
    "FetchContent source cache for sindrecpp third-party dependencies")

include("${SINDRE_THIRDS_DIR}/general/Dependencies.cmake")

function(sindre_thirds_declare_git name repository tag)
    if(NOT repository OR NOT tag)
        message(FATAL_ERROR
            "Third-party Git dependency '${name}' requires a repository and a fixed tag/commit")
    endif()
    FetchContent_Declare(${name}
        GIT_REPOSITORY "${repository}"
        GIT_TAG "${tag}"
        GIT_SHALLOW TRUE)
endfunction()

function(sindre_thirds_declare_local name source_dir)
    if(NOT IS_DIRECTORY "${source_dir}")
        message(FATAL_ERROR
            "Fixed third-party source for '${name}' was not found: ${source_dir}")
    endif()
    FetchContent_Declare(${name} SOURCE_DIR "${source_dir}")
endfunction()

function(sindre_thirds_declare_url name url sha256)
    if(NOT url OR NOT sha256)
        message(FATAL_ERROR
            "Third-party URL dependency '${name}' requires a URL and SHA256 checksum")
    endif()
    FetchContent_Declare(${name}
        URL "${url}"
        URL_HASH "SHA256=${sha256}")
endfunction()
