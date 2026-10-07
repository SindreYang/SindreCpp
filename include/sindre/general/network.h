#pragma once

#include <sindre/general/core.h>
#include <sindre/general/runtime.h>

#if defined(SINDRE_WITH_JSON)
#include <sindre/general/system.h>
#endif

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace sindre::general::network {

enum class NetworkErrc {
    invalid_url = 1,
    invalid_request,
    unsupported_scheme,
    dns_failure,
    connection_failed,
    connection_timeout,
    read_timeout,
    write_timeout,
    tls_failed,
    deadline_exceeded,
    operation_cancelled,
    response_too_large,
    callback_failed,
    retry_exhausted,
    file_io_failed,
    http_status,
};

const std::error_category &network_category() noexcept;
std::error_code make_error_code(NetworkErrc code) noexcept;

using Query = std::vector<std::pair<std::string, std::string>>;

class Url {
public:
    static Result<Url> parse(std::string_view value) noexcept;

    Result<std::string> format() const noexcept;
    Result<void> set_query(const Query &query) noexcept;

    std::string get_scheme() const;
    std::string get_host() const;
    std::uint16_t get_port() const;
    std::string get_path() const;
    Query get_query() const;

private:
    std::string scheme_;
    std::string host_;
    std::uint16_t port_ = 0;
    bool explicit_port_ = false;
    std::string path_ = "/";
    Query query_;
};

Result<Url> parse(std::string_view value) noexcept;
Result<std::string> encode(std::string_view value) noexcept;
Result<std::string> decode(std::string_view value) noexcept;
Result<Query> parse_query(std::string_view value) noexcept;
Result<std::string> build_query(const Query &query) noexcept;

struct Header {
    std::string name;
    std::string value;
};
using Headers = std::vector<Header>;

enum class Method { get, post, put, patch, delete_, head, options };

struct TimeoutOptions {
    std::chrono::milliseconds connect{5000};
    std::chrono::milliseconds read{30000};
    std::chrono::milliseconds write{30000};
    std::chrono::milliseconds total{60000};
};

struct TlsOptions {
    bool verify_peer = true;
    bool verify_host = true;
    std::filesystem::path ca_file;
    std::filesystem::path ca_directory;
    std::filesystem::path client_certificate;
    std::filesystem::path client_key;
};

struct ProxyOptions {
    std::string host;
    std::uint16_t port = 0;
    std::string username;
    std::string password;
};

struct RetryContext {
    std::size_t attempt = 1;
    std::optional<int> status;
    std::error_code error;
};

struct RetryPolicy {
    std::size_t max_attempts = 1;
    std::chrono::milliseconds initial_delay{0};
    std::chrono::milliseconds maximum_delay{0};
    double jitter = 0.0;
    std::function<bool(const RetryContext &)> should_retry;
};

struct RequestOptions {
    TimeoutOptions timeout;
    TlsOptions tls;
    std::optional<ProxyOptions> proxy;
    RetryPolicy retry;
    CancellationToken token;

    Headers headers;
    std::size_t maximum_response_bytes = 16 * 1024 * 1024;
    bool follow_redirects = false;
    std::size_t maximum_redirects = 0;

    std::function<bool(const char *, std::size_t)> on_body;
    std::function<void(std::uint64_t current, std::uint64_t total)> on_progress;
};

struct ClientOptions {
    TlsOptions tls;
    std::optional<ProxyOptions> proxy;
    Headers default_headers;
};

struct Request {
    using BodyProvider = std::function<Result<std::string>(std::uint64_t offset, std::size_t length)>;

    Method method = Method::get;
    Url target;
    Headers headers;
    std::string body;
    std::uint64_t body_size = 0;
    BodyProvider body_provider;
};

struct Response {
    int status = 0;
    Headers headers;
    std::optional<std::uint64_t> content_length;
    std::string body;

    bool is_success() const noexcept;
    bool is_redirect() const noexcept;
    Result<void> require_success() const noexcept;
};

class Client {
public:
    explicit Client(ClientOptions options = {});
    ~Client();

    Client(Client &&) noexcept;
    Client &operator=(Client &&) noexcept;
    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    Result<Response> request(const Request &request, const RequestOptions &options = {}) noexcept;
    Result<std::future<Result<Response>>> request_async(
        Request request, RequestOptions options = {}) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

Result<Response> request(const Request &request, RequestOptions options = {}) noexcept;
Result<Response> get(const Url &target, RequestOptions options = {}) noexcept;
Result<Response> get(std::string_view target, RequestOptions options = {}) noexcept;
Result<Response> get(const Url &target,
                     std::chrono::milliseconds timeout,
                     CancellationToken token = {},
                     Headers headers = {}) noexcept;
Result<Response> get(std::string_view target,
                     std::chrono::milliseconds timeout,
                     CancellationToken token = {},
                     Headers headers = {}) noexcept;
Result<Response> post(const Url &target,
                      std::string body,
                      std::string content_type = "application/json",
                      RequestOptions options = {}) noexcept;
Result<Response> post(std::string_view target,
                      std::string body,
                      std::string content_type = "application/json",
                      RequestOptions options = {}) noexcept;
Result<Response> post(const Url &target,
                      std::string body,
                      std::string content_type,
                      std::chrono::milliseconds timeout,
                      CancellationToken token = {},
                      Headers headers = {}) noexcept;
Result<Response> post(std::string_view target,
                      std::string body,
                      std::string content_type,
                      std::chrono::milliseconds timeout,
                      CancellationToken token = {},
                      Headers headers = {}) noexcept;

#if defined(SINDRE_WITH_JSON)
Result<json::Value> get_json(const Url &target, RequestOptions options = {}) noexcept;
Result<json::Value> request_json(const Request &request, RequestOptions options = {}) noexcept;
Result<Response> post_json(const Url &target,
                           const json::Object &fields,
                           RequestOptions options = {}) noexcept;
Result<Response> post_json(std::string_view target,
                           const json::Object &fields,
                           RequestOptions options = {}) noexcept;
#endif

using Progress = std::function<void(std::uint64_t current, std::uint64_t total)>;

struct TransferOptions {
    RequestOptions request;
    Progress progress;
};

Result<std::uint64_t> download(const Url &target,
                               const std::filesystem::path &destination,
                               TransferOptions options = {}) noexcept;
Result<std::uint64_t> download(const Url &target,
                               const std::filesystem::path &destination,
                               Progress progress,
                               std::chrono::milliseconds timeout,
                               CancellationToken token = {}) noexcept;

Result<int> upload(const Url &target,
                   const std::filesystem::path &source,
                   std::string content_type = "application/octet-stream",
                   TransferOptions options = {}) noexcept;
Result<int> upload(const Url &target,
                   const std::filesystem::path &source,
                   std::string content_type,
                   Progress progress,
                   std::chrono::milliseconds timeout,
                   CancellationToken token = {}) noexcept;

Result<std::future<Result<std::uint64_t>>> download_async(
    Url target, std::filesystem::path destination, TransferOptions options = {}) noexcept;
Result<std::future<Result<int>>> upload_async(
    Url target,
    std::filesystem::path source,
    std::string content_type = "application/octet-stream",
    TransferOptions options = {}) noexcept;

} // namespace sindre::general::network

namespace std {
template <>
struct is_error_code_enum<sindre::general::network::NetworkErrc> : true_type {};
} // namespace std
