# GUI dependencies

本文面向启用 GUI 的应用，说明 ImGui、GLFW 和 OpenGL 的来源与平台要求；窗口系统
的运行时仍由宿主应用负责。

| Feature | Dependency | Source | Version/requirement |
| --- | --- | --- | --- |
| widgets | [Dear ImGui](https://github.com/ocornut/imgui) | fixed source archive with SHA256 | `v1.92.9b` |
| image loading | stb_image | fixed source archive with SHA256 | pinned commit |
| GLFW backend | GLFW 3.4 | fixed source archive with SHA256 | private `glfw` target |
| OpenGL backend | OpenGL | system SDK | `OpenGL::GL` |

Dear ImGui is downloaded only when GUI is enabled from the fixed archive recipe in
`3rdparty/imgui/imgui.cmake`; it does not use vcpkg or an unpinned Git checkout.
GLFW is built from the fixed source recipe in `3rdparty/glfw/glfw.cmake`; it does
not use vcpkg or an unpinned system package. OpenGL remains a system dependency
because its platform runtime and window-system integration belong to the host
application. The install tree includes the ImGui and, when enabled, stb_image
headers needed by the public GUI target. The fixed source recipe is the only
accepted source; system and vcpkg replacements are rejected.
The fixed GLFW archive and static target are also exported with the install;
an installed `find_package(sindre CONFIG REQUIRED COMPONENTS gui)` therefore
does not rediscover GLFW from vcpkg or the host system.

The archive version and hash are intentionally fixed; changing the version
requires updating the recipe and its verification record.
