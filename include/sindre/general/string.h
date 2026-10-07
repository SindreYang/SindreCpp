#pragma once

#include <sindre/general/core.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <cs_string.h>

namespace sindre::general::string {

/// @brief 表示未找到或不受限的位置。
inline constexpr std::size_t npos = std::string_view::npos;

/// @brief 删除首尾 ASCII 空白并返回原文本视图。
std::string_view trim(std::string_view text) noexcept;
/// @brief 删除首尾 ASCII 空白并返回新字符串。
std::string trim_copy(std::string_view text);
/// @brief 查询文本前缀。
bool starts_with(std::string_view text, std::string_view prefix) noexcept;
/// @brief 查询文本后缀。
bool ends_with(std::string_view text, std::string_view suffix) noexcept;
/// @brief 查询子串。
bool contains(std::string_view text, std::string_view needle) noexcept;
std::size_t count(std::string_view text, std::string_view needle) noexcept;
std::string_view lstrip(std::string_view text, std::string_view chars = " \t\n\r\f\v") noexcept;
std::string_view rstrip(std::string_view text, std::string_view chars = " \t\n\r\f\v") noexcept;
std::string_view strip(std::string_view text, std::string_view chars = " \t\n\r\f\v") noexcept;
/// @brief 替换全部非重叠匹配。
std::string replace_all(std::string_view text, std::string_view from, std::string_view to);
/// @brief 按单字节分隔符返回原文本视图。
std::vector<std::string_view> split_view(std::string_view text, char delimiter);
std::vector<std::string> split(std::string_view text, char delimiter);
std::vector<std::string> split(std::string_view text, std::string_view delimiter,
                               bool keep_empty = true);
std::vector<std::string> rsplit(std::string_view text, std::string_view delimiter,
                                std::size_t max_splits = npos, bool keep_empty = true);
std::string repeat(std::string_view text, std::size_t times);
/// @brief 将 ASCII 字母转换为小写，其他 UTF-8 字节保持不变。
std::string lower_ascii(std::string_view text);
/// @brief 将 ASCII 字母转换为大写，其他 UTF-8 字节保持不变。
std::string upper_ascii(std::string_view text);
/// @brief 校验文本是否为合法 UTF-8。
bool valid_utf8(std::string_view text) noexcept;
Result<std::string> normalize_utf8(std::string_view text);
Result<std::size_t> count_code_points(std::string_view text);
/// @brief 严格解析整数；空白允许出现在首尾。
Result<std::int64_t> parse_int(std::string_view text, int base = 10);
/// @brief 严格解析有限浮点数。
Result<double> parse_float(std::string_view text);
/// @brief 按分隔符拼接字符串视图。
std::string join(const std::vector<std::string_view> &parts, std::string_view separator);
std::string join(std::initializer_list<std::string_view> parts, std::string_view separator);
std::string join(const std::vector<std::string> &parts, std::string_view separator);

template <class Encoding,
          class Allocator = std::allocator<typename Encoding::storage_unit>>
class BasicString;

namespace detail {

template <class Value>
struct is_basic_string : std::false_type {};

template <class Encoding, class Allocator>
struct is_basic_string<BasicString<Encoding, Allocator>> : std::true_type {};

template <class Function>
auto try_invoke(Function &&function, std::string context)
    -> Result<std::decay_t<std::invoke_result_t<Function>>> {
    using Value = std::decay_t<std::invoke_result_t<Function>>;
    try {
        if constexpr (std::is_void_v<Value>) {
            std::forward<Function>(function)();
            return Result<void>::success();
        } else {
            return Result<Value>::success(std::forward<Function>(function)());
        }
    } catch (const std::bad_alloc &error) {
        return Result<Value>::failure(std::make_error_code(std::errc::not_enough_memory),
                                      error.what(), std::move(context));
    } catch (const std::out_of_range &error) {
        return Result<Value>::failure(std::make_error_code(std::errc::result_out_of_range),
                                      error.what(), std::move(context));
    } catch (const std::exception &error) {
        return Result<Value>::failure(std::make_error_code(std::errc::invalid_argument),
                                      error.what(), std::move(context));
    } catch (...) {
        return Result<Value>::failure(std::make_error_code(std::errc::invalid_argument),
                                      "CsString operation failed", std::move(context));
    }
}

} // namespace detail

/// @brief CsString-backed Unicode string with a compile-time encoding policy.
///
/// `size()` and indexes use Unicode code points. Storage units, conversion and
/// malformed-input handling are delegated to CsString. Use `try_*` methods at
/// public failure boundaries; they convert backend exceptions into Result.
template <class Encoding, class Allocator>
class BasicString {
public:
    using encoding_type = Encoding;
    using allocator_type = Allocator;
    using native_type = CsString::CsBasicString<Encoding, Allocator>;
    using code_point_type = CsString::CsChar;
    using size_type = std::size_t;

