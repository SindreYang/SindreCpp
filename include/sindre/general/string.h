#pragma once

#include <sindre/general/core.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <iterator>
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
#include <cs_string_view.h>

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

template <class Encoding,
          class Allocator = std::allocator<typename Encoding::storage_unit>>
class BasicStringView;

template <class Value>
struct is_basic_string : std::false_type {};

template <class Encoding, class Allocator>
struct is_basic_string<BasicString<Encoding, Allocator>> : std::true_type {};

template <class Function>
auto try_invoke(Function &&function, std::string context)
    -> Result<std::decay_t<std::invoke_result_t<Function>>> {
    using Value = std::decay_t<std::invoke_result_t<Function>>;
#if defined(SINDRE_NO_EXCEPTIONS)
    // In a no-exceptions build the backend is compiled with exception support
    // disabled as well. Keep the same Result API without emitting try/catch,
    // which would otherwise make the public header uncompilable.
    (void)context;
    if constexpr (std::is_void_v<Value>) {
        std::forward<Function>(function)();
        return Result<void>::success();
    } else {
        return Result<Value>::success(std::forward<Function>(function)());
    }
#else
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
#endif
}

} // namespace detail

/// @brief CsString-backed Unicode string implementation.
///
/// `size()` and indexes use Unicode code points. Storage units, conversion and
/// malformed-input handling are delegated to CsString. Use `try_*` methods at
/// public failure boundaries; they convert backend exceptions into Result.
template <class Encoding, class Allocator>
class detail::BasicString {
private:
    using encoding_type = Encoding;
    using allocator_type = Allocator;
    using native_type = CsString::CsBasicString<Encoding, Allocator>;
    using native_code_point_type = CsString::CsChar;

public:
    using code_point_type = char32_t;
    using view_type = BasicStringView<Encoding, Allocator>;
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

    bool empty() const noexcept { return value_.empty(); }
    size_type size() const noexcept { return static_cast<size_type>(value_.size()); }
    size_type size_storage() const noexcept { return static_cast<size_type>(value_.size_storage()); }

    bool is_empty() const noexcept { return empty(); }
    size_type get_size() const noexcept { return size(); }
    size_type get_storage_size() const noexcept { return size_storage(); }
    view_type get_view() const noexcept;

    Result<code_point_type> try_get_code_point(size_type index) const noexcept {
        return detail::try_invoke([&] {
            return static_cast<code_point_type>(value_.at(
                static_cast<typename native_type::size_type>(index)).unicode());
        }, "string.get_code_point");
    }
    Result<code_point_type> try_get_front() const noexcept {
        if (empty()) return Result<code_point_type>::failure(
            std::make_error_code(std::errc::invalid_argument), "String is empty", "string.get_front");
        return detail::try_invoke([&] {
            return static_cast<code_point_type>(value_.front().unicode());
        }, "string.get_front");
    }
    Result<code_point_type> try_get_back() const noexcept {
        if (empty()) return Result<code_point_type>::failure(
            std::make_error_code(std::errc::invalid_argument), "String is empty", "string.get_back");
        return detail::try_invoke([&] {
            return static_cast<code_point_type>(value_.back().unicode());
        }, "string.get_back");
    }

    // Compatibility names retained for the first public String API.
    Result<code_point_type> get_code_point(size_type index) const noexcept {
        return try_get_code_point(index);
    }
    Result<code_point_type> get_front() const noexcept { return try_get_front(); }
    Result<code_point_type> get_back() const noexcept { return try_get_back(); }

