#pragma once

#if !defined(SINDRECPP_WITH_LOG)
#error "Enable SINDRECPP_WITH_LOG and link SindreCpp::Log before including this header."
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace sindrecpp::log {

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

inline LoggerPtr rotating_file(std::string name, std::string filename,
                               std::size_t max_size_bytes = 10 * 1024 * 1024,
                               std::size_t max_files = 5,
                               bool rotate_on_open = false) {
    auto logger = spdlog::rotating_logger_mt(std::move(name), std::move(filename),
                                             max_size_bytes, max_files, rotate_on_open);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    return logger;
}

} // namespace sindrecpp::log
