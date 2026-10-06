#pragma once

#if !defined(SINDRECPP_WITH_HTTP)
#error "Enable SINDRECPP_WITH_HTTP and link SindreCpp::General before including this header."
#endif

#include <httplib.h>
#include <general/core/async.hpp>
#if defined(SINDRECPP_WITH_JSON)
#include <general/core/json.hpp>
#endif
#include <chrono>
#include <thread>
#include <stdexcept>

namespace sindrecpp::general::http {

using Client = httplib::Client;
using Server = httplib::Server;
using Request = httplib::Request;
using Response = httplib::Response;
using Result = httplib::Result;
namespace native = httplib;

struct RequestOptions {
    int connect_timeout_seconds = 5;
    int read_timeout_seconds = 5;
    int write_timeout_seconds = 5;
    int retries = 2;
    std::chrono::milliseconds retry_delay{50};
    std::chrono::milliseconds total_timeout{0};
    ::sindrecpp::general::CancellationToken token{};
};

struct ResponseData {
    int status = 0;
    std::string body;
    std::string content_type;
};

inline ::sindrecpp::general::Result<ResponseData>
get(std::string host, int port, std::string path = "/", RequestOptions options = {}) {
    if (host.empty() || port < 1 || port > 65535 || path.empty() || options.connect_timeout_seconds < 0 ||
        options.read_timeout_seconds < 0 || options.write_timeout_seconds < 0 || options.retries < 0 ||
        options.retry_delay.count() < 0 || options.total_timeout.count() < 0)
        return ::sindrecpp::general::Result<ResponseData>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid HTTP request options", "http.get");
    if (path.front() != '/') path.insert(path.begin(), '/');
#if !defined(SINDRECPP_NO_EXCEPTIONS)
    try {
#endif
        const auto started = std::chrono::steady_clock::now();
        Client client(host, port);
        client.set_connection_timeout(options.connect_timeout_seconds);
        client.set_read_timeout(options.read_timeout_seconds);
        client.set_write_timeout(options.write_timeout_seconds);
        for (int attempt = 0; attempt <= options.retries; ++attempt) {
            if (options.token.cancelled())
                return ::sindrecpp::general::Result<ResponseData>::failure(
                    std::make_error_code(std::errc::operation_canceled), "HTTP request cancelled", "http.get");
            if (options.total_timeout.count() > 0 && std::chrono::steady_clock::now() - started >= options.total_timeout)
                return ::sindrecpp::general::Result<ResponseData>::failure(
                    std::make_error_code(std::errc::timed_out), "HTTP request timed out", "http.get");
            auto response = client.Get(path.c_str());
            if (response) {
                ResponseData result{response->status, response->body,
                                    response->get_header_value("Content-Type")};
                if (result.status < 500 || attempt == options.retries)
                    return ::sindrecpp::general::Result<ResponseData>::success(std::move(result));
            } else if (attempt == options.retries) {
                return ::sindrecpp::general::Result<ResponseData>::failure(
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
#if !defined(SINDRECPP_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return ::sindrecpp::general::Result<ResponseData>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "http.get");
    } catch (...) {
        return ::sindrecpp::general::Result<ResponseData>::failure(
            std::make_error_code(std::errc::io_error), "Unknown HTTP failure", "http.get");
    }
#endif
    return ::sindrecpp::general::Result<ResponseData>::failure(
        std::make_error_code(std::errc::io_error), "HTTP GET retry loop failed", "http.get");
}

inline ::sindrecpp::general::Result<ResponseData>
post(std::string host, int port, std::string path, std::string body,
     std::string content_type = "application/octet-stream", RequestOptions options = {}) {
    if (host.empty() || port < 1 || port > 65535 || path.empty())
        return ::sindrecpp::general::Result<ResponseData>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid HTTP POST request", "http.post");
    if (path.front() != '/') path.insert(path.begin(), '/');
#if !defined(SINDRECPP_NO_EXCEPTIONS)
    try {
#endif
        Client client(host, port);
        client.set_connection_timeout(options.connect_timeout_seconds);
        client.set_read_timeout(options.read_timeout_seconds);
        client.set_write_timeout(options.write_timeout_seconds);
        for (int attempt = 0; attempt <= options.retries; ++attempt) {
            if (options.token.cancelled()) return ::sindrecpp::general::Result<ResponseData>::failure(
                std::make_error_code(std::errc::operation_canceled), "HTTP request cancelled", "http.post");
            auto response = client.Post(path.c_str(), body, content_type.c_str());
            if (response) return ::sindrecpp::general::Result<ResponseData>::success(
                ResponseData{response->status, response->body, response->get_header_value("Content-Type")});
            if (attempt != options.retries) std::this_thread::sleep_for(options.retry_delay);
        }
#if !defined(SINDRECPP_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return ::sindrecpp::general::Result<ResponseData>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "http.post");
    } catch (...) {
        return ::sindrecpp::general::Result<ResponseData>::failure(
            std::make_error_code(std::errc::io_error), "Unknown HTTP POST failure", "http.post");
    }
#endif
    return ::sindrecpp::general::Result<ResponseData>::failure(
        std::make_error_code(std::errc::connection_refused), "HTTP POST failed", "http.post");
}

#if defined(SINDRECPP_WITH_JSON)
inline ::sindrecpp::general::Result<::sindrecpp::general::json::Document>
get_json(std::string host, int port, std::string path = "/", RequestOptions options = {}) {
    auto response = get(std::move(host), port, std::move(path), options);
    if (!response)
        return ::sindrecpp::general::Result<::sindrecpp::general::json::Document>::failure(
            response.error());
    if (response.value().status < 200 || response.value().status >= 300)
        return ::sindrecpp::general::Result<::sindrecpp::general::json::Document>::failure(
            std::make_error_code(std::errc::protocol_error),
            "HTTP response is not successful: " + std::to_string(response.value().status), "http.get_json");
    auto document = ::sindrecpp::general::json::try_parse(response.value().body);
    if (!document)
        return ::sindrecpp::general::Result<::sindrecpp::general::json::Document>::failure(
            document.error().code, document.error().message, "http.get_json");
    return document;
}
#endif

} // namespace sindrecpp::general::http
