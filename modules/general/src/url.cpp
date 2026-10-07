#include <sindre/general/network.h>

#include <algorithm>
#include <cctype>
#include <limits>

namespace sindre::general::network {
namespace {

template <class T>
Result<T> failure(NetworkErrc code, std::string message, std::string context) {
    return Result<T>::failure(make_error_code(code), std::move(message), std::move(context));
}

bool is_control(char value) noexcept {
    return static_cast<unsigned char>(value) < 0x20u || static_cast<unsigned char>(value) == 0x7fu;
}

int hex_value(char value) noexcept {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool is_unreserved(unsigned char value) noexcept {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') || value == '-' || value == '.' || value == '_' || value == '~';
}

} // namespace

Result<std::string> encode(std::string_view value) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        static constexpr char digits[] = "0123456789ABCDEF";
        std::string result;
        result.reserve(value.size());
        for (const unsigned char byte : value) {
            if (is_unreserved(byte)) {
                result.push_back(static_cast<char>(byte));
            } else {
                result.push_back('%');
                result.push_back(digits[byte >> 4]);
                result.push_back(digits[byte & 0x0f]);
            }
        }
        return Result<std::string>::success(std::move(result));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return failure<std::string>(NetworkErrc::invalid_url, error.what(), "network.encode");
    } catch (...) {
        return failure<std::string>(NetworkErrc::invalid_url, "URL encoding failed", "network.encode");
    }
#endif
}

Result<std::string> decode(std::string_view value) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::string result;
        result.reserve(value.size());
        for (std::size_t index = 0; index < value.size(); ++index) {
            if (value[index] != '%') {
                result.push_back(value[index]);
                continue;
            }
            if (index + 2 >= value.size()) return failure<std::string>(
                NetworkErrc::invalid_url, "Invalid percent escape", "network.decode");
            const auto high = hex_value(value[index + 1]);
            const auto low = hex_value(value[index + 2]);
            if (high < 0 || low < 0) return failure<std::string>(
                NetworkErrc::invalid_url, "Invalid percent escape", "network.decode");
            result.push_back(static_cast<char>((high << 4) | low));
            index += 2;
        }
        return Result<std::string>::success(std::move(result));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return failure<std::string>(NetworkErrc::invalid_url, error.what(), "network.decode");
    } catch (...) {
        return failure<std::string>(NetworkErrc::invalid_url, "URL decoding failed", "network.decode");
    }
#endif
}

Result<Query> parse_query(std::string_view input) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        Query result;
        if (input.empty()) return Result<Query>::success(std::move(result));
        std::size_t begin = 0;
        while (true) {
            const auto separator = input.find('&', begin);
            const auto item = input.substr(begin, separator == std::string_view::npos ?
                std::string_view::npos : separator - begin);
            const auto equal = item.find('=');
            const auto key = decode(item.substr(0, equal));
            if (!key) return Result<Query>::failure(key.error().with_context("url.parse_query"));
            const auto value = decode(equal == std::string_view::npos ?
                std::string_view{} : item.substr(equal + 1));
            if (!value) return Result<Query>::failure(value.error().with_context("url.parse_query"));
            result.emplace_back(key.value(), value.value());
            if (separator == std::string_view::npos) break;
            begin = separator + 1;
        }
        return Result<Query>::success(std::move(result));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return failure<Query>(NetworkErrc::invalid_url, error.what(), "url.parse_query");
    } catch (...) {
        return failure<Query>(NetworkErrc::invalid_url, "URL query parsing failed", "url.parse_query");
    }
#endif
}

Result<std::string> build_query(const Query &query) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::string result;
        for (const auto &item : query) {
            const auto key = encode(item.first);
            const auto value = encode(item.second);
            if (!key) return Result<std::string>::failure(key.error().with_context("url.build_query"));
            if (!value) return Result<std::string>::failure(value.error().with_context("url.build_query"));
            if (!result.empty()) result.push_back('&');
            result += key.value();
            result.push_back('=');
            result += value.value();
        }
        return Result<std::string>::success(std::move(result));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return failure<std::string>(NetworkErrc::invalid_url, error.what(), "url.build_query");
    } catch (...) {
        return failure<std::string>(NetworkErrc::invalid_url, "URL query formatting failed", "url.build_query");
    }
#endif
}

