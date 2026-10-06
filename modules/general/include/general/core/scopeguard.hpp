#pragma once

#include <utility>

namespace sindrecpp::general {

template <class Function>
class ScopeGuard {
public:
    explicit ScopeGuard(Function function) noexcept : function_(std::move(function)) {}
    ScopeGuard(ScopeGuard &&other) noexcept
        : function_(std::move(other.function_)), active_(std::exchange(other.active_, false)) {}
    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;
    ~ScopeGuard() noexcept {
        if (active_) {
#if defined(SINDRECPP_NO_EXCEPTIONS)
            function_();
#else
            try { function_(); } catch (...) {}
#endif
        }
    }
    void dismiss() noexcept { active_ = false; }

private:
    Function function_;
    bool active_ = true;
};

template <class Function>
ScopeGuard<Function> scope_guard(Function function) noexcept {
    return ScopeGuard<Function>(std::move(function));
}

} // namespace sindrecpp::general
