# Utils3d dependencies

| Capability | Dependency | Source | Requirement |
| --- | --- | --- | --- |
| mesh/math core | [Eigen](https://gitlab.com/libeigen/eigen) | installed package or Git fallback | `5.0.1` fallback |
| mesh I/O/filtering | VTK | installed package | VTK 9 |
| rendering/data | VTK | installed package | controlled by `SINDRECPP_UTILS3D_SHOW` and `SINDRECPP_UTILS3D_VTK_DATA` |
| optional algorithms | MeshLib, CGAL, Open3D, libigl, VCG, BLAS | installed SDK/package | opt-in CMake options |

VTK and geometry SDKs are not downloaded automatically. Eigen is header-only and
has a small Git fallback when no `Eigen3::Eigen` package target is available.
Optional backend roots must expose only the include paths and targets needed by
the selected feature.
