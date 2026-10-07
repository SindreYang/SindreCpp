#include <sindre/general/network.h>

#if defined(SINDRE_WITH_HTTP)


#endif

// Network implementation boundary.

#include <cctype>

namespace sindre::general::url {

std::string encode_component(std::string_view input) {
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    for (const unsigned char byte : input) {
        if (std::isalnum(byte) || byte == '-' || byte == '_' || byte == '.' || byte == '~') result.push_back(static_cast<char>(byte));
        else { result.push_back('%'); result.push_back(digits[byte >> 4]); result.push_back(digits[byte & 15]); }
    }
    return result;
}

Result<std::string> decode_component(std::string_view input) {
    const auto hex = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    };
    std::string result;
    for (std::size_t index = 0; index < input.size(); ++index) {
        if (input[index] == '%') {
            if (index + 2 >= input.size() || hex(input[index + 1]) < 0 || hex(input[index + 2]) < 0)
                return Result<std::string>::failure(
                    std::make_error_code(std::errc::invalid_argument), "Invalid percent escape", "url.decode");
            result.push_back(static_cast<char>((hex(input[index + 1]) << 4) | hex(input[index + 2])));
            index += 2;
        } else result.push_back(input[index] == '+' ? ' ' : input[index]);
    }
    return Result<std::string>::success(std::move(result));
}

Result<Query> parse_query(std::string_view input) {
    Query result;
    while (!input.empty()) {
        const auto amp = input.find('&');
        const auto item = input.substr(0, amp);
        const auto equal = item.find('=');
        auto key = decode_component(item.substr(0, equal));
        auto value = decode_component(equal == std::string_view::npos ? std::string_view{} : item.substr(equal + 1));
        if (!key || !value) return Result<Query>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid URL query", "url.parse_query");
        result.emplace_back(std::move(key.value()), std::move(value.value()));
        if (amp == std::string_view::npos) break;
        input.remove_prefix(amp + 1);
    }
    return Result<Query>::success(std::move(result));
}

std::string build_query(const Query &query) {
    std::string result;
    for (const auto &[key, value] : query) {
        if (!result.empty()) result += '&';
        result += encode_component(key) + '=' + encode_component(value);
    }
    return result;
}

} // namespace sindre::general::url

// ---- merged from http_impl.cpp ----

#if defined(SINDRE_WITH_HTTP)

#include <httplib.h>

#if defined(SINDRE_WITH_JSON)

#endif
#include <chrono>
#include <thread>
#include <stdexcept>

namespace sindre::general::http {

using Client = httplib::Client;
using Server = httplib::Server;
using Request = httplib::Request;
using Response = httplib::Response;
using Result = httplib::Result;
namespace native = httplib;

::sindre::general::Result<ResponseData>
get(std::string host, int port, std::string path, RequestOptions options) {
    if (host.empty() || port < 1 || port > 65535 || path.empty() || options.connect_timeout_seconds < 0 ||
        options.read_timeout_seconds < 0 || options.write_timeout_seconds < 0 || options.retries < 0 ||
        options.retry_delay.count() < 0 || options.total_timeout.count() < 0)
        return ::sindre::general::Result<ResponseData>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid HTTP request options", "http.get");
    if (path.front() != '/') path.insert(path.begin(), '/');
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        const auto started = std::chrono::steady_clock::now();
        Client client(host, port);
        client.set_connection_timeout(options.connect_timeout_seconds);
        client.set_read_timeout(options.read_timeout_seconds);
        client.set_write_timeout(options.write_timeout_seconds);
        for (int attempt = 0; attempt <= options.retries; ++attempt) {
            if (options.token.cancelled())
                return ::sindre::general::Result<ResponseData>::failure(
                    std::make_error_code(std::errc::operation_canceled), "HTTP request cancelled", "http.get");
            if (options.total_timeout.count() > 0 && std::chrono::steady_clock::now() - started >= options.total_timeout)
                return ::sindre::general::Result<ResponseData>::failure(
                    std::make_error_code(std::errc::timed_out), "HTTP request timed out", "http.get");
            auto response = client.Get(path.c_str());
            if (response) {
                ResponseData result{response->status, response->body,
                                    response->get_header_value("Content-Type")};
                if (result.status < 500 || attempt == options.retries)
                    return ::sindre::general::Result<ResponseData>::success(std::move(result));
            } else if (attempt == options.retries) {
                return ::sindre::general::Result<ResponseData>::failure(
                    std::make_error_code(std::errc::connection_refused), "HTTP GET failed", "http.get");
            }
            if (options.retry_delay.count()) {
                const auto step = std::chrono::milliseconds(5);
                auto remaining = options.retry_delay;
                while (remaining.count() > 0 && !options.token.cancelled()) {
                    const auto wait = remaining < step ? remaining : step;
                    std::this_thread::sleep_for(wait);
                    remaining -= wait;
                }
            }
        }
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return ::sindre::general::Result<ResponseData>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "http.get");
    } catch (...) {
        return ::sindre::general::Result<ResponseData>::failure(
            std::make_error_code(std::errc::io_error), "Unknown HTTP failure", "http.get");
    }
