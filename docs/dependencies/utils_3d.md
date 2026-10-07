# utils_3d dependencies

本文面向启用 3D 能力的项目，列出 Eigen、VTK 以及可选几何后端的要求；VTK 和
大型几何 SDK 不会被自动下载。

| Capability | Dependency | Source | Requirement |
| --- | --- | --- | --- |
| mesh/math core | [Eigen](https://gitlab.com/libeigen/eigen) | project-level dependency from General | `3.4.1` fixed source fallback |
| mesh I/O/filtering | VTK | installed official SDK | VTK 9.7.1 verified |
| rendering/data | VTK | installed package | controlled by `SINDRE_UTILS_3D_SHOW` and `SINDRE_UTILS_3D_VTK_DATA` |
| optional algorithms | MeshLib, CGAL, Open3D, libigl, VCG, BLAS | installed SDK/package | opt-in CMake options; no source build |

VTK and geometry SDKs are not downloaded automatically. The verified Windows
package is the official VTK 9.7.1 SDK and its `vtk-config.cmake` is under the
SDK `cmake` directory. Add the SDK `content/bin` directory to `PATH` for
executables. Eigen is header-only and is supplied by General's fixed source
tree when no `Eigen3::Eigen` package target is available.
Optional backend roots must expose only the include paths and targets needed by
the selected feature. The fixed validation profile is CGAL `5.6.1`, the vcpkg
libigl port `2.5.0` (its installed CMake package reports upstream package
version `2.4.0`), MeshLib SDK `3.1.4.297`, and Open3D SDK `0.19.0`.
CGAL and libigl were found from the local vcpkg `x64-windows` installation;
Open3D and MeshLib were not present in that vcpkg registry and require their
official extracted SDK roots via `SINDRE_OPEN3D_ROOT` and
`SINDRE_MESHLIB_ROOT`. No optional backend is fetched or compiled by this
module.

The tested Windows combination is MSVC + CGAL 5.6.1 + libigl 2.5.0. The same
CGAL profile fails during CGAL's own iterator headers under clang-cl, so that
combination is explicitly unverified and CMake emits a warning. libigl's
source-root fallback is private to the library build and is not leaked into an
installed consumer; an installed libigl package is re-discovered when the
package target was used.
