# Utils_2d 仅封装 OpenCV 相关能力。依赖和 DLL 运行时都限制在本模块，
# 只有显式开启 SINDRE_WITH_UTILS_2D 时才参与配置和构建。
sindre_add_static_target(utils_2d "${SINDRE_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils_2d.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/image.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/algorithms.cpp")
target_compile_definitions(sindre_utils_2d PUBLIC SINDRE_WITH_UTILS_2D=1)
set(_sindre_opencv_components "${SINDRE_THIRD_UTILS_2D_OPENCV_COMPONENTS}")
set(_sindre_utils_2d_has_xfeatures2d OFF)
if(NOT WIN32)
    # Ubuntu/Debian OpenCV packages commonly omit the non-free xfeatures2d
    # module even when opencv-contrib is installed. Keep the core API usable.
    list(REMOVE_ITEM _sindre_opencv_components xfeatures2d)
endif()
if(WIN32)
    find_package(OpenCV ${SINDRE_THIRD_UTILS_2D_OPENCV_MIN_VERSION} EXACT CONFIG REQUIRED
        COMPONENTS ${_sindre_opencv_components})
else()
    find_package(OpenCV ${SINDRE_THIRD_UTILS_2D_OPENCV_LINUX_MIN_VERSION} CONFIG REQUIRED
        COMPONENTS ${_sindre_opencv_components})
endif()
if(NOT DEFINED OpenCV_VERSION)
    message(FATAL_ERROR "OpenCV package did not provide OpenCV_VERSION")
endif()
if(WIN32)
    set(_sindre_opencv_min_version "${SINDRE_THIRD_UTILS_2D_OPENCV_MIN_VERSION}")
else()
    set(_sindre_opencv_min_version "${SINDRE_THIRD_UTILS_2D_OPENCV_LINUX_MIN_VERSION}")
endif()
if(OpenCV_VERSION VERSION_LESS _sindre_opencv_min_version)
    message(FATAL_ERROR
        "Utils_2d requires OpenCV >= ${_sindre_opencv_min_version}; "
        "found ${OpenCV_VERSION}")
endif()
set(_sindre_opencv_targets)
foreach(component IN LISTS SINDRE_THIRD_UTILS_2D_OPENCV_COMPONENTS)
    if(TARGET opencv_${component})
        list(APPEND _sindre_opencv_targets opencv_${component})
    elseif(TARGET OpenCV::opencv_${component})
        list(APPEND _sindre_opencv_targets OpenCV::opencv_${component})
    endif()
endforeach()
if(NOT WIN32)
    find_path(_sindre_xfeatures2d_include_dir
        NAMES opencv2/xfeatures2d.hpp
        PATHS ${OpenCV_INCLUDE_DIRS}
        NO_DEFAULT_PATH)
    find_library(_sindre_xfeatures2d_library
        NAMES opencv_xfeatures2d
        PATHS ${OpenCV_LIBRARY_DIRS}
        NO_DEFAULT_PATH)
    if(_sindre_xfeatures2d_include_dir AND _sindre_xfeatures2d_library)
        set(_sindre_utils_2d_has_xfeatures2d ON)
        target_compile_definitions(sindre_utils_2d PRIVATE SINDRE_OPENCV_HAS_XFEATURES2D=1)
        list(APPEND _sindre_opencv_targets "${_sindre_xfeatures2d_library}")
    endif()
else()
    set(_sindre_utils_2d_has_xfeatures2d ON)
    target_compile_definitions(sindre_utils_2d PRIVATE SINDRE_OPENCV_HAS_XFEATURES2D=1)
endif()
if(_sindre_opencv_targets)
    target_link_libraries(sindre_utils_2d PUBLIC sindre::general
        ${_sindre_opencv_targets})
else()
    # Older OpenCV package files expose only the legacy absolute library list.
    target_link_libraries(sindre_utils_2d PUBLIC sindre::general ${OpenCV_LIBS})
endif()
target_include_directories(sindre_utils_2d SYSTEM PUBLIC ${OpenCV_INCLUDE_DIRS})

# OpenCV's Windows SDK exports import libraries but does not make its DLL
# directory part of CTest's environment.  Keep the runtime lookup explicit so
# a freshly built test is runnable without a machine-wide PATH edit.
if(WIN32)
    set(_sindre_opencv_runtime_dir "")
    foreach(candidate IN ITEMS
        "${OpenCV_DIR}/../bin"
        "${OpenCV_DIR}/bin"
        "${OpenCV_INSTALL_PATH}/bin")
        if(IS_DIRECTORY "${candidate}")
            set(_sindre_opencv_runtime_dir "${candidate}")
            break()
        endif()
    endforeach()
    if(_sindre_opencv_runtime_dir)
        set_property(TARGET sindre_utils_2d PROPERTY SINDRE_RUNTIME_DIRS
            "${_sindre_opencv_runtime_dir}")
        sindre_install_runtime_dirs("${_sindre_opencv_runtime_dir}")
    endif()
endif()

if(SINDRE_BUILD_TESTS)
    add_executable(sindre_utils_2d_tests tests/utils_2d_tests.cpp)
    target_link_libraries(sindre_utils_2d_tests PRIVATE sindre::utils_2d)
    sindre_copy_target_runtime_dlls(sindre_utils_2d_tests)
    if(_sindre_opencv_runtime_dir)
        sindre_copy_runtime_dirs(sindre_utils_2d_tests "${_sindre_opencv_runtime_dir}")
    endif()
    add_test(NAME sindre.utils_2d COMMAND sindre_utils_2d_tests)

    add_executable(sindre_utils_2d_algorithms_tests tests/algorithms_tests.cpp)
    target_link_libraries(sindre_utils_2d_algorithms_tests PRIVATE sindre::utils_2d)
    if(_sindre_utils_2d_has_xfeatures2d)
        target_compile_definitions(sindre_utils_2d_algorithms_tests
            PRIVATE SINDRE_OPENCV_HAS_XFEATURES2D=1)
    endif()
    sindre_copy_target_runtime_dlls(sindre_utils_2d_algorithms_tests)
    if(_sindre_opencv_runtime_dir)
        sindre_copy_runtime_dirs(sindre_utils_2d_algorithms_tests "${_sindre_opencv_runtime_dir}")
    endif()
    add_test(NAME sindre.utils_2d.algorithms COMMAND sindre_utils_2d_algorithms_tests)
endif()
