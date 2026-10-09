# GUI 模块只在显式启用时构建，负责 ImGui、GLFW/OpenGL3 和 stb_image 的组合。
# GUI 的第三方运行时不向 General 或默认模块传播。
set(SINDRE_GUI_GLFW_BUNDLED OFF)
sindre_add_static_target(gui "${SINDRE_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gui.cpp")
target_compile_definitions(sindre_gui PUBLIC
    SINDRE_WITH_GUI=1)
target_link_libraries(sindre_gui PUBLIC sindre::general)

sindre_3rdparty_setup_imgui("${SINDRE_IMGUI_VERSION}")
set(SINDRE_IMGUI_SOURCE_DIR "${SINDRE_THIRD_GUI_IMGUI_SOURCE_DIR}")

if(SINDRE_GUI_STB_IMAGE)
    sindre_3rdparty_setup_stb()
    find_path(_sindre_stb_image_include stb_image.h
        HINTS
            "${SINDRE_THIRD_GUI_STB_SOURCE_DIR}"
            "${SINDRE_IMGUI_SOURCE_DIR}/examples/example_glfw_opengl3"
            "${SINDRE_IMGUI_SOURCE_DIR}/examples/example_sdl2_opengl3"
        NO_DEFAULT_PATH)
    if(NOT _sindre_stb_image_include)
        message(FATAL_ERROR
            "SINDRE_GUI_STB_IMAGE requires the fixed stb source recipe to "
            "provide stb_image.h")
    endif()
    target_compile_definitions(sindre_gui PUBLIC SINDRE_GUI_STB_IMAGE=1)
    target_include_directories(sindre_gui SYSTEM PUBLIC
        "$<BUILD_INTERFACE:${_sindre_stb_image_include}>"
        "$<INSTALL_INTERFACE:include/sindre/third_party/stb>")
endif()

set(_sindre_imgui_sources
    "${SINDRE_IMGUI_SOURCE_DIR}/imgui.cpp"
    "${SINDRE_IMGUI_SOURCE_DIR}/imgui_draw.cpp"
    "${SINDRE_IMGUI_SOURCE_DIR}/imgui_tables.cpp"
    "${SINDRE_IMGUI_SOURCE_DIR}/imgui_widgets.cpp")
if(SINDRE_GUI_GLFW_OPENGL3)
    list(APPEND _sindre_imgui_sources
        "${SINDRE_IMGUI_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
        "${SINDRE_IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp")
endif()
add_library(sindre_imgui STATIC ${_sindre_imgui_sources})
set_target_properties(sindre_imgui PROPERTIES EXPORT_NAME imgui)
target_compile_features(sindre_imgui PUBLIC cxx_std_17)
target_include_directories(sindre_imgui SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${SINDRE_IMGUI_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${SINDRE_IMGUI_SOURCE_DIR}/backends>"
    "$<INSTALL_INTERFACE:include/sindre/third_party/imgui>"
    "$<INSTALL_INTERFACE:include/sindre/third_party/imgui/backends>")
target_link_libraries(sindre_gui PUBLIC sindre_imgui)
install(TARGETS sindre_imgui EXPORT sindreTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

file(GLOB _sindre_imgui_root_headers CONFIGURE_DEPENDS
    "${SINDRE_IMGUI_SOURCE_DIR}/*.h")
install(FILES ${_sindre_imgui_root_headers}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sindre/third_party/imgui")
install(DIRECTORY "${SINDRE_IMGUI_SOURCE_DIR}/backends/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sindre/third_party/imgui/backends"
    FILES_MATCHING PATTERN "*.h")
if(SINDRE_GUI_STB_IMAGE)
    install(FILES "${_sindre_stb_image_include}/stb_image.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sindre/third_party/stb")
endif()

if(SINDRE_GUI_GLFW_OPENGL3)
    sindre_3rdparty_setup_glfw()
    if(TARGET glfw)
        set(_sindre_glfw_target glfw)
    elseif(TARGET glfw3::glfw)
        set(_sindre_glfw_target glfw3::glfw)
    else()
        message(FATAL_ERROR "Pinned GLFW did not provide a usable target")
    endif()
    find_package(OpenGL REQUIRED)
    if(WIN32)
        set(_sindre_opengl_target opengl32)
    else()
        set(_sindre_opengl_target OpenGL::GL)
    endif()
    target_link_libraries(sindre_imgui PUBLIC ${_sindre_glfw_target}
        ${_sindre_opengl_target})
    if(TARGET glfw)
        # The static ImGui backend has a real link-time dependency on GLFW.
        # Export the fixed in-tree GLFW target with the GUI package so an
        # installed consumer does not need to rediscover a system/vcpkg GLFW.
        install(TARGETS glfw EXPORT sindreTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        set(SINDRE_GUI_GLFW_BUNDLED ON)
    endif()
    target_compile_definitions(sindre_gui PUBLIC SINDRE_GUI_GLFW_OPENGL3=1)
endif()

set(SINDRE_GUI_GLFW_BUNDLED "${SINDRE_GUI_GLFW_BUNDLED}" PARENT_SCOPE)

if(SINDRE_BUILD_GUI_RUNTIME_TESTS)
    target_compile_definitions(sindre_gui PUBLIC SINDRE_GUI_RUNTIME_TEST=1)
endif()

if(SINDRE_BUILD_TESTS)
    add_executable(sindre_gui_tests tests/gui_tests.cpp)
    target_link_libraries(sindre_gui_tests PRIVATE sindre::gui)
    sindre_copy_target_runtime_dlls(sindre_gui_tests)
    add_test(NAME sindre.gui COMMAND sindre_gui_tests)
endif()
