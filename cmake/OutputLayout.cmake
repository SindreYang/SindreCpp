include_guard(GLOBAL)

set(SINDRE_BIN_DIR "${CMAKE_BINARY_DIR}/bin" CACHE PATH
    "Directory for sindre executables and runtime libraries")

file(MAKE_DIRECTORY "${SINDRE_BIN_DIR}")

# Keep every configuration in the same build/bin directory. This is intentional
# for this library: Windows DLLs must sit next to executables.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${SINDRE_BIN_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${SINDRE_BIN_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${SINDRE_BIN_DIR}")
foreach(configuration IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${configuration}" configuration_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${configuration_upper} "${SINDRE_BIN_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${configuration_upper} "${SINDRE_BIN_DIR}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${configuration_upper} "${SINDRE_BIN_DIR}")
endforeach()
