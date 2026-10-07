#include <sindre/general.h>
#if defined(SINDRE_WITH_HTTP)
#include <sindre/general/network.h>
#include <httplib.h>
#endif
#if defined(SINDRE_WITH_JSON)
#include <sindre/general/core.h>
#endif
#if defined(SINDRE_WITH_CLI)
#include <sindre/general/cli.h>
#endif
#if defined(SINDRE_WITH_LOG)
#include <sindre/general/diag.h>
#include <spdlog/async_logger.h>
#endif

#include <cstdlib>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <memory>
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
    const auto first_code_point = python_text.try_get_code_point(0);
    CHECK(first_code_point && first_code_point.value() == U'a');
    CHECK(!python_text.is_empty() && python_text.get_size() == 6 &&
          python_text.get_storage_size() > python_text.get_size());
    const auto python_view = python_text.get_view();
    CHECK(!python_view.is_empty() && python_view.get_size() == 6 &&
          python_view.get_storage_size() == python_text.get_storage_size());
    CHECK(python_view.find(sindre::general::string::String("中文")) == 4);
    CHECK(python_view.find_first_of(sindre::general::string::String("中文")) == 4);
    CHECK(python_view.find_last_of(sindre::general::string::String("中文")) == 5);
    CHECK(python_view.find_first_not_of(sindre::general::string::String("a,")) == 2);
    CHECK(python_view.find_last_not_of(sindre::general::string::String("文")) == 4);
    CHECK(python_view.starts_with(sindre::general::string::String("a")) &&
          python_view.ends_with(sindre::general::string::String("中文")) &&
          python_view.contains(sindre::general::string::String("b,")) &&
          python_view.count(sindre::general::string::String(",")) == 2);
    const auto view_part = python_view.try_substr(2, 2);
    CHECK(view_part && view_part.value().try_to_string() &&
          view_part.value().try_to_string().value().to_utf8() == "b,");
    std::u32string visited;
    CHECK(python_view.try_for_each_code_point([&](char32_t code_point) {
        visited.push_back(code_point);
    }));
    CHECK(visited == U"a,b,中文");
    CHECK(python_view.try_get_code_point(4) && python_view.try_get_code_point(4).value() == U'中');
    CHECK(python_view.try_get_front() && python_view.try_get_front().value() == U'a');
    CHECK(python_view.try_get_back() && python_view.try_get_back().value() == U'文');
    CHECK(python_text.try_substr_view(2, 2) &&
          python_text.try_substr_view(2, 2).value().try_to_string() &&
          python_text.try_substr_view(2, 2).value().try_to_string().value().to_utf8() == "b,");
    sindre::general::string::String mutation("ab");
    CHECK(mutation.try_append(U'中', 2));
    CHECK(mutation.try_insert(1, python_view));
    CHECK(mutation.try_replace(1, 2, python_view));
    CHECK(mutation.try_clear());
    CHECK(mutation.is_empty());
    CHECK(mutation.try_append(U'x') && mutation.try_pop_back() && mutation.is_empty());
    CHECK(mutation.try_shrink_to_fit());
    const auto string_int = sindre::general::string::String(" 42 ").to_int();
    const auto string_float = sindre::general::string::String("3.125").to_float();
    const auto string_bool = sindre::general::string::String(" YES ").to_bool();
    CHECK(string_int && string_int.value() == 42);
    CHECK(string_float && std::abs(string_float.value() - 3.125) < 1e-12);
    CHECK(string_bool && string_bool.value());
    CHECK(!sindre::general::string::String("maybe").to_bool());
    const sindre::general::string::String integer_text(123);
    const sindre::general::string::String floating_text(3.5);
    const sindre::general::string::String bool_text(true);
    const sindre::general::string::String code_point_text(U"中");
    CHECK(integer_text.to_stdstr() == "123");
    CHECK(floating_text.to_std_string().find("3.5") == 0);
    CHECK(bool_text.to_utf8() == "true");
    CHECK(code_point_text.to_utf8() == "中");
    const auto code_point = code_point_text.to_code_point();
    CHECK(code_point && code_point.value() == U'中');
    CHECK(code_point_text.to_char32() && code_point_text.to_char32().value() == U'中');
    CHECK(sindre::general::string::String("A").to_char() &&
          sindre::general::string::String("A").to_char().value() == 'A');
    CHECK(!code_point_text.to_char());
    const auto from_literal = sindre::general::string::String::from("literal");
    CHECK(from_literal && from_literal.value().to_utf8() == "literal");
    const auto from_integer = sindre::general::string::String::from(456);
    CHECK(from_integer && from_integer.value().to_int() && from_integer.value().to_int().value() == 456);
    sindre::general::string::String replacement_text("a中文a");
    CHECK(replacement_text.replace(1, 2, sindre::general::string::String("世界")));
    CHECK(replacement_text.to_utf8() == "a世界a");
    CHECK(replacement_text.replace_all(sindre::general::string::String("a"),
                                       sindre::general::string::String("x")));
    CHECK(replacement_text.to_utf8() == "x世界x");
    const auto converted_utf16 = python_text.try_to_utf16();
    CHECK(converted_utf16 && converted_utf16.value() == u"a,b,中文");
    const sindre::general::string::String converted_back(converted_utf16.value());
    CHECK(converted_back.to_utf8() == "a,b,中文");
    const sindre::general::string::String invalid_utf8(std::string("\xE4", 1));
    CHECK(invalid_utf8.size() == 1 && invalid_utf8.try_get_code_point(0) &&
          invalid_utf8.try_get_code_point(0).value() == 0xFFFD);
    CHECK(!sindre::general::string::normalize_utf8(std::string("\xE4", 1)));

    sindre::general::CancellationSource source;
    auto async_value = sindre::general::run_async([](sindre::general::CancellationToken token) {
        for (int i = 0; i < 10; ++i) {
            if (token.is_cancelled()) return -1;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return 42;
    }, sindre::general::TaskOptions{source.get_token()});
    auto async_result = sindre::general::wait_for(async_value, std::chrono::seconds(1));
    CHECK(async_result && async_result.value() == 42);
    auto slow = sindre::general::run_async([](sindre::general::CancellationToken token) {
        while (!token.is_cancelled()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return 7;
    }, sindre::general::TaskOptions{source.get_token()});
    auto timed_out = sindre::general::wait_for(slow, std::chrono::milliseconds(1));
    CHECK(!timed_out && timed_out.error().code == std::make_error_code(std::errc::timed_out));
    source.cancel();
    const auto cancelled = slow.get();
    CHECK(!cancelled && cancelled.error().code == std::make_error_code(std::errc::operation_canceled));
    std::atomic<int> progress_events{0};
    sindre::general::TaskOptions progress_options;
    progress_options.progress = [&](double value) {
        if (value >= 0.0 && value <= 1.0) ++progress_events;
    };
    auto progressed = sindre::general::run_async(
        [](sindre::general::CancellationToken, const std::function<void(double)> &progress) {
            progress(0.25);
            progress(1.0);
            return 5;
        }, progress_options);
    auto progressed_result = sindre::general::wait_for(progressed, std::chrono::seconds(1));
    CHECK(progressed_result && progressed_result.value() == 5 && progress_events == 2);
    sindre::general::ThreadPool pool(2);
    auto pooled = pool.submit([](sindre::general::CancellationToken) { return 9; });
    CHECK(pooled);
    auto pooled_result = sindre::general::wait_for(pooled.value(), std::chrono::seconds(1));
    CHECK(pooled_result && pooled_result.value() == 9);
    auto move_only = pool.submit(
        [value = std::make_unique<int>(13)](sindre::general::CancellationToken) {
            return *value;
        });
    CHECK(move_only);
    auto move_only_result = sindre::general::wait_for(
        move_only.value(), std::chrono::seconds(1));
    CHECK(move_only_result && move_only_result.value() == 13);
    auto created_pool = sindre::general::ThreadPool::create(1);
    CHECK(created_pool && created_pool.value().is_running());
    auto managed_pool = std::move(created_pool).value();
    sindre::general::CancellationSource running_source;
    auto running = managed_pool.submit(
        [](sindre::general::CancellationToken token) {
            while (!token.is_cancelled()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return 14;
        },
        sindre::general::TaskOptions{running_source.get_token()});
    auto queued = managed_pool.submit(
        [](sindre::general::CancellationToken) { return 15; });
    CHECK(running && queued);
    std::thread stopper([&] { managed_pool.stop(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    running_source.cancel();
    stopper.join();
    CHECK(!running.value().get());
    CHECK(!queued.value().get());
    auto safely_launched = sindre::general::try_run_async(
        [](sindre::general::CancellationToken) { return 11; });
    CHECK(safely_launched);
    auto safely_launched_result = sindre::general::wait_for(
        safely_launched.value(), std::chrono::seconds(1));
    CHECK(safely_launched_result && safely_launched_result.value() == 11);
    sindre::general::TaskOptions expired_options;
    expired_options.deadline = std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
    auto expired_task = sindre::general::try_run_async(
        [](sindre::general::CancellationToken) { return 12; }, expired_options);
    CHECK(expired_task);
    auto expired_result = sindre::general::wait_for(
        expired_task.value(), std::chrono::seconds(1));
    CHECK(!expired_result &&
          expired_result.error().code == std::make_error_code(std::errc::timed_out));
    CHECK(sindre::general::sleep(0.001));
    CHECK(!sindre::general::sleep(-0.001));
    sindre::general::CancellationSource sleep_source;
    sleep_source.cancel();
    CHECK(!sindre::general::sleep(
        1.0,
        sindre::general::TaskOptions{sleep_source.get_token()}));
    std::future<sindre::general::Result<int>> invalid_future;
    CHECK(!sindre::general::wait_for(invalid_future, std::chrono::milliseconds(1)));
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
    const std::string secret("中文\0binary", 13);
    const auto encrypted_text = sindre::general::codec::encrypt(secret, "correct horse");
    CHECK(encrypted_text);
    const auto decrypted_text = sindre::general::codec::decrypt(encrypted_text.value(), "correct horse");
    CHECK(decrypted_text && decrypted_text.value() == secret);
    const auto wrong_password = sindre::general::codec::decrypt(encrypted_text.value(), "wrong horse");
    CHECK(!wrong_password &&
          wrong_password.error().code == std::make_error_code(std::errc::permission_denied));
    auto tampered = encrypted_text.value();
    tampered[tampered.size() - 1] = tampered[tampered.size() - 1] == 'A' ? 'B' : 'A';
    CHECK(!sindre::general::codec::decrypt(tampered, "correct horse"));
    const auto encrypted_bytes = sindre::general::codec::encrypt_bytes(bytes, "binary password");
    CHECK(encrypted_bytes);
    const auto decrypted_bytes = sindre::general::codec::decrypt_bytes(
        encrypted_bytes.value(), "binary password");
    CHECK(decrypted_bytes && decrypted_bytes.value() == bytes);
    const auto compressed = sindre::general::codec::rle_compress(
        std::vector<std::uint8_t>{1, 1, 1, 2, 3, 3, 3, 3});
    CHECK(compressed && compressed.value().size() < 8 &&
          sindre::general::codec::rle_decompress(compressed.value()).value() ==
              std::vector<std::uint8_t>({1, 1, 1, 2, 3, 3, 3, 3}));
    CHECK(!sindre::general::codec::rle_decompress(std::vector<std::uint8_t>{1}));
    const auto unicode_path = sindre::general::path::from_utf8(
        sindre::general::path::to_utf8(std::filesystem::temp_directory_path() / L"sindre-监控.txt"));
    const auto path_text = sindre::general::string::String::from(unicode_path);
    CHECK(path_text);
    const auto path_roundtrip = path_text.value().to_path();
    CHECK(path_roundtrip && sindre::general::path::to_utf8(path_roundtrip.value()) ==
          sindre::general::path::to_utf8(unicode_path));
    std::filesystem::remove(unicode_path);
    std::promise<void> changed;
    auto changed_future = changed.get_future();
    std::promise<void> modified;
    auto modified_future = modified.get_future();
    std::promise<void> removed;
    auto removed_future = removed.get_future();
    std::atomic_bool created_seen = false;
    std::atomic_bool modified_seen = false;
    std::atomic_bool removed_seen = false;
    sindre::general::file_watch::Watcher watcher(unicode_path, std::chrono::milliseconds(10));
    CHECK(watcher.start([&](const auto &event) {
        if (event.type == sindre::general::file_watch::EventType::created && !created_seen.exchange(true)) {
            changed.set_value();
        } else if (event.type == sindre::general::file_watch::EventType::modified && !modified_seen.exchange(true)) {
            modified.set_value();
        } else if (event.type == sindre::general::file_watch::EventType::removed && !removed_seen.exchange(true)) {
            removed.set_value();
        }
    }));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    { std::ofstream output(unicode_path); output << "中文"; }
    CHECK(changed_future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    { std::ofstream output(unicode_path, std::ios::app); output << " changed"; }
    CHECK(modified_future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    std::filesystem::remove(unicode_path);
    CHECK(removed_future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    watcher.stop();
    CHECK(!watcher.get_error());

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
    const auto hash_size = sindre::general::get_file_size(hash_path);
    CHECK(hash_size && hash_size.value() == 3);
    const auto hash_info = sindre::general::get_file_info(hash_path);
    CHECK(hash_info && hash_info.value().regular_file && hash_info.value().size == 3);
    const auto missing_file = sindre::general::path_exists(directory_root / "missing.txt");
    CHECK(missing_file && !missing_file.value());
    const auto md5 = sindre::general::calculate_file_md5(hash_path);
    CHECK(md5 && md5.value() == "900150983cd24fb0d6963f7d28e17f72");
    const auto sha256 = sindre::general::calculate_file_sha256(hash_path);
    CHECK(sha256 && sha256.value() ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const auto large_hash_path = directory_root / "large-hash.bin";
    { std::ofstream output(large_hash_path, std::ios::binary); output << std::string(1000, 'a'); }
    CHECK(sindre::general::calculate_file_md5(large_hash_path).value() ==
          "cabe45dcc9ae5b66ba86600cca6b8ba8");
    CHECK(sindre::general::calculate_file_sha256(large_hash_path).value() ==
          "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
    CHECK(sindre::general::compare_files(hash_path, hash_path).value());
    CHECK(!sindre::general::compare_files(hash_path, directory_root / "one" / "first.onnx").value());
    std::filesystem::remove_all(directory_root);

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
    const auto url_encoded = sindre::general::network::encode("中文 a+b");
    CHECK(url_encoded && url_encoded.value() == "%E4%B8%AD%E6%96%87%20a%2Bb");
    const auto query = sindre::general::network::parse_query("name=%E4%B8%AD%E6%96%87&x=1%2B2");
    const auto rebuilt_query = query ? sindre::general::network::build_query(query.value())
                                     : sindre::general::Result<std::string>::failure(query.error());
    CHECK(query && query.value().size() == 2 && query.value()[0].second == "中文" &&
          query.value()[1].second == "1+2" && rebuilt_query && rebuilt_query.value() ==
          "name=%E4%B8%AD%E6%96%87&x=1%2B2");
    CHECK(!sindre::general::network::decode("%Q0"));
    CHECK(sindre::general::system::set_environment_variable("SINDRE_GENERAL_TEST", "中文"));
    CHECK(sindre::general::system::get_environment_variable("SINDRE_GENERAL_TEST").value() == "中文");
    const auto system_info = sindre::general::system::get_system_information();
    CHECK(system_info && !system_info.value().os.empty() && !system_info.value().architecture.empty());
    const auto temporary = sindre::general::temp::File::create("sindre-test-", ".dat");
    CHECK(temporary && std::filesystem::is_regular_file(temporary.value().get_path()));
    const auto temporary_path = temporary.value().get_path();
#if defined(_WIN32)
    const std::filesystem::path missing_library = L"sindre-does-not-exist.dll";
#else
    const std::filesystem::path missing_library = "/sindre-does-not-exist.so";
#endif
    CHECK(!sindre::general::dynamic_library::Library::open(missing_library));
    CHECK(sindre::general::diagnostics::check(true, "ok"));
    sindre::general::runtime::stopwatch stopwatch;
    stopwatch.start();
    CHECK(stopwatch.stop().count() >= 0);
    std::chrono::nanoseconds scoped_elapsed{};
    bool scoped_name_valid = false;
    {
        sindre::general::runtime::scopedtimer timer(
            "general_test",
            [&](std::string_view name, std::chrono::nanoseconds elapsed) {
                scoped_name_valid = name == "general_test";
                scoped_elapsed = elapsed;
            });
        CHECK(timer.elapsed().count() >= 0);
    }
    CHECK(scoped_name_valid && scoped_elapsed.count() >= 0);
    auto thread_name_result = sindre::general::runtime::set_thread_name(
        "sindre_test");
    CHECK(thread_name_result ||
          thread_name_result.error().code ==
              std::make_error_code(std::errc::function_not_supported));

    std::atomic<int> retry_attempts{0};
    sindre::general::runtime::retry_options retry_options;
    retry_options.max_attempts = 3;
    retry_options.initial_delay = 0.0;
    retry_options.maximum_delay = 0.0;
    const auto retried = sindre::general::runtime::retry(
        [&](sindre::general::CancellationToken) -> sindre::general::Result<int> {
            const auto attempt = ++retry_attempts;
            if (attempt < 3) {
                return sindre::general::Result<int>::failure(
                    std::make_error_code(std::errc::connection_refused),
                    "retry me", "general.test");
            }
            return sindre::general::Result<int>::success(27);
        },
        retry_options);
    CHECK(retried && retried.value() == 27 && retry_attempts == 3);

    std::atomic<std::size_t> parallel_count{0};
    std::atomic<int> parallel_progress_events{0};
    sindre::general::runtime::parallel_options parallel_options;
    parallel_options.workers = 2;
    parallel_options.thread_name = "par";
    parallel_options.progress = [&](double value) {
        if (value >= 0.0 && value <= 1.0) ++parallel_progress_events;
    };
    const auto parallel_result = sindre::general::runtime::parallel_for(
        0, 32,
        [&](std::size_t, sindre::general::CancellationToken token) {
            if (token.is_cancelled()) return;
            ++parallel_count;
        },
        parallel_options);
    CHECK(parallel_result && parallel_count == 32 && parallel_progress_events > 0);

    sindre::general::CancellationSource parallel_source;
    parallel_source.cancel();
    parallel_options.token = parallel_source.get_token();
    const auto cancelled_parallel = sindre::general::runtime::parallel_for(
        0, 1, [](std::size_t) {}, parallel_options);
    CHECK(!cancelled_parallel &&
          cancelled_parallel.error().code ==
              std::make_error_code(std::errc::operation_canceled));
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
    CHECK(sindre::general::startup::get_startup_location("sindre-test"));
    // Interactive desktop backends are covered by platform-specific smoke tests.

    sindre::general::string::String utf8_text("中文字符串");
    sindre::general::string::String utf16_text(u"中文字符串");
    CHECK(utf8_text.to_utf8() == "中文字符串");
    CHECK(utf16_text.to_utf16() == u"中文字符串");
    CHECK(utf16_text.to_utf8() == "中文字符串");

#if defined(SINDRE_WITH_JSON)
    auto parsed = sindre::general::json::parse(
        R"({"名字":"中文","items":[1,true,null]})");
    CHECK(parsed && parsed.value().find("名字") &&
          parsed.value().find("名字")->get_string().value() == "中文");
    CHECK(parsed.value().find("items") && parsed.value().find("items")->is_array() &&
          parsed.value().find("items")->at(1)->get_bool().value());

    auto constructed = sindre::general::json::object({
        {"name", "sindre"},
        {"port", 8080},
        {"enabled", true},
        {"labels", sindre::general::json::array({"中文", "general"})},
    });
    sindre::general::string::String unicode_name("中文变量");
    constructed.set("unicode_name", unicode_name);
    auto serialized = sindre::general::json::stringify(
        sindre::general::json::Value(constructed));
    CHECK(serialized && serialized.value().find("\"port\":8080") != std::string::npos);
    auto round_trip = serialized ? sindre::general::json::parse(serialized.value())
                                 : sindre::general::Result<sindre::general::json::Value>::failure(
                                       std::make_error_code(std::errc::invalid_argument),
                                       "serialization failed", "test.json");
    CHECK(round_trip && round_trip.value().find("enabled")->get_bool().value());

    const auto invalid_json = sindre::general::json::try_parse(R"({"unterminated":)");
    CHECK(!invalid_json && invalid_json.error().context == "json.parse");
    auto config = sindre::general::config::Config::parse_json(
        R"({"server":{"host":"中文主机","port":8080},"ratio":1.25})");
    CHECK(config && config.value().get_string("server.host").value() == "中文主机" &&
          config.value().get_int("server.port").value() == 8080 &&
          std::abs(config.value().get_float_or("ratio", 0) - 1.25) < 1e-12);
    config.value().set("server.enabled", true);
    auto config_json = config.value().to_json();
    CHECK(config_json && config_json.value().find("\"server\":{") != std::string::npos &&
          config_json.value().find("\"enabled\":true") != std::string::npos);
    const auto defaults = sindre::general::config::Config::create_with_defaults({
        {"name", "default"}, {"enabled", "true"}, {"retries", "3"}});
    auto default_config = sindre::general::config::Config::parse_json("{\"ratio\":2.5}", defaults);
    CHECK(default_config && default_config.value().get_string("name").value() == "default" &&
          default_config.value().get_bool("enabled").value() &&
          default_config.value().get_int_or("retries", 0) == 3 &&
          default_config.value().get_bool_or("missing", false) == false &&
          std::abs(default_config.value().get_float_or("missing", 4.5) - 4.5) < 1e-12);
#if defined(_WIN32)
    _putenv_s("SINDRE_TEST_SERVER_PORT", "9090");
#else
    setenv("SINDRE_TEST_SERVER_PORT", "9090", 1);
#endif
    config.value().apply_environment_overrides("SINDRE_TEST");
    CHECK(config.value().get_int("server.port").value() == 9090);
    const auto config_path = std::filesystem::temp_directory_path() / L"sindre-中文-config.json";
    { std::ofstream output(config_path); output << R"({"server":{"port":8081}})"; }
    auto file_config = sindre::general::config::Config::load_file(config_path);
    CHECK(file_config && file_config.value().get_int("server.port").value() == 8081);
    std::filesystem::remove(config_path);
#if defined(_WIN32)
    _putenv_s("SINDRE_TEST_SERVER_PORT", "");
#else
    unsetenv("SINDRE_TEST_SERVER_PORT");
#endif
#endif

#if defined(SINDRE_WITH_CLI)
    sindre::general::cli::Specification cli_spec;
    cli_spec.add_option("--name", "-n")
        .default_value("默认")
        .help("display name");
    cli_spec.add_flag("--verbose", "-v")
        .help("enable verbose output");
    auto parsed_arguments = sindre::general::cli::parse(
        std::vector<std::string_view>{"--name", "中文", "--verbose"},
        cli_spec);
    CHECK(parsed_arguments && parsed_arguments.value().get_string("--name") == "中文" &&
          parsed_arguments.value().has("--verbose") &&
          parsed_arguments.value().is_set("--verbose"));
    CHECK(!sindre::general::cli::parse({"--missing"}, cli_spec));
#endif

#if defined(SINDRE_WITH_LOG)
    CHECK(sindre::general::log::init_log("sindre"));
    CHECK(sindre::general::log::init_log("sindre"));
    sindre::general::log::info("中文初始化日志");
    const auto log_path = std::filesystem::temp_directory_path() / L"sindre-中文日志.log";
    auto logger_result = sindre::general::log::try_rotating_file("sindre-general-test", log_path);
    CHECK(logger_result);
    auto logger = std::move(logger_result).value();
    logger->info("中文日志");
    logger->flush();
    CHECK(std::filesystem::is_regular_file(log_path));
    logger.reset();
    sindre::general::log::native::drop("sindre-general-test");
    std::filesystem::remove(log_path);
    CHECK(!sindre::general::log::rotating_file("sindre-null-filename", nullptr));
    CHECK(sindre::general::log::shutdown());
    CHECK(!sindre::general::log::init_log(
        "sindre-invalid-rotation", sindre::general::log::Level::info,
        sindre::general::log::default_pattern, log_path, 0, 2));
    CHECK(sindre::general::log::init_log(
        "sindre-file", sindre::general::log::Level::info,
        sindre::general::log::default_pattern, log_path, 1024, 2, true));
    auto file_logger = sindre::general::log::native::default_logger();
    file_logger->info("中文初始化轮转日志");
    file_logger->flush();
    CHECK(std::filesystem::is_regular_file(log_path));
    file_logger->sinks().clear();
    CHECK(sindre::general::log::shutdown());
    sindre::general::log::native::drop("sindre-file");
    std::filesystem::remove(log_path);
    const auto async_log_path = std::filesystem::temp_directory_path() / L"sindre-中文异步日志.log";
    CHECK(sindre::general::log::init_log(
        "sindre-async", sindre::general::log::Level::info,
        sindre::general::log::default_pattern, async_log_path, 1024, 2, false, true,
        64, 1, sindre::general::log::AsyncOverflowPolicy::overrun_oldest));
    auto async_logger = sindre::general::log::native::default_logger();
    CHECK(std::dynamic_pointer_cast<spdlog::async_logger>(async_logger) != nullptr);
    async_logger->info("中文异步初始化日志");
    async_logger->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(std::filesystem::is_regular_file(async_log_path));
    CHECK(sindre::general::log::shutdown());
    async_logger.reset();
    sindre::general::log::native::drop("sindre-async");
    std::filesystem::remove(async_log_path);
#endif


#if defined(SINDRE_WITH_HTTP)
    httplib::Server server;
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
    auto target = sindre::general::network::Url::parse(
        "http://127.0.0.1:" + std::to_string(port) + "/hello");
    CHECK(target);
    sindre::general::network::Request request;
    request.target = target.value();
    request.method = sindre::general::network::Method::get;
    auto response = sindre::general::network::Client().request(request);
    CHECK(response && response.value().status == 200 && response.value().body == "你好，sindre");
    sindre::general::network::RequestOptions request_options;
    request_options.timeout.connect = std::chrono::seconds(1);
    request_options.timeout.read = std::chrono::seconds(1);
    request_options.timeout.write = std::chrono::seconds(1);
    request_options.timeout.total = std::chrono::seconds(5);
    request_options.retry.max_attempts = 2;
    request_options.retry.initial_delay = std::chrono::milliseconds(1);
    request_options.retry.should_retry = [](const sindre::general::network::RetryContext &context) {
        return context.status && *context.status >= 500;
    };
    auto retry_target = sindre::general::network::Url::parse(
        "http://127.0.0.1:" + std::to_string(port) + "/retry");
    CHECK(retry_target);
    auto wrapped = sindre::general::network::get(retry_target.value(), request_options);
    CHECK(wrapped && wrapped.value().status == 200 && wrapped.value().body == "重试成功" &&
          retry_count == 2);
    auto json_target = sindre::general::network::Url::parse(
        "http://127.0.0.1:" + std::to_string(port) + "/json");
    CHECK(json_target);
    auto json_response = sindre::general::network::get_json(json_target.value());
    CHECK(json_response && json_response.value().find("ok") &&
          json_response.value().find("ok")->get_bool().value());
    auto invalid_http = sindre::general::network::Url::parse("ftp://127.0.0.1/");
    CHECK(!invalid_http && invalid_http.error().code ==
          sindre::general::network::make_error_code(sindre::general::network::NetworkErrc::unsupported_scheme));
    server.stop();
    server_thread.join();
    auto unavailable_target = sindre::general::network::Url::parse(
        "http://127.0.0.1:" + std::to_string(port + 1) + "/missing");
    CHECK(unavailable_target);
    CHECK(!sindre::general::network::get(unavailable_target.value()));
#endif

    CHECK(std::string(sindre::general::version) == "0.1.0");
}
