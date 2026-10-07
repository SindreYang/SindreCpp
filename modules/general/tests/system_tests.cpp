#include <sindre/general/system.h>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "Check failed at " << __FILE__ << ':' << __LINE__ \
                  << ": " #condition << '\n'; \
        return EXIT_FAILURE; \
    } \
} while (false)

int main() {
    const auto root = std::filesystem::temp_directory_path() / L"sindre-system-tests";
    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    std::filesystem::create_directories(root);

    const auto text_path = root / L"中文.txt";
    CHECK(sindre::general::path::write_text(text_path, "中文内容"));
    const auto exists = sindre::general::path_exists(text_path);
    CHECK(exists && exists.value());
    const auto read = sindre::general::path::read_text(text_path);
    CHECK(read && read.value() == "中文内容");
    const std::vector<std::uint8_t> bytes{0, 1, 127, 128, 255};
    const auto binary_path = root / L"data.bin";
    CHECK(sindre::general::path::write_bytes(binary_path, bytes));
    const auto binary = sindre::general::path::read_bytes(binary_path);
    CHECK(binary && binary.value() == bytes);
    const auto size = sindre::general::get_file_size(text_path);
    CHECK(size && size.value() == std::string("中文内容").size());
    const auto info = sindre::general::get_file_info(text_path);
    CHECK(info && info.value().regular_file && !info.value().directory);
    const auto md5 = sindre::general::calculate_file_md5(text_path);
    CHECK(md5 && md5.value() == "bcfd6d08ffd55b1bccb3d3d5abc1520c");
    const auto sha256 = sindre::general::calculate_file_sha256(text_path);
    CHECK(sha256 && sha256.value() ==
          "082516888f2d11c0affc46ead329ad836f3ab7e40444623315196c6fe712bb9a");

    const auto copy_path = root / L"copy.txt";
    CHECK(sindre::general::path::write_text(copy_path, "中文内容"));
    const auto equal = sindre::general::compare_files(text_path, copy_path);
    CHECK(equal && equal.value());
    const auto entries = sindre::general::list_directory(root);
    CHECK(entries && entries.value().size() == 3);
    const auto matches = sindre::general::glob(sindre::general::path::to_utf8(root / "*.txt"));
    CHECK(matches && matches.value().size() == 2);
    const auto nested = root / "nested" / "deep";
    CHECK(sindre::general::create_directories(nested));
    CHECK(sindre::general::path::try_from_utf8(
        sindre::general::path::to_utf8(text_path)));
    CHECK(!sindre::general::path::try_from_utf8(std::string("bad\xFF", 4)));
    const auto copied_path = nested / "copied.txt";
    CHECK(sindre::general::copy_file(text_path, copied_path));
    const auto moved_path = nested / "moved.txt";
    CHECK(sindre::general::move_path(copied_path, moved_path));
    const auto removed_file = sindre::general::remove_file(moved_path);
    CHECK(removed_file && removed_file.value());
    const auto missing_file = sindre::general::remove_file(moved_path);
    CHECK(missing_file && !missing_file.value());
    const auto empty_directory = root / "empty";
    CHECK(sindre::general::create_directories(empty_directory));
    const auto removed_empty_directory = sindre::general::remove_directory(empty_directory);
    CHECK(removed_empty_directory && removed_empty_directory.value());
    const auto removed_nested_directory = sindre::general::remove_directory(root / "nested", true);
    CHECK(removed_nested_directory && removed_nested_directory.value());

    std::promise<sindre::general::file_watch::EventType> created;
    std::promise<sindre::general::file_watch::EventType> modified;
    std::promise<sindre::general::file_watch::EventType> removed;
    auto created_future = created.get_future();
    auto modified_future = modified.get_future();
    auto removed_future = removed.get_future();
    std::atomic_bool created_seen = false;
    std::atomic_bool modified_seen = false;
    std::atomic_bool removed_seen = false;
    const auto watched_path = root / L"watch.txt";
    sindre::general::file_watch::Watcher watcher(watched_path, std::chrono::milliseconds(5));
    CHECK(watcher.start([&](const auto &event) {
        if (event.type == sindre::general::file_watch::EventType::created && !created_seen.exchange(true))
            created.set_value(event.type);
        if (event.type == sindre::general::file_watch::EventType::modified && !modified_seen.exchange(true))
            modified.set_value(event.type);
        if (event.type == sindre::general::file_watch::EventType::removed && !removed_seen.exchange(true))
            removed.set_value(event.type);
    }));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    { std::ofstream output(watched_path); output << "one"; }
    CHECK(created_future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    { std::ofstream output(watched_path, std::ios::app); output << "two"; }
    CHECK(modified_future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    std::filesystem::remove(watched_path);
    CHECK(removed_future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    watcher.stop();
    CHECK(!watcher.get_error());

    CHECK(sindre::general::system::set_environment_variable("SINDRE_SYSTEM_TEST", "中文"));
    const auto environment = sindre::general::system::get_environment_variable("SINDRE_SYSTEM_TEST");
    CHECK(environment && environment.value() == "中文");
    CHECK(sindre::general::system::unset_environment_variable("SINDRE_SYSTEM_TEST"));
    CHECK(!sindre::general::system::get_environment_variable("SINDRE_SYSTEM_TEST"));
    CHECK(sindre::general::system::get_system_information());
    const auto executable = sindre::general::system::get_executable_path();
    CHECK(executable && std::filesystem::is_regular_file(executable.value()));

    auto temporary = sindre::general::temp::File::create("sindre-system-");
    CHECK(temporary && std::filesystem::is_regular_file(temporary.value().get_path()));
    const auto temporary_path = temporary.value().get_path();
    temporary.value().keep();
    CHECK(std::filesystem::exists(temporary_path));
    std::filesystem::remove(temporary_path);

    CHECK(sindre::general::startup::get_startup_location("sindre-system-test"));
    // Interactive desktop backends are covered by platform-specific smoke tests.

#if defined(SINDRE_WITH_JSON)
    const auto defaults = sindre::general::config::Config::create_with_defaults({
        {"name", "default"}, {"enabled", "true"}, {"retries", "3"}});
    auto config = sindre::general::config::Config::parse_json(
        R"({"server":{"host":"localhost"},"ratio":1.5})", defaults);
    CHECK(config && config.value().get_string("name").value() == "default" &&
          config.value().get_bool("enabled").value() &&
          config.value().get_int_or("retries", 0) == 3 &&
          std::abs(config.value().get_float("ratio").value() - 1.5) < 1e-12);
    CHECK(config.value().get_string_or("missing", "fallback") == "fallback");
    CHECK(!sindre::general::json::try_parse("{broken"));
#endif

    std::filesystem::remove_all(root, cleanup_error);
    return EXIT_SUCCESS;
}
