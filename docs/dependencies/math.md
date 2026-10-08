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

Eigen and OpenBLAS belong to Math, not General. Consumers should link
`sindre::math` when they need the numerical API; they should not discover or
link these dependencies independently.