    size_type find(const BasicString &needle, size_type start = 0) const noexcept {
        if (start > size()) return npos;
        const auto result = value_.find(needle.value_, static_cast<typename native_type::size_type>(start));
        return result < 0 ? npos : static_cast<size_type>(result);
    }
    size_type find(code_point_type code_point, size_type start = 0) const noexcept {
        if (start > size()) return npos;
        const auto result = value_.find(native_code_point_type(code_point),
                                        static_cast<typename native_type::size_type>(start));
        return result < 0 ? npos : static_cast<size_type>(result);
    }
    size_type rfind(const BasicString &needle, size_type start = npos) const noexcept {
        const auto native_start = start == npos
            ? native_type::npos
            : static_cast<typename native_type::size_type>(start);
        const auto result = value_.rfind(needle.value_, native_start);
        return result < 0 ? npos : static_cast<size_type>(result);
    }
    size_type rfind(code_point_type code_point, size_type start = npos) const noexcept {
        const auto native_start = start == npos
            ? native_type::npos
            : static_cast<typename native_type::size_type>(start);
        const auto result = value_.rfind(native_code_point_type(code_point), native_start);
        return result < 0 ? npos : static_cast<size_type>(result);
    }

    size_type find_first_of(const BasicString &characters, size_type start = 0) const noexcept {
        if (start > size()) return npos;
        const auto result = value_.find_first_of(
            characters.value_, static_cast<typename native_type::size_type>(start));
        return result < 0 ? npos : static_cast<size_type>(result);
    }
    size_type find_last_of(const BasicString &characters, size_type start = npos) const noexcept {
        const auto native_start = start == npos
            ? native_type::npos
            : static_cast<typename native_type::size_type>(start);
        const auto result = value_.find_last_of(characters.value_, native_start);
        return result < 0 ? npos : static_cast<size_type>(result);
    }
    size_type find_first_not_of(const BasicString &characters, size_type start = 0) const noexcept {
        if (start > size()) return npos;
        const auto result = value_.find_first_not_of(
            characters.value_, static_cast<typename native_type::size_type>(start));
        return result < 0 ? npos : static_cast<size_type>(result);
    }
    size_type find_last_not_of(const BasicString &characters, size_type start = npos) const noexcept {
        const auto native_start = start == npos
            ? native_type::npos
            : static_cast<typename native_type::size_type>(start);
        const auto result = value_.find_last_not_of(characters.value_, native_start);
        return result < 0 ? npos : static_cast<size_type>(result);
    }

    bool starts_with(const BasicString &prefix) const noexcept {
        return prefix.size() <= size() && find(prefix, 0) == 0;
    }
    bool ends_with(const BasicString &suffix) const noexcept {
        if (suffix.size() > size()) return false;
        return rfind(suffix, size() - suffix.size()) == size() - suffix.size();
    }
    bool contains(const BasicString &needle) const noexcept { return find(needle) != npos; }
    size_type count(const BasicString &needle) const noexcept {
        if (needle.empty()) return size() + 1;
        size_type result = 0;
        for (size_type cursor = 0; cursor <= size();) {
            const auto match = find(needle, cursor);
            if (match == npos) break;
            ++result;
            cursor = match + needle.size();
        }
        return result;
    }

    size_type find(const view_type &needle, size_type start = 0) const noexcept;
    size_type rfind(const view_type &needle, size_type start = npos) const noexcept;
    size_type find_first_of(const view_type &characters, size_type start = 0) const noexcept;
    size_type find_last_of(const view_type &characters, size_type start = npos) const noexcept;
    size_type find_first_not_of(const view_type &characters, size_type start = 0) const noexcept;
    size_type find_last_not_of(const view_type &characters, size_type start = npos) const noexcept;
    bool starts_with(const view_type &prefix) const noexcept;
    bool ends_with(const view_type &suffix) const noexcept;
    bool contains(const view_type &needle) const noexcept;
    size_type count(const view_type &needle) const noexcept;
    int compare(const BasicString &other) const noexcept;
    int compare(const view_type &other) const noexcept;

    Result<view_type> try_substr_view(size_type start, size_type count = npos) const noexcept;

