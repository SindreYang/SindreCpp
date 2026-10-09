# General 是全库基础模块：承载 Result、日志、系统、网络、JSON 和密码学等公共能力。
# 其公共 target 负责向上层传播统一的 C++17、线程、异常和第三方 ABI 约束。
sindre_add_interface_target(general "${SINDRE_SOURCE_DIR}/include")
target_include_directories(sindre_general INTERFACE
    $<INSTALL_INTERFACE:include>)
target_include_directories(sindre_general SYSTEM BEFORE INTERFACE
    $<BUILD_INTERFACE:${SINDRE_GENERAL_PACKAGE_ROOT}/include>)
target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_GENERAL=1)
add_dependencies(sindre_general
    sindre_ext_cs_string
    sindre_ext_spdlog
    sindre_ext_cpp_httplib
    sindre_ext_simdjson
    sindre_ext_argparse
    sindre_ext_abseil
    sindre_ext_re2
    sindre_ext_zlib)

# General 的默认产物是静态库，并且在 Windows 上固定使用静态 CRT 与
# `_ITERATOR_DEBUG_LEVEL=0`。这些设置必须通过已安装的聚合 target 传递给
# 外部消费者，否则消费者会用 `/MD` 或 STL 调试 ABI 编译自己的目标，最终
# 在链接阶段出现 LNK2038/LNK4098。共享库由 DLL 自己拥有 CRT，不应继承该
# 静态库约束。
if(MSVC AND NOT SINDRE_GENERAL_SHARED)
    target_compile_definitions(sindre_general INTERFACE
        _ITERATOR_DEBUG_LEVEL=0)
    target_compile_options(sindre_general INTERFACE /MT)
endif()

