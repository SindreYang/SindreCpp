#pragma once

/// @file
/// @brief 日志、诊断上下文和崩溃报告接口。

#include <sindre/general/core.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sindre::general::diagnostics {
Result<void> check(bool condition, std::string_view message,
                   std::string_view context = "assert") noexcept;
}

namespace sindre::general::log {

/// @brief Public log severity. The concrete logger backend is private.
enum class Level { trace, debug, info, warning, error, critical, off };

enum class AsyncOverflowPolicy { block, overrun_oldest, discard_new };

inline constexpr std::string_view default_pattern =
    "[%Y-%m-%d %H:%M:%S.%e] [%l] %v";
inline constexpr std::size_t default_max_size_bytes = 10 * 1024 * 1024;
inline constexpr std::size_t default_max_files = 5;
inline constexpr std::size_t default_async_queue_size = 8192;
inline constexpr std::size_t default_async_worker_threads = 1;

/// @brief Opaque, shared logger handle.
///
/// The implementation deliberately does not expose spdlog types. A logger is
/// safe to copy as a shared handle and backend failures are contained by the
/// Result-returning methods.
class Logger {
public:
    struct Impl;
    explicit Logger(std::shared_ptr<Impl> impl) noexcept;
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;
    Logger(Logger &&) noexcept;
    Logger &operator=(Logger &&) noexcept;
    ~Logger();

    [[nodiscard]] std::string get_name() const;
    [[nodiscard]] Level get_level() const noexcept;
    Result<void> set_level(Level level) noexcept;
    Result<void> write(Level level, std::string_view message) noexcept;
    Result<void> flush() noexcept;

private:
    std::shared_ptr<Impl> impl_;
    friend Result<void> initialize(std::shared_ptr<Logger> logger,
                                   Level level) noexcept;
};

using LoggerPtr = std::shared_ptr<Logger>;

void info(std::string_view message) noexcept;
void warning(std::string_view message) noexcept;
void error(std::string_view message) noexcept;

Result<void> set_level(Level level) noexcept;
Result<void> init_log(
    std::string name = "sindre", Level level = Level::info,
    std::string_view pattern = default_pattern,
    std::filesystem::path filename = {},
    std::size_t max_size_bytes = default_max_size_bytes,
    std::size_t max_files = default_max_files,
    bool rotate_on_open = false,
    bool asynchronous = false,
    std::size_t async_queue_size = default_async_queue_size,
    std::size_t async_worker_threads = default_async_worker_threads,
    AsyncOverflowPolicy async_overflow = AsyncOverflowPolicy::block) noexcept;

/// @brief Create or reuse a named logger without changing the global logger.
Result<LoggerPtr> create_logger(std::string name,
                                Level level = Level::info) noexcept;
Result<void> initialize(LoggerPtr logger = {},
                        Level level = Level::info) noexcept;
Result<void> shutdown() noexcept;

Result<LoggerPtr> try_rotating_file(
    std::string name, std::string filename,
    std::size_t max_size_bytes = default_max_size_bytes,
    std::size_t max_files = default_max_files,
    bool rotate_on_open = false) noexcept;
LoggerPtr rotating_file(
    std::string name, std::string filename,
    std::size_t max_size_bytes = default_max_size_bytes,
    std::size_t max_files = default_max_files,
    bool rotate_on_open = false) noexcept;
LoggerPtr rotating_file(
    std::string name, const char *filename,
    std::size_t max_size_bytes = default_max_size_bytes,
    std::size_t max_files = default_max_files,
    bool rotate_on_open = false) noexcept;
Result<LoggerPtr> try_rotating_file(
    std::string name, const std::filesystem::path &filename,
    std::size_t max_size_bytes = default_max_size_bytes,
    std::size_t max_files = default_max_files,
    bool rotate_on_open = false) noexcept;
LoggerPtr rotating_file(
    std::string name, const std::filesystem::path &filename,
    std::size_t max_size_bytes = default_max_size_bytes,
    std::size_t max_files = default_max_files,
    bool rotate_on_open = false) noexcept;

} // namespace sindre::general::log

namespace sindre::general::crashpad {
struct Options {
    std::filesystem::path handler;
    std::filesystem::path database;
    std::filesystem::path metrics_dir;
    std::string upload_url;
    std::map<std::string, std::string> annotations;
    std::vector<std::string> arguments;
    bool restartable = true;
    bool asynchronous = true;
    std::chrono::milliseconds startup_timeout{10000};
};

Result<void> start(const Options &options) noexcept;
}
