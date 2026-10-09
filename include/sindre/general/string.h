#pragma once

/// @file
/// @brief UTF-8 字符串、路径转换和正则表达式接口。
///
/// Public declarations use only the standard library. The fixed CsString and
/// RE2 backends are implementation details of the General runtime.

#include <sindre/general/core.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <memory>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace sindre::general::string {

inline constexpr std::size_t npos = std::string_view::npos;

std::string_view trim(std::string_view text) noexcept;
std::string trim_copy(std::string_view text);
bool starts_with(std::string_view text, std::string_view prefix) noexcept;
bool ends_with(std::string_view text, std::string_view suffix) noexcept;
bool contains(std::string_view text, std::string_view needle) noexcept;
std::size_t count(std::string_view text, std::string_view needle) noexcept;
std::string_view lstrip(std::string_view text, std::string_view chars = " \t\n\r\f\v") noexcept;
std::string_view rstrip(std::string_view text, std::string_view chars = " \t\n\r\f\v") noexcept;
std::string_view strip(std::string_view text, std::string_view chars = " \t\n\r\f\v") noexcept;
std::string replace_all(std::string_view text, std::string_view from, std::string_view to);
std::vector<std::string_view> split_view(std::string_view text, char delimiter);
std::vector<std::string> split(std::string_view text, char delimiter);
std::vector<std::string> split(std::string_view text, std::string_view delimiter,
                               bool keep_empty = true);
std::vector<std::string> rsplit(std::string_view text, std::string_view delimiter,
                                std::size_t max_splits = npos, bool keep_empty = true);
std::string repeat(std::string_view text, std::size_t times);
std::string lower_ascii(std::string_view text);
std::string upper_ascii(std::string_view text);
bool valid_utf8(std::string_view text) noexcept;
Result<std::string> normalize_utf8(std::string_view text);
Result<std::size_t> count_code_points(std::string_view text);
Result<std::int64_t> parse_int(std::string_view text, int base = 10);
Result<double> parse_float(std::string_view text);
std::string join(const std::vector<std::string_view> &parts, std::string_view separator);
std::string join(std::initializer_list<std::string_view> parts, std::string_view separator);
std::string join(const std::vector<std::string> &parts, std::string_view separator);

class String;

/// @brief Non-owning UTF-8 view indexed by Unicode code point.
class StringView {
public:
    using size_type = std::size_t;
    using code_point_type = char32_t;
    static constexpr size_type npos = std::string_view::npos;

    StringView() noexcept = default;

    [[nodiscard]] bool is_empty() const noexcept;
    [[nodiscard]] size_type get_size() const noexcept;
    [[nodiscard]] size_type get_storage_size() const noexcept;
    Result<code_point_type> try_get_code_point(size_type index) const noexcept;
    Result<code_point_type> try_get_front() const noexcept;
    Result<code_point_type> try_get_back() const noexcept;

    size_type find(const String &needle, size_type start = 0) const noexcept;
    size_type find(const StringView &needle, size_type start = 0) const noexcept;
    size_type find(code_point_type code_point, size_type start = 0) const noexcept;
    size_type rfind(const String &needle, size_type start = npos) const noexcept;
    size_type rfind(const StringView &needle, size_type start = npos) const noexcept;
    size_type rfind(code_point_type code_point, size_type start = npos) const noexcept;
    size_type find_first_of(const StringView &, size_type start = 0) const noexcept;
    size_type find_last_of(const StringView &, size_type start = npos) const noexcept;
    size_type find_first_not_of(const StringView &, size_type start = 0) const noexcept;
    size_type find_last_not_of(const StringView &, size_type start = npos) const noexcept;
    size_type find_first_of(const String &, size_type start = 0) const noexcept;
    size_type find_last_of(const String &, size_type start = npos) const noexcept;
    size_type find_first_not_of(const String &, size_type start = 0) const noexcept;
    size_type find_last_not_of(const String &, size_type start = npos) const noexcept;
    bool starts_with(const StringView &) const noexcept;
    bool ends_with(const StringView &) const noexcept;
    bool contains(const StringView &) const noexcept;
    size_type count(const StringView &) const noexcept;
    bool starts_with(const String &) const noexcept;
    bool ends_with(const String &) const noexcept;
    bool contains(const String &) const noexcept;
    size_type count(const String &) const noexcept;
    int compare(const StringView &) const noexcept;
    int compare(const String &) const noexcept;

    Result<StringView> try_substr(size_type start = 0, size_type count = npos) const noexcept;
    StringView remove_prefix(size_type count) const noexcept;
    StringView remove_suffix(size_type count) const noexcept;
    Result<String> try_to_string() const noexcept;

