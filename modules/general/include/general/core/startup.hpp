#pragma once

#include <general/core/async.hpp>
#include <general/core/system.hpp>
#include <filesystem>
#include <fstream>

namespace sindrecpp::general::startup {

inline ::sindrecpp::general::Result<std::filesystem::path> location(std::string_view name) noexcept {
    if (name.empty()) return ::sindrecpp::general::Result<std::filesystem::path>::failure(
        std::make_error_code(std::errc::invalid_argument), "Startup name is empty", "startup.location");
#if defined(_WIN32)
    auto base = ::sindrecpp::general::system::environment("APPDATA");
    if (!base) return ::sindrecpp::general::Result<std::filesystem::path>::failure(base.error());
    return ::sindrecpp::general::Result<std::filesystem::path>::success(
        std::filesystem::path(base.value()) / "Microsoft/Windows/Start Menu/Programs/Startup" /
        (std::string(name) + ".cmd"));
#else
    auto home = ::sindrecpp::general::system::environment("HOME");
    if (!home) return ::sindrecpp::general::Result<std::filesystem::path>::failure(home.error());
    return ::sindrecpp::general::Result<std::filesystem::path>::success(
        std::filesystem::path(home.value()) / ".config/systemd/user" / (std::string(name) + ".service"));
#endif
}

inline ::sindrecpp::general::Result<void> enable(std::string name, std::string command) noexcept {
    if (command.empty()) return ::sindrecpp::general::Result<void>::failure(
        std::make_error_code(std::errc::invalid_argument), "Startup command is empty", "startup.enable");
    auto path = location(name); if (!path) return ::sindrecpp::general::Result<void>::failure(path.error());
#if defined(SINDRECPP_NO_EXCEPTIONS)
    std::filesystem::create_directories(path.value().parent_path());
    std::ofstream output(path.value(), std::ios::trunc);
#if defined(_WIN32)
    output << "@echo off\n" << command << "\n";
#else
    output << "[Unit]\nDescription=" << name << "\nAfter=graphical-session.target\n"
           << "[Service]\nType=simple\nExecStart=" << command << "\n"
           << "[Install]\nWantedBy=default.target\n";
#endif
    if (!output) return ::sindrecpp::general::Result<void>::failure(
        std::make_error_code(std::errc::permission_denied), "Cannot write startup entry", "startup.enable");
    return ::sindrecpp::general::Result<void>::success();
#else
    try {
        std::filesystem::create_directories(path.value().parent_path());
        std::ofstream output(path.value(), std::ios::trunc);
#if defined(_WIN32)
        output << "@echo off\n" << command << "\n";
#else
        output << "[Unit]\nDescription=" << name << "\nAfter=graphical-session.target\n"
               << "[Service]\nType=simple\nExecStart=" << command << "\n"
               << "[Install]\nWantedBy=default.target\n";
#endif
        if (!output) return ::sindrecpp::general::Result<void>::failure(
            std::make_error_code(std::errc::permission_denied), "Cannot write startup entry", "startup.enable");
        return ::sindrecpp::general::Result<void>::success();
    } catch (const std::exception &error) { return ::sindrecpp::general::Result<void>::failure(
        std::make_error_code(std::errc::io_error), error.what(), "startup.enable"); }
#endif
}

inline ::sindrecpp::general::Result<void> disable(std::string_view name) noexcept {
    auto path = location(name); if (!path) return ::sindrecpp::general::Result<void>::failure(path.error());
    std::error_code code; std::filesystem::remove(path.value(), code);
    if (code) return ::sindrecpp::general::Result<void>::failure(code, "Cannot remove startup entry", "startup.disable");
    return ::sindrecpp::general::Result<void>::success();
}

} // namespace sindrecpp::general::startup
