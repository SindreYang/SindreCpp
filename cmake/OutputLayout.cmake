include_guard(GLOBAL)

# 统一所有生成器的输出位置，便于运行测试、收集 DLL 和清理构建产物。
# 该目录属于构建树，不属于源码安装包。

set(SINDRE_BIN_DIR "${CMAKE_BINARY_DIR}/bin" CACHE PATH
    "Directory for sindre executables and runtime libraries")

file(MAKE_DIRECTORY "${SINDRE_BIN_DIR}")

# Keep every configuration in the same profile/bin directory. This is
# intentional for this library: Windows DLLs must sit next to executables.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${SINDRE_BIN_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${SINDRE_BIN_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${SINDRE_BIN_DIR}")
foreach(configuration IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${configuration}" configuration_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${configuration_upper} "${SINDRE_BIN_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${configuration_upper} "${SINDRE_BIN_DIR}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${configuration_upper} "${SINDRE_BIN_DIR}")
endforeach()
