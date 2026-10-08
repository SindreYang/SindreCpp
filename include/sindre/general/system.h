#pragma once

/// @file
/// @brief 系统环境、文件、目录、配置和桌面相关基础接口。

#include <sindre/general/core.h>
#include <sindre/general/runtime.h>
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
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace sindre::general::path {
std::string to_utf8(const std::filesystem::path &value);
std::filesystem::path from_utf8(std::string_view value);
Result<std::string> try_to_utf8(const std::filesystem::path &value) noexcept;
Result<std::filesystem::path> try_from_utf8(std::string_view value) noexcept;
Result<std::string> read_text(const std::filesystem::path &value) noexcept;
Result<void> write_text(const std::filesystem::path &value, std::string_view text) noexcept;
Result<std::vector<std::uint8_t>> read_bytes(const std::filesystem::path &value) noexcept;
Result<void> write_bytes(const std::filesystem::path &value,
                         const std::vector<std::uint8_t> &data) noexcept;
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
Result<std::uint64_t> get_file_size(const std::filesystem::path &path) noexcept;
Result<bool> path_exists(const std::filesystem::path &path) noexcept;
Result<FileInfo> get_file_info(const std::filesystem::path &path) noexcept;
Result<bool> compare_files(const std::filesystem::path &left, const std::filesystem::path &right) noexcept;
Result<std::string> calculate_file_md5(const std::filesystem::path &path) noexcept;
Result<std::string> calculate_file_sha256(const std::filesystem::path &path) noexcept;
Result<void> create_directories(const std::filesystem::path &directory) noexcept;
Result<bool> remove_file(const std::filesystem::path &path) noexcept;
Result<bool> remove_directory(const std::filesystem::path &directory,
                              bool recursive = false) noexcept;
Result<void> copy_file(const std::filesystem::path &source,
                       const std::filesystem::path &destination,
                       bool overwrite = false) noexcept;
Result<void> move_path(const std::filesystem::path &source,
                       const std::filesystem::path &destination) noexcept;
Result<std::vector<std::filesystem::path>> list_directory(
    const std::filesystem::path &directory, bool recursive = false) noexcept;
Result<std::vector<std::filesystem::path>> glob(std::string_view pattern) noexcept;
}

namespace sindre::general::file {
/// @brief 文件加解密的缓冲区、覆盖、进度和取消选项。
struct CryptoOptions {
    bool overwrite = false;
    std::size_t buffer_size = 1024 * 1024;
    std::function<void(std::uint64_t current, std::uint64_t total)> progress;
    CancellationToken token{};
};

/// @brief 流式加密文件并在成功后原子替换目标文件。
Result<void> encrypt(const std::filesystem::path &source,
                     const std::filesystem::path &destination,
                     std::string_view password,
                     CryptoOptions options = {}) noexcept;
/// @brief 流式解密文件并在认证成功后原子替换目标文件。
Result<void> decrypt(const std::filesystem::path &source,
                     const std::filesystem::path &destination,
                     std::string_view password,
                     CryptoOptions options = {}) noexcept;
}

namespace sindre::general::file_watch {
enum class EventType {
    created,
    modified,
    removed
};
struct Event { std::filesystem::path path; EventType type = EventType::modified; };
class Watcher {
public:
    explicit Watcher(std::filesystem::path path,
                     std::chrono::milliseconds interval = std::chrono::milliseconds(100));
    ~Watcher();
    Watcher(const Watcher &) = delete;
    Watcher &operator=(const Watcher &) = delete;
    Result<void> start(std::function<void(const Event &)> callback);
    void stop() noexcept;
    std::optional<Error> get_error() const;
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
    const std::filesystem::path &get_path() const noexcept;
    void keep() noexcept;
private:
    void cleanup() noexcept;
    std::filesystem::path path_;
    bool remove_ = false;
};
}

