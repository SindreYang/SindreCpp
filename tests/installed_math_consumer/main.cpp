#include <sindre/math.h>

int main() {
    sindre::math::MatrixXd matrix(2, 2);
    matrix << 1.0, 2.0,
               3.0, 4.0;
    const auto product = matrix * sindre::math::MatrixXd::Identity(2, 2);
    return product.isApprox(matrix) && sindre::math::MatrixXd::IsRowMajor ? 0 : 1;
}
