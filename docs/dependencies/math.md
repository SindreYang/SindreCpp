# Math dependencies

Math has fixed dependencies because it is a foundation module:

| Dependency | Version | Location | Role |
| --- | --- | --- | --- |
| Eigen | 3.4.1 | `thirds/math/eigen/` | Matrix, array, geometry, fixed-rank Tensor and expression backend |
| OpenBLAS | 0.3.34 | `thirds/math/openblas/` | Default BLAS backend and Windows runtime |

The versions and package metadata are recorded in
`thirds/math/Dependencies.cmake`. The source configuration fails when the
default fixed OpenBLAS root is missing. Set
`SINDRE_MATH_OPENBLAS_ROOT` to another explicitly provisioned OpenBLAS SDK,
or select `SINDRE_MATH_BLAS_BACKEND=EIGEN` for a build that intentionally does
not use BLAS.

Eigen and OpenBLAS belong to Math, not General. Consumers should link
`sindre::math` when they need the numerical API; they should not discover or
link these dependencies independently.
