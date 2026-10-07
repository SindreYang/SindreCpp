#pragma once

#include <sindre/general/core.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
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

namespace detail {

template <class Encoding,
          class Allocator = std::allocator<typename Encoding::storage_unit>>
class BasicString;

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

/// @brief CsString-backed Unicode string implementation.
///
/// `size()` and indexes use Unicode code points. Storage units, conversion and
/// malformed-input handling are delegated to CsString. Use `try_*` methods at
/// public failure boundaries; they convert backend exceptions into Result.
template <class Encoding, class Allocator>
class detail::BasicString {
public:
    using encoding_type = Encoding;
    using allocator_type = Allocator;
    using native_type = CsString::CsBasicString<Encoding, Allocator>;
    using code_point_type = CsString::CsChar;
    using size_type = std::size_t;

    static constexpr size_type npos = std::numeric_limits<size_type>::max();

    BasicString() = default;
    explicit BasicString(bool value) : BasicString(value ? "true" : "false") {}
    explicit BasicString(char value) : BasicString(std::string(1, value)) {}
    template <class Integer,
              std::enable_if_t<std::is_integral_v<Integer> &&
                               !std::is_same_v<Integer, bool> &&
                               !std::is_same_v<Integer, char>, int> = 0>
    explicit BasicString(Integer value) : BasicString(std::to_string(value)) {}
    template <class Floating,
              std::enable_if_t<std::is_floating_point_v<Floating>, int> = 0>
    explicit BasicString(Floating value) : BasicString(std::to_string(value)) {}
    explicit BasicString(const char *text)
        : value_(native_type::fromUtf8(text ? text : "")) {}
    explicit BasicString(const char16_t *text)
        : BasicString(std::u16string_view(text ? text : u"")) {}
    explicit BasicString(const char32_t *text)
        : BasicString(std::u32string_view(text ? text : U"")) {}
    explicit BasicString(const wchar_t *text)
        : BasicString(std::wstring_view(text ? text : L"")) {}
    explicit BasicString(std::string_view text)
        : value_(native_type::fromUtf8(text.data(), static_cast<typename native_type::size_type>(text.size()))) {}
    explicit BasicString(std::u16string_view text)
        : value_(native_type::fromUtf16(text.data(), static_cast<typename native_type::size_type>(text.size()))) {}
    explicit BasicString(std::u32string_view text)
        : value_(from_code_points(text.data(), text.size())) {}
    explicit BasicString(std::wstring_view text)
        : value_(from_wide(text)) {}
    template <std::size_t Size>
    explicit BasicString(const char16_t (&text)[Size])
        : BasicString(std::u16string_view(text, Size > 0 ? Size - 1 : 0)) {}
    explicit BasicString(const native_type &value) : value_(value) {}
    explicit BasicString(native_type &&value) noexcept : value_(std::move(value)) {}

