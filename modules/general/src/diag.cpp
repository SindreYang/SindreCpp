#include <sindre/general/diag.h>

#include <string>
#include <exception>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <system_error>
#if defined(SINDRE_CRASHPAD_READY)
#include <client/crashpad_client.h>
#include <base/files/file_path.h>
#endif

namespace sindre::general::diagnostics {

Result<void> check(bool condition, std::string_view message, std::string_view context) noexcept {
    if (condition) return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                     std::string(message), std::string(context));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::bad_alloc &) {
        return Result<void>::failure(
            std::make_error_code(std::errc::not_enough_memory),
            "Not enough memory", "diagnostics.check");
    } catch (...) {
        return Result<void>::failure(
            std::make_error_code(std::errc::io_error),
            "Diagnostic check failed", "diagnostics.check");
    }
#endif
}

} // namespace sindre::general::diagnostics

namespace sindre::general::crashpad {

Result<void> start(const Options &options) noexcept {
#if defined(SINDRE_WITH_CRASHPAD) && defined(SINDRE_CRASHPAD_READY)
    static std::mutex mutex;
    static bool started = false;
    static std::unique_ptr<::crashpad::CrashpadClient> client;
    if (options.handler.empty() || options.database.empty()) return Result<void>::failure(
        std::make_error_code(std::errc::invalid_argument),
        "Crashpad handler and database are required", "crashpad.start");

    std::error_code filesystem_error;
    if (!std::filesystem::is_regular_file(options.handler, filesystem_error))
        return Result<void>::failure(
            filesystem_error ? filesystem_error : std::make_error_code(std::errc::no_such_file_or_directory),
            "Crashpad handler is not a regular file", "crashpad.start");
    if (!options.metrics_dir.empty()) {
        filesystem_error.clear();
        if (!std::filesystem::is_directory(options.metrics_dir, filesystem_error))
            return Result<void>::failure(
                filesystem_error ? filesystem_error : std::make_error_code(std::errc::not_a_directory),
                "Crashpad metrics directory is not valid", "crashpad.start");
    }
    if (options.startup_timeout.count() < 0 ||
        static_cast<unsigned long long>(options.startup_timeout.count()) >
            std::numeric_limits<unsigned int>::max())
        return Result<void>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "Invalid Crashpad startup timeout", "crashpad.start");

#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::lock_guard lock(mutex);
        if (started) return Result<void>::success();
        auto candidate = std::make_unique<::crashpad::CrashpadClient>();
#if defined(_WIN32)
        const bool asynchronous_start = options.asynchronous;
#else
        // Crashpad 2022 的 Linux 实现不提供 Windows 的异步就绪等待接口。
        // 统一使用同步启动，保证 Linux 上的公共接口仍然具有确定语义。
        const bool asynchronous_start = false;
#endif
        const bool handler_started = candidate->StartHandler(
            base::FilePath(options.handler.native()), base::FilePath(options.database.native()),
            base::FilePath((options.metrics_dir.empty() ? options.database : options.metrics_dir).native()),
            options.upload_url, options.annotations, options.arguments,
            options.restartable, asynchronous_start);
        if (!handler_started) return Result<void>::failure(
            std::make_error_code(std::errc::io_error),
            "Crashpad handler did not start", "crashpad.start");
#if defined(_WIN32)
        if (asynchronous_start &&
            !candidate->WaitForHandlerStart(static_cast<unsigned int>(options.startup_timeout.count())))
            return Result<void>::failure(
                std::make_error_code(std::errc::timed_out),
                "Crashpad handler did not become ready", "crashpad.start");
#else
        (void)options.startup_timeout;
#endif
        client = std::move(candidate);
        started = true;
        return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::bad_alloc &) {
        return Result<void>::failure(
            std::make_error_code(std::errc::not_enough_memory),
            "Not enough memory", "crashpad.start");
    } catch (const std::exception &error) {
        return Result<void>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "crashpad.start");
    } catch (...) {
        return Result<void>::failure(
            std::make_error_code(std::errc::io_error),
            "Unknown Crashpad failure", "crashpad.start");
    }
#endif
#else
    (void)options;
    return Result<void>::failure(std::make_error_code(std::errc::function_not_supported),
        "Crashpad support is not enabled", "crashpad.start");
