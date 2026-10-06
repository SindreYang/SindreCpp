#pragma once

#include <general/core/async.hpp>
#include <string>
#include <string_view>

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace sindrecpp::general::desktop {

inline bool is_elevated() noexcept {
#if defined(_WIN32)
    return false;
#else
    return ::geteuid() == 0;
#endif
}

inline ::sindrecpp::general::Result<void> request_elevation(std::string_view) noexcept {
    return ::sindrecpp::general::Result<void>::failure(
        std::make_error_code(std::errc::function_not_supported),
        "Elevation requires an application-specific launcher", "desktop.elevation");
}

inline ::sindrecpp::general::Result<void> clipboard_set(std::string_view) noexcept {
    return ::sindrecpp::general::Result<void>::failure(
        std::make_error_code(std::errc::function_not_supported), "Clipboard backend is not enabled", "desktop.clipboard");
}
inline ::sindrecpp::general::Result<std::string> clipboard_get() noexcept {
    return ::sindrecpp::general::Result<std::string>::failure(
        std::make_error_code(std::errc::function_not_supported), "Clipboard backend is not enabled", "desktop.clipboard");
}
inline ::sindrecpp::general::Result<void> notify(std::string_view, std::string_view) noexcept {
    return ::sindrecpp::general::Result<void>::failure(
        std::make_error_code(std::errc::function_not_supported), "Notification backend is not enabled", "desktop.notify");
}
inline ::sindrecpp::general::Result<void> tray_start(std::string_view) noexcept {
    return ::sindrecpp::general::Result<void>::failure(
        std::make_error_code(std::errc::function_not_supported), "Tray backend is not enabled", "desktop.tray");
}

} // namespace sindrecpp::general::desktop
