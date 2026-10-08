# CsString 需要编译为动态库。这里固定源码版本、校验值、安装前缀和跨平台产物名，
# 供 General 的导入 target 使用，并由构建后步骤复制运行时 DLL。
include(ExternalProject)

set(SINDRE_THIRD_GENERAL_CS_STRING_VERSION "string-1.4.1")
set(SINDRE_THIRD_GENERAL_CS_STRING_URL
    "https://github.com/copperspice/cs_string/archive/refs/tags/string-1.4.1.zip")
set(SINDRE_THIRD_GENERAL_CS_STRING_SHA256
    "69b2cf7f848eb42038bf0c892a4aa18af9baa11ce2d67377d78a80d9248e202e")
if(WIN32)
    set(_sindre_cs_string_byproduct "<INSTALL_DIR>/lib/CsString.lib")
else()
    set(_sindre_cs_string_byproduct "<INSTALL_DIR>/lib/libCsString${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()

ExternalProject_Add(
    sindre_ext_cs_string
    PREFIX "${SINDRE_THIRD_PARTY_CACHE_DIR}/cs_string"
    URL "${SINDRE_THIRD_GENERAL_CS_STRING_URL}"
    URL_HASH "SHA256=${SINDRE_THIRD_GENERAL_CS_STRING_SHA256}"
    DOWNLOAD_DIR "${SINDRE_THIRD_PARTY_DOWNLOAD_DIR}/cs_string"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    CMAKE_ARGS
        ${SINDRE_EXTERNAL_PROJECT_CMAKE_ARGS}
        -DBUILD_TESTS=OFF
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    BUILD_BYPRODUCTS ${_sindre_cs_string_byproduct})
ExternalProject_Get_Property(sindre_ext_cs_string SOURCE_DIR)
set(SINDRE_THIRD_GENERAL_CS_STRING_SOURCE_DIR "${SOURCE_DIR}")
ExternalProject_Get_Property(sindre_ext_cs_string INSTALL_DIR)
set(SINDRE_THIRD_GENERAL_CS_STRING_INCLUDE_DIR "${INSTALL_DIR}/include")
file(MAKE_DIRECTORY "${SINDRE_THIRD_GENERAL_CS_STRING_INCLUDE_DIR}")
if(WIN32)
    set(SINDRE_THIRD_GENERAL_CS_STRING_LIBRARY
        "${INSTALL_DIR}/lib/CsString.lib")
    set(SINDRE_THIRD_GENERAL_CS_STRING_RUNTIME
        "${INSTALL_DIR}/bin/CsString.dll")
else()
    set(SINDRE_THIRD_GENERAL_CS_STRING_LIBRARY
        "${INSTALL_DIR}/lib/libCsString${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(SINDRE_THIRD_GENERAL_CS_STRING_RUNTIME
        "${SINDRE_THIRD_GENERAL_CS_STRING_LIBRARY}")
endif()
