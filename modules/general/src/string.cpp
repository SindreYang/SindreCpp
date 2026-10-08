#include <sindre/general/string.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <sstream>

#if defined(SINDRE_WITH_RE2)
#include <re2/re2.h>
#endif

namespace sindre::general::string {

namespace {

struct CodePoint {
    char32_t value = 0xfffd;
    std::size_t bytes = 1;
};

CodePoint decode(std::string_view text, std::size_t offset) noexcept {
    if (offset >= text.size()) return {};
    const auto first = static_cast<unsigned char>(text[offset]);
    if (first <= 0x7f) return {static_cast<char32_t>(first), 1};
    std::size_t width = 0;
    char32_t value = 0;
    if (first >= 0xc2 && first <= 0xdf) { width = 2; value = first & 0x1f; }
    else if (first >= 0xe0 && first <= 0xef) { width = 3; value = first & 0x0f; }
    else if (first >= 0xf0 && first <= 0xf4) { width = 4; value = first & 0x07; }
    else return {};
    if (offset + width > text.size()) return {};
    for (std::size_t index = 1; index < width; ++index) {
        const auto byte = static_cast<unsigned char>(text[offset + index]);
        if ((byte & 0xc0) != 0x80) return {};
        value = (value << 6) | (byte & 0x3f);
    }
    if ((width == 2 && value < 0x80) || (width == 3 && value < 0x800) ||
        (width == 4 && value < 0x10000) || value > 0x10ffff ||
        (value >= 0xd800 && value <= 0xdfff)) return {};
    return {value, width};
}

std::vector<std::size_t> offsets(std::string_view text) {
    std::vector<std::size_t> result;
    result.reserve(text.size() + 1);
    std::size_t offset = 0;
    result.push_back(0);
    while (offset < text.size()) {
        const auto point = decode(text, offset);
        offset += point.bytes;
        result.push_back(offset);
    }
    return result;
}

void append_utf8(std::string &output, char32_t value) {
    if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) value = 0xfffd;
    if (value <= 0x7f) output.push_back(static_cast<char>(value));
    else if (value <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (value >> 6)));
        output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    } else if (value <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (value >> 12)));
        output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (value >> 18)));
        output.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    }
}

std::string sanitize(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (std::size_t offset = 0; offset < text.size();) {
        const auto point = decode(text, offset);
        append_utf8(result, point.value);
        offset += point.bytes;
    }
    return result;
}

std::string from_utf16(std::u16string_view text) {
    std::string result;
    for (std::size_t index = 0; index < text.size(); ++index) {
        char32_t value = text[index];
        if (value >= 0xd800 && value <= 0xdbff) {
            if (index + 1 < text.size() && text[index + 1] >= 0xdc00 &&
                text[index + 1] <= 0xdfff) {
                value = 0x10000 + ((value - 0xd800) << 10) +
                        (text[++index] - 0xdc00);
            } else value = 0xfffd;
        } else if (value >= 0xdc00 && value <= 0xdfff) value = 0xfffd;
        append_utf8(result, value);
    }
    return result;
}

std::string from_utf32(std::u32string_view text) {
    std::string result;
    for (const auto value : text) append_utf8(result, value);
    return result;
}

std::string code_points_to_utf8(const std::vector<char32_t> &points) {
    std::string result;
    for (const auto point : points) append_utf8(result, point);
    return result;
}

Result<void> invalid(std::string message, std::string context) {
    return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                 std::move(message), std::move(context));
}

template <class T, class Function>
Result<T> guarded(Function &&function, std::string context) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        return Result<T>::success(std::forward<Function>(function)());
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::bad_alloc &error) {
        return Result<T>::failure(std::make_error_code(std::errc::not_enough_memory),
                                  error.what(), std::move(context));
    } catch (const std::exception &error) {
        return Result<T>::failure(std::make_error_code(std::errc::invalid_argument),
                                  error.what(), std::move(context));
    } catch (...) {
        return Result<T>::failure(std::make_error_code(std::errc::io_error),
                                  "String operation failed", std::move(context));
    }
