include_guard(GLOBAL)

set(SINDRE_THIRD_GENERAL_OPENBLAS_VERSION "0.3.34")
set(SINDRE_THIRD_GENERAL_OPENBLAS_CONFIG_PACKAGE "OpenBLAS")
set(SINDRE_THIRD_GENERAL_OPENBLAS_IMPORT_LIBRARY "libopenblas.lib")

set(SINDRE_THIRD_GENERAL_CS_STRING_REPOSITORY
    "https://github.com/copperspice/cs_string.git")
# CsString 1.4.1 is the last supported C++17 release.  This is deliberately
# not a cache variable: changing it would change the public string ABI and
# could silently select the C++20-only CsString 2.x line.
set(SINDRE_THIRD_GENERAL_CS_STRING_TAG
    "string-1.4.1")

set(SINDRE_THIRD_GENERAL_SPDLOG_REPOSITORY "https://github.com/gabime/spdlog.git")
set(SINDRE_THIRD_GENERAL_SPDLOG_TAG "v1.17.0")

set(SINDRE_THIRD_GENERAL_HTTPLIB_REPOSITORY
    "https://github.com/yhirose/cpp-httplib.git")
set(SINDRE_THIRD_GENERAL_HTTPLIB_TAG "v0.56.0")

set(SINDRE_THIRD_GENERAL_SIMDJSON_REPOSITORY
    "https://github.com/simdjson/simdjson.git")
set(SINDRE_THIRD_GENERAL_SIMDJSON_TAG "v4.6.11")

set(SINDRE_THIRD_GENERAL_ARGPARSE_REPOSITORY
    "https://github.com/p-ranav/argparse.git")
set(SINDRE_THIRD_GENERAL_ARGPARSE_TAG "v3.2")

set(SINDRE_THIRD_GENERAL_RE2_REPOSITORY "https://github.com/google/re2.git")
set(SINDRE_THIRD_GENERAL_RE2_VERSION "2024-04-01#2")
set(SINDRE_THIRD_GENERAL_CRASHPAD_VERSION "2024-04-11")
set(SINDRE_THIRD_GENERAL_OPENSSL_VERSION "3.3.0#1")
set(SINDRE_THIRD_GENERAL_ZLIB_VERSION "1.3.1")

# Crashpad, RE2, OpenSSL and zlib are consumed from this fixed package
# profile.  The profile is provisioned outside the source tree and is never
# replaced by a host installation.
set(SINDRE_THIRD_GENERAL_PACKAGE_PROFILE "general-x64-windows")
set(SINDRE_THIRD_GENERAL_PACKAGE_ROOT
    "${SINDRE_THIRDS_DIR}/general/packages/${SINDRE_THIRD_GENERAL_PACKAGE_PROFILE}/installed/x64-windows")