namespace sindre::general::system {
/// @brief Shell backend used by shell_run().
enum class ShellBackend {
    platform_default,
    cmd,
    powershell,
    sh,
    bash
};

/// @brief Options for executing a trusted command through the platform shell.
struct ShellOptions {
    std::filesystem::path working_directory;
    bool capture_output = true;
    ShellBackend backend = ShellBackend::platform_default;
    // Timeout in whole seconds. Zero disables the timeout.
    int timeout_seconds = 30;
    CancellationToken token{};
    std::size_t maximum_output_bytes = 16u * 1024u * 1024u;
};

/// @brief Result of a completed shell command.
struct ShellResult {
    int exit_code = -1;
    bool signaled = false;
    std::string stdout_text;
    std::string stderr_text;
};

/// @brief Execute a trusted command through the platform shell.
///
/// The default backend is `cmd.exe` on Windows and `/bin/bash` on Linux/WSL;
/// `ShellOptions::backend` can select another supported backend. Do not
/// concatenate untrusted input into `command`; use a validated argument
/// strategy for untrusted values. A non-zero exit code is a completed command
/// and is returned in `ShellResult`; startup, timeout and cancellation failures
/// are returned as `Result` errors.
Result<ShellResult> shell_run(std::string command, ShellOptions options = {}) noexcept;

Result<std::string> get_environment_variable(std::string_view name) noexcept;
Result<void> set_environment_variable(std::string_view name, std::string_view value) noexcept;
Result<void> unset_environment_variable(std::string_view name) noexcept;
Result<std::filesystem::path> get_executable_path() noexcept;
struct DiskInformation {
    std::filesystem::path path;
    std::uint64_t total_bytes = 0;
    std::uint64_t available_bytes = 0;
};
struct GpuInformation {
    std::string name;
    std::string driver;
};
struct Information {
    std::string os;
    std::string os_version;
    std::string architecture;
    std::string compiler;
    std::string hostname;
    std::string username;
    std::string cpu_model;
    std::uint32_t cpu_count = 0;
    std::uint64_t memory_bytes = 0;
    std::uint64_t available_memory_bytes = 0;
    std::vector<std::string> local_ip_addresses;
    DiskInformation system_disk;
    std::vector<GpuInformation> gpus;
};
Result<Information> get_system_information() noexcept;
}

namespace sindre::general::startup {
Result<std::filesystem::path> get_startup_location(std::string_view name) noexcept;
Result<void> enable_startup(std::string name, std::string command) noexcept;
Result<void> disable_startup(std::string_view name) noexcept;
}

namespace sindre::general::desktop {
enum class MessageBoxType {
    information,
    warning,
    error,
    question
};

enum class MessageBoxResult {
    ok,
    yes,
    no,
    cancel
};

bool is_elevated() noexcept;
Result<void> request_elevation(std::string_view executable) noexcept;
Result<void> set_clipboard_text(std::string_view) noexcept;
Result<std::string> get_clipboard_text() noexcept;
Result<void> send_notification(std::string_view title, std::string_view message) noexcept;
Result<MessageBoxResult> show_message_box(
    std::string_view title,
    std::string_view message,
    MessageBoxType type = MessageBoxType::information) noexcept;
Result<std::vector<std::filesystem::path>> open_file_dialog(
    std::filesystem::path initial_directory = {}, bool multiple = false) noexcept;
Result<std::filesystem::path> save_file_dialog(
    std::filesystem::path initial_directory = {}, std::string suggested_name = {}) noexcept;
Result<std::filesystem::path> select_directory_dialog(
    std::filesystem::path initial_directory = {}) noexcept;
Result<void> start_tray(std::string_view tooltip) noexcept;
Result<void> stop_tray() noexcept;
}

namespace sindre::general::json {

namespace detail {
template <class T, class = void>
struct has_to_utf8 : std::false_type {};
template <class T>
struct has_to_utf8<T, std::void_t<decltype(std::declval<const T &>().to_utf8())>>
    : std::true_type {};
}

class Value;
class Object;
class Array;

class Value {
public:
    using Storage = std::variant<std::nullptr_t, bool, std::int64_t, std::uint64_t, double,
                                 std::string, std::shared_ptr<Object>, std::shared_ptr<Array>>;

    Value();
    Value(std::nullptr_t);
    Value(bool value);
    template <class T,
              std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<std::decay_t<T>, bool>, int> = 0>
    Value(T value);
    Value(float value);
    Value(double value);
    Value(const char *value);
    Value(std::string value);
    Value(std::string_view value);
    template <class T, std::enable_if_t<detail::has_to_utf8<T>::value, int> = 0>
    Value(const T &value) : storage_(value.to_utf8()) {}
    Value(Object value);
    Value(Array value);
    Value(const Value &other);
    Value(Value &&other) noexcept;
    Value &operator=(const Value &other);
    Value &operator=(Value &&other) noexcept;
    ~Value();

