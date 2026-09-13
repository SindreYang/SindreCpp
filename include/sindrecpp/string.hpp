#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#if defined(SINDRECPP_WITH_STRING)
#include <cs_string.h>
#endif

namespace sindrecpp::string {

inline std::string_view trim(std::string_view text) noexcept {
    constexpr std::string_view whitespace = " \t\n\r\f\v";
    const auto first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) return {};
    const auto last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
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

inline std::vector<std::string_view> split(std::string_view text, char delimiter) {
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

#if defined(SINDRECPP_WITH_STRING)
namespace native = CsString;
using Utf8String = CsString::CsString;
using Utf16String = CsString::CsString_utf16;
#endif

} // namespace sindrecpp::string
