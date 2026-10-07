#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

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
    [[nodiscard]] static Result success(T value) {
        return Result(std::in_place_index<0>, std::move(value));
    }
    [[nodiscard]] static Result failure(Error error) {
        return Result(std::in_place_index<1>, std::move(error));
    }
    [[nodiscard]] static Result failure(
        std::error_code code, std::string message, std::string context = {}) {
        return failure(Error{code, std::move(message), std::move(context)});
    }
    [[nodiscard]] bool has_value() const noexcept { return value_.index() == 0; }
    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }
    // Accessors require the matching Result state.  Termination is deliberate:
    // it keeps misuse deterministic in both exception-enabled and no-exception builds.
    T &value() & noexcept {
        auto *value = std::get_if<0>(&value_);
        if (!value) std::terminate();
        return *value;
    }
    const T &value() const & noexcept {
        const auto *value = std::get_if<0>(&value_);
        if (!value) std::terminate();
        return *value;
    }
    T &&value() && noexcept {
        auto *value = std::get_if<0>(&value_);
        if (!value) std::terminate();
        return std::move(*value);
    }
    Error &error() & noexcept {
        auto *error = std::get_if<1>(&value_);
        if (!error) std::terminate();
        return *error;
    }
    const Error &error() const & noexcept {
        const auto *error = std::get_if<1>(&value_);
        if (!error) std::terminate();
        return *error;
    }
    [[nodiscard]] T *value_ptr() noexcept { return std::get_if<0>(&value_); }
    [[nodiscard]] const T *value_ptr() const noexcept { return std::get_if<0>(&value_); }
    [[nodiscard]] Error *error_ptr() noexcept { return std::get_if<1>(&value_); }
    [[nodiscard]] const Error *error_ptr() const noexcept { return std::get_if<1>(&value_); }
    [[nodiscard]] T value_or(T fallback) const & {
        const auto *value = value_ptr();
        return value ? *value : std::move(fallback);
    }
    [[nodiscard]] T value_or(T fallback) && {
        auto *value = value_ptr();
        return value ? std::move(*value) : std::move(fallback);
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
    [[nodiscard]] static Result success() { return Result(true, {}); }
    [[nodiscard]] static Result failure(Error error) { return Result(false, std::move(error)); }
    [[nodiscard]] static Result failure(
        std::error_code code, std::string message, std::string context = {}) {
        return failure(Error{code, std::move(message), std::move(context)});
    }
    [[nodiscard]] bool has_value() const noexcept { return ok_; }
    [[nodiscard]] explicit operator bool() const noexcept { return ok_; }
    // The error accessor is valid only for a failed Result.
    Error &error() & noexcept {
        if (ok_) std::terminate();
        return error_;
    }
    const Error &error() const & noexcept {
        if (ok_) std::terminate();
        return error_;
    }
    [[nodiscard]] Error *error_ptr() noexcept { return ok_ ? nullptr : &error_; }
    [[nodiscard]] const Error *error_ptr() const noexcept { return ok_ ? nullptr : &error_; }
private:
    Result(bool ok, Error error) : ok_(ok), error_(std::move(error)) {}
    bool ok_;
    Error error_;
};

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
    explicit ScopeGuard(Function function) noexcept(std::is_nothrow_move_constructible_v<Function>)
        : function_(std::move(function)) {}
    ScopeGuard(ScopeGuard &&other) noexcept(std::is_nothrow_move_constructible_v<Function>)
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
ScopeGuard<Function> scope_guard(Function function) {
    return ScopeGuard<Function>(std::move(function));
}

namespace codec {
/// @brief 计算 UTF-8 字节序列的 FNV-1a 64 位哈希。
std::uint64_t fnv1a64(std::string_view text) noexcept;
/// @brief 把整数格式化为小写十六进制文本。
std::string hex(std::uint64_t value);
/// @brief 把整数格式化为小写十六进制文本，并把分配失败转换为 Result。
Result<std::string> try_hex(std::uint64_t value);
/// @brief 创建 RFC 4122 版本 4 UUID。
Result<std::string> uuid4();
Result<std::string> base64_encode(const std::vector<std::uint8_t> &data);
Result<std::vector<std::uint8_t>> base64_decode(std::string_view text);
/// @brief 对内存中的字节执行 RLE 编码；这不是文件压缩格式。
Result<std::vector<std::uint8_t>> rle_compress(const std::vector<std::uint8_t> &data);
/// @brief 解码由 `rle_compress` 生成的内存字节序列。
Result<std::vector<std::uint8_t>> rle_decompress(const std::vector<std::uint8_t> &data);
/// @brief 兼容旧名称；等价于 `rle_compress`。
Result<std::vector<std::uint8_t>> simple_compress(const std::vector<std::uint8_t> &data);
/// @brief 兼容旧名称；等价于 `rle_decompress`。
Result<std::vector<std::uint8_t>> simple_decompress(const std::vector<std::uint8_t> &data);
}

namespace versioning {
/// @brief 可比较的 SemVer 2.0.0 版本号。
///
/// `prerelease` 不包含前导 `-`，`build` 不包含前导 `+`。build metadata
/// 会保留在格式化结果中，但不参与版本优先级比较。
struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string prerelease;
    std::string build;
};
bool operator==(const Version &, const Version &) noexcept;
bool operator!=(const Version &, const Version &) noexcept;
bool operator<(const Version &, const Version &) noexcept;
bool operator>(const Version &, const Version &) noexcept;
bool operator<=(const Version &, const Version &) noexcept;
bool operator>=(const Version &, const Version &) noexcept;
/// @brief 严格解析 SemVer 2.0.0 版本文本。
Result<Version> parse(std::string_view text);
/// @brief 格式化版本文本。
std::string to_string(const Version &value);
/// @brief 格式化版本文本，并把分配失败转换为 Result。
Result<std::string> try_to_string(const Version &value);
}

inline constexpr char version[] = "0.1.0";

const char *library_abi() noexcept;

} // namespace sindre::general
