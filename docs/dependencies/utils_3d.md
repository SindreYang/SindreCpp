# utils_3d dependencies

本文面向维护者，列出 Math、nanoflann、VTK 以及内部算法后端的要求；这些依赖不会
出现在 `utils_3d` 的公共 C++ API 中。

| Capability | Dependency | Source | Requirement |
| --- | --- | --- | --- |
| mesh/math core | [Math](../modules/math.md) | fixed `sindre::math` target | Eigen 3.4.1 + OpenBLAS 0.3.34 |
| nearest-neighbor search | nanoflann 1.8.0 | fixed source recipe in `3rdparty/nanoflann/` | header-only KD-tree backend |
| mesh I/O/filtering | VTK | installed official SDK or Linux system package | Windows VTK 9.7.1 exact; Linux VTK 9.1+ |
| internal mesh implementation | VTK | installed package | private module implementation; enabled by default |
| mesh algorithms | CGAL | official standalone SDK/config or Linux system package | Windows CGAL 6.2.1 exact; Linux CGAL 5.6+; no vcpkg CGAL package |
| internal point-cloud implementation | PCL | installed package/config or vcpkg | Windows 1.15.1 exact; Linux 1.14+; opt-in with `SINDRE_UTILS_3D_PCL` |
| historical backend record | MeshLib 3.1.4.297 | official SDK evaluated previously | retained for design history only; not built by the current target |

VTK and CGAL are external SDKs and are not downloaded by CMake. The required
Windows profile is VTK `9.7.1` plus CGAL `6.2.1`; configuration uses `EXACT`
package matching. Linux accepts the documented minimum versions and may use
the distribution packages. The VTK `vtk-config.cmake`
must be supplied through `VTK_DIR`, and the CGAL `CGALConfig.cmake` must be
supplied explicitly through `CGAL_DIR` or `SINDRE_UTILS_3D_CGAL_ROOT`.
On Windows CGAL's Boost/GMP/MPFR inputs must come from standalone SDKs; the
vcpkg CGAL package is not accepted and implicit `CMAKE_PREFIX_PATH` discovery
is disabled. On Linux CGAL may use the system Boost/GMP/MPFR packages, but a
vcpkg CGAL path is still rejected.
Add VTK's
`content/bin` and the official GMP/MPFR `bin` directory to `PATH` for
executables. Eigen is header-only and is supplied by Math's fixed source tree;
OpenBLAS is configured by the Math target.
The standalone Boost adapter requires `SINDRE_UTILS_3D_BOOST_ROOT` (or the
equivalent `BOOST_ROOT`/`Boost_ROOT`) explicitly. CMake then selects the
repository adapter and does not inspect `CMAKE_PREFIX_PATH` for Boost, so a
vcpkg Boost package cannot be selected accidentally as a transitive CGAL
dependency.
The installed `sindre` package repeats the platform-version and standalone-path
check for downstream CMake consumers. Windows keeps exact-version matching;
Linux uses the documented minimum versions. It also installs the small
`BoostStandalone` package adapter; installed consumers still provide
`Boost_ROOT`/`SINDRE_UTILS_3D_BOOST_ROOT` so the adapter can validate the
official standalone SDK without searching vcpkg or the host prefix.
On Windows, installation also copies the VTK runtime directory and official
GMP/MPFR DLLs into the package `bin/` directory. Applications should keep that
directory beside the executable or prepend it to `PATH`; the installed
consumer test uses the same runtime layout. The installed CMake package maps
VTK's `RelWithDebInfo` dependency lookup to its Release SDK while preserving
the consumer's own Sindre target configuration, so the default project
configuration remains usable by installed consumers.

The Windows installation procedure is documented in
[the 3D backend installation guide](../guides/vtk.md). PCL `1.15.1` remains an
opt-in point-cloud backend; VTK, CGAL and PCL are consumed from their SDKs and
no third-party SDK source tree is copied into this repository. nanoflann is the
one fixed source-only exception and is fetched into the external cache. CGAL cannot
be disabled or replaced by a fallback package. If its exact version or
the required minimum version or standalone SDK is unavailable, configuration
fails. Set `SINDRE_UTILS_3D_PCL=ON` only when deliberately changing
the default profile. When enabled, use `SINDRE_UTILS_3D_PCL_ROOT` for an SDK
or vcpkg installation root, or set `PCL_DIR` directly. The source and installed
targets request the same PCL components (`common`, `features`, `filters`,
`registration`, `search`, `segmentation`, `surface`) so a static consumer does
not lose a transitive PCL library at link time.

On Ubuntu 24.04, the validated system profile is VTK 9.1, CGAL 5.6, Boost
1.83, and GMP/MPFR from the distribution. CGAL 5.6 does not provide the newer
polygon curvature header used by CGAL 6.2; `get_curvature_by_cgal()` therefore
returns `function_not_supported` for that capability while the other CGAL
operations remain available. VTK 9.1 compatibility is handled in the
implementation.

The supported Windows vcpkg profile is `pcl:x64-windows-static` (PCL 1.15.1).
The Linux PCL 1.14+ path is also runtime-validated on Ubuntu 24.04 with
system PCL 1.14, Clang 18 and `libomp-18-dev`. The validation uses the same
public filtering, plane-segmentation and Euclidean-clustering APIs as the
source build and an installed-package consumer.
Because this binary profile is not built with AVX, PCL builds require
`SINDRE_MATH_NATIVE_ARCH=OFF`; CMake rejects the unsafe combination instead of
allowing an Eigen/PCL alignment ABI mismatch. CGAL's SSE4.1 requirement is
applied privately to Utils3D and does not enable AVX in Math or PCL.
The static Windows PCL profile also requires the LLVM OpenMP runtime when
consumed by clang-cl. The build locates `libomp.lib` beside the selected LLVM
toolchain, installs `libomp.dll` with the package, and recreates this
dependency in an installed consumer instead of leaving unresolved `__kmpc_*`
symbols. The Ubuntu PCL package uses the distribution OpenMP profile and was
validated with `libomp-18`.

Because Ubuntu's PCL/VTK package exports a `VTK::mpi` link interface containing
`MPI::MPI_C`, an installed Linux consumer that enables `utils_3d` with PCL must
enable both C and C++ in its CMake project and have the system MPI development
package available. This is a property of the distro PCL/VTK export, not a
dependency of General or Math.

nanoflann is the exception: it is a fixed header-only build dependency fetched
by the module's `3rdparty` recipe. The public `NearestNeighborIndex` API does
not expose nanoflann types.

Applications must include only `<sindre/utils_3d.h>`. They do not include VTK,
CGAL or PCL headers and do not select a backend at runtime.

Utils3D is not currently compatible with the strict
`SINDRE_NO_EXCEPTIONS=ON` profile: its mesh/image convenience APIs and private
VTK/CGAL adapters still use exceptions. CMake rejects this combination during
configuration; build Utils3D with normal C++ exception support.

The current implementation does not contain VCG, Open3D, or libigl adapters.
Their former CMake options, include-root fallbacks, package export entries, and
backend tests were removed together so an installed package cannot advertise
those backends.

MeshLib `3.1.4.297` is retained here as a historical dependency record. It was
previously evaluated as a C++20 backend, but the current `utils_3d` target does
not expose a MeshLib option, does not consume `SINDRE_MESHLIB_ROOT`, and does
not link or package MRMesh.
