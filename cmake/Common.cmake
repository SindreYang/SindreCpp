include_guard(GLOBAL)

include(FetchContent)
include(CompilerDefaults)
find_package(Threads REQUIRED)
include("${SINDRE_THIRDS_DIR}/general/Dependencies.cmake")

# Keep the helper-script location available when an individual example embeds
# the repository with add_subdirectory(). Normal directory variables can be
# shadowed by the standalone example's scope, while this cache entry remains
# stable for post-build commands generated from that scope.
set(SINDRE_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL
    "sindre CMake helper directory")

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

add_library(sindre_base INTERFACE)
target_compile_features(sindre_base INTERFACE cxx_std_17)
sindre_apply_compiler_defaults(sindre_base)
if(WIN32)
    # Keep Windows headers from pulling in the legacy winsock.h before
    # networking dependencies include winsock2.h.
    target_compile_definitions(sindre_base INTERFACE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
target_include_directories(sindre_base INTERFACE
    $<BUILD_INTERFACE:${SINDRE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(sindre_base INTERFACE Threads::Threads)
set(SINDRE_EIGEN_BLAS_ENABLED OFF)

if(SINDRE_WITH_EIGEN AND (SINDRE_WITH_GENERAL OR SINDRE_WITH_UTILS_2D OR
                          SINDRE_WITH_UTILS_3D OR SINDRE_WITH_UTILS_PY))
    if(NOT TARGET Eigen3::Eigen)
        find_package(Eigen3 3.4.1 CONFIG QUIET NO_MODULE)
    endif()
    if(NOT TARGET Eigen3::Eigen)
        sindre_thirds_declare_local(eigen
            "${SINDRE_THIRDS_DIR}/general/sources/eigen/3.4.1/eigen-3.4.1")
        set(EIGEN_BUILD_DOC OFF)
        set(EIGEN_BUILD_PKGCONFIG OFF)
        FetchContent_MakeAvailable(eigen)
    endif()
    if(NOT TARGET Eigen3::Eigen)
        message(FATAL_ERROR "SINDRE_WITH_EIGEN requires an Eigen3::Eigen target")
    endif()
    target_link_libraries(sindre_base INTERFACE Eigen3::Eigen)

    string(TOUPPER "${SINDRE_EIGEN_BLAS_BACKEND}" _sindre_eigen_blas_backend)
    if(NOT _sindre_eigen_blas_backend MATCHES "^(AUTO|MKL|OPENBLAS|BLAS|EIGEN)$")
        message(FATAL_ERROR
            "SINDRE_EIGEN_BLAS_BACKEND must be AUTO, MKL, OPENBLAS, BLAS, or EIGEN")
    endif()
    set(_sindre_eigen_blas_found OFF)
    if(NOT _sindre_eigen_blas_backend STREQUAL "EIGEN")
        if(_sindre_eigen_blas_backend STREQUAL "MKL")
            set(BLA_VENDOR Intel10_64lp)
            find_package(BLAS REQUIRED)
        elseif(_sindre_eigen_blas_backend STREQUAL "OPENBLAS")
            set(BLA_VENDOR OpenBLAS)
            # Official Windows archives ship an OpenBLASConfig.cmake file,
            # but older releases expose variables instead of a CMake target.
            # Prefer its MSVC import library when the package is discoverable,
            # then let FindBLAS create the standard BLAS::BLAS target.
            find_package(${SINDRE_THIRD_GENERAL_OPENBLAS_CONFIG_PACKAGE} CONFIG QUIET)
            if(DEFINED OpenBLAS_INCLUDE_DIRS AND NOT DEFINED BLAS_LIBRARIES)
                get_filename_component(_sindre_openblas_root
                    "${OpenBLAS_INCLUDE_DIRS}" DIRECTORY)
                if(EXISTS "${_sindre_openblas_root}/lib/${SINDRE_THIRD_GENERAL_OPENBLAS_IMPORT_LIBRARY}")
                    set(BLAS_LIBRARIES
                        "${_sindre_openblas_root}/lib/${SINDRE_THIRD_GENERAL_OPENBLAS_IMPORT_LIBRARY}"
                        CACHE STRING "OpenBLAS import library" FORCE)
                endif()
            endif()
            find_package(BLAS REQUIRED)
        elseif(_sindre_eigen_blas_backend STREQUAL "BLAS")
            find_package(BLAS REQUIRED)
        else()
            # AUTO lets CMake prefer an installed optimized vendor while still
            # allowing a portable Eigen-only build when no BLAS is present.
            find_package(BLAS QUIET)
        endif()
        if(TARGET BLAS::BLAS)
            set(_sindre_eigen_blas_found ON)
            set(SINDRE_EIGEN_BLAS_ENABLED ON)
            target_link_libraries(sindre_base INTERFACE BLAS::BLAS)
            target_compile_definitions(sindre_base INTERFACE EIGEN_USE_BLAS=1)
        elseif(NOT _sindre_eigen_blas_backend STREQUAL "AUTO")
            message(FATAL_ERROR "Requested Eigen BLAS backend was not found")
        endif()
    endif()
    if(SINDRE_EIGEN_NATIVE_ARCH)
        if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            # clang-cl accepts the clang tuning option through /clang:. This
            # enables Eigen's native SIMD detection, including AVX2 and FMA
            # on supported x86 CPUs.
            target_compile_options(sindre_base INTERFACE /clang:-march=native)
        elseif(MSVC)
            target_compile_options(sindre_base INTERFACE /arch:AVX2)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|AppleClang")
            target_compile_options(sindre_base INTERFACE -march=native)
        endif()
    endif()
    if(_sindre_eigen_blas_found)
        message(STATUS "Sindre Eigen backend: ${_sindre_eigen_blas_backend} via BLAS::BLAS")
    else()
        message(STATUS "Sindre Eigen backend: Eigen native vectorization")
    endif()
endif()

if(SINDRE_NO_EXCEPTIONS)
    target_compile_definitions(sindre_base INTERFACE
        SINDRE_NO_EXCEPTIONS=1
        SPDLOG_NO_EXCEPTIONS=1
        CPPHTTPLIB_NO_EXCEPTIONS=1
        SIMDJSON_EXCEPTIONS=0)
    if(MSVC)
        target_compile_options(sindre_base INTERFACE /EHs-c-)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(sindre_base INTERFACE -fno-exceptions)
    endif()
endif()

function(sindre_add_interface_target target module_include)
    add_library(sindre_${target} INTERFACE)
    add_library(sindre::${target} ALIAS sindre_${target})
    target_link_libraries(sindre_${target} INTERFACE sindre_base)
    target_include_directories(sindre_${target} INTERFACE
        $<BUILD_INTERFACE:${SINDRE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
    target_compile_features(sindre_${target} INTERFACE cxx_std_17)
    set_target_properties(sindre_${target} PROPERTIES EXPORT_NAME ${target})
endfunction()

function(sindre_add_static_target target module_include)
    if(ARGN)
        add_library(sindre_${target} STATIC ${ARGN})
    else()
        message(FATAL_ERROR "Static target '${target}' requires at least one source file")
    endif()
    add_library(sindre::${target} ALIAS sindre_${target})
    target_link_libraries(sindre_${target} PUBLIC sindre_base)
    target_include_directories(sindre_${target} PUBLIC
        $<BUILD_INTERFACE:${SINDRE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
    target_compile_features(sindre_${target} PUBLIC cxx_std_17)
    set_target_properties(sindre_${target} PROPERTIES EXPORT_NAME ${target})
endfunction()

function(sindre_make_available dependency)
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

function(sindre_add_test target source)
    if(NOT SINDRE_BUILD_TESTS)
        return()
    endif()
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE ${ARGN})
    add_test(NAME ${target} COMMAND ${target})
endfunction()

function(sindre_copy_runtime_dirs target)
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

function(sindre_copy_target_runtime_dlls target)
    if(NOT WIN32 OR NOT TARGET ${target} OR CMAKE_VERSION VERSION_LESS 3.21)
        return()
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DSINDRE_RUNTIME_DEST=$<TARGET_FILE_DIR:${target}>"
            "-DSINDRE_RUNTIME_FILES=$<TARGET_RUNTIME_DLLS:${target}>"
            -P "${SINDRE_CMAKE_DIR}/CopyRuntime.cmake"
        COMMAND_EXPAND_LISTS
        VERBATIM)
    if(SINDRE_OPENBLAS_RUNTIME_DIR)
        sindre_copy_runtime_dirs(${target} "${SINDRE_OPENBLAS_RUNTIME_DIR}")
    endif()
endfunction()
