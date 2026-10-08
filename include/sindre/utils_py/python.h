#pragma once

/// @file
/// @brief Python/NumPy 运行时和自有数组交换接口。

#if !defined(SINDRE_WITH_UTILS_PY)
#error "Enable SINDRE_WITH_UTILS_PY and link sindre::utils_py before including this header."
#endif

#include <sindre/general/runtime.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <exception>
#include <memory>
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

namespace sindre::utils_py {

struct InterpreterConfig {
    std::filesystem::path python_executable;
    std::filesystem::path python_home;
    std::vector<std::filesystem::path> module_search_paths;
};

enum class DType { boolean, int8, uint8, int16, uint16, int32, uint32,
                   int64, uint64, float32, float64 };

class Array {
public:
    Array() = default;
    Array(DType type, std::vector<std::int64_t> shape,
          std::vector<std::uint8_t> bytes)
        : type_(type), shape_(std::move(shape)), bytes_(std::move(bytes)) {}

    [[nodiscard]] DType get_dtype() const noexcept { return type_; }
    [[nodiscard]] const std::vector<std::int64_t> &get_shape() const noexcept { return shape_; }
    [[nodiscard]] const std::vector<std::uint8_t> &get_bytes() const noexcept { return bytes_; }
    [[nodiscard]] bool empty() const noexcept { return bytes_.empty(); }
    [[nodiscard]] std::size_t size_bytes() const noexcept { return bytes_.size(); }

private:
    DType type_ = DType::float64;
    std::vector<std::int64_t> shape_;
    std::vector<std::uint8_t> bytes_;
};

class SINDRE_UTILS_PY_API GilGuard {
public:
    GilGuard(const GilGuard &) = delete;
    GilGuard &operator=(const GilGuard &) = delete;
    GilGuard(GilGuard &&) = delete;
    GilGuard &operator=(GilGuard &&) = delete;
    ~GilGuard();

private:
    explicit GilGuard(void *impl) noexcept : impl_(impl) {}
    void *impl_ = nullptr;
    friend class Interpreter;
};

class SINDRE_UTILS_PY_API GilReleaseGuard {
public:
    GilReleaseGuard(const GilReleaseGuard &) = delete;
    GilReleaseGuard &operator=(const GilReleaseGuard &) = delete;
    GilReleaseGuard(GilReleaseGuard &&) = delete;
    GilReleaseGuard &operator=(GilReleaseGuard &&) = delete;
    ~GilReleaseGuard();

private:
    explicit GilReleaseGuard(void *impl) noexcept : impl_(impl) {}
    void *impl_ = nullptr;
    friend class Interpreter;
};

class SINDRE_UTILS_PY_API Interpreter {
public:
    static ::sindre::general::Result<std::shared_ptr<Interpreter>>
    create(const InterpreterConfig &config = {}) noexcept;
    static ::sindre::general::Result<std::shared_ptr<Interpreter>> attach() noexcept;

    GilGuard get_gil() const;
    GilReleaseGuard get_gil_release() const;

    template <class Function>
    auto run_with_gil(Function &&function) const noexcept
        -> ::sindre::general::Result<std::decay_t<std::invoke_result_t<Function &>>> {
        using Return = std::decay_t<std::invoke_result_t<Function &>>;
        try {
            auto guard = get_gil();
            if constexpr (std::is_void_v<Return>) {
                std::invoke(std::forward<Function>(function));
                return ::sindre::general::Result<void>::success();
            } else {
                return ::sindre::general::Result<Return>::success(
                    std::invoke(std::forward<Function>(function)));
            }
        } catch (const std::exception &error) {
            return ::sindre::general::Result<Return>::failure(
                ::sindre::general::Error::make(std::errc::io_error, error.what(),
                                               "utils_py.interpreter"));
        } catch (...) {
            return ::sindre::general::Result<Return>::failure(
                ::sindre::general::Error::make(std::errc::io_error,
                                               "Python callback failed",
                                               "utils_py.interpreter"));
        }
    }

    Interpreter(const Interpreter &) = delete;
    Interpreter &operator=(const Interpreter &) = delete;
    Interpreter(Interpreter &&) noexcept;
    Interpreter &operator=(Interpreter &&) noexcept;
    ~Interpreter();
    [[nodiscard]] bool initialized() const noexcept { return initialized_; }

private:
    explicit Interpreter(void *impl) noexcept : impl_(impl), initialized_(impl != nullptr) {}
    void *impl_ = nullptr;
    bool initialized_ = false;
};

SINDRE_UTILS_PY_API
::sindre::general::Result<std::shared_ptr<Interpreter>>
get_runtime(const InterpreterConfig &config = {}) noexcept;

template <class T> constexpr DType dtype_of() {
    if constexpr (std::is_same_v<T, bool>) return DType::boolean;
    else if constexpr (std::is_same_v<T, std::int8_t>) return DType::int8;
    else if constexpr (std::is_same_v<T, std::uint8_t>) return DType::uint8;
    else if constexpr (std::is_same_v<T, std::int16_t>) return DType::int16;
    else if constexpr (std::is_same_v<T, std::uint16_t>) return DType::uint16;
    else if constexpr (std::is_same_v<T, std::int32_t>) return DType::int32;
    else if constexpr (std::is_same_v<T, std::uint32_t>) return DType::uint32;
    else if constexpr (std::is_same_v<T, std::int64_t>) return DType::int64;
    else if constexpr (std::is_same_v<T, std::uint64_t>) return DType::uint64;
    else if constexpr (std::is_same_v<T, float>) return DType::float32;
    else return DType::float64;
}

template <class T>
Array array_from_vector(const std::vector<T> &values) {
    std::vector<std::uint8_t> bytes(values.size() * sizeof(T));
    if (!values.empty()) std::memcpy(bytes.data(), values.data(), bytes.size());
    return Array(dtype_of<T>(), {static_cast<std::int64_t>(values.size())}, std::move(bytes));
}

template <class T>
::sindre::general::Result<std::vector<T>> vector_from_array(const Array &input) noexcept {
    if (input.get_shape().size() != 1 || input.get_dtype() != dtype_of<T>() ||
        input.size_bytes() % sizeof(T) != 0)
        return ::sindre::general::Result<std::vector<T>>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "Array type or shape does not match the requested vector",
            "utils_py.vector_from_array");
    std::vector<T> result(input.size_bytes() / sizeof(T));
    if (!result.empty()) std::memcpy(result.data(), input.get_bytes().data(), input.size_bytes());
    return ::sindre::general::Result<std::vector<T>>::success(std::move(result));
}

} // namespace sindre::utils_py

#undef SINDRE_UTILS_PY_API