    static Result<BasicString> from(std::string_view text) noexcept {
        return try_create_utf8(text);
    }
    static Result<BasicString> from(const char *text) noexcept {
        return from(std::string_view(text ? text : ""));
    }
    static Result<BasicString> from(std::u16string_view text) noexcept {
        return try_create_utf16(text);
    }
    static Result<BasicString> from(const char16_t *text) noexcept {
        return from(std::u16string_view(text ? text : u""));
    }
    static Result<BasicString> from(std::u32string_view text) noexcept {
        return detail::try_invoke([&] { return BasicString(text); }, "string.from_utf32");
    }
    static Result<BasicString> from(const char32_t *text) noexcept {
        return from(std::u32string_view(text ? text : U""));
    }
    static Result<BasicString> from(std::wstring_view text) noexcept {
        return detail::try_invoke([&] { return BasicString(text); }, "string.from_wide");
    }
    static Result<BasicString> from(const wchar_t *text) noexcept {
        return from(std::wstring_view(text ? text : L""));
    }
    static Result<BasicString> from(const std::filesystem::path &path) noexcept {
        return detail::try_invoke([&] {
#if defined(_WIN32)
            const auto native = path.native();
            return BasicString(std::wstring_view(native));
#else
            const auto native = path.native();
            return BasicString(std::string_view(native.data(), native.size()));
#endif
        }, "string.from_path");
    }
    static Result<BasicString> from(const BasicString &text) noexcept {
        return detail::try_invoke([&] { return BasicString(text); }, "string.from_string");
    }
    static Result<BasicString> from(bool value) noexcept {
        return detail::try_invoke([&] { return BasicString(value); }, "string.from_bool");
    }
    static Result<BasicString> from(char value) noexcept {
        return detail::try_invoke([&] { return BasicString(value); }, "string.from_char");
    }
    template <class Integer,
              std::enable_if_t<std::is_integral_v<Integer> &&
                               !std::is_same_v<Integer, bool> &&
                               !std::is_same_v<Integer, char>, int> = 0>
    static Result<BasicString> from(Integer value) noexcept {
        return detail::try_invoke([&] { return BasicString(value); }, "string.from_int");
    }
    template <class Floating,
              std::enable_if_t<std::is_floating_point_v<Floating>, int> = 0>
    static Result<BasicString> from(Floating value) noexcept {
        return detail::try_invoke([&] { return BasicString(value); }, "string.from_float");
    }

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
        return detail::try_invoke([&] { replace_range_unchecked(index, count, replacement); }, "string.replace");
    }
    Result<void> try_replace_all(const BasicString &from, const BasicString &to) noexcept {
        if (from.empty()) return Result<void>::success();
        return detail::try_invoke([&] {
            size_type cursor = 0;
            while (cursor <= size()) {
                const auto match = find(from, cursor);
                if (match == npos) break;
                replace_range_unchecked(match, from.size(), to);
                cursor = match + to.size();
            }
        }, "string.replace_all");
    }
    /// @brief 按 Unicode code point 修改自身；必要时允许后端重新分配内存。
    Result<void> replace(size_type index, size_type count, const BasicString &replacement) noexcept {
        return try_replace(index, count, replacement);
    }
    /// @brief 原地替换的显式命名版本。
    Result<void> replace_in_place(size_type index, size_type count,
                                  const BasicString &replacement) noexcept {
        return try_replace(index, count, replacement);
    }
    /// @brief 修改自身并替换全部非重叠匹配。
    Result<void> replace_all(const BasicString &from, const BasicString &to) noexcept {
        return try_replace_all(from, to);
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
    /// @brief Explicit UTF-16 interop for Windows and UTF-16 based APIs.
    std::u16string to_utf16() const { return try_to_utf16().value(); }
    /// @brief Return the single Unicode code point represented by this string.
    Result<char32_t> to_code_point() const noexcept {
        if (size() != 1) return Result<char32_t>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "String must contain exactly one Unicode code point", "string.to_code_point");
        const auto code_point = get_code_point(0);
        if (!code_point) return Result<char32_t>::failure(code_point.error());
        return Result<char32_t>::success(static_cast<char32_t>(code_point.value().unicode()));
    }
    /// @brief Convert to a single ASCII char; non-ASCII code points fail.
    Result<char> to_char() const noexcept {
        const auto code_point = to_code_point();
        if (!code_point) return Result<char>::failure(code_point.error());
        if (code_point.value() > 0x7F) return Result<char>::failure(
            std::make_error_code(std::errc::illegal_byte_sequence),
            "Unicode code point does not fit in char", "string.to_char");
        return Result<char>::success(static_cast<char>(code_point.value()));
    }
    /// @brief Convert to a single Unicode code point.
    Result<char32_t> to_char32() const noexcept { return to_code_point(); }
    /// @brief Convert to the standard narrow string representation (UTF-8).
    std::string to_stdstr() const { return to_utf8(); }
    std::string to_std_string() const { return to_utf8(); }
    /// @brief Convert to the platform standard filesystem path.
    Result<std::filesystem::path> to_path() const noexcept {
        const auto converted = try_to_utf8();
        if (!converted) return Result<std::filesystem::path>::failure(converted.error());
        return detail::try_invoke([&] {
#if defined(_WIN32)
            return std::filesystem::u8path(converted.value());
#else
            return std::filesystem::path(converted.value());
#endif
        }, "string.to_path");
    }
    /// @brief Parse the string as a strict integer.
    Result<std::int64_t> to_int(int base = 10) const noexcept {
        const auto converted = try_to_utf8();
        if (!converted) return Result<std::int64_t>::failure(converted.error());
        return parse_int(converted.value(), base);
    }
    /// @brief Parse the string as a strict finite floating-point value.
    Result<double> to_float() const noexcept {
        const auto converted = try_to_utf8();
        if (!converted) return Result<double>::failure(converted.error());
        return parse_float(converted.value());
    }
    /// @brief Parse common Python-style boolean spellings.
    Result<bool> to_bool() const noexcept {
        const auto converted = try_to_utf8();
        if (!converted) return Result<bool>::failure(converted.error());
        const auto normalized = lower_ascii(trim(converted.value()));
        if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on")
            return Result<bool>::success(true);
        if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off")
            return Result<bool>::success(false);
        return Result<bool>::failure(std::make_error_code(std::errc::invalid_argument),
                                     "Expected a boolean value", "string.to_bool");
    }

    template <class OtherEncoding, class OtherAllocator>
    bool operator==(const BasicString<OtherEncoding, OtherAllocator> &other) const noexcept {
        const auto converted = other.template try_convert<Encoding, Allocator>();
        return converted && value_ == converted.value().value_;
    }

private:
    void replace_range_unchecked(size_type index, size_type count,
                                 const BasicString &replacement) {
        const auto suffix_start = count > size() - index ? size() : index + count;
        auto result = value_.substr(0, static_cast<typename native_type::size_type>(index));
        result += replacement.value_;
        result += value_.substr(static_cast<typename native_type::size_type>(suffix_start), native_type::npos);
        value_ = std::move(result);
    }
    static native_type from_wide(std::wstring_view text) {
        if constexpr (sizeof(wchar_t) == sizeof(char16_t)) {
            return native_type::fromUtf16(
                reinterpret_cast<const char16_t *>(text.data()),
                static_cast<typename native_type::size_type>(text.size()));
        } else {
            return from_code_points(
                reinterpret_cast<const char32_t *>(text.data()),
                text.size());
        }
    }
    static native_type from_code_points(const char32_t *data, size_type count) {
        native_type result;
        for (size_type index = 0; index < count; ++index)
            result.push_back(code_point_type(data[index]));
        return result;
    }
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

/// @brief The single public string type used by Sindre.
///
/// Callers do not select an encoding policy. CsString owns the representation
/// and conversion details internally; construct this type from UTF-8 or UTF-16
/// input and use `to_utf8()` / `to_utf16()` when crossing an external API.
using String = detail::BasicString<CsString::utf8>;

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
