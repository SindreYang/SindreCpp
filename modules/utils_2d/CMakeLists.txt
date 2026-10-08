sindre_add_static_target(utils_2d "${SINDRE_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils_2d.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/image.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/algorithms.cpp")
target_compile_definitions(sindre_utils_2d PUBLIC SINDRE_WITH_UTILS_2D=1)
include("${SINDRE_THIRDS_DIR}/utils_2d/Dependencies.cmake")
find_package(OpenCV CONFIG REQUIRED COMPONENTS ${SINDRE_THIRD_UTILS_2D_OPENCV_COMPONENTS})
if(NOT DEFINED OpenCV_VERSION)
    message(FATAL_ERROR "OpenCV package did not provide OpenCV_VERSION")
endif()
if(OpenCV_VERSION VERSION_LESS SINDRE_THIRD_UTILS_2D_OPENCV_MIN_VERSION)
    message(FATAL_ERROR
        "Utils_2d requires OpenCV >= ${SINDRE_THIRD_UTILS_2D_OPENCV_MIN_VERSION}; "
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
    sindre_copy_target_runtime_dlls(sindre_utils_2d_algorithms_tests)
    if(_sindre_opencv_runtime_dir)
        sindre_copy_runtime_dirs(sindre_utils_2d_algorithms_tests "${_sindre_opencv_runtime_dir}")
    endif()
    add_test(NAME sindre.utils_2d.algorithms COMMAND sindre_utils_2d_algorithms_tests)
endif()