if(SINDRE_GENERAL_BUILD_LIBRARY)
    set(SINDRE_GENERAL_SOURCES
        src/core.cpp
        src/crypto.cpp
        src/system.cpp
        src/runtime.cpp
        src/cli.cpp
        src/string.cpp
        src/system_desktop.cpp
        src/network_common.cpp
        src/url.cpp
        src/http.cpp
        src/transfer.cpp
        src/diag.cpp
        )
    if(SINDRE_GENERAL_SHARED)
        add_library(sindre_general_runtime SHARED ${SINDRE_GENERAL_SOURCES})
    else()
        add_library(sindre_general_runtime STATIC ${SINDRE_GENERAL_SOURCES})
    endif()
    target_compile_features(sindre_general_runtime PUBLIC cxx_std_17)
    if(MSVC AND SINDRE_MSVC_STATIC_RUNTIME)
        # General 的运行库必须与固定平台依赖保持 /MT 一致；Debug 只影响
        # 本项目的编译配置，不切换到与依赖不兼容的 /MTd。
        set_property(TARGET sindre_general_runtime PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded")
    endif()
    target_include_directories(sindre_general_runtime PUBLIC
        $<BUILD_INTERFACE:${SINDRE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
    target_include_directories(sindre_general_runtime SYSTEM BEFORE PRIVATE
        "${SINDRE_GENERAL_PACKAGE_ROOT}/include"
        "${SINDRE_THIRD_GENERAL_SIMDJSON_INCLUDE_DIR}"
        "${SINDRE_THIRD_GENERAL_SPDLOG_INCLUDE_DIR}"
        "${SINDRE_THIRD_GENERAL_HTTPLIB_SOURCE_DIR}"
        "${SINDRE_THIRD_GENERAL_CS_STRING_INCLUDE_DIR}"
        "${SINDRE_THIRD_GENERAL_RE2_INCLUDE_DIR}"
        "${SINDRE_THIRD_GENERAL_ZLIB_INCLUDE_DIR}")
    target_link_libraries(sindre_general_runtime PUBLIC sindre_base)
    if(WIN32)
        target_link_libraries(sindre_general_runtime PRIVATE
            advapi32 shell32 user32 comdlg32 ole32 iphlpapi ws2_32 crypt32)
    endif()
    target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_GENERAL_BUILDING_LIBRARY=1)
    target_compile_definitions(sindre_general_runtime PRIVATE
        SINDRE_WITH_LOG=1
        SINDRE_WITH_HTTP=1
        SINDRE_WITH_JSON=1
        SINDRE_WITH_CLI=1
        SINDRE_WITH_RE2=1
        SINDRE_WITH_CRASHPAD=1
        SINDRE_WITH_ZLIB=1
        SINDRE_WITH_CRYPTO=1)
    if(SINDRE_GENERAL_SHARED)
        target_compile_definitions(sindre_general INTERFACE SINDRE_GENERAL_SHARED=1)
    endif()
    target_link_libraries(sindre_general INTERFACE sindre_general_runtime)
    set_target_properties(sindre_general_runtime PROPERTIES EXPORT_NAME general_runtime)
endif()

# CsString is the fixed C++17 backend of General's public string abstraction.
# Its source is downloaded by the isolated ExternalProject recipe; the small
# implementation is compiled as part of the General dependency boundary.
add_library(sindre_cs_string SHARED IMPORTED GLOBAL)
set_target_properties(sindre_cs_string PROPERTIES
    IMPORTED_LOCATION "${SINDRE_THIRD_GENERAL_CS_STRING_RUNTIME}"
    IMPORTED_IMPLIB "${SINDRE_THIRD_GENERAL_CS_STRING_LIBRARY}"
    IMPORTED_LOCATION_RELEASE "${SINDRE_THIRD_GENERAL_CS_STRING_RUNTIME}"
    IMPORTED_IMPLIB_RELEASE "${SINDRE_THIRD_GENERAL_CS_STRING_LIBRARY}"
    IMPORTED_LOCATION_RELWITHDEBINFO "${SINDRE_THIRD_GENERAL_CS_STRING_RUNTIME}"
    IMPORTED_IMPLIB_RELWITHDEBINFO "${SINDRE_THIRD_GENERAL_CS_STRING_LIBRARY}"
    IMPORTED_LOCATION_MINSIZEREL "${SINDRE_THIRD_GENERAL_CS_STRING_RUNTIME}"
    IMPORTED_IMPLIB_MINSIZEREL "${SINDRE_THIRD_GENERAL_CS_STRING_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SINDRE_THIRD_GENERAL_CS_STRING_INCLUDE_DIR}")
add_dependencies(sindre_cs_string sindre_ext_cs_string)
add_library(sindre_general_cs_string INTERFACE)
set_target_properties(sindre_general_cs_string PROPERTIES
    EXPORT_NAME general_cs_string)
if(WIN32)
    set(_sindre_cs_string_install_link
        "${CMAKE_INSTALL_LIBDIR}/CsString.lib")
else()
    set(_sindre_cs_string_install_link
        "${CMAKE_INSTALL_LIBDIR}/libCsString.so")
endif()
target_link_libraries(sindre_general_cs_string INTERFACE
    $<BUILD_INTERFACE:sindre_cs_string>
    $<INSTALL_INTERFACE:${_sindre_cs_string_install_link}>)
install(TARGETS sindre_general_cs_string EXPORT sindreTargets)
target_link_libraries(sindre_general INTERFACE sindre_general_cs_string)
if(TARGET sindre_general_runtime)
    target_link_libraries(sindre_general_runtime PRIVATE sindre_general_cs_string)
endif()
if(WIN32 AND IS_DIRECTORY "${SINDRE_GENERAL_PACKAGE_ROOT}/bin")
    sindre_install_runtime_dirs("${SINDRE_GENERAL_PACKAGE_ROOT}/bin")
endif()
if(WIN32)
    install(FILES "${SINDRE_THIRD_GENERAL_CS_STRING_LIBRARY}"
        DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES "${SINDRE_THIRD_GENERAL_CS_STRING_RUNTIME}"
        DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    install(FILES "${SINDRE_THIRD_GENERAL_CS_STRING_RUNTIME}"
        DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
    # General always owns the pinned implementation. Do not accept a parent
    # project's spdlog target: that would silently change the ABI and defeat
    # the fixed dependency profile.
    add_library(sindre_spdlog STATIC IMPORTED GLOBAL)
    set_target_properties(sindre_spdlog PROPERTIES
        IMPORTED_LOCATION "${SINDRE_THIRD_GENERAL_SPDLOG_LIBRARY}"
        IMPORTED_LOCATION_RELEASE "${SINDRE_THIRD_GENERAL_SPDLOG_LIBRARY}"
        IMPORTED_LOCATION_RELWITHDEBINFO "${SINDRE_THIRD_GENERAL_SPDLOG_LIBRARY}"
        IMPORTED_LOCATION_MINSIZEREL "${SINDRE_THIRD_GENERAL_SPDLOG_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SINDRE_THIRD_GENERAL_SPDLOG_INCLUDE_DIR}")
    add_dependencies(sindre_spdlog sindre_ext_spdlog)
    target_compile_definitions(sindre_spdlog INTERFACE SPDLOG_COMPILED_LIB)
    if(SINDRE_NO_EXCEPTIONS)
        target_compile_definitions(sindre_spdlog INTERFACE SPDLOG_NO_EXCEPTIONS)
    endif()
    if(NOT WIN32)
        find_package(Threads REQUIRED)
        target_link_libraries(sindre_spdlog INTERFACE Threads::Threads)
    endif()
    add_library(sindre_general_spdlog INTERFACE)
    set_target_properties(sindre_general_spdlog PROPERTIES
        EXPORT_NAME general_spdlog)
    if(WIN32)
        set(_sindre_spdlog_install_link
            "${CMAKE_INSTALL_LIBDIR}/spdlog.lib")
    else()
        set(_sindre_spdlog_install_link
            "${CMAKE_INSTALL_LIBDIR}/libspdlog.a")
    endif()
    target_link_libraries(sindre_general_spdlog INTERFACE
        $<BUILD_INTERFACE:sindre_spdlog>
        $<INSTALL_INTERFACE:${_sindre_spdlog_install_link}>)
    install(TARGETS sindre_general_spdlog EXPORT sindreTargets)
    set(SINDRE_SPDLOG_BUNDLED ON)
    target_link_libraries(sindre_general INTERFACE sindre_general_spdlog)
    target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_LOG=1)
    if(TARGET sindre_general_runtime)
        target_link_libraries(sindre_general_runtime PRIVATE sindre_general_spdlog)
        target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_WITH_LOG=1)
    endif()
    install(FILES "${SINDRE_THIRD_GENERAL_SPDLOG_LIBRARY}"
        DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(DIRECTORY "${SINDRE_THIRD_GENERAL_SPDLOG_INCLUDE_DIR}/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/spdlog")

find_package(OpenSSL 3.3.0 EXACT CONFIG REQUIRED COMPONENTS SSL Crypto
    PATHS "${SINDRE_GENERAL_PACKAGE_ROOT}" NO_DEFAULT_PATH)

    set(HTTPLIB_INSTALL OFF CACHE BOOL "Disable installation of cpp-httplib from General" FORCE)
    set(_sindre_httplib_nonblocking ON)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(_sindre_httplib_nonblocking OFF)
    endif()
    set(SINDRE_HTTPLIB_BUNDLED ON)
    add_library(sindre_general_httplib INTERFACE)
    set_target_properties(sindre_general_httplib PROPERTIES EXPORT_NAME general_httplib)
    target_include_directories(sindre_general_httplib INTERFACE
        $<BUILD_INTERFACE:${SINDRE_THIRD_GENERAL_HTTPLIB_SOURCE_DIR}>)
    add_dependencies(sindre_general_httplib sindre_ext_cpp_httplib)
    target_compile_definitions(sindre_general_httplib INTERFACE
        CPPHTTPLIB_ZLIB_SUPPORT CPPHTTPLIB_OPENSSL_SUPPORT)
    if(NOT (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
        target_compile_definitions(sindre_general_httplib INTERFACE
            CPPHTTPLIB_USE_NON_BLOCKING_GETADDRINFO)
    endif()
    # cpp-httplib is header-only. Keep its include path and compile policy in
    # the private adapter target, but do not export its upstream target.
    if(TARGET sindre_general_runtime)
        target_link_libraries(sindre_general_runtime PRIVATE sindre_general_httplib)
        target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_WITH_HTTP=1)
    endif()
    install(TARGETS sindre_general_httplib EXPORT sindreTargets)
    target_link_libraries(sindre_general INTERFACE OpenSSL::SSL OpenSSL::Crypto)
    if(TARGET sindre_general_runtime)
        target_link_libraries(sindre_general_runtime PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
    target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_HTTP=1)

    # General always uses the pinned upstream release. This keeps the public
    # JSON ABI and headers identical across MSVC, clang-cl and GCC instead of
    # silently selecting a different system/vcpkg simdjson version.
    add_library(sindre_simdjson STATIC IMPORTED GLOBAL)
    set_target_properties(sindre_simdjson PROPERTIES
        IMPORTED_LOCATION "${SINDRE_THIRD_GENERAL_SIMDJSON_LIBRARY}"
        IMPORTED_LOCATION_RELEASE "${SINDRE_THIRD_GENERAL_SIMDJSON_LIBRARY}"
        IMPORTED_LOCATION_RELWITHDEBINFO "${SINDRE_THIRD_GENERAL_SIMDJSON_LIBRARY}"
        IMPORTED_LOCATION_MINSIZEREL "${SINDRE_THIRD_GENERAL_SIMDJSON_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SINDRE_THIRD_GENERAL_SIMDJSON_INCLUDE_DIR}")
    add_dependencies(sindre_simdjson sindre_ext_simdjson)
    add_library(sindre_general_simdjson INTERFACE)
    set_target_properties(sindre_general_simdjson PROPERTIES
        EXPORT_NAME general_simdjson)
    if(WIN32)
        set(_sindre_simdjson_install_link
            "${CMAKE_INSTALL_LIBDIR}/simdjson.lib")
    else()
        set(_sindre_simdjson_install_link
            "${CMAKE_INSTALL_LIBDIR}/libsimdjson.a")
    endif()
    target_link_libraries(sindre_general_simdjson INTERFACE
        $<BUILD_INTERFACE:sindre_simdjson>
        $<INSTALL_INTERFACE:${_sindre_simdjson_install_link}>)
    target_link_libraries(sindre_general INTERFACE sindre_general_simdjson)
    if(TARGET sindre_general_runtime)
        target_link_libraries(sindre_general_runtime PRIVATE sindre_general_simdjson)
        target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_WITH_JSON=1 SIMDJSON_EXCEPTIONS=0)
    endif()
    target_include_directories(sindre_general SYSTEM BEFORE INTERFACE
        $<BUILD_INTERFACE:${SINDRE_THIRD_GENERAL_SIMDJSON_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include/simdjson>)
    install(TARGETS sindre_general_simdjson EXPORT sindreTargets)
    install(FILES "${SINDRE_THIRD_GENERAL_SIMDJSON_LIBRARY}"
        DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(DIRECTORY "${SINDRE_THIRD_GENERAL_SIMDJSON_INCLUDE_DIR}/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/simdjson")
    target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_JSON=1)
    # General's JSON Result boundary is non-throwing; keep the simdjson
    # formatter/DOM API in the same no-exception mode for all consumers.
    target_compile_definitions(sindre_general INTERFACE SIMDJSON_EXCEPTIONS=0)
    set(SINDRE_SIMDJSON_BUNDLED ON PARENT_SCOPE)

    set(SINDRE_CLI_USE_ARGPARSE OFF)
    if(NOT SINDRE_NO_EXCEPTIONS)
        set(SINDRE_CLI_USE_ARGPARSE ON)
        set(SINDRE_ARGPARSE_BUNDLED ON)
        # argparse is an implementation detail of the compiled General
        # runtime. The public CLI header contains only sindre-owned types.
        if(TARGET sindre_general_runtime)
            target_include_directories(sindre_general_runtime PRIVATE
                "${SINDRE_THIRD_GENERAL_ARGPARSE_SOURCE_DIR}/include")
            add_dependencies(sindre_general_runtime sindre_ext_argparse)
        endif()
    endif()
    target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_CLI=1)
    if(TARGET sindre_general_runtime)
        target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_WITH_CLI=1)
    endif()
    if(SINDRE_CLI_USE_ARGPARSE)
        install(FILES "${SINDRE_THIRD_GENERAL_ARGPARSE_SOURCE_DIR}/include/argparse/argparse.hpp"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/argparse")
    endif()

    # RE2 and Abseil are ExternalProjects, so their generated package files
    # do not exist during the first top-level configure. Use fixed imported
    # paths instead of find_package(), which would make a clean configure
    # depend on a pre-existing cache or a host/vcpkg installation.
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
    set(_sindre_abseil_libraries)
    foreach(_sindre_abseil_component IN LISTS _sindre_abseil_components)
        list(APPEND _sindre_abseil_libraries
            "${SINDRE_THIRD_GENERAL_ABSEIL_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}absl_${_sindre_abseil_component}${CMAKE_STATIC_LIBRARY_SUFFIX}")
    endforeach()
    add_library(sindre_general_abseil INTERFACE)
    add_dependencies(sindre_general_abseil sindre_ext_abseil)
    # Abseil is consumed as a set of static archives.  GNU ld requires the
    # archives to be rescanned because the component graph is cyclic and the
    # generated component list is not a dependency-order guarantee.  MSVC's
    # linker does not need (and does not support) a link group.
    if(UNIX AND NOT APPLE)
        target_link_libraries(sindre_general_abseil INTERFACE
            "$<LINK_GROUP:RESCAN,${_sindre_abseil_libraries}>")
    else()
        target_link_libraries(sindre_general_abseil INTERFACE ${_sindre_abseil_libraries})
    endif()
    add_library(sindre_general_re2_archive STATIC IMPORTED GLOBAL)
    set_target_properties(sindre_general_re2_archive PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo;MinSizeRel"
        IMPORTED_LOCATION_DEBUG "${SINDRE_THIRD_GENERAL_RE2_LIBRARY}"
        IMPORTED_LOCATION_RELEASE "${SINDRE_THIRD_GENERAL_RE2_LIBRARY}"
        IMPORTED_LOCATION_RELWITHDEBINFO "${SINDRE_THIRD_GENERAL_RE2_LIBRARY}"
        IMPORTED_LOCATION_MINSIZEREL "${SINDRE_THIRD_GENERAL_RE2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SINDRE_THIRD_GENERAL_RE2_INCLUDE_DIR}")
    add_dependencies(sindre_general_re2_archive sindre_ext_re2)
    add_library(sindre_general_re2 INTERFACE)
    set_target_properties(sindre_general_re2 PROPERTIES
        EXPORT_NAME general_re2)
    if(WIN32)
        set(_sindre_re2_install_link "${CMAKE_INSTALL_LIBDIR}/re2.lib")
    else()
        set(_sindre_re2_install_link "${CMAKE_INSTALL_LIBDIR}/libre2.a")
    endif()
    target_link_libraries(sindre_general_re2 INTERFACE
        $<BUILD_INTERFACE:sindre_general_re2_archive>
        $<BUILD_INTERFACE:sindre_general_abseil>
        $<INSTALL_INTERFACE:${_sindre_re2_install_link}>)
    install(TARGETS sindre_general_re2 EXPORT sindreTargets)
    install(FILES "${SINDRE_THIRD_GENERAL_RE2_LIBRARY}"
        DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(DIRECTORY "${SINDRE_THIRD_GENERAL_ABSEIL_INSTALL_DIR}/lib/"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        FILES_MATCHING PATTERN "${CMAKE_STATIC_LIBRARY_PREFIX}absl_*${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_link_libraries(sindre_general INTERFACE sindre_general_re2)
    if(TARGET sindre_general_runtime)
        target_link_libraries(sindre_general_runtime PRIVATE sindre_general_re2)
    endif()
    target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_RE2=1)
    if(TARGET sindre_general_runtime)
        target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_WITH_RE2=1)
    endif()

    set(_sindre_crashpad_metadata
        "${SINDRE_GENERAL_PACKAGE_ROOT}/share/crashpad/vcpkg.spdx.json")
    if(NOT EXISTS "${_sindre_crashpad_metadata}")
        message(FATAL_ERROR "Fixed Crashpad package metadata is missing")
    endif()
    file(READ "${_sindre_crashpad_metadata}" _sindre_crashpad_metadata_text)
    string(FIND "${_sindre_crashpad_metadata_text}"
        "crashpad:${SINDRE_THIRD_GENERAL_PACKAGE_TRIPLET}@${SINDRE_THIRD_GENERAL_CRASHPAD_VERSION}"
        _sindre_crashpad_version_position)
    if(_sindre_crashpad_version_position EQUAL -1)
        message(FATAL_ERROR
            "Fixed Crashpad package version ${SINDRE_THIRD_GENERAL_CRASHPAD_VERSION} was not found")
    endif()
    # Crashpad's package config asks for ZLIB.  Seed CMake's standard variables
    # with the fixed source-built target before reading that config, so it cannot
    # silently resolve the vcpkg zlib package.
    set(ZLIB_ROOT "${SINDRE_THIRD_GENERAL_ZLIB_INSTALL_DIR}")
    set(ZLIB_INCLUDE_DIR "${SINDRE_THIRD_GENERAL_ZLIB_INCLUDE_DIR}")
    set(ZLIB_INCLUDE_DIRS "${SINDRE_THIRD_GENERAL_ZLIB_INCLUDE_DIR}")
    set(ZLIB_LIBRARY "${SINDRE_THIRD_GENERAL_ZLIB_LIBRARY}")
    set(ZLIB_LIBRARY_RELEASE "${SINDRE_THIRD_GENERAL_ZLIB_LIBRARY}")
    set(ZLIB_LIBRARY_DEBUG "${SINDRE_THIRD_GENERAL_ZLIB_LIBRARY}")
    set(ZLIB_LIBRARIES "${SINDRE_THIRD_GENERAL_ZLIB_LIBRARY}")
    set(ZLIB_VERSION_STRING "${SINDRE_THIRD_GENERAL_ZLIB_VERSION}")
    find_package(crashpad CONFIG REQUIRED
        PATHS "${SINDRE_GENERAL_PACKAGE_ROOT}" NO_DEFAULT_PATH)
    if(TARGET crashpad)
        set(SINDRE_CRASHPAD_FIXED_TARGET crashpad)
    elseif(TARGET crashpad::crashpad)
        set(SINDRE_CRASHPAD_FIXED_TARGET crashpad::crashpad)
    else()
        message(FATAL_ERROR "Fixed General dependency profile has no Crashpad target")
    endif()
    get_target_property(_sindre_crashpad_links
        ${SINDRE_CRASHPAD_FIXED_TARGET} INTERFACE_LINK_LIBRARIES)
    if(_sindre_crashpad_links)
        list(REMOVE_ITEM _sindre_crashpad_links ZLIB::ZLIB)
        set_property(TARGET ${SINDRE_CRASHPAD_FIXED_TARGET}
            PROPERTY INTERFACE_LINK_LIBRARIES "${_sindre_crashpad_links}")
    endif()
    # Keep the external package behind an exported wrapper. Exporting the
    # vcpkg interface target itself would bake absolute build-machine library
    # paths into an installed sindre package.
    add_library(sindre_general_crashpad INTERFACE)
    set_target_properties(sindre_general_crashpad PROPERTIES EXPORT_NAME general_crashpad)
    target_link_libraries(sindre_general_crashpad INTERFACE
        $<BUILD_INTERFACE:${SINDRE_CRASHPAD_FIXED_TARGET}>)
    target_link_libraries(sindre_general INTERFACE sindre_general_crashpad)
    target_compile_definitions(sindre_general INTERFACE
        SINDRE_WITH_CRASHPAD=1 SINDRE_CRASHPAD_READY=1)
    if(TARGET sindre_general_runtime)
        target_link_libraries(sindre_general_runtime PRIVATE sindre_general_crashpad)
        target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_WITH_CRASHPAD=1 SINDRE_CRASHPAD_READY=1)
    endif()
    install(TARGETS sindre_general_crashpad EXPORT sindreTargets)

    add_library(sindre_general_zlib INTERFACE)
    set_target_properties(sindre_general_zlib PROPERTIES
        EXPORT_NAME general_zlib)
    if(WIN32)
        set(_sindre_zlib_install_link "${CMAKE_INSTALL_LIBDIR}/zlibstatic.lib")
    else()
        set(_sindre_zlib_install_link "${CMAKE_INSTALL_LIBDIR}/libz.a")
    endif()
    target_link_libraries(sindre_general_zlib INTERFACE
        $<BUILD_INTERFACE:sindre_zlib>
        $<INSTALL_INTERFACE:${_sindre_zlib_install_link}>)
    install(TARGETS sindre_general_zlib EXPORT sindreTargets)
    install(FILES "${SINDRE_THIRD_GENERAL_ZLIB_LIBRARY}"
        DESTINATION ${CMAKE_INSTALL_LIBDIR})
    target_link_libraries(sindre_general INTERFACE sindre_general_zlib)
    target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_ZLIB=1)
    if(TARGET sindre_general_runtime)
        target_link_libraries(sindre_general_runtime PRIVATE sindre_general_zlib)
        target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_WITH_ZLIB=1)
    endif()