    template <class Function>
    Result<void> try_for_each_code_point(Function &&function) const noexcept;

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
    Result<void> try_append(const view_type &other) noexcept;
    Result<void> try_append(code_point_type code_point, size_type count = 1) noexcept {
        return detail::try_invoke([&] {
            value_.append(static_cast<typename native_type::size_type>(count),
                          native_code_point_type(code_point));
        }, "string.append_code_point");
    }
    Result<void> try_assign(const BasicString &other) noexcept {
        return detail::try_invoke([&] { value_ = other.value_; }, "string.assign");
    }
    Result<void> try_assign(const view_type &other) noexcept;
    Result<void> try_clear() noexcept {
        return detail::try_invoke([&] { value_.clear(); }, "string.clear");
    }
    Result<void> try_pop_back() noexcept {
        if (empty()) return Result<void>::failure(
            std::make_error_code(std::errc::invalid_argument), "String is empty", "string.pop_back");
        return detail::try_invoke([&] { value_.pop_back(); }, "string.pop_back");
    }
    Result<void> try_swap(BasicString &other) noexcept {
        return detail::try_invoke([&] { value_.swap(other.value_); }, "string.swap");
    }
    Result<void> try_shrink_to_fit() noexcept {
        return detail::try_invoke([&] { value_.shrink_to_fit(); }, "string.shrink_to_fit");
    }
    Result<void> try_insert(size_type index, const BasicString &other) noexcept {
        if (index > size()) return Result<void>::failure(
            std::make_error_code(std::errc::invalid_argument), "String index is out of range", "string.insert");
        return detail::try_invoke([&] {
            value_.insert(static_cast<typename native_type::size_type>(index), other.value_);
        }, "string.insert");
    }
    Result<void> try_insert(size_type index, const view_type &other) noexcept;
    Result<void> try_erase(size_type index, size_type count = npos) noexcept {
        if (index > size()) return Result<void>::failure(
            std::make_error_code(std::errc::invalid_argument), "String index is out of range", "string.erase");
        return detail::try_invoke([&] {
            value_.erase(static_cast<typename native_type::size_type>(index),
                         count == npos ? native_type::npos
                                        : static_cast<typename native_type::size_type>(count));
        }, "string.erase");
    }
    Result<void> try_insert(size_type index, code_point_type code_point, size_type count = 1) noexcept {
        if (index > size()) return Result<void>::failure(
            std::make_error_code(std::errc::invalid_argument), "String index is out of range", "string.insert");
        return detail::try_invoke([&] {
            value_.insert(static_cast<typename native_type::size_type>(index),
                          static_cast<typename native_type::size_type>(count),
                          native_code_point_type(code_point));
        }, "string.insert_code_point");
    }
    Result<void> try_replace(size_type index, size_type count, const BasicString &replacement) noexcept {
        if (index > size()) return Result<void>::failure(
            std::make_error_code(std::errc::invalid_argument), "String index is out of range", "string.replace");
        return detail::try_invoke([&] { replace_range_unchecked(index, count, replacement); }, "string.replace");
    }
    Result<void> try_replace(size_type index, size_type count,
                             code_point_type code_point, size_type repeat_count) noexcept {
        if (index > size()) return Result<void>::failure(
            std::make_error_code(std::errc::invalid_argument), "String index is out of range", "string.replace");
        return detail::try_invoke([&] {
            value_.replace(static_cast<typename native_type::size_type>(index),
                           count == npos ? native_type::npos
                                         : static_cast<typename native_type::size_type>(count),
                           static_cast<typename native_type::size_type>(repeat_count),
                           native_code_point_type(code_point));
        }, "string.replace_code_point");
    }
    Result<void> try_replace(size_type index, size_type count,
                             const view_type &replacement) noexcept;
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
    Result<void> try_replace_all(const view_type &from, const view_type &to) noexcept;
    /// @brief 按 Unicode code point 修改自身；必要时允许后端重新分配内存。
    Result<void> replace(size_type index, size_type count, const BasicString &replacement) noexcept {
        return try_replace(index, count, replacement);
    }
    Result<void> replace(size_type index, size_type count, const view_type &replacement) noexcept {
        return try_replace(index, count, replacement);
    }
    /// @brief 原地替换的显式命名版本。
    Result<void> replace_in_place(size_type index, size_type count,
                                  const BasicString &replacement) noexcept {
        return try_replace(index, count, replacement);
    }
    Result<void> replace_in_place(size_type index, size_type count,
                                  const view_type &replacement) noexcept {
        return try_replace(index, count, replacement);
    }
    /// @brief 修改自身并替换全部非重叠匹配。
    Result<void> replace_all(const BasicString &from, const BasicString &to) noexcept {
        return try_replace_all(from, to);
    }
    Result<void> replace_all(const view_type &from, const view_type &to) noexcept {
        return try_replace_all(from, to);
    }
    Result<void> try_push_back(code_point_type code_point) noexcept {
        return detail::try_invoke([&] { value_.push_back(native_code_point_type(code_point)); }, "string.push_back");
    }
    Result<void> try_resize(size_type count, code_point_type fill = code_point_type{}) noexcept {
        return detail::try_invoke([&] {
            value_.resize(static_cast<typename native_type::size_type>(count), native_code_point_type(fill));
        }, "string.resize");
    }

