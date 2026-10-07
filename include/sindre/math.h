#pragma once

/// @file
/// @brief Sindre's fixed Eigen-backed math types.

#if !defined(SINDRE_WITH_MATH)
#error "Enable sindre::math through CMake before including this header."
#endif

#include <sindre/general/core.h>
#include <sindre/math/tensor.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace sindre::math {

using Index = Eigen::Index;

template <
    class Scalar,
    int Rows = Eigen::Dynamic,
    int Cols = Eigen::Dynamic>
using Matrix = Eigen::Matrix<Scalar, Rows, Cols, Eigen::RowMajor>;

template <
    class Scalar,
    int Rows = Eigen::Dynamic,
    int Cols = Eigen::Dynamic>
using Array = Eigen::Array<Scalar, Rows, Cols, Eigen::RowMajor>;

template <class Scalar>
using Vector = Eigen::Matrix<Scalar, Eigen::Dynamic, 1>;

// Eigen does not permit RowMajor for a column vector.  Keep vectors in
// Eigen's native column-major layout while matrices and arrays remain row-major.
using Vector2 = Eigen::Matrix<double, 2, 1>;
using Vector3 = Eigen::Matrix<double, 3, 1>;
using Vector4 = Eigen::Matrix<double, 4, 1>;

using Matrix2 = Matrix<double, 2, 2>;
using Matrix3 = Matrix<double, 3, 3>;
using Matrix4 = Matrix<double, 4, 4>;
using Matrix4f = Matrix<float, 4, 4>;

using MatrixXf = Matrix<float>;
using MatrixXd = Matrix<double>;

using Vector3f = Eigen::Matrix<float, 3, 1>;
using VectorXf = Vector<float>;
using VectorXd = Vector<double>;

using ArrayXf = Array<float>;
using ArrayXd = Array<double>;

using Quaternion = Eigen::Quaterniond;
using Quaternionf = Eigen::Quaternionf;
using Transform3 = Eigen::Transform<double, 3, Eigen::Affine>;

/// @brief 将 std::vector 拷贝为行主序动态矩阵。
template <class Scalar>
::sindre::general::Result<Matrix<Scalar>>
to_matrix(const std::vector<Scalar> &values, Index rows, Index cols) {
    if (rows < 0 || cols < 0) {
        return ::sindre::general::Result<Matrix<Scalar>>::failure(
            ::sindre::general::Error::make(
                std::errc::invalid_argument,
                "Matrix dimensions must not be negative",
                "math.to_matrix"));
    }

    const auto row_count = static_cast<std::size_t>(rows);
    const auto column_count = static_cast<std::size_t>(cols);
    if (column_count != 0 && row_count >
                                  std::numeric_limits<std::size_t>::max() / column_count) {
        return ::sindre::general::Result<Matrix<Scalar>>::failure(
            ::sindre::general::Error::make(
                std::errc::value_too_large,
                "Matrix dimensions overflow std::size_t",
                "math.to_matrix"));
    }

    const auto expected = row_count * column_count;
    if (values.size() != expected) {
        return ::sindre::general::Result<Matrix<Scalar>>::failure(
            ::sindre::general::Error::make(
                std::errc::invalid_argument,
                "std::vector size does not match matrix dimensions",
                "math.to_matrix"));
    }

    Matrix<Scalar> result(rows, cols);
    if (!values.empty()) {
        std::copy(values.begin(), values.end(), result.data());
    }
    return ::sindre::general::Result<Matrix<Scalar>>::success(std::move(result));
}

/// @brief 将 std::vector 拷贝为动态列向量。
template <class Scalar>
::sindre::general::Result<Vector<Scalar>>
to_vector(const std::vector<Scalar> &values) {
    if (values.size() > static_cast<std::size_t>(std::numeric_limits<Index>::max())) {
        return ::sindre::general::Result<Vector<Scalar>>::failure(
            ::sindre::general::Error::make(
                std::errc::value_too_large,
                "std::vector is too large for the Eigen index type",
                "math.to_vector"));
    }

    Vector<Scalar> result(static_cast<Index>(values.size()));
    if (!values.empty()) {
        std::copy(values.begin(), values.end(), result.data());
    }
    return ::sindre::general::Result<Vector<Scalar>>::success(std::move(result));
}

/// @brief 按逻辑行优先顺序拷贝矩阵或数组到 std::vector。
template <class Derived>
std::vector<typename Derived::Scalar>
to_std_vector(const Eigen::DenseBase<Derived> &values) {
    using Scalar = typename Derived::Scalar;
    std::vector<Scalar> result;
    result.reserve(static_cast<std::size_t>(values.size()));
    for (Index row = 0; row < values.rows(); ++row) {
        for (Index column = 0; column < values.cols(); ++column) {
            result.push_back(values.coeff(row, column));
        }
    }
    return result;
}

/// @brief Explicit escape hatch for advanced Eigen interoperability.
namespace eigen = ::Eigen;

} // namespace sindre::math
