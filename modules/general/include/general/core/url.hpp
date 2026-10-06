#pragma once

#include <general/core/async.hpp>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace sindrecpp::general::url {

inline std::string encode_component(std::string_view input) {
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    for (unsigned char c : input) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') result.push_back(static_cast<char>(c));
        else { result.push_back('%'); result.push_back(digits[c >> 4]); result.push_back(digits[c & 15]); }
    }
    return result;
}

inline ::sindrecpp::general::Result<std::string> decode_component(std::string_view input) {
    std::string result;
    auto hex = [](char c) -> int { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return -1; };
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%') {
            if (i + 2 >= input.size() || hex(input[i + 1]) < 0 || hex(input[i + 2]) < 0)
                return ::sindrecpp::general::Result<std::string>::failure(
                    std::make_error_code(std::errc::invalid_argument), "Invalid percent escape", "url.decode");
            result.push_back(static_cast<char>((hex(input[i + 1]) << 4) | hex(input[i + 2]))); i += 2;
        } else result.push_back(input[i] == '+' ? ' ' : input[i]);
    }
    return ::sindrecpp::general::Result<std::string>::success(std::move(result));
}

using Query = std::vector<std::pair<std::string, std::string>>;
inline ::sindrecpp::general::Result<Query> parse_query(std::string_view input) {
    Query result;
    while (!input.empty()) {
        const auto amp = input.find('&'); const auto item = input.substr(0, amp);
        const auto equal = item.find('=');
        auto key = decode_component(item.substr(0, equal));
        auto value = decode_component(equal == std::string_view::npos ? std::string_view{} : item.substr(equal + 1));
        if (!key || !value) return ::sindrecpp::general::Result<Query>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid URL query", "url.parse_query");
        result.emplace_back(std::move(key.value()), std::move(value.value()));
        if (amp == std::string_view::npos) break; input.remove_prefix(amp + 1);
    }
    return ::sindrecpp::general::Result<Query>::success(std::move(result));
}

inline std::string build_query(const Query &query) {
    std::string result; for (const auto &[key, value] : query) { if (!result.empty()) result += '&'; result += encode_component(key) + '=' + encode_component(value); } return result;
}

} // namespace sindrecpp::general::url
