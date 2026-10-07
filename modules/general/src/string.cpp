#include <sindre/general/string.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#if defined(SINDRE_WITH_RE2)
#include <re2/re2.h>
#endif

namespace sindre::general::string {

std::string_view trim(std::string_view text) noexcept {
    constexpr std::string_view whitespace = " \t\n\r\f\v";
    const auto first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) return {};
    return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
}

std::string trim_copy(std::string_view text) { return std::string(trim(text)); }
bool starts_with(std::string_view text, std::string_view prefix) noexcept {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}
bool ends_with(std::string_view text, std::string_view suffix) noexcept {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}
bool contains(std::string_view text, std::string_view needle) noexcept { return text.find(needle) != npos; }

std::size_t count(std::string_view text, std::string_view needle) noexcept {
    if (needle.empty()) return text.size() + 1;
    std::size_t result = 0;
    for (std::size_t cursor = 0; cursor <= text.size();) {
        const auto match = text.find(needle, cursor);
        if (match == npos) break;
        ++result;
        cursor = match + needle.size();
    }
    return result;
}

std::string_view lstrip(std::string_view text, std::string_view chars) noexcept {
    const auto first = text.find_first_not_of(chars);
    return first == npos ? std::string_view{} : text.substr(first);
}
std::string_view rstrip(std::string_view text, std::string_view chars) noexcept {
    const auto last = text.find_last_not_of(chars);
    return last == npos ? std::string_view{} : text.substr(0, last + 1);
}
std::string_view strip(std::string_view text, std::string_view chars) noexcept {
    return rstrip(lstrip(text, chars), chars);
}

std::string replace_all(std::string_view text, std::string_view from, std::string_view to) {
    if (from.empty()) return std::string(text);
    std::string result;
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        const auto match = text.find(from, cursor);
        if (match == npos) {
            result.append(text.substr(cursor));
            break;
        }
        result.append(text.substr(cursor, match - cursor));
        result.append(to);
        cursor = match + from.size();
    }
    return result;
}

std::vector<std::string_view> split_view(std::string_view text, char delimiter) {
    std::vector<std::string_view> result;
    std::size_t begin = 0;
    for (std::size_t index = 0; index <= text.size(); ++index) {
        if (index == text.size() || text[index] == delimiter) {
            result.emplace_back(text.substr(begin, index - begin));
            begin = index + 1;
        }
    }
    return result;
}

std::vector<std::string> split(std::string_view text, char delimiter) {
    const auto views = split_view(text, delimiter);
    std::vector<std::string> result;
    result.reserve(views.size());
    for (const auto part : views) result.emplace_back(part);
    return result;
}

std::vector<std::string> split(std::string_view text, std::string_view delimiter, bool keep_empty) {
    std::vector<std::string> result;
    if (delimiter.empty()) {
        for (const auto byte : text) result.emplace_back(1, byte);
        return result;
    }
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const auto end = text.find(delimiter, begin);
        const auto part = text.substr(begin, end == npos ? end : end - begin);
        if (keep_empty || !part.empty()) result.emplace_back(part);
        if (end == npos) break;
        begin = end + delimiter.size();
    }
    return result;
}

std::vector<std::string> rsplit(std::string_view text, std::string_view delimiter,
                                std::size_t max_splits, bool keep_empty) {
    if (delimiter.empty()) return {std::string(text)};
    std::vector<std::string> result;
    std::size_t end = text.size();
    std::size_t splits = 0;
    while (end > 0 && (max_splits == npos || splits < max_splits)) {
        const auto match = text.rfind(delimiter, end - 1);
        if (match == npos) break;
        const auto part = text.substr(match + delimiter.size(), end - match - delimiter.size());
        if (keep_empty || !part.empty()) result.emplace_back(part);
        end = match;
        ++splits;
    }
    const auto head = text.substr(0, end);
    if (keep_empty || !head.empty() || result.empty()) result.emplace_back(head);
    std::reverse(result.begin(), result.end());
    return result;
}

std::string repeat(std::string_view text, std::size_t times) {
    if (times == 0 || text.empty() || text.size() > std::string{}.max_size() / times) return {};
    std::string result;
    result.reserve(text.size() * times);
    for (std::size_t i = 0; i < times; ++i) result.append(text);
    return result;
}

std::string lower_ascii(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char byte) {
        return static_cast<char>(byte >= 'A' && byte <= 'Z' ? byte + ('a' - 'A') : byte);
    });
    return result;
}
std::string upper_ascii(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char byte) {
        return static_cast<char>(byte >= 'a' && byte <= 'z' ? byte - ('a' - 'A') : byte);
    });
    return result;
}

bool valid_utf8(std::string_view text) noexcept {
    for (std::size_t index = 0; index < text.size();) {
        const auto byte = static_cast<unsigned char>(text[index]);
        std::size_t width = 0;
        std::uint32_t codepoint = 0;
        if (byte <= 0x7f) { width = 1; codepoint = byte; }
        else if (byte >= 0xc2 && byte <= 0xdf) { width = 2; codepoint = byte & 0x1f; }
        else if (byte >= 0xe0 && byte <= 0xef) { width = 3; codepoint = byte & 0x0f; }
        else if (byte >= 0xf0 && byte <= 0xf4) { width = 4; codepoint = byte & 0x07; }
        else return false;
        if (index + width > text.size()) return false;
        for (std::size_t offset = 1; offset < width; ++offset) {
            const auto continuation = static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xc0) != 0x80) return false;
            codepoint = (codepoint << 6) | (continuation & 0x3f);
        }
        if ((width == 2 && codepoint < 0x80) || (width == 3 && codepoint < 0x800) ||
            (width == 4 && codepoint < 0x10000) || codepoint > 0x10ffff ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) return false;
        index += width;
    }
    return true;
}