#endif
    return ::sindre::general::Result<ResponseData>::failure(
        std::make_error_code(std::errc::io_error), "HTTP GET retry loop failed", "http.get");
}

::sindre::general::Result<ResponseData>
post(std::string host, int port, std::string path, std::string body,
     std::string content_type, RequestOptions options) {
    if (host.empty() || port < 1 || port > 65535 || path.empty())
        return ::sindre::general::Result<ResponseData>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid HTTP POST request", "http.post");
    if (path.front() != '/') path.insert(path.begin(), '/');
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        Client client(host, port);
        client.set_connection_timeout(options.connect_timeout_seconds);
        client.set_read_timeout(options.read_timeout_seconds);
        client.set_write_timeout(options.write_timeout_seconds);
        for (int attempt = 0; attempt <= options.retries; ++attempt) {
            if (options.token.cancelled()) return ::sindre::general::Result<ResponseData>::failure(
                std::make_error_code(std::errc::operation_canceled), "HTTP request cancelled", "http.post");
            auto response = client.Post(path.c_str(), body, content_type.c_str());
            if (response) return ::sindre::general::Result<ResponseData>::success(
                ResponseData{response->status, response->body, response->get_header_value("Content-Type")});
            if (attempt != options.retries) std::this_thread::sleep_for(options.retry_delay);
        }
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return ::sindre::general::Result<ResponseData>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "http.post");
    } catch (...) {
        return ::sindre::general::Result<ResponseData>::failure(
            std::make_error_code(std::errc::io_error), "Unknown HTTP POST failure", "http.post");
    }
#endif
    return ::sindre::general::Result<ResponseData>::failure(
        std::make_error_code(std::errc::connection_refused), "HTTP POST failed", "http.post");
}

#if defined(SINDRE_WITH_JSON)
::sindre::general::Result<::sindre::general::json::Document>
get_json(std::string host, int port, std::string path, RequestOptions options) {
    auto response = get(std::move(host), port, std::move(path), options);
    if (!response)
        return ::sindre::general::Result<::sindre::general::json::Document>::failure(
            response.error());
    if (response.value().status < 200 || response.value().status >= 300)
        return ::sindre::general::Result<::sindre::general::json::Document>::failure(
            std::make_error_code(std::errc::protocol_error),
            "HTTP response is not successful: " + std::to_string(response.value().status), "http.get_json");
    auto document = ::sindre::general::json::try_parse(response.value().body);
    if (!document)
        return ::sindre::general::Result<::sindre::general::json::Document>::failure(
            document.error().code, document.error().message, "http.get_json");
    return document;
}
#endif

} // namespace sindre::general::http

#endif

// ---- merged from transfer_impl.cpp ----

#include <filesystem>
#include <fstream>
#include <functional>
#include <cstdint>
#include <iterator>
#include <string>

#if defined(SINDRE_WITH_HTTP)

#endif