if(SINDRE_BUILD_TESTS)
    add_executable(sindre_general_result_tests tests/result_tests.cpp)
    target_link_libraries(sindre_general_result_tests PRIVATE sindre::general)
    add_test(NAME sindre.general.result COMMAND sindre_general_result_tests)
    add_executable(sindre_general_tests tests/general_tests.cpp)
    target_link_libraries(sindre_general_tests PRIVATE sindre::general)
    sindre_copy_target_runtime_dlls(sindre_general_tests)
    if(WIN32 AND IS_DIRECTORY "${SINDRE_GENERAL_PACKAGE_ROOT}/bin")
        sindre_copy_runtime_dirs(sindre_general_tests
            "${SINDRE_GENERAL_PACKAGE_ROOT}/bin")
    endif()
    target_compile_definitions(sindre_general_tests PRIVATE SIMDJSON_EXCEPTIONS=0)
    target_include_directories(sindre_general_tests PRIVATE
        "${SINDRE_THIRD_GENERAL_HTTPLIB_SOURCE_DIR}")
    add_dependencies(sindre_general_tests sindre_ext_cpp_httplib)
    target_compile_definitions(sindre_general_tests PRIVATE
        CPPHTTPLIB_ZLIB_SUPPORT CPPHTTPLIB_OPENSSL_SUPPORT)
    if(NOT (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
        target_compile_definitions(sindre_general_tests PRIVATE CPPHTTPLIB_USE_NON_BLOCKING_GETADDRINFO)
    endif()
    add_test(NAME sindre.general COMMAND sindre_general_tests)
    add_executable(sindre_general_network_tests tests/network_tests.cpp)
    target_link_libraries(sindre_general_network_tests PRIVATE sindre::general)
    target_include_directories(sindre_general_network_tests PRIVATE
        "${SINDRE_THIRD_GENERAL_HTTPLIB_SOURCE_DIR}")
    add_dependencies(sindre_general_network_tests sindre_ext_cpp_httplib)
    target_compile_definitions(sindre_general_network_tests PRIVATE
        CPPHTTPLIB_ZLIB_SUPPORT CPPHTTPLIB_OPENSSL_SUPPORT)
    if(NOT (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
        target_compile_definitions(sindre_general_network_tests PRIVATE CPPHTTPLIB_USE_NON_BLOCKING_GETADDRINFO)
    endif()
    sindre_copy_target_runtime_dlls(sindre_general_network_tests)
    add_test(NAME sindre.general.network COMMAND sindre_general_network_tests)
    add_executable(sindre_general_system_tests tests/system_tests.cpp)
    target_link_libraries(sindre_general_system_tests PRIVATE sindre::general)
    sindre_copy_target_runtime_dlls(sindre_general_system_tests)
    if(WIN32 AND IS_DIRECTORY "${SINDRE_GENERAL_PACKAGE_ROOT}/bin")
        sindre_copy_runtime_dirs(sindre_general_system_tests
            "${SINDRE_GENERAL_PACKAGE_ROOT}/bin")
    endif()
    target_compile_definitions(sindre_general_system_tests PRIVATE SIMDJSON_EXCEPTIONS=0)
    add_test(NAME sindre.general.system COMMAND sindre_general_system_tests)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_tests.cpp")
        add_executable(sindre_general_cli_tests tests/cli_tests.cpp)
        target_link_libraries(sindre_general_cli_tests PRIVATE sindre::general)
        sindre_copy_target_runtime_dlls(sindre_general_cli_tests)
        if(WIN32 AND IS_DIRECTORY "${SINDRE_GENERAL_PACKAGE_ROOT}/bin")
            sindre_copy_runtime_dirs(sindre_general_cli_tests
                "${SINDRE_GENERAL_PACKAGE_ROOT}/bin")
        endif()
        add_test(NAME sindre.general.cli COMMAND sindre_general_cli_tests
            --name 中文 --count -1 --verbose sindre-cli-input.txt)
    endif()
endif()

if(TARGET sindre_general_runtime)
    install(TARGETS sindre_general_runtime EXPORT sindreTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

foreach(sindre_general_dependency IN ITEMS SPDLOG HTTPLIB ARGPARSE)
    if(DEFINED SINDRE_${sindre_general_dependency}_BUNDLED)
        set(SINDRE_${sindre_general_dependency}_BUNDLED
            "${SINDRE_${sindre_general_dependency}_BUNDLED}" PARENT_SCOPE)
    endif()
endforeach()
if(DEFINED SINDRE_CLI_USE_ARGPARSE)
    set(SINDRE_CLI_USE_ARGPARSE "${SINDRE_CLI_USE_ARGPARSE}" PARENT_SCOPE)
endif()
