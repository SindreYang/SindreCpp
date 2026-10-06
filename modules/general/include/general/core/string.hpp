#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <initializer_list>
#include <initializer_list>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>
#include <general/core/async.hpp>

#if defined(SINDRECPP_WITH_STRING)
#include <cs_string.h>
#endif

namespace sindrecpp::general::string {

inline std::string_view trim(std::string_view text) noexcept {
    constexpr std::string_view whitespace = " \t\n\r\f\v";
    const auto first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) return {};
    const auto last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

// Use this overload when the source may be a temporary or when an owning
// result is more convenient than a view into the source text.
inline std::string trim_copy(std::string_view text) {
    return std::string(trim(text));
}

inline bool starts_with(std::string_view text, std::string_view prefix) noexcept {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

inline bool ends_with(std::string_view text, std::string_view suffix) noexcept {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

inline std::string replace_all(std::string_view text, std::string_view from, std::string_view to) {
    if (from.empty()) return std::string(text);
    std::string result;
    result.reserve(text.size());
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        const auto match = text.find(from, cursor);
        if (match == std::string_view::npos) {
            result.append(text.substr(cursor));
            break;
        }
        result.append(text.substr(cursor, match - cursor));
        result.append(to);
        cursor = match + from.size();
    }
    return result;
}

// The owning overload is the safe default: its result remains valid when text
// was a temporary string. Use split_view when the caller controls the source
// lifetime and wants zero-copy tokens.
inline std::vector<std::string_view> split_view(std::string_view text, char delimiter) {
    std::vector<std::string_view> result;
    std::size_t begin = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == delimiter) {
            result.emplace_back(text.substr(begin, i - begin));
            begin = i + 1;
        }
    }
    return result;
}

inline std::vector<std::string> split(std::string_view text, char delimiter) {
    const auto views = split_view(text, delimiter);
    std::vector<std::string> result;
    result.reserve(views.size());
    for (const auto part : views)
        result.emplace_back(part);
    return result;
}

inline std::vector<std::string> split(std::string_view text, std::string_view delimiter,
                                      bool keep_empty = true) {
    std::vector<std::string> result;
    if (delimiter.empty()) {
        for (const auto c : text) result.emplace_back(1, c);
        return result;
    }
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const auto end = text.find(delimiter, begin);
        const auto part = text.substr(begin, end == std::string_view::npos ? end : end - begin);
        if (keep_empty || !part.empty()) result.emplace_back(part);
        if (end == std::string_view::npos) break;
        begin = end + delimiter.size();
    }
    return result;
}

inline std::string lower_ascii(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    });
    return result;
}

inline std::string upper_ascii(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c);
    });
    return result;
}

inline bool valid_utf8(std::string_view text) noexcept;

inline ::sindrecpp::general::Result<std::string> normalize_utf8(std::string_view text) {
    if (!valid_utf8(text))
        return ::sindrecpp::general::Result<std::string>::failure(
            std::make_error_code(std::errc::illegal_byte_sequence), "Invalid UTF-8 text", "string.utf8");
    return ::sindrecpp::general::Result<std::string>::success(std::string(text));
}

inline bool valid_utf8(std::string_view text) noexcept {
    for (std::size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i]);
        std::size_t width = 0;
        std::uint32_t codepoint = 0;
        if (c <= 0x7f) { width = 1; codepoint = c; }
        else if (c >= 0xc2 && c <= 0xdf) { width = 2; codepoint = c & 0x1f; }
        else if (c >= 0xe0 && c <= 0xef) { width = 3; codepoint = c & 0x0f; }
        else if (c >= 0xf0 && c <= 0xf4) { width = 4; codepoint = c & 0x07; }
        else return false;
        if (i + width > text.size()) return false;
        for (std::size_t j = 1; j < width; ++j) {
            const auto continuation = static_cast<unsigned char>(text[i + j]);
            if ((continuation & 0xc0) != 0x80) return false;
            codepoint = (codepoint << 6) | (continuation & 0x3f);
        }
        if ((width == 2 && codepoint < 0x80) ||
            (width == 3 && codepoint < 0x800) ||
            (width == 4 && codepoint < 0x10000) ||
            codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff))
            return false;
        i += width;
    }
    return true;
}