#endif
}

template <class Function>
Result<void> guarded_void(Function &&function, std::string context) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::forward<Function>(function)();
        return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::bad_alloc &error) {
        return Result<void>::failure(std::make_error_code(std::errc::not_enough_memory),
                                     error.what(), std::move(context));
    } catch (const std::exception &error) {
        return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                     error.what(), std::move(context));
    } catch (...) {
        return Result<void>::failure(std::make_error_code(std::errc::io_error),
                                     "String operation failed", std::move(context));
    }
#endif
}

std::size_t find_view(std::string_view text, std::string_view needle,
                      std::size_t start, bool reverse) noexcept {
    const auto text_offsets = offsets(text);
    const auto needle_offsets = offsets(needle);
    const auto text_size = text_offsets.size() - 1;
    const auto needle_size = needle_offsets.size() - 1;
    if (needle_size == 0) return reverse ? std::min(start, text_size) : start;
    if (start > text_size) return npos;
    if (needle_size > text_size) return npos;
    if (reverse) {
        std::size_t last = start == npos ? text_size - needle_size
                                         : std::min(start, text_size - needle_size);
        for (std::size_t index = last + 1; index > 0; --index) {
            const auto candidate = index - 1;
            if (text.substr(text_offsets[candidate],
                            text_offsets[candidate + needle_size] - text_offsets[candidate]) == needle)
                return candidate;
        }
        return npos;
    }
    for (std::size_t index = start; index + needle_size <= text_size; ++index) {
        if (text.substr(text_offsets[index],
                        text_offsets[index + needle_size] - text_offsets[index]) == needle)
            return index;
    }
    return npos;
}

std::size_t find_set(std::string_view text, std::string_view set,
                     std::size_t start, bool reverse, bool negated) noexcept {
    const auto text_offsets = offsets(text);
    const auto set_offsets = offsets(set);
    const auto size = text_offsets.size() - 1;
    if (size == 0) return npos;
    auto matches = [&](char32_t value) {
        for (std::size_t index = 0; index + 1 < set_offsets.size(); ++index)
            if (decode(set, set_offsets[index]).value == value) return true;
        return false;
    };
    if (!reverse) {
        for (std::size_t index = std::min(start, size); index < size; ++index)
            if (matches(decode(text, text_offsets[index]).value) != negated) return index;
    } else {
        std::size_t index = start == npos ? size : std::min(start + 1, size);
        while (index > 0) {
            --index;
            if (matches(decode(text, text_offsets[index]).value) != negated) return index;
        }
    }
    return npos;
}

} // namespace

