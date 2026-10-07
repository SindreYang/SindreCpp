#pragma once

#if !defined(SINDRE_WITH_UTILS_PY)
#error "Enable SINDRE_WITH_UTILS_PY and link sindre::utils_py before including this header."
#endif

#include <sindre/general/runtime.h>
#include <sindre/math.h>

#include <pybind11/embed.h>
#include <pybind11/numpy.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(_WIN32)
#if defined(SINDRE_UTILS_PY_BUILD)
#define SINDRE_UTILS_PY_API __declspec(dllexport)
#elif defined(SINDRE_UTILS_PY_SHARED)
#define SINDRE_UTILS_PY_API __declspec(dllimport)
#else
#define SINDRE_UTILS_PY_API
#endif
#else
#define SINDRE_UTILS_PY_API
#endif

#if defined(SINDRE_WITH_UTILS_3D)
#include <sindre/utils_3d/sindremesh.h>
#endif

namespace sindre::utils_py {

namespace native = pybind11;

/// @brief 描述嵌入式 Python 的可执行文件、根目录和模块搜索路径。
struct InterpreterConfig {
    std::filesystem::path python_executable;
    std::filesystem::path python_home;
    std::vector<std::filesystem::path> module_search_paths;
};

class Interpreter;

/// @brief 在作用域内获取 Python 全局解释器锁。
class GilGuard {
public:
    GilGuard(const GilGuard &) = delete;
    GilGuard &operator=(const GilGuard &) = delete;
    GilGuard(GilGuard &&) = delete;
    GilGuard &operator=(GilGuard &&) = delete;
    ~GilGuard() = default;

private:
    friend class Interpreter;
    explicit GilGuard(int) : guard_() {}

    native::gil_scoped_acquire guard_;
};

/// @brief 在作用域内释放 Python 全局解释器锁以执行原生计算。
class GilReleaseGuard {
public:
    GilReleaseGuard(const GilReleaseGuard &) = delete;
    GilReleaseGuard &operator=(const GilReleaseGuard &) = delete;
    GilReleaseGuard(GilReleaseGuard &&) = delete;
    GilReleaseGuard &operator=(GilReleaseGuard &&) = delete;
    ~GilReleaseGuard() = default;

private:
    friend class Interpreter;
    explicit GilReleaseGuard(int) : guard_() {}

    native::gil_scoped_release guard_;
};

class SINDRE_UTILS_PY_API Interpreter {
public:
    /// @brief 创建并拥有一个嵌入式 Python 解释器。
    static ::sindre::general::Result<std::shared_ptr<Interpreter>>
    create(const InterpreterConfig &config = {}) noexcept;

    /// @brief 附加到宿主已初始化的解释器，析构时不会终止宿主解释器。
    static ::sindre::general::Result<std::shared_ptr<Interpreter>> attach() noexcept;

    GilGuard get_gil() const { return GilGuard(0); }
    GilReleaseGuard get_gil_release() const { return GilReleaseGuard(0); }

    template <class Function>
    auto run_with_gil(Function &&function) const noexcept
        -> ::sindre::general::Result<
            std::decay_t<std::invoke_result_t<Function &>>> {
        using Return = std::decay_t<std::invoke_result_t<Function &>>;
        try {
            auto gil = get_gil();
            try {
                if constexpr (std::is_void_v<Return>) {
                    std::invoke(std::forward<Function>(function));
                    return ::sindre::general::Result<void>::success();
                } else {
                    return ::sindre::general::Result<Return>::success(
                        std::invoke(std::forward<Function>(function)));
                }
            } catch (const native::error_already_set &error) {
                return ::sindre::general::Result<Return>::failure(
                    ::sindre::general::Error::make(
                        std::errc::io_error, error.what(), "utils_py.interpreter"));
            } catch (const std::exception &error) {
                return ::sindre::general::Result<Return>::failure(
                    ::sindre::general::Error::make(
                        std::errc::io_error, error.what(), "utils_py.interpreter"));
            } catch (...) {
                return ::sindre::general::Result<Return>::failure(
                    ::sindre::general::Error::make(
                        std::errc::io_error, "Python callback failed", "utils_py.interpreter"));
            }
        } catch (const std::exception &error) {
            return ::sindre::general::Result<Return>::failure(
                ::sindre::general::Error::make(
                    std::errc::io_error, error.what(), "utils_py.interpreter"));
        } catch (...) {
            return ::sindre::general::Result<Return>::failure(
                ::sindre::general::Error::make(
                    std::errc::io_error, "Python callback failed", "utils_py.interpreter"));
        }
    }

    Interpreter(const Interpreter &) = delete;
    Interpreter &operator=(const Interpreter &) = delete;
    Interpreter(Interpreter &&) noexcept;
    Interpreter &operator=(Interpreter &&) noexcept;
    ~Interpreter();

    bool initialized() const noexcept { return initialized_; }

private:
    explicit Interpreter(std::unique_ptr<native::scoped_interpreter> guard,
                         bool initialized) noexcept;

