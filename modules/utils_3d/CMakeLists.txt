# Utils_3d 的公共入口：VTK 提供基础网格/点云能力，CGAL 和 PCL 作为可选后端。
# 复杂算法仍在 algorithms 源文件中维护，模块级 CMake 只负责依赖、导出和测试。
set(_sindre_vtk_components CommonCore CommonDataModel CommonTransforms
    FiltersCore FiltersGeneral FiltersGeometry FiltersModeling IOGeometry IOLegacy IOPLY IOXML)
set(SINDRE_UTILS_3D_VTK_COMPONENTS "${_sindre_vtk_components}" CACHE INTERNAL
    "VTK components required by the installed Utils_3d target")
find_package(VTK ${SINDRE_THIRD_UTILS_3D_VTK_MIN_VERSION}
    REQUIRED COMPONENTS ${_sindre_vtk_components})
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
    "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_compile_definitions(sindre_utils_3d PUBLIC SINDRE_WITH_UTILS_3D=1)
target_link_libraries(sindre_utils_3d PUBLIC sindre::general sindre::math)
target_link_libraries(sindre_utils_3d PRIVATE sindre::nanoflann ${VTK_LIBRARIES})
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

foreach(backend IN ITEMS CGAL PCL)
    if(SINDRE_UTILS_3D_${backend})
        target_compile_definitions(sindre_utils_3d PRIVATE SINDRE_UTILS_3D_${backend}=1)
    endif()
endforeach()
if(SINDRE_UTILS_3D_CGAL)
    find_package(CGAL ${SINDRE_THIRD_UTILS_3D_CGAL_MIN_VERSION}
        CONFIG REQUIRED)
    if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(WARNING
            "The fixed CGAL ${SINDRE_THIRD_UTILS_3D_CGAL_MIN_VERSION} profile is not verified with clang-cl; "
            "use the MSVC compiler or a newer CGAL SDK with a passing backend test.")
    endif()
    target_link_libraries(sindre_utils_3d PRIVATE CGAL::CGAL)
endif()
if(SINDRE_UTILS_3D_PCL)
    find_package(PCL ${SINDRE_THIRD_UTILS_3D_PCL_MIN_VERSION} CONFIG REQUIRED
        COMPONENTS common features filters registration search segmentation surface)
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
endif()

if(WIN32 AND VTK_DIR)
    get_filename_component(_sindre_vtk_root "${VTK_DIR}" DIRECTORY)
    set(_sindre_vtk_runtime_dir "${_sindre_vtk_root}/content/bin")
    if(EXISTS "${_sindre_vtk_runtime_dir}")
        set_property(TARGET sindre_utils_3d PROPERTY SINDRE_RUNTIME_DIRS
            "${_sindre_vtk_runtime_dir}")
    endif()
endif()

if(SINDRE_BUILD_TESTS)
    add_executable(sindre_utils_3d_tests tests/utils_3d_tests.cpp)
    target_link_libraries(sindre_utils_3d_tests PRIVATE sindre::utils_3d)
    sindre_copy_target_runtime_dlls(sindre_utils_3d_tests)
    add_test(NAME sindre.utils_3d COMMAND sindre_utils_3d_tests)
    add_executable(sindre_mesh_tests tests/mesh_tests.cpp)
    target_link_libraries(sindre_mesh_tests PRIVATE sindre::utils_3d)
    if(SINDRE_UTILS_3D_CGAL)
        target_compile_definitions(sindre_mesh_tests PRIVATE SINDRE_UTILS_3D_CGAL=1)
    endif()
    sindre_copy_target_runtime_dlls(sindre_mesh_tests)
    add_test(NAME sindre.mesh COMMAND sindre_mesh_tests)
    add_executable(sindre_nearest_neighbors_tests tests/nearest_neighbors_tests.cpp)
    target_link_libraries(sindre_nearest_neighbors_tests PRIVATE sindre::utils_3d)
    sindre_copy_target_runtime_dlls(sindre_nearest_neighbors_tests)
    add_test(NAME sindre.nearest_neighbors COMMAND sindre_nearest_neighbors_tests)
    if(WIN32 AND VTK_DIR)
        if(EXISTS "${_sindre_vtk_runtime_dir}")
            set(_sindre_vtk_path_modification
                "PATH=path_list_prepend:${_sindre_vtk_runtime_dir}")
            set(_sindre_vtk_tests sindre.utils_3d sindre.mesh)
            foreach(_sindre_vtk_target IN ITEMS sindre_utils_3d_tests sindre_mesh_tests)
                if(TARGET ${_sindre_vtk_target})
                    sindre_copy_runtime_dirs(${_sindre_vtk_target}
                        "${_sindre_vtk_runtime_dir}")
                endif()
            endforeach()
            set_tests_properties(${_sindre_vtk_tests} PROPERTIES
                ENVIRONMENT_MODIFICATION "${_sindre_vtk_path_modification}")
        endif()
    endif()
endif()

if(SINDRE_BUILD_UTILS_3D_BENCHMARKS)
    add_executable(sindre_utils_3d_benchmark tests/utils_3d_benchmark.cpp)
    target_link_libraries(sindre_utils_3d_benchmark PRIVATE sindre::utils_3d)
    sindre_copy_target_runtime_dlls(sindre_utils_3d_benchmark)
endif()
