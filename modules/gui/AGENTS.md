# GUI module guidance

GUI owns Dear ImGui helpers and the optional GLFW/OpenGL3 application backend.
Include `gui/index.hpp` and link `SindreCpp::Gui`.

GUI depends on General for paths, system information, Result, and scope guards.
The `SINDRECPP_GUI_GLFW_OPENGL3` option controls backend sources and runtime
dependencies. Keep headless image/font tests independent of creating a window.
