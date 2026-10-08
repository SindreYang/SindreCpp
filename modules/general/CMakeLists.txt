sindre_add_interface_target(general "${SINDRE_SOURCE_DIR}/include")
target_include_directories(sindre_general INTERFACE
    $<INSTALL_INTERFACE:include>)
target_include_directories(sindre_general SYSTEM BEFORE INTERFACE
    $<BUILD_INTERFACE:${SINDRE_GENERAL_PACKAGE_ROOT}/include>)
target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_GENERAL=1)

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
    target_include_directories(sindre_general_runtime PUBLIC
        $<BUILD_INTERFACE:${SINDRE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
    target_include_directories(sindre_general_runtime SYSTEM BEFORE PRIVATE
        "${SINDRE_GENERAL_PACKAGE_ROOT}/include"
        "${SINDRE_THIRD_GENERAL_SOURCE_ROOT}/simdjson/v4.6.11/simdjson-4.6.11/include"
        "${SINDRE_THIRD_GENERAL_SOURCE_ROOT}/spdlog/v1.17.0/spdlog-1.17.0/include"
        "${SINDRE_THIRD_GENERAL_SOURCE_ROOT}/cpp_httplib/v0.56.0/cpp-httplib-0.56.0"
        "${SINDRE_THIRD_GENERAL_SOURCE_ROOT}/cs_string/string-1.4.1/cs_string-string-1.4.1/src")
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
include("${SINDRE_THIRDS_DIR}/general/Dependencies.cmake")

# CsString is the fixed C++17 backend of General's public string abstraction.
# It is intentionally not an optional integration: every General consumer gets
# the same encoding policy and the same compiled implementation.
sindre_thirds_declare_local(csstring
    "${SINDRE_THIRD_GENERAL_SOURCE_ROOT}/cs_string/string-1.4.1/cs_string-string-1.4.1")
FetchContent_GetProperties(csstring)
if(NOT csstring_POPULATED)
    FetchContent_Populate(csstring)
endif()
add_library(sindre_cs_string STATIC
    "${csstring_SOURCE_DIR}/src/cs_string.cpp")
target_compile_features(sindre_cs_string PUBLIC cxx_std_17)
target_include_directories(sindre_cs_string SYSTEM PUBLIC
    $<BUILD_INTERFACE:${csstring_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include/cs_string>)
# The upstream export macro otherwise marks static-library symbols as
# __declspec(dllimport) for consumers on Windows.
target_compile_definitions(sindre_cs_string PUBLIC BUILDING_LIB_CS_STRING)
target_link_libraries(sindre_general INTERFACE sindre_cs_string)
if(TARGET sindre_general_runtime)
    target_link_libraries(sindre_general_runtime PRIVATE sindre_cs_string)
endif()
install(TARGETS sindre_cs_string EXPORT sindrecppTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY "${csstring_SOURCE_DIR}/src/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/cs_string"
    FILES_MATCHING PATTERN "*.h")

    # General always owns the pinned implementation. Do not accept a parent
    # project's spdlog target: that would silently change the ABI and defeat
    # the fixed dependency profile.
    sindre_thirds_declare_local(spdlog
        "${SINDRE_THIRD_GENERAL_SOURCE_ROOT}/spdlog/v1.17.0/spdlog-1.17.0")
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "Build spdlog as a static library" FORCE)
    set(SPDLOG_INSTALL ON CACHE BOOL "Install bundled spdlog with sindrecpp" FORCE)
    sindre_make_available(spdlog
        SPDLOG_BUILD_SHARED=OFF
        SPDLOG_BUILD_TESTS=OFF
        SPDLOG_BUILD_EXAMPLE=OFF
        SPDLOG_INSTALL=ON)
    set(SINDRE_SPDLOG_BUNDLED ON)
    target_link_libraries(sindre_general INTERFACE spdlog::spdlog)
    target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_LOG=1)
    if(TARGET sindre_general_runtime)
        target_link_libraries(sindre_general_runtime PRIVATE spdlog::spdlog)
        target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_WITH_LOG=1)
    endif()
    if(TARGET spdlog AND DEFINED spdlog_SOURCE_DIR)
        install(TARGETS spdlog EXPORT sindrecppTargets)
    endif()

