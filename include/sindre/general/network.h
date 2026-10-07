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
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace sindre::general::url {
std::string encode_component(std::string_view input);
Result<std::string> decode_component(std::string_view input);
using Query = std::vector<std::pair<std::string, std::string>>;
Result<Query> parse_query(std::string_view input);
std::string build_query(const Query &query);
}
#if defined(SINDRE_WITH_HTTP)
#include <httplib.h>
namespace sindre::general::http {
using Client = httplib::Client; using Server = httplib::Server;
using Request = httplib::Request; using Response = httplib::Response;
using Result = httplib::Result; namespace native = httplib;
struct RequestOptions {
    int connect_timeout_seconds = 5; int read_timeout_seconds = 5; int write_timeout_seconds = 5;
    int retries = 2; std::chrono::milliseconds retry_delay{50};
    std::chrono::milliseconds total_timeout{0}; ::sindre::general::CancellationToken token{};
};
struct ResponseData { int status = 0; std::string body; std::string content_type; };
::sindre::general::Result<ResponseData> get(std::string host, int port, std::string path = "/", RequestOptions options = {});
::sindre::general::Result<ResponseData> post(std::string host, int port, std::string path, std::string body,
 std::string content_type = "application/octet-stream", RequestOptions options = {});
#if defined(SINDRE_WITH_JSON)
::sindre::general::Result<::sindre::general::json::Document> get_json(std::string host, int port, std::string path = "/", RequestOptions options = {});
#endif
}
namespace sindre::general::transfer {
using Progress = std::function<void(std::uint64_t current, std::uint64_t total)>;
Result<std::uint64_t> download(std::string host, int port, std::string remote_path,
 const std::filesystem::path &destination, http::RequestOptions options = {}, Progress progress = {}) noexcept;
Result<int> upload(std::string host, int port, std::string remote_path,
 const std::filesystem::path &source, std::string content_type = "application/octet-stream",
 http::RequestOptions options = {}, Progress progress = {}) noexcept;
}
#endif
