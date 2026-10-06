set(_sindrecpp_gui_include "${CMAKE_CURRENT_SOURCE_DIR}/include")
sindrecpp_add_interface_target(Gui "${_sindrecpp_gui_include}")
include("${SINDRECPP_THIRDS_DIR}/gui/Dependencies.cmake")
target_compile_definitions(SindreCpp_Gui INTERFACE
    SINDRECPP_WITH_GUI=1)
target_link_libraries(SindreCpp_Gui INTERFACE SindreCpp::General)

if(NOT SINDRECPP_IMGUI_SOURCE_DIR)
    sindrecpp_thirds_declare_git(imgui
        "${SINDRECPP_THIRD_GUI_IMGUI_REPOSITORY}"
        "v${SINDRECPP_IMGUI_VERSION}")
    FetchContent_GetProperties(imgui)
    if(NOT imgui_POPULATED)
        FetchContent_Populate(imgui)
    endif()
    set(SINDRECPP_IMGUI_SOURCE_DIR "${imgui_SOURCE_DIR}")
endif()

set(_sindrecpp_imgui_sources
    "${SINDRECPP_IMGUI_SOURCE_DIR}/imgui.cpp"
    "${SINDRECPP_IMGUI_SOURCE_DIR}/imgui_draw.cpp"
    "${SINDRECPP_IMGUI_SOURCE_DIR}/imgui_tables.cpp"
    "${SINDRECPP_IMGUI_SOURCE_DIR}/imgui_widgets.cpp")
if(SINDRECPP_GUI_GLFW_OPENGL3)
    list(APPEND _sindrecpp_imgui_sources
        "${SINDRECPP_IMGUI_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
        "${SINDRECPP_IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp")
endif()
add_library(sindrecpp_imgui STATIC ${_sindrecpp_imgui_sources})
set_target_properties(sindrecpp_imgui PROPERTIES EXPORT_NAME imgui)
target_compile_features(sindrecpp_imgui PUBLIC cxx_std_17)
target_include_directories(sindrecpp_imgui SYSTEM PUBLIC
    "${SINDRECPP_IMGUI_SOURCE_DIR}"
    "${SINDRECPP_IMGUI_SOURCE_DIR}/backends")
target_link_libraries(SindreCpp_Gui INTERFACE sindrecpp_imgui)
install(TARGETS sindrecpp_imgui EXPORT SindreCppTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(SINDRECPP_GUI_GLFW_OPENGL3)
    find_package(glfw3 CONFIG QUIET)
    if(TARGET glfw)
        set(_sindrecpp_glfw_target glfw)
    elseif(TARGET glfw3::glfw)
        set(_sindrecpp_glfw_target glfw3::glfw)
    else()
        find_package(glfw3 REQUIRED)
        if(TARGET glfw)
            set(_sindrecpp_glfw_target glfw)
        else()
            set(_sindrecpp_glfw_target glfw3::glfw)
        endif()
    endif()
    find_package(OpenGL REQUIRED)
    target_link_libraries(sindrecpp_imgui PUBLIC ${_sindrecpp_glfw_target} OpenGL::GL)
    target_compile_definitions(SindreCpp_Gui INTERFACE SINDRECPP_GUI_GLFW_OPENGL3=1)
endif()

if(SINDRECPP_BUILD_GUI_RUNTIME_TESTS)
    target_compile_definitions(SindreCpp_Gui INTERFACE SINDRECPP_GUI_RUNTIME_TEST=1)
endif()

if(SINDRECPP_BUILD_TESTS)
    add_executable(sindrecpp_gui_tests tests/utils_gui_tests.cpp)
    target_link_libraries(sindrecpp_gui_tests PRIVATE SindreCpp::Gui)
    sindrecpp_copy_target_runtime_dlls(sindrecpp_gui_tests)
    add_test(NAME sindrecpp.gui COMMAND sindrecpp_gui_tests)
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