std::string_view trim(std::string_view text) noexcept {
    constexpr std::string_view whitespace = " \t\n\r\f\v";
    const auto first = text.find_first_not_of(whitespace);
    if (first == npos) return {};
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
        ++result; cursor = match + needle.size();
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
        if (match == npos) { result.append(text.substr(cursor)); break; }
        result.append(text.substr(cursor, match - cursor));
        result.append(to); cursor = match + from.size();
    }
    return result;
}
std::vector<std::string_view> split_view(std::string_view text, char delimiter) {
    std::vector<std::string_view> result;
    std::size_t begin = 0;
    for (std::size_t index = 0; index <= text.size(); ++index)
        if (index == text.size() || text[index] == delimiter) {
            result.emplace_back(text.substr(begin, index - begin)); begin = index + 1;
        }
    return result;
}
std::vector<std::string> split(std::string_view text, char delimiter) {
    const auto views = split_view(text, delimiter);
    std::vector<std::string> result; result.reserve(views.size());
    for (const auto part : views) result.emplace_back(part);
    return result;
}
std::vector<std::string> split(std::string_view text, std::string_view delimiter, bool keep_empty) {
    std::vector<std::string> result;
    if (delimiter.empty()) { for (const auto byte : text) result.emplace_back(1, byte); return result; }
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const auto end = text.find(delimiter, begin);
        const auto part = text.substr(begin, end == npos ? end : end - begin);
        if (keep_empty || !part.empty()) result.emplace_back(part);
        if (end == npos) break; begin = end + delimiter.size();
    }
    return result;
}
std::vector<std::string> rsplit(std::string_view text, std::string_view delimiter,
                                std::size_t max_splits, bool keep_empty) {
    if (delimiter.empty()) return {std::string(text)};
    std::vector<std::string> result; std::size_t end = text.size(); std::size_t splits = 0;
    while (end > 0 && (max_splits == npos || splits < max_splits)) {
        const auto match = text.rfind(delimiter, end - 1);
        if (match == npos) break;
        const auto part = text.substr(match + delimiter.size(), end - match - delimiter.size());
        if (keep_empty || !part.empty()) result.emplace_back(part);
        end = match; ++splits;
    }
    const auto head = text.substr(0, end);
    if (keep_empty || !head.empty() || result.empty()) result.emplace_back(head);
    std::reverse(result.begin(), result.end()); return result;
}
std::string repeat(std::string_view text, std::size_t times) {
    if (times == 0 || text.empty() || text.size() > std::string{}.max_size() / times) return {};
    std::string result; result.reserve(text.size() * times);
    for (std::size_t i = 0; i < times; ++i) result.append(text);
    return result;
}
std::string lower_ascii(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char byte) {
        return static_cast<char>(byte >= 'A' && byte <= 'Z' ? byte + ('a' - 'A') : byte);
    }); return result;
}
std::string upper_ascii(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char byte) {
        return static_cast<char>(byte >= 'a' && byte <= 'z' ? byte - ('a' - 'A') : byte);
    }); return result;
}
bool valid_utf8(std::string_view text) noexcept {
    for (std::size_t offset = 0; offset < text.size();) {
        const auto point = decode(text, offset);
        if (point.value == 0xfffd && !(static_cast<unsigned char>(text[offset]) == 0xef &&
                                        offset + 2 < text.size() &&
                                        static_cast<unsigned char>(text[offset + 1]) == 0xbf &&
                                        static_cast<unsigned char>(text[offset + 2]) == 0xbd)) return false;
        offset += point.bytes;
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
    return Result<std::size_t>::success(offsets(text).size() - 1);
}
Result<std::int64_t> parse_int(std::string_view text, int base) {
    if (base < 2 || base > 36) return Result<std::int64_t>::failure(
        std::make_error_code(std::errc::invalid_argument), "Invalid integer base", "string.parse_int");
    const auto value = trim(text); if (value.empty()) return Result<std::int64_t>::failure(
        std::make_error_code(std::errc::invalid_argument), "Integer is empty", "string.parse_int");
    std::int64_t result{}; const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result, base);
    if (parsed.ec == std::errc::result_out_of_range) return Result<std::int64_t>::failure(
        std::make_error_code(std::errc::result_out_of_range), "Integer is out of range", "string.parse_int");
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) return Result<std::int64_t>::failure(
        std::make_error_code(std::errc::invalid_argument), "Invalid integer", "string.parse_int");
    return Result<std::int64_t>::success(result);
}
Result<double> parse_float(std::string_view text) {
    const auto value = trim(text); if (value.empty()) return Result<double>::failure(
        std::make_error_code(std::errc::invalid_argument), "Float is empty", "string.parse_float");
    double result{}; const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec == std::errc::result_out_of_range) return Result<double>::failure(
        std::make_error_code(std::errc::result_out_of_range), "Float is out of range", "string.parse_float");
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !std::isfinite(result))
        return Result<double>::failure(std::make_error_code(std::errc::invalid_argument), "Invalid float", "string.parse_float");
    return Result<double>::success(result);
}
std::string join(const std::vector<std::string_view> &parts, std::string_view separator) {
    std::string result; for (std::size_t i = 0; i < parts.size(); ++i) { if (i) result += separator; result += parts[i]; } return result;
}
std::string join(std::initializer_list<std::string_view> parts, std::string_view separator) {
    return join(std::vector<std::string_view>(parts), separator);
}
std::string join(const std::vector<std::string> &parts, std::string_view separator) {
    std::string result; for (std::size_t i = 0; i < parts.size(); ++i) { if (i) result += separator; result += parts[i]; } return result;
}