    std::unique_ptr<native::scoped_interpreter> guard_;
    bool initialized_ = false;
};

/// @brief 获取进程级共享运行时；首次调用初始化或附加，后续调用复用它。
SINDRE_UTILS_PY_API
::sindre::general::Result<std::shared_ptr<Interpreter>>
get_runtime(const InterpreterConfig &config = {}) noexcept;

/// @brief 将一维 C++ vector 拷贝为 NumPy 数组。
template <class T> pybind11::array_t<T> array_from_vector(const std::vector<T> &values) {
    pybind11::array_t<T> result(values.size());
    if (!values.empty()) {
        std::copy(values.begin(), values.end(), result.mutable_data());
    }
    return result;
}

/// @brief 将一维 NumPy 数组安全拷贝为 C++ vector。
template <class T> std::vector<T> vector_from_array(const pybind11::array &input) {
    using Array = pybind11::array_t<T, pybind11::array::c_style | pybind11::array::forcecast>;
    const auto array = Array::ensure(input);
    if (!array) {
        throw std::invalid_argument("NumPy array could not be converted to the requested sindre element type");
    }
    if (array.ndim() != 1) {
        throw std::invalid_argument("sindre expects a one-dimensional NumPy array");
    }
    if (array.size() == 0)
        return {};
    const auto info = array.request();
    const auto *begin = static_cast<const T *>(info.ptr);
    return std::vector<T>(begin, begin + info.size);
}

#if defined(SINDRE_WITH_UTILS_3D)
// Copy-only interchange: no Python/VTK/Eigen object borrows memory from
// another. Strided, transposed, negative-stride and Fortran arrays are
// materialized safely.
template <class Scalar>
sindre::math::Matrix<Scalar>
matrix_from_array(const pybind11::array &input) {
    if (input.ndim() != 2)
        throw std::invalid_argument("Expected a two-dimensional NumPy array");
    if constexpr (std::is_integral_v<Scalar>) {
        const auto kind = input.dtype().kind();
        if (kind != 'i' && kind != 'u')
            throw std::invalid_argument("Integer matrix requires integer dtype (no floating-point truncation)");
        auto np = pybind11::module_::import("numpy");
        if (input.size()) {
            auto min_value = np.attr("min")(input).attr("item")();
            auto max_value = np.attr("max")(input).attr("item")();
            if (pybind11::cast<bool>(
                    min_value.attr("__lt__")(pybind11::int_(std::numeric_limits<Scalar>::min()))) ||
                pybind11::cast<bool>(
                    max_value.attr("__gt__")(pybind11::int_(std::numeric_limits<Scalar>::max()))))
                throw std::overflow_error("NumPy integer value exceeds target matrix dtype");
        }
    } else {
        const auto kind = input.dtype().kind();
        if (kind != 'f' && kind != 'i' && kind != 'u')
            throw std::invalid_argument("Expected a real numeric NumPy dtype");
    }
    using Array = pybind11::array_t<Scalar, pybind11::array::c_style | pybind11::array::forcecast>;
    auto array = Array::ensure(input);
    if (!array)
        throw std::invalid_argument("NumPy array conversion failed");
    sindre::math::Matrix<Scalar> result(array.shape(0), array.shape(1));
    if (array.size())
        std::copy(array.data(), array.data() + array.size(), result.data());
    if constexpr (std::is_floating_point_v<Scalar>)
        if (!result.allFinite())
            throw std::invalid_argument("Matrix values must be finite");
    return result;
}

template <class Derived>
pybind11::array_t<typename Derived::Scalar>
array_from_matrix(const sindre::math::eigen::MatrixBase<Derived> &matrix) {
    using T = typename Derived::Scalar;
    pybind11::array_t<T> out({pybind11::ssize_t(matrix.rows()), pybind11::ssize_t(matrix.cols())});
    auto a = out.template mutable_unchecked<2>();
    for (sindre::math::Index i = 0; i < matrix.rows(); ++i)
        for (sindre::math::Index j = 0; j < matrix.cols(); ++j)
            a(i, j) = matrix(i, j);
    return out;
}

inline utils_3d::SindreMesh mesh_from_arrays(const pybind11::array &vertices,
                                            const pybind11::array &faces) {
    if (vertices.ndim() != 2 || vertices.shape(1) != 3 || faces.ndim() != 2 || faces.shape(1) != 3)
        throw std::invalid_argument("Mesh vertices/faces must have shape (N,3)/(M,3)");
    return utils_3d::SindreMesh(utils_3d::Vertices(matrix_from_array<double>(vertices)),
                               utils_3d::Faces(matrix_from_array<std::int64_t>(faces)));
}

inline pybind11::tuple arrays_from_mesh(const utils_3d::SindreMesh &mesh) {
    return pybind11::make_tuple(array_from_matrix(mesh.vertices()),
                                array_from_matrix(mesh.faces()));
}
#endif

} // namespace sindre::utils_py

#undef SINDRE_UTILS_PY_API