Result<std::string> normalize_utf8(std::string_view text) {
    if (!valid_utf8(text)) return Result<std::string>::failure(
        std::make_error_code(std::errc::illegal_byte_sequence), "Invalid UTF-8 text", "string.utf8");
    return Result<std::string>::success(std::string(text));
}

Result<std::size_t> count_code_points(std::string_view text) {
    if (!valid_utf8(text)) return Result<std::size_t>::failure(
        std::make_error_code(std::errc::illegal_byte_sequence), "Invalid UTF-8 text", "string.count_code_points");
    std::size_t result = 0;
    for (const auto byte : text) if ((static_cast<unsigned char>(byte) & 0xc0) != 0x80) ++result;
    return Result<std::size_t>::success(result);
}

Result<std::int64_t> parse_int(std::string_view text, int base) {
    if (base < 2 || base > 36) return Result<std::int64_t>::failure(
        std::make_error_code(std::errc::invalid_argument), "Invalid integer base", "string.parse_int");
    const auto value = trim(text);
    if (value.empty()) return Result<std::int64_t>::failure(
        std::make_error_code(std::errc::invalid_argument), "Integer is empty", "string.parse_int");
    std::int64_t result{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result, base);
    if (parsed.ec == std::errc::result_out_of_range) return Result<std::int64_t>::failure(
        std::make_error_code(std::errc::result_out_of_range), "Integer is out of range", "string.parse_int");
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) return Result<std::int64_t>::failure(
        std::make_error_code(std::errc::invalid_argument), "Invalid integer", "string.parse_int");
    return Result<std::int64_t>::success(result);
}

Result<double> parse_float(std::string_view text) {
    const auto value = trim(text);
    if (value.empty()) return Result<double>::failure(
        std::make_error_code(std::errc::invalid_argument), "Float is empty", "string.parse_float");
    double result{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec == std::errc::result_out_of_range) return Result<double>::failure(
        std::make_error_code(std::errc::result_out_of_range), "Float is out of range", "string.parse_float");
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !std::isfinite(result))
        return Result<double>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid float", "string.parse_float");
    return Result<double>::success(result);
}

std::string join(const std::vector<std::string_view> &parts, std::string_view separator) {
    std::string result;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        if (index) result.append(separator);
        result.append(parts[index]);
    }
    return result;
}
std::string join(std::initializer_list<std::string_view> parts, std::string_view separator) {
    return join(std::vector<std::string_view>(parts), separator);
}
std::string join(const std::vector<std::string> &parts, std::string_view separator) {
    std::string result;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        if (index) result.append(separator);
        result.append(parts[index]);
    }
    return result;
}

} // namespace sindre::general::string

#if defined(SINDRE_WITH_RE2)
namespace sindre::general::regex {

Result<Regex> Regex::compile(std::string pattern) noexcept {
#if defined(SINDRE_WITH_RE2)
    try {
        Regex result;
        result.pattern_ = std::make_shared<::re2::RE2>(std::move(pattern));
        if (!result.pattern_->ok()) return Result<Regex>::failure(
            std::make_error_code(std::errc::invalid_argument), result.pattern_->error(), "regex.compile");
        return Result<Regex>::success(std::move(result));
    } catch (const std::exception &error) {
        return Result<Regex>::failure(std::make_error_code(std::errc::invalid_argument), error.what(), "regex.compile");
    } catch (...) {
        return Result<Regex>::failure(std::make_error_code(std::errc::invalid_argument), "Regex compilation failed", "regex.compile");
    }
#else
    (void)pattern;
    return Result<Regex>::failure(std::make_error_code(std::errc::function_not_supported), "RE2 support is not enabled", "regex.compile");
#endif
}

bool Regex::match(std::string_view text) const noexcept {
#if defined(SINDRE_WITH_RE2)
    return pattern_ && ::re2::RE2::FullMatch(::re2::StringPiece(text.data(), text.size()), *pattern_);
#else
    (void)text; return false;
#endif
}

Result<bool> Regex::try_match(std::string_view text) const noexcept {
#if defined(SINDRE_WITH_RE2)
    if (!pattern_) return Result<bool>::failure(std::make_error_code(std::errc::invalid_argument), "Regex is not initialized", "regex.match");
    return Result<bool>::success(::re2::RE2::FullMatch(::re2::StringPiece(text.data(), text.size()), *pattern_));
#else
    (void)text;
    return Result<bool>::failure(std::make_error_code(std::errc::function_not_supported), "RE2 support is not enabled", "regex.match");
#endif
}

Result<std::string> Regex::replace(std::string_view text, std::string_view replacement) const noexcept {
#if defined(SINDRE_WITH_RE2)
    if (!pattern_) return Result<std::string>::failure(std::make_error_code(std::errc::invalid_argument), "Regex is not initialized", "regex.replace");
    std::string result(text);
    ::re2::RE2::GlobalReplace(&result, *pattern_, std::string(replacement));
    return Result<std::string>::success(std::move(result));
#else
    (void)text; (void)replacement;
    return Result<std::string>::failure(std::make_error_code(std::errc::function_not_supported), "RE2 support is not enabled", "regex.replace");
#endif
}

} // namespace sindre::general::regex
#endif

// String implementation boundary.  concat and other constrained templates
// stay header-visible by design.
