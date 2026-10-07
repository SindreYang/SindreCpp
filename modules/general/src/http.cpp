#include <sindre/general/network.h>

#if defined(SINDRE_WITH_HTTP)

#include <httplib.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <thread>

namespace sindre::general::network {
namespace {

template <class T>
Result<T> failure(NetworkErrc code, std::string message, std::string context) {
    return Result<T>::failure(make_error_code(code), std::move(message), std::move(context));
}

bool contains_forbidden_header_character(std::string_view value) noexcept {
    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (character == '\r' || character == '\n' || byte < 0x20u || byte == 0x7fu) return true;
    }
    return false;
}

Result<httplib::Headers> convert_headers(const Headers &headers) {
    httplib::Headers result;
    for (const auto &header : headers) {
        if (header.name.empty() || contains_forbidden_header_character(header.name) ||
            contains_forbidden_header_character(header.value))
            return Result<httplib::Headers>::failure(
                make_error_code(NetworkErrc::invalid_request),
                "Header name or value contains an invalid character", "http.headers");
        result.emplace(header.name, header.value);
    }
    return Result<httplib::Headers>::success(std::move(result));
}

std::string request_content_type(const Request &request) {
    for (const auto &header : request.headers) {
        if (header.name == "Content-Type" || header.name == "content-type") return header.value;
    }
    return "application/octet-stream";
}

bool is_idempotent(Method method) noexcept {
    return method == Method::get || method == Method::head || method == Method::options ||
           method == Method::put || method == Method::delete_;
}

std::error_code map_backend_error(httplib::Error error) noexcept {
    switch (error) {
    case httplib::Error::ConnectionTimeout:
        return make_error_code(NetworkErrc::connection_timeout);
    case httplib::Error::Timeout:
    case httplib::Error::Read:
        return make_error_code(NetworkErrc::read_timeout);
    case httplib::Error::Write:
        return make_error_code(NetworkErrc::write_timeout);
    case httplib::Error::Canceled:
        return make_error_code(NetworkErrc::operation_cancelled);
    case httplib::Error::SSLConnection:
    case httplib::Error::SSLLoadingCerts:
    case httplib::Error::SSLServerVerification:
    case httplib::Error::SSLServerHostnameVerification:
        return make_error_code(NetworkErrc::tls_failed);
    case httplib::Error::Connection:
    case httplib::Error::ConnectionClosed:
    case httplib::Error::ProxyConnection:
        return make_error_code(NetworkErrc::connection_failed);
    case httplib::Error::OpenFile:
        return make_error_code(NetworkErrc::file_io_failed);
    default:
        return make_error_code(NetworkErrc::connection_failed);
    }
}

std::string safe_error_text(httplib::Error error) {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        return httplib::to_string(error);
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (...) {
        return "cpp-httplib request failed";
    }
#endif
}

std::string request_target(const Url &target) {
    const auto query = build_query(target.get_query());
    if (!query) return {};
    auto path = target.get_path();
    if (path.empty()) path = "/";
    if (!query.value().empty()) path += "?" + query.value();
    return path;
}

std::chrono::milliseconds remaining_time(
    std::chrono::steady_clock::time_point deadline,
    std::chrono::milliseconds fallback) {
    if (deadline == std::chrono::steady_clock::time_point::max()) return fallback;
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
    return remaining < fallback ? remaining : fallback;
}

void set_client_timeouts(httplib::Client &client, const TimeoutOptions &timeout,
                         std::chrono::milliseconds total) {
    client.set_connection_timeout(timeout.connect);
    client.set_read_timeout(timeout.read);
    client.set_write_timeout(timeout.write);
    if (total.count() > 0) client.set_max_timeout(total);
}

TlsOptions effective_tls(const ClientOptions &client_options,
                         const RequestOptions &request_options) {
    auto tls = request_options.tls;
    if (tls.ca_file.empty()) tls.ca_file = client_options.tls.ca_file;
    if (tls.ca_directory.empty()) tls.ca_directory = client_options.tls.ca_directory;
    if (tls.client_certificate.empty()) tls.client_certificate = client_options.tls.client_certificate;
    if (tls.client_key.empty()) tls.client_key = client_options.tls.client_key;
    tls.verify_peer = tls.verify_peer && client_options.tls.verify_peer;
    tls.verify_host = tls.verify_host && client_options.tls.verify_host;
    return tls;
}

void configure_client(httplib::Client &client, const ClientOptions &client_options,
                      const RequestOptions &request_options) {
    const auto tls = effective_tls(client_options, request_options);
    if (!tls.ca_file.empty() || !tls.ca_directory.empty()) {
        client.set_ca_cert_path(tls.ca_file.u8string(), tls.ca_directory.u8string());
    } else {
        client.enable_system_ca(true);
    }
    client.enable_server_certificate_verification(tls.verify_peer);
    client.enable_server_hostname_verification(tls.verify_host);
    const auto proxy = request_options.proxy ? request_options.proxy : client_options.proxy;
    if (proxy && !proxy->host.empty() && proxy->port != 0) {
        client.set_proxy(proxy->host, static_cast<int>(proxy->port));
        if (!proxy->username.empty()) client.set_proxy_basic_auth(proxy->username, proxy->password);
    }
    // cpp-httplib owns its internal redirect loop.  A zero maximum is the
    // public API's explicit "do not follow" value; positive values enable
    // the backend's bounded redirect loop.
    client.set_follow_location(request_options.follow_redirects &&
                               request_options.maximum_redirects > 0);
}

std::string diagnostic_url(const Url &target) {
    const auto formatted = target.format();
    return formatted ? formatted.value() : std::string("<invalid-url>");
}

struct AttemptResult {
    Result<Response> result = Result<Response>::failure(
        make_error_code(NetworkErrc::connection_failed), "Request not executed", "http.request");
    std::error_code retry_error;
    std::optional<int> status;
};

AttemptResult execute_attempt(const Request &request, const ClientOptions &client_options,
                              const RequestOptions &options, std::chrono::steady_clock::time_point deadline) {
    AttemptResult output;
        const auto formatted = request.target.format();
    if (!formatted) {
        output.result = Result<Response>::failure(formatted.error().with_context("http.request"));
        output.retry_error = formatted.error().code;
        return output;
    }
    const auto path = request_target(request.target);
    if (path.empty()) {
        output.result = failure<Response>(NetworkErrc::invalid_url, "Unable to build HTTP request target", "http.request");
        output.retry_error = make_error_code(NetworkErrc::invalid_url);
        return output;
    }
    Headers merged_headers = client_options.default_headers;
    merged_headers.insert(merged_headers.end(), options.headers.begin(), options.headers.end());
    auto headers = convert_headers(merged_headers);
    if (!headers) {
        output.result = Result<Response>::failure(headers.error());
        output.retry_error = headers.error().code;
        return output;
    }
    for (const auto &header : request.headers) {
        if (contains_forbidden_header_character(header.name) || contains_forbidden_header_character(header.value) || header.name.empty()) {
            output.result = failure<Response>(NetworkErrc::invalid_request, "Invalid request header", "http.request");
            output.retry_error = make_error_code(NetworkErrc::invalid_request);
            return output;
        }
        headers.value().emplace(header.name, header.value);
    }

    const auto tls = effective_tls(client_options, options);
    std::unique_ptr<httplib::Client> client;
    if (!tls.client_certificate.empty() || !tls.client_key.empty()) {
        client = std::make_unique<httplib::Client>(formatted.value(), tls.client_certificate.u8string(), tls.client_key.u8string());
    } else {
        client = std::make_unique<httplib::Client>(formatted.value());
    }
    if (!client->is_valid()) {
        output.result = failure<Response>(NetworkErrc::invalid_url, "Invalid HTTP client URL", "http.request");
        output.retry_error = make_error_code(NetworkErrc::invalid_url);
        return output;
    }
    configure_client(*client, client_options, options);
    const auto total = remaining_time(deadline, options.timeout.total);
    if (total.count() <= 0 && deadline != std::chrono::steady_clock::time_point::max()) {
        output.result = failure<Response>(NetworkErrc::deadline_exceeded, "HTTP request deadline exceeded", "http.request");
        output.retry_error = output.result.error().code;
        return output;
    }
    set_client_timeouts(*client, options.timeout, total);

    Response response;
    bool callback_failed = false;
    bool callback_cancelled = false;
    std::uint64_t received = 0;
    const auto response_handler = [&](const httplib::Response &backend_response) {
        response.status = backend_response.status;
        for (const auto &header : backend_response.headers) response.headers.push_back({header.first, header.second});
        if (backend_response.has_header("Content-Length"))
            response.content_length = static_cast<std::uint64_t>(backend_response.get_header_value_u64("Content-Length"));
        return true;
    };
    const auto content_receiver = [&](const char *data, std::size_t length) {
        if (options.token.is_cancelled()) {
            callback_cancelled = true;
            return false;
        }
        received += static_cast<std::uint64_t>(length);
        if (!options.on_body) {
            if (received > options.maximum_response_bytes) {
                callback_failed = true;
                return false;
            }
            response.body.append(data, length);
        } else {
#if !defined(SINDRE_NO_EXCEPTIONS)
            try {
#endif
                if (!options.on_body(data, length)) {
                    callback_failed = true;
                    return false;
                }
#if !defined(SINDRE_NO_EXCEPTIONS)
            } catch (...) {
                callback_failed = true;
                return false;
            }
#endif
        }
        return true;
    };
    const auto content_provider = [&](std::size_t offset, std::size_t length, httplib::DataSink &sink) {
        if (!request.body_provider) return false;
        if (options.token.is_cancelled()) {
            callback_cancelled = true;
            return false;
        }
#if !defined(SINDRE_NO_EXCEPTIONS)
        try {
#endif
            auto chunk = request.body_provider(static_cast<std::uint64_t>(offset), length);
            if (!chunk) {
                callback_failed = true;
                return false;
            }
            if (!chunk.value().empty() && !sink.write(chunk.value().data(), chunk.value().size())) {
                callback_failed = true;
                return false;
            }
            return true;
#if !defined(SINDRE_NO_EXCEPTIONS)
        } catch (...) {
            callback_failed = true;
            return false;
        }
#endif
    };
    const auto progress = [&](std::size_t current, std::size_t total_length) {
        if (!options.on_progress) return true;
#if !defined(SINDRE_NO_EXCEPTIONS)
        try {
#endif
            options.on_progress(static_cast<std::uint64_t>(current), static_cast<std::uint64_t>(total_length));
            return !options.token.is_cancelled();
#if !defined(SINDRE_NO_EXCEPTIONS)
        } catch (...) {
            callback_failed = true;
            return false;
        }
#endif
    };

    std::atomic<bool> finished{false};
    std::thread monitor;
    if (options.timeout.total.count() > 0 || options.token.is_cancelled()) {
        monitor = std::thread([&] {
            while (!finished.load(std::memory_order_acquire)) {
                if (options.token.is_cancelled() ||
                    (deadline != std::chrono::steady_clock::time_point::max() && std::chrono::steady_clock::now() >= deadline)) {
                    client->stop();
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
    }

    httplib::Result backend_result;
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        switch (request.method) {
        case Method::get:
            backend_result = client->Get(path, response_handler, content_receiver, progress);
            break;
        case Method::post: {
            const auto content_type = request_content_type(request);
            if (request.body_provider) backend_result = client->Post(path, headers.value(),
                static_cast<std::size_t>(request.body_size), content_provider, content_type, progress);
            else backend_result = client->Post(path, headers.value(), request.body, content_type,
                                               content_receiver, progress);
            break;
        }
        case Method::put: {
            const auto content_type = request_content_type(request);
            if (request.body_provider) backend_result = client->Put(path, headers.value(),
                static_cast<std::size_t>(request.body_size), content_provider, content_type, progress);
            else backend_result = client->Put(path, headers.value(), request.body, content_type,
                                               content_receiver, progress);
            break;
        }
        case Method::patch: {
            const auto content_type = request_content_type(request);
            if (request.body_provider) backend_result = client->Patch(path, headers.value(),
                static_cast<std::size_t>(request.body_size), content_provider, content_type, progress);
            else backend_result = client->Patch(path, headers.value(), request.body, content_type,
                                                content_receiver, progress);
            break;
        }
        case Method::delete_:
            if (!request.body.empty()) backend_result = client->Delete(path, headers.value(), request.body,
                request_content_type(request), progress);
            else backend_result = client->Delete(path, headers.value(), progress);
            break;
        case Method::head:
            backend_result = client->Head(path, headers.value());
            break;
        case Method::options:
            backend_result = client->Options(path, headers.value());
            break;
        }
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        finished.store(true, std::memory_order_release);
        if (monitor.joinable()) monitor.join();
        output.result = failure<Response>(NetworkErrc::connection_failed, error.what(), "http.request");
        output.retry_error = output.result.error().code;
        return output;
    } catch (...) {
        finished.store(true, std::memory_order_release);
        if (monitor.joinable()) monitor.join();
        output.result = failure<Response>(NetworkErrc::connection_failed, "Unknown HTTP backend failure", "http.request");
        output.retry_error = output.result.error().code;
        return output;
    }
#endif
    finished.store(true, std::memory_order_release);
    if (monitor.joinable()) monitor.join();

    if (options.token.is_cancelled() || callback_cancelled) {
        output.result = failure<Response>(NetworkErrc::operation_cancelled, "HTTP request cancelled", "http.request");
        output.retry_error = output.result.error().code;
        return output;
    }
    if (deadline != std::chrono::steady_clock::time_point::max() && std::chrono::steady_clock::now() >= deadline) {
        output.result = failure<Response>(NetworkErrc::deadline_exceeded, "HTTP request deadline exceeded", "http.request");
        output.retry_error = output.result.error().code;
        return output;
    }
    if (callback_failed) {
        if (received > options.maximum_response_bytes && !options.on_body)
            output.result = failure<Response>(NetworkErrc::response_too_large, "HTTP response exceeds the configured memory limit", "http.request");
        else output.result = failure<Response>(NetworkErrc::callback_failed, "HTTP callback failed", "http.request");
        output.retry_error = output.result.error().code;
        return output;
    }
    if (!backend_result) {
        const auto code = map_backend_error(backend_result.error());
        output.result = Result<Response>::failure(code, safe_error_text(backend_result.error()), "http.request");
        output.retry_error = code;
        return output;
    }
    if (response.status == 0) {
        response.status = backend_result->status;
        for (const auto &header : backend_result->headers) response.headers.push_back({header.first, header.second});
        if (backend_result->has_header("Content-Length"))
            response.content_length = static_cast<std::uint64_t>(backend_result->get_header_value_u64("Content-Length"));
        // POST/PUT/PATCH 的 cpp-httplib 重载没有 response_handler，响应内容
        // 已经由 content_receiver 写入 response.body。只有接收器没有收到
        // 内容时，才回退到后端保留的完整 body，避免把已收集的数据清空。
        if (!options.on_body && response.body.empty()) response.body = backend_result->body;
    }
    output.status = response.status;
    output.result = Result<Response>::success(std::move(response));
    return output;
}

bool wait_before_retry(const RetryPolicy &policy, std::size_t attempt,
                       CancellationToken token, std::chrono::steady_clock::time_point deadline) {
    if (policy.initial_delay.count() <= 0) return !token.is_cancelled();
    auto delay = policy.initial_delay;
    for (std::size_t i = 1; i < attempt; ++i) {
        if (delay > std::chrono::milliseconds::max() / 2) break;
        delay *= 2;
    }
    if (policy.maximum_delay.count() > 0 && delay > policy.maximum_delay) delay = policy.maximum_delay;
    if (policy.jitter > 0.0) {
        std::mt19937 generator(static_cast<std::uint32_t>(attempt * 2654435761u));
        std::uniform_real_distribution<double> distribution(0.0, policy.jitter);
        delay += std::chrono::milliseconds(static_cast<std::int64_t>(delay.count() * distribution(generator)));
    }
    const auto slice = std::chrono::milliseconds(5);
    while (delay.count() > 0) {
        if (token.is_cancelled()) return false;
        if (deadline != std::chrono::steady_clock::time_point::max() && std::chrono::steady_clock::now() >= deadline) return false;
        const auto current = delay < slice ? delay : slice;
        std::this_thread::sleep_for(current);
        delay -= current;
    }
    return !token.is_cancelled();
}

} // namespace

struct Client::Impl {
    explicit Impl(ClientOptions value) : options(std::move(value)) {}
    ClientOptions options;
};

Client::Client(ClientOptions options) : impl_(std::make_unique<Impl>(std::move(options))) {}
Client::~Client() = default;
Client::Client(Client &&) noexcept = default;
Client &Client::operator=(Client &&) noexcept = default;

Result<Response> Client::request(const Request &request, const RequestOptions &options) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        const auto target_text = diagnostic_url(request.target);
        const auto request_context = [&](std::string operation, std::size_t attempt) {
            return std::move(operation) + ".url=" + target_text + ".attempt=" + std::to_string(attempt);
        };
        if (!impl_) return failure<Response>(NetworkErrc::invalid_request, "HTTP client is moved from", request_context("http.request", 0));
        if (request.target.get_scheme() != "http" && request.target.get_scheme() != "https")
            return failure<Response>(NetworkErrc::unsupported_scheme, "Only http and https are supported", request_context("http.request", 0));
        if (options.retry.max_attempts == 0 || options.timeout.connect.count() <= 0 || options.timeout.read.count() <= 0 ||
            options.timeout.write.count() <= 0 || options.timeout.total.count() <= 0)
            return failure<Response>(NetworkErrc::invalid_request, "Invalid HTTP timeout or retry options", request_context("http.request", 0));
        const auto started = std::chrono::steady_clock::now();
        const auto deadline = started + options.timeout.total;
        const bool allow_non_idempotent_retry = static_cast<bool>(options.retry.should_retry);
        for (std::size_t attempt = 1; attempt <= options.retry.max_attempts; ++attempt) {
            if (options.token.is_cancelled()) return failure<Response>(NetworkErrc::operation_cancelled, "HTTP request cancelled", request_context("http.request", attempt));
            if (std::chrono::steady_clock::now() >= deadline) return failure<Response>(NetworkErrc::deadline_exceeded, "HTTP request deadline exceeded", request_context("http.request", attempt));
            auto attempt_result = execute_attempt(request, impl_->options, options, deadline);
            if (attempt_result.result) {
                auto response = std::move(attempt_result.result.value());
                const RetryContext context{attempt, response.status, {}};
                bool retry = response.status >= 500 && attempt < options.retry.max_attempts &&
                             (is_idempotent(request.method) || allow_non_idempotent_retry);
                if (options.retry.should_retry) {
#if !defined(SINDRE_NO_EXCEPTIONS)
                    try {
#endif
                        retry = retry && options.retry.should_retry(context);
#if !defined(SINDRE_NO_EXCEPTIONS)
                    } catch (...) {
                        return failure<Response>(NetworkErrc::callback_failed, "Retry callback failed", request_context("http.request", attempt));
                    }
#endif
                }
                if (!retry || !wait_before_retry(options.retry, attempt, options.token, deadline)) return Result<Response>::success(std::move(response));
                continue;
            }
            const auto error = attempt_result.result.error();
            const RetryContext context{attempt, std::nullopt, error.code};
            bool retry = attempt < options.retry.max_attempts &&
                         (is_idempotent(request.method) || allow_non_idempotent_retry);
            if (options.retry.should_retry) {
#if !defined(SINDRE_NO_EXCEPTIONS)
                try {
#endif
                    retry = retry && options.retry.should_retry(context);
#if !defined(SINDRE_NO_EXCEPTIONS)
                } catch (...) {
                    return failure<Response>(NetworkErrc::callback_failed, "Retry callback failed", "http.request");
                }
#endif
            }
            if (!retry) return Result<Response>::failure(error.with_context("url=" + target_text + ".attempt=" + std::to_string(attempt)));
            if (!wait_before_retry(options.retry, attempt, options.token, deadline)) {
                if (options.token.is_cancelled()) return failure<Response>(NetworkErrc::operation_cancelled, "HTTP request cancelled during retry delay", request_context("http.request", attempt));
                return failure<Response>(NetworkErrc::deadline_exceeded, "HTTP request deadline exceeded during retry delay", request_context("http.request", attempt));
            }
            if (attempt + 1 > options.retry.max_attempts) break;
        }
        return failure<Response>(NetworkErrc::retry_exhausted, "HTTP retry attempts exhausted", request_context("http.request", options.retry.max_attempts));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return failure<Response>(NetworkErrc::connection_failed, error.what(), "http.request");
    } catch (...) {
        return failure<Response>(NetworkErrc::connection_failed, "Unknown HTTP request failure", "http.request");
    }
#endif
}

Result<std::future<Result<Response>>> Client::request_async(Request request, RequestOptions options) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        if (!impl_) return failure<std::future<Result<Response>>>(NetworkErrc::invalid_request, "HTTP client is moved from", "http.request_async");
        const auto client_options = impl_->options;
        auto future = std::async(std::launch::async,
            [request = std::move(request), options = std::move(options), client_options]() mutable -> Result<Response> {
                Client client(client_options);
                return client.request(request, options);
            });
        return Result<std::future<Result<Response>>>::success(std::move(future));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return failure<std::future<Result<Response>>>(NetworkErrc::connection_failed, error.what(), "http.request_async");
    } catch (...) {
        return failure<std::future<Result<Response>>>(NetworkErrc::connection_failed, "Unable to start async HTTP request", "http.request_async");
    }