inline ::sindrecpp::general::Result<std::int64_t> parse_int(std::string_view text, int base = 10) {
    if (base < 2 || base > 36)
        return ::sindrecpp::general::Result<std::int64_t>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid integer base", "string.parse_int");
    const auto trimmed = trim(text);
    if (trimmed.empty())
        return ::sindrecpp::general::Result<std::int64_t>::failure(
            std::make_error_code(std::errc::invalid_argument), "Integer is empty", "string.parse_int");
    std::int64_t value{};
    const auto parsed = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value, base);
    if (parsed.ec == std::errc::result_out_of_range)
        return ::sindrecpp::general::Result<std::int64_t>::failure(
            std::make_error_code(std::errc::result_out_of_range), "Integer is out of range", "string.parse_int");
    if (parsed.ec != std::errc{} || parsed.ptr != trimmed.data() + trimmed.size())
        return ::sindrecpp::general::Result<std::int64_t>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid integer", "string.parse_int");
    return ::sindrecpp::general::Result<std::int64_t>::success(value);
}

inline ::sindrecpp::general::Result<double> parse_float(std::string_view text) {
    const auto trimmed = trim(text);
    if (trimmed.empty())
        return ::sindrecpp::general::Result<double>::failure(
            std::make_error_code(std::errc::invalid_argument), "Float is empty", "string.parse_float");
    double value{};
    const auto parsed = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value);
    if (parsed.ec == std::errc::result_out_of_range)
        return ::sindrecpp::general::Result<double>::failure(
            std::make_error_code(std::errc::result_out_of_range), "Float is out of range", "string.parse_float");
    if (parsed.ec != std::errc{} || parsed.ptr != trimmed.data() + trimmed.size() || !std::isfinite(value))
        return ::sindrecpp::general::Result<double>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid float", "string.parse_float");
    return ::sindrecpp::general::Result<double>::success(value);
}

inline std::string join(const std::vector<std::string_view> &parts, std::string_view separator) {
    std::string result;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) result.append(separator);
        result.append(parts[i]);
    }
    return result;
}

inline std::string join(std::initializer_list<std::string_view> parts, std::string_view separator) {
    return join(std::vector<std::string_view>(parts), separator);
}

inline std::string join(const std::vector<std::string> &parts, std::string_view separator) {
    std::string result;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) result.append(separator);
        result.append(parts[i]);
    }
    return result;
}

class String {
public:
    String() = default;
    String(const char *text) : value_(text ? text : "") {}
    String(std::string text) : value_(std::move(text)) {}
    String(std::string_view text) : value_(text) {}

    const std::string &str() const noexcept { return value_; }
    std::string_view view() const noexcept { return value_; }
    bool empty() const noexcept { return value_.empty(); }
    std::size_t size() const noexcept { return value_.size(); }
    String trim() const { return String(trim_copy(value_)); }
    String lower() const { return String(lower_ascii(value_)); }
    String upper() const { return String(upper_ascii(value_)); }
    bool starts_with(std::string_view prefix) const noexcept { return string::starts_with(value_, prefix); }
    bool ends_with(std::string_view suffix) const noexcept { return string::ends_with(value_, suffix); }
    String replace(std::string_view from, std::string_view to) const {
        return String(replace_all(value_, from, to));
    }
    std::vector<std::string> split(std::string_view delimiter, bool keep_empty = true) const {
        return string::split(value_, delimiter, keep_empty);
    }
    ::sindrecpp::general::Result<std::int64_t> to_int(int base = 10) const {
        return parse_int(value_, base);
    }
    ::sindrecpp::general::Result<double> to_float() const { return parse_float(value_); }
    operator std::string_view() const noexcept { return value_; }

private:
    std::string value_;
};

template <class Part> inline void concat_append(std::string &result, Part &&part) {
    using Value = std::decay_t<Part>;
    if constexpr (std::is_arithmetic_v<Value>)
        result += std::to_string(part);
    else if constexpr (std::is_same_v<Value, String>)
        result.append(part.view());
    else if constexpr (std::is_convertible_v<Part, std::string_view>)
        result.append(std::string_view(std::forward<Part>(part)));
    else
        result.append(std::forward<Part>(part));
}

template <class... Parts> inline std::string concat(Parts &&...parts) {
    std::string result;
    (concat_append(result, std::forward<Parts>(parts)), ...);
    return result;
}

#if defined(SINDRECPP_WITH_STRING)
namespace native = CsString;
using Utf8String = CsString::CsString;
using Utf16String = CsString::CsString_utf16;
#endif

} // namespace sindrecpp::general::string
