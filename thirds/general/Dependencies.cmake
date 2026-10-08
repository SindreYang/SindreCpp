include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")

# The platform file is the only place that knows the package profile and
# triplet.  Keep this dispatch in the third-party registry rather than in a
# module implementation or in the public CMake options.
if(WIN32)
    include("${CMAKE_CURRENT_LIST_DIR}/win/Dependencies.cmake")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${CMAKE_CURRENT_LIST_DIR}/linux/Dependencies.cmake")
else()
    message(FATAL_ERROR
        "General currently supports Windows and Linux/WSL only; macOS is not supported")
endif()