    template <class Function>
    Result<void> try_for_each_code_point(Function &&function) const noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
        try {
#endif
            for (size_type index = 0; index < get_size(); ++index) {
                const auto value = try_get_code_point(index);
                if (!value) return Result<void>::failure(value.error());
                std::invoke(std::forward<Function>(function), value.value());
            }
            return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
        } catch (const std::exception &error) {
            return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                         error.what(), "string.view.for_each_code_point");
        } catch (...) {
            return Result<void>::failure(std::make_error_code(std::errc::io_error),
                                         "String callback failed", "string.view.for_each_code_point");
        }
#endif
    }

private:
    explicit StringView(std::string_view bytes) noexcept : bytes_(bytes) {}
    std::string_view bytes_{};
    friend class String;
};

/// @brief Owning UTF-8 string with Unicode-code-point indexing.
class String {
public:
    using size_type = std::size_t;
    using code_point_type = char32_t;
    static constexpr size_type npos = std::string_view::npos;

    String() = default;
    explicit String(bool value);
    explicit String(char value);
    explicit String(std::string_view value);
    explicit String(const std::string &value);
    explicit String(const char *value);
    explicit String(std::u16string_view value);
    explicit String(const std::u16string &value);
    explicit String(const char16_t *value);
    explicit String(std::u32string_view value);
    explicit String(const std::u32string &value);
    explicit String(const char32_t *value);
    explicit String(std::wstring_view value);
    explicit String(const std::wstring &value);
    explicit String(const wchar_t *value);
    explicit String(const std::filesystem::path &value);

    template <class Integer,
              std::enable_if_t<std::is_integral_v<Integer> &&
                               !std::is_same_v<std::decay_t<Integer>, bool> &&
                               !std::is_same_v<std::decay_t<Integer>, char>, int> = 0>
    explicit String(Integer value) : String(std::to_string(value)) {}

    template <class Floating,
              std::enable_if_t<std::is_floating_point_v<Floating>, int> = 0>
    explicit String(Floating value) : String(std::to_string(value)) {}

    static Result<String> from(std::string_view) noexcept;
    static Result<String> from(const char *) noexcept;
    static Result<String> from(std::u16string_view) noexcept;
    static Result<String> from(const char16_t *) noexcept;
    static Result<String> from(std::u32string_view) noexcept;
    static Result<String> from(const char32_t *) noexcept;
    static Result<String> from(std::wstring_view) noexcept;
    static Result<String> from(const wchar_t *) noexcept;
    static Result<String> from(const std::filesystem::path &) noexcept;
    static Result<String> from(const String &) noexcept;
    static Result<String> from(bool) noexcept;
    static Result<String> from(char) noexcept;

    template <class Integer,
              std::enable_if_t<std::is_integral_v<Integer> &&
                               !std::is_same_v<std::decay_t<Integer>, bool> &&
                               !std::is_same_v<std::decay_t<Integer>, char>, int> = 0>
    static Result<String> from(Integer value) noexcept { return from(String(value)); }

    template <class Floating,
              std::enable_if_t<std::is_floating_point_v<Floating>, int> = 0>
    static Result<String> from(Floating value) noexcept { return from(String(value)); }