String::String(bool value) : value_(value ? "true" : "false") {}
String::String(char value) : value_(1, value) {}
String::String(std::string_view value) : value_(sanitize(value)) {}
String::String(const std::string &value) : String(std::string_view(value)) {}
String::String(const char *value) : String(std::string_view(value ? value : "")) {}
String::String(std::u16string_view value) : value_(from_utf16(value)) {}
String::String(const std::u16string &value) : String(std::u16string_view(value)) {}
String::String(const char16_t *value) : String(std::u16string_view(value ? value : u"")) {}
String::String(std::u32string_view value) : value_(from_utf32(value)) {}
String::String(const std::u32string &value) : String(std::u32string_view(value)) {}
String::String(const char32_t *value) : String(std::u32string_view(value ? value : U"")) {}
String::String(std::wstring_view value) : value_(sizeof(wchar_t) == sizeof(char16_t)
    ? from_utf16(std::u16string_view(reinterpret_cast<const char16_t *>(value.data()), value.size()))
    : from_utf32(std::u32string_view(reinterpret_cast<const char32_t *>(value.data()), value.size()))) {}
String::String(const std::wstring &value) : String(std::wstring_view(value)) {}
String::String(const wchar_t *value) : String(std::wstring_view(value ? value : L"")) {}
String::String(const std::filesystem::path &value)
    : String(std::string_view(value.u8string())) {}

Result<String> String::from(std::string_view value) noexcept { return try_create_utf8(value); }
Result<String> String::from(const char *value) noexcept { return from(std::string_view(value ? value : "")); }
Result<String> String::from(std::u16string_view value) noexcept { return try_create_utf16(value); }
Result<String> String::from(const char16_t *value) noexcept { return from(std::u16string_view(value ? value : u"")); }
Result<String> String::from(std::u32string_view value) noexcept { return guarded<String>([&] { return String(value); }, "string.from_utf32"); }
Result<String> String::from(const char32_t *value) noexcept { return from(std::u32string_view(value ? value : U"")); }
Result<String> String::from(std::wstring_view value) noexcept { return guarded<String>([&] { return String(value); }, "string.from_wide"); }
Result<String> String::from(const wchar_t *value) noexcept { return from(std::wstring_view(value ? value : L"")); }
Result<String> String::from(const std::filesystem::path &value) noexcept { return guarded<String>([&] { return String(value); }, "string.from_path"); }
Result<String> String::from(const String &value) noexcept { return guarded<String>([&] { return value; }, "string.from_string"); }
Result<String> String::from(bool value) noexcept { return guarded<String>([&] { return String(value); }, "string.from_bool"); }
Result<String> String::from(char value) noexcept { return guarded<String>([&] { return String(value); }, "string.from_char"); }
Result<String> String::try_create_utf8(std::string_view value) noexcept { return guarded<String>([&] { return String(value); }, "string.create_utf8"); }
Result<String> String::try_create_utf16(std::u16string_view value) noexcept { return guarded<String>([&] { return String(value); }, "string.create_utf16"); }

