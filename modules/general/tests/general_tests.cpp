#include <sindre/general.h>
#if defined(SINDRE_WITH_HTTP)
#include <sindre/general/network.h>
#endif
#if defined(SINDRE_WITH_JSON)
#include <sindre/general/core.h>
#endif
#if defined(SINDRE_WITH_CLI)
#include <sindre/general/cli.h>
#endif
#if defined(SINDRE_WITH_LOG)
#include <sindre/general/diag.h>
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
    auto value = sindre::general::Result<int>::success(42);
    CHECK(value);
    CHECK(value.value() == 42);

    auto failure = sindre::general::Result<int>::failure({{}, "expected failure", {}});
    CHECK(!failure);
    CHECK(failure.error().message == "expected failure");
    auto detailed_failure = sindre::general::Result<int>::failure(
        std::make_error_code(std::errc::invalid_argument), "bad value", "general.test");
    CHECK(detailed_failure.error().context == "general.test" &&
          detailed_failure.error().describe().find("general.test") != std::string::npos);

    auto no_value = sindre::general::Result<void>::success();
    CHECK(no_value);
    auto no_value_error = sindre::general::Result<void>::failure({{}, "failed", {}});
    CHECK(!no_value_error);
    CHECK(no_value_error.error().message == "failed");
    CHECK(std::string(sindre::general::library_abi()) == "sindre.general.cxx17");
    double matrix_data[] = {1.0, 2.0, 3.0, 4.0};
    auto matrix = sindre::general::eigen::map_matrix(matrix_data, 2, 2);
    CHECK(matrix.rows() == 2 && matrix.cols() == 2 && matrix(1, 1) == 4.0);

    CHECK(sindre::general::string::trim(" \t hello\r\n") == "hello");
    CHECK(sindre::general::string::trim(" \t\r\n").empty());
    CHECK(sindre::general::string::trim_copy(std::string("  临时字符串  ")) == "临时字符串");
    CHECK(sindre::general::string::starts_with("sindre", "sin"));
    CHECK(sindre::general::string::ends_with("sindre", "dre"));
    CHECK(sindre::general::string::contains("sindre", "ind"));
    CHECK(sindre::general::string::count("aaaa", "aa") == 2);
    CHECK(sindre::general::string::count("abc", "") == 4);
    CHECK(sindre::general::string::strip("  hello  ") == "hello");
    CHECK(sindre::general::string::lstrip("  hello  ") == "hello  ");
    CHECK(sindre::general::string::rstrip("  hello  ") == "  hello");
    CHECK(sindre::general::string::replace_all("a-b-a", "a", "x") == "x-b-x");
    const auto parts = sindre::general::string::split("one,two,", ',');
    CHECK(parts.size() == 3 && parts[0] == "one" && parts[2].empty());
    const auto owned_parts = sindre::general::string::split(std::string("临时,字符串,"), ',');
    CHECK(owned_parts.size() == 3 && owned_parts[0] == "临时" && owned_parts[2].empty());
    const auto reverse_parts = sindre::general::string::rsplit("one::two::three", "::", 1);
    CHECK(reverse_parts.size() == 2 && reverse_parts[0] == "one::two" && reverse_parts[1] == "three");
    CHECK(sindre::general::string::repeat("ab", 3) == "ababab");
    const std::string chinese = "中文字符";
    const char* chinese_chars = chinese.c_str();
    CHECK(std::string_view(chinese_chars) == chinese);
    CHECK(sindre::general::string::trim("  " + chinese + "  ") == chinese);
    CHECK(sindre::general::string::replace_all(chinese + "-" + chinese, chinese, "x") == "x-x");
    const std::string mixed = chinese + "-字符";
    const auto views = sindre::general::string::split_view(mixed, '-');
    CHECK(views.size() == 2 && views[0] == chinese && views[1] == "字符");
    CHECK(sindre::general::string::lower_ascii("AbC中文") == "abc中文");
    CHECK(sindre::general::string::upper_ascii("aBc中文") == "ABC中文");
    CHECK(sindre::general::string::valid_utf8(chinese));
    CHECK(!sindre::general::string::valid_utf8(std::string("\xE4\xB8", 2)));
    const auto parsed_int = sindre::general::string::parse_int("  -42 ");
    CHECK(parsed_int && parsed_int.value() == -42);
    const auto parsed_hex = sindre::general::string::parse_int("ff", 16);
    CHECK(parsed_hex && parsed_hex.value() == 255);
    const auto parsed_float = sindre::general::string::parse_float(" 3.125 ");
    CHECK(parsed_float && std::abs(parsed_float.value() - 3.125) < 1e-12);
    CHECK(!sindre::general::string::parse_int("12x"));
    CHECK(!sindre::general::string::parse_float("nan"));
    CHECK(sindre::general::string::join({"中文", "sindre"}, "/") == "中文/sindre");
    CHECK(sindre::general::string::concat("Sindre", "Cpp", 17) == "SindreCpp17");
    sindre::general::string::String python_text("  A,b,中文  ");
    CHECK(python_text.try_trim());
    CHECK(python_text.try_lower_ascii());
    CHECK(python_text.to_utf8() == "a,b,中文");
    const auto python_parts = python_text.try_split(",");
    CHECK(python_parts && python_parts.value().size() == 3);
    CHECK(python_text.find(sindre::general::string::String("中文")) !=
          sindre::general::string::String::npos);
    CHECK(python_text.size() == 6 && python_text.size_storage() > python_text.size());
    const auto first_code_point = python_text.get_code_point(0);
    CHECK(first_code_point && first_code_point.value().unicode() == 'a');
    const auto string_int = sindre::general::string::String(" 42 ").to_int();
    const auto string_float = sindre::general::string::String("3.125").to_float();
    const auto string_bool = sindre::general::string::String(" YES ").to_bool();
    CHECK(string_int && string_int.value() == 42);
    CHECK(string_float && std::abs(string_float.value() - 3.125) < 1e-12);
    CHECK(string_bool && string_bool.value());
    CHECK(!sindre::general::string::String("maybe").to_bool());
    const auto converted_utf16 = python_text.try_to_utf16();
    CHECK(converted_utf16 && converted_utf16.value() == u"a,b,中文");
    const sindre::general::string::String converted_back(converted_utf16.value());
    CHECK(converted_back.to_utf8() == "a,b,中文");
    const sindre::general::string::String invalid_utf8(std::string("\xE4", 1));
    CHECK(invalid_utf8.size() == 1 && invalid_utf8.get_code_point(0) &&
          invalid_utf8.get_code_point(0).value().unicode() == 0xFFFD);
    CHECK(!sindre::general::string::normalize_utf8(std::string("\xE4", 1)));

    sindre::general::CancellationSource source;
    auto async_value = sindre::general::run_async([](sindre::general::CancellationToken token) {
        for (int i = 0; i < 10; ++i) {
            if (token.cancelled()) return -1;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return 42;
    }, source.token());
    auto async_result = sindre::general::wait_for(async_value, std::chrono::seconds(1));
    CHECK(async_result && async_result.value() == 42);
    auto slow = sindre::general::run_async([](sindre::general::CancellationToken token) {
        while (!token.cancelled()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return 7;
    }, source.token());
    auto timed_out = sindre::general::wait_for(slow, std::chrono::milliseconds(1));
    CHECK(!timed_out && timed_out.error().code == std::make_error_code(std::errc::timed_out));
    source.cancel();
    const auto cancelled = slow.get();
    CHECK(!cancelled && cancelled.error().code == std::make_error_code(std::errc::operation_canceled));
    std::atomic<int> progress_events{0};
    auto progressed = sindre::general::run_async(
        [](sindre::general::CancellationToken, const std::function<void(double)> &progress) {
            progress(0.25);
            progress(1.0);
            return 5;
        }, {}, [&](double value) {
            if (value >= 0.0 && value <= 1.0) ++progress_events;
        });
    auto progressed_result = sindre::general::wait_for(progressed, std::chrono::seconds(1));
    CHECK(progressed_result && progressed_result.value() == 5 && progress_events == 2);
    sindre::general::ThreadPool pool(2);
    auto pooled = pool.submit([](sindre::general::CancellationToken) { return 9; });
    CHECK(pooled);
    auto pooled_result = sindre::general::wait_for(pooled.value(), std::chrono::seconds(1));
    CHECK(pooled_result && pooled_result.value() == 9);
    auto safely_launched = sindre::general::try_run_async(
        [](sindre::general::CancellationToken) { return 11; });
    CHECK(safely_launched);
    auto safely_launched_result = sindre::general::wait_for(
        safely_launched.value(), std::chrono::seconds(1));
    CHECK(safely_launched_result && safely_launched_result.value() == 11);
    const auto uuid = sindre::general::codec::uuid4();
    CHECK(uuid && uuid.value().size() == 36 && uuid.value()[14] == '4' &&
          (uuid.value()[19] == '8' || uuid.value()[19] == '9' || uuid.value()[19] == 'a' ||
           uuid.value()[19] == 'b'));
    CHECK(sindre::general::codec::fnv1a64("中文") != sindre::general::codec::fnv1a64("中文 "));
    const std::vector<std::uint8_t> bytes{0, 1, 2, 253, 254, 255};
    const auto encoded = sindre::general::codec::base64_encode(bytes);
    CHECK(encoded && encoded.value() == "AAEC/f7/");
    const auto decoded = sindre::general::codec::base64_decode(encoded.value());
    CHECK(decoded && decoded.value() == bytes);
    CHECK(!sindre::general::codec::base64_decode("bad?"));
    const auto compressed = sindre::general::codec::simple_compress(
        std::vector<std::uint8_t>{1, 1, 1, 2, 3, 3, 3, 3});
    CHECK(compressed && compressed.value().size() < 8 &&
          sindre::general::codec::simple_decompress(compressed.value()).value() ==
              std::vector<std::uint8_t>({1, 1, 1, 2, 3, 3, 3, 3}));
    CHECK(!sindre::general::codec::simple_decompress(std::vector<std::uint8_t>{1}));
    const auto unicode_path = sindre::general::path::from_utf8(
        sindre::general::path::to_utf8(std::filesystem::temp_directory_path() / L"sindre-监控.txt"));
    std::filesystem::remove(unicode_path);
    std::promise<void> changed;
    auto changed_future = changed.get_future();
    sindre::general::file_watch::Watcher watcher(unicode_path, std::chrono::milliseconds(10));
    CHECK(watcher.start([&](const auto &event) {
        if (event.exists) changed.set_value();
    }));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    { std::ofstream output(unicode_path); output << "中文"; }
    CHECK(changed_future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    watcher.stop();
    std::filesystem::remove(unicode_path);

    const auto directory_root = std::filesystem::temp_directory_path() / L"sindre-general-directory";
    std::filesystem::remove_all(directory_root);
    std::filesystem::create_directories(directory_root / "one");
    std::filesystem::create_directories(directory_root / "two");
    { std::ofstream output(directory_root / "one" / "first.onnx"); output << "one"; }
    { std::ofstream output(directory_root / "two" / "second.onnx"); output << "two"; }
    const auto direct_entries = sindre::general::list_directory(directory_root);
    CHECK(direct_entries && direct_entries.value().size() == 2);
    const auto recursive_entries = sindre::general::list_directory(directory_root, true);
    CHECK(recursive_entries && recursive_entries.value().size() == 4);
    const auto model_pattern = sindre::general::path::to_utf8(directory_root / "*" / "*.onnx");
    const auto model_files = sindre::general::glob(model_pattern);
    CHECK(model_files && model_files.value().size() == 2);
    const auto single_character_pattern =
        sindre::general::path::to_utf8(directory_root / "one" / "first.on?x");
    const auto single_character_files = sindre::general::glob(single_character_pattern);
    CHECK(single_character_files && single_character_files.value().size() == 1);
    const auto recursive_pattern = sindre::general::path::to_utf8(directory_root / "**" / "*.onnx");
    const auto recursive_model_files = sindre::general::glob(recursive_pattern);
    CHECK(recursive_model_files && recursive_model_files.value().size() == 2);
    const auto missing_pattern = sindre::general::path::to_utf8(directory_root / "missing" / "*.onnx");
    const auto missing_files = sindre::general::glob(missing_pattern);
    CHECK(missing_files && missing_files.value().empty());
    const auto hash_path = directory_root / "hash.txt";
    { std::ofstream output(hash_path); output << "abc"; }
    const auto hash_size = sindre::general::file_size(hash_path);
    CHECK(hash_size && hash_size.value() == 3);
    const auto hash_info = sindre::general::file_info(hash_path);
    CHECK(hash_info && hash_info.value().regular_file && hash_info.value().size == 3);
    const auto missing_file = sindre::general::file_exists(directory_root / "missing.txt");
    CHECK(missing_file && !missing_file.value());
    const auto md5 = sindre::general::file_md5(hash_path);
    CHECK(md5 && md5.value() == "900150983cd24fb0d6963f7d28e17f72");
    const auto sha256 = sindre::general::file_sha256(hash_path);
    CHECK(sha256 && sha256.value() ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const auto large_hash_path = directory_root / "large-hash.bin";
    { std::ofstream output(large_hash_path, std::ios::binary); output << std::string(1000, 'a'); }
    CHECK(sindre::general::file_md5(large_hash_path).value() ==
          "cabe45dcc9ae5b66ba86600cca6b8ba8");
    CHECK(sindre::general::file_sha256(large_hash_path).value() ==
          "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
    CHECK(sindre::general::files_equal(hash_path, hash_path).value());
    CHECK(!sindre::general::files_equal(hash_path, directory_root / "one" / "first.onnx").value());
    std::filesystem::remove_all(directory_root);

    auto pointer = sindre::general::pointer::make_unique<std::string>("owned");
    CHECK(*pointer == "owned");
    auto shared = sindre::general::pointer::make_shared<std::string>(chinese);
    sindre::general::pointer::weak_ptr<std::string> weak = shared;
    CHECK(!weak.expired() && *weak.lock() == chinese);
    shared.reset();
    CHECK(weak.expired());

    bool guard_called = false;
    { auto guard = sindre::general::scope_guard([&] { guard_called = true; }); }
    CHECK(guard_called);
    const std::vector<int> numbers{1, 2, 3};
    CHECK(sindre::general::ranges::contains(numbers, 2));
    const auto doubled = sindre::general::ranges::transform(numbers, [](int value) { return value * 2; });
    CHECK(doubled == std::vector<int>({2, 4, 6}));
    CHECK(sindre::general::ranges::for_each_result(numbers, [](int) {
        return sindre::general::Result<void>::success();
    }));
    CHECK(sindre::general::url::encode_component("中文 a+b") == "%E4%B8%AD%E6%96%87%20a%2Bb");
    const auto query = sindre::general::url::parse_query("name=%E4%B8%AD%E6%96%87&x=1%2B2");
    CHECK(query && query.value().size() == 2 && query.value()[0].second == "中文" &&
          query.value()[1].second == "1+2" && sindre::general::url::build_query(query.value()) ==
          "name=%E4%B8%AD%E6%96%87&x=1%2B2");
    CHECK(!sindre::general::url::decode_component("%Q0"));
    CHECK(sindre::general::system::set_environment("SINDRE_GENERAL_TEST", "中文"));
    CHECK(sindre::general::system::environment("SINDRE_GENERAL_TEST").value() == "中文");
    const auto system_info = sindre::general::system::information();
    CHECK(!system_info.os.empty() && !system_info.architecture.empty());
    const auto temporary = sindre::general::temp::File::create("sindre-test-", ".dat");
    CHECK(temporary && std::filesystem::is_regular_file(temporary.value().path()));
    const auto temporary_path = temporary.value().path();
#if defined(_WIN32)
    const std::filesystem::path missing_library = L"sindre-does-not-exist.dll";
#else
    const std::filesystem::path missing_library = "/sindre-does-not-exist.so";
#endif
    CHECK(!sindre::general::dynamic_library::Library::open(missing_library));
    CHECK(sindre::general::diagnostics::check(true, "ok"));
    sindre::general::diagnostics::Stopwatch stopwatch;
    CHECK(stopwatch.elapsed().count() >= 0);
    sindre::general::process::Options process_options;
    process_options.capture_output = true;
    const auto process_result = sindre::general::process::run(
#if defined(_WIN32)
        "cmd /c echo sindre"
#else
        "printf sindre"
#endif
        , process_options);
    CHECK(process_result && process_result.value().exit_code == 0 &&
          process_result.value().stdout_text.find("sindre") != std::string::npos);
    CHECK(!sindre::general::crashpad::start({}).has_value());
    CHECK(std::filesystem::exists(temporary_path));
    // File is removed by the owning temporary-file guard at scope exit.
    const auto parsed_version = sindre::general::versioning::parse("1.2.3-beta");
    CHECK(parsed_version && parsed_version.value().major == 1 &&
          sindre::general::versioning::to_string(parsed_version.value()) == "1.2.3-beta");
    CHECK(!sindre::general::versioning::parse("1.2"));
    const auto stable_version = sindre::general::versioning::parse("1.2.3");
    const auto newer_version = sindre::general::versioning::parse("1.2.4");
    CHECK(stable_version && newer_version && stable_version.value() < newer_version.value());
    CHECK(sindre::general::startup::location("sindre-test"));
    CHECK(!sindre::general::desktop::notify("title", "body"));

    sindre::general::string::String utf8_text("中文字符串");
    sindre::general::string::String utf16_text(u"中文字符串");
    CHECK(utf8_text.to_utf8() == "中文字符串");
    CHECK(utf16_text.to_utf16() == u"中文字符串");
    CHECK(utf16_text.to_utf8() == "中文字符串");

#if defined(SINDRE_WITH_JSON)
    const auto document = sindre::general::json::try_parse(R"({"名字":"中文"})");
    CHECK(document);
    auto name = document.value().root()["名字"].get_string();
    CHECK(name.error() == simdjson::SUCCESS && name.value_unsafe() == std::string_view("中文"));
    const auto invalid_json = sindre::general::json::try_parse(R"({"unterminated":)");
    CHECK(!invalid_json && invalid_json.error().context == "json.parse");
    auto config = sindre::general::config::Config::from_json(
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
    const auto config_path = std::filesystem::temp_directory_path() / L"sindre-中文-config.json";
    { std::ofstream output(config_path); output << R"({"server":{"port":8081}})"; }
    auto file_config = sindre::general::config::Config::from_file(config_path);
    CHECK(file_config && file_config.value().get_int("server.port").value() == 8081);
    std::filesystem::remove(config_path);
#if defined(_WIN32)
    _putenv_s("SINDRE_TEST_SERVER_PORT", "");
#else
    unsetenv("SINDRE_TEST_SERVER_PORT");
#endif
#endif

#if defined(SINDRE_WITH_CLI)
#if !defined(SINDRE_NO_EXCEPTIONS)
    sindre::general::cli::ArgumentParser parser("sindre-test");
    parser.add_argument("--name").default_value(std::string("默认"));
    const char* cli_args[] = {"sindre-test", "--name", "中文"};
    parser.parse_args(3, cli_args);
    CHECK(parser.get<std::string>("--name") == "中文");
#endif
    auto parsed_arguments = sindre::general::cli::parse(
        std::vector<std::string_view>{"--name", "中文", "--verbose"},
        { {"--name", true, "默认"}, {"--verbose", false, "false"} });
    CHECK(parsed_arguments && parsed_arguments.value().get_string("--name") == "中文" &&
          parsed_arguments.value().has("--verbose"));
    CHECK(!sindre::general::cli::parse({"--missing"}, {{"--name", true, {}}}));
#endif

#if defined(SINDRE_WITH_LOG)
    CHECK(sindre::general::log::initialize());
    CHECK(sindre::general::log::initialize());
    sindre::general::log::info("中文初始化日志");
    const auto log_path = std::filesystem::temp_directory_path() / L"sindre-中文日志.log";
    auto logger = sindre::general::log::rotating_file("sindre-general-test", log_path);
    logger->info("中文日志");
    logger->flush();
    CHECK(std::filesystem::is_regular_file(log_path));
    logger.reset();
    sindre::general::log::native::drop("sindre-general-test");
    std::filesystem::remove(log_path);
    CHECK(!sindre::general::log::rotating_file("sindre-null-filename", nullptr));
#endif


#if defined(SINDRE_WITH_HTTP)
    sindre::general::http::Server server;
    std::atomic<int> retry_count{0};
    server.Get("/hello", [](const auto&, auto& response) {
        response.set_content("你好，sindre", "text/plain; charset=UTF-8");
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
    sindre::general::http::Client client("127.0.0.1", port);
    client.set_connection_timeout(1, 0);
    client.set_read_timeout(1, 0);
    const auto response = client.Get("/hello");
    CHECK(response && response->status == 200 && response->body == "你好，sindre");
    sindre::general::http::RequestOptions request_options;
    request_options.connect_timeout_seconds = 1;
    request_options.read_timeout_seconds = 1;
    request_options.write_timeout_seconds = 1;
    request_options.retries = 2;
    request_options.retry_delay = std::chrono::milliseconds(1);
    auto wrapped = sindre::general::http::get("127.0.0.1", port, "retry", request_options);
    CHECK(wrapped && wrapped.value().status == 200 && wrapped.value().body == "重试成功" &&
          retry_count == 2);
    auto json_response = sindre::general::http::get_json("127.0.0.1", port, "/json");
    CHECK(json_response && json_response.value().root()["ok"].get_bool().value_unsafe());
    auto invalid_http = sindre::general::http::get("", port);
    CHECK(!invalid_http && invalid_http.error().context == "http.get");
    server.stop();
    server_thread.join();
    sindre::general::http::Client unavailable("127.0.0.1", port + 1);
    unavailable.set_connection_timeout(1, 0);
    unavailable.set_read_timeout(1, 0);
    CHECK(!unavailable.Get("/missing"));
#endif

    CHECK(std::string(sindre::general::version) == "0.1.0");
}
