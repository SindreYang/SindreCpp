# GUI module

本文面向需要 ImGui 上下文、字体、图片或 GLFW/OpenGL3 窗口封装的使用者，说明 GUI
target 的编译开关和运行时边界；GUI 依赖见 [GUI 依赖说明](../dependencies/gui.md)。

`sindre::gui` is a static library target, not a header-only module. Dear ImGui,
GLFW/OpenGL, stb_image and the GUI wrappers are compiled once by the module;
consumer translation units only include the public declarations and link the target.

The module provides an opaque ImGui context, CJK font lookup, image assets and
cache helpers, texture upload callbacks, and an optional GLFW/OpenGL3
`GuiApplication`. Dear ImGui and GLFW types remain private to the implementation;
the public facade uses `Vec2`, `Color`, `TextureHandle`, and `InputFlags`.

For a single-window application, the short lifecycle API is usually enough:

```cpp
sindre::gui::GuiConfig config;
config.title = "sindre";
config.width = 1280;
config.height = 720;

auto initialized = sindre::gui::gui_init(config);
if (!initialized) return 1;

while (auto frame = sindre::gui::gui_begin()) {
    std::string text = "Hello Sindre";
    sindre::gui::input_text("Main", text);
    sindre::gui::help_marker("中文字体会自动选择");
}

sindre::gui::gui_shutdown();
```

`gui_begin()` polls events and starts a frame. Its returned RAII object renders and
submits the frame when it leaves scope. Closing the window is reported as a failed
frame with `operation_canceled`; other failures retain their `Result` error. The
configuration fields have usable defaults, so an application can simply call
`gui_init()` for the default window. `GuiApplication` remains available when an
application needs manual frame control. Because the GLFW/OpenGL3 ImGui backends
own process-level state, only one `GuiApplication` (including the `gui_init()`
global application) may be active at a time; destroy it before creating another.

Configure with `SINDRE_WITH_GUI=ON`. Dear ImGui 1.92.9b, GLFW 3.4 and stb_image
are fetched from fixed, SHA256-verified source recipes; system and vcpkg
replacements are rejected. Image decoding is controlled independently by
`SINDRE_GUI_STB_IMAGE`. Set `SINDRE_GUI_GLFW_OPENGL3=OFF` for the headless helper surface. The module
test is `sindre.gui`; runtime window testing is opt-in via
`SINDRE_BUILD_GUI_RUNTIME_TESTS`.

The optional runtime test creates a real GLFW/OpenGL window, runs an ImGui frame
and closes it successfully; it is not a substitute for validating an
application's own renderer and event loop.
