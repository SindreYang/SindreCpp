#include <sindre/general/diag.h>
#if defined(SINDRE_WITH_LOG)

#endif

#include <string>
#include <exception>
#if defined(SINDRE_CRASHPAD_READY)
#include <client/crashpad_client.h>
#include <base/files/file_path.h>
#endif

namespace sindre::general::diagnostics {

Result<void> check(bool condition, std::string_view message, std::string_view context) noexcept {
    if (condition) return Result<void>::success();
    return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                 std::string(message), std::string(context));
}

Stopwatch::Stopwatch() noexcept : started_(clock::now()) {}
void Stopwatch::reset() noexcept { started_ = clock::now(); }
std::chrono::nanoseconds Stopwatch::elapsed() const noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - started_);
}
double Stopwatch::elapsed_seconds() const noexcept {
    return std::chrono::duration<double>(clock::now() - started_).count();
}

} // namespace sindre::general::diagnostics
#if defined(SINDRE_WITH_CRASHPAD)

#endif

// Diagnostics implementation boundary.

namespace sindre::general::crashpad {

Result<void> start(const Options &options) noexcept {
#if defined(SINDRE_WITH_CRASHPAD) && defined(SINDRE_CRASHPAD_READY)
    if (options.handler.empty() || options.database.empty()) return Result<void>::failure(
        std::make_error_code(std::errc::invalid_argument), "Crashpad handler and database are required", "crashpad.start");
    try {
        ::crashpad::CrashpadClient client;
        const bool started = client.StartHandler(
            base::FilePath(options.handler.native()), base::FilePath(options.database.native()),
            base::FilePath((options.dumps.empty() ? options.database : options.dumps).native()),
            options.upload_url, options.annotations, options.arguments, options.restartable, options.asynchronous);
        if (!started) return Result<void>::failure(
            std::make_error_code(std::errc::io_error), "Crashpad handler did not start", "crashpad.start");
        return Result<void>::success();
    } catch (const std::exception &error) {
        return Result<void>::failure(std::make_error_code(std::errc::io_error), error.what(), "crashpad.start");
    } catch (...) {
        return Result<void>::failure(std::make_error_code(std::errc::io_error), "Unknown Crashpad failure", "crashpad.start");
    }
#else
    (void)options;
    return Result<void>::failure(std::make_error_code(std::errc::function_not_supported),
        "Crashpad support is not enabled", "crashpad.start");
#endif
}

} // namespace sindre::general::crashpad

// ---- merged from log.cpp ----

#if defined(SINDRE_WITH_LOG)

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
#include <mutex>

namespace sindre::general::log {

using Logger = spdlog::logger;
using LoggerPtr = std::shared_ptr<Logger>;
using Level = spdlog::level::level_enum;
namespace native = spdlog;

void set_level(Level level) { spdlog::set_level(level); }

::sindre::general::Result<void> initialize(
    LoggerPtr logger, Level level) noexcept {
    static std::mutex mutex;
    static bool initialized = false;
    std::lock_guard lock(mutex);
    if (initialized) return ::sindre::general::Result<void>::success();
#if defined(SINDRE_NO_EXCEPTIONS)
    if (!logger) logger = spdlog::stdout_color_mt("sindre");
    if (!logger) return ::sindre::general::Result<void>::failure(
        std::make_error_code(std::errc::io_error), "Cannot create logger", "log.initialize");
    logger->set_level(level); logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    spdlog::set_default_logger(std::move(logger)); spdlog::set_level(level); initialized = true;
    return ::sindre::general::Result<void>::success();
#else
    try {
        if (!logger) logger = spdlog::stdout_color_mt("sindre");
        logger->set_level(level);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        spdlog::set_default_logger(std::move(logger));
        spdlog::set_level(level);
        initialized = true;
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
    return rotating_file(std::move(name), std::string(filename), max_size_bytes,
                         max_files, rotate_on_open);
}

::sindre::general::Result<LoggerPtr> try_rotating_file(
    std::string name, const std::filesystem::path &filename,
    std::size_t max_size_bytes, std::size_t max_files,
    bool rotate_on_open) noexcept {
    if (name.empty() || filename.empty() || max_size_bytes == 0 || max_files == 0)
        return ::sindre::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid rotating logger options",
            "log.rotating_file");
    try {
        auto logger = spdlog::rotating_logger_mt(std::move(name), filename_from_path(filename),
                                                 max_size_bytes, max_files, rotate_on_open);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        return ::sindre::general::Result<LoggerPtr>::success(std::move(logger));
    } catch (const std::exception &error) {
        return ::sindre::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "log.rotating_file");
    } catch (...) {
        return ::sindre::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::io_error), "Cannot create rotating logger", "log.rotating_file");
    }
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
