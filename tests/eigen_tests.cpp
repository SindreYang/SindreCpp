#include <sindrecpp/eigen.hpp>

#include <cmath>
#include <cstdlib>
#include <iostream>

int main() {
    Eigen::Matrix3d left;
    left << 1.0, 2.0, 3.0,
            4.0, 5.0, 6.0,
            7.0, 8.0, 10.0;
    Eigen::Matrix3d right;
    right << 2.0, 0.0, 1.0,
             1.0, 3.0, 2.0,
             4.0, 1.0, 0.0;

    const Eigen::Matrix3d product = left * right;
    Eigen::Matrix3d expected;
    expected << 16.0, 9.0, 5.0,
                37.0, 21.0, 14.0,
                62.0, 34.0, 23.0;
    const double max_error = (product - expected).cwiseAbs().maxCoeff();
    if (max_error > 1e-12) {
        std::cerr << "Eigen matrix product exceeded tolerance: " << max_error << '\n';
        return EXIT_FAILURE;
    }

    constexpr Eigen::Index size = 128;
    Eigen::MatrixXd dynamic_left(size, size), dynamic_right(size, size);
    for (Eigen::Index row = 0; row < size; ++row) {
        for (Eigen::Index col = 0; col < size; ++col) {
            dynamic_left(row, col) = std::sin(0.1 * row + 0.03 * col);
            dynamic_right(row, col) = std::cos(0.07 * row - 0.02 * col);
        }
    }
    const Eigen::MatrixXd dynamic_product = dynamic_left * dynamic_right;
    for (Eigen::Index row = 0; row < size; ++row) {
        for (Eigen::Index col = 0; col < size; ++col) {
            double reference = 0.0;
            for (Eigen::Index inner = 0; inner < size; ++inner) {
                reference += dynamic_left(row, inner) * dynamic_right(inner, col);
            }
            if (std::abs(dynamic_product(row, col) - reference) > 1e-10) {
                std::cerr << "Dynamic Eigen matrix product exceeded tolerance at ("
                          << row << ", " << col << ")\n";
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
