#pragma once
#include <sindre/general/core.h>
#include <chrono>
#include <string_view>
#include <utility>
namespace sindre::general::diagnostics {
Result<void> check(bool condition, std::string_view message, std::string_view context = "assert") noexcept;
}
#if defined(SINDRE_WITH_LOG)
#include <spdlog/spdlog.h>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
namespace sindre::general::log {
using Logger = spdlog::logger;
using LoggerPtr = std::shared_ptr<Logger>;
using Level = spdlog::level::level_enum;
namespace native = spdlog; // Compatibility escape hatch; new code should use this API.

inline constexpr std::string_view default_pattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] %v";
inline constexpr std::size_t default_max_size_bytes = 10 * 1024 * 1024;
inline constexpr std::size_t default_max_files = 5;
inline constexpr std::size_t default_async_queue_size = 8192;
inline constexpr std::size_t default_async_worker_threads = 1;

enum class AsyncOverflowPolicy {
    block,
    overrun_oldest,
    discard_new
};

template <class... Args>
inline void info(Args &&...args) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        spdlog::info(std::forward<Args>(args)...);
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (...) {}
#endif
}
template <class... Args>
inline void warning(Args &&...args) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        spdlog::warn(std::forward<Args>(args)...);
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (...) {}
#endif
}
template <class... Args>
inline void error(Args &&...args) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        spdlog::error(std::forward<Args>(args)...);
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (...) {}
#endif
}

Result<void> set_level(Level level) noexcept;
/// @brief 初始化全局日志；第一个参数是 logger name。
///
/// `filename` 为空时创建彩色控制台 logger；非空时创建按大小轮转的文件 logger。
Result<void> init_log(
    std::string name = "sindre", Level level = spdlog::level::info,
    std::string_view pattern = default_pattern,
    std::filesystem::path filename = {},
    std::size_t max_size_bytes = default_max_size_bytes,
    std::size_t max_files = default_max_files,
    bool rotate_on_open = false,
    bool asynchronous = false,
    std::size_t async_queue_size = default_async_queue_size,
    std::size_t async_worker_threads = default_async_worker_threads,
    AsyncOverflowPolicy async_overflow = AsyncOverflowPolicy::block) noexcept;
/// @brief 旧初始化接口，保留给已有调用方；新代码使用 init_log。
Result<void> initialize(LoggerPtr logger = {}, Level level = spdlog::level::info) noexcept;
/// @brief 释放 General 管理的日志注册状态。
Result<void> shutdown() noexcept;
Result<LoggerPtr> try_rotating_file(std::string, std::string, std::size_t = 10*1024*1024, std::size_t = 5, bool = false) noexcept;
LoggerPtr rotating_file(std::string, std::string, std::size_t = 10*1024*1024, std::size_t = 5, bool = false) noexcept;
LoggerPtr rotating_file(std::string, const char *, std::size_t = 10*1024*1024, std::size_t = 5, bool = false) noexcept;
Result<LoggerPtr> try_rotating_file(std::string, const std::filesystem::path &, std::size_t = 10*1024*1024, std::size_t = 5, bool = false) noexcept;
LoggerPtr rotating_file(std::string, const std::filesystem::path &, std::size_t = 10*1024*1024, std::size_t = 5, bool = false) noexcept;
}
#endif
#include <filesystem>
#include <map>
#include <string>
#include <vector>
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
/// @brief 启动 Crashpad 并等待 handler 就绪；重复调用是幂等的。
Result<void> start(const Options &options) noexcept;
}
