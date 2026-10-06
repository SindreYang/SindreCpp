include_guard(GLOBAL)

if(WIN32)
    set(SINDRECPP_PLATFORM_NAME "win")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SINDRECPP_PLATFORM_NAME "linux")
else()
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" SINDRECPP_PLATFORM_NAME)
endif()

set(SINDRECPP_BIN_DIR "${CMAKE_BINARY_DIR}/bin" CACHE PATH
    "Directory for SindreCpp executables and runtime libraries")

file(MAKE_DIRECTORY "${SINDRECPP_BIN_DIR}")

# Keep every configuration in the same platform-specific bin directory. This
# is intentional for this library: Windows DLLs must sit next to executables.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${SINDRECPP_BIN_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${SINDRECPP_BIN_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${SINDRECPP_BIN_DIR}")
foreach(configuration IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${configuration}" configuration_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${configuration_upper} "${SINDRECPP_BIN_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${configuration_upper} "${SINDRECPP_BIN_DIR}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${configuration_upper} "${SINDRECPP_BIN_DIR}")
endforeach()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    get_filename_component(_sindrecpp_expected_build_dir
        "${CMAKE_SOURCE_DIR}/build_${SINDRECPP_PLATFORM_NAME}" ABSOLUTE)
    get_filename_component(_sindrecpp_actual_build_dir "${CMAKE_BINARY_DIR}" ABSOLUTE)
    if(NOT _sindrecpp_actual_build_dir STREQUAL _sindrecpp_expected_build_dir)
        message(STATUS
            "SindreCpp recommended build directory: ${_sindrecpp_expected_build_dir}")
    endif()
endif()