    Result<void> try_trim() noexcept {
        return detail::try_invoke([&] {
            size_type first = 0;
            size_type last = size();
            while (first < last && is_ascii_space(static_cast<code_point_type>(value_.at(
                       static_cast<typename native_type::size_type>(first)).unicode()))) ++first;
            while (last > first && is_ascii_space(static_cast<code_point_type>(value_.at(
                       static_cast<typename native_type::size_type>(last - 1)).unicode()))) --last;
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
                for (const auto code_point : value_)
                    result.emplace_back(native_type(1, code_point));
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
        return Result<char32_t>::success(code_point.value());
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
    const native_type &get_native() const noexcept { return value_; }
    native_type &get_native() noexcept { return value_; }

    template <class TargetEncoding,
              class TargetAllocator = std::allocator<typename TargetEncoding::storage_unit>>
    Result<BasicString<TargetEncoding, TargetAllocator>> try_convert() const noexcept {
        using Target = BasicString<TargetEncoding, TargetAllocator>;
        return detail::try_invoke([&] {
            Target result;
            CsString::convert(value_, result.value_);
            return result;
        }, "string.convert");
    }

    template <class OtherEncoding, class OtherAllocator>
    friend class BasicString;
    template <class OtherEncoding, class OtherAllocator>
    friend class BasicStringView;

    void replace_range_unchecked(size_type index, size_type count,
                                 const BasicString &replacement) {
        // Use CsString's code-point iterator implementation directly. Building
        // the result from substr() can leave stale storage for multibyte UTF-8
        // ranges in the fixed CsString backend.
        value_.replace(static_cast<typename native_type::size_type>(index),
                       count == npos ? native_type::npos
                                     : static_cast<typename native_type::size_type>(count),
                       replacement.value_);
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
            result.push_back(native_code_point_type(data[index]));
        return result;
    }
    static bool is_ascii_space(code_point_type code_point) noexcept {
        return code_point == U' ' || code_point == U'\t' || code_point == U'\n' ||
               code_point == U'\r' || code_point == U'\f' || code_point == U'\v';
    }
    Result<void> change_ascii_case(bool upper) noexcept {
        return detail::try_invoke([&] {
            for (size_type index = 0; index < size(); ++index) {
                const auto code_point = value_.at(static_cast<typename native_type::size_type>(index));
                const auto value = code_point.unicode();
                if (upper && value >= 'a' && value <= 'z')
                    value_.replace(static_cast<typename native_type::size_type>(index), 1, 1,
                    native_code_point_type(static_cast<char32_t>(value - ('a' - 'A'))));
                else if (!upper && value >= 'A' && value <= 'Z')
                    value_.replace(static_cast<typename native_type::size_type>(index), 1, 1,
                                   native_code_point_type(static_cast<char32_t>(value + ('a' - 'A'))));
            }
        }, upper ? "string.upper_ascii" : "string.lower_ascii");
    }

    native_type value_;
};

/// @brief Non-owning Unicode view over a String.
///
/// The view does not copy text. It remains valid only while the source String
/// remains alive and is not modified in a way that invalidates its storage.
template <class Encoding, class Allocator>
class detail::BasicStringView {
public:
    using string_type = BasicString<Encoding, Allocator>;
    using size_type = std::size_t;
    using code_point_type = char32_t;

    static constexpr size_type npos = std::numeric_limits<size_type>::max();

    BasicStringView() = default;

    bool is_empty() const noexcept { return native_view_.empty(); }
    size_type get_size() const noexcept {
        const auto value = native_view_.size();
        return value < 0 ? 0 : static_cast<size_type>(value);
    }
    size_type get_storage_size() const noexcept {
        return static_cast<size_type>(std::distance(native_view_.storage_begin(), native_view_.storage_end()));
    }

    Result<code_point_type> try_get_code_point(size_type index) const noexcept {
        if (index >= get_size()) return Result<code_point_type>::failure(
            std::make_error_code(std::errc::result_out_of_range),
            "String view index is out of range", "string.view.get_code_point");
        return detail::try_invoke([&] {
            return static_cast<code_point_type>(native_view_.at(
                static_cast<typename native_view_type::size_type>(index)).unicode());
        }, "string.view.get_code_point");
    }
    Result<code_point_type> try_get_front() const noexcept {
        if (is_empty()) return Result<code_point_type>::failure(
            std::make_error_code(std::errc::invalid_argument), "String view is empty", "string.view.get_front");
        return detail::try_invoke([&] {
            return static_cast<code_point_type>(native_view_.front().unicode());
        }, "string.view.get_front");
    }
    Result<code_point_type> try_get_back() const noexcept {
        if (is_empty()) return Result<code_point_type>::failure(
            std::make_error_code(std::errc::invalid_argument), "String view is empty", "string.view.get_back");
        return detail::try_invoke([&] {
            return static_cast<code_point_type>(native_view_.back().unicode());
        }, "string.view.get_back");
    }

    size_type find(const string_type &needle, size_type start = 0) const noexcept {
        if (start > get_size()) return npos;
        const auto begin = get_iterator_at(start);
        const auto found = native_view_.find_fast(native_view_type(needle.get_native()), begin);
        return found == native_view_.cend() ? npos : iterator_index(found);
    }
    size_type find(const BasicStringView &needle, size_type start = 0) const noexcept {
        if (start > get_size()) return npos;
        const auto begin = get_iterator_at(start);
        const auto found = native_view_.find_fast(needle.native_view_, begin);
        return found == native_view_.cend() ? npos : iterator_index(found);
    }
    size_type find(code_point_type code_point, size_type start = 0) const noexcept {
        if (start > get_size()) return npos;
        auto iterator = get_iterator_at(start);
        for (; iterator != native_view_.cend(); ++iterator)
            if (static_cast<code_point_type>((*iterator).unicode()) == code_point)
                return iterator_index(iterator);
        return npos;
    }
    size_type rfind(const string_type &needle, size_type start = npos) const noexcept {
        return rfind(BasicStringView(needle), start);
    }
    size_type rfind(const BasicStringView &needle, size_type start = npos) const noexcept {
        const auto limit = start == npos ? get_size() : std::min(start, get_size());
        if (needle.is_empty()) return limit;
        size_type result = npos;
        for (size_type cursor = 0; cursor <= limit;) {
            const auto found = find(needle, cursor);
            if (found == npos || found > limit) break;
            result = found;
            cursor = found + needle.get_size();
        }
        return result;
    }
    size_type rfind(code_point_type code_point, size_type start = npos) const noexcept {
        if (is_empty()) return npos;
        const auto last = start == npos ? get_size() - 1 : std::min(start, get_size() - 1);
        for (size_type index = last + 1; index > 0; --index)
            if (static_cast<code_point_type>(native_view_.at(
                    static_cast<typename native_view_type::size_type>(index - 1)).unicode()) == code_point)
                return index - 1;
        return npos;
    }

    size_type find_first_of(const BasicStringView &characters, size_type start = 0) const noexcept {
        return find_character_set(characters, start, false, false);
    }
    size_type find_last_of(const BasicStringView &characters, size_type start = npos) const noexcept {
        return find_character_set(characters, start, true, false);
    }
    size_type find_first_not_of(const BasicStringView &characters, size_type start = 0) const noexcept {
        return find_character_set(characters, start, false, true);
    }
    size_type find_last_not_of(const BasicStringView &characters, size_type start = npos) const noexcept {
        return find_character_set(characters, start, true, true);
    }

    size_type find_first_of(const string_type &characters, size_type start = 0) const noexcept {
        return find_first_of(BasicStringView(characters), start);
    }
    size_type find_last_of(const string_type &characters, size_type start = npos) const noexcept {
        return find_last_of(BasicStringView(characters), start);
    }
    size_type find_first_not_of(const string_type &characters, size_type start = 0) const noexcept {
        return find_first_not_of(BasicStringView(characters), start);
    }
    size_type find_last_not_of(const string_type &characters, size_type start = npos) const noexcept {
        return find_last_not_of(BasicStringView(characters), start);
    }

    bool starts_with(const BasicStringView &prefix) const noexcept {
        return native_view_.startsWith(prefix.native_view_);
    }
    bool ends_with(const BasicStringView &suffix) const noexcept {
        return native_view_.endsWith(suffix.native_view_);
    }
    bool contains(const BasicStringView &needle) const noexcept { return find(needle) != npos; }
    size_type count(const BasicStringView &needle) const noexcept {
        if (needle.is_empty()) return get_size() + 1;
        size_type result = 0;
        for (size_type cursor = 0; cursor <= get_size();) {
            const auto found = find(needle, cursor);
            if (found == npos) break;
            ++result;
            cursor = found + needle.get_size();
        }
        return result;
    }
    bool starts_with(const string_type &prefix) const noexcept { return starts_with(BasicStringView(prefix)); }
    bool ends_with(const string_type &suffix) const noexcept { return ends_with(BasicStringView(suffix)); }
    bool contains(const string_type &needle) const noexcept { return contains(BasicStringView(needle)); }
    size_type count(const string_type &needle) const noexcept { return count(BasicStringView(needle)); }
    int compare(const BasicStringView &other) const noexcept {
        return native_view_.compare(other.native_view_);
    }
    int compare(const string_type &other) const noexcept { return compare(BasicStringView(other)); }

    Result<BasicStringView> try_substr(size_type start = 0, size_type count = npos) const noexcept {
        if (start > get_size()) return Result<BasicStringView>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "String view index is out of range", "string.view.substr");
        return detail::try_invoke([&] {
            const auto native_count = count == npos
                ? native_view_type::npos
                : static_cast<typename native_view_type::size_type>(count);
            return BasicStringView(native_view_.substr(
                static_cast<typename native_view_type::size_type>(start), native_count));
        }, "string.view.substr");
    }
    BasicStringView remove_prefix(size_type count) const noexcept {
        return BasicStringView(native_view_.remove_prefix(
            static_cast<typename native_view_type::size_type>(count)));
    }
    BasicStringView remove_suffix(size_type count) const noexcept {
        return BasicStringView(native_view_.remove_suffix(
            static_cast<typename native_view_type::size_type>(count)));
    }
    Result<string_type> try_to_string() const noexcept {
        return detail::try_invoke([&] { return string_type(native_view_); }, "string.view.to_string");
    }

    template <class Function>
    Result<void> try_for_each_code_point(Function &&function) const noexcept {
        return detail::try_invoke([&] {
            for (const auto code_point : native_view_)
                std::invoke(function, static_cast<code_point_type>(code_point.unicode()));
        }, "string.view.for_each_code_point");
    }

private:
    using native_type = typename string_type::native_type;
    using native_view_type = CsString::CsBasicStringView<native_type>;
    using native_iterator = typename native_view_type::const_iterator;

    explicit BasicStringView(const string_type &value) : native_view_(value.get_native()) {}
    explicit BasicStringView(const native_type &value) : native_view_(value) {}
    explicit BasicStringView(native_view_type value) : native_view_(std::move(value)) {}

    native_iterator get_iterator_at(size_type index) const noexcept {
        auto result = native_view_.cbegin();
        std::advance(result, static_cast<typename native_view_type::difference_type>(index));
        return result;
    }
    size_type iterator_index(native_iterator iterator) const noexcept {
        return static_cast<size_type>(std::distance(native_view_.cbegin(), iterator));
    }
    size_type find_character_set(const BasicStringView &characters, size_type start,
                                 bool reverse, bool negated) const noexcept {
        const auto size = get_size();
        if (start != npos && start > size) return npos;
        if (characters.is_empty()) {
            if (!negated || size == 0) return npos;
            if (!reverse) return start < size ? start : npos;
            return start == npos ? size - 1 : std::min(start, size - 1);
        }
        if (is_empty()) return npos;
        if (!reverse) {
            if (start >= size) return npos;
            for (size_type index = start; index < size; ++index) {
                const auto current = native_view_.at(static_cast<typename native_view_type::size_type>(index));
                bool matched = false;
                for (size_type candidate_index = 0;
                     candidate_index < characters.get_size(); ++candidate_index) {
                    const auto candidate = characters.try_get_code_point(candidate_index);
                    if (candidate && candidate.value() == current.unicode()) {
                        matched = true;
                        break;
                    }
                }
                if (matched != negated) return index;
            }
            return npos;
        }
        const auto last = start == npos ? size - 1 : std::min(start, size - 1);
        for (size_type index = last + 1; index > 0; --index) {
            const auto current = native_view_.at(static_cast<typename native_view_type::size_type>(index - 1));
            bool matched = false;
            for (size_type candidate_index = 0;
                 candidate_index < characters.get_size(); ++candidate_index) {
                const auto candidate = characters.try_get_code_point(candidate_index);
                if (candidate && candidate.value() == current.unicode()) {
                    matched = true;
                    break;
                }
            }
            if (matched != negated) return index - 1;
        }
        return npos;
    }

    friend class BasicString<Encoding, Allocator>;
    native_view_type native_view_;
};

template <class Encoding, class Allocator>
typename detail::BasicString<Encoding, Allocator>::view_type
detail::BasicString<Encoding, Allocator>::get_view() const noexcept {
    return view_type(value_);
}

template <class Encoding, class Allocator>
typename detail::BasicString<Encoding, Allocator>::size_type
detail::BasicString<Encoding, Allocator>::find(
    const view_type &needle, size_type start) const noexcept {
    return get_view().find(needle, start);
}

template <class Encoding, class Allocator>
typename detail::BasicString<Encoding, Allocator>::size_type
detail::BasicString<Encoding, Allocator>::rfind(
    const view_type &needle, size_type start) const noexcept {
    return get_view().rfind(needle, start);
}

template <class Encoding, class Allocator>
typename detail::BasicString<Encoding, Allocator>::size_type
detail::BasicString<Encoding, Allocator>::find_first_of(
    const view_type &characters, size_type start) const noexcept {
    return get_view().find_first_of(characters, start);
}

template <class Encoding, class Allocator>
typename detail::BasicString<Encoding, Allocator>::size_type
detail::BasicString<Encoding, Allocator>::find_last_of(
    const view_type &characters, size_type start) const noexcept {
    return get_view().find_last_of(characters, start);
}

template <class Encoding, class Allocator>
typename detail::BasicString<Encoding, Allocator>::size_type
detail::BasicString<Encoding, Allocator>::find_first_not_of(
    const view_type &characters, size_type start) const noexcept {
    return get_view().find_first_not_of(characters, start);
}

template <class Encoding, class Allocator>
typename detail::BasicString<Encoding, Allocator>::size_type
detail::BasicString<Encoding, Allocator>::find_last_not_of(
    const view_type &characters, size_type start) const noexcept {
    return get_view().find_last_not_of(characters, start);
}

template <class Encoding, class Allocator>
bool detail::BasicString<Encoding, Allocator>::starts_with(
    const view_type &prefix) const noexcept {
    return get_view().starts_with(prefix);
}

template <class Encoding, class Allocator>
bool detail::BasicString<Encoding, Allocator>::ends_with(
    const view_type &suffix) const noexcept {
    return get_view().ends_with(suffix);
}

template <class Encoding, class Allocator>
bool detail::BasicString<Encoding, Allocator>::contains(
    const view_type &needle) const noexcept {
    return get_view().contains(needle);
}

template <class Encoding, class Allocator>
typename detail::BasicString<Encoding, Allocator>::size_type
detail::BasicString<Encoding, Allocator>::count(
    const view_type &needle) const noexcept {
    return get_view().count(needle);
}

template <class Encoding, class Allocator>
int detail::BasicString<Encoding, Allocator>::compare(
    const BasicString &other) const noexcept {
    return get_view().compare(other.get_view());
}

template <class Encoding, class Allocator>
int detail::BasicString<Encoding, Allocator>::compare(
    const view_type &other) const noexcept {
    return get_view().compare(other);
}

template <class Encoding, class Allocator>
Result<typename detail::BasicString<Encoding, Allocator>::view_type>
detail::BasicString<Encoding, Allocator>::try_substr_view(
    size_type start, size_type count) const noexcept {
    return get_view().try_substr(start, count);
}

template <class Encoding, class Allocator>
template <class Function>
Result<void> detail::BasicString<Encoding, Allocator>::try_for_each_code_point(
    Function &&function) const noexcept {
    return get_view().try_for_each_code_point(std::forward<Function>(function));
}

template <class Encoding, class Allocator>
Result<void> detail::BasicString<Encoding, Allocator>::try_append(
    const view_type &other) noexcept {
    auto owned = other.try_to_string();
    if (!owned) return Result<void>::failure(owned.error());
    return try_append(owned.value());
}

template <class Encoding, class Allocator>
Result<void> detail::BasicString<Encoding, Allocator>::try_assign(
    const view_type &other) noexcept {
    auto owned = other.try_to_string();
    if (!owned) return Result<void>::failure(owned.error());
    return try_assign(owned.value());
}

template <class Encoding, class Allocator>
Result<void> detail::BasicString<Encoding, Allocator>::try_insert(
    size_type index, const view_type &other) noexcept {
    if (index > size()) return Result<void>::failure(
        std::make_error_code(std::errc::invalid_argument), "String index is out of range", "string.insert");
    auto owned = other.try_to_string();
    if (!owned) return Result<void>::failure(owned.error());
    return try_insert(index, owned.value());
}

template <class Encoding, class Allocator>
Result<void> detail::BasicString<Encoding, Allocator>::try_replace(
    size_type index, size_type count, const view_type &replacement) noexcept {
    auto owned = replacement.try_to_string();
    if (!owned) return Result<void>::failure(owned.error());
    return try_replace(index, count, owned.value());
}

template <class Encoding, class Allocator>
Result<void> detail::BasicString<Encoding, Allocator>::try_replace_all(
    const view_type &from, const view_type &to) noexcept {
    auto source = from.try_to_string();
    if (!source) return Result<void>::failure(source.error());
    auto replacement = to.try_to_string();
    if (!replacement) return Result<void>::failure(replacement.error());
    return try_replace_all(source.value(), replacement.value());
}

/// @brief The single public string type used by Sindre.
///
/// Callers do not select an encoding policy. CsString owns the representation
/// and conversion details internally; construct this type from UTF-8 or UTF-16
/// input and use `to_utf8()` / `to_utf16()` when crossing an external API.
using String = detail::BasicString<CsString::utf8>;
using StringView = detail::BasicStringView<CsString::utf8>;

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
