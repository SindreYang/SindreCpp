#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if defined(SINDRE_WITH_EIGEN)
#include <Eigen/Core>
#include <Eigen/Geometry>
#endif

namespace sindre::general {

/// @brief 可跨模块传递的结构化错误。
///
/// `code` 适合程序判断，`message` 面向人，`context` 用于定位失败边界。
struct Error {
    std::error_code code{};
    std::string message;
    std::string context;

    static Error make(std::errc code, std::string message, std::string context = {}) {
        return {std::make_error_code(code), std::move(message), std::move(context)};
    }
    Error with_context(std::string_view extra) const {
        Error result = *this;
        if (extra.empty()) return result;
        if (!result.context.empty()) result.context += ".";
        result.context += extra;
        return result;
    }
    explicit operator bool() const noexcept {
        return static_cast<bool>(code) || !message.empty() || !context.empty();
    }
    std::string describe() const {
        std::string result;
        if (code) result += "[" + std::to_string(code.value()) + "] ";
        result += message;
        if (!context.empty()) {
            if (!result.empty()) result += " | ";
            result += context;
        }
        return result;
    }
};

template <class T>
/// @brief 表示成功值或结构化错误的非异常结果。
class Result {
public:
    static Result success(T value) { return Result(std::in_place_index<0>, std::move(value)); }
    static Result failure(Error error) { return Result(std::in_place_index<1>, std::move(error)); }
    static Result failure(std::error_code code, std::string message, std::string context = {}) {
        return failure(Error{code, std::move(message), std::move(context)});
    }
    bool has_value() const noexcept { return value_.index() == 0; }
    explicit operator bool() const noexcept { return has_value(); }
    T &value() & { return *std::get_if<0>(&value_); }
    const T &value() const & { return *std::get_if<0>(&value_); }
    T &&value() && { return std::move(*std::get_if<0>(&value_)); }
    Error &error() & { return *std::get_if<1>(&value_); }
    const Error &error() const & { return *std::get_if<1>(&value_); }
    T *value_ptr() noexcept { return std::get_if<0>(&value_); }
    const T *value_ptr() const noexcept { return std::get_if<0>(&value_); }
    Error *error_ptr() noexcept { return std::get_if<1>(&value_); }
    const Error *error_ptr() const noexcept { return std::get_if<1>(&value_); }
    T value_or(T fallback) const {
        const auto *value = value_ptr();
        return value ? *value : std::move(fallback);
    }
private:
    template <class... Args>
    explicit Result(std::in_place_index_t<0> tag, Args &&...args)
        : value_(tag, std::forward<Args>(args)...) {}
    template <class... Args>
    explicit Result(std::in_place_index_t<1> tag, Args &&...args)
        : value_(tag, std::forward<Args>(args)...) {}
    std::variant<T, Error> value_;
};

template <>
/// @brief 无返回值操作的 Result 特化。
class Result<void> {
public:
    static Result success() { return Result(true, {}); }
    static Result failure(Error error) { return Result(false, std::move(error)); }
    static Result failure(std::error_code code, std::string message, std::string context = {}) {
        return failure(Error{code, std::move(message), std::move(context)});
    }
    bool has_value() const noexcept { return ok_; }
    explicit operator bool() const noexcept { return ok_; }
    const Error &error() const & { return error_; }
    const Error *error_ptr() const noexcept { return ok_ ? nullptr : &error_; }
private:
    Result(bool ok, Error error) : ok_(ok), error_(std::move(error)) {}
    bool ok_;
    Error error_;
};

namespace pointer {
template <class T> using unique_ptr = std::unique_ptr<T>;
template <class T> using shared_ptr = std::shared_ptr<T>;
template <class T> using weak_ptr = std::weak_ptr<T>;
using std::make_shared;
using std::make_unique;
}

namespace ranges {
/// @brief 判断范围中是否包含指定值。
template <class Range, class Value>
bool contains(const Range &range, const Value &value) {
    using std::begin; using std::end;
    return std::find(begin(range), end(range), value) != end(range);
}
template <class Range, class Function>
/// @brief 将函数应用到范围并返回拥有型结果。
auto transform(const Range &range, Function function)
    -> std::vector<std::decay_t<decltype(function(*std::begin(range)))>> {
    using Output = std::decay_t<decltype(function(*std::begin(range)))>;
    std::vector<Output> result;
    result.reserve(static_cast<std::size_t>(std::distance(std::begin(range), std::end(range))));
    for (const auto &value : range) result.push_back(function(value));
    return result;
}
template <class Range, class Function>
/// @brief 顺序执行范围操作，并把异常转换为 Result。
Result<void> for_each_result(const Range &range, Function function) noexcept {
#if defined(SINDRE_NO_EXCEPTIONS)
    for (const auto &value : range) { auto status = function(value); if (!status) return status; }
    return Result<void>::success();
#else
    try {
        for (const auto &value : range) { auto status = function(value); if (!status) return status; }
        return Result<void>::success();
    } catch (const std::exception &error) {
        return Result<void>::failure(std::make_error_code(std::errc::invalid_argument), error.what(), "ranges.for_each");
    } catch (...) {
        return Result<void>::failure(std::make_error_code(std::errc::invalid_argument), "Unknown range operation failure", "ranges.for_each");
    }
#endif
}
}

template <class Function>
/// @brief 在离开作用域时执行一次清理函数。
class ScopeGuard {
public:
    explicit ScopeGuard(Function function) noexcept : function_(std::move(function)) {}
    ScopeGuard(ScopeGuard &&other) noexcept
        : function_(std::move(other.function_)), active_(std::exchange(other.active_, false)) {}
    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;
    ~ScopeGuard() noexcept {
        if (!active_) return;
#if defined(SINDRE_NO_EXCEPTIONS)
        function_();
#else
        try { function_(); } catch (...) {}
#endif
    }
    void dismiss() noexcept { active_ = false; }
private:
    Function function_;
    bool active_ = true;
};
template <class Function>
ScopeGuard<Function> scope_guard(Function function) noexcept { return ScopeGuard<Function>(std::move(function)); }

namespace codec {
/// @brief 计算 UTF-8 字节序列的 FNV-1a 64 位哈希。
std::uint64_t fnv1a64(std::string_view text) noexcept;
/// @brief 把整数格式化为小写十六进制文本。
std::string hex(std::uint64_t value);
/// @brief 创建 RFC 4122 版本 4 UUID。
Result<std::string> uuid4();
Result<std::string> base64_encode(const std::vector<std::uint8_t> &data);
Result<std::vector<std::uint8_t>> base64_decode(std::string_view text);
Result<std::vector<std::uint8_t>> simple_compress(const std::vector<std::uint8_t> &data);
Result<std::vector<std::uint8_t>> simple_decompress(const std::vector<std::uint8_t> &data);
}

namespace versioning {
/// @brief 可比较的三段式版本号。
struct Version { int major = 0; int minor = 0; int patch = 0; std::string suffix; };
bool operator==(const Version &, const Version &) noexcept;
bool operator!=(const Version &, const Version &) noexcept;
bool operator<(const Version &, const Version &) noexcept;
bool operator>(const Version &, const Version &) noexcept;
bool operator<=(const Version &, const Version &) noexcept;
bool operator>=(const Version &, const Version &) noexcept;
/// @brief 解析 `major.minor.patch` 版本文本。
Result<Version> parse(std::string_view text) noexcept;
/// @brief 格式化版本文本。
std::string to_string(const Version &value);
}

inline constexpr char version[] = "0.1.0";

const char *library_abi() noexcept;

#if defined(SINDRE_WITH_EIGEN)
namespace eigen {
using Vector2 = Eigen::Vector2d;
using Vector3 = Eigen::Vector3d;
using Matrix3 = Eigen::Matrix3d;
using Matrix4 = Eigen::Matrix4d;
namespace native = Eigen;
template <typename Scalar>
using MatrixView = Eigen::Map<Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>>;
template <typename Scalar>
inline MatrixView<Scalar> map_matrix(Scalar *data, std::ptrdiff_t rows, std::ptrdiff_t columns) {
    return MatrixView<Scalar>(data, rows, columns);
}
}
#endif

} // namespace sindre::general
