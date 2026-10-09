# Utils_3d 的公共入口：VTK + CGAL + nanoflann 是固定后端，PCL 作为可选后端。
# 复杂算法仍在 algorithms 源文件中维护，模块级 CMake 只负责依赖、导出和测试。
set(_sindre_vtk_components CommonCore CommonDataModel CommonTransforms
    FiltersCore FiltersGeneral FiltersGeometry FiltersModeling IOGeometry IOLegacy IOPLY IOXML)
set(SINDRE_UTILS_3D_VTK_COMPONENTS "${_sindre_vtk_components}" CACHE INTERNAL
    "VTK components required by the installed Utils_3d target")
if(WIN32)
    find_package(VTK ${SINDRE_THIRD_UTILS_3D_VTK_MIN_VERSION} EXACT CONFIG
        REQUIRED COMPONENTS ${_sindre_vtk_components})
else()
    find_package(VTK ${SINDRE_THIRD_UTILS_3D_VTK_LINUX_MIN_VERSION} CONFIG
        REQUIRED COMPONENTS ${_sindre_vtk_components})
endif()
set(_sindre_utils_3d_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils_3d.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/mesh.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/point_cloud.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/algorithms/algorithms.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/algorithms/advanced.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/algorithms/nearest_neighbors.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sindremesh.cpp")
sindre_add_static_target(utils_3d "${SINDRE_SOURCE_DIR}/include" ${_sindre_utils_3d_sources})
sindre_3rdparty_setup_nanoflann()
target_include_directories(sindre_utils_3d PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/private"
    "${SINDRE_NANOFLANN_INCLUDE_DIR}")
target_compile_definitions(sindre_utils_3d PUBLIC SINDRE_WITH_UTILS_3D=1)
target_link_libraries(sindre_utils_3d PUBLIC sindre::general sindre::math)
target_link_libraries(sindre_utils_3d PRIVATE ${VTK_LIBRARIES})
# CGAL's interval implementation uses SSE4.1 intrinsics. Keep this
# requirement private to Utils3D: it must not turn on AVX for PCL or for
# consumers of the Math target.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(MSVC)
        target_compile_options(sindre_utils_3d PRIVATE /clang:-msse4.1)
    else()
        target_compile_options(sindre_utils_3d PRIVATE -msse4.1)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(sindre_utils_3d PRIVATE -msse4.1)
endif()
if(SINDRE_UTILS_3D_NATIVE_ARCH)
    add_library(sindre_utils_3d_native_arch INTERFACE)
    if(MSVC)
        target_compile_options(sindre_utils_3d_native_arch INTERFACE /arch:AVX2)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(sindre_utils_3d_native_arch INTERFACE -march=native)
    endif()
    target_link_libraries(sindre_utils_3d PUBLIC sindre_utils_3d_native_arch)
    install(TARGETS sindre_utils_3d_native_arch EXPORT sindreTargets)
endif()

target_compile_definitions(sindre_utils_3d PRIVATE SINDRE_UTILS_3D_CGAL=1)
if(SINDRE_UTILS_3D_PCL)
    target_compile_definitions(sindre_utils_3d PRIVATE SINDRE_UTILS_3D_PCL=1)
endif()
# The value is an internal invariant set by Options.cmake, not a user-facing
# feature switch. Keep the scope explicit so the dependency block remains
# easy to audit.
if(SINDRE_UTILS_3D_CGAL)
if(WIN32)
    # CGAL's setup calls find_package(Boost) internally.  CMake 4.4 defaults
    # CMP0167 to CONFIG-only mode; the standalone CGAL profile supplies its
    # own small BoostConfig.cmake adapter instead of using vcpkg targets.
    if(POLICY CMP0167)
        cmake_policy(SET CMP0167 OLD)
    endif()
    if(NOT SINDRE_UTILS_3D_BOOST_ROOT)
        if(BOOST_ROOT)
            set(SINDRE_UTILS_3D_BOOST_ROOT "${BOOST_ROOT}" CACHE PATH
                "Official standalone Boost root used by the CGAL backend" FORCE)
        elseif(Boost_ROOT)
            set(SINDRE_UTILS_3D_BOOST_ROOT "${Boost_ROOT}" CACHE PATH
                "Official standalone Boost root used by the CGAL backend" FORCE)
        endif()
    endif()
    # An explicit standalone root is authoritative.  Do not let a stale
    # CGAL_DIR from a previous configure (including a vcpkg package) win over
    # the SDK the caller selected for this build.
    if(SINDRE_UTILS_3D_CGAL_ROOT)
        set(_sindre_cgal_config_dir
            "${SINDRE_UTILS_3D_CGAL_ROOT}/lib/cmake/CGAL")
        if(EXISTS "${_sindre_cgal_config_dir}/CGALConfig.cmake")
            set(CGAL_DIR "${_sindre_cgal_config_dir}" CACHE PATH
                "Official standalone CGAL package directory" FORCE)
        else()
            message(FATAL_ERROR
                "SINDRE_UTILS_3D_CGAL_ROOT does not contain "
                "lib/cmake/CGAL/CGALConfig.cmake: ${SINDRE_UTILS_3D_CGAL_ROOT}")
        endif()
    endif()
    if(NOT CGAL_DIR)
        message(FATAL_ERROR
            "sindre::utils_3d CGAL requires an explicit standalone CGAL SDK. "
            "Set CGAL_DIR to its lib/cmake/CGAL directory or set "
            "SINDRE_UTILS_3D_CGAL_ROOT. vcpkg and implicit CMAKE_PREFIX_PATH "
            "discovery are not accepted.")
    endif()
    if(NOT EXISTS "${CGAL_DIR}/CGALConfig.cmake")
        message(FATAL_ERROR
            "CGAL_DIR must contain CGALConfig.cmake: ${CGAL_DIR}")
    endif()
    if(CGAL_DIR MATCHES "[Vv][Cc][Pp][Kk][Gg]" OR
       SINDRE_UTILS_3D_CGAL_ROOT MATCHES "[Vv][Cc][Pp][Kk][Gg]")
        message(FATAL_ERROR
            "sindre::utils_3d requires the official standalone CGAL SDK; "
            "CGAL_DIR or SINDRE_UTILS_3D_CGAL_ROOT appears to point into vcpkg: "
            "${CGAL_DIR}")
    endif()
    if(NOT SINDRE_UTILS_3D_BOOST_ROOT)
        message(FATAL_ERROR
            "sindre::utils_3d CGAL requires SINDRE_UTILS_3D_BOOST_ROOT "
            "or BOOST_ROOT/Boost_ROOT pointing to an official standalone Boost SDK; "
            "vcpkg Boost is not accepted.")
    endif()
    if(SINDRE_UTILS_3D_BOOST_ROOT MATCHES "[Vv][Cc][Pp][Kk][Gg]")
        message(FATAL_ERROR
            "sindre::utils_3d requires an official standalone Boost SDK; "
            "SINDRE_UTILS_3D_BOOST_ROOT appears to point into vcpkg: "
            "${SINDRE_UTILS_3D_BOOST_ROOT}")
    endif()
    if(DEFINED Boost_DIR AND Boost_DIR MATCHES "[Vv][Cc][Pp][Kk][Gg]")
        message(FATAL_ERROR
            "sindre::utils_3d CGAL requires the standalone Boost adapter; "
            "the configured Boost package appears to come from vcpkg: ${Boost_DIR}")
    endif()
    set(BOOST_ROOT "${SINDRE_UTILS_3D_BOOST_ROOT}" CACHE PATH
        "Official standalone Boost root used by the CGAL backend" FORCE)
    set(Boost_ROOT "${SINDRE_UTILS_3D_BOOST_ROOT}" CACHE PATH
        "Official standalone Boost root used by the CGAL backend" FORCE)
    set(Boost_DIR "${SINDRE_SOURCE_DIR}/cmake/BoostStandalone" CACHE PATH
        "sindre standalone Boost adapter for CGAL" FORCE)
    list(PREPEND CMAKE_PREFIX_PATH "${SINDRE_SOURCE_DIR}/cmake/BoostStandalone")

    foreach(_sindre_cgal_numeric_dep IN ITEMS GMP MPFR)
        if((NOT DEFINED ${_sindre_cgal_numeric_dep}_ROOT OR
            "${${_sindre_cgal_numeric_dep}_ROOT}" STREQUAL "") AND
           DEFINED SINDRE_UTILS_3D_${_sindre_cgal_numeric_dep}_ROOT AND
           NOT "${SINDRE_UTILS_3D_${_sindre_cgal_numeric_dep}_ROOT}" STREQUAL "")
            set(${_sindre_cgal_numeric_dep}_ROOT
                "${SINDRE_UTILS_3D_${_sindre_cgal_numeric_dep}_ROOT}")
        endif()
        if(NOT DEFINED ${_sindre_cgal_numeric_dep}_ROOT OR
           "${${_sindre_cgal_numeric_dep}_ROOT}" STREQUAL "")
            message(FATAL_ERROR
                "sindre::utils_3d requires an explicit standalone "
                "${_sindre_cgal_numeric_dep} SDK root for CGAL. Set "
                "SINDRE_UTILS_3D_${_sindre_cgal_numeric_dep}_ROOT or "
                "${_sindre_cgal_numeric_dep}_ROOT; see docs/guides/vtk.md.")
        endif()
        if("${${_sindre_cgal_numeric_dep}_ROOT}" MATCHES "[Vv][Cc][Pp][Kk][Gg]")
            message(FATAL_ERROR
                "sindre::utils_3d rejects vcpkg ${_sindre_cgal_numeric_dep}; "
                "use the official standalone CGAL auxiliary SDK.")
        endif()
        if(NOT IS_DIRECTORY "${${_sindre_cgal_numeric_dep}_ROOT}")
            message(FATAL_ERROR
                "${_sindre_cgal_numeric_dep}_ROOT is not a directory: "
                "${${_sindre_cgal_numeric_dep}_ROOT}")
        endif()
    endforeach()

    # CGAL 6.2's FindGMP/FindMPFR modules read the legacy include/library
    # variables instead of GMP_ROOT/MPFR_ROOT. Translate explicit standalone
    # roots before loading CGAL so a supplied SDK is actually used rather than
    # silently falling back to the host search path.
    foreach(_sindre_cgal_numeric_dep IN ITEMS GMP MPFR)
        if(DEFINED ${_sindre_cgal_numeric_dep}_ROOT AND
           IS_DIRECTORY "${${_sindre_cgal_numeric_dep}_ROOT}")
            set(_sindre_cgal_numeric_root
                "${${_sindre_cgal_numeric_dep}_ROOT}")
            set(${_sindre_cgal_numeric_dep}_INCLUDE_DIR
                "${_sindre_cgal_numeric_root}/include" CACHE PATH
                "Standalone ${_sindre_cgal_numeric_dep} include directory" FORCE)
            if(_sindre_cgal_numeric_dep STREQUAL "GMP")
                find_library(_sindre_cgal_numeric_release
                    NAMES gmp libgmp-10 gmp-10 mpir
                    HINTS "${_sindre_cgal_numeric_root}/lib" NO_DEFAULT_PATH)
                find_library(_sindre_cgal_numeric_debug
                    NAMES gmpd gmp libgmp-10 gmp-10 mpir
                    HINTS "${_sindre_cgal_numeric_root}/debug/lib"
                          "${_sindre_cgal_numeric_root}/lib" NO_DEFAULT_PATH)
                if(_sindre_cgal_numeric_release)
                    set(GMP_LIBRARY_RELEASE "${_sindre_cgal_numeric_release}"
                        CACHE FILEPATH "Standalone GMP release library" FORCE)
                    set(GMP_LIBRARIES "${_sindre_cgal_numeric_release}"
                        CACHE STRING "Standalone GMP libraries" FORCE)
                endif()
                if(_sindre_cgal_numeric_debug)
                    set(GMP_LIBRARY_DEBUG "${_sindre_cgal_numeric_debug}"
                        CACHE FILEPATH "Standalone GMP debug library" FORCE)
                endif()
            else()
                find_library(_sindre_cgal_numeric_library
                    NAMES mpfr libmpfr-4 libmpfr-1
                    HINTS "${_sindre_cgal_numeric_root}/lib" NO_DEFAULT_PATH)
                if(_sindre_cgal_numeric_library)
                    set(MPFR_LIBRARIES "${_sindre_cgal_numeric_library}"
                        CACHE FILEPATH "Standalone MPFR library" FORCE)
                endif()
            endif()
        endif()
        unset(_sindre_cgal_numeric_root)
        unset(_sindre_cgal_numeric_release)
        unset(_sindre_cgal_numeric_debug)
        unset(_sindre_cgal_numeric_library)
    endforeach()
    if(NOT GMP_INCLUDE_DIR OR NOT GMP_LIBRARIES OR
       NOT EXISTS "${GMP_INCLUDE_DIR}/gmp.h")
        message(FATAL_ERROR
            "The standalone GMP SDK was not resolved from GMP_ROOT: "
            "${GMP_ROOT}")
    endif()
    if(NOT MPFR_INCLUDE_DIR OR NOT MPFR_LIBRARIES OR
       NOT EXISTS "${MPFR_INCLUDE_DIR}/mpfr.h")
        message(FATAL_ERROR
            "The standalone MPFR SDK was not resolved from MPFR_ROOT: "
            "${MPFR_ROOT}")
    endif()
    find_package(CGAL ${SINDRE_THIRD_UTILS_3D_CGAL_MIN_VERSION} EXACT
        CONFIG REQUIRED)
    # CGAL is intentionally not supplied by vcpkg.  Other modules may still
    # use a vcpkg toolchain, but this backend must resolve to the fixed
    # standalone CGAL SDK selected by CGAL_DIR/package discovery.
    if(DEFINED CGAL_DIR AND CGAL_DIR MATCHES "[Vv][Cc][Pp][Kk][Gg]")
        message(FATAL_ERROR
            "sindre::utils_3d requires standalone CGAL ${SINDRE_THIRD_UTILS_3D_CGAL_MIN_VERSION}; "
            "the CGAL package resolved from '${CGAL_DIR}' appears to be a vcpkg package. "
            "Set CGAL_DIR to the official standalone CGAL SDK.")
    endif()
    if(DEFINED CGAL_CONFIG AND CGAL_CONFIG MATCHES "[Vv][Cc][Pp][Kk][Gg]")
        message(FATAL_ERROR
            "sindre::utils_3d resolved CGAL from a vcpkg package at '${CGAL_CONFIG}'. "
            "Use the official standalone CGAL SDK instead.")
    endif()
    # CGAL's own FindGMP/FindMPFR modules can search default prefixes even
    # when CGAL_DIR is explicit.  Reject every resolved CGAL dependency that
    # points into vcpkg so a mixed SDK cannot pass configuration accidentally.
    foreach(_sindre_cgal_path_var IN ITEMS
            CGAL_CONFIG_DIR CGAL_ROOT CGAL_INCLUDE_DIRS
            GMP_INCLUDE_DIR GMP_LIBRARIES GMP_LIBRARY_RELEASE GMP_LIBRARY_DEBUG
            MPFR_INCLUDE_DIR MPFR_LIBRARIES)
        if(DEFINED ${_sindre_cgal_path_var})
            set(_sindre_cgal_path_value "${${_sindre_cgal_path_var}}")
            if(_sindre_cgal_path_value MATCHES "[Vv][Cc][Pp][Kk][Gg]")
                message(FATAL_ERROR
                    "sindre::utils_3d CGAL dependency '${_sindre_cgal_path_var}' "
                    "resolved to a vcpkg path: ${_sindre_cgal_path_value}. "
                    "Use standalone CGAL/GMP/MPFR SDKs.")
            endif()
        endif()
    endforeach()
    # General and vcpkg may contribute a broad include directory transitively.
    # Put the exact CGAL SDK first so a stale vcpkg CGAL header cannot be mixed
    # with the requested fixed CGAL package.
    if(CGAL_INCLUDE_DIRS)
        target_include_directories(sindre_utils_3d SYSTEM BEFORE PRIVATE ${CGAL_INCLUDE_DIRS})
    endif()
    if(WIN32 AND GMP_INCLUDE_DIR)
        get_filename_component(_sindre_cgal_gmp_root "${GMP_INCLUDE_DIR}" DIRECTORY)
        set(_sindre_cgal_runtime_dir "${_sindre_cgal_gmp_root}/bin")
        if(EXISTS "${_sindre_cgal_runtime_dir}")
            set_property(TARGET sindre_utils_3d APPEND PROPERTY SINDRE_RUNTIME_DIRS
                "${_sindre_cgal_runtime_dir}")
        endif()
    endif()
    target_link_libraries(sindre_utils_3d PRIVATE CGAL::CGAL)
else()
    # Linux accepts the distribution CGAL/Boost/GMP/MPFR packages.  CGAL is
    # still never taken from vcpkg; an explicit vcpkg path is rejected below.
    if(POLICY CMP0167)
        cmake_policy(SET CMP0167 OLD)
    endif()
    if(SINDRE_UTILS_3D_CGAL_ROOT)
        set(CGAL_DIR "${SINDRE_UTILS_3D_CGAL_ROOT}/lib/cmake/CGAL"
            CACHE PATH "Linux CGAL package directory" FORCE)
    endif()
    if(CGAL_DIR MATCHES "[Vv][Cc][Pp][Kk][Gg]" OR
       SINDRE_UTILS_3D_CGAL_ROOT MATCHES "[Vv][Cc][Pp][Kk][Gg]")
        message(FATAL_ERROR
            "sindre::utils_3d does not accept a vcpkg CGAL package; "
            "use the Linux system CGAL package or an independent SDK")
    endif()
    find_package(CGAL ${SINDRE_THIRD_UTILS_3D_CGAL_LINUX_MIN_VERSION}
        CONFIG REQUIRED)
    foreach(_sindre_linux_cgal_path_var IN ITEMS
            CGAL_CONFIG_DIR CGAL_ROOT CGAL_INCLUDE_DIRS CGAL_DIR)
        if(DEFINED ${_sindre_linux_cgal_path_var})
            set(_sindre_linux_cgal_path "${${_sindre_linux_cgal_path_var}}")
            if(_sindre_linux_cgal_path MATCHES "[Vv][Cc][Pp][Kk][Gg]")
                message(FATAL_ERROR
                    "CGAL resolved from a vcpkg path: ${_sindre_linux_cgal_path}")
            endif()
        endif()
    endforeach()
    if(CGAL_INCLUDE_DIRS)
        target_include_directories(sindre_utils_3d SYSTEM BEFORE PRIVATE
            ${CGAL_INCLUDE_DIRS})
    endif()
    target_link_libraries(sindre_utils_3d PRIVATE CGAL::CGAL)
endif()
endif()
find_file(_sindre_cgal_curvature_header
    NAMES interpolated_corrected_curvatures.h
    PATHS ${CGAL_INCLUDE_DIRS} /usr/include /usr/local/include
    PATH_SUFFIXES CGAL/Polygon_mesh_processing
    NO_DEFAULT_PATH)
if(_sindre_cgal_curvature_header)
    target_compile_definitions(sindre_utils_3d PRIVATE
        SINDRE_UTILS_3D_HAS_CGAL_CURVATURE=1)
endif()
if(SINDRE_UTILS_3D_PCL)
    if(SINDRE_MATH_NATIVE_ARCH)
        message(FATAL_ERROR
            "SINDRE_UTILS_3D_PCL cannot be combined with "
            "SINDRE_MATH_NATIVE_ARCH=ON. The supported PCL SDK profile is "
            "not built with AVX; configure with "
            "-DSINDRE_MATH_NATIVE_ARCH=OFF to avoid Eigen/PCL alignment ABI "
            "mismatch.")
    endif()
    if(SINDRE_UTILS_3D_PCL_ROOT AND NOT PCL_DIR)
        find_path(_sindre_pcl_config_dir PCLConfig.cmake
            HINTS "${SINDRE_UTILS_3D_PCL_ROOT}"
            PATH_SUFFIXES lib/cmake/pcl share/pcl lib/pcl)
        if(_sindre_pcl_config_dir)
            set(PCL_DIR "${_sindre_pcl_config_dir}" CACHE PATH
                "PCL CMake package directory" FORCE)
        endif()
    endif()
    if(WIN32)
        find_package(PCL ${SINDRE_THIRD_UTILS_3D_PCL_MIN_VERSION} EXACT CONFIG REQUIRED
            COMPONENTS common features filters registration search segmentation surface)
    else()
        find_package(PCL ${SINDRE_THIRD_UTILS_3D_PCL_LINUX_MIN_VERSION} CONFIG REQUIRED
            COMPONENTS common features filters registration search segmentation surface)
    endif()
    if(PCL_INCLUDE_DIRS)
        target_include_directories(sindre_utils_3d SYSTEM PRIVATE ${PCL_INCLUDE_DIRS})
    endif()
    if(PCL_LIBRARY_DIRS)
        target_link_directories(sindre_utils_3d PRIVATE ${PCL_LIBRARY_DIRS})
    endif()
    if(PCL_DEFINITIONS)
        target_compile_options(sindre_utils_3d PRIVATE ${PCL_DEFINITIONS})
    endif()
    target_link_libraries(sindre_utils_3d PRIVATE ${PCL_LIBRARIES})
    # The fixed Windows PCL archive is built with OpenMP-enabled Eigen/PCL
    # templates. PCL's package file does not propagate the LLVM runtime, so
    # close that static-link dependency explicitly without adding OpenMP to
    # General or CGAL.
    if(NOT TARGET OpenMP::OpenMP_CXX AND MSVC AND
       CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        get_filename_component(_sindre_llvm_bin
            "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(_sindre_llvm_root
            "${_sindre_llvm_bin}" DIRECTORY)
        set(_sindre_libomp_candidate "${_sindre_llvm_root}/lib/libomp.lib")
        if(EXISTS "${_sindre_libomp_candidate}")
            set(_sindre_libomp_library "${_sindre_libomp_candidate}")
        else()
            find_library(_sindre_libomp_library NAMES libomp
                HINTS "${_sindre_llvm_root}/lib" NO_DEFAULT_PATH)
        endif()
        if(NOT _sindre_libomp_library)
            message(FATAL_ERROR
                "The PCL backend requires LLVM OpenMP (libomp.lib). "
                "Install/use a clang-cl toolchain that provides it.")
        endif()
        add_library(sindre_utils_3d_openmp UNKNOWN IMPORTED GLOBAL)
        set_target_properties(sindre_utils_3d_openmp PROPERTIES
            IMPORTED_CONFIGURATIONS "DEBUG;RELEASE;RELWITHDEBINFO;MINSIZEREL"
            IMPORTED_LOCATION "${_sindre_libomp_library}"
            IMPORTED_LOCATION_DEBUG "${_sindre_libomp_library}"
            IMPORTED_LOCATION_RELEASE "${_sindre_libomp_library}"
            IMPORTED_LOCATION_RELWITHDEBINFO "${_sindre_libomp_library}"
            IMPORTED_LOCATION_MINSIZEREL "${_sindre_libomp_library}")
        add_library(OpenMP::OpenMP_CXX ALIAS sindre_utils_3d_openmp)
        set(_sindre_libomp_runtime_file "${_sindre_llvm_bin}/libomp.dll")
    endif()
    if(NOT TARGET OpenMP::OpenMP_CXX)
        find_package(OpenMP REQUIRED COMPONENTS CXX)
    endif()
    target_link_libraries(sindre_utils_3d PRIVATE OpenMP::OpenMP_CXX)
    if(WIN32 AND MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND
       NOT DEFINED _sindre_libomp_runtime_file)
        get_filename_component(_sindre_llvm_bin
            "${CMAKE_CXX_COMPILER}" DIRECTORY)
        set(_sindre_libomp_runtime_file "${_sindre_llvm_bin}/libomp.dll")
    endif()
    if(WIN32 AND EXISTS "${_sindre_libomp_runtime_file}")
        install(FILES "${_sindre_libomp_runtime_file}"
            DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()

if(WIN32 AND VTK_DIR)
    get_filename_component(_sindre_vtk_root "${VTK_DIR}" DIRECTORY)
    set(_sindre_vtk_runtime_dir "${_sindre_vtk_root}/content/bin")
    if(EXISTS "${_sindre_vtk_runtime_dir}")
        set_property(TARGET sindre_utils_3d PROPERTY SINDRE_RUNTIME_DIRS
            "${_sindre_vtk_runtime_dir}")
    endif()
endif()
if(WIN32)
    if(DEFINED _sindre_vtk_runtime_dir AND EXISTS "${_sindre_vtk_runtime_dir}")
        sindre_install_runtime_dirs("${_sindre_vtk_runtime_dir}")
    endif()
    if(DEFINED _sindre_cgal_runtime_dir AND EXISTS "${_sindre_cgal_runtime_dir}")
        sindre_install_runtime_dirs("${_sindre_cgal_runtime_dir}")
    endif()
endif()

if(SINDRE_BUILD_TESTS)
    add_executable(sindre_utils_3d_tests tests/utils_3d_tests.cpp)
    target_link_libraries(sindre_utils_3d_tests PRIVATE sindre::utils_3d)
    sindre_copy_target_runtime_dlls(sindre_utils_3d_tests)
    if(DEFINED _sindre_libomp_runtime_file AND
       EXISTS "${_sindre_libomp_runtime_file}")
        sindre_copy_runtime_files(sindre_utils_3d_tests
            "${_sindre_libomp_runtime_file}")
    endif()
    add_test(NAME sindre.utils_3d COMMAND sindre_utils_3d_tests)
    add_executable(sindre_mesh_tests tests/mesh_tests.cpp)
    target_link_libraries(sindre_mesh_tests PRIVATE sindre::utils_3d)
    target_compile_definitions(sindre_mesh_tests PRIVATE SINDRE_UTILS_3D_CGAL=1)
    if(_sindre_cgal_curvature_header)
        target_compile_definitions(sindre_mesh_tests PRIVATE
            SINDRE_UTILS_3D_HAS_CGAL_CURVATURE=1)
    endif()
    sindre_copy_target_runtime_dlls(sindre_mesh_tests)
    if(DEFINED _sindre_libomp_runtime_file AND
       EXISTS "${_sindre_libomp_runtime_file}")
        sindre_copy_runtime_files(sindre_mesh_tests
            "${_sindre_libomp_runtime_file}")
    endif()
    add_test(NAME sindre.mesh COMMAND sindre_mesh_tests)
    add_executable(sindre_nearest_neighbors_tests tests/nearest_neighbors_tests.cpp)
    target_link_libraries(sindre_nearest_neighbors_tests PRIVATE sindre::utils_3d)
    sindre_copy_target_runtime_dlls(sindre_nearest_neighbors_tests)
    if(DEFINED _sindre_libomp_runtime_file AND
       EXISTS "${_sindre_libomp_runtime_file}")
        sindre_copy_runtime_files(sindre_nearest_neighbors_tests
            "${_sindre_libomp_runtime_file}")
    endif()
    add_test(NAME sindre.nearest_neighbors COMMAND sindre_nearest_neighbors_tests)
    if(SINDRE_UTILS_3D_PCL)
        add_executable(sindre_pcl_tests tests/pcl_tests.cpp)
        target_link_libraries(sindre_pcl_tests PRIVATE sindre::utils_3d)
        sindre_copy_target_runtime_dlls(sindre_pcl_tests)
        if(DEFINED _sindre_libomp_runtime_file AND
           EXISTS "${_sindre_libomp_runtime_file}")
            sindre_copy_runtime_files(sindre_pcl_tests
                "${_sindre_libomp_runtime_file}")
        endif()
        add_test(NAME sindre.pcl COMMAND sindre_pcl_tests)
    endif()
    if(WIN32 AND VTK_DIR)
        if(EXISTS "${_sindre_vtk_runtime_dir}")
            set(_sindre_vtk_tests sindre.utils_3d sindre.mesh)
            foreach(_sindre_vtk_target IN ITEMS sindre_utils_3d_tests sindre_mesh_tests)
                if(TARGET ${_sindre_vtk_target})
                    sindre_copy_runtime_dirs(${_sindre_vtk_target}
                        "${_sindre_vtk_runtime_dir}")
                    if(DEFINED _sindre_cgal_runtime_dir AND
                       EXISTS "${_sindre_cgal_runtime_dir}")
                        sindre_copy_runtime_dirs(${_sindre_vtk_target}
                            "${_sindre_cgal_runtime_dir}")
                    endif()
                endif()
            endforeach()
            set(_sindre_runtime_path_modifications
                "PATH=path_list_prepend:${_sindre_vtk_runtime_dir}")
            if(DEFINED _sindre_cgal_runtime_dir AND
               EXISTS "${_sindre_cgal_runtime_dir}")
                list(APPEND _sindre_runtime_path_modifications
                    "PATH=path_list_prepend:${_sindre_cgal_runtime_dir}")
            endif()
            set_tests_properties(${_sindre_vtk_tests} PROPERTIES
                ENVIRONMENT_MODIFICATION "${_sindre_runtime_path_modifications}")
        endif()
    endif()
endif()

if(SINDRE_BUILD_UTILS_3D_BENCHMARKS)
    add_executable(sindre_utils_3d_benchmark tests/utils_3d_benchmark.cpp)
    target_link_libraries(sindre_utils_3d_benchmark PRIVATE sindre::utils_3d)
    sindre_copy_target_runtime_dlls(sindre_utils_3d_benchmark)
endif()
