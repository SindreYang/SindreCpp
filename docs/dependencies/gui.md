# GUI dependencies

本文面向启用 GUI 的应用，说明 ImGui、GLFW 和 OpenGL 的来源与平台要求；窗口系统
的运行时仍由宿主应用负责。

| Feature | Dependency | Source | Version/requirement |
| --- | --- | --- | --- |
| widgets | [Dear ImGui](https://github.com/ocornut/imgui) | Git | `v${SINDRE_IMGUI_VERSION}` |
| image loading | stb_image | installed header or `SINDRE_STB_IMAGE_ROOT` | `stb_image.h` |
| GLFW backend | GLFW 3 | installed CMake package | `glfw3` target |
| OpenGL backend | OpenGL | system SDK | `OpenGL::GL` |

Dear ImGui is fetched only when GUI is enabled. GLFW and OpenGL stay system/package
dependencies because their platform runtime and window-system integration belong to
the host application. The install tree includes the ImGui and, when enabled,
stb_image headers needed by the public GUI target; GLFW/OpenGL are rediscovered by
the generated package configuration.

Dear ImGui can be pinned to a local source tree with
`SINDRE_IMGUI_SOURCE_DIR`; the verified vcpkg source was 1.90.6. With clang-cl
and vcpkg, supplying `CMAKE_PREFIX_PATH` to the `x64-windows` installed tree is
more reliable than relying on vcpkg's architecture inference.