    static constexpr size_type npos = std::numeric_limits<size_type>::max();

    BasicString() = default;
    explicit BasicString(const char *text)
        : value_(native_type::fromUtf8(text ? text : "")) {}
    explicit BasicString(std::string_view text)
        : value_(native_type::fromUtf8(text.data(), static_cast<typename native_type::size_type>(text.size()))) {}
    explicit BasicString(std::u16string_view text)
        : value_(native_type::fromUtf16(text.data(), static_cast<typename native_type::size_type>(text.size()))) {}
    template <std::size_t Size>
    explicit BasicString(const char16_t (&text)[Size])
        : BasicString(std::u16string_view(text, Size > 0 ? Size - 1 : 0)) {}
    explicit BasicString(const native_type &value) : value_(value) {}
    explicit BasicString(native_type &&value) noexcept : value_(std::move(value)) {}

    static Result<BasicString> try_create_utf8(std::string_view text) noexcept {
        return detail::try_invoke([&] { return BasicString(text); }, "string.create_utf8");
    }
    static Result<BasicString> try_create_utf16(std::u16string_view text) noexcept {
        return detail::try_invoke([&] { return BasicString(text); }, "string.create_utf16");
    }

    const native_type &get_native() const noexcept { return value_; }
    native_type &get_native() noexcept { return value_; }
    bool empty() const noexcept { return value_.empty(); }
    size_type size() const noexcept { return static_cast<size_type>(value_.size()); }
    size_type size_storage() const noexcept { return static_cast<size_type>(value_.size_storage()); }

    Result<code_point_type> get_code_point(size_type index) const noexcept {
        return detail::try_invoke([&] { return value_.at(static_cast<typename native_type::size_type>(index)); },
                                   "string.get_code_point");
    }
    Result<code_point_type> get_front() const noexcept {
        if (empty()) return Result<code_point_type>::failure(
            std::make_error_code(std::errc::invalid_argument), "String is empty", "string.get_front");
        return detail::try_invoke([&] { return value_.front(); }, "string.get_front");
    }
    Result<code_point_type> get_back() const noexcept {
        if (empty()) return Result<code_point_type>::failure(
            std::make_error_code(std::errc::invalid_argument), "String is empty", "string.get_back");
        return detail::try_invoke([&] { return value_.back(); }, "string.get_back");
    }

    size_type find(const BasicString &needle, size_type start = 0) const noexcept {
        if (start > size()) return npos;
        const auto result = value_.find(needle.value_, static_cast<typename native_type::size_type>(start));
        return result < 0 ? npos : static_cast<size_type>(result);
    }
    size_type rfind(const BasicString &needle, size_type start = npos) const noexcept {
        const auto native_start = start == npos
            ? native_type::npos
            : static_cast<typename native_type::size_type>(start);
        const auto result = value_.rfind(needle.value_, native_start);
        return result < 0 ? npos : static_cast<size_type>(result);
    }

    Result<BasicString> try_substr(size_type start, size_type count = npos) const noexcept {
        if (start > size()) return Result<BasicString>::failure(
            std::make_error_code(std::errc::invalid_argument), "String index is out of range", "string.substr");
        return detail::try_invoke([&] {
            const auto native_count = count == npos ? native_type::npos
                : static_cast<typename native_type::size_type>(count);
            return BasicString(value_.substr(static_cast<typename native_type::size_type>(start), native_count));
        }, "string.substr");
    }

