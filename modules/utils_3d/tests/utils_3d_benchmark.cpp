#include <Eigen/Core>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>

#ifndef SINDRE_BENCHMARK_BLAS_VENDOR
#define SINDRE_BENCHMARK_BLAS_VENDOR "BLAS"
#endif

extern "C" void dgemm_(const char* transa, const char* transb,
                       const int* m, const int* n, const int* k,
                       const double* alpha, const double* a, const int* lda,
                       const double* b, const int* ldb, const double* beta,
                       double* c, const int* ldc);

namespace {
using Clock = std::chrono::steady_clock;
using Matrix = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>;

template<class Function>
double benchmark_ms(Function&& function, int repetitions) {
    function(); // warm-up
    const auto start = Clock::now();
    for (int i = 0; i < repetitions; ++i) {
        function();
    }
    const auto stop = Clock::now();
    return std::chrono::duration<double, std::milli>(stop - start).count() / repetitions;
}
}

int main(int argc, char** argv) {
    const int size = argc > 1 ? std::max(64, std::atoi(argv[1])) : 1024;
    const int repetitions = argc > 2 ? std::max(1, std::atoi(argv[2])) : 5;
    Eigen::setNbThreads(1);

    Matrix a(size, size), b(size, size), eigen_result(size, size), blas_result(size, size);
    std::mt19937_64 random(20260914);
    std::uniform_real_distribution<double> values(-1.0, 1.0);
    for (Eigen::Index i = 0; i < a.size(); ++i) {
        a.data()[i] = values(random);
        b.data()[i] = values(random);
    }

    const double eigen_ms = benchmark_ms([&] { eigen_result.noalias() = a * b; }, repetitions);
    const char no_transpose = 'N';
    const double alpha = 1.0, beta = 0.0;
    const int m = size, n = size, k = size, leading = size;
    const double blas_ms = benchmark_ms([&] {
        dgemm_(&no_transpose, &no_transpose, &m, &n, &k, &alpha,
               a.data(), &leading, b.data(), &leading, &beta, blas_result.data(), &leading);
    }, repetitions);

    const double max_abs_error = (eigen_result - blas_result).cwiseAbs().maxCoeff();
    const double relative_l2_error = (eigen_result - blas_result).norm() / std::max(blas_result.norm(), 1e-300);
    const double operations = 2.0 * size * size * size;

    std::cout << std::fixed << std::setprecision(6)
              << "Benchmark: double-precision dense square GEMM, " << size << "x" << size
              << ", repetitions=" << repetitions << ", Eigen threads=1, BLAS="
              << SINDRE_BENCHMARK_BLAS_VENDOR << '\n'
              << "Eigen built-in: " << eigen_ms << " ms, " << (operations / (eigen_ms * 1e6)) << " GFLOP/s\n"
              << "BLAS dgemm:     " << blas_ms << " ms, " << (operations / (blas_ms * 1e6)) << " GFLOP/s\n"
              << "BLAS speedup over Eigen: " << (eigen_ms / blas_ms) << "x\n"
              << std::scientific << "Max absolute error: " << max_abs_error
              << "\nRelative L2 error: " << relative_l2_error << '\n';
    return std::isfinite(max_abs_error) && std::isfinite(relative_l2_error)
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
