#include <sindre/general/diag.h>

#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <system_error>
#include <unordered_map>

#if defined(SINDRE_WITH_LOG)
#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#endif

#if defined(SINDRE_CRASHPAD_READY)
#include <base/files/file_path.h>
#include <client/crashpad_client.h>
#include <memory>
#endif

namespace sindre::general::diagnostics {

Result<void> check(bool condition, std::string_view message,
                   std::string_view context) noexcept {
    if (condition) return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                     std::string(message), std::string(context));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::bad_alloc &) {
        return Result<void>::failure(std::make_error_code(std::errc::not_enough_memory),
                                     "Not enough memory", "diagnostics.check");
    } catch (...) {
        return Result<void>::failure(std::make_error_code(std::errc::io_error),
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
    if (options.handler.empty() || options.database.empty())
        return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                     "Crashpad handler and database are required",
                                     "crashpad.start");
    std::error_code filesystem_error;
    if (!std::filesystem::is_regular_file(options.handler, filesystem_error))
        return Result<void>::failure(
            filesystem_error ? filesystem_error
                             : std::make_error_code(std::errc::no_such_file_or_directory),
            "Crashpad handler is not a regular file", "crashpad.start");
    if (!options.metrics_dir.empty()) {
        filesystem_error.clear();
        if (!std::filesystem::is_directory(options.metrics_dir, filesystem_error))
            return Result<void>::failure(
                filesystem_error ? filesystem_error
                                 : std::make_error_code(std::errc::not_a_directory),
                "Crashpad metrics directory is not valid", "crashpad.start");
    }
    if (options.startup_timeout.count() < 0 ||
        static_cast<unsigned long long>(options.startup_timeout.count()) >
            std::numeric_limits<unsigned int>::max())
        return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
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
        const bool asynchronous_start = false;
#endif
        const bool handler_started = candidate->StartHandler(
            base::FilePath(options.handler.native()),
            base::FilePath(options.database.native()),
            base::FilePath((options.metrics_dir.empty() ? options.database
                                                          : options.metrics_dir).native()),
            options.upload_url, options.annotations, options.arguments,
            options.restartable, asynchronous_start);
        if (!handler_started)
            return Result<void>::failure(std::make_error_code(std::errc::io_error),
                                         "Crashpad handler did not start",
                                         "crashpad.start");
#if defined(_WIN32)
        if (asynchronous_start &&
            !candidate->WaitForHandlerStart(
                static_cast<unsigned int>(options.startup_timeout.count())))
            return Result<void>::failure(std::make_error_code(std::errc::timed_out),
                                         "Crashpad handler did not become ready",
                                         "crashpad.start");
#else
        (void)options.startup_timeout;
#endif
        client = std::move(candidate);
        started = true;
        return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::bad_alloc &) {
        return Result<void>::failure(std::make_error_code(std::errc::not_enough_memory),
                                     "Not enough memory", "crashpad.start");
    } catch (const std::exception &error) {
        return Result<void>::failure(std::make_error_code(std::errc::io_error),
                                     error.what(), "crashpad.start");
    } catch (...) {
        return Result<void>::failure(std::make_error_code(std::errc::io_error),
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

#if defined(SINDRE_WITH_LOG)

namespace sindre::general::log {

struct Logger::Impl {
    std::shared_ptr<spdlog::logger> native;
    Level level = Level::info;
};

namespace {
std::mutex log_mutex;
std::mutex wrapper_mutex;
std::unordered_map<std::string, std::weak_ptr<Logger>> wrapper_cache;
bool log_initialized = false;
bool log_owned = false;
std::string log_name;

Result<void> log_failure(std::errc code, const char *message, const char *context) {
    return Result<void>::failure(std::make_error_code(code), message, context);
}

spdlog::level::level_enum to_native(Level level) noexcept {
    switch (level) {
    case Level::trace: return spdlog::level::trace;
    case Level::debug: return spdlog::level::debug;
    case Level::info: return spdlog::level::info;
    case Level::warning: return spdlog::level::warn;
    case Level::error: return spdlog::level::err;
    case Level::critical: return spdlog::level::critical;
    case Level::off: return spdlog::level::off;
    }
    return spdlog::level::info;
}

Level from_native(spdlog::level::level_enum level) noexcept {
    switch (level) {
    case spdlog::level::trace: return Level::trace;
    case spdlog::level::debug: return Level::debug;
    case spdlog::level::info: return Level::info;
    case spdlog::level::warn: return Level::warning;
    case spdlog::level::err: return Level::error;
    case spdlog::level::critical: return Level::critical;
    case spdlog::level::off: return Level::off;
    default: return Level::info;
    }
}

LoggerPtr wrap(std::shared_ptr<spdlog::logger> logger, Level level) {
    if (!logger) return {};
    const auto name = logger->name();
    std::lock_guard cache_lock(wrapper_mutex);
    if (auto existing = wrapper_cache[name].lock()) return existing;
    auto impl = std::make_shared<Logger::Impl>();
    impl->native = std::move(logger);
    impl->level = level;
    auto result = std::shared_ptr<Logger>(new Logger(std::move(impl)));
    wrapper_cache[name] = result;
    return result;
}

#if defined(_WIN32) && defined(SPDLOG_WCHAR_FILENAMES)
spdlog::filename_t native_filename(const std::filesystem::path &path) {
    return path.wstring();
}
#else
spdlog::filename_t native_filename(const std::filesystem::path &path) {
    return path.string();
}
#endif

} // namespace

Logger::Logger(std::shared_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
Logger::Logger(Logger &&other) noexcept = default;
Logger &Logger::operator=(Logger &&other) noexcept = default;
Logger::~Logger() = default;

std::string Logger::get_name() const {
    return impl_ && impl_->native ? impl_->native->name() : std::string{};
}

Level Logger::get_level() const noexcept {
    return impl_ ? impl_->level : Level::off;
}

Result<void> Logger::set_level(Level level) noexcept {
    if (!impl_ || !impl_->native)
        return log_failure(std::errc::invalid_argument, "Logger is not initialized",
                           "log.set_level");
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        impl_->native->set_level(to_native(level));
        impl_->level = level;
        return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<void>::failure(std::make_error_code(std::errc::io_error),
                                     error.what(), "log.set_level");
    } catch (...) {
        return log_failure(std::errc::io_error, "Logger level update failed",
                           "log.set_level");
    }
#endif
}

Result<void> Logger::write(Level level, std::string_view message) noexcept {
    if (!impl_ || !impl_->native)
        return log_failure(std::errc::invalid_argument, "Logger is not initialized",
                           "log.write");
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        impl_->native->log(to_native(level), "{}", std::string(message));
        return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<void>::failure(std::make_error_code(std::errc::io_error),
                                     error.what(), "log.write");
    } catch (...) {
        return log_failure(std::errc::io_error, "Logger write failed", "log.write");
    }
#endif
}

Result<void> Logger::flush() noexcept {
    if (!impl_ || !impl_->native)
        return log_failure(std::errc::invalid_argument, "Logger is not initialized",
                           "log.flush");
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        impl_->native->flush();
        return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (...) {
        return log_failure(std::errc::io_error, "Logger flush failed", "log.flush");
    }
#endif
}

void info(std::string_view message) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try { spdlog::info("{}", std::string(message)); } catch (...) {}
#else
    spdlog::info("{}", std::string(message));
#endif
}
void warning(std::string_view message) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try { spdlog::warn("{}", std::string(message)); } catch (...) {}
#else
    spdlog::warn("{}", std::string(message));
#endif
}
void error(std::string_view message) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try { spdlog::error("{}", std::string(message)); } catch (...) {}
#else
    spdlog::error("{}", std::string(message));