bool String::empty() const noexcept { return value_.empty(); }
String::size_type String::size() const noexcept { return offsets(value_).size() - 1; }
String::size_type String::size_storage() const noexcept { return value_.size(); }
StringView String::get_view() const noexcept { return StringView(value_); }
Result<String::code_point_type> String::try_get_code_point(size_type index) const noexcept { return get_view().try_get_code_point(index); }
Result<String::code_point_type> String::try_get_front() const noexcept { return get_view().try_get_front(); }
Result<String::code_point_type> String::try_get_back() const noexcept { return get_view().try_get_back(); }
String::size_type String::find(const String &value, size_type start) const noexcept { return get_view().find(value, start); }
String::size_type String::find(const StringView &value, size_type start) const noexcept { return get_view().find(value, start); }
String::size_type String::find(code_point_type value, size_type start) const noexcept { return get_view().find(value, start); }
String::size_type String::rfind(const String &value, size_type start) const noexcept { return get_view().rfind(value, start); }
String::size_type String::rfind(const StringView &value, size_type start) const noexcept { return get_view().rfind(value, start); }
String::size_type String::rfind(code_point_type value, size_type start) const noexcept { return get_view().rfind(value, start); }
String::size_type String::find_first_of(const String &v, size_type s) const noexcept { return get_view().find_first_of(v, s); }
String::size_type String::find_last_of(const String &v, size_type s) const noexcept { return get_view().find_last_of(v, s); }
String::size_type String::find_first_not_of(const String &v, size_type s) const noexcept { return get_view().find_first_not_of(v, s); }
String::size_type String::find_last_not_of(const String &v, size_type s) const noexcept { return get_view().find_last_not_of(v, s); }
bool String::starts_with(const String &v) const noexcept { return get_view().starts_with(v); }
bool String::ends_with(const String &v) const noexcept { return get_view().ends_with(v); }
bool String::contains(const String &v) const noexcept { return get_view().contains(v); }
String::size_type String::count(const String &v) const noexcept { return get_view().count(v); }
int String::compare(const String &v) const noexcept { return get_view().compare(v); }
Result<StringView> String::try_substr_view(size_type start, size_type count) const noexcept { return get_view().try_substr(start, count); }
Result<String> String::try_substr(size_type start, size_type count) const noexcept { auto v = try_substr_view(start, count); return v ? v.value().try_to_string() : Result<String>::failure(v.error()); }

Result<void> String::try_append(const String &v) noexcept { return guarded_void([&] { value_ += v.value_; }, "string.append"); }
Result<void> String::try_append(const StringView &v) noexcept { return guarded_void([&] { value_ += v.bytes_; }, "string.append"); }
Result<void> String::try_append(code_point_type v, size_type count) noexcept { return guarded_void([&] { for (size_type i=0;i<count;++i) append_utf8(value_,v); }, "string.append_code_point"); }
Result<void> String::try_assign(const String &v) noexcept { return guarded_void([&] { value_=v.value_; }, "string.assign"); }
Result<void> String::try_assign(const StringView &v) noexcept { return guarded_void([&] { value_=std::string(v.bytes_); }, "string.assign"); }
Result<void> String::try_clear() noexcept { return guarded_void([&] { value_.clear(); }, "string.clear"); }
Result<void> String::try_pop_back() noexcept { if(empty()) return invalid("String is empty","string.pop_back"); return try_erase(size()-1,1); }
Result<void> String::try_swap(String &v) noexcept { return guarded_void([&] { value_.swap(v.value_); }, "string.swap"); }
Result<void> String::try_shrink_to_fit() noexcept { return guarded_void([&] { value_.shrink_to_fit(); }, "string.shrink_to_fit"); }

