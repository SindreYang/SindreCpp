set(_sindrecpp_general_include "${CMAKE_CURRENT_SOURCE_DIR}/include")
sindrecpp_add_interface_target(General "${_sindrecpp_general_include}")
target_compile_definitions(SindreCpp_General INTERFACE SINDRECPP_WITH_GENERAL=1)
include("${SINDRECPP_THIRDS_DIR}/general/Dependencies.cmake")

if(SINDRECPP_WITH_POINTER)
    sindrecpp_thirds_declare_git(cppointer
        "${SINDRECPP_THIRD_GENERAL_CS_POINTER_REPOSITORY}"
        "${SINDRECPP_THIRD_GENERAL_CS_POINTER_TAG}")
    FetchContent_GetProperties(cppointer)
    if(NOT cppointer_POPULATED)
        FetchContent_Populate(cppointer)
    endif()
    add_library(sindrecpp_cs_pointer INTERFACE)
    target_compile_features(sindrecpp_cs_pointer INTERFACE cxx_std_17)
    target_include_directories(sindrecpp_cs_pointer SYSTEM INTERFACE ${cppointer_SOURCE_DIR}/src)
    target_link_libraries(SindreCpp_General INTERFACE sindrecpp_cs_pointer)
    target_compile_definitions(SindreCpp_General INTERFACE SINDRECPP_WITH_POINTER=1)
    install(TARGETS sindrecpp_cs_pointer EXPORT SindreCppTargets)
endif()