#endif
}

bool Response::is_success() const noexcept { return status >= 200 && status < 300; }
bool Response::is_redirect() const noexcept { return status >= 300 && status < 400; }
Result<void> Response::require_success() const noexcept {
    if (is_success()) return Result<void>::success();
    return Result<void>::failure(make_error_code(NetworkErrc::http_status),
        "HTTP response status is not successful: " + std::to_string(status), "http.response");
}

Result<Response> request(const Request &request, RequestOptions options) noexcept {
    return Client().request(request, options);
}

Result<Response> get(const Url &target, RequestOptions options) noexcept {
    Request request;
    request.method = Method::get;
    request.target = target;
    return Client().request(request, options);
}

Result<Response> get(std::string_view target, RequestOptions options) noexcept {
    const auto parsed = Url::parse(target);
    if (!parsed) return Result<Response>::failure(parsed.error().with_context("network.get"));
    return get(parsed.value(), std::move(options));
}

Result<Response> get(const Url &target,
                     std::chrono::milliseconds timeout,
                     CancellationToken token,
                     Headers headers) noexcept {
    RequestOptions options;
    options.timeout.total = timeout;
    options.token = std::move(token);
    options.headers = std::move(headers);
    return get(target, std::move(options));
}

Result<Response> get(std::string_view target,
                     std::chrono::milliseconds timeout,
                     CancellationToken token,
                     Headers headers) noexcept {
    const auto parsed = Url::parse(target);
    if (!parsed) return Result<Response>::failure(parsed.error().with_context("network.get"));
    return get(parsed.value(), timeout, std::move(token), std::move(headers));
}

