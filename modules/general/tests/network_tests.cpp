#include <sindre/general/network.h>

#include <httplib.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "Check failed at " << __FILE__ << ':' << __LINE__ << ": " #condition << '\n'; \
    return EXIT_FAILURE; \
} } while (false)

int main() {
    using namespace sindre::general;
    using namespace sindre::general::network;

    const auto encoded = encode("中文 a+b&c");
    CHECK(encoded && encoded.value() == "%E4%B8%AD%E6%96%87%20a%2Bb%26c");
    const auto decoded = decode(encoded.value());
    CHECK(decoded && decoded.value() == "中文 a+b&c");
    const auto parsed = Url::parse("https://[::1]:8443/api?q=a%2Bb&empty=");
    CHECK(parsed && parsed.value().get_scheme() == "https" && parsed.value().get_host() == "::1" &&
          parsed.value().get_port() == 8443 && parsed.value().get_query().size() == 2);
    CHECK(!Url::parse("https://user:password@example.com/") &&
          !Url::parse("https://example.com/#fragment"));

    httplib::Server server;
    std::atomic<int> retry_count{0};
    server.Get("/ok", [](const auto &, auto &response) {
        response.set_content("你好，network", "text/plain; charset=UTF-8");
    });
    server.Get("/retry", [&](const auto &, auto &response) {
        if (++retry_count == 1) response.status = 503;
        else response.set_content("retried", "text/plain");
    });
    server.Get("/json", [](const auto &, auto &response) {
        response.set_content(R"({"ok":true})", "application/json");
    });
    server.Post("/json-post", [](const auto &request, auto &response) {
        response.set_content(request.body, "application/json");
    });
    server.Get("/download", [](const auto &, auto &response) {
        response.set_content("download-payload", "application/octet-stream");
    });
    server.Post("/upload", [](const auto &request, auto &response) {
        response.set_content(request.body, "text/plain");
        response.status = 201;
    });
    const auto port = server.bind_to_any_port("127.0.0.1");
    CHECK(port > 0);
    std::thread server_thread([&] { server.listen_after_bind(); });
    struct ServerShutdown {
        httplib::Server &server;
        std::thread &thread;
        ~ServerShutdown() {
            server.stop();
            if (thread.joinable()) thread.join();
        }
    } server_shutdown{server, server_thread};

    const auto make_url = [&](std::string path) {
        return Url::parse("http://127.0.0.1:" + std::to_string(port) + std::move(path));
    };
    const auto ok_target = make_url("/ok");
    CHECK(ok_target);
    auto simple_response = get("http://127.0.0.1:" + std::to_string(port) + "/ok");
    CHECK(simple_response && simple_response.value().is_success() &&
          simple_response.value().body == "你好，network");
    Request request;
    request.target = ok_target.value();
    auto response = Client().request(request);
    CHECK(response && response.value().is_success() && response.value().body == "你好，network");

    RequestOptions retry_options;
    retry_options.retry.max_attempts = 2;
    retry_options.retry.initial_delay = std::chrono::milliseconds(1);
    retry_options.retry.should_retry = [](const RetryContext &context) {
        return context.status && *context.status == 503;
    };
    const auto retry_target = make_url("/retry");
    CHECK(retry_target);
    auto retried = get(retry_target.value(), retry_options);
    CHECK(retried && retried.value().status == 200 && retry_count == 2);

    const auto json_target = make_url("/json");
    CHECK(json_target);
    auto json = get_json(json_target.value());
    CHECK(json && json.value().find("ok") && json.value().find("ok")->get_bool().value());

    auto posted_json = post_json(
        "http://127.0.0.1:" + std::to_string(port) + "/json-post",
        {{"name", "sindre"}, {"age", 18}, {"enabled", true}});
    CHECK(posted_json);
    const auto posted_value = json::parse(posted_json.value().body);
    CHECK(posted_value);
    const auto *posted_name = posted_value.value().find("name");
    const auto *posted_age = posted_value.value().find("age");
    const auto *posted_enabled = posted_value.value().find("enabled");
    CHECK(posted_name && posted_name->get_string() &&
          posted_name->get_string().value() == "sindre");
    CHECK(posted_age && posted_age->get_int() && posted_age->get_int().value() == 18);
    CHECK(posted_enabled && posted_enabled->get_bool() &&
          posted_enabled->get_bool().value());

    std::string streamed;
    RequestOptions stream_options;
    stream_options.on_body = [&](const char *data, std::size_t size) {
        streamed.append(data, size);
        return true;
    };
    auto streamed_response = get(ok_target.value(), stream_options);
    CHECK(streamed_response && streamed_response.value().body.empty() && streamed == "你好，network");

    auto async_response = Client().request_async(request);
    CHECK(async_response);
    auto async_value = async_response.value().get();
    CHECK(async_value && async_value.value().is_success());

    const auto temp_root = std::filesystem::temp_directory_path() / "sindre-network-tests";
    std::filesystem::create_directories(temp_root);
    const auto destination = temp_root / "中文目标.dat";
    {
        std::ofstream old(destination, std::ios::binary | std::ios::trunc);
        old << "old";
    }
    const auto download_target = make_url("/download");
    CHECK(download_target);
    auto downloaded = download(download_target.value(), destination);
    CHECK(downloaded && downloaded.value() == std::string("download-payload").size());
    std::ifstream downloaded_file(destination, std::ios::binary);
    const std::string downloaded_text((std::istreambuf_iterator<char>(downloaded_file)), {});
    CHECK(downloaded_text == "download-payload");

    const auto missing_destination = temp_root / "must-stay.dat";
    {
        std::ofstream old(missing_destination, std::ios::binary | std::ios::trunc);
        old << "keep";
    }
    const auto missing_target = make_url("/missing");
    CHECK(missing_target);
    auto missing = download(missing_target.value(), missing_destination);
    CHECK(!missing);
    std::ifstream kept_file(missing_destination, std::ios::binary);
    const std::string kept((std::istreambuf_iterator<char>(kept_file)), {});
    CHECK(kept == "keep");

    const auto source = temp_root / "upload.dat";
    {
        std::ofstream input(source, std::ios::binary | std::ios::trunc);
        input << "upload-payload";
    }
    std::uint64_t last_progress = 0;
    TransferOptions transfer_options;
    transfer_options.progress = [&](std::uint64_t current, std::uint64_t) {
        if (current < last_progress) std::abort();
        last_progress = current;
    };
    const auto upload_target = make_url("/upload");
    CHECK(upload_target);
    auto uploaded = upload(upload_target.value(), source, "text/plain", transfer_options);
    CHECK(uploaded && uploaded.value() == 201);

    std::error_code ignored;
    std::filesystem::remove_all(temp_root, ignored);
    return EXIT_SUCCESS;
}