Result<Url> Url::parse(std::string_view value) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        if (value.empty()) return failure<Url>(NetworkErrc::invalid_url, "URL is empty", "url.parse");
        for (const char character : value) {
            if (is_control(character)) return failure<Url>(
                NetworkErrc::invalid_url, "URL contains a control character", "url.parse");
        }
        const auto scheme_end = value.find("://");
        if (scheme_end == std::string_view::npos || scheme_end == 0)
            return failure<Url>(NetworkErrc::invalid_url, "URL must contain a scheme", "url.parse");
        Url result;
        result.scheme_.assign(value.substr(0, scheme_end));
        std::transform(result.scheme_.begin(), result.scheme_.end(), result.scheme_.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (result.scheme_ != "http" && result.scheme_ != "https")
            return failure<Url>(NetworkErrc::unsupported_scheme, "Only http and https are supported", "url.parse");

        const auto authority_begin = scheme_end + 3;
        const auto authority_end = value.find_first_of("/?#", authority_begin);
        const auto authority = value.substr(authority_begin,
            authority_end == std::string_view::npos ? std::string_view::npos : authority_end - authority_begin);
        if (authority.empty() || authority.find('@') != std::string_view::npos)
            return failure<Url>(NetworkErrc::invalid_url, "URL host is missing or contains user information", "url.parse");

        std::string_view host;
        std::string_view port;
        if (authority.front() == '[') {
            const auto close = authority.find(']');
            if (close == std::string_view::npos || close == 1)
                return failure<Url>(NetworkErrc::invalid_url, "Invalid IPv6 host", "url.parse");
            host = authority.substr(1, close - 1);
            if (close + 1 < authority.size()) {
                if (authority[close + 1] != ':') return failure<Url>(
                    NetworkErrc::invalid_url, "Invalid IPv6 port", "url.parse");
                port = authority.substr(close + 2);
            }
        } else {
            const auto colon = authority.rfind(':');
            if (colon != std::string_view::npos) {
                if (authority.find(':') != colon) return failure<Url>(
                    NetworkErrc::invalid_url, "IPv6 hosts must use brackets", "url.parse");
                host = authority.substr(0, colon);
                port = authority.substr(colon + 1);
            } else {
                host = authority;
            }
        }
        if (host.empty()) return failure<Url>(NetworkErrc::invalid_url, "URL host is empty", "url.parse");
        for (const char character : host) {
            if (std::isspace(static_cast<unsigned char>(character)) || character == '/' || character == '?' || character == '#')
                return failure<Url>(NetworkErrc::invalid_url, "Invalid URL host", "url.parse");
        }
        result.host_.assign(host);
        if (!port.empty()) {
            std::uint32_t parsed_port = 0;
            for (const char character : port) {
                if (character < '0' || character > '9') return failure<Url>(
                    NetworkErrc::invalid_url, "Invalid URL port", "url.parse");
                parsed_port = parsed_port * 10u + static_cast<std::uint32_t>(character - '0');
                if (parsed_port > 65535u) return failure<Url>(
                    NetworkErrc::invalid_url, "URL port is out of range", "url.parse");
            }
            if (parsed_port == 0) return failure<Url>(NetworkErrc::invalid_url, "URL port must be nonzero", "url.parse");
            result.port_ = static_cast<std::uint16_t>(parsed_port);
            result.explicit_port_ = true;
        } else {
            result.port_ = result.scheme_ == "https" ? 443 : 80;
        }
        const auto fragment = value.find('#', authority_begin);
        if (fragment != std::string_view::npos)
            return failure<Url>(NetworkErrc::invalid_url, "URL fragments are not supported for HTTP", "url.parse");
        const auto path_begin = authority_end == std::string_view::npos ? value.size() : authority_end;
        const auto query_mark = value.find('?', path_begin);
        const auto path_end = query_mark == std::string_view::npos ? value.size() : query_mark;
        if (path_begin < value.size() && value[path_begin] == '/') result.path_.assign(value.substr(path_begin, path_end - path_begin));
        else if (path_begin != path_end) return failure<Url>(NetworkErrc::invalid_url, "Invalid URL path", "url.parse");
        if (result.path_.empty()) result.path_ = "/";
        if (query_mark != std::string_view::npos) {
            const auto query = parse_query(value.substr(query_mark + 1));
            if (!query) return Result<Url>::failure(query.error().with_context("url.parse"));
            result.query_ = std::move(query.value());
        }
        return Result<Url>::success(std::move(result));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return failure<Url>(NetworkErrc::invalid_url, error.what(), "url.parse");
    } catch (...) {
        return failure<Url>(NetworkErrc::invalid_url, "URL parsing failed", "url.parse");
    }
#endif
}

Result<std::string> Url::format() const noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        if (scheme_ != "http" && scheme_ != "https") return failure<std::string>(
            NetworkErrc::unsupported_scheme, "Only http and https are supported", "url.format");
        if (host_.empty()) return failure<std::string>(NetworkErrc::invalid_url, "URL host is empty", "url.format");
        const auto query = build_query(query_);
        if (!query) return Result<std::string>::failure(query.error().with_context("url.format"));
        std::string result = scheme_ + "://";
        if (host_.find(':') != std::string::npos) result += "[" + host_ + "]";
        else result += host_;
        const auto default_port = scheme_ == "https" ? 443 : 80;
        if (explicit_port_ || port_ != default_port) result += ":" + std::to_string(port_);
        result += path_.empty() ? "/" : path_;
        if (!query.value().empty()) result += "?" + query.value();
        return Result<std::string>::success(std::move(result));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return failure<std::string>(NetworkErrc::invalid_url, error.what(), "url.format");
    } catch (...) {
        return failure<std::string>(NetworkErrc::invalid_url, "URL formatting failed", "url.format");
    }
#endif
}

Result<void> Url::set_query(const Query &query) noexcept {
    const auto built = build_query(query);
    if (!built) return Result<void>::failure(built.error().with_context("url.set_query"));
    query_ = query;
    return Result<void>::success();
}

std::string Url::get_scheme() const { return scheme_; }
std::string Url::get_host() const { return host_; }
std::uint16_t Url::get_port() const { return port_; }
std::string Url::get_path() const { return path_; }
Query Url::get_query() const { return query_; }

Result<Url> parse(std::string_view value) noexcept { return Url::parse(value); }

} // namespace sindre::general::network
