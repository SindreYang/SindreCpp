sindre_add_interface_target(general "${SINDRE_SOURCE_DIR}/include")
target_include_directories(sindre_general INTERFACE
    $<INSTALL_INTERFACE:include>)
target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_GENERAL=1)
target_link_libraries(sindre_general INTERFACE Eigen3::Eigen)
target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_EIGEN=1)

if(SINDRE_GENERAL_BUILD_LIBRARY)
    set(SINDRE_GENERAL_SOURCES
        src/core.cpp
        src/system.cpp
        src/runtime.cpp
        src/cli.cpp
        src/string.cpp
        src/network.cpp
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
    target_include_directories(sindre_general_runtime PRIVATE
        "${SINDRE_THIRDS_DIR}/general/sources/simdjson/v4.6.11/simdjson-4.6.11/include"
        "${SINDRE_THIRDS_DIR}/general/sources/argparse/v3.2/argparse-3.2/include"
        "${SINDRE_THIRDS_DIR}/general/sources/spdlog/v1.17.0/spdlog-1.17.0/include"
        "${SINDRE_THIRDS_DIR}/general/sources/cpp_httplib/v0.56.0/cpp-httplib-0.56.0"
        "${SINDRE_THIRDS_DIR}/general/sources/cs_string/string-1.4.1/cs_string-string-1.4.1/src")
    target_link_libraries(sindre_general_runtime PUBLIC sindre_base)
    target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_GENERAL_BUILDING_LIBRARY=1)
    foreach(feature IN ITEMS LOG HTTP JSON CLI RE2 CRASHPAD ZLIB)
        if(SINDRE_WITH_${feature})
            target_compile_definitions(sindre_general_runtime PRIVATE
                SINDRE_WITH_${feature}=1)
        endif()
    endforeach()
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
    "${SINDRE_THIRDS_DIR}/general/sources/cs_string/string-1.4.1/cs_string-string-1.4.1")
FetchContent_GetProperties(csstring)
if(NOT csstring_POPULATED)
    FetchContent_Populate(csstring)
endif()
add_library(sindre_cs_string STATIC
    "${csstring_SOURCE_DIR}/src/cs_string.cpp")
target_compile_features(sindre_cs_string PUBLIC cxx_std_17)
target_include_directories(sindre_cs_string PUBLIC
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

if(SINDRE_WITH_LOG)
    set(SINDRE_SPDLOG_BUNDLED OFF)
    if(NOT TARGET spdlog::spdlog)
        sindre_thirds_declare_local(spdlog
            "${SINDRE_THIRDS_DIR}/general/sources/spdlog/v1.17.0/spdlog-1.17.0")
        set(SPDLOG_INSTALL ON CACHE BOOL "Install bundled spdlog with sindrecpp" FORCE)
        sindre_make_available(spdlog
            SPDLOG_BUILD_TESTS=OFF
            SPDLOG_BUILD_EXAMPLE=OFF
            SPDLOG_INSTALL=ON)
        set(SINDRE_SPDLOG_BUNDLED ON)
    endif()
    if(TARGET spdlog AND DEFINED spdlog_SOURCE_DIR)
        set(SINDRE_SPDLOG_BUNDLED ON)
    endif()
    target_link_libraries(sindre_general INTERFACE spdlog::spdlog)
    target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_LOG=1)
    if(TARGET sindre_general_runtime)
        target_link_libraries(sindre_general_runtime PRIVATE spdlog::spdlog)
        target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_WITH_LOG=1)
    endif()
    if(TARGET spdlog AND DEFINED spdlog_SOURCE_DIR)
        install(TARGETS spdlog EXPORT sindrecppTargets)
    endif()
endif()