#endif
}

} // namespace sindre::general::crashpad

// ---- merged from log.cpp ----

#if defined(SINDRE_WITH_LOG)

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <mutex>

namespace sindre::general::log {

using Logger = spdlog::logger;
using LoggerPtr = std::shared_ptr<Logger>;
using Level = spdlog::level::level_enum;
namespace native = spdlog;

namespace {

std::mutex log_mutex;
bool log_initialized = false;
bool log_owned = false;
std::string log_name;

Result<void> log_failure(std::errc code, const char *message, const char *context) {
    return Result<void>::failure(std::make_error_code(code), message, context);
}

} // namespace

spdlog::filename_t filename_from_path(const std::filesystem::path &path);

Result<void> set_level(Level level) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::lock_guard lock(log_mutex);
        spdlog::set_level(level);
        return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::bad_alloc &) {
        return log_failure(std::errc::not_enough_memory, "Not enough memory", "log.set_level");
    } catch (const std::exception &error) {
        return Result<void>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "log.set_level");
    } catch (...) {
        return log_failure(std::errc::io_error, "Unknown logger level failure", "log.set_level");
    }
#endif
}

Result<void> init_log(std::string name, Level level, std::string_view pattern,
                      std::filesystem::path filename, std::size_t max_size_bytes,
                      std::size_t max_files, bool rotate_on_open, bool asynchronous,
                      std::size_t async_queue_size, std::size_t async_worker_threads,
                      AsyncOverflowPolicy async_overflow) noexcept {
    if (name.empty() || pattern.empty() ||
        (!filename.empty() && (max_size_bytes == 0 || max_files == 0)) ||
        (asynchronous && (async_queue_size == 0 || async_worker_threads == 0)))
        return log_failure(std::errc::invalid_argument, "Invalid logger options", "log.init");

#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::lock_guard lock(log_mutex);
        if (log_initialized) {
            if (name == log_name) return Result<void>::success();
            return log_failure(std::errc::device_or_resource_busy,
                               "A different logger is already initialized", "log.init");
        }
        auto logger = spdlog::get(name);
        const bool owns_logger = !logger;
        if (logger && asynchronous &&
            !std::dynamic_pointer_cast<spdlog::async_logger>(logger))
            return log_failure(
                std::errc::device_or_resource_busy,
                "An existing synchronous logger has the requested name", "log.init");
        if (!logger) {
            if (!asynchronous) {
                logger = filename.empty()
                    ? spdlog::stdout_color_mt(name)
                    : spdlog::rotating_logger_mt(
                        name, filename_from_path(filename), max_size_bytes, max_files, rotate_on_open);
            } else {
                auto pool = spdlog::thread_pool();
                if (!pool) {
                    spdlog::init_thread_pool(async_queue_size, async_worker_threads);
                    pool = spdlog::thread_pool();
                }
                if (!pool) return log_failure(
                    std::errc::io_error, "Cannot create async logger thread pool", "log.init");

                std::vector<spdlog::sink_ptr> sinks;
                if (filename.empty()) {
                    sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
                } else {
                    sinks.emplace_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                        filename_from_path(filename), max_size_bytes, max_files, rotate_on_open));
                }

                const auto overflow_policy = [&] {
                    switch (async_overflow) {
                    case AsyncOverflowPolicy::overrun_oldest:
                        return spdlog::async_overflow_policy::overrun_oldest;
                    case AsyncOverflowPolicy::discard_new:
                        return spdlog::async_overflow_policy::discard_new;
                    case AsyncOverflowPolicy::block:
                    default:
                        return spdlog::async_overflow_policy::block;
                    }
                }();
                logger = std::make_shared<spdlog::async_logger>(
                    std::move(name), sinks.begin(), sinks.end(), pool, overflow_policy);
            }
        }
        if (!logger) return log_failure(
            std::errc::io_error, "Cannot create logger", "log.init");
        log_name = logger->name();
        logger->set_level(level);
        logger->set_pattern(std::string(pattern));
        spdlog::set_default_logger(std::move(logger));
        spdlog::set_level(level);
        log_owned = owns_logger;
        log_initialized = true;
        return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::bad_alloc &) {
        return log_failure(std::errc::not_enough_memory, "Not enough memory", "log.init");
    } catch (const std::exception &error) {
        return Result<void>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "log.init");
    } catch (...) {
        return log_failure(std::errc::io_error, "Unknown logger initialization failure", "log.init");
    }
