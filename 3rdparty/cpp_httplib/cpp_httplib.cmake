# cpp-httplib 是单头文件网络库。ExternalProject 只负责可复现下载和解压，
# 不执行其独立构建，也不把上游 target 泄露到 Sindre 的公共导出中。
include(ExternalProject)

set(SINDRE_THIRD_GENERAL_HTTPLIB_VERSION "0.56.0")
set(SINDRE_THIRD_GENERAL_HTTPLIB_URL
    "https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.56.0.zip")
set(SINDRE_THIRD_GENERAL_HTTPLIB_SHA256
    "a8c0ed8e198b71eed9ef55e9a69de31be13b9519959fac4d20abbe60a828fa45")

ExternalProject_Add(
    sindre_ext_cpp_httplib
    PREFIX "${SINDRE_THIRD_PARTY_CACHE_DIR}/cpp_httplib"
    URL "${SINDRE_THIRD_GENERAL_HTTPLIB_URL}"
    URL_HASH "SHA256=${SINDRE_THIRD_GENERAL_HTTPLIB_SHA256}"
    DOWNLOAD_DIR "${SINDRE_THIRD_PARTY_DOWNLOAD_DIR}/cpp_httplib"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "")
ExternalProject_Get_Property(sindre_ext_cpp_httplib SOURCE_DIR)
set(SINDRE_THIRD_GENERAL_HTTPLIB_SOURCE_DIR "${SOURCE_DIR}")
file(MAKE_DIRECTORY "${SINDRE_THIRD_GENERAL_HTTPLIB_SOURCE_DIR}")
