# Eigen 是头文件库；ExternalProject 只下载并解压固定版本，
# Math 模块直接引用其源码目录并在安装时复制 Eigen/ 和 unsupported/Eigen/。
include(ExternalProject)

set(SINDRE_THIRD_MATH_EIGEN_VERSION "3.4.1")
set(SINDRE_THIRD_MATH_EIGEN_URL
    "https://gitlab.com/libeigen/eigen/-/archive/3.4.1/eigen-3.4.1.tar.gz")
set(SINDRE_THIRD_MATH_EIGEN_SHA256
    "b93c667d1b69265cdb4d9f30ec21f8facbbe8b307cf34c0b9942834c6d4fdbe2")

ExternalProject_Add(
    sindre_ext_eigen
    PREFIX "${SINDRE_THIRD_PARTY_BUILD_CACHE_DIR}/eigen"
    URL "${SINDRE_THIRD_MATH_EIGEN_URL}"
    URL_HASH "SHA256=${SINDRE_THIRD_MATH_EIGEN_SHA256}"
    DOWNLOAD_DIR "${SINDRE_THIRD_PARTY_DOWNLOAD_DIR}/eigen"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "")
ExternalProject_Get_Property(sindre_ext_eigen SOURCE_DIR)
file(MAKE_DIRECTORY "${SOURCE_DIR}")
set(SINDRE_THIRD_MATH_EIGEN_SOURCE_DIR "${SOURCE_DIR}")