Result<void> String::try_insert(size_type index, const String &v) noexcept { return try_replace(index,0,v); }
Result<void> String::try_insert(size_type index, const StringView &v) noexcept { return try_replace(index,0,v); }
Result<void> String::try_insert(size_type index, code_point_type cp, size_type count) noexcept { return try_replace(index,0,cp,count); }
Result<void> String::try_erase(size_type index, size_type count) noexcept { return try_replace(index,count,String("")); }
Result<void> String::try_replace(size_type index, size_type count, const String &v) noexcept {
    const auto positions = offsets(value_);
    if (index > positions.size()-1) return invalid("String index is out of range","string.replace");
    const auto end = count == npos ? positions.size()-1 : std::min(index+count,positions.size()-1);
    return guarded_void([&] { value_.replace(positions[index], positions[end]-positions[index], v.value_); }, "string.replace");
}
Result<void> String::try_replace(size_type index, size_type count, const StringView &v) noexcept {
    const auto value = v.try_to_string();
    if (!value) return Result<void>::failure(value.error());
    return try_replace(index, count, value.value());
}
Result<void> String::try_replace(size_type index, size_type count, code_point_type cp, size_type repeat_count) noexcept { return try_replace(index,count,String(code_points_to_utf8(std::vector<char32_t>(repeat_count,cp)),0)); }
Result<void> String::try_replace_all(const String &a, const String &b) noexcept { if(a.empty()) return Result<void>::success(); return guarded_void([&] { value_=::sindre::general::string::replace_all(value_,a.value_,b.value_); }, "string.replace_all"); }
Result<void> String::try_replace_all(const StringView &a, const StringView &b) noexcept { return try_replace_all(String(a.bytes_),String(b.bytes_)); }
Result<void> String::try_push_back(code_point_type cp) noexcept { return try_append(cp); }
Result<void> String::try_resize(size_type count, code_point_type fill) noexcept { const auto current=size(); if(count<current) return try_erase(count,current-count); if(count>current) return try_append(fill,count-current); return Result<void>::success(); }
Result<void> String::try_trim() noexcept { return guarded_void([&] { value_=std::string(trim(value_)); }, "string.trim"); }
Result<void> String::try_lower_ascii() noexcept { return guarded_void([&] { value_=lower_ascii(value_); }, "string.lower_ascii"); }
Result<void> String::try_upper_ascii() noexcept { return guarded_void([&] { value_=upper_ascii(value_); }, "string.upper_ascii"); }
Result<std::vector<String>> String::try_split(const String &delimiter, bool keep_empty) const noexcept { return guarded<std::vector<String>>([&] { std::vector<String> result; auto parts=split(value_,delimiter.value_,keep_empty); for(auto &part:parts) result.emplace_back(std::string_view(part)); return result; }, "string.split"); }
Result<std::vector<String>> String::try_split(std::string_view delimiter, bool keep_empty) const noexcept { return try_split(String(delimiter),keep_empty); }
Result<String> String::try_repeat(size_type times) const noexcept { return guarded<String>([&] { return String(std::string_view(repeat(value_,times))); }, "string.repeat"); }
Result<std::string> String::try_to_utf8() const noexcept { return Result<std::string>::success(value_); }
Result<std::u16string> String::try_to_utf16() const noexcept {
    return guarded<std::u16string>([&] { std::u16string result; for(std::size_t i=0;i<size();++i){ auto cp=try_get_code_point(i).value(); if(cp<=0xffff) result.push_back(static_cast<char16_t>(cp)); else { cp-=0x10000; result.push_back(static_cast<char16_t>(0xd800+(cp>>10))); result.push_back(static_cast<char16_t>(0xdc00+(cp&0x3ff))); } } return result; }, "string.to_utf16");
}
std::string String::to_utf8() const { return value_; }
std::u16string String::to_utf16() const { return try_to_utf16().value(); }
Result<String::code_point_type> String::to_code_point() const noexcept { if(size()!=1) return Result<code_point_type>::failure(std::make_error_code(std::errc::invalid_argument),"String must contain exactly one Unicode code point","string.to_code_point"); return try_get_code_point(0); }
Result<char> String::to_char() const noexcept { auto cp=to_code_point(); if(!cp) return Result<char>::failure(cp.error()); if(cp.value()>0x7f) return Result<char>::failure(std::make_error_code(std::errc::illegal_byte_sequence),"Unicode code point does not fit in char","string.to_char"); return Result<char>::success(static_cast<char>(cp.value())); }
Result<std::filesystem::path> String::to_path() const noexcept { return guarded<std::filesystem::path>([&] { return std::filesystem::u8path(value_); }, "string.to_path"); }
Result<std::int64_t> String::to_int(int base) const noexcept { return parse_int(value_,base); }
Result<double> String::to_float() const noexcept { return parse_float(value_); }
Result<bool> String::to_bool() const noexcept { const auto v=lower_ascii(trim(value_)); if(v=="1"||v=="true"||v=="yes"||v=="on") return Result<bool>::success(true); if(v=="0"||v=="false"||v=="no"||v=="off") return Result<bool>::success(false); return Result<bool>::failure(std::make_error_code(std::errc::invalid_argument),"Expected a boolean value","string.to_bool"); }