#endif
}

Result<LoggerPtr> create_logger(std::string name, Level level) noexcept {
    if (name.empty()) {
        return Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "Logger name must not be empty", "log.create_logger");
    }

#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::lock_guard lock(log_mutex);
        if (auto existing = spdlog::get(name))
            return Result<LoggerPtr>::success(std::move(existing));

        const auto default_logger = spdlog::default_logger();
        if (!default_logger) {
            return Result<LoggerPtr>::failure(
                std::make_error_code(std::errc::io_error),
                "Default logger is not available", "log.create_logger");
        }

        auto logger = default_logger->clone(std::move(name));
        if (!logger) {
            return Result<LoggerPtr>::failure(
                std::make_error_code(std::errc::io_error),
                "Cannot clone default logger", "log.create_logger");
        }
        logger->set_level(level);
        spdlog::register_logger(logger);
        return Result<LoggerPtr>::success(std::move(logger));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::bad_alloc &) {
        return Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::not_enough_memory),
            "Not enough memory", "log.create_logger");
    } catch (const std::exception &error) {
        return Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::io_error), error.what(),
            "log.create_logger");
    } catch (...) {
        return Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::io_error),
            "Unknown logger creation failure", "log.create_logger");
    }
#endif
}

::sindre::general::Result<void> initialize(
    LoggerPtr logger, Level level) noexcept {
    if (!logger) return init_log("sindre", level);
#if defined(SINDRE_NO_EXCEPTIONS)
    std::lock_guard lock(log_mutex);
    if (log_initialized) {
        if (logger->name() == log_name) return ::sindre::general::Result<void>::success();
        return log_failure(std::errc::device_or_resource_busy,
                           "A different logger is already initialized", "log.initialize");
    }
    logger->set_level(level); logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    log_name = logger->name();
    spdlog::set_default_logger(std::move(logger)); spdlog::set_level(level); log_initialized = true;
    log_owned = false;
    return ::sindre::general::Result<void>::success();
#else
    try {
        std::lock_guard lock(log_mutex);
        if (log_initialized) {
            if (logger->name() == log_name) return ::sindre::general::Result<void>::success();
            return log_failure(std::errc::device_or_resource_busy,
                               "A different logger is already initialized", "log.initialize");
        }
        logger->set_level(level);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        log_name = logger->name();
        spdlog::set_default_logger(std::move(logger));
        spdlog::set_level(level);
        log_owned = false;
        log_initialized = true;
        return ::sindre::general::Result<void>::success();
    } catch (const std::exception &error) {
        return ::sindre::general::Result<void>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "log.initialize");
    } catch (...) {
        return ::sindre::general::Result<void>::failure(
            std::make_error_code(std::errc::io_error), "Unknown logger initialization failure",
            "log.initialize");
    }
#endif
}

Result<void> shutdown() noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::lock_guard lock(log_mutex);
        if (log_initialized && log_owned && !log_name.empty()) {
            if (auto logger = spdlog::get(log_name)) logger->flush();
            spdlog::drop(log_name);
        }
        log_name.clear();
        log_owned = false;
        log_initialized = false;
        return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::bad_alloc &) {
        return log_failure(std::errc::not_enough_memory, "Not enough memory", "log.shutdown");
    } catch (const std::exception &error) {
        return Result<void>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "log.shutdown");
    } catch (...) {
        return log_failure(std::errc::io_error, "Unknown logger shutdown failure", "log.shutdown");
    }
#endif
}

#if defined(_WIN32) && defined(SPDLOG_WCHAR_FILENAMES)
spdlog::filename_t filename_from_utf8(std::string_view filename) {
    return std::filesystem::u8path(filename).wstring();
}
spdlog::filename_t filename_from_path(const std::filesystem::path& path) {
    return path.wstring();
}
#else
spdlog::filename_t filename_from_utf8(std::string_view filename) {
    return std::string(filename);
}
spdlog::filename_t filename_from_path(const std::filesystem::path& path) {
    return path.string();
}
#endif