if(SINDRE_WITH_HTTP)
    set(SINDRE_HTTPLIB_BUNDLED OFF)
    if(SINDRE_HTTP_OPENSSL)
        find_package(OpenSSL 3.3.0 EXACT CONFIG REQUIRED COMPONENTS SSL Crypto
            PATHS "${SINDRE_GENERAL_PACKAGE_ROOT}" NO_DEFAULT_PATH)
    endif()
    if(NOT TARGET httplib::httplib)
        sindre_thirds_declare_local(cpp_httplib
            "${SINDRE_THIRDS_DIR}/general/sources/cpp_httplib/v0.56.0/cpp-httplib-0.56.0")
        sindre_make_available(cpp_httplib
            HTTPLIB_TEST=OFF
            HTTPLIB_USE_OPENSSL_IF_AVAILABLE=${SINDRE_HTTP_OPENSSL})
        set(SINDRE_HTTPLIB_BUNDLED ON)
    endif()
    if(TARGET httplib AND DEFINED cpp_httplib_SOURCE_DIR)
        set(SINDRE_HTTPLIB_BUNDLED ON)
    endif()
    if(SINDRE_HTTPLIB_BUNDLED)
        add_library(sindre_general_httplib INTERFACE)
        set_target_properties(sindre_general_httplib PROPERTIES EXPORT_NAME general_httplib)
        target_include_directories(sindre_general_httplib INTERFACE
            $<BUILD_INTERFACE:${cpp_httplib_SOURCE_DIR}>
            $<INSTALL_INTERFACE:include>)
        target_compile_definitions(sindre_general_httplib INTERFACE
            CPPHTTPLIB_USE_NON_BLOCKING_GETADDRINFO
            CPPHTTPLIB_ZLIB_SUPPORT)
        if(SINDRE_HTTP_OPENSSL)
            target_compile_definitions(sindre_general_httplib INTERFACE
                CPPHTTPLIB_OPENSSL_SUPPORT)
        endif()
        target_link_libraries(sindre_general_httplib INTERFACE
            $<BUILD_INTERFACE:httplib::httplib>)
        target_link_libraries(sindre_general INTERFACE sindre_general_httplib)
        if(TARGET sindre_general_runtime)
            target_link_libraries(sindre_general_runtime PRIVATE sindre_general_httplib)
            target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_WITH_HTTP=1)
        endif()
        if(DEFINED cpp_httplib_SOURCE_DIR)
            install(FILES "${cpp_httplib_SOURCE_DIR}/httplib.h"
                DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
        endif()
        install(TARGETS sindre_general_httplib EXPORT sindrecppTargets)
    else()
        target_link_libraries(sindre_general INTERFACE httplib::httplib)
        if(TARGET sindre_general_runtime)
            target_link_libraries(sindre_general_runtime PRIVATE httplib::httplib)
            target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_WITH_HTTP=1)
        endif()
    endif()
    if(SINDRE_HTTP_OPENSSL)
        target_link_libraries(sindre_general INTERFACE OpenSSL::SSL OpenSSL::Crypto)
        if(TARGET sindre_general_runtime)
            target_link_libraries(sindre_general_runtime PRIVATE OpenSSL::SSL OpenSSL::Crypto)
        endif()
    endif()
    target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_HTTP=1)
endif()

