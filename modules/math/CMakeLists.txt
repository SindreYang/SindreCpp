
# Math 是基础数学模块：以 Eigen 为头文件后端，并按选项接入 OpenBLAS。
# Eigen 在安装时随 Math 一起导出，避免消费者依赖构建机上的源码缓存。
sindre_add_interface_target(math "${SINDRE_SOURCE_DIR}/include")
target_compile_definitions(sindre_math INTERFACE SINDRE_WITH_MATH=1)

add_library(sindre_math_eigen INTERFACE)
add_library(sindre::eigen ALIAS sindre_math_eigen)
# This value is consumed by the top-level installed-package template after
# this subdirectory has been processed.  Keep it in the parent directory
# scope; a normal directory-local variable would be lost at export time.
set(SINDRE_MATH_EIGEN_BUNDLED ON PARENT_SCOPE)
target_include_directories(sindre_math_eigen SYSTEM INTERFACE
    $<BUILD_INTERFACE:${SINDRE_THIRD_MATH_EIGEN_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/eigen3>)
add_dependencies(sindre_math_eigen sindre_ext_eigen)
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
    sindre_3rdparty_setup_openblas()
    target_link_libraries(sindre_math INTERFACE
        $<BUILD_INTERFACE:sindre::openblas>)
    target_compile_definitions(sindre_math INTERFACE EIGEN_USE_BLAS=1)
    set(SINDRE_MATH_BLAS_ENABLED ON CACHE INTERNAL
        "Whether Math uses the configured BLAS backend" FORCE)
endif()

if(SINDRE_MATH_NATIVE_ARCH)
    # Keep the option compiler-dependent in the exported interface.  A
    # package configured with clang-cl can be consumed by cl.exe, and a
    # producer-side CMAKE_CXX_COMPILER_ID check would leak /clang: into that
    # MSVC consumer command line.
    target_compile_options(sindre_math INTERFACE
        "$<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>"
        "$<$<CXX_COMPILER_ID:Clang>:-march=native>"
        "$<$<CXX_COMPILER_ID:GNU>:-march=native>")
endif()

message(STATUS "Sindre Math backend: ${_sindre_math_blas_backend}")

install(TARGETS sindre_math_eigen EXPORT sindreTargets)
install(DIRECTORY "${SINDRE_THIRD_MATH_EIGEN_SOURCE_DIR}/Eigen"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/eigen3")
    # Tensor 属于 Eigen 的 unsupported 公共头文件树，sindre/math/tensor.h
    # 会直接包含它；只安装头文件子树，避免把 Eigen 的测试和文档打进包。
install(DIRECTORY "${SINDRE_THIRD_MATH_EIGEN_SOURCE_DIR}/unsupported/Eigen"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/eigen3/unsupported")
install(FILES "${SINDRE_THIRD_MATH_EIGEN_SOURCE_DIR}/signature_of_eigen3_matrix_library"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/eigen3")

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
