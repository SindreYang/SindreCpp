include_guard(GLOBAL)

include(FetchContent)
include(CompilerDefaults)
find_package(Threads REQUIRED)

# Keep the helper-script location available when an individual example embeds
# the repository with add_subdirectory(). Normal directory variables can be
# shadowed by the standalone example's scope, while this cache entry remains
# stable for post-build commands generated from that scope.
set(SINDRECPP_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL
    "SindreCpp CMake helper directory")

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

add_library(sindrecpp_base INTERFACE)
target_compile_features(sindrecpp_base INTERFACE cxx_std_17)
sindrecpp_apply_compiler_defaults(sindrecpp_base)
if(WIN32)
    # Keep Windows headers from pulling in the legacy winsock.h before
    # networking dependencies include winsock2.h.
    target_compile_definitions(sindrecpp_base INTERFACE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
target_include_directories(sindrecpp_base INTERFACE
    $<BUILD_INTERFACE:${SINDRECPP_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(sindrecpp_base INTERFACE Threads::Threads)

if(SINDRECPP_NO_EXCEPTIONS)
    target_compile_definitions(sindrecpp_base INTERFACE
        SINDRECPP_NO_EXCEPTIONS=1
        SPDLOG_NO_EXCEPTIONS=1
        CPPHTTPLIB_NO_EXCEPTIONS=1
        SIMDJSON_EXCEPTIONS=0)
    if(MSVC)
        target_compile_options(sindrecpp_base INTERFACE /EHs-c-)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(sindrecpp_base INTERFACE -fno-exceptions)
    endif()
endif()

function(sindrecpp_add_interface_target target module_include)
    add_library(SindreCpp_${target} INTERFACE)
    add_library(SindreCpp::${target} ALIAS SindreCpp_${target})
    target_link_libraries(SindreCpp_${target} INTERFACE sindrecpp_base)
    target_include_directories(SindreCpp_${target} INTERFACE
        $<BUILD_INTERFACE:${module_include}>
        $<INSTALL_INTERFACE:include>)
    target_compile_features(SindreCpp_${target} INTERFACE cxx_std_17)
    set_target_properties(SindreCpp_${target} PROPERTIES EXPORT_NAME ${target})
endfunction()

function(sindrecpp_make_available dependency)
    foreach(default IN LISTS ARGN)
        string(FIND "${default}" "=" separator)
        if(separator LESS 1)
            message(FATAL_ERROR "Invalid dependency default '${default}'; expected NAME=VALUE")
        endif()
        string(SUBSTRING "${default}" 0 ${separator} variable)
        math(EXPR value_start "${separator} + 1")
        string(SUBSTRING "${default}" ${value_start} -1 value)
        if(NOT DEFINED ${variable})
            set(${variable} "${value}")
        endif()
    endforeach()
    FetchContent_MakeAvailable(${dependency})
endfunction()

function(sindrecpp_add_test target source)
    if(NOT SINDRECPP_BUILD_TESTS)
        return()
    endif()
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE ${ARGN})
    add_test(NAME ${target} COMMAND ${target})
endfunction()

function(sindrecpp_copy_runtime_dirs target)
    if(NOT WIN32 OR NOT TARGET ${target})
        return()
    endif()

    foreach(runtime_dir IN LISTS ARGN)
        if(NOT IS_DIRECTORY "${runtime_dir}")
            continue()
        endif()
        file(GLOB runtime_files CONFIGURE_DEPENDS "${runtime_dir}/*.dll")
        foreach(runtime_file IN LISTS runtime_files)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${runtime_file}" "$<TARGET_FILE_DIR:${target}>"
                VERBATIM)
        endforeach()
    endforeach()
endfunction()

function(sindrecpp_copy_target_runtime_dlls target)
    if(NOT WIN32 OR NOT TARGET ${target} OR CMAKE_VERSION VERSION_LESS 3.21)
        return()
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DSINDRECPP_RUNTIME_DEST=$<TARGET_FILE_DIR:${target}>"
            "-DSINDRECPP_RUNTIME_FILES=$<TARGET_RUNTIME_DLLS:${target}>"
            -P "${SINDRECPP_CMAKE_DIR}/CopyRuntime.cmake"
        COMMAND_EXPAND_LISTS
        VERBATIM)
endfunction()
