#pragma once

#include <general/core/async.hpp>
#include <chrono>
#include <string_view>

namespace sindrecpp::general::diagnostics {

inline ::sindrecpp::general::Result<void> check(bool condition, std::string_view message,
                                                std::string_view context = "assert") noexcept {
    if (condition) return ::sindrecpp::general::Result<void>::success();
    return ::sindrecpp::general::Result<void>::failure(
        std::make_error_code(std::errc::invalid_argument), std::string(message), std::string(context));
}

class Stopwatch {
public:
    using clock = std::chrono::steady_clock;
    Stopwatch() noexcept : started_(clock::now()) {}
    void reset() noexcept { started_ = clock::now(); }
    std::chrono::nanoseconds elapsed() const noexcept { return std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - started_); }
    double elapsed_seconds() const noexcept { return std::chrono::duration<double>(clock::now() - started_).count(); }

private:
    clock::time_point started_;
};

} // namespace sindrecpp::general::diagnostics
