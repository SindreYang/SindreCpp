# utils_3d dependencies

本文面向维护者，列出 Math、VTK 以及内部算法后端的要求；这些依赖不会被自动下载，
也不会出现在 `utils_3d` 的公共 C++ API 中。

| Capability | Dependency | Source | Requirement |
| --- | --- | --- | --- |
| mesh/math core | [Math](../modules/math.md) | fixed `sindre::math` target | Eigen 3.4.1 + OpenBLAS 0.3.34 |
| mesh I/O/filtering | VTK | installed official SDK | VTK 9.7.1 verified |
| internal mesh implementation | VTK | installed package | private module implementation |
| mesh algorithms | CGAL 6.2.1 | installed package/config | enabled by default with `SINDRE_UTILS_3D_CGAL`; no source build |
| internal point-cloud implementation | PCL 1.15.1 | installed package/config | private module implementation |
| historical backend record | MeshLib 3.1.4.297 | official SDK evaluated previously | retained for design history only; not built by the current target |

VTK and geometry SDKs are not downloaded automatically. The verified Windows
package is the official VTK 9.7.1 SDK and its `vtk-config.cmake` is under the
SDK `cmake` directory. Add the SDK `content/bin` directory to `PATH` for
executables. Eigen is header-only and is supplied by Math's fixed source tree;
OpenBLAS is configured by the Math target.
The formal stable profile is CGAL `6.2.1` and PCL `1.15.1`. Both are discovered
from installed CMake packages and are linked only through the private implementation
of the `utils_3d` target;
no third-party source tree is copied into this repository and no backend is
fetched or compiled by this module. Set `SINDRE_UTILS_3D_CGAL=OFF` or
`SINDRE_UTILS_3D_PCL=OFF` only for a deliberate reduced-capability build.

Applications must include only `<sindre/utils_3d.h>`. They do not include VTK,
CGAL or PCL headers and do not select a backend at runtime.

The current implementation does not contain VCG, Open3D, or libigl adapters.
Their former CMake options, include-root fallbacks, package export entries, and
backend tests were removed together so an installed package cannot advertise
those backends.

MeshLib `3.1.4.297` is retained here as a historical dependency record. It was
previously evaluated as a C++20 backend, but the current `utils_3d` target does
not expose a MeshLib option, does not consume `SINDRE_MESHLIB_ROOT`, and does
not link or package MRMesh.
