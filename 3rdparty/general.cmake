include_guard(GLOBAL)

# General 的固定平台依赖 profile。这里仅选择平台、架构和 triplet，
# 不在源码树内复制 vcpkg 或其他第三方二进制文件。

# General's fixed binary package cache is a local build input. It is
# intentionally outside the source registry and may be shared by CI.
set(SINDRE_THIRD_GENERAL_PACKAGE_CACHE_ROOT
    "${SINDRE_THIRD_PARTY_CACHE_DIR}/general/packages" CACHE PATH
    "Fixed General platform package cache")

# The platform file is intentionally reduced to profile selection here. The
# package payload itself is never part of this source tree.
if(WIN32)
    set(SINDRE_THIRD_GENERAL_PLATFORM "windows")
    set(SINDRE_THIRD_GENERAL_PACKAGE_PROFILE "general-x64-windows-static")
    set(SINDRE_THIRD_GENERAL_PACKAGE_TRIPLET "x64-windows-static")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SINDRE_THIRD_GENERAL_PLATFORM "linux")
    set(SINDRE_THIRD_GENERAL_PACKAGE_PROFILE "general-x64-linux")
    set(SINDRE_THIRD_GENERAL_PACKAGE_TRIPLET "x64-linux")
else()
    message(FATAL_ERROR
        "General currently supports Windows and Linux/WSL only; macOS is not supported")
endif()

set(SINDRE_THIRD_GENERAL_PACKAGE_ROOT
    "${SINDRE_THIRD_GENERAL_PACKAGE_CACHE_ROOT}/${SINDRE_THIRD_GENERAL_PACKAGE_PROFILE}/installed/${SINDRE_THIRD_GENERAL_PACKAGE_TRIPLET}")
set(SINDRE_GENERAL_PACKAGE_ROOT "${SINDRE_THIRD_GENERAL_PACKAGE_ROOT}")
if(NOT IS_DIRECTORY "${SINDRE_THIRD_GENERAL_PACKAGE_ROOT}/share")
    message(FATAL_ERROR
        "Fixed General ${SINDRE_THIRD_GENERAL_PLATFORM} dependency profile is missing: ${SINDRE_THIRD_GENERAL_PACKAGE_ROOT}")
endif()