    Result<void> try_append(const BasicString &other) noexcept {
        return detail::try_invoke([&] { value_ += other.value_; }, "string.append");
    }
    Result<void> try_insert(size_type index, const BasicString &other) noexcept {
        if (index > size()) return Result<void>::failure(
            std::make_error_code(std::errc::invalid_argument), "String index is out of range", "string.insert");
        return detail::try_invoke([&] {
            value_.insert(static_cast<typename native_type::size_type>(index), other.value_);
        }, "string.insert");
    }
    Result<void> try_erase(size_type index, size_type count = npos) noexcept {
        if (index > size()) return Result<void>::failure(
            std::make_error_code(std::errc::invalid_argument), "String index is out of range", "string.erase");
        return detail::try_invoke([&] {
            value_.erase(static_cast<typename native_type::size_type>(index),
                         count == npos ? native_type::npos
                                        : static_cast<typename native_type::size_type>(count));
        }, "string.erase");
    }
    Result<void> try_replace(size_type index, size_type count, const BasicString &replacement) noexcept {
        if (index > size()) return Result<void>::failure(
            std::make_error_code(std::errc::invalid_argument), "String index is out of range", "string.replace");
        return detail::try_invoke([&] {
            value_.replace(static_cast<typename native_type::size_type>(index),
                           static_cast<typename native_type::size_type>(count), replacement.value_);
        }, "string.replace");
    }
    Result<void> try_replace_all(const BasicString &from, const BasicString &to) noexcept {
        if (from.empty()) return Result<void>::success();
        return detail::try_invoke([&] {
            size_type cursor = 0;
            while (cursor <= size()) {
                const auto match = find(from, cursor);
                if (match == npos) break;
                value_.replace(static_cast<typename native_type::size_type>(match),
                               static_cast<typename native_type::size_type>(from.size()), to.value_);
                cursor = match + to.size();
            }
        }, "string.replace_all");
    }
    Result<void> try_push_back(code_point_type code_point) noexcept {
        return detail::try_invoke([&] { value_.push_back(code_point); }, "string.push_back");
    }
    Result<void> try_resize(size_type count, code_point_type fill = code_point_type{}) noexcept {
        return detail::try_invoke([&] {
            value_.resize(static_cast<typename native_type::size_type>(count), fill);
        }, "string.resize");
    }

    Result<void> try_trim() noexcept {
        return detail::try_invoke([&] {
            size_type first = 0;
            size_type last = size();
            while (first < last && is_ascii_space(value_.at(static_cast<typename native_type::size_type>(first)))) ++first;
            while (last > first && is_ascii_space(value_.at(static_cast<typename native_type::size_type>(last - 1)))) --last;
            value_.erase(static_cast<typename native_type::size_type>(last), native_type::npos);
            value_.erase(0, static_cast<typename native_type::size_type>(first));
        }, "string.trim");
    }
    Result<void> try_lower_ascii() noexcept { return change_ascii_case(false); }
    Result<void> try_upper_ascii() noexcept { return change_ascii_case(true); }

    Result<std::vector<BasicString>> try_split(const BasicString &delimiter,
                                                bool keep_empty = true) const noexcept {
        return detail::try_invoke([&] {
            std::vector<BasicString> result;
            if (delimiter.empty()) {
                result.reserve(size());
                for (const auto code_point : value_) result.emplace_back(native_type(1, code_point));
                return result;
            }
            size_type begin = 0;
            while (begin <= size()) {
                const auto end = find(delimiter, begin);
                const auto length = end == npos ? size() - begin : end - begin;
                if (keep_empty || length != 0) result.emplace_back(value_.substr(
                    static_cast<typename native_type::size_type>(begin),
                    static_cast<typename native_type::size_type>(length)));
                if (end == npos) break;
                begin = end + delimiter.size();
            }
            return result;
        }, "string.split");
    }
    Result<std::vector<BasicString>> try_split(std::string_view delimiter,
                                                bool keep_empty = true) const noexcept {
        const auto converted = try_create_utf8(delimiter);
        if (!converted) return Result<std::vector<BasicString>>::failure(converted.error());
        return try_split(converted.value(), keep_empty);
    }

    Result<BasicString> try_repeat(size_type times) const noexcept {
        return detail::try_invoke([&] {
            BasicString result;
            for (size_type index = 0; index < times; ++index) result.value_ += value_;
            return result;
        }, "string.repeat");
    }

