#include <sindre/math.h>

#include <cmath>
#include <iostream>
#include <type_traits>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int main() {
    using sindre::math::ArrayXd;
    using sindre::math::Matrix3;
    using sindre::math::MatrixXd;
    using sindre::math::Vector3;
    using sindre::math::VectorXd;

    static_assert(MatrixXd::IsRowMajor, "MatrixXd must be RowMajor");
    static_assert(sindre::math::MatrixXf::IsRowMajor, "MatrixXf must be RowMajor");
    static_assert(ArrayXd::IsRowMajor, "ArrayXd must be RowMajor");
    static_assert(Matrix3::IsRowMajor, "Matrix3 must be RowMajor");
    static_assert(!sindre::math::Vector3f::IsRowMajor,
                  "column vectors must use Eigen's native column-major layout");
    static_assert(sindre::math::Matrix4f::IsRowMajor, "Matrix4f must be RowMajor");

    MatrixXd left(2, 2);
    left << 1.0, 2.0,
            3.0, 4.0;
    MatrixXd right = MatrixXd::Identity(2, 2);
    const MatrixXd product = left * right;
    check(product.isApprox(left), "row-major matrix multiplication");
    check(product.data()[1] == 2.0, "row-major contiguous storage");

    ArrayXd values(2, 2);
    values << 1.0, 2.0,
              3.0, 4.0;
    check(values(1, 0) == 3.0, "row-major array indexing");

    VectorXd vector(3);
    vector << 1.0, 2.0, 3.0;
    check(vector.size() == 3 && vector.sum() == 6.0, "dynamic vector");

    Vector3 point(1.0, 2.0, 3.0);
    check(point.size() == 3, "fixed vector");

    const auto inverse = Matrix3::Identity().inverse();
    check(inverse.isApprox(Matrix3::Identity()), "matrix inverse");

    const auto quaternion = sindre::math::Quaternion::Identity();
    check(std::abs(quaternion.norm() - 1.0) < 1e-12, "quaternion");
    const auto quaternionf = sindre::math::Quaternionf::Identity();
    check(std::abs(quaternionf.norm() - 1.0f) < 1e-6f, "float quaternion");

    const std::vector<double> flat{1, 2, 3, 4, 5, 6};
    const auto matrix_from_vector = sindre::math::to_matrix(flat, 2, 3);
    check(matrix_from_vector && matrix_from_vector.value()(1, 2) == 6.0,
          "std::vector to row-major matrix");
    const auto vector_from_vector = sindre::math::to_vector(flat);
    check(vector_from_vector && vector_from_vector.value().size() == 6 &&
              vector_from_vector.value()(2) == 3.0,
          "std::vector to dynamic vector");
    if (matrix_from_vector) {
        const auto roundtrip = sindre::math::to_std_vector(matrix_from_vector.value());
        check(roundtrip == flat, "row-major matrix to std::vector");
    }
    const auto array_roundtrip = sindre::math::to_std_vector(values);
    check(array_roundtrip == std::vector<double>({1.0, 2.0, 3.0, 4.0}),
          "row-major array to std::vector");
    const auto invalid_matrix = sindre::math::to_matrix(flat, 4, 2);
    check(!invalid_matrix && invalid_matrix.error().context == "math.to_matrix",
          "invalid std::vector matrix dimensions");

    auto transform = sindre::math::Transform3::Identity();
    transform.translate(Vector3(1.0, 2.0, 3.0));
    const auto translated = transform * Vector3::Zero();
    check(translated.isApprox(Vector3(1.0, 2.0, 3.0)), "affine transform");

    sindre::math::eigen::MatrixXd native(1, 1);
    native(0, 0) = 42.0;
    check(native(0, 0) == 42.0, "native Eigen escape hatch");

    using Image = sindre::math::tensor::CHW<int>;
    static_assert(Image::Layout == Eigen::RowMajor, "CHW must use RowMajor layout");
    Image image(2, 3, 4);
    for (sindre::math::TensorIndex channel = 0; channel < image.dimension(0); ++channel) {
        for (sindre::math::TensorIndex height = 0; height < image.dimension(1); ++height) {
            for (sindre::math::TensorIndex width = 0; width < image.dimension(2); ++width) {
                image(channel, height, width) =
                    static_cast<int>(channel * 100 + height * 10 + width);
            }
        }
    }
    check(image(1, 2, 3) == 123, "CHW axis order");
    check(image.data()[0] == 0 && image.data()[1] == 1,
          "CHW row-major contiguous width axis");

    const auto offsets = sindre::math::TensorDimensions<3>{0, 1, 1};
    const auto extents = sindre::math::TensorDimensions<3>{2, 2, 3};
    Image sliced = image.slice(offsets, extents);
    check(sliced.dimension(0) == 2 && sliced.dimension(1) == 2 &&
              sliced.dimension(2) == 3 && sliced(1, 1, 2) == 123,
          "CHW slicing");

    const auto broadcast_factors = sindre::math::TensorDimensions<3>{1, 2, 3};
    Image broadcast = image.broadcast(broadcast_factors);
    check(broadcast.dimension(0) == 2 && broadcast.dimension(1) == 6 &&
              broadcast.dimension(2) == 12 && broadcast(1, 4, 5) == image(1, 1, 1),
          "CHW broadcasting");

    sindre::math::Tensor<int, 2> reshaped =
        image.reshape(sindre::math::TensorDimensions<2>{3, 8});
    check(reshaped.dimension(0) == 3 && reshaped.dimension(1) == 8 &&
              reshaped(1, 0) == image(0, 2, 0),
          "CHW reshape");

    std::vector<int> mapped_storage(6, 0);
    sindre::math::TensorMap<int, 3> mapped(
        mapped_storage.data(), sindre::math::TensorDimensions<3>{1, 2, 3});
    mapped(0, 1, 2) = 7;
    check(mapped_storage[5] == 7, "TensorMap non-owning storage");

    return failures == 0 ? 0 : 1;
}