#endif
}

Result<void> set_level(Level level) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::lock_guard lock(log_mutex);
        spdlog::set_level(to_native(level));
        return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<void>::failure(std::make_error_code(std::errc::io_error),
                                     error.what(), "log.set_level");
    } catch (...) {
        return log_failure(std::errc::io_error, "Logger level update failed",
                           "log.set_level");
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
        if (!logger && !asynchronous) {
            logger = filename.empty()
                ? spdlog::stdout_color_mt(name)
                : spdlog::rotating_logger_mt(name, native_filename(filename),
                                             max_size_bytes, max_files, rotate_on_open);
        } else if (!logger && asynchronous) {
            if (!spdlog::thread_pool())
                spdlog::init_thread_pool(async_queue_size, async_worker_threads);
            auto pool = spdlog::thread_pool();
            if (!pool) return log_failure(std::errc::io_error,
                                          "Cannot create async logger thread pool", "log.init");
            std::vector<spdlog::sink_ptr> sinks;
            if (filename.empty())
                sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
            else
                sinks.emplace_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    native_filename(filename), max_size_bytes, max_files, rotate_on_open));
            const auto overflow = async_overflow == AsyncOverflowPolicy::overrun_oldest
                ? spdlog::async_overflow_policy::overrun_oldest
                : async_overflow == AsyncOverflowPolicy::discard_new
                    ? spdlog::async_overflow_policy::discard_new
                    : spdlog::async_overflow_policy::block;
            logger = std::make_shared<spdlog::async_logger>(
                name, sinks.begin(), sinks.end(), pool, overflow);
        }
        if (!logger) return log_failure(std::errc::io_error,
                                        "Cannot create logger", "log.init");
        logger->set_level(to_native(level));
        logger->set_pattern(std::string(pattern));
        spdlog::set_default_logger(logger);
        spdlog::set_level(to_native(level));
        log_name = logger->name();
        log_owned = owns_logger;
        log_initialized = true;
        return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::bad_alloc &) {
        return log_failure(std::errc::not_enough_memory, "Not enough memory", "log.init");
    } catch (const std::exception &error) {
        return Result<void>::failure(std::make_error_code(std::errc::io_error),
                                     error.what(), "log.init");
    } catch (...) {
        return log_failure(std::errc::io_error, "Logger initialization failed", "log.init");
    }
#endif
}

