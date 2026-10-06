include_guard(GLOBAL)

include(FetchContent)

# This file is included by the root project. The cache path also remains
# available when a standalone example embeds SindreCpp with add_subdirectory().
set(SINDRECPP_THIRDS_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH
    "SindreCpp third-party dependency registry")

function(sindrecpp_thirds_declare_git name repository tag)
    if(NOT repository OR NOT tag)
        message(FATAL_ERROR
            "Third-party Git dependency '${name}' requires a repository and a fixed tag/commit")
    endif()
    FetchContent_Declare(${name}
        GIT_REPOSITORY "${repository}"
        GIT_TAG "${tag}"
        GIT_SHALLOW TRUE)
endfunction()

function(sindrecpp_thirds_declare_url name url sha256)
    if(NOT url OR NOT sha256)
        message(FATAL_ERROR
            "Third-party URL dependency '${name}' requires a URL and SHA256 checksum")
    endif()
    FetchContent_Declare(${name}
        URL "${url}"
        URL_HASH "SHA256=${sha256}")
endfunction()
