# Math module

`sindre::math` is the fixed linear-algebra foundation between `sindre::general`
and the numerical utility modules. It is enabled and built by default.

```cmake
target_link_libraries(my_app PRIVATE sindre::math)
```

```cpp
#include <sindre/math.h>

sindre::math::MatrixXd matrix(128, 128);
matrix.setZero();
sindre::math::Vector3 point(1.0, 2.0, 3.0);
```

The Sindre aliases use Eigen as their implementation backend, but their own
matrix and array aliases are row-major. `MatrixXd`, `MatrixXf`, `ArrayXd`,
`ArrayXf`, and fixed matrices are therefore contiguous in row order. `VectorXd`
and `VectorXf` are dynamic column vectors. The commonly used float aliases are
`Vector3f` and `Matrix4f`. `Quaternion`/`Quaternionf` and `Transform3` are the
standard Eigen geometry types exposed by Math.

## Tensor and PyTorch-style layout

`sindre/math.h` also exposes the fixed-rank Eigen Tensor API through
`sindre::math::Tensor<T, Rank>`. Tensor values are row-major by default, so
the last axis is contiguous in memory. The named image aliases use the
PyTorch axis convention:

| Alias | Axis order |
| --- | --- |
| `sindre::math::tensor::CHW<T>` | channel, height, width |
| `sindre::math::tensor::NCHW<T>` | batch, channel, height, width |
| `sindre::math::tensor::NCTHW<T>` | batch, channel, time, height, width |

```cpp
using Image = sindre::math::tensor::CHW<float>;
Image image(3, 224, 224);  // C, H, W
image(0, 10, 20) = 1.0f;

using Dimensions = sindre::math::TensorDimensions<3>;
const Dimensions offsets{0, 10, 20};
const Dimensions extents{3, 32, 32};
Image patch = image.slice(offsets, extents);
```

Eigen Tensor slicing and broadcasting are expression operations. Assignment
to another Tensor evaluates the expression:

```cpp
const Dimensions factors{1, 2, 2};
Image enlarged = image.broadcast(factors);
```

`broadcast()` receives repeat factors, not a target shape. Tensor Rank is a
compile-time parameter while dimension sizes are runtime values. For a
runtime-variable Rank or Python-style indexing syntax, use a future
`NdArray` layer rather than treating `Tensor<T, Rank>` as a dynamic array.

`TensorMap<T, Rank>` maps existing contiguous memory without taking ownership;
the source memory must outlive the map. Advanced Tensor operations remain
available through the fixed Eigen backend and are not duplicated by Math.

## std::vector interoperability

Conversions always copy data and never borrow the input buffer:

```cpp
std::vector<double> values{1, 2, 3, 4, 5, 6};

auto matrix = sindre::math::to_matrix(values, 2, 3);
auto vector = sindre::math::to_vector(values);
if (matrix && vector) {
    auto flat = sindre::math::to_std_vector(matrix.value());
}
```

`to_matrix()` checks dimensions and returns `Result<Matrix<T>>` on failure.
`to_vector()` returns a dynamic column vector through `Result<Vector<T>>`.
Matrix and array values are exported by `to_std_vector()` in logical row-major
order, regardless of the Eigen storage order of the input expression.

Use `sindre::math::eigen` only when an API specifically needs a native Eigen
type or Eigen algorithm. That namespace is exactly an alias of `::Eigen`; its
native defaults, including `Eigen::MatrixXd` column-major storage, are not
rewritten by Sindre.

Math deliberately does not duplicate Eigen arithmetic, decompositions, or
geometry algorithms. Use the normal Eigen expression API through the Sindre
aliases. Geometry algorithms remain in `sindre::utils_3d`.

## Backend and options

Math uses the fixed Eigen 3.4.1 source and OpenBLAS 0.3.34 profile registered
under `thirds/math/`. OpenBLAS is the default backend and is not silently
replaced by a system BLAS. The explicit fallback for platforms without the
fixed OpenBLAS profile is:

```text
-DSINDRE_MATH_BLAS_BACKEND=EIGEN
```

`SINDRE_MATH_NATIVE_ARCH` controls local CPU tuning and
`SINDRE_MATH_OPENBLAS_ROOT` selects the fixed OpenBLAS root. `AUTO`, MKL, and
unregistered BLAS backends are not supported.

On Windows, Math consumers and tests copy the selected OpenBLAS DLL next to
the executable through the common runtime-copy helper.

## Boundary

General does not include Eigen and can be consumed without Math. Utilities
that expose matrices link Math explicitly or transitively. There is no
`sindre::general::eigen` compatibility namespace and no General-level matrix
mapping helper.
