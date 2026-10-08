# argparse 只提供头文件；使用固定版本和 SHA256 校验下载源码，
# 不创建额外库 target，由 General 模块按头文件路径使用。
include(ExternalProject)

set(SINDRE_THIRD_GENERAL_ARGPARSE_VERSION "3.2")
set(SINDRE_THIRD_GENERAL_ARGPARSE_URL
    "https://github.com/p-ranav/argparse/archive/refs/tags/v3.2.zip")
set(SINDRE_THIRD_GENERAL_ARGPARSE_SHA256
    "14c1a0e975d6877dfeaf52a1e79e54f70169a847e29c7e13aa7fe68a3d0ecbf1")

ExternalProject_Add(
    sindre_ext_argparse
    PREFIX "${SINDRE_THIRD_PARTY_CACHE_DIR}/argparse"
    URL "${SINDRE_THIRD_GENERAL_ARGPARSE_URL}"
    URL_HASH "SHA256=${SINDRE_THIRD_GENERAL_ARGPARSE_SHA256}"
    DOWNLOAD_DIR "${SINDRE_THIRD_PARTY_DOWNLOAD_DIR}/argparse"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "")
ExternalProject_Get_Property(sindre_ext_argparse SOURCE_DIR)
set(SINDRE_THIRD_GENERAL_ARGPARSE_SOURCE_DIR "${SOURCE_DIR}")
file(MAKE_DIRECTORY "${SINDRE_THIRD_GENERAL_ARGPARSE_SOURCE_DIR}/include")
