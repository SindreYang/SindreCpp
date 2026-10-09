# Abseil 是 RE2 的源码依赖。固定源码包并安装到第三方缓存，禁止通过
# vcpkg 或宿主系统隐式替换其 ABI。
include(ExternalProject)

set(SINDRE_THIRD_GENERAL_ABSEIL_VERSION "20240116.2")
set(SINDRE_THIRD_GENERAL_ABSEIL_URL
    "https://github.com/abseil/abseil-cpp/archive/refs/tags/20240116.2.zip")
set(SINDRE_THIRD_GENERAL_ABSEIL_SHA256
    "69909DD729932CBBABB9EEAFF56179E8D124515F5D3AC906663D573D700B4C7D")
set(_sindre_abseil_components
    bad_optional_access bad_variant_access base city civil_time
    cord_internal cord cordz_functions cordz_handle cordz_info
    crc_cord_state crc_cpu_detect crc_internal crc32c debugging_internal
    demangle_internal exponential_biased flags_commandlineflag_internal
    flags_commandlineflag flags_config flags_internal flags_marshalling
    flags_private_handle_accessor flags_program_name flags_reflection
    graphcycles_internal hash hashtablez_sampler int128 kernel_timeout_internal
    log_severity low_level_hash malloc_internal raw_hash_set raw_logging_internal
    spinlock_wait stacktrace str_format_internal string_view strings_internal
    strings symbolize synchronization throw_delegate time_zone time)
set(_sindre_abseil_byproducts
    <INSTALL_DIR>/lib/cmake/absl/abslConfig.cmake)
foreach(_sindre_abseil_component IN LISTS _sindre_abseil_components)
    list(APPEND _sindre_abseil_byproducts
        "<INSTALL_DIR>/lib/${CMAKE_STATIC_LIBRARY_PREFIX}absl_${_sindre_abseil_component}${CMAKE_STATIC_LIBRARY_SUFFIX}")
endforeach()

ExternalProject_Add(
    sindre_ext_abseil
    PREFIX "${SINDRE_THIRD_PARTY_BUILD_CACHE_DIR}/abseil"
    URL "${SINDRE_THIRD_GENERAL_ABSEIL_URL}"
    URL_HASH "SHA256=${SINDRE_THIRD_GENERAL_ABSEIL_SHA256}"
    DOWNLOAD_DIR "${SINDRE_THIRD_PARTY_DOWNLOAD_DIR}/abseil"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    CMAKE_ARGS
        ${SINDRE_EXTERNAL_PROJECT_CMAKE_ARGS}
        -DABSL_BUILD_TESTING=OFF
        -DABSL_PROPAGATE_CXX_STD=ON
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    BUILD_BYPRODUCTS ${_sindre_abseil_byproducts})
ExternalProject_Get_Property(sindre_ext_abseil INSTALL_DIR)
set(SINDRE_THIRD_GENERAL_ABSEIL_INSTALL_DIR "${INSTALL_DIR}")