Result<Response> post(const Url &target, std::string body, std::string content_type, RequestOptions options) noexcept {
    Request request;
    request.method = Method::post;
    request.target = target;
    request.body = std::move(body);
    request.headers.push_back({"Content-Type", std::move(content_type)});
    return Client().request(request, options);
}

Result<Response> post(std::string_view target,
                      std::string body,
                      std::string content_type,
                      RequestOptions options) noexcept {
    const auto parsed = Url::parse(target);
    if (!parsed) return Result<Response>::failure(parsed.error().with_context("network.post"));
    return post(parsed.value(), std::move(body), std::move(content_type), std::move(options));
}

Result<Response> post(const Url &target,
                      std::string body,
                      std::string content_type,
                      std::chrono::milliseconds timeout,
                      CancellationToken token,
                      Headers headers) noexcept {
    RequestOptions options;
    options.timeout.total = timeout;
    options.token = std::move(token);
    options.headers = std::move(headers);
    return post(target, std::move(body), std::move(content_type), std::move(options));
}

Result<Response> post(std::string_view target,
                      std::string body,
                      std::string content_type,
                      std::chrono::milliseconds timeout,
                      CancellationToken token,
                      Headers headers) noexcept {
    const auto parsed = Url::parse(target);
    if (!parsed) return Result<Response>::failure(parsed.error().with_context("network.post"));
    return post(parsed.value(), std::move(body), std::move(content_type), timeout,
                std::move(token), std::move(headers));
}