if(SINDRE_WITH_JSON)
    # General always uses the pinned upstream release. This keeps the public
    # JSON ABI and headers identical across MSVC, clang-cl and GCC instead of
    # silently selecting a different system/vcpkg simdjson version.
    set(SINDRE_SIMDJSON_BUNDLED OFF)
    if(TARGET simdjson)
        set(SINDRE_SIMDJSON_BUNDLED ON)
    endif()
    if(NOT TARGET simdjson::simdjson)
        sindre_thirds_declare_local(simdjson
            "${SINDRE_THIRDS_DIR}/general/sources/simdjson/v4.6.11/simdjson-4.6.11")
        sindre_make_available(simdjson SIMDJSON_DEVELOPER_MODE=OFF)
        set(SINDRE_SIMDJSON_BUNDLED ON)
    endif()
    if(TARGET simdjson AND DEFINED simdjson_SOURCE_DIR)
        set(SINDRE_SIMDJSON_BUNDLED ON)
    endif()
    if(TARGET simdjson AND NOT TARGET simdjson::simdjson)
        add_library(simdjson::simdjson ALIAS simdjson)
    endif()
    if(SINDRE_SIMDJSON_BUNDLED)
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
        target_include_directories(sindre_general BEFORE INTERFACE
            $<BUILD_INTERFACE:${simdjson_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include/simdjson>)
        install(TARGETS sindre_general_simdjson simdjson EXPORT sindrecppTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        install(DIRECTORY "${simdjson_SOURCE_DIR}/include/"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/simdjson")
    else()
        target_link_libraries(sindre_general INTERFACE simdjson::simdjson)
        if(TARGET sindre_general_runtime)
            target_link_libraries(sindre_general_runtime PRIVATE simdjson::simdjson)
            target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_WITH_JSON=1 SIMDJSON_EXCEPTIONS=0)
        endif()
    endif()
    target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_JSON=1)
    # General's JSON Result boundary is non-throwing; keep the simdjson
    # formatter/DOM API in the same no-exception mode for all consumers.
    target_compile_definitions(sindre_general INTERFACE SIMDJSON_EXCEPTIONS=0)
    # FetchContent creates a local `simdjson` target. Because General is an
    # installable interface target, CMake also needs that concrete dependency
    # in the same export set; otherwise standalone JSON examples fail during
    # the generate step with an incomplete export graph.
    if(TARGET simdjson AND DEFINED simdjson_SOURCE_DIR AND NOT SINDRE_SIMDJSON_BUNDLED)
        install(TARGETS simdjson EXPORT sindrecppTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
    set(SINDRE_SIMDJSON_BUNDLED ${SINDRE_SIMDJSON_BUNDLED} PARENT_SCOPE)
endif()

if(SINDRE_WITH_CLI)
    set(SINDRE_ARGPARSE_BUNDLED OFF)
    if(NOT TARGET argparse::argparse)
        sindre_thirds_declare_local(argparse
            "${SINDRE_THIRDS_DIR}/general/sources/argparse/v3.2/argparse-3.2")
        sindre_make_available(argparse ARGPARSE_BUILD_TESTS=OFF)
        set(SINDRE_ARGPARSE_BUNDLED ON)
    endif()
    if(TARGET argparse AND DEFINED argparse_SOURCE_DIR)
        set(SINDRE_ARGPARSE_BUNDLED ON)
    endif()
    target_link_libraries(sindre_general INTERFACE argparse::argparse)
    target_compile_definitions(sindre_general INTERFACE SINDRE_WITH_CLI=1)
    if(TARGET sindre_general_runtime)
        target_link_libraries(sindre_general_runtime PRIVATE argparse::argparse)
        target_compile_definitions(sindre_general_runtime PRIVATE SINDRE_WITH_CLI=1)
    endif()
    if(TARGET argparse AND DEFINED argparse_SOURCE_DIR)
        target_include_directories(argparse INTERFACE
            $<BUILD_INTERFACE:${argparse_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)
        install(TARGETS argparse EXPORT sindrecppTargets)
        install(FILES "${argparse_SOURCE_DIR}/include/argparse/argparse.hpp"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/argparse")
    endif()
endif()

if(SINDRE_WITH_RE2)
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
endif()

if(SINDRE_WITH_CRASHPAD)
    set(_sindre_crashpad_metadata
        "${SINDRE_GENERAL_PACKAGE_ROOT}/share/crashpad/vcpkg.spdx.json")
    if(NOT EXISTS "${_sindre_crashpad_metadata}")
        message(FATAL_ERROR "Fixed Crashpad package metadata is missing")
    endif()
    file(READ "${_sindre_crashpad_metadata}" _sindre_crashpad_metadata_text)
    string(FIND "${_sindre_crashpad_metadata_text}"
        "crashpad:x64-windows@${SINDRE_THIRD_GENERAL_CRASHPAD_VERSION}"
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
endif()

if(SINDRE_WITH_ZLIB)
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
endif()

if(SINDRE_BUILD_TESTS)
    add_executable(sindre_general_tests tests/general_tests.cpp)
    target_link_libraries(sindre_general_tests PRIVATE sindre::general)
    sindre_copy_target_runtime_dlls(sindre_general_tests)
    if(WIN32 AND IS_DIRECTORY "${SINDRE_GENERAL_PACKAGE_ROOT}/bin")
        sindre_copy_runtime_dirs(sindre_general_tests
            "${SINDRE_GENERAL_PACKAGE_ROOT}/bin")
    endif()
    if(SINDRE_WITH_JSON)
        target_compile_definitions(sindre_general_tests PRIVATE SIMDJSON_EXCEPTIONS=0)
    endif()
    add_test(NAME sindre.general COMMAND sindre_general_tests)
endif()

if(SINDRE_BUILD_BENCHMARKS)
    add_executable(sindre_general_eigen_benchmark tests/eigen_benchmark.cpp)
    target_link_libraries(sindre_general_eigen_benchmark PRIVATE sindre::general)
    if(WIN32 AND IS_DIRECTORY "${SINDRE_GENERAL_PACKAGE_ROOT}/bin")
        sindre_copy_runtime_dirs(sindre_general_eigen_benchmark
            "${SINDRE_GENERAL_PACKAGE_ROOT}/bin")
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