bool StringView::is_empty() const noexcept { return bytes_.empty(); }
StringView::size_type StringView::get_size() const noexcept { return offsets(bytes_).size()-1; }
StringView::size_type StringView::get_storage_size() const noexcept { return bytes_.size(); }
Result<StringView::code_point_type> StringView::try_get_code_point(size_type index) const noexcept { const auto pos=offsets(bytes_); if(index+1>=pos.size()) return Result<code_point_type>::failure(std::make_error_code(std::errc::result_out_of_range),"String view index is out of range","string.view.get_code_point"); return Result<code_point_type>::success(decode(bytes_,pos[index]).value); }
Result<StringView::code_point_type> StringView::try_get_front() const noexcept { return try_get_code_point(0); }
Result<StringView::code_point_type> StringView::try_get_back() const noexcept { const auto n=get_size(); if(!n) return Result<code_point_type>::failure(std::make_error_code(std::errc::invalid_argument),"String view is empty","string.view.get_back"); return try_get_code_point(n-1); }
StringView::size_type StringView::find(const String &v,size_type s) const noexcept{return find(v.get_view(),s);}
StringView::size_type StringView::find(const StringView &v,size_type s) const noexcept{return find_view(bytes_,v.bytes_,s,false);}
StringView::size_type StringView::find(code_point_type cp,size_type s) const noexcept { for(size_type i=s;i<get_size();++i) if(try_get_code_point(i).value()==cp) return i; return npos; }
StringView::size_type StringView::rfind(const String &v,size_type s) const noexcept{return rfind(v.get_view(),s);}
StringView::size_type StringView::rfind(const StringView &v,size_type s) const noexcept{return find_view(bytes_,v.bytes_,s,true);}
StringView::size_type StringView::rfind(code_point_type cp,size_type s) const noexcept { if(!get_size()) return npos; size_type i=s==npos?get_size()-1:std::min(s,get_size()-1); for(;;){if(try_get_code_point(i).value()==cp)return i;if(i==0)break;--i;}return npos; }
StringView::size_type StringView::find_first_of(const StringView &v,size_type s) const noexcept{return find_set(bytes_,v.bytes_,s,false,false);}
StringView::size_type StringView::find_last_of(const StringView &v,size_type s) const noexcept{return find_set(bytes_,v.bytes_,s,true,false);}
StringView::size_type StringView::find_first_not_of(const StringView &v,size_type s) const noexcept{return find_set(bytes_,v.bytes_,s,false,true);}
StringView::size_type StringView::find_last_not_of(const StringView &v,size_type s) const noexcept{return find_set(bytes_,v.bytes_,s,true,true);}
StringView::size_type StringView::find_first_of(const String &v,size_type s) const noexcept{return find_first_of(v.get_view(),s);}
StringView::size_type StringView::find_last_of(const String &v,size_type s) const noexcept{return find_last_of(v.get_view(),s);}
StringView::size_type StringView::find_first_not_of(const String &v,size_type s) const noexcept{return find_first_not_of(v.get_view(),s);}
StringView::size_type StringView::find_last_not_of(const String &v,size_type s) const noexcept{return find_last_not_of(v.get_view(),s);}
bool StringView::starts_with(const StringView &v) const noexcept{return find(v,0)==0;}
bool StringView::ends_with(const StringView &v) const noexcept{const auto n=get_size(),m=v.get_size();return m<=n&&rfind(v,n-m)==n-m;}
bool StringView::contains(const StringView &v) const noexcept{return find(v)!=npos;}
StringView::size_type StringView::count(const StringView &v) const noexcept{if(v.is_empty())return get_size()+1;size_type r=0;for(size_type i=0;i<=get_size();){auto x=find(v,i);if(x==npos)break;++r;i=x+v.get_size();}return r;}
bool StringView::starts_with(const String &v) const noexcept{return starts_with(v.get_view());}
bool StringView::ends_with(const String &v) const noexcept{return ends_with(v.get_view());}
bool StringView::contains(const String &v) const noexcept{return contains(v.get_view());}
StringView::size_type StringView::count(const String &v) const noexcept{return count(v.get_view());}
int StringView::compare(const StringView &v) const noexcept{return bytes_.compare(v.bytes_);}
int StringView::compare(const String &v) const noexcept{return compare(v.get_view());}
Result<StringView> StringView::try_substr(size_type start,size_type count) const noexcept{const auto pos=offsets(bytes_);if(start+1>pos.size())return Result<StringView>::failure(std::make_error_code(std::errc::invalid_argument),"String view index is out of range","string.view.substr");const auto end=count==npos?pos.size()-1:std::min(start+count,pos.size()-1);return Result<StringView>::success(StringView(bytes_.substr(pos[start],pos[end]-pos[start])));}
StringView StringView::remove_prefix(size_type count) const noexcept{auto x=try_substr(count);return x?x.value():StringView{};}
StringView StringView::remove_suffix(size_type count) const noexcept{const auto n=get_size();if(count>n)return {};const auto value=try_substr(0,n-count);return value?value.value():StringView{};}
Result<String> StringView::try_to_string() const noexcept{return guarded<String>([&]{return String(bytes_);},"string.view.to_string");}

} // namespace sindre::general::string

