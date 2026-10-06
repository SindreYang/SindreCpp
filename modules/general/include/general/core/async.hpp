#pragma once


#define SINDRECPP_VERSION_MAJOR 0
#define SINDRECPP_VERSION_MINOR 1
#define SINDRECPP_VERSION_PATCH 0
#define SINDRECPP_VERSION "0.1.0"

namespace sindrecpp::general {
inline constexpr char version[] = SINDRECPP_VERSION;
}


#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <functional>
#include <memory>
#include <optional>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>

namespace sindrecpp::general {

struct Error {
    std::error_code code{};
    std::string message;
    std::string context;

    static Error make(std::errc code, std::string message, std::string context = {}) {
        return {std::make_error_code(code), std::move(message), std::move(context)};
    }

    Error with_context(std::string_view extra) const {
        Error result = *this;
        if (extra.empty()) return result;
        if (!result.context.empty()) result.context += ".";
        result.context += extra;
        return result;
    }

    explicit operator bool() const noexcept {
        return static_cast<bool>(code) || !message.empty() || !context.empty();
    }
    std::string describe() const {
        std::string result;
        if (code)
            result += "[" + std::to_string(code.value()) + "] ";
        result += message;
        if (!context.empty()) {
            if (!result.empty()) result += " | ";
            result += context;
        }
        return result;
    }
};

template <class T>
class Result {
public:
    static Result success(T value) { return Result(std::in_place_index<0>, std::move(value)); }
    static Result failure(Error error) { return Result(std::in_place_index<1>, std::move(error)); }
    static Result failure(std::error_code code, std::string message,
                          std::string context = {}) {
        return failure(Error{code, std::move(message), std::move(context)});
    }

    bool has_value() const noexcept { return value_.index() == 0; }
    explicit operator bool() const noexcept { return has_value(); }
    // Unchecked accessors deliberately do not throw. Callers must check the
    // Result first; value_ptr/error_ptr are available when a checked pointer
    // is preferable.
    T& value() & { return *std::get_if<0>(&value_); }
    const T& value() const& { return *std::get_if<0>(&value_); }
    T&& value() && { return std::move(*std::get_if<0>(&value_)); }
    Error& error() & { return *std::get_if<1>(&value_); }
    const Error& error() const& { return *std::get_if<1>(&value_); }
    T *value_ptr() noexcept { return std::get_if<0>(&value_); }
    const T *value_ptr() const noexcept { return std::get_if<0>(&value_); }
    Error *error_ptr() noexcept { return std::get_if<1>(&value_); }
    const Error *error_ptr() const noexcept { return std::get_if<1>(&value_); }
    T value_or(T fallback) const {
        const auto *value = value_ptr();
        return value ? *value : std::move(fallback);
    }

private:
    template <class... Args>
    explicit Result(std::in_place_index_t<0> tag, Args&&... args)
        : value_(tag, std::forward<Args>(args)...) {}
    template <class... Args>
    explicit Result(std::in_place_index_t<1> tag, Args&&... args)
        : value_(tag, std::forward<Args>(args)...) {}

    std::variant<T, Error> value_;
};

template <>
class Result<void> {
public:
    static Result success() { return Result(true, {}); }
    static Result failure(Error error) { return Result(false, std::move(error)); }
    static Result failure(std::error_code code, std::string message,
                          std::string context = {}) {
        return failure(Error{code, std::move(message), std::move(context)});
    }