#if defined(SINDRE_WITH_JSON)
Result<json::Value> request_json(const Request &request, RequestOptions options) noexcept {
    auto response = Client().request(request, options);
    if (!response) return Result<json::Value>::failure(response.error().with_context("http.json"));
    const auto status = response.value().require_success();
    if (!status) return Result<json::Value>::failure(status.error().with_context("http.json"));
    auto value = json::parse(response.value().body);
    if (!value) return Result<json::Value>::failure(value.error().with_context("http.json"));
    return value;
}

Result<json::Value> get_json(const Url &target, RequestOptions options) noexcept {
    Request request;
    request.method = Method::get;
    request.target = target;
    return request_json(request, std::move(options));
}

Result<Response> post_json(const Url &target,
                           const json::Object &fields,
                           RequestOptions options) noexcept {
    auto body = json::Value(fields).to_json();
    if (!body) return Result<Response>::failure(body.error().with_context("network.post_json"));
    return post(target, std::move(body.value()), "application/json", std::move(options));
}

Result<Response> post_json(std::string_view target,
                           const json::Object &fields,
                           RequestOptions options) noexcept {
    auto body = json::Value(fields).to_json();
    if (!body) return Result<Response>::failure(body.error().with_context("network.post_json"));
    return post(target, std::move(body.value()), "application/json", std::move(options));
}
#endif

} // namespace sindre::general::network

