include_guard(GLOBAL)

set(SINDRE_THIRD_GENERAL_OPENBLAS_MIN_VERSION
    "0.3.27" CACHE STRING "Minimum OpenBLAS version for the General Eigen backend")
set(SINDRE_THIRD_GENERAL_OPENBLAS_CONFIG_PACKAGE
    "OpenBLAS" CACHE STRING "OpenBLAS CMake package name")
set(SINDRE_THIRD_GENERAL_OPENBLAS_IMPORT_LIBRARY
    "libopenblas.lib" CACHE STRING "OpenBLAS Windows import/static library name")

set(SINDRE_THIRD_GENERAL_CS_STRING_REPOSITORY
    "https://github.com/copperspice/cs_string.git" CACHE STRING "CsString repository")
# CsString 1.4.1 is the last supported C++17 release.  This is deliberately
# not a cache variable: changing it would change the public string ABI and
# could silently select the C++20-only CsString 2.x line.
set(SINDRE_THIRD_GENERAL_CS_STRING_TAG
    "string-1.4.1")

set(SINDRE_THIRD_GENERAL_SPDLOG_REPOSITORY
    "https://github.com/gabime/spdlog.git" CACHE STRING "spdlog repository")
set(SINDRE_THIRD_GENERAL_SPDLOG_TAG
    "v1.17.0" CACHE STRING "spdlog fixed tag")

set(SINDRE_THIRD_GENERAL_HTTPLIB_REPOSITORY
    "https://github.com/yhirose/cpp-httplib.git" CACHE STRING "cpp-httplib repository")
set(SINDRE_THIRD_GENERAL_HTTPLIB_TAG
    "v0.56.0" CACHE STRING "cpp-httplib fixed tag")

set(SINDRE_THIRD_GENERAL_SIMDJSON_REPOSITORY
    "https://github.com/simdjson/simdjson.git" CACHE STRING "simdjson repository")
set(SINDRE_THIRD_GENERAL_SIMDJSON_TAG
    "v4.6.11" CACHE STRING "simdjson fixed tag")

set(SINDRE_THIRD_GENERAL_ARGPARSE_REPOSITORY
    "https://github.com/p-ranav/argparse.git" CACHE STRING "argparse repository")
set(SINDRE_THIRD_GENERAL_ARGPARSE_TAG
    "v3.2" CACHE STRING "argparse fixed tag")