    bool has_value() const noexcept { return ok_; }
    explicit operator bool() const noexcept { return ok_; }
    const Error& error() const& { return error_; }
    const Error *error_ptr() const noexcept { return ok_ ? nullptr : &error_; }

private:
    Result(bool ok, Error error) : ok_(ok), error_(std::move(error)) {}
    bool ok_;
    Error error_;
};

class CancellationToken {
    friend class CancellationSource;
    std::shared_ptr<std::atomic<bool>> state_;
    explicit CancellationToken(std::shared_ptr<std::atomic<bool>> state)
        : state_(std::move(state)) {}
public:
    CancellationToken() : state_(std::make_shared<std::atomic<bool>>(false)) {}
    bool cancelled() const noexcept {
        return state_ && state_->load(std::memory_order_acquire);
    }
    bool is_cancelled() const noexcept { return cancelled(); }
};

class CancellationSource {
    std::shared_ptr<std::atomic<bool>> state_ = std::make_shared<std::atomic<bool>>(false);
public:
    CancellationToken token() const { return CancellationToken(state_); }
    void cancel() noexcept { state_->store(true, std::memory_order_release); }
    bool cancelled() const noexcept { return state_->load(std::memory_order_acquire); }
};

using ProgressCallback = std::function<void(double)>;

inline bool deadline_expired(const std::chrono::steady_clock::time_point &deadline) noexcept {
    return deadline != std::chrono::steady_clock::time_point{} &&
           std::chrono::steady_clock::now() >= deadline;
}

struct TaskOptions {
    CancellationToken token{};
    std::chrono::steady_clock::time_point deadline{};
    ProgressCallback progress{};
};

namespace detail {
template <class R, class Function>
Result<R> invoke_cancellable(Function &, const CancellationToken &,
                             const ProgressCallback &progress = {});
}

class ThreadPool {
public:
    explicit ThreadPool(std::size_t workers = std::thread::hardware_concurrency()) {
        if (workers == 0) workers = 1;
#if defined(SINDRECPP_NO_EXCEPTIONS)
        for (std::size_t i = 0; i < workers; ++i) workers_.emplace_back([this] { worker_loop(); });
#else
        try {
            for (std::size_t i = 0; i < workers; ++i) workers_.emplace_back([this] { worker_loop(); });
        } catch (...) {
            stop();
        }
#endif
    }
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    ~ThreadPool() { stop(); }

    template <class Function,
              std::enable_if_t<std::is_invocable_v<Function &, CancellationToken>, int> = 0>
    auto submit(Function function, CancellationToken token = {})
        -> Result<std::future<Result<std::invoke_result_t<Function &, CancellationToken>>>> {
        using Return = std::invoke_result_t<Function &, CancellationToken>;
#if !defined(SINDRECPP_NO_EXCEPTIONS)
        try {
#endif
            auto promise = std::make_shared<std::promise<Result<Return>>>();
            auto future = promise->get_future();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (stopping_) return Result<std::future<Result<Return>>>::failure(
                    Error::make(std::errc::operation_canceled, "Thread pool is stopped", "general.thread_pool"));
                queue_.emplace([function = std::move(function), token, promise]() mutable {
                    promise->set_value(detail::invoke_cancellable<Return>(function, token));
                });
            }
            condition_.notify_one();
            return Result<std::future<Result<Return>>>::success(std::move(future));
#if !defined(SINDRECPP_NO_EXCEPTIONS)
        } catch (const std::exception &error) {
            return Result<std::future<Result<Return>>>::failure(
                Error::make(std::errc::resource_unavailable_try_again, error.what(), "general.thread_pool"));
        } catch (...) {
            return Result<std::future<Result<Return>>>::failure(
                Error::make(std::errc::resource_unavailable_try_again, "Cannot submit thread-pool task",
                            "general.thread_pool"));
        }
#endif
    }

    void stop() noexcept {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return;
            stopping_ = true;
        }
        condition_.notify_all();
        for (auto &worker : workers_) if (worker.joinable()) worker.join();
        workers_.clear();
    }

private:
    void worker_loop() noexcept {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
                if (stopping_ && queue_.empty()) return;
                task = std::move(queue_.front());
                queue_.pop();
            }
#if defined(SINDRECPP_NO_EXCEPTIONS)
            task();
#else
            try { task(); } catch (...) {}
#endif
        }
    }

    std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<std::function<void()>> queue_;
    std::vector<std::thread> workers_;
    bool stopping_ = false;
};