#else

namespace sindre::general::network {
namespace {
template <class T>
Result<T> unsupported() {
    return Result<T>::failure(make_error_code(NetworkErrc::unsupported_scheme),
                              "HTTP support is not enabled", "http");
}
}
struct Client::Impl {};
Client::Client(ClientOptions) : impl_(std::make_unique<Impl>()) {}
Client::~Client() = default;
Client::Client(Client &&) noexcept = default;
Client &Client::operator=(Client &&) noexcept = default;
Result<Response> Client::request(const Request &, const RequestOptions &) noexcept { return unsupported<Response>(); }
Result<std::future<Result<Response>>> Client::request_async(Request, RequestOptions) noexcept { return unsupported<std::future<Result<Response>>>(); }
bool Response::is_success() const noexcept { return status >= 200 && status < 300; }
bool Response::is_redirect() const noexcept { return status >= 300 && status < 400; }
Result<void> Response::require_success() const noexcept { return is_success() ? Result<void>::success() : Result<void>::failure(make_error_code(NetworkErrc::http_status), "HTTP status is not successful", "http.response"); }
Result<Response> request(const Request &, RequestOptions) noexcept { return unsupported<Response>(); }
Result<Response> get(const Url &, RequestOptions) noexcept { return unsupported<Response>(); }
Result<Response> get(std::string_view, RequestOptions) noexcept { return unsupported<Response>(); }
Result<Response> get(const Url &, std::chrono::milliseconds, CancellationToken, Headers) noexcept { return unsupported<Response>(); }
Result<Response> get(std::string_view, std::chrono::milliseconds, CancellationToken, Headers) noexcept { return unsupported<Response>(); }
Result<Response> post(const Url &, std::string, std::string, RequestOptions) noexcept { return unsupported<Response>(); }
Result<Response> post(std::string_view, std::string, std::string, RequestOptions) noexcept { return unsupported<Response>(); }
Result<Response> post(const Url &, std::string, std::string, std::chrono::milliseconds, CancellationToken, Headers) noexcept { return unsupported<Response>(); }
Result<Response> post(std::string_view, std::string, std::string, std::chrono::milliseconds, CancellationToken, Headers) noexcept { return unsupported<Response>(); }
#if defined(SINDRE_WITH_JSON)
Result<Response> post_json(const Url &, const json::Object &, RequestOptions) noexcept { return unsupported<Response>(); }
Result<Response> post_json(std::string_view, const json::Object &, RequestOptions) noexcept { return unsupported<Response>(); }
#endif
}

#endif