    template <class TargetEncoding,
              class TargetAllocator = std::allocator<typename TargetEncoding::storage_unit>>
    Result<BasicString<TargetEncoding, TargetAllocator>> try_convert() const noexcept {
        using Target = BasicString<TargetEncoding, TargetAllocator>;
        return detail::try_invoke([&] {
            Target result;
            CsString::convert(value_, result.get_native());
            return result;
        }, "string.convert");
    }

    Result<std::string> try_to_utf8() const noexcept {
        return detail::try_invoke([&] {
            CsString::CsString converted;
            CsString::convert(value_, converted);
            std::string result;
            result.reserve(static_cast<std::size_t>(converted.size_storage()));
            for (auto iter = converted.storage_begin(); iter != converted.storage_end(); ++iter)
                result.push_back(static_cast<char>(*iter));
            return result;
        }, "string.to_utf8");
    }
    Result<std::u16string> try_to_utf16() const noexcept {
        return detail::try_invoke([&] {
            CsString::CsString_utf16 converted;
            CsString::convert(value_, converted);
            std::u16string result;
            result.reserve(static_cast<std::size_t>(converted.size_storage()));
            for (auto iter = converted.storage_begin(); iter != converted.storage_end(); ++iter)
                result.push_back(static_cast<char16_t>(*iter));
            return result;
        }, "string.to_utf16");
    }

    /// @brief Explicit UTF-8 interop for logging, streams and legacy APIs.
    std::string to_utf8() const { return try_to_utf8().value(); }

    template <class OtherEncoding, class OtherAllocator>
    bool operator==(const BasicString<OtherEncoding, OtherAllocator> &other) const noexcept {
        const auto converted = other.template try_convert<Encoding, Allocator>();
        return converted && value_ == converted.value().value_;
    }

private:
    static bool is_ascii_space(code_point_type code_point) noexcept {
        const auto value = code_point.unicode();
        return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f' || value == '\v';
    }
    Result<void> change_ascii_case(bool upper) noexcept {
        return detail::try_invoke([&] {
            for (size_type index = 0; index < size(); ++index) {
                const auto code_point = value_.at(static_cast<typename native_type::size_type>(index));
                const auto value = code_point.unicode();
                if (upper && value >= 'a' && value <= 'z')
                    value_.replace(static_cast<typename native_type::size_type>(index), 1, 1,
                                   code_point_type(static_cast<char32_t>(value - ('a' - 'A'))));
                else if (!upper && value >= 'A' && value <= 'Z')
                    value_.replace(static_cast<typename native_type::size_type>(index), 1, 1,
                                   code_point_type(static_cast<char32_t>(value + ('a' - 'A'))));
            }
        }, upper ? "string.upper_ascii" : "string.lower_ascii");
    }

    native_type value_;
};

using String = BasicString<CsString::utf8>;
using String16 = BasicString<CsString::utf16>;

template <class Part>
inline void concat_append(std::string &result, Part &&part) {
    using Value = std::decay_t<Part>;
    if constexpr (std::is_arithmetic_v<Value>) result += std::to_string(part);
    else if constexpr (detail::is_basic_string<Value>::value) result.append(part.to_utf8());
    else if constexpr (std::is_convertible_v<Part, std::string_view>)
        result.append(std::string_view(std::forward<Part>(part)));
    else result.append(std::forward<Part>(part));
}

template <class... Parts>
inline std::string concat(Parts &&...parts) {
    std::string result;
    (concat_append(result, std::forward<Parts>(parts)), ...);
    return result;
}

namespace native = CsString;
using Utf8String = CsString::CsString;
using Utf16String = CsString::CsString_utf16;

} // namespace sindre::general::string

#if defined(SINDRE_WITH_RE2)
#include <memory>
#include <re2/re2.h>
namespace sindre::general::regex {
class Regex {
public:
    static Result<Regex> compile(std::string pattern) noexcept;
    bool match(std::string_view text) const noexcept;
    Result<bool> try_match(std::string_view text) const noexcept;
    Result<std::string> replace(std::string_view text, std::string_view replacement) const noexcept;
private:
    std::shared_ptr<::re2::RE2> pattern_;
};
}
#endif
