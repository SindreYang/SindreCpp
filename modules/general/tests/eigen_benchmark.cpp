#include <sindre/general/core.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>

namespace {

using Clock = std::chrono::steady_clock;

int parse_positive(const char *value, int fallback) {
    if (!value) return fallback;
    char *end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed < 1 || parsed > 8192) return fallback;
    return static_cast<int>(parsed);
}

} // namespace

int main(int argc, char **argv) {
    const int size = parse_positive(argc > 1 ? argv[1] : nullptr, 1024);
    const int repeats = parse_positive(argc > 2 ? argv[2] : nullptr, 3);
    std::mt19937 generator(42);
    std::normal_distribution<double> distribution(0.0, 1.0);

    Eigen::MatrixXd left(size, size);
    Eigen::MatrixXd right(size, size);
    for (Eigen::Index row = 0; row < size; ++row) {
        for (Eigen::Index column = 0; column < size; ++column) {
            left(row, column) = distribution(generator);
            right(row, column) = distribution(generator);
        }
    }

    Eigen::MatrixXd product(size, size);
    product.noalias() = left * right;
    double elapsed_seconds = 0.0;
    for (int iteration = 0; iteration < repeats; ++iteration) {
        const auto started = Clock::now();
        product.noalias() = left * right;
        elapsed_seconds += std::chrono::duration<double>(Clock::now() - started).count();
    }

    const double average = elapsed_seconds / repeats;
    const double operations = 2.0 * static_cast<double>(size) * size * size;
    const double gflops = operations / average / 1.0e9;
    const double checksum = product.row(0).sum();
    std::cout << "backend="
#if defined(EIGEN_USE_BLAS)
              << "blas"
#else
              << "eigen"
#endif
              << " size=" << size
              << " repeats=" << repeats
              << " average_ms=" << std::fixed << std::setprecision(3) << average * 1000.0
              << " gflops=" << std::setprecision(3) << gflops
              << " checksum=" << std::setprecision(9) << checksum << '\n';
    return std::isfinite(checksum) ? EXIT_SUCCESS : EXIT_FAILURE;
}
