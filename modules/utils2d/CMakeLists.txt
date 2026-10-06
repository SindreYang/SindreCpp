set(_sindrecpp_utils2d_include "${CMAKE_CURRENT_SOURCE_DIR}/include")
sindrecpp_add_interface_target(Utils2d "${_sindrecpp_utils2d_include}")
target_compile_definitions(SindreCpp_Utils2d INTERFACE SINDRECPP_WITH_UTILS2D=1)
include("${SINDRECPP_THIRDS_DIR}/utils2d/Dependencies.cmake")
find_package(OpenCV ${SINDRECPP_THIRD_UTILS2D_OPENCV_MIN_VERSION}
    REQUIRED COMPONENTS ${SINDRECPP_THIRD_UTILS2D_OPENCV_COMPONENTS})
target_link_libraries(SindreCpp_Utils2d INTERFACE ${OpenCV_LIBS})
target_include_directories(SindreCpp_Utils2d SYSTEM INTERFACE ${OpenCV_INCLUDE_DIRS})

if(SINDRECPP_BUILD_TESTS)
    add_executable(sindrecpp_utils2d_tests tests/utils2d_tests.cpp)
    target_link_libraries(sindrecpp_utils2d_tests PRIVATE SindreCpp::Utils2d)
    sindrecpp_copy_target_runtime_dlls(sindrecpp_utils2d_tests)
    add_test(NAME sindrecpp.utils2d COMMAND sindrecpp_utils2d_tests)
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