    bool is_null() const noexcept;
    bool is_bool() const noexcept;
    bool is_integer() const noexcept;
    bool is_number() const noexcept;
    bool is_string() const noexcept;
    bool is_object() const noexcept;
    bool is_array() const noexcept;

    Value &operator[](std::string_view key);
    const Value *find(std::string_view key) const noexcept;
    const Value *at(std::size_t index) const noexcept;
    Object *get_object() noexcept;
    const Object *get_object() const noexcept;
    Array *get_array() noexcept;
    const Array *get_array() const noexcept;
    void set(std::string key, Value value);
    void push_back(Value value);

    Result<std::string> get_string() const;
    Result<std::int64_t> get_int() const;
    Result<double> get_float() const;
    Result<bool> get_bool() const;
    Result<std::string> to_json() const noexcept;

    const Storage &get_storage() const noexcept { return storage_; }

private:
    void ensure_object();
    Storage storage_;
};

using Field = std::pair<std::string, Value>;
using Fields = std::vector<Field>;

class Object {
public:
    Object() = default;
    Object(std::initializer_list<Field> fields);
    explicit Object(Fields fields);

    Value &operator[](std::string_view key);
    const Value *find(std::string_view key) const noexcept;
    void set(std::string key, Value value);
    bool empty() const noexcept { return values_.empty(); }
    std::size_t size() const noexcept { return values_.size(); }
    auto begin() noexcept { return values_.begin(); }
    auto end() noexcept { return values_.end(); }
    auto begin() const noexcept { return values_.begin(); }
    auto end() const noexcept { return values_.end(); }

private:
    std::map<std::string, Value> values_;
};

class Array {
public:
    Array() = default;
    Array(std::initializer_list<Value> values);

    void push_back(Value value);
    bool empty() const noexcept { return values_.empty(); }
    std::size_t size() const noexcept { return values_.size(); }
    Value &operator[](std::size_t index) { return values_[index]; }
    const Value &operator[](std::size_t index) const { return values_[index]; }
    const Value *at(std::size_t index) const noexcept {
        return index < values_.size() ? &values_[index] : nullptr;
    }
    auto begin() noexcept { return values_.begin(); }
    auto end() noexcept { return values_.end(); }
    auto begin() const noexcept { return values_.begin(); }
    auto end() const noexcept { return values_.end(); }

private:
    std::vector<Value> values_;
};

template <class T,
          std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<std::decay_t<T>, bool>, int>>
Value::Value(T value)
    : storage_(static_cast<std::conditional_t<std::is_signed_v<T>, std::int64_t, std::uint64_t>>(value)) {}

Object object(std::initializer_list<Field> fields);
Array array(std::initializer_list<Value> values);
Result<Value> parse(std::string_view text);
Result<std::string> stringify(const Value &value) noexcept;
Result<std::string> build(const Fields &fields) noexcept;

class Document {
public:
    const Value &root() const noexcept;
private:
    struct State;
    explicit Document(std::shared_ptr<State> state) noexcept;
    friend Result<Document> try_parse(std::string_view);
    std::shared_ptr<State> state_;
};
Result<Document> try_parse(std::string_view json);
}
namespace sindre::general::config {
class Config {
public:
    static Config create_with_defaults(std::initializer_list<json::Field> defaults);
    void merge_values(const Config &override_values);
    static Result<Config> parse_json(std::string_view text);
    static Result<Config> parse_json(std::string_view text, const Config &defaults);
    static Result<Config> load_file(const std::filesystem::path &path);
    static Result<Config> load_file(const std::filesystem::path &path, const Config &defaults);
    void set(std::string key, json::Value value);
    void set_value(std::string key, std::string value);
    Result<std::string> to_json() const noexcept;
    Result<void> apply_environment_overrides(std::string_view prefix = {});
    Result<std::string> get_string(std::string_view key) const;
    std::string get_string_or(std::string_view key, std::string fallback = {}) const;
    Result<std::int64_t> get_int(std::string_view key) const;
    std::int64_t get_int_or(std::string_view key, std::int64_t fallback) const;
    Result<bool> get_bool(std::string_view key) const;
    bool get_bool_or(std::string_view key, bool fallback) const;
    Result<double> get_float(std::string_view key) const;
    double get_float_or(std::string_view key, double fallback) const;
private:
    static Result<void> flatten(const json::Value &, std::string, Config &);
    static Result<void> invalid(std::string, const std::string &);
    const json::Value *find(std::string_view key) const;
    std::map<std::string, json::Value> values_;
};
}
