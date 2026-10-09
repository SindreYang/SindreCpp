# Math dependencies

Math has fixed dependencies because it is a foundation module:

| Dependency | Version | Location | Role |
| --- | --- | --- | --- |
| Eigen | 3.4.1 | external cache under `SINDRE_THIRD_PARTY_CACHE_DIR/math/eigen/` | Matrix, array, geometry, fixed-rank Tensor and expression backend |
| OpenBLAS | 0.3.34 | Open3D-style `ExternalProject_Add` with external download cache | Default BLAS backend |

The versions and package metadata are recorded in
`3rdparty/openblas/openblas.cmake`. OpenBLAS is always built through the pinned
Open3D-style `ExternalProject_Add` recipe, using a fixed URL and SHA256,
an external download cache, and an isolated build/install directory. Select
`SINDRE_MATH_BLAS_BACKEND=EIGEN` for a build that intentionally does not use
BLAS.

On Windows, the fixed OpenBLAS profile requires the Visual Studio LLVM
`clang-cl` compiler and MSVC ABI. GNU-style `clang`/`clang++` is rejected at
configuration time instead of being allowed to fail later inside OpenBLAS.

Eigen and OpenBLAS belong to Math, not General. Consumers should link
`sindre::math` when they need the numerical API; they should not discover or
link these dependencies independently.

The install package carries the exact OpenBLAS static archive and generated
headers used by the build. An installed Math consumer therefore does not need
an OpenBLAS CMake package, system BLAS, or vcpkg; only the selected large SDKs
of other modules remain external.