Result<LoggerPtr> create_logger(std::string name, Level level) noexcept {
    if (name.empty())
        return Result<LoggerPtr>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Logger name must not be empty", "log.create_logger");
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::lock_guard lock(log_mutex);
        if (auto existing = spdlog::get(name)) {
            const auto existing_level = from_native(existing->level());
            return Result<LoggerPtr>::success(wrap(std::move(existing), existing_level));
        }
        auto default_logger = spdlog::default_logger();
        if (!default_logger)
            return Result<LoggerPtr>::failure(std::make_error_code(std::errc::io_error),
                                              "Default logger is not available",
                                              "log.create_logger");
        auto logger = default_logger->clone(name);
        if (!logger)
            return Result<LoggerPtr>::failure(std::make_error_code(std::errc::io_error),
                                              "Cannot clone default logger",
                                              "log.create_logger");
        logger->set_level(to_native(level));
        spdlog::register_logger(logger);
        return Result<LoggerPtr>::success(wrap(std::move(logger), level));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::bad_alloc &) {
        return Result<LoggerPtr>::failure(std::make_error_code(std::errc::not_enough_memory),
                                          "Not enough memory", "log.create_logger");
    } catch (const std::exception &error) {
        return Result<LoggerPtr>::failure(std::make_error_code(std::errc::io_error),
                                          error.what(), "log.create_logger");
    } catch (...) {
        return Result<LoggerPtr>::failure(std::make_error_code(std::errc::io_error),
                                          "Logger creation failed", "log.create_logger");
    }
#endif
}

Result<void> initialize(LoggerPtr logger, Level level) noexcept {
    if (!logger) return init_log("sindre", level);
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::lock_guard lock(log_mutex);
        if (!logger->impl_ || !logger->impl_->native)
            return log_failure(std::errc::invalid_argument, "Logger is not initialized",
                               "log.initialize");
        if (log_initialized) {
            if (logger->get_name() == log_name) return Result<void>::success();
            return log_failure(std::errc::device_or_resource_busy,
                               "A different logger is already initialized", "log.initialize");
        }
        logger->impl_->native->set_level(to_native(level));
        logger->impl_->level = level;
        spdlog::set_default_logger(logger->impl_->native);
        spdlog::set_level(to_native(level));
        log_name = logger->get_name();
        log_owned = false;
        log_initialized = true;
        return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<void>::failure(std::make_error_code(std::errc::io_error),
                                     error.what(), "log.initialize");
    } catch (...) {
        return log_failure(std::errc::io_error, "Logger initialization failed",
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
    } catch (...) {
        return log_failure(std::errc::io_error, "Logger shutdown failed", "log.shutdown");
    }
#endif
}

Result<LoggerPtr> try_rotating_file(std::string name, std::string filename,
                                    std::size_t max_size_bytes, std::size_t max_files,
                                    bool rotate_on_open) noexcept {
    return try_rotating_file(std::move(name), std::filesystem::u8path(filename),
                             max_size_bytes, max_files, rotate_on_open);
}

Result<LoggerPtr> try_rotating_file(std::string name,
                                    const std::filesystem::path &filename,
                                    std::size_t max_size_bytes, std::size_t max_files,
                                    bool rotate_on_open) noexcept {
    if (name.empty() || filename.empty() || max_size_bytes == 0 || max_files == 0)
        return Result<LoggerPtr>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Invalid rotating logger options", "log.rotating_file");
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            native_filename(filename), max_size_bytes, max_files, rotate_on_open);
        auto logger = std::make_shared<spdlog::logger>(std::move(name), sink);
        if (!logger)
            return Result<LoggerPtr>::failure(std::make_error_code(std::errc::io_error),
                                              "Cannot create rotating logger", "log.rotating_file");
        logger->set_pattern(std::string(default_pattern));
        return Result<LoggerPtr>::success(wrap(std::move(logger), Level::info));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<LoggerPtr>::failure(std::make_error_code(std::errc::io_error),
                                          error.what(), "log.rotating_file");
    } catch (...) {
        return Result<LoggerPtr>::failure(std::make_error_code(std::errc::io_error),
                                          "Cannot create rotating logger", "log.rotating_file");
    }
#endif
}

LoggerPtr rotating_file(std::string name, std::string filename,
                        std::size_t max_size_bytes, std::size_t max_files,
                        bool rotate_on_open) noexcept {
    auto result = try_rotating_file(std::move(name), std::move(filename), max_size_bytes,
                                    max_files, rotate_on_open);
    return result ? result.value() : LoggerPtr{};
}

LoggerPtr rotating_file(std::string name, const char *filename,
                        std::size_t max_size_bytes, std::size_t max_files,
                        bool rotate_on_open) noexcept {
    if (!filename) return {};
    return rotating_file(std::move(name), std::string(filename), max_size_bytes,
                         max_files, rotate_on_open);
}

LoggerPtr rotating_file(std::string name, const std::filesystem::path &filename,
                        std::size_t max_size_bytes, std::size_t max_files,
                        bool rotate_on_open) noexcept {
    auto result = try_rotating_file(std::move(name), filename, max_size_bytes,
                                    max_files, rotate_on_open);
    return result ? result.value() : LoggerPtr{};
}

} // namespace sindre::general::log

#endif