namespace detail {
template <class Function, bool = std::is_invocable_v<Function &, CancellationToken>>
struct progress_return;

template <class Function>
struct progress_return<Function, true> {
    using type = std::invoke_result_t<Function &, CancellationToken>;
};

template <class Function>
struct progress_return<Function, false> {
    using type = std::invoke_result_t<Function &, CancellationToken,
                                      const std::function<void(double)> &>;
};

inline Error async_error(std::errc code, const char *message) {
    return Error::make(code, message, "general.async");
}
template <class R, class Function>
Result<R> invoke_cancellable(Function &function, const CancellationToken &token,
                             const std::function<void(double)> &progress) {
#if defined(SINDRECPP_NO_EXCEPTIONS)
    if (token.cancelled()) return Result<R>::failure(async_error(std::errc::operation_canceled, "Operation cancelled"));
    if constexpr (std::is_void_v<R>) {
        if constexpr (std::is_invocable_v<Function &, CancellationToken,
                                          const std::function<void(double)> &>) function(token, progress);
        else function(token);
        if (token.cancelled()) return Result<void>::failure(async_error(std::errc::operation_canceled, "Operation cancelled"));
        return Result<void>::success();
    } else {
        auto value = [&]() {
            if constexpr (std::is_invocable_v<Function &, CancellationToken,
                                              const std::function<void(double)> &>) return function(token, progress);
            else return function(token);
        }();
        if (token.cancelled()) return Result<R>::failure(async_error(std::errc::operation_canceled, "Operation cancelled"));
        return Result<R>::success(std::move(value));
    }
#else
    try {
        if (token.cancelled()) return Result<R>::failure(async_error(std::errc::operation_canceled, "Operation cancelled"));
        if constexpr (std::is_void_v<R>) {
            if constexpr (std::is_invocable_v<Function &, CancellationToken,
                                              const std::function<void(double)> &>)
                function(token, progress);
            else
                function(token);
            if (token.cancelled())
                return Result<void>::failure(async_error(std::errc::operation_canceled, "Operation cancelled"));
            return Result<void>::success();
        } else {
            auto value = [&]() {
                if constexpr (std::is_invocable_v<Function &, CancellationToken,
                                                  const std::function<void(double)> &>)
                    return function(token, progress);
                else
                    return function(token);
            }();
            if (token.cancelled())
                return Result<R>::failure(async_error(std::errc::operation_canceled, "Operation cancelled"));
            return Result<R>::success(std::move(value));
        }
    } catch (const std::exception &error) {
        return Result<R>::failure({std::make_error_code(std::errc::io_error), error.what(), "general.async"});
    } catch (...) {
        return Result<R>::failure(async_error(std::errc::io_error, "Unknown asynchronous failure"));
    }
#endif
}
} // namespace detail

template <class Function>
using async_return_t = std::invoke_result_t<Function &, CancellationToken>;

template <class Function,
          std::enable_if_t<std::is_invocable_v<Function &, CancellationToken>, int> = 0>
Result<std::future<Result<async_return_t<Function>>>>
try_run_async(Function function, CancellationToken token = {}) noexcept {
    using Return = async_return_t<Function>;
#if defined(SINDRECPP_NO_EXCEPTIONS)
    return Result<std::future<Result<Return>>>::success(
        std::async(std::launch::async,
                   [function = std::move(function), token]() mutable {
                       return detail::invoke_cancellable<Return>(function, token);
                   }));
#else
    try {
        return Result<std::future<Result<Return>>>::success(
            std::async(std::launch::async,
                       [function = std::move(function), token]() mutable {
                           return detail::invoke_cancellable<Return>(function, token);
                       }));
    } catch (const std::exception &error) {
        return Result<std::future<Result<Return>>>::failure(
            Error::make(std::errc::resource_unavailable_try_again, error.what(), "general.async.launch"));
    } catch (...) {
        return Result<std::future<Result<Return>>>::failure(
            Error::make(std::errc::resource_unavailable_try_again, "Unable to launch asynchronous task",
                        "general.async.launch"));
    }
#endif
}

template <class Function, std::enable_if_t<std::is_invocable_v<Function &, CancellationToken>, int> = 0>
auto run_async(Function function, CancellationToken token = {})
    -> std::future<Result<std::invoke_result_t<Function &, CancellationToken>>> {
    using Return = std::invoke_result_t<Function &, CancellationToken>;
    return std::async(std::launch::async,
                      [function = std::move(function), token]() mutable {
                          return detail::invoke_cancellable<Return>(function, token);
                      });
}

template <class Function,
          std::enable_if_t<std::is_invocable_v<Function &, CancellationToken> ||
                               std::is_invocable_v<Function &, CancellationToken,
                                                   const std::function<void(double)> &>, int> = 0>
auto run_async(Function function, CancellationToken token, std::function<void(double)> progress)
    -> std::future<Result<typename detail::progress_return<Function>::type>> {
    using Return = typename detail::progress_return<Function>::type;
    return std::async(std::launch::async,
                      [function = std::move(function), token,
                       progress = std::move(progress)]() mutable {
                          return detail::invoke_cancellable<Return>(function, token, progress);
                      });
}

template <class T, class Rep, class Period>
Result<T> wait_for(std::future<Result<T>> &future,
                   std::chrono::duration<Rep, Period> timeout,
                   CancellationToken token = {}) {
    if (timeout < timeout.zero())
        return Result<T>::failure(detail::async_error(std::errc::invalid_argument, "Negative timeout"));
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (future.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready) {
        if (token.cancelled())
            return Result<T>::failure(detail::async_error(std::errc::operation_canceled, "Wait cancelled"));
        if (std::chrono::steady_clock::now() >= deadline)
            return Result<T>::failure(detail::async_error(std::errc::timed_out, "Operation timed out"));
    }
    return future.get();
}

} // namespace sindrecpp::general