namespace sindre::general::regex {
struct Regex::Impl {
#if defined(SINDRE_WITH_RE2)
    std::shared_ptr<::re2::RE2> pattern;
#endif
};
Regex::Regex(std::shared_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
Regex::Regex(Regex &&) noexcept = default;
Regex &Regex::operator=(Regex &&) noexcept = default;
Regex::~Regex() = default;
Result<Regex> Regex::compile(std::string pattern) noexcept {
#if defined(SINDRE_WITH_RE2)
    return ::sindre::general::Result<Regex>::success([&] {
        auto impl=std::make_shared<Impl>(); impl->pattern=std::make_shared<::re2::RE2>(std::move(pattern)); return Regex(std::move(impl));
    }());
#else
    (void)pattern; return Result<Regex>::failure(std::make_error_code(std::errc::function_not_supported),"RE2 support is not enabled","regex.compile");
#endif
}
bool Regex::match(std::string_view text) const noexcept {
#if defined(SINDRE_WITH_RE2)
    return impl_ && impl_->pattern && ::re2::RE2::FullMatch(::re2::StringPiece(text.data(),text.size()),*impl_->pattern);
#else
    (void)text; return false;
#endif
}
Result<bool> Regex::try_match(std::string_view text) const noexcept { if(!impl_) return Result<bool>::failure(std::make_error_code(std::errc::invalid_argument),"Regex is not initialized","regex.match"); return Result<bool>::success(match(text)); }
Result<std::string> Regex::replace(std::string_view text,std::string_view replacement) const noexcept {
#if defined(SINDRE_WITH_RE2)
    if(!impl_||!impl_->pattern)return Result<std::string>::failure(std::make_error_code(std::errc::invalid_argument),"Regex is not initialized","regex.replace"); std::string result(text); ::re2::RE2::GlobalReplace(&result,*impl_->pattern,std::string(replacement)); return Result<std::string>::success(std::move(result));
#else
    (void)text;(void)replacement;return Result<std::string>::failure(std::make_error_code(std::errc::function_not_supported),"RE2 support is not enabled","regex.replace");
#endif
}
} // namespace sindre::general::regex
