# GUI dependencies

| Feature | Dependency | Source | Version/requirement |
| --- | --- | --- | --- |
| widgets | [Dear ImGui](https://github.com/ocornut/imgui) | Git | `v${SINDRECPP_IMGUI_VERSION}` |
| GLFW backend | GLFW 3 | installed CMake package | `glfw3` target |
| OpenGL backend | OpenGL | system SDK | `OpenGL::GL` |

Dear ImGui is fetched only when GUI is enabled. GLFW and OpenGL stay system/package
dependencies because their platform runtime and window-system integration belong to
the host application.
