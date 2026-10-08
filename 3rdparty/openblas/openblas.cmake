include_guard(GLOBAL)

# OpenBLAS 的源码下载、隔离构建和导入 target。只生成 Math 所需的静态 CBLAS，
# 关闭 Fortran、LAPACK 和测试，减少依赖面并保持构建结果可复现。

set(SINDRE_THIRD_MATH_OPENBLAS_VERSION "0.3.34")
set(SINDRE_THIRD_MATH_OPENBLAS_URL
    "https://github.com/OpenMathLib/OpenBLAS/releases/download/v0.3.34/OpenBLAS-0.3.34.tar.gz")
set(SINDRE_THIRD_MATH_OPENBLAS_SHA256
    "cd7e129868320cc2d033afa920e31202dfe0b8066a5b66661900ccc0f197dfed")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(SINDRE_THIRD_MATH_OPENBLAS_TARGET "ARMV8")
else()
    set(SINDRE_THIRD_MATH_OPENBLAS_TARGET "NEHALEM")
endif()

function(sindre_3rdparty_setup_openblas)
    if(TARGET sindre::openblas)
        return()
    endif()

    set(_prefix "${SINDRE_THIRD_PARTY_CACHE_DIR}/openblas")
    ExternalProject_Add(sindre_ext_openblas
        PREFIX "${_prefix}"
        URL "${SINDRE_THIRD_MATH_OPENBLAS_URL}"
        URL_HASH "SHA256=${SINDRE_THIRD_MATH_OPENBLAS_SHA256}"
        DOWNLOAD_DIR "${SINDRE_THIRD_PARTY_DOWNLOAD_DIR}/openblas"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        CMAKE_ARGS
            ${SINDRE_EXTERNAL_PROJECT_CMAKE_ARGS}
            -DTARGET=${SINDRE_THIRD_MATH_OPENBLAS_TARGET}
            -DNOFORTRAN=ON
            -DBUILD_WITHOUT_LAPACK=ON
            -DBUILD_WITHOUT_LAPACKE=ON
            -DBUILD_WITHOUT_CBLAS=OFF
            -DDYNAMIC_ARCH=OFF
            -DBUILD_STATIC_LIBS=ON
            -DBUILD_SHARED_LIBS=OFF
            -DBUILD_TESTING=OFF
            -DCMAKE_INSTALL_LIBDIR=lib
            -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        BUILD_BYPRODUCTS
            <INSTALL_DIR>/lib/${CMAKE_STATIC_LIBRARY_PREFIX}openblas${CMAKE_STATIC_LIBRARY_SUFFIX})
    ExternalProject_Get_Property(sindre_ext_openblas INSTALL_DIR)
    set(_include_dir "${INSTALL_DIR}/include")
    set(_lib_dir "${INSTALL_DIR}/lib")
    set(_library "${_lib_dir}/${CMAKE_STATIC_LIBRARY_PREFIX}openblas${CMAKE_STATIC_LIBRARY_SUFFIX}")
    add_library(sindre_openblas STATIC IMPORTED GLOBAL)
    set_target_properties(sindre_openblas PROPERTIES
        IMPORTED_LOCATION "${_library}"
        IMPORTED_LOCATION_RELEASE "${_library}"
        IMPORTED_LOCATION_RELWITHDEBINFO "${_library}"
        IMPORTED_LOCATION_MINSIZEREL "${_library}"
        INTERFACE_INCLUDE_DIRECTORIES "${_include_dir}")
    add_dependencies(sindre_openblas sindre_ext_openblas)
    add_library(sindre::openblas ALIAS sindre_openblas)
    set(SINDRE_MATH_OPENBLAS_INCLUDE_DIR "${_include_dir}" PARENT_SCOPE)
    set(SINDRE_MATH_OPENBLAS_LIB_DIR "${_lib_dir}" PARENT_SCOPE)
    set(SINDRE_MATH_OPENBLAS_LIBRARY "${_library}" PARENT_SCOPE)
endfunction()