::sindre::general::Result<LoggerPtr> try_rotating_file(
    std::string name, std::string filename, std::size_t max_size_bytes,
    std::size_t max_files, bool rotate_on_open) noexcept {
    if (name.empty() || filename.empty() || max_size_bytes == 0 || max_files == 0)
        return ::sindre::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid rotating logger options",
            "log.rotating_file");
#if defined(SINDRE_NO_EXCEPTIONS)
    auto logger = spdlog::rotating_logger_mt(std::move(name), filename_from_utf8(filename),
                                             max_size_bytes, max_files, rotate_on_open);
    if (!logger) return ::sindre::general::Result<LoggerPtr>::failure(
        std::make_error_code(std::errc::io_error), "Cannot create rotating logger", "log.rotating_file");
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    return ::sindre::general::Result<LoggerPtr>::success(std::move(logger));
#else
    try {
        auto logger = spdlog::rotating_logger_mt(std::move(name), filename_from_utf8(filename),
                                                 max_size_bytes, max_files, rotate_on_open);
        if (!logger) return ::sindre::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::io_error), "Cannot create rotating logger", "log.rotating_file");
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        return ::sindre::general::Result<LoggerPtr>::success(std::move(logger));
    } catch (const std::exception &error) {
        return ::sindre::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "log.rotating_file");
    } catch (...) {
        return ::sindre::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::io_error), "Cannot create rotating logger", "log.rotating_file");
    }
#endif
}

LoggerPtr rotating_file(std::string name, std::string filename,
                               std::size_t max_size_bytes,
                               std::size_t max_files,
                               bool rotate_on_open) noexcept {
    auto result = try_rotating_file(std::move(name), std::move(filename), max_size_bytes, max_files, rotate_on_open);
    return result ? result.value() : LoggerPtr{};
}

LoggerPtr rotating_file(std::string name, const char* filename,
                                std::size_t max_size_bytes,
                                std::size_t max_files,
                                bool rotate_on_open) noexcept {
    if (!filename) return {};
#if defined(SINDRE_NO_EXCEPTIONS)
    return rotating_file(std::move(name), std::string(filename), max_size_bytes,
                         max_files, rotate_on_open);
#else
    try {
        return rotating_file(std::move(name), std::string(filename), max_size_bytes,
                             max_files, rotate_on_open);
    } catch (...) {
        return {};
    }
#endif
}

::sindre::general::Result<LoggerPtr> try_rotating_file(
    std::string name, const std::filesystem::path &filename,
    std::size_t max_size_bytes, std::size_t max_files,
    bool rotate_on_open) noexcept {
    if (name.empty() || filename.empty() || max_size_bytes == 0 || max_files == 0)
        return ::sindre::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid rotating logger options",
            "log.rotating_file");
#if defined(SINDRE_NO_EXCEPTIONS)
    auto logger = spdlog::rotating_logger_mt(std::move(name), filename_from_path(filename),
                                             max_size_bytes, max_files, rotate_on_open);
    if (!logger) return ::sindre::general::Result<LoggerPtr>::failure(
        std::make_error_code(std::errc::io_error), "Cannot create rotating logger", "log.rotating_file");
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    return ::sindre::general::Result<LoggerPtr>::success(std::move(logger));
#else
    try {
        auto logger = spdlog::rotating_logger_mt(std::move(name), filename_from_path(filename),
                                                 max_size_bytes, max_files, rotate_on_open);
        if (!logger) return ::sindre::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::io_error), "Cannot create rotating logger", "log.rotating_file");
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        return ::sindre::general::Result<LoggerPtr>::success(std::move(logger));
    } catch (const std::exception &error) {
        return ::sindre::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "log.rotating_file");
    } catch (...) {
        return ::sindre::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::io_error), "Cannot create rotating logger", "log.rotating_file");
    }
#endif
}

LoggerPtr rotating_file(std::string name, const std::filesystem::path& filename,
                               std::size_t max_size_bytes,
                               std::size_t max_files,
                               bool rotate_on_open) noexcept {
    auto result = try_rotating_file(std::move(name), filename, max_size_bytes, max_files, rotate_on_open);
    return result ? result.value() : LoggerPtr{};
}

} // namespace sindre::general::log

#endif
