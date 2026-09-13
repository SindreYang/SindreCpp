#pragma once

#if !defined(SINDRECPP_WITH_LOG)
#error "Enable SINDRECPP_WITH_LOG and link SindreCpp::Log before including this header."
#endif

#include <spdlog/spdlog.h>

#include <utility>

namespace sindrecpp::log {

using Logger = spdlog::logger;
using Level = spdlog::level::level_enum;
namespace native = spdlog;

template <class... Args>
inline void info(Args&&... args) { spdlog::info(std::forward<Args>(args)...); }
template <class... Args>
inline void warn(Args&&... args) { spdlog::warn(std::forward<Args>(args)...); }
template <class... Args>
inline void error(Args&&... args) { spdlog::error(std::forward<Args>(args)...); }
inline void set_level(Level level) { spdlog::set_level(level); }

} // namespace sindrecpp::log