    static Result<String> try_create_utf8(std::string_view) noexcept;
    static Result<String> try_create_utf16(std::u16string_view) noexcept;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] size_type size() const noexcept;
    [[nodiscard]] size_type size_storage() const noexcept;
    [[nodiscard]] bool is_empty() const noexcept { return empty(); }
    [[nodiscard]] size_type get_size() const noexcept { return size(); }
    [[nodiscard]] size_type get_storage_size() const noexcept { return size_storage(); }
    [[nodiscard]] StringView get_view() const noexcept;

    Result<code_point_type> try_get_code_point(size_type) const noexcept;
    Result<code_point_type> try_get_front() const noexcept;
    Result<code_point_type> try_get_back() const noexcept;
    size_type find(const String &, size_type start = 0) const noexcept;
    size_type find(const StringView &, size_type start = 0) const noexcept;
    size_type find(code_point_type, size_type start = 0) const noexcept;
    size_type rfind(const String &, size_type start = npos) const noexcept;
    size_type rfind(const StringView &, size_type start = npos) const noexcept;
    size_type rfind(code_point_type, size_type start = npos) const noexcept;
    size_type find_first_of(const String &, size_type start = 0) const noexcept;
    size_type find_last_of(const String &, size_type start = npos) const noexcept;
    size_type find_first_not_of(const String &, size_type start = 0) const noexcept;
    size_type find_last_not_of(const String &, size_type start = npos) const noexcept;
    bool starts_with(const String &) const noexcept;
    bool ends_with(const String &) const noexcept;
    bool contains(const String &) const noexcept;
    size_type count(const String &) const noexcept;
    int compare(const String &) const noexcept;
    Result<StringView> try_substr_view(size_type, size_type = npos) const noexcept;
    Result<String> try_substr(size_type, size_type = npos) const noexcept;

    template <class Function>
    Result<void> try_for_each_code_point(Function &&function) const noexcept {
        return get_view().try_for_each_code_point(std::forward<Function>(function));
    }

    Result<void> try_append(const String &) noexcept;
    Result<void> try_append(const StringView &) noexcept;
    Result<void> try_append(code_point_type, size_type count = 1) noexcept;
    Result<void> try_assign(const String &) noexcept;
    Result<void> try_assign(const StringView &) noexcept;
    Result<void> try_clear() noexcept;
    Result<void> try_pop_back() noexcept;
    Result<void> try_swap(String &) noexcept;
    Result<void> try_shrink_to_fit() noexcept;
    Result<void> try_insert(size_type, const String &) noexcept;
    Result<void> try_insert(size_type, const StringView &) noexcept;
    Result<void> try_insert(size_type, code_point_type, size_type count = 1) noexcept;
    Result<void> try_erase(size_type, size_type count = npos) noexcept;
    Result<void> try_replace(size_type, size_type, const String &) noexcept;
    Result<void> try_replace(size_type, size_type, const StringView &) noexcept;
    Result<void> try_replace(size_type, size_type, code_point_type, size_type) noexcept;
    Result<void> try_replace_all(const String &, const String &) noexcept;
    Result<void> try_replace_all(const StringView &, const StringView &) noexcept;
    Result<void> replace(size_type i, size_type n, const String &v) noexcept {
        return try_replace(i, n, v);
    }
    Result<void> replace(size_type i, size_type n, const StringView &v) noexcept {
        return try_replace(i, n, v);
    }
    Result<void> replace_all(const String &a, const String &b) noexcept {
        return try_replace_all(a, b);
    }
    Result<void> try_push_back(code_point_type) noexcept;
    Result<void> try_resize(size_type, code_point_type fill = {}) noexcept;
    Result<void> try_trim() noexcept;
    Result<void> try_lower_ascii() noexcept;
    Result<void> try_upper_ascii() noexcept;
    Result<std::vector<String>> try_split(const String &, bool keep_empty = true) const noexcept;
    Result<std::vector<String>> try_split(std::string_view, bool keep_empty = true) const noexcept;
    Result<String> try_repeat(size_type) const noexcept;

    Result<std::string> try_to_utf8() const noexcept;
    Result<std::u16string> try_to_utf16() const noexcept;
    std::string to_utf8() const;
    std::u16string to_utf16() const;
    Result<code_point_type> to_code_point() const noexcept;
    Result<char> to_char() const noexcept;
    Result<code_point_type> to_char32() const noexcept { return to_code_point(); }
    std::string to_stdstr() const { return to_utf8(); }
    std::string to_std_string() const { return to_utf8(); }
    Result<std::filesystem::path> to_path() const noexcept;
    Result<std::int64_t> to_int(int base = 10) const noexcept;
    Result<double> to_float() const noexcept;
    Result<bool> to_bool() const noexcept;

    bool operator==(const String &other) const noexcept { return value_ == other.value_; }
    bool operator!=(const String &other) const noexcept { return !(*this == other); }

private:
    explicit String(std::string utf8, int) noexcept : value_(std::move(utf8)) {}
    std::string value_;
    friend class StringView;
};

template <class... Parts>
inline std::string concat(Parts &&...parts) {
    std::string result;
    auto append = [&result](auto &&part) {
        using Value = std::decay_t<decltype(part)>;
        if constexpr (std::is_arithmetic_v<Value>) result += std::to_string(part);
        else if constexpr (std::is_same_v<Value, String>) result += part.to_utf8();
        else result += std::string_view(std::forward<decltype(part)>(part));
    };
    (append(std::forward<Parts>(parts)), ...);
    return result;
}

} // namespace sindre::general::string

namespace sindre::general {

/// @brief General 的 UTF-8 拥有字符串便捷别名。
using String = string::String;

/// @brief General 的非拥有 UTF-8 字符串视图便捷别名。
using StringView = string::StringView;

} // namespace sindre::general

namespace sindre::general::regex {
class Regex {
public:
    static Result<Regex> compile(std::string pattern) noexcept;
    Regex(Regex &&) noexcept;
    Regex &operator=(Regex &&) noexcept;
    Regex(const Regex &) = delete;
    Regex &operator=(const Regex &) = delete;
    ~Regex();
    bool match(std::string_view text) const noexcept;
    Result<bool> try_match(std::string_view text) const noexcept;
    Result<std::string> replace(std::string_view text,
                                std::string_view replacement) const noexcept;
private:
    struct Impl;
    explicit Regex(std::shared_ptr<Impl> impl) noexcept;
    std::shared_ptr<Impl> impl_;
};
} // namespace sindre::general::regex
