include("${SINDRE_THIRDS_DIR}/math/Dependencies.cmake")

sindre_add_interface_target(math "${SINDRE_SOURCE_DIR}/include")
target_compile_definitions(sindre_math INTERFACE SINDRE_WITH_MATH=1)

sindre_thirds_declare_local(eigen
    "${SINDRE_THIRDS_DIR}/math/eigen/3.4.1/eigen-3.4.1")
set(SINDRE_MATH_EIGEN_BUNDLED ON CACHE INTERNAL
    "Math uses the fixed bundled Eigen source")
set(EIGEN_BUILD_DOC OFF CACHE BOOL "Disable Eigen documentation" FORCE)
set(EIGEN_BUILD_PKGCONFIG OFF CACHE BOOL "Disable Eigen pkg-config" FORCE)
FetchContent_MakeAvailable(eigen)
if(NOT TARGET Eigen3::Eigen)
    message(FATAL_ERROR "Math requires the fixed Eigen3::Eigen target")
endif()

add_library(sindre_math_eigen INTERFACE)
add_library(sindre::eigen ALIAS sindre_math_eigen)
target_include_directories(sindre_math_eigen SYSTEM INTERFACE
    $<BUILD_INTERFACE:${eigen_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/eigen3>)
set_target_properties(sindre_math_eigen PROPERTIES EXPORT_NAME eigen)

target_link_libraries(sindre_math INTERFACE
    $<BUILD_INTERFACE:sindre_math_eigen>
    $<INSTALL_INTERFACE:sindre::eigen>)

string(TOUPPER "${SINDRE_MATH_BLAS_BACKEND}" _sindre_math_blas_backend)
if(NOT _sindre_math_blas_backend MATCHES "^(OPENBLAS|EIGEN)$")
    message(FATAL_ERROR
        "SINDRE_MATH_BLAS_BACKEND must be OPENBLAS or EIGEN")
endif()

set(SINDRE_MATH_BLAS_ENABLED OFF CACHE INTERNAL
    "Whether Math uses the configured BLAS backend" FORCE)
if(_sindre_math_blas_backend STREQUAL "OPENBLAS")
    if(NOT IS_DIRECTORY "${SINDRE_MATH_OPENBLAS_ROOT}")
        message(FATAL_ERROR
            "Fixed Math dependency is missing: OpenBLAS at ${SINDRE_MATH_OPENBLAS_ROOT}")
    endif()

    list(PREPEND CMAKE_PREFIX_PATH "${SINDRE_MATH_OPENBLAS_ROOT}")
    if(EXISTS "${SINDRE_MATH_OPENBLAS_ROOT}/bin")
        set(SINDRE_MATH_OPENBLAS_RUNTIME_DIR
            "${SINDRE_MATH_OPENBLAS_ROOT}/bin" CACHE INTERNAL
            "OpenBLAS runtime directory for Math targets")
    endif()
    if(EXISTS "${SINDRE_MATH_OPENBLAS_ROOT}/lib/cmake/openblas")
        set(OpenBLAS_DIR "${SINDRE_MATH_OPENBLAS_ROOT}/lib/cmake/openblas"
            CACHE PATH "Fixed Math OpenBLAS CMake package directory" FORCE)
    endif()

    set(BLA_VENDOR OpenBLAS)
    find_package(${SINDRE_THIRD_MATH_OPENBLAS_CONFIG_PACKAGE}
        ${SINDRE_THIRD_MATH_OPENBLAS_VERSION} EXACT CONFIG REQUIRED)
    if(NOT DEFINED OpenBLAS_INCLUDE_DIRS)
        message(FATAL_ERROR "Fixed OpenBLAS package did not provide OpenBLAS_INCLUDE_DIRS")
    endif()
    get_filename_component(_sindre_openblas_root
        "${OpenBLAS_INCLUDE_DIRS}" DIRECTORY)
    set(_sindre_openblas_import
        "${_sindre_openblas_root}/lib/${SINDRE_THIRD_MATH_OPENBLAS_IMPORT_LIBRARY}")
    if(NOT EXISTS "${_sindre_openblas_import}")
        message(FATAL_ERROR "Fixed OpenBLAS import library is missing: ${_sindre_openblas_import}")
    endif()
    set(BLAS_LIBRARIES "${_sindre_openblas_import}"
        CACHE STRING "Fixed OpenBLAS import library" FORCE)
    find_package(BLAS REQUIRED)
    if(NOT TARGET BLAS::BLAS)
        message(FATAL_ERROR "Math requires a BLAS::BLAS target for OpenBLAS")
    endif()
    target_link_libraries(sindre_math INTERFACE
        $<BUILD_INTERFACE:BLAS::BLAS>
        $<INSTALL_INTERFACE:BLAS::BLAS>)
    target_compile_definitions(sindre_math INTERFACE EIGEN_USE_BLAS=1)
    set(SINDRE_MATH_BLAS_ENABLED ON CACHE INTERNAL
        "Whether Math uses the configured BLAS backend" FORCE)
endif()

if(SINDRE_MATH_NATIVE_ARCH)
    if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(sindre_math INTERFACE /clang:-march=native)
    elseif(MSVC)
        target_compile_options(sindre_math INTERFACE /arch:AVX2)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|AppleClang")
        target_compile_options(sindre_math INTERFACE -march=native)
    endif()
endif()

message(STATUS "Sindre Math backend: ${_sindre_math_blas_backend}")

if(DEFINED eigen_SOURCE_DIR)
    install(TARGETS sindre_math_eigen EXPORT sindrecppTargets)
    install(DIRECTORY "${eigen_SOURCE_DIR}/Eigen"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/eigen3")
    # Tensor 属于 Eigen 的 unsupported 公共头文件树，sindre/math/tensor.h
    # 会直接包含它；只安装头文件子树，避免把 Eigen 的测试和文档打进包。
    install(DIRECTORY "${eigen_SOURCE_DIR}/unsupported/Eigen"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/eigen3/unsupported")
    install(FILES "${eigen_SOURCE_DIR}/signature_of_eigen3_matrix_library"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/eigen3")
endif()

if(SINDRE_BUILD_TESTS)
    add_executable(sindre_math_tests tests/math_tests.cpp)
    target_link_libraries(sindre_math_tests PRIVATE sindre::math)
    sindre_copy_target_runtime_dlls(sindre_math_tests)
    add_test(NAME sindre.math COMMAND sindre_math_tests)
endif()

if(SINDRE_BUILD_BENCHMARKS)
    add_executable(sindre_math_benchmark tests/math_benchmark.cpp)
    target_link_libraries(sindre_math_benchmark PRIVATE sindre::math)
    sindre_copy_target_runtime_dlls(sindre_math_benchmark)
endif()
