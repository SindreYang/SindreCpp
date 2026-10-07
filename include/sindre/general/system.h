#pragma once

/// @file
/// @brief 系统环境、文件、目录、配置和桌面相关基础接口。

#include <sindre/general/core.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sindre::general::path {
std::string to_utf8(const std::filesystem::path &value);
std::filesystem::path from_utf8(std::string_view value);
Result<std::string> try_to_utf8(const std::filesystem::path &value) noexcept;
Result<std::string> read_text(const std::filesystem::path &value) noexcept;
Result<void> write_text(const std::filesystem::path &value, std::string_view text) noexcept;
Result<std::filesystem::path> require_file(const std::filesystem::path &value);
}

namespace sindre::general {
struct FileInfo {
    std::filesystem::path path;
    std::uint64_t size = 0;
    bool regular_file = false;
    bool directory = false;
    std::filesystem::file_time_type modified{};
};
Result<std::uint64_t> file_size(const std::filesystem::path &path) noexcept;
Result<bool> file_exists(const std::filesystem::path &path) noexcept;
Result<FileInfo> file_info(const std::filesystem::path &path) noexcept;
Result<bool> files_equal(const std::filesystem::path &left, const std::filesystem::path &right) noexcept;
Result<std::string> file_md5(const std::filesystem::path &path) noexcept;
Result<std::string> file_sha256(const std::filesystem::path &path) noexcept;
Result<std::vector<std::filesystem::path>> list_directory(
    const std::filesystem::path &directory, bool recursive = false) noexcept;
Result<std::vector<std::filesystem::path>> glob(std::string_view pattern) noexcept;
}

namespace sindre::general::file_watch {
struct Event { std::filesystem::path path; bool exists = false; };
class Watcher {
public:
    explicit Watcher(std::filesystem::path path,
                     std::chrono::milliseconds interval = std::chrono::milliseconds(100));
    ~Watcher();
    Watcher(const Watcher &) = delete;
    Watcher &operator=(const Watcher &) = delete;
    Result<void> start(std::function<void(const Event &)> callback);
    void stop() noexcept;
    std::optional<Error> error() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}

namespace sindre::general::temp {
class File {
public:
    File() = default;
    File(const File &) = delete;
    File &operator=(const File &) = delete;
    File(File &&other) noexcept;
    File &operator=(File &&other) noexcept;
    ~File();
    static Result<File> create(std::string_view prefix = "sindre-",
                               std::string_view suffix = ".tmp") noexcept;
    const std::filesystem::path &path() const noexcept;
    void keep() noexcept;
private:
    void cleanup() noexcept;
    std::filesystem::path path_;
    bool remove_ = false;
};
}

namespace sindre::general::system {
Result<std::string> environment(std::string_view name) noexcept;
Result<void> set_environment(std::string_view name, std::string_view value) noexcept;
struct Information {
    std::string os;
    std::string architecture;
    std::string compiler;
    std::uint32_t cpu_count = 0;
    std::uint64_t memory_bytes = 0;
};
Information information();
}

namespace sindre::general::startup {
Result<std::filesystem::path> location(std::string_view name) noexcept;
Result<void> enable(std::string name, std::string command) noexcept;
Result<void> disable(std::string_view name) noexcept;
}

namespace sindre::general::desktop {
bool is_elevated() noexcept;
Result<void> request_elevation(std::string_view) noexcept;
Result<void> clipboard_set(std::string_view) noexcept;
Result<std::string> clipboard_get() noexcept;
Result<void> notify(std::string_view, std::string_view) noexcept;
Result<void> tray_start(std::string_view) noexcept;
}

#if defined(SINDRE_WITH_JSON)
#include <simdjson.h>
namespace sindre::general::json {
using Parser = simdjson::dom::parser;
using Element = simdjson::dom::element;
using Object = simdjson::dom::object;
using Array = simdjson::dom::array;
using Error = simdjson::error_code;
namespace native = simdjson;
class Document {
public:
    const Element &root() const noexcept;
private:
    struct State;
    explicit Document(std::shared_ptr<State> state) noexcept;
    friend Result<Document> try_parse(std::string_view);
    std::shared_ptr<State> state_;
};
Result<Document> try_parse(std::string_view json);
Result<Document> parse(std::string_view json);
}
namespace sindre::general::config {
class Config {
public:
    static Config with_defaults(std::initializer_list<std::pair<std::string, std::string>> defaults);
    void merge(const Config &override_values);
    static Result<Config> from_json(std::string_view text);
    static Result<Config> from_json(std::string_view text, const Config &defaults);
    static Result<Config> from_file(const std::filesystem::path &path);
    static Result<Config> from_file(const std::filesystem::path &path, const Config &defaults);
    void set(std::string key, std::string value);
    Result<void> apply_environment(std::string_view prefix = {});
    Result<std::string> get_string(std::string_view key) const;
    std::string get_string_or(std::string_view key, std::string fallback = {}) const;
    Result<std::int64_t> get_int(std::string_view key) const;
    Result<bool> get_bool(std::string_view key) const;
    Result<double> get_float(std::string_view key) const;
    double get_float_or(std::string_view key, double fallback) const;
private:
    static Result<void> flatten(const json::Element &, std::string, Config &);
    static Result<void> invalid(std::string, const std::string &);
    const std::string *find(std::string_view key) const;
    std::unordered_map<std::string, std::string> values_;
};
}
#endif
