# GUI module

The module provides ImGui context helpers, CJK font lookup, image assets and
cache helpers, texture upload callbacks, and an optional GLFW/OpenGL3
`GuiApplication`.

Configure with `SINDRECPP_WITH_GUI=ON`. Dear ImGui is fetched unless
`SINDRECPP_IMGUI_SOURCE_DIR` points at an existing source tree. Set
`SINDRECPP_GUI_GLFW_OPENGL3=OFF` for the headless helper surface. The module
test is `sindrecpp.gui`; runtime window testing is opt-in via
`SINDRECPP_BUILD_GUI_RUNTIME_TESTS`.
