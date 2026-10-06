#include <general/index.hpp>
#if defined(SINDRECPP_WITH_HTTP)
#include <general/core/http.hpp>
#endif
#if defined(SINDRECPP_WITH_JSON)
#include <general/core/json.hpp>
#include <general/core/config.hpp>
#endif
#if defined(SINDRECPP_WITH_CLI)
#include <general/core/cli.hpp>
#endif
#if defined(SINDRECPP_WITH_LOG)
#include <general/core/log.hpp>
#endif

#include <cstdlib>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <thread>
#include <string>

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "Check failed at " << __FILE__ << ':' << __LINE__ << ": " #condition << '\n'; \
    return EXIT_FAILURE; \
} } while (false)

int main() {
    auto value = sindrecpp::general::Result<int>::success(42);
    CHECK(value);
    CHECK(value.value() == 42);

    auto failure = sindrecpp::general::Result<int>::failure({{}, "expected failure", {}});
    CHECK(!failure);
    CHECK(failure.error().message == "expected failure");
    auto detailed_failure = sindrecpp::general::Result<int>::failure(
        std::make_error_code(std::errc::invalid_argument), "bad value", "general.test");
    CHECK(detailed_failure.error().context == "general.test" &&
          detailed_failure.error().describe().find("general.test") != std::string::npos);

    auto no_value = sindrecpp::general::Result<void>::success();
    CHECK(no_value);
    auto no_value_error = sindrecpp::general::Result<void>::failure({{}, "failed", {}});
    CHECK(!no_value_error);
    CHECK(no_value_error.error().message == "failed");

    CHECK(sindrecpp::general::string::trim(" \t hello\r\n") == "hello");
    CHECK(sindrecpp::general::string::trim(" \t\r\n").empty());
    CHECK(sindrecpp::general::string::trim_copy(std::string("  临时字符串  ")) == "临时字符串");
    CHECK(sindrecpp::general::string::starts_with("SindreCpp", "Sindre"));
    CHECK(sindrecpp::general::string::ends_with("SindreCpp", "Cpp"));
    CHECK(sindrecpp::general::string::replace_all("a-b-a", "a", "x") == "x-b-x");
    const auto parts = sindrecpp::general::string::split("one,two,", ',');
    CHECK(parts.size() == 3 && parts[0] == "one" && parts[2].empty());
    const auto owned_parts = sindrecpp::general::string::split(std::string("临时,字符串,"), ',');
    CHECK(owned_parts.size() == 3 && owned_parts[0] == "临时" && owned_parts[2].empty());
    const std::string chinese = "中文字符";
    const char* chinese_chars = chinese.c_str();
    CHECK(std::string_view(chinese_chars) == chinese);
    CHECK(sindrecpp::general::string::trim("  " + chinese + "  ") == chinese);
    CHECK(sindrecpp::general::string::replace_all(chinese + "-" + chinese, chinese, "x") == "x-x");
    const std::string mixed = chinese + "-字符";
    const auto views = sindrecpp::general::string::split_view(mixed, '-');
    CHECK(views.size() == 2 && views[0] == chinese && views[1] == "字符");
    CHECK(sindrecpp::general::string::lower_ascii("AbC中文") == "abc中文");
    CHECK(sindrecpp::general::string::upper_ascii("aBc中文") == "ABC中文");
    CHECK(sindrecpp::general::string::valid_utf8(chinese));
    CHECK(!sindrecpp::general::string::valid_utf8(std::string("\xE4\xB8", 2)));
    const auto parsed_int = sindrecpp::general::string::parse_int("  -42 ");
    CHECK(parsed_int && parsed_int.value() == -42);
    const auto parsed_hex = sindrecpp::general::string::parse_int("ff", 16);
    CHECK(parsed_hex && parsed_hex.value() == 255);
    const auto parsed_float = sindrecpp::general::string::parse_float(" 3.125 ");
    CHECK(parsed_float && std::abs(parsed_float.value() - 3.125) < 1e-12);
    CHECK(!sindrecpp::general::string::parse_int("12x"));
    CHECK(!sindrecpp::general::string::parse_float("nan"));
    CHECK(sindrecpp::general::string::join({"中文", "SindreCpp"}, "/") == "中文/SindreCpp");
    CHECK(sindrecpp::general::string::concat("Sindre", "Cpp", 17) == "SindreCpp17");
    const sindrecpp::general::string::String python_text("  A,b,中文  ");
    CHECK(python_text.trim().lower().str() == "a,b,中文");
    CHECK(python_text.split(",").size() == 3);
    CHECK(!sindrecpp::general::string::normalize_utf8(std::string("\xE4", 1)));

    sindrecpp::general::CancellationSource source;
    auto async_value = sindrecpp::general::run_async([](sindrecpp::general::CancellationToken token) {
        for (int i = 0; i < 10; ++i) {
            if (token.cancelled()) return -1;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return 42;
    }, source.token());
    auto async_result = sindrecpp::general::wait_for(async_value, std::chrono::seconds(1));
    CHECK(async_result && async_result.value() == 42);
    auto slow = sindrecpp::general::run_async([](sindrecpp::general::CancellationToken token) {
        while (!token.cancelled()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return 7;
    }, source.token());
    auto timed_out = sindrecpp::general::wait_for(slow, std::chrono::milliseconds(1));
    CHECK(!timed_out && timed_out.error().code == std::make_error_code(std::errc::timed_out));
    source.cancel();
    const auto cancelled = slow.get();
    CHECK(!cancelled && cancelled.error().code == std::make_error_code(std::errc::operation_canceled));
    std::atomic<int> progress_events{0};
    auto progressed = sindrecpp::general::run_async(
        [](sindrecpp::general::CancellationToken, const std::function<void(double)> &progress) {
            progress(0.25);
            progress(1.0);
            return 5;
        }, {}, [&](double value) {
            if (value >= 0.0 && value <= 1.0) ++progress_events;
        });
    auto progressed_result = sindrecpp::general::wait_for(progressed, std::chrono::seconds(1));
    CHECK(progressed_result && progressed_result.value() == 5 && progress_events == 2);
    sindrecpp::general::ThreadPool pool(2);
    auto pooled = pool.submit([](sindrecpp::general::CancellationToken) { return 9; });
    CHECK(pooled);
    auto pooled_result = sindrecpp::general::wait_for(pooled.value(), std::chrono::seconds(1));
    CHECK(pooled_result && pooled_result.value() == 9);
    auto safely_launched = sindrecpp::general::try_run_async(
        [](sindrecpp::general::CancellationToken) { return 11; });
    CHECK(safely_launched);
    auto safely_launched_result = sindrecpp::general::wait_for(
        safely_launched.value(), std::chrono::seconds(1));
    CHECK(safely_launched_result && safely_launched_result.value() == 11);
    const auto uuid = sindrecpp::general::codec::uuid4();
    CHECK(uuid && uuid.value().size() == 36 && uuid.value()[14] == '4' &&
          (uuid.value()[19] == '8' || uuid.value()[19] == '9' || uuid.value()[19] == 'a' ||
           uuid.value()[19] == 'b'));
    CHECK(sindrecpp::general::codec::fnv1a64("中文") != sindrecpp::general::codec::fnv1a64("中文 "));
    const std::vector<std::uint8_t> bytes{0, 1, 2, 253, 254, 255};
    const auto encoded = sindrecpp::general::codec::base64_encode(bytes);
    CHECK(encoded && encoded.value() == "AAEC/f7/");
    const auto decoded = sindrecpp::general::codec::base64_decode(encoded.value());
    CHECK(decoded && decoded.value() == bytes);
    CHECK(!sindrecpp::general::codec::base64_decode("bad?"));
    const auto compressed = sindrecpp::general::codec::simple_compress(
        std::vector<std::uint8_t>{1, 1, 1, 2, 3, 3, 3, 3});
    CHECK(compressed && compressed.value().size() < 8 &&
          sindrecpp::general::codec::simple_decompress(compressed.value()).value() ==
              std::vector<std::uint8_t>({1, 1, 1, 2, 3, 3, 3, 3}));
    CHECK(!sindrecpp::general::codec::simple_decompress(std::vector<std::uint8_t>{1}));
    const auto unicode_path = sindrecpp::general::path::from_utf8(
        sindrecpp::general::path::to_utf8(std::filesystem::temp_directory_path() / L"sindrecpp-监控.txt"));
    std::filesystem::remove(unicode_path);
    std::promise<void> changed;
    auto changed_future = changed.get_future();
    sindrecpp::general::file_watch::Watcher watcher(unicode_path, std::chrono::milliseconds(10));
    CHECK(watcher.start([&](const auto &event) {
        if (event.exists) changed.set_value();
    }));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    { std::ofstream output(unicode_path); output << "中文"; }
    CHECK(changed_future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    watcher.stop();
    std::filesystem::remove(unicode_path);

    auto pointer = sindrecpp::general::pointer::make_unique<std::string>("owned");
    CHECK(*pointer == "owned");
    auto shared = sindrecpp::general::pointer::make_shared<std::string>(chinese);
    sindrecpp::general::pointer::weak_ptr<std::string> weak = shared;
    CHECK(!weak.expired() && *weak.lock() == chinese);
    shared.reset();
    CHECK(weak.expired());

    bool guard_called = false;
    { auto guard = sindrecpp::general::scope_guard([&] { guard_called = true; }); }
    CHECK(guard_called);
    const std::vector<int> numbers{1, 2, 3};
    CHECK(sindrecpp::general::ranges::contains(numbers, 2));
    const auto doubled = sindrecpp::general::ranges::transform(numbers, [](int value) { return value * 2; });
    CHECK(doubled == std::vector<int>({2, 4, 6}));
    CHECK(sindrecpp::general::ranges::for_each_result(numbers, [](int) {
        return sindrecpp::general::Result<void>::success();
    }));
    CHECK(sindrecpp::general::url::encode_component("中文 a+b") == "%E4%B8%AD%E6%96%87%20a%2Bb");
    const auto query = sindrecpp::general::url::parse_query("name=%E4%B8%AD%E6%96%87&x=1%2B2");
    CHECK(query && query.value().size() == 2 && query.value()[0].second == "中文" &&
          query.value()[1].second == "1+2" && sindrecpp::general::url::build_query(query.value()) ==
          "name=%E4%B8%AD%E6%96%87&x=1%2B2");
    CHECK(!sindrecpp::general::url::decode_component("%Q0"));
    CHECK(sindrecpp::general::system::set_environment("SINDRE_GENERAL_TEST", "中文"));
    CHECK(sindrecpp::general::system::environment("SINDRE_GENERAL_TEST").value() == "中文");
    const auto system_info = sindrecpp::general::system::information();
    CHECK(!system_info.os.empty() && !system_info.architecture.empty());
    const auto temporary = sindrecpp::general::temp::File::create("sindrecpp-test-", ".dat");
    CHECK(temporary && std::filesystem::is_regular_file(temporary.value().path()));
    const auto temporary_path = temporary.value().path();
#if defined(_WIN32)
    const std::filesystem::path missing_library = L"sindrecpp-does-not-exist.dll";
#else
    const std::filesystem::path missing_library = "/sindrecpp-does-not-exist.so";
#endif
    CHECK(!sindrecpp::general::dynamic_library::Library::open(missing_library));
    CHECK(sindrecpp::general::diagnostics::check(true, "ok"));
    sindrecpp::general::diagnostics::Stopwatch stopwatch;
    CHECK(stopwatch.elapsed().count() >= 0);
    sindrecpp::general::process::Options process_options;
    process_options.capture_output = true;
    const auto process_result = sindrecpp::general::process::run(
#if defined(_WIN32)
        "cmd /c echo SindreCpp"
#else
        "printf SindreCpp"
#endif
        , process_options);
    CHECK(process_result && process_result.value().exit_code == 0 &&
          process_result.value().stdout_text.find("SindreCpp") != std::string::npos);
    CHECK(!sindrecpp::general::crashpad::start({}).has_value());
    CHECK(std::filesystem::exists(temporary_path));
    // File is removed by the owning temporary-file guard at scope exit.
    const auto parsed_version = sindrecpp::general::versioning::parse("1.2.3-beta");
    CHECK(parsed_version && parsed_version.value().major == 1 &&
          sindrecpp::general::versioning::to_string(parsed_version.value()) == "1.2.3-beta");
    CHECK(!sindrecpp::general::versioning::parse("1.2"));
    const auto stable_version = sindrecpp::general::versioning::parse("1.2.3");
    const auto newer_version = sindrecpp::general::versioning::parse("1.2.4");
    CHECK(stable_version && newer_version && stable_version.value() < newer_version.value());
    CHECK(sindrecpp::general::startup::location("sindrecpp-test"));
    CHECK(!sindrecpp::general::desktop::notify("title", "body"));

#if defined(SINDRECPP_WITH_STRING)
    sindrecpp::general::string::Utf8String utf8_text("中文字符串");
    sindrecpp::general::string::Utf16String utf16_text(u"中文字符串");
    CHECK(utf8_text == "中文字符串");
    CHECK(utf16_text == u"中文字符串");
#endif

#if defined(SINDRECPP_WITH_JSON)
    const auto document = sindrecpp::general::json::try_parse(R"({"名字":"中文"})");
    CHECK(document);
    auto name = document.value().root()["名字"].get_string();
    CHECK(name.error() == simdjson::SUCCESS && name.value_unsafe() == std::string_view("中文"));
    const auto invalid_json = sindrecpp::general::json::try_parse(R"({"unterminated":)");
    CHECK(!invalid_json && invalid_json.error().context == "json.parse");
    auto config = sindrecpp::general::config::Config::from_json(
        R"({"server":{"host":"中文主机","port":8080},"ratio":1.25})");
    CHECK(config && config.value().get_string("server.host").value() == "中文主机" &&
          config.value().get_int("server.port").value() == 8080 &&
          std::abs(config.value().get_float_or("ratio", 0) - 1.25) < 1e-12);
#if defined(_WIN32)
    _putenv_s("SINDRE_TEST_SERVER_PORT", "9090");
#else
    setenv("SINDRE_TEST_SERVER_PORT", "9090", 1);
#endif
    config.value().apply_environment("SINDRE_TEST");
    CHECK(config.value().get_int("server.port").value() == 9090);
    const auto config_path = std::filesystem::temp_directory_path() / L"sindrecpp-中文-config.json";
    { std::ofstream output(config_path); output << R"({"server":{"port":8081}})"; }
    auto file_config = sindrecpp::general::config::Config::from_file(config_path);
    CHECK(file_config && file_config.value().get_int("server.port").value() == 8081);
    std::filesystem::remove(config_path);
#if defined(_WIN32)
    _putenv_s("SINDRE_TEST_SERVER_PORT", "");
#else
    unsetenv("SINDRE_TEST_SERVER_PORT");
#endif
#endif

#if defined(SINDRECPP_WITH_CLI)
#if !defined(SINDRECPP_NO_EXCEPTIONS)
    sindrecpp::general::cli::ArgumentParser parser("sindrecpp-test");
    parser.add_argument("--name").default_value(std::string("默认"));
    const char* cli_args[] = {"sindrecpp-test", "--name", "中文"};
    parser.parse_args(3, cli_args);
    CHECK(parser.get<std::string>("--name") == "中文");
#endif
    auto parsed_arguments = sindrecpp::general::cli::parse(
        std::vector<std::string_view>{"--name", "中文", "--verbose"},
        { {"--name", true, "默认"}, {"--verbose", false, "false"} });
    CHECK(parsed_arguments && parsed_arguments.value().get_string("--name") == "中文" &&
          parsed_arguments.value().has("--verbose"));
    CHECK(!sindrecpp::general::cli::parse({"--missing"}, {{"--name", true, {}}}));
#endif

#if defined(SINDRECPP_WITH_LOG)
    CHECK(sindrecpp::general::log::initialize());
    CHECK(sindrecpp::general::log::initialize());
    sindrecpp::general::log::info("中文初始化日志");
    const auto log_path = std::filesystem::temp_directory_path() / L"sindrecpp-中文日志.log";
    auto logger = sindrecpp::general::log::rotating_file("sindrecpp-general-test", log_path);
    logger->info("中文日志");
    logger->flush();
    CHECK(std::filesystem::is_regular_file(log_path));
    logger.reset();
    sindrecpp::general::log::native::drop("sindrecpp-general-test");
    std::filesystem::remove(log_path);
    CHECK(!sindrecpp::general::log::rotating_file("sindrecpp-null-filename", nullptr));
#endif

#if defined(SINDRECPP_WITH_HTTP)
    sindrecpp::general::http::Server server;
    std::atomic<int> retry_count{0};
    server.Get("/hello", [](const auto&, auto& response) {
        response.set_content("你好，SindreCpp", "text/plain; charset=UTF-8");
    });
    server.Get("/retry", [&](const auto&, auto& response) {
        if (++retry_count < 2)
            response.status = 503;
        else
            response.set_content("重试成功", "text/plain; charset=UTF-8");
    });
    server.Get("/json", [](const auto&, auto& response) {
        response.set_content(R"({"ok":true,"名字":"中文"})", "application/json");
    });
    const int port = server.bind_to_any_port("127.0.0.1");
    CHECK(port > 0);
    std::thread server_thread([&] { server.listen_after_bind(); });
    sindrecpp::general::http::Client client("127.0.0.1", port);
    client.set_connection_timeout(1, 0);
    client.set_read_timeout(1, 0);
    const auto response = client.Get("/hello");
    CHECK(response && response->status == 200 && response->body == "你好，SindreCpp");
    sindrecpp::general::http::RequestOptions request_options;
    request_options.connect_timeout_seconds = 1;
    request_options.read_timeout_seconds = 1;
    request_options.write_timeout_seconds = 1;
    request_options.retries = 2;
    request_options.retry_delay = std::chrono::milliseconds(1);
    auto wrapped = sindrecpp::general::http::get("127.0.0.1", port, "retry", request_options);
    CHECK(wrapped && wrapped.value().status == 200 && wrapped.value().body == "重试成功" &&
          retry_count == 2);
    auto json_response = sindrecpp::general::http::get_json("127.0.0.1", port, "/json");
    CHECK(json_response && json_response.value().root()["ok"].get_bool().value_unsafe());
    auto invalid_http = sindrecpp::general::http::get("", port);
    CHECK(!invalid_http && invalid_http.error().context == "http.get");
    server.stop();
    server_thread.join();
    sindrecpp::general::http::Client unavailable("127.0.0.1", port + 1);
    unavailable.set_connection_timeout(1, 0);
    unavailable.set_read_timeout(1, 0);
    CHECK(!unavailable.Get("/missing"));
#endif

    CHECK(std::string(sindrecpp::general::version) == "0.1.0");
}
