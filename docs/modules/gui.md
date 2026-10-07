# GUI module

本文面向需要 ImGui 上下文、字体、图片或 GLFW/OpenGL3 窗口封装的使用者，说明 GUI
target 的编译开关和运行时边界；GUI 依赖见 [GUI 依赖说明](../dependencies/gui.md)。

`sindre::gui` is a static library target, not a header-only module. Dear ImGui,
GLFW/OpenGL, stb_image and the GUI wrappers are compiled once by the module;
consumer translation units only include the public declarations and link the target.

The module provides ImGui context helpers, CJK font lookup, image assets and
cache helpers, texture upload callbacks, and an optional GLFW/OpenGL3
`GuiApplication`.

The complete Dear ImGui API is also exposed as `sindre::gui::imgui`; for example,
applications can call `sindre::gui::imgui::Begin(...)` and use the regular ImGui
types directly. `sindre::gui::native` remains as a compatibility alias.

Configure with `SINDRE_WITH_GUI=ON`. Dear ImGui is fetched unless
`SINDRE_IMGUI_SOURCE_DIR` points at an existing source tree. Image decoding is
controlled independently by `SINDRE_GUI_STB_IMAGE` (and can use
`SINDRE_STB_IMAGE_ROOT`). Set `SINDRE_GUI_GLFW_OPENGL3=OFF` for the headless helper surface. The module
test is `sindre.gui`; runtime window testing is opt-in via
`SINDRE_BUILD_GUI_RUNTIME_TESTS`.

The Windows verification used the vcpkg-provided Dear ImGui 1.90.6 source,
GLFW 3.4 and stb headers. The optional runtime test created a real GLFW/OpenGL
window, ran an ImGui frame and closed it successfully; it is not a substitute
for validating an application's own renderer and event loop.
