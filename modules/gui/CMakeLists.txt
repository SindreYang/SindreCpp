sindre_add_static_target(gui "${SINDRE_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gui.cpp")
include("${SINDRE_THIRDS_DIR}/gui/Dependencies.cmake")
target_compile_definitions(sindre_gui PUBLIC
    SINDRE_WITH_GUI=1)
target_link_libraries(sindre_gui PUBLIC sindre::general)

if(NOT SINDRE_IMGUI_SOURCE_DIR)
    sindre_thirds_declare_git(imgui
        "${SINDRE_THIRD_GUI_IMGUI_REPOSITORY}"
        "v${SINDRE_IMGUI_VERSION}")
    FetchContent_GetProperties(imgui)
    if(NOT imgui_POPULATED)
        FetchContent_Populate(imgui)
    endif()
    set(SINDRE_IMGUI_SOURCE_DIR "${imgui_SOURCE_DIR}")
endif()

if(SINDRE_GUI_STB_IMAGE)
    find_path(_sindre_stb_image_include stb_image.h
        HINTS
            "${SINDRE_STB_IMAGE_ROOT}"
            "${SINDRE_STB_IMAGE_ROOT}/include"
            "${SINDRE_IMGUI_SOURCE_DIR}/examples/example_glfw_opengl3"
            "${SINDRE_IMGUI_SOURCE_DIR}/examples/example_sdl2_opengl3")
    if(NOT _sindre_stb_image_include)
        message(FATAL_ERROR
            "SINDRE_GUI_STB_IMAGE requires stb_image.h; set SINDRE_STB_IMAGE_ROOT or install stb.")
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
install(TARGETS sindre_imgui EXPORT sindrecppTargets
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
    find_package(glfw3 CONFIG QUIET)
    if(TARGET glfw)
        set(_sindre_glfw_target glfw)
    elseif(TARGET glfw3::glfw)
        set(_sindre_glfw_target glfw3::glfw)
    else()
        find_package(glfw3 REQUIRED)
        if(TARGET glfw)
            set(_sindre_glfw_target glfw)
        else()
            set(_sindre_glfw_target glfw3::glfw)
        endif()
    endif()
    find_package(OpenGL REQUIRED)
    target_link_libraries(sindre_imgui PUBLIC ${_sindre_glfw_target} OpenGL::GL)
    target_compile_definitions(sindre_gui PUBLIC SINDRE_GUI_GLFW_OPENGL3=1)
endif()

if(SINDRE_BUILD_GUI_RUNTIME_TESTS)
    target_compile_definitions(sindre_gui PUBLIC SINDRE_GUI_RUNTIME_TEST=1)
endif()

if(SINDRE_BUILD_TESTS)
    add_executable(sindre_gui_tests tests/gui_tests.cpp)
    target_link_libraries(sindre_gui_tests PRIVATE sindre::gui)
    sindre_copy_target_runtime_dlls(sindre_gui_tests)
    add_test(NAME sindre.gui COMMAND sindre_gui_tests)
endif()
