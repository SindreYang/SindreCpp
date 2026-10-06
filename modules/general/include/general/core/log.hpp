#pragma once

#if !defined(SINDRECPP_WITH_LOG)
#error "Enable SINDRECPP_WITH_LOG and link SindreCpp::General before including this header."
#endif

#include <general/core/async.hpp>
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

namespace sindrecpp::general::log {

using Logger = spdlog::logger;
using LoggerPtr = std::shared_ptr<Logger>;
using Level = spdlog::level::level_enum;
namespace native = spdlog;

template <class... Args>
inline void info(Args&&... args) { spdlog::info(std::forward<Args>(args)...); }
template <class... Args>
inline void warning(Args&&... args) { spdlog::warn(std::forward<Args>(args)...); }
template <class... Args>
inline void error(Args&&... args) { spdlog::error(std::forward<Args>(args)...); }
inline void set_level(Level level) { spdlog::set_level(level); }

inline ::sindrecpp::general::Result<void> initialize(
    LoggerPtr logger = {}, Level level = spdlog::level::info) noexcept {
    static std::mutex mutex;
    static bool initialized = false;
    std::lock_guard lock(mutex);
    if (initialized) return ::sindrecpp::general::Result<void>::success();
#if defined(SINDRECPP_NO_EXCEPTIONS)
    if (!logger) logger = spdlog::stdout_color_mt("sindrecpp");
    if (!logger) return ::sindrecpp::general::Result<void>::failure(
        std::make_error_code(std::errc::io_error), "Cannot create logger", "log.initialize");
    logger->set_level(level); logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    spdlog::set_default_logger(std::move(logger)); spdlog::set_level(level); initialized = true;
    return ::sindrecpp::general::Result<void>::success();
#else
    try {
        if (!logger) logger = spdlog::stdout_color_mt("sindrecpp");
        logger->set_level(level);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        spdlog::set_default_logger(std::move(logger));
        spdlog::set_level(level);
        initialized = true;
        return ::sindrecpp::general::Result<void>::success();
    } catch (const std::exception &error) {
        return ::sindrecpp::general::Result<void>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "log.initialize");
    } catch (...) {
        return ::sindrecpp::general::Result<void>::failure(
            std::make_error_code(std::errc::io_error), "Unknown logger initialization failure",
            "log.initialize");
    }
#endif
}

#if defined(_WIN32) && defined(SPDLOG_WCHAR_FILENAMES)
inline spdlog::filename_t filename_from_utf8(std::string_view filename) {
    return std::filesystem::u8path(filename).wstring();
}
inline spdlog::filename_t filename_from_path(const std::filesystem::path& path) {
    return path.wstring();
}
#else
inline spdlog::filename_t filename_from_utf8(std::string_view filename) {
    return std::string(filename);
}
inline spdlog::filename_t filename_from_path(const std::filesystem::path& path) {
    return path.string();
}
#endif

inline ::sindrecpp::general::Result<LoggerPtr> try_rotating_file(
    std::string name, std::string filename, std::size_t max_size_bytes = 10 * 1024 * 1024,
    std::size_t max_files = 5, bool rotate_on_open = false) noexcept {
    if (name.empty() || filename.empty() || max_size_bytes == 0 || max_files == 0)
        return ::sindrecpp::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid rotating logger options",
            "log.rotating_file");
#if defined(SINDRECPP_NO_EXCEPTIONS)
    auto logger = spdlog::rotating_logger_mt(std::move(name), filename_from_utf8(filename),
                                             max_size_bytes, max_files, rotate_on_open);
    if (!logger) return ::sindrecpp::general::Result<LoggerPtr>::failure(
        std::make_error_code(std::errc::io_error), "Cannot create rotating logger", "log.rotating_file");
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    return ::sindrecpp::general::Result<LoggerPtr>::success(std::move(logger));
#else
    try {
        auto logger = spdlog::rotating_logger_mt(std::move(name), filename_from_utf8(filename),
                                                 max_size_bytes, max_files, rotate_on_open);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        return ::sindrecpp::general::Result<LoggerPtr>::success(std::move(logger));
    } catch (const std::exception &error) {
        return ::sindrecpp::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "log.rotating_file");
    } catch (...) {
        return ::sindrecpp::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::io_error), "Cannot create rotating logger", "log.rotating_file");
    }
#endif
}

inline LoggerPtr rotating_file(std::string name, std::string filename,
                               std::size_t max_size_bytes = 10 * 1024 * 1024,
                               std::size_t max_files = 5,
                               bool rotate_on_open = false) noexcept {
    auto result = try_rotating_file(std::move(name), std::move(filename), max_size_bytes, max_files, rotate_on_open);
    return result ? result.value() : LoggerPtr{};
}

inline LoggerPtr rotating_file(std::string name, const char* filename,
                               std::size_t max_size_bytes = 10 * 1024 * 1024,
                               std::size_t max_files = 5,
                               bool rotate_on_open = false) noexcept {
    if (!filename) return {};
    return rotating_file(std::move(name), std::string(filename), max_size_bytes,
                         max_files, rotate_on_open);
}

inline ::sindrecpp::general::Result<LoggerPtr> try_rotating_file(
    std::string name, const std::filesystem::path &filename,
    std::size_t max_size_bytes = 10 * 1024 * 1024, std::size_t max_files = 5,
    bool rotate_on_open = false) noexcept {
    if (name.empty() || filename.empty() || max_size_bytes == 0 || max_files == 0)
        return ::sindrecpp::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid rotating logger options",
            "log.rotating_file");
    try {
        auto logger = spdlog::rotating_logger_mt(std::move(name), filename_from_path(filename),
                                                 max_size_bytes, max_files, rotate_on_open);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        return ::sindrecpp::general::Result<LoggerPtr>::success(std::move(logger));
    } catch (const std::exception &error) {
        return ::sindrecpp::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "log.rotating_file");
    } catch (...) {
        return ::sindrecpp::general::Result<LoggerPtr>::failure(
            std::make_error_code(std::errc::io_error), "Cannot create rotating logger", "log.rotating_file");
    }
}

inline LoggerPtr rotating_file(std::string name, const std::filesystem::path& filename,
                               std::size_t max_size_bytes = 10 * 1024 * 1024,
                               std::size_t max_files = 5,
                               bool rotate_on_open = false) noexcept {
    auto result = try_rotating_file(std::move(name), filename, max_size_bytes, max_files, rotate_on_open);
    return result ? result.value() : LoggerPtr{};
}

} // namespace sindrecpp::general::log