if(SINDRECPP_WITH_STRING)
    sindrecpp_thirds_declare_git(csstring
        "${SINDRECPP_THIRD_GENERAL_CS_STRING_REPOSITORY}"
        "${SINDRECPP_THIRD_GENERAL_CS_STRING_TAG}")
    FetchContent_GetProperties(csstring)
    if(NOT csstring_POPULATED)
        FetchContent_Populate(csstring)
    endif()
    add_library(sindrecpp_cs_string STATIC ${csstring_SOURCE_DIR}/src/cs_string.cpp)
    target_compile_features(sindrecpp_cs_string PUBLIC cxx_std_17)
    target_include_directories(sindrecpp_cs_string SYSTEM PUBLIC ${csstring_SOURCE_DIR}/src)
    target_link_libraries(SindreCpp_General INTERFACE sindrecpp_cs_string)
    target_compile_definitions(SindreCpp_General INTERFACE SINDRECPP_WITH_STRING=1)
    install(TARGETS sindrecpp_cs_string EXPORT SindreCppTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(SINDRECPP_WITH_LOG)
    if(NOT TARGET spdlog::spdlog)
        find_package(spdlog CONFIG QUIET)
    endif()
    if(NOT TARGET spdlog::spdlog)
        sindrecpp_thirds_declare_git(spdlog
            "${SINDRECPP_THIRD_GENERAL_SPDLOG_REPOSITORY}"
            "${SINDRECPP_THIRD_GENERAL_SPDLOG_TAG}")
        sindrecpp_make_available(spdlog SPDLOG_BUILD_TESTS=OFF SPDLOG_BUILD_EXAMPLE=OFF)
    endif()
    target_link_libraries(SindreCpp_General INTERFACE spdlog::spdlog)
    target_compile_definitions(SindreCpp_General INTERFACE SINDRECPP_WITH_LOG=1)
endif()

if(SINDRECPP_WITH_HTTP)
    if(NOT TARGET httplib::httplib)
        find_package(httplib CONFIG QUIET)
    endif()
    if(NOT TARGET httplib::httplib)
        sindrecpp_thirds_declare_git(cpp_httplib
            "${SINDRECPP_THIRD_GENERAL_HTTPLIB_REPOSITORY}"
            "${SINDRECPP_THIRD_GENERAL_HTTPLIB_TAG}")
        sindrecpp_make_available(cpp_httplib
            HTTPLIB_TEST=OFF
            HTTPLIB_USE_OPENSSL_IF_AVAILABLE=${SINDRECPP_HTTP_OPENSSL})
    endif()
    target_link_libraries(SindreCpp_General INTERFACE httplib::httplib)
    target_compile_definitions(SindreCpp_General INTERFACE SINDRECPP_WITH_HTTP=1)
endif()

if(SINDRECPP_WITH_JSON)
    if(NOT TARGET simdjson::simdjson)
        find_package(simdjson CONFIG QUIET)
    endif()
    if(NOT TARGET simdjson::simdjson)
        sindrecpp_thirds_declare_git(simdjson
            "${SINDRECPP_THIRD_GENERAL_SIMDJSON_REPOSITORY}"
            "${SINDRECPP_THIRD_GENERAL_SIMDJSON_TAG}")
        sindrecpp_make_available(simdjson SIMDJSON_DEVELOPER_MODE=OFF)
    endif()
    target_link_libraries(SindreCpp_General INTERFACE simdjson::simdjson)
    target_compile_definitions(SindreCpp_General INTERFACE SINDRECPP_WITH_JSON=1)
    # FetchContent creates a local `simdjson` target. Because General is an
    # installable interface target, CMake also needs that concrete dependency
    # in the same export set; otherwise standalone JSON examples fail during
    # the generate step with an incomplete export graph.
    if(TARGET simdjson AND DEFINED simdjson_SOURCE_DIR)
        install(TARGETS simdjson EXPORT SindreCppTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()

if(SINDRECPP_WITH_CLI)
    if(NOT TARGET argparse::argparse)
        find_package(argparse CONFIG QUIET)
    endif()
    if(NOT TARGET argparse::argparse)
        sindrecpp_thirds_declare_git(argparse
            "${SINDRECPP_THIRD_GENERAL_ARGPARSE_REPOSITORY}"
            "${SINDRECPP_THIRD_GENERAL_ARGPARSE_TAG}")
        sindrecpp_make_available(argparse ARGPARSE_BUILD_TESTS=OFF)
    endif()
    target_link_libraries(SindreCpp_General INTERFACE argparse::argparse)
    target_compile_definitions(SindreCpp_General INTERFACE SINDRECPP_WITH_CLI=1)
endif()

if(SINDRECPP_WITH_RE2)
    find_package(re2 CONFIG QUIET)
    if(TARGET re2::re2)
        target_link_libraries(SindreCpp_General INTERFACE re2::re2)
    elseif(TARGET re2)
        target_link_libraries(SindreCpp_General INTERFACE re2)
    else()
        message(FATAL_ERROR "SINDRECPP_WITH_RE2 requires a re2 CMake package")
    endif()
    target_compile_definitions(SindreCpp_General INTERFACE SINDRECPP_WITH_RE2=1)
endif()

if(SINDRECPP_WITH_CRASHPAD)
    if(NOT SINDRECPP_CRASHPAD_TARGET OR NOT TARGET ${SINDRECPP_CRASHPAD_TARGET})
        message(FATAL_ERROR "SINDRECPP_WITH_CRASHPAD requires SINDRECPP_CRASHPAD_TARGET")
    endif()
    target_link_libraries(SindreCpp_General INTERFACE ${SINDRECPP_CRASHPAD_TARGET})
    target_compile_definitions(SindreCpp_General INTERFACE
        SINDRECPP_WITH_CRASHPAD=1 SINDRECPP_CRASHPAD_READY=1)
endif()

if(SINDRECPP_WITH_ZLIB)
    find_package(ZLIB REQUIRED)
    target_link_libraries(SindreCpp_General INTERFACE ZLIB::ZLIB)
    target_compile_definitions(SindreCpp_General INTERFACE SINDRECPP_WITH_ZLIB=1)
endif()

if(SINDRECPP_WITH_UTILS_PY)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
    if(NOT TARGET pybind11::embed)
        find_package(pybind11 CONFIG QUIET)
    endif()
    if(NOT TARGET pybind11::embed)
        sindrecpp_thirds_declare_git(pybind11
            "${SINDRECPP_THIRD_GENERAL_PYBIND11_REPOSITORY}"
            "${SINDRECPP_THIRD_GENERAL_PYBIND11_TAG}")
        sindrecpp_make_available(pybind11 PYBIND11_TEST=OFF)
    endif()
    target_link_libraries(SindreCpp_General INTERFACE pybind11::embed)
    target_compile_definitions(SindreCpp_General INTERFACE SINDRECPP_WITH_UTILS_PY=1)
endif()

if(SINDRECPP_BUILD_TESTS)
    add_executable(sindrecpp_general_tests tests/general_tests.cpp)
    target_link_libraries(sindrecpp_general_tests PRIVATE SindreCpp::General)
    sindrecpp_copy_target_runtime_dlls(sindrecpp_general_tests)
    if(SINDRECPP_WITH_JSON)
        target_compile_definitions(sindrecpp_general_tests PRIVATE SIMDJSON_EXCEPTIONS=0)
    endif()
    add_test(NAME sindrecpp.general COMMAND sindrecpp_general_tests)
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