find_package(OpenSSL 3.3.0 EXACT CONFIG REQUIRED COMPONENTS SSL Crypto
    PATHS "${SINDRE_GENERAL_PACKAGE_ROOT}" NO_DEFAULT_PATH)

    set(HTTPLIB_INSTALL OFF CACHE BOOL "Disable installation of cpp-httplib from General" FORCE)
    sindre_thirds_declare_local(cpp_httplib
        "${SINDRE_THIRD_GENERAL_SOURCE_ROOT}/cpp_httplib/v0.56.0/cpp-httplib-0.56.0")
    set(_sindre_httplib_nonblocking ON)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(_sindre_httplib_nonblocking OFF)
    endif()
    sindre_make_available(cpp_httplib
        HTTPLIB_TEST=OFF
        HTTPLIB_INSTALL=OFF
        HTTPLIB_USE_OPENSSL_IF_AVAILABLE=ON
        HTTPLIB_USE_NON_BLOCKING_GETADDRINFO=${_sindre_httplib_nonblocking})
    set(SINDRE_HTTPLIB_BUNDLED ON)
    add_library(sindre_general_httplib INTERFACE)
    set_target_properties(sindre_general_httplib PROPERTIES EXPORT_NAME general_httplib)
    target_include_directories(sindre_general_httplib INTERFACE
        $<BUILD_INTERFACE:${cpp_httplib_SOURCE_DIR}>)
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
    install(TARGETS sindre_general_httplib EXPORT sindrecppTargets)
    target_link_libraries(sindre_general INTERFACE OpenSSL::SSL OpenSSL::Crypto)
    if(TARGET sindre_general_runtime)
        target_link_libraries(sindre_general_runtime PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
    target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_HTTP=1)

    # General always uses the pinned upstream release. This keeps the public
    # JSON ABI and headers identical across MSVC, clang-cl and GCC instead of
    # silently selecting a different system/vcpkg simdjson version.
    sindre_thirds_declare_local(simdjson
        "${SINDRE_THIRD_GENERAL_SOURCE_ROOT}/simdjson/v4.6.11/simdjson-4.6.11")
    sindre_make_available(simdjson SIMDJSON_DEVELOPER_MODE=OFF)
    add_library(sindre_general_simdjson INTERFACE)
    target_link_libraries(sindre_general_simdjson INTERFACE
        $<BUILD_INTERFACE:simdjson>
        $<INSTALL_INTERFACE:sindre::simdjson>)
    target_link_libraries(sindre_general INTERFACE sindre_general_simdjson)
    if(TARGET sindre_general_runtime)
        target_link_libraries(sindre_general_runtime PRIVATE sindre_general_simdjson)
        target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_WITH_JSON=1 SIMDJSON_EXCEPTIONS=0)
    endif()
    set_target_properties(simdjson PROPERTIES EXPORT_NAME simdjson)
    target_include_directories(simdjson PUBLIC
        $<BUILD_INTERFACE:${simdjson_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/simdjson>)
    target_include_directories(sindre_general SYSTEM BEFORE INTERFACE
        $<BUILD_INTERFACE:${simdjson_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/simdjson>)
    install(TARGETS sindre_general_simdjson simdjson EXPORT sindrecppTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY "${simdjson_SOURCE_DIR}/include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/simdjson")
    target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_JSON=1)
    # General's JSON Result boundary is non-throwing; keep the simdjson
    # formatter/DOM API in the same no-exception mode for all consumers.
    target_compile_definitions(sindre_general INTERFACE SIMDJSON_EXCEPTIONS=0)
    set(SINDRE_SIMDJSON_BUNDLED ON PARENT_SCOPE)

    set(SINDRE_CLI_USE_ARGPARSE OFF)
    if(NOT SINDRE_NO_EXCEPTIONS)
        set(SINDRE_CLI_USE_ARGPARSE ON)
        sindre_thirds_declare_local(argparse
            "${SINDRE_THIRD_GENERAL_SOURCE_ROOT}/argparse/v3.2/argparse-3.2")
        sindre_make_available(argparse ARGPARSE_BUILD_TESTS=OFF)
        set(SINDRE_ARGPARSE_BUNDLED ON)
        # argparse is an implementation detail of the compiled General
        # runtime. The public CLI header contains only sindre-owned types.
        if(TARGET sindre_general_runtime)
            target_include_directories(sindre_general_runtime PRIVATE
                "${SINDRE_THIRDS_DIR}/general/sources/argparse/v3.2/argparse-3.2/include")
            target_link_libraries(sindre_general_runtime PRIVATE argparse::argparse)
        endif()
    endif()
    target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_CLI=1)
    if(TARGET sindre_general_runtime)
        target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_WITH_CLI=1)
    endif()
    if(SINDRE_CLI_USE_ARGPARSE AND TARGET argparse AND DEFINED argparse_SOURCE_DIR)
        target_include_directories(argparse INTERFACE
            $<BUILD_INTERFACE:${argparse_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)
        install(TARGETS argparse EXPORT sindrecppTargets)
        install(FILES "${argparse_SOURCE_DIR}/include/argparse/argparse.hpp"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/argparse")
    endif()

    find_package(re2 11.0.0 EXACT CONFIG REQUIRED
        PATHS "${SINDRE_GENERAL_PACKAGE_ROOT}" NO_DEFAULT_PATH)
    if(TARGET re2::re2)
        target_link_libraries(sindre_general INTERFACE re2::re2)
        if(TARGET sindre_general_runtime)
            target_link_libraries(sindre_general_runtime PRIVATE re2::re2)
        endif()
    elseif(TARGET re2)
        target_link_libraries(sindre_general INTERFACE re2)
        if(TARGET sindre_general_runtime)
            target_link_libraries(sindre_general_runtime PRIVATE re2)
        endif()
    else()
        message(FATAL_ERROR "Fixed General dependency profile has no re2 target")
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
    find_package(crashpad CONFIG REQUIRED
        PATHS "${SINDRE_GENERAL_PACKAGE_ROOT}" NO_DEFAULT_PATH)
    if(TARGET crashpad)
        set(SINDRE_CRASHPAD_FIXED_TARGET crashpad)
    elseif(TARGET crashpad::crashpad)
        set(SINDRE_CRASHPAD_FIXED_TARGET crashpad::crashpad)
    else()
        message(FATAL_ERROR "Fixed General dependency profile has no Crashpad target")
    endif()
    # Keep the external package behind an exported wrapper. Exporting the
    # vcpkg interface target itself would bake absolute build-machine library
    # paths into an installed sindrecpp package.
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
    install(TARGETS sindre_general_crashpad EXPORT sindrecppTargets)

    find_package(ZLIB REQUIRED MODULE)
    if(NOT ZLIB_VERSION_STRING STREQUAL "1.3.1")
        message(FATAL_ERROR
            "Fixed General dependency requires zlib 1.3.1, found ${ZLIB_VERSION_STRING}")
    endif()
    target_link_libraries(sindre_general INTERFACE ZLIB::ZLIB)
    target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_ZLIB=1)
    if(TARGET sindre_general_runtime)
        target_link_libraries(sindre_general_runtime PRIVATE ZLIB::ZLIB)
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
        "${SINDRE_THIRD_GENERAL_SOURCE_ROOT}/cpp_httplib/v0.56.0/cpp-httplib-0.56.0")
    target_compile_definitions(sindre_general_tests PRIVATE
        CPPHTTPLIB_ZLIB_SUPPORT CPPHTTPLIB_OPENSSL_SUPPORT)
    if(NOT (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
        target_compile_definitions(sindre_general_tests PRIVATE CPPHTTPLIB_USE_NON_BLOCKING_GETADDRINFO)
    endif()
    add_test(NAME sindre.general COMMAND sindre_general_tests)
    add_executable(sindre_general_network_tests tests/network_tests.cpp)
    target_link_libraries(sindre_general_network_tests PRIVATE sindre::general)
    target_include_directories(sindre_general_network_tests PRIVATE
        "${SINDRE_THIRD_GENERAL_SOURCE_ROOT}/cpp_httplib/v0.56.0/cpp-httplib-0.56.0")
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
    install(TARGETS sindre_general_runtime EXPORT sindrecppTargets
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
