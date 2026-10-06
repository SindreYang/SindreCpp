#pragma once

#include <general/core/async.hpp>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#if defined(SINDRECPP_CRASHPAD_READY)
#include <client/crashpad_client.h>
#include <base/files/file_path.h>
#endif

namespace sindrecpp::general::crashpad {

struct Options {
    std::filesystem::path handler;
    std::filesystem::path database;
    std::filesystem::path dumps;
    std::string upload_url;
    std::map<std::string, std::string> annotations;
    std::vector<std::string> arguments;
    bool restartable = true;
    bool asynchronous = true;
};

inline ::sindrecpp::general::Result<void> start(const Options &options) noexcept {
#if defined(SINDRECPP_WITH_CRASHPAD)
#if defined(SINDRECPP_CRASHPAD_READY)
    if (options.handler.empty() || options.database.empty())
        return ::sindrecpp::general::Result<void>::failure(
            std::make_error_code(std::errc::invalid_argument), "Crashpad handler and database are required", "crashpad.start");
#if defined(SINDRECPP_NO_EXCEPTIONS)
    crashpad::CrashpadClient client;
    const bool started = client.StartHandler(
        base::FilePath(options.handler.native()), base::FilePath(options.database.native()),
        base::FilePath((options.dumps.empty() ? options.database : options.dumps).native()),
        options.upload_url, options.annotations, options.arguments, options.restartable, options.asynchronous);
    if (!started) return ::sindrecpp::general::Result<void>::failure(
        std::make_error_code(std::errc::io_error), "Crashpad handler did not start", "crashpad.start");
    return ::sindrecpp::general::Result<void>::success();
#else
    try {
        crashpad::CrashpadClient client;
        const bool started = client.StartHandler(
            base::FilePath(options.handler.native()), base::FilePath(options.database.native()),
            base::FilePath((options.dumps.empty() ? options.database : options.dumps).native()),
            options.upload_url, options.annotations, options.arguments, options.restartable, options.asynchronous);
        if (!started) return ::sindrecpp::general::Result<void>::failure(
            std::make_error_code(std::errc::io_error), "Crashpad handler did not start", "crashpad.start");
        return ::sindrecpp::general::Result<void>::success();
    } catch (const std::exception &error) {
        return ::sindrecpp::general::Result<void>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "crashpad.start");
    } catch (...) {
        return ::sindrecpp::general::Result<void>::failure(
            std::make_error_code(std::errc::io_error), "Unknown Crashpad failure", "crashpad.start");
    }
#endif
#else
    (void)options;
    return ::sindrecpp::general::Result<void>::failure(
        std::make_error_code(std::errc::function_not_supported), "Crashpad SDK target is not ready", "crashpad.start");
#endif
#else
    (void)options;
    return ::sindrecpp::general::Result<void>::failure(
        std::make_error_code(std::errc::function_not_supported), "Crashpad support is not enabled",
        "crashpad.start");
#endif
}

} // namespace sindrecpp::general::crashpad
