# utils_3d dependencies

本文面向启用 3D 能力的项目，列出 Math、VTK 以及可选几何后端的要求；VTK 和
大型几何 SDK 不会被自动下载。

| Capability | Dependency | Source | Requirement |
| --- | --- | --- | --- |
| mesh/math core | [Math](../modules/math.md) | fixed `sindre::math` target | Eigen 3.4.1 + OpenBLAS 0.3.34 |
| mesh I/O/filtering | VTK | installed official SDK | VTK 9.7.1 verified |
| rendering/data | VTK | installed package | controlled by `SINDRE_UTILS_3D_SHOW` and `SINDRE_UTILS_3D_VTK_DATA` |
| optional algorithms | MeshLib, CGAL, Open3D, libigl, VCG | installed SDK/package or user-provided include root | opt-in CMake options; no source build |

VTK and geometry SDKs are not downloaded automatically. The verified Windows
package is the official VTK 9.7.1 SDK and its `vtk-config.cmake` is under the
SDK `cmake` directory. Add the SDK `content/bin` directory to `PATH` for
executables. Eigen is header-only and is supplied by Math's fixed source tree;
OpenBLAS is configured by the Math target.
Optional backend roots must expose only the include paths and targets needed by
the selected feature. The fixed validation profile is CGAL `5.6.1`, the vcpkg
libigl port `2.5.0` (its installed CMake package reports upstream package
version `2.4.0`), MeshLib SDK `3.1.4.297`, and Open3D SDK `0.20.0`.
CGAL and libigl were found from the local vcpkg `x64-windows` installation;
Open3D and MeshLib were not present in that vcpkg registry and use their
official extracted SDK roots via `SINDRE_OPEN3D_ROOT` and
`SINDRE_MESHLIB_ROOT`. Open3D is consumed through its official CMake config;
MeshLib's official Windows package has no config, so sindrecpp creates an
imported `MeshLib::MRMesh` target from `include/MRMesh`, `lib/Release/MRMesh.lib`
and `app/Release/MRMesh.dll`. No optional backend is fetched or compiled by
this module.

The tested Windows combinations are MSVC + CGAL 5.6.1 + libigl 2.5.0,
MSVC + official Open3D 0.20.0, and MSVC + official MeshLib 3.1.4.297. The same
CGAL profile fails during CGAL's own iterator headers under clang-cl, so that
combination is explicitly unverified and CMake emits a warning. libigl's
source-root fallback is private to the library build and is not leaked into an
installed consumer; an installed libigl package is re-discovered when the
package target was used. MeshLib 3.1.4 requires C++20; enabling that backend
raises only `sindre::utils_3d`'s target requirement, while the repository
baseline remains C++17. VCG is not part of the fixed binary validation profile;
it remains an explicit source/include-root adapter through `SINDRE_VCG_ROOT`.
