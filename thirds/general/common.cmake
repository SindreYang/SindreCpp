include_guard(GLOBAL)

# General's source archives are fixed implementation inputs.  They are
# provisioned locally and ignored by Git; the public repository contains only
# this manifest and the platform selectors.
set(SINDRE_THIRD_GENERAL_SOURCE_ROOT
    "${SINDRE_THIRDS_DIR}/general/sources" CACHE PATH
    "Fixed General third-party source cache")
set(SINDRE_THIRD_GENERAL_PACKAGE_CACHE_ROOT
    "${SINDRE_THIRDS_DIR}/general/packages" CACHE PATH
    "Fixed General platform package cache")

set(SINDRE_THIRD_GENERAL_CS_STRING_REPOSITORY
    "https://github.com/copperspice/cs_string.git")
# CsString 1.4.1 是当前支持的最后一个 C++17 版本。
# 这里不设为 cache 变量，避免改变公共字符串 ABI 或误选 C++20-only 的 CsString 2.x。
set(SINDRE_THIRD_GENERAL_CS_STRING_TAG "string-1.4.1")

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
set(SINDRE_THIRD_GENERAL_OPENSSL_VERSION "3.3.0#1")
set(SINDRE_THIRD_GENERAL_ZLIB_VERSION "1.3.1")
set(SINDRE_THIRD_GENERAL_CRASHPAD_VERSION "2022-09-05#5")
