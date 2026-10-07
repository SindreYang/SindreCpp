#pragma once
#include <sindre/general/core.h>
#include <chrono>
#include <string_view>
#include <utility>
namespace sindre::general::diagnostics {
Result<void> check(bool condition, std::string_view message, std::string_view context = "assert") noexcept;
class Stopwatch {
public: using clock = std::chrono::steady_clock;
    Stopwatch() noexcept; void reset() noexcept;
    std::chrono::nanoseconds elapsed() const noexcept; double elapsed_seconds() const noexcept;
private: clock::time_point started_;
};
}
#if defined(SINDRE_WITH_LOG)
#include <spdlog/spdlog.h>
#include <filesystem>
#include <memory>
#include <string>
namespace sindre::general::log {
using Logger = spdlog::logger; using LoggerPtr = std::shared_ptr<Logger>;
using Level = spdlog::level::level_enum; namespace native = spdlog;
template <class... Args> inline void info(Args &&...args) { spdlog::info(std::forward<Args>(args)...); }
template <class... Args> inline void warning(Args &&...args) { spdlog::warn(std::forward<Args>(args)...); }
template <class... Args> inline void error(Args &&...args) { spdlog::error(std::forward<Args>(args)...); }
void set_level(Level level);
Result<void> initialize(LoggerPtr logger = {}, Level level = spdlog::level::info) noexcept;
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
struct Options { std::filesystem::path handler; std::filesystem::path database; std::filesystem::path dumps;
 std::string upload_url; std::map<std::string,std::string> annotations; std::vector<std::string> arguments;
 bool restartable=true; bool asynchronous=true; };
Result<void> start(const Options &options) noexcept;
}