namespace sindre::general::transfer {

using Progress = std::function<void(std::uint64_t current, std::uint64_t total)>;

#if defined(SINDRE_WITH_HTTP)
::sindre::general::Result<std::uint64_t> download(
    std::string host, int port, std::string remote_path, const std::filesystem::path &destination,
    ::sindre::general::http::RequestOptions options, Progress progress) noexcept {
#if defined(SINDRE_NO_EXCEPTIONS)
    {
#else
    try {
#endif
        if (host.empty() || port < 1 || port > 65535 || remote_path.empty())
            return ::sindre::general::Result<std::uint64_t>::failure(
                std::make_error_code(std::errc::invalid_argument), "Invalid download request", "transfer.download");
        if (remote_path.front() != '/') remote_path.insert(remote_path.begin(), '/');
        std::ofstream output(destination, std::ios::binary | std::ios::trunc);
        if (!output) return ::sindre::general::Result<std::uint64_t>::failure(
            std::make_error_code(std::errc::permission_denied), "Cannot open download destination", "transfer.download");
        ::sindre::general::http::Client client(host, port);
        client.set_connection_timeout(options.connect_timeout_seconds);
        client.set_read_timeout(options.read_timeout_seconds);
        client.set_write_timeout(options.write_timeout_seconds);
        std::uint64_t size = 0;
        auto response = client.Get(remote_path.c_str(), [&](const char *data, std::size_t length) {
            if (options.token.cancelled()) return false;
            output.write(data, static_cast<std::streamsize>(length));
            if (!output) return false;
            size += static_cast<std::uint64_t>(length);
            if (progress) progress(size, 0);
            return true;
        });
        if (!response) return ::sindre::general::Result<std::uint64_t>::failure(
            std::make_error_code(std::errc::connection_refused), "HTTP download failed", "transfer.download");
        if (response->status < 200 || response->status >= 300)
            return ::sindre::general::Result<std::uint64_t>::failure(
                std::make_error_code(std::errc::protocol_error), "HTTP download failed", "transfer.download");
        if (progress) progress(size, size);
        return ::sindre::general::Result<std::uint64_t>::success(size);
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return ::sindre::general::Result<std::uint64_t>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "transfer.download");
    }
#else
    }
#endif
}

::sindre::general::Result<int> upload(
    std::string host, int port, std::string remote_path, const std::filesystem::path &source,
    std::string content_type, ::sindre::general::http::RequestOptions options, Progress progress) noexcept {
#if defined(SINDRE_NO_EXCEPTIONS)
    {
#else
    try {
#endif
        std::ifstream input(source, std::ios::binary);
        if (!input) return ::sindre::general::Result<int>::failure(
            std::make_error_code(std::errc::no_such_file_or_directory), "Cannot open upload source", "transfer.upload");
        std::error_code file_error;
        const auto file_size = std::filesystem::file_size(source, file_error);
        if (file_error) return ::sindre::general::Result<int>::failure(
            file_error, "Cannot determine upload size", "transfer.upload");
        const auto total = static_cast<std::uint64_t>(file_size);
        input.close();
        if (progress) progress(0, total);
        if (remote_path.empty() || remote_path.front() != '/') remote_path.insert(remote_path.begin(), '/');
        ::sindre::general::http::Client client(host, port);
        client.set_connection_timeout(options.connect_timeout_seconds);
        client.set_read_timeout(options.read_timeout_seconds);
        client.set_write_timeout(options.write_timeout_seconds);
        auto response = client.Post(remote_path.c_str(), static_cast<std::size_t>(total),
            [&](std::size_t offset, std::size_t length, ::httplib::DataSink &sink) {
                if (options.token.cancelled()) return false;
                std::ifstream file(source, std::ios::binary);
                if (!file) return false;
                file.seekg(static_cast<std::streamoff>(offset));
                std::string chunk(length, '\0');
                file.read(chunk.data(), static_cast<std::streamsize>(length));
                const auto read = static_cast<std::size_t>(file.gcount());
                if (read == 0 && offset < total) return false;
                sink.write(chunk.data(), read);
                if (progress) progress(static_cast<std::uint64_t>(offset + read), total);
                return true;
            }, content_type.c_str());
        if (!response) return ::sindre::general::Result<int>::failure(
            std::make_error_code(std::errc::connection_refused), "HTTP upload failed", "transfer.upload");
        if (response->status < 200 || response->status >= 300)
            return ::sindre::general::Result<int>::failure(
                std::make_error_code(std::errc::protocol_error), "HTTP upload failed", "transfer.upload");
        if (progress) progress(total, total);
        return ::sindre::general::Result<int>::success(response->status);
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return ::sindre::general::Result<int>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "transfer.upload");
    }
#else
    }
#endif
}
#else
::sindre::general::Result<std::uint64_t> download(
    std::string, int, std::string, const std::filesystem::path &, Progress = {}) noexcept {
    return ::sindre::general::Result<std::uint64_t>::failure(
        std::make_error_code(std::errc::function_not_supported), "HTTP support is not enabled", "transfer.download");
}
#endif

} // namespace sindre::general::transfer
