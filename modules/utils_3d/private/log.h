#pragma once

/// @brief Utils_3d 的私有模块日志适配；不把 spdlog 暴露到公共头。

#include <string>
#include <string_view>

#if defined(SINDRE_WITH_LOG)
#include <sindre/general/diag.h>
#endif

namespace sindre::utils_3d::detail::logging {

#if defined(SINDRE_WITH_LOG)

inline ::sindre::general::log::LoggerPtr module_logger() noexcept {
    static const auto logger = []() noexcept {
        auto result = ::sindre::general::log::create_logger("sindre.utils_3d");
        if (result) return std::move(result).value();
        return ::sindre::general::log::LoggerPtr{};
    }();
    return logger;
}

inline void error(std::string_view context, std::string_view message) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        if (const auto logger = module_logger())
            (void)logger->write(::sindre::general::log::Level::error,
                                "[" + std::string(context) + "] " + std::string(message));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (...) {
    }
#endif
}

inline void warning(std::string_view context, std::string_view message) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        if (const auto logger = module_logger())
            (void)logger->write(::sindre::general::log::Level::warning,
                                "[" + std::string(context) + "] " + std::string(message));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (...) {
    }
#endif
}

#else

inline void error(std::string_view, std::string_view) noexcept {}
inline void warning(std::string_view, std::string_view) noexcept {}

#endif

} // namespace sindre::utils_3d::detail::logging
