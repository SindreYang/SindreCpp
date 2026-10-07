#pragma once

/// @file
/// @brief 线程、并发、重试、计时和动态库运行时接口。

#include <sindre/general/core.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cmath>
#include <filesystem>
#include <future>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace sindre::general {

namespace runtime {

class stopwatch {
public:
    using clock = std::chrono::steady_clock;

    stopwatch() noexcept : started_(clock::now()) {}

    void start() noexcept {
        started_ = clock::now();
        stopped_ = {};
    }

    void reset() noexcept { start(); }

    [[nodiscard]] std::chrono::nanoseconds elapsed() const noexcept {
        const auto end = stopped_ == clock::time_point{} ? clock::now() : stopped_;
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - started_);
    }

    [[nodiscard]] double elapsed_seconds() const noexcept {
        return std::chrono::duration<double>(elapsed()).count();
    }

    [[nodiscard]] std::chrono::nanoseconds stop() noexcept {
        if (stopped_ == clock::time_point{}) stopped_ = clock::now();
        return elapsed();
    }

private:
    clock::time_point started_;
    clock::time_point stopped_{};
};

class scopedtimer {
public:
    using callback = std::function<void(std::string_view, std::chrono::nanoseconds)>;

    explicit scopedtimer(std::string_view name, callback on_done = {})
        : name_(name), on_done_(std::move(on_done)) {}

    scopedtimer(const scopedtimer &) = delete;
    scopedtimer &operator=(const scopedtimer &) = delete;

    ~scopedtimer() noexcept { static_cast<void>(done()); }

    [[nodiscard]] std::chrono::nanoseconds elapsed() const noexcept {
        return timer_.elapsed();
    }

    [[nodiscard]] std::chrono::nanoseconds done() noexcept {
        if (done_) return elapsed_;
        elapsed_ = timer_.stop();
        done_ = true;
#if !defined(SINDRE_NO_EXCEPTIONS)
        try {
#endif
            if (on_done_) on_done_(name_, elapsed_);
#if !defined(SINDRE_NO_EXCEPTIONS)
        } catch (...) {
        }
#endif
        return elapsed_;
    }

private:
    std::string name_;
    callback on_done_;
    stopwatch timer_;
    std::chrono::nanoseconds elapsed_{};
    bool done_ = false;
};

} // namespace runtime

class CancellationToken {
    friend class CancellationSource;
    std::shared_ptr<std::atomic<bool>> state_;
    explicit CancellationToken(std::shared_ptr<std::atomic<bool>> state);

public:
    CancellationToken();

    [[nodiscard]] bool is_cancelled() const noexcept;
};

class CancellationSource {
    std::shared_ptr<std::atomic<bool>> state_;

public:
    CancellationSource();

    [[nodiscard]] CancellationToken get_token() const noexcept;
    void cancel() noexcept;
    [[nodiscard]] bool is_cancelled() const noexcept;
};

using ProgressCallback = std::function<void(double)>;

[[nodiscard]] bool is_deadline_expired(
    const std::chrono::steady_clock::time_point &deadline) noexcept;

struct TaskOptions {
    CancellationToken token{};
    std::chrono::steady_clock::time_point deadline{};
    ProgressCallback progress{};
};

namespace detail {
Error async_error(std::errc code, const char *message);

template <class Function, bool = std::is_invocable_v<Function &, CancellationToken>>
struct progress_return;

template <class Function>
struct progress_return<Function, true> {
    using type = std::invoke_result_t<Function &, CancellationToken>;
};

template <class Function>
struct progress_return<Function, false> {
    using type = std::invoke_result_t<Function &, CancellationToken,
                                      const ProgressCallback &>;
};

inline bool is_shutdown(
    const std::shared_ptr<std::atomic<bool>> &shutdown) noexcept {
    return shutdown && shutdown->load(std::memory_order_acquire);
}

inline bool task_cancelled(
    const TaskOptions &options,
    const std::shared_ptr<std::atomic<bool>> &shutdown = {}) noexcept {
    return options.token.is_cancelled() || is_shutdown(shutdown);
}

template <class R, class Function>
Result<R> invoke_cancellable(
    Function &function,
    const TaskOptions &options,
    const std::shared_ptr<std::atomic<bool>> &shutdown = {}) {
    const auto cancelled = [&]() noexcept {
        return task_cancelled(options, shutdown);
    };

    if (is_deadline_expired(options.deadline)) {
        return Result<R>::failure(
            async_error(std::errc::timed_out, "Operation deadline expired"));
    }
    if (cancelled()) {
        return Result<R>::failure(
            async_error(std::errc::operation_canceled, "Operation cancelled"));
    }

#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        if constexpr (std::is_void_v<R>) {
            if constexpr (std::is_invocable_v<Function &, CancellationToken,
                                              const ProgressCallback &>) {
                function(options.token, options.progress);
            } else {
                function(options.token);
            }
            if (cancelled()) {
                return Result<void>::failure(
                    async_error(std::errc::operation_canceled, "Operation cancelled"));
            }
            if (is_deadline_expired(options.deadline)) {
                return Result<void>::failure(
                    async_error(std::errc::timed_out, "Operation deadline expired"));
            }
            return Result<void>::success();
        } else {
            auto value = [&]() {
                if constexpr (std::is_invocable_v<Function &, CancellationToken,
                                                  const ProgressCallback &>) {
                    return function(options.token, options.progress);
                } else {
                    return function(options.token);
                }
            }();
            if (cancelled()) {
                return Result<R>::failure(
                    async_error(std::errc::operation_canceled, "Operation cancelled"));
            }
            if (is_deadline_expired(options.deadline)) {
                return Result<R>::failure(
                    async_error(std::errc::timed_out, "Operation deadline expired"));
            }
            return Result<R>::success(std::move(value));
        }
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<R>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "general.async");
    } catch (...) {
        return Result<R>::failure(
            async_error(std::errc::io_error, "Unknown asynchronous failure"));
    }
#endif
}

} // namespace detail

class ThreadPool {
    struct QueuedTask {
        std::function<void()> run;
        std::function<void()> cancel;
    };

    struct State {
        std::mutex queue_mutex;
        std::condition_variable condition;
        std::queue<QueuedTask> queue;
        std::vector<std::thread> workers;
        std::mutex lifecycle_mutex;
        std::shared_ptr<std::atomic<bool>> shutdown =
            std::make_shared<std::atomic<bool>>(false);
        Error initialization_error{};
        bool stopping = false;
        bool initialized = false;
    };

    std::shared_ptr<State> state_;

    explicit ThreadPool(std::shared_ptr<State> state) noexcept
        : state_(std::move(state)) {}

    static void worker_loop(const std::shared_ptr<State> &state) noexcept;
    static void stop_state(const std::shared_ptr<State> &state) noexcept;

public:
    explicit ThreadPool(
        std::size_t workers = std::thread::hardware_concurrency());
    static Result<ThreadPool> create(
        std::size_t workers = std::thread::hardware_concurrency()) noexcept;

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&) noexcept = default;
    ThreadPool &operator=(ThreadPool &&other) noexcept {
        if (this != &other) {
            stop();
            state_ = std::move(other.state_);
        }
        return *this;
    }
    ~ThreadPool();

    template <class Function,
              std::enable_if_t<std::is_invocable_v<
                  std::decay_t<Function> &, CancellationToken>, int> = 0>
    auto submit(Function function, TaskOptions options = {})
        -> Result<std::future<Result<std::invoke_result_t<
            std::decay_t<Function> &, CancellationToken>>>> {
        using FunctionType = std::decay_t<Function>;
        using Return = std::invoke_result_t<FunctionType &, CancellationToken>;
        using Future = std::future<Result<Return>>;

#if !defined(SINDRE_NO_EXCEPTIONS)
        try {
#endif
            if (!state_) {
                return Result<Future>::failure(
                    detail::async_error(std::errc::operation_canceled,
                                "Thread pool is not initialized"));
            }
            auto function_ptr =
                std::make_shared<FunctionType>(std::move(function));
            auto promise = std::make_shared<std::promise<Result<Return>>>();
            auto future = promise->get_future();
            auto state = state_;
            QueuedTask task{
                [function_ptr, promise, options, state]() mutable {
                    promise->set_value(detail::invoke_cancellable<Return>(
                        *function_ptr, options, state->shutdown));
                },
                [promise]() mutable {
                    promise->set_value(Result<Return>::failure(
                        detail::async_error(std::errc::operation_canceled,
                                    "Queued task cancelled")));
                }};
            {
                std::lock_guard<std::mutex> lock(state_->queue_mutex);
                if (!state_->initialized || state_->stopping) {
                    return Result<Future>::failure(
                        detail::async_error(std::errc::operation_canceled,
                                    "Thread pool is stopped"));
                }
                state_->queue.emplace(std::move(task));
            }
            state_->condition.notify_one();
            return Result<Future>::success(std::move(future));
#if !defined(SINDRE_NO_EXCEPTIONS)
        } catch (const std::exception &error) {
            return Result<Future>::failure(
                std::make_error_code(std::errc::resource_unavailable_try_again),
                error.what(), "general.thread_pool.submit");
        } catch (...) {
            return Result<Future>::failure(
                Error::make(std::errc::resource_unavailable_try_again,
                            "Cannot submit thread-pool task",
                            "general.thread_pool.submit"));
        }
#endif
    }

    void stop() noexcept;
    [[nodiscard]] bool is_running() const noexcept;
    [[nodiscard]] Error get_error() const;
};

template <class Function>
using async_return_t = typename detail::progress_return<
    std::decay_t<Function>>::type;

template <class Function,
          std::enable_if_t<
              std::is_invocable_v<std::decay_t<Function> &, CancellationToken> ||
                  std::is_invocable_v<std::decay_t<Function> &, CancellationToken,
                                      const ProgressCallback &>,
              int> = 0>
Result<std::future<Result<async_return_t<Function>>>>
try_run_async(Function function, TaskOptions options = {}) {
    using FunctionType = std::decay_t<Function>;
    using Return = async_return_t<Function>;
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        return Result<std::future<Result<Return>>>::success(
            std::async(std::launch::async,
                       [function = FunctionType(std::move(function)), options]() mutable {
                           return detail::invoke_cancellable<Return>(function, options);
                       }));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<std::future<Result<Return>>>::failure(
            std::make_error_code(std::errc::resource_unavailable_try_again),
            error.what(), "general.async.launch");
    } catch (...) {
        return Result<std::future<Result<Return>>>::failure(
            Error::make(std::errc::resource_unavailable_try_again,
                        "Unable to launch asynchronous task",
                        "general.async.launch"));
    }
#endif
}

template <class Function,
          std::enable_if_t<
              std::is_invocable_v<std::decay_t<Function> &, CancellationToken> ||
                  std::is_invocable_v<std::decay_t<Function> &, CancellationToken,
                                      const ProgressCallback &>,
              int> = 0>
auto try_run_async(Function function, CancellationToken token)
    -> Result<std::future<Result<async_return_t<Function>>>> {
    TaskOptions options;
    options.token = std::move(token);
    return try_run_async(std::move(function), std::move(options));
}

template <class Function,
          std::enable_if_t<
              std::is_invocable_v<std::decay_t<Function> &, CancellationToken> ||
                  std::is_invocable_v<std::decay_t<Function> &, CancellationToken,
                                      const ProgressCallback &>,
              int> = 0>
auto run_async(Function function, TaskOptions options = {})
    -> std::future<Result<async_return_t<Function>>> {
    using FunctionType = std::decay_t<Function>;
    using Return = async_return_t<Function>;
    return std::async(
        std::launch::async,
        [function = FunctionType(std::move(function)), options]() mutable {
            return detail::invoke_cancellable<Return>(function, options);
        });
}

/// @brief Sleep for a number of seconds while observing cancellation and deadline options.
///
/// The public unit is always seconds, so callers can use `sleep(0.1)` without
/// importing chrono duration literals. Fractional seconds are supported.
Result<void> sleep(double seconds, TaskOptions options = {});

template <class T, class Rep, class Period>
Result<T> wait_for(std::future<Result<T>> &future,
                   std::chrono::duration<Rep, Period> timeout,
                   CancellationToken token = {}) {
    if (!future.valid()) {
        return Result<T>::failure(
            detail::async_error(std::errc::invalid_argument,
                                "Future is not valid"));
    }
    if (timeout.count() < 0) {
        return Result<T>::failure(
            detail::async_error(std::errc::invalid_argument,
                                "Timeout is negative"));
    }
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(timeout);
    for (;;) {
        if (token.is_cancelled()) {
            return Result<T>::failure(
                detail::async_error(std::errc::operation_canceled,
                                    "Wait cancelled"));
        }
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline &&
            future.wait_for(std::chrono::steady_clock::duration::zero()) !=
                std::future_status::ready) {
            return Result<T>::failure(
                detail::async_error(std::errc::timed_out,
                                    "Operation timed out"));
        }
        const auto remaining = deadline - now;
        const auto slice = std::min(
            remaining,
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::milliseconds(10)));
        if (future.wait_for(std::max(
                slice, std::chrono::steady_clock::duration::zero())) ==
            std::future_status::ready) {
#if !defined(SINDRE_NO_EXCEPTIONS)
            try {
#endif
                return future.get();
#if !defined(SINDRE_NO_EXCEPTIONS)
            } catch (const std::exception &error) {
                return Result<T>::failure(
                    std::make_error_code(std::errc::io_error), error.what(),
                    "general.async.future");
            } catch (...) {
                return Result<T>::failure(
                    detail::async_error(std::errc::io_error,
                                        "Unknown future failure"));
            }
#endif
        }
    }
}

namespace runtime {

/// @brief Options for a cancellable parallel index loop.
struct parallel_options {
    std::size_t workers = 0;
    CancellationToken token{};
    std::chrono::steady_clock::time_point deadline{};
    std::string thread_name;
    ProgressCallback progress{};
};

/// @brief Options for retrying a Result-returning operation.
struct retry_options {
    std::size_t max_attempts = 3;
    double initial_delay = 0.1;
    double maximum_delay = 5.0;
    double backoff_multiplier = 2.0;
    CancellationToken token{};
    std::chrono::steady_clock::time_point deadline{};
    std::function<bool(const Error &, std::size_t)> should_retry;
};

namespace detail {

using ::sindre::general::detail::async_error;

template <class Function, bool = std::is_invocable_v<Function &, CancellationToken>>
struct retry_return;

template <class Function>
struct retry_return<Function, true> {
    using type = std::invoke_result_t<Function &, CancellationToken>;
};

template <class Function>
struct retry_return<Function, false> {
    using type = std::invoke_result_t<Function &>;
};

template <class Function, bool = std::is_invocable_v<Function &, std::size_t,
                                                      CancellationToken>>
struct parallel_return;

template <class Function>
struct parallel_return<Function, true> {
    using type = std::invoke_result_t<Function &, std::size_t, CancellationToken>;
};

template <class Function>
struct parallel_return<Function, false> {
    using type = std::invoke_result_t<Function &, std::size_t>;
};

} // namespace detail

/// @brief Set the name of the current operating-system thread.
Result<void> set_thread_name(std::string_view name) noexcept;

template <class Function>
auto retry(Function function, retry_options options = {})
    -> typename detail::retry_return<std::decay_t<Function>>::type {
    using FunctionType = std::decay_t<Function>;
    using ResultType = typename detail::retry_return<FunctionType>::type;

    if (options.max_attempts == 0 ||
        !std::isfinite(options.initial_delay) || options.initial_delay < 0.0 ||
        !std::isfinite(options.maximum_delay) || options.maximum_delay < 0.0 ||
        !std::isfinite(options.backoff_multiplier) || options.backoff_multiplier < 1.0 ||
        options.initial_delay > options.maximum_delay) {
        return ResultType::failure(
            detail::async_error(std::errc::invalid_argument,
                                "Invalid retry options"));
    }

    for (std::size_t attempt = 1; attempt <= options.max_attempts; ++attempt) {
        if (options.token.is_cancelled()) {
            return ResultType::failure(
                detail::async_error(std::errc::operation_canceled,
                                    "Retry cancelled"));
        }
        if (is_deadline_expired(options.deadline)) {
            return ResultType::failure(
                detail::async_error(std::errc::timed_out,
                                    "Retry deadline expired"));
        }

        auto result = [&]() -> ResultType {
#if !defined(SINDRE_NO_EXCEPTIONS)
            try {
#endif
                if constexpr (std::is_invocable_v<FunctionType &, CancellationToken>) {
                    return function(options.token);
                } else {
                    return function();
                }
#if !defined(SINDRE_NO_EXCEPTIONS)
            } catch (const std::exception &error) {
                return ResultType::failure(
                    std::make_error_code(std::errc::io_error), error.what(),
                                         "runtime.retry");
            } catch (...) {
                return ResultType::failure(
                    detail::async_error(std::errc::io_error,
                                        "Unknown retry operation failure"));
            }
#endif
        }();

        if (result) return result;
        if (attempt == options.max_attempts) return result;

        bool retry_attempt = true;
        if (options.should_retry) {
#if !defined(SINDRE_NO_EXCEPTIONS)
            try {
#endif
                retry_attempt = options.should_retry(result.error(), attempt);
#if !defined(SINDRE_NO_EXCEPTIONS)
            } catch (...) {
                return ResultType::failure(
                    detail::async_error(std::errc::io_error,
                                        "Retry callback failed"));
            }
#endif
        }
        if (!retry_attempt) return result;

        double delay = options.initial_delay;
        for (std::size_t index = 1; index < attempt; ++index) {
            if (delay >= options.maximum_delay) {
                delay = options.maximum_delay;
                break;
            }
            delay *= options.backoff_multiplier;
            if (!std::isfinite(delay) || delay > options.maximum_delay) {
                delay = options.maximum_delay;
                break;
            }
        }
        TaskOptions wait_options;
        wait_options.token = options.token;
        wait_options.deadline = options.deadline;
        auto waited = sleep(delay, wait_options);
        if (!waited) return ResultType::failure(waited.error());
    }

    return ResultType::failure(
        detail::async_error(std::errc::io_error, "Retry operation did not complete"));
}

template <class Function>
Result<void> parallel_for(std::size_t begin, std::size_t end,
                          Function function, parallel_options options = {}) {
    using FunctionType = std::decay_t<Function>;
    constexpr bool accepts_token =
        std::is_invocable_v<FunctionType &, std::size_t, CancellationToken>;
    static_assert(accepts_token || std::is_invocable_v<FunctionType &, std::size_t>,
                  "parallel_for requires (index) or (index, CancellationToken)");
    using ItemResult = typename detail::parallel_return<FunctionType, accepts_token>::type;
    static_assert(std::is_void_v<ItemResult> ||
                      std::is_same_v<ItemResult, Result<void>>,
                  "parallel_for callback must return void or Result<void>");

    if (begin >= end) return Result<void>::success();
    if (options.token.is_cancelled()) {
        return Result<void>::failure(
            detail::async_error(std::errc::operation_canceled,
                                "Parallel loop cancelled"));
    }
    if (is_deadline_expired(options.deadline)) {
        return Result<void>::failure(
            detail::async_error(std::errc::timed_out,
                                "Parallel loop deadline expired"));
    }

    const auto count = end - begin;
    const auto requested_workers = options.workers == 0
        ? static_cast<std::size_t>(std::thread::hardware_concurrency())
        : options.workers;
    const auto worker_count = std::max<std::size_t>(
        1, std::min(count, requested_workers == 0 ? std::size_t{1} : requested_workers));
    auto pool_result = ThreadPool::create(worker_count);
    if (!pool_result) return Result<void>::failure(pool_result.error());
    auto pool = std::move(pool_result).value();

    auto function_ptr = std::make_shared<FunctionType>(std::move(function));
    auto next = std::make_shared<std::atomic<std::size_t>>(begin);
    auto completed = std::make_shared<std::atomic<std::size_t>>(0);
    auto failed = std::make_shared<std::atomic<bool>>(false);
    auto first_error = std::make_shared<Error>();
    auto error_mutex = std::make_shared<std::mutex>();
    auto progress_mutex = std::make_shared<std::mutex>();
    auto record_error = [failed, first_error, error_mutex](Error error) {
        bool expected = false;
        if (failed->compare_exchange_strong(expected, true)) {
            std::lock_guard<std::mutex> lock(*error_mutex);
            *first_error = std::move(error);
        }
    };

    std::vector<std::future<Result<void>>> futures;
    futures.reserve(worker_count);
    for (std::size_t worker_index = 0; worker_index < worker_count; ++worker_index) {
        auto worker = [function_ptr, next, completed, failed, first_error, error_mutex,
                       progress_mutex, record_error, options, end, count,
                       worker_index](CancellationToken token) mutable {
            if (!options.thread_name.empty()) {
                std::string name = options.thread_name + "-" + std::to_string(worker_index);
                auto named = set_thread_name(name);
                if (!named) {
                    record_error(named.error());
                    return;
                }
            }
            while (!failed->load(std::memory_order_acquire)) {
                if (token.is_cancelled()) {
                    record_error(detail::async_error(
                        std::errc::operation_canceled, "Parallel loop cancelled"));
                    return;
                }
                if (is_deadline_expired(options.deadline)) {
                    record_error(detail::async_error(
                        std::errc::timed_out, "Parallel loop deadline expired"));
                    return;
                }
                const auto index = next->fetch_add(1, std::memory_order_relaxed);
                if (index >= end) return;

#if !defined(SINDRE_NO_EXCEPTIONS)
                try {
#endif
                    if constexpr (accepts_token) {
                        if constexpr (std::is_void_v<ItemResult>) {
                            (*function_ptr)(index, token);
                        } else {
                            auto item = (*function_ptr)(index, token);
                            if (!item) {
                                record_error(item.error());
                                return;
                            }
                        }
                    } else {
                        if constexpr (std::is_void_v<ItemResult>) {
                            (*function_ptr)(index);
                        } else {
                            auto item = (*function_ptr)(index);
                            if (!item) {
                                record_error(item.error());
                                return;
                            }
                        }
                    }
#if !defined(SINDRE_NO_EXCEPTIONS)
                } catch (const std::exception &error) {
                    record_error(Error::make(std::errc::io_error, error.what(),
                                             "runtime.parallel_for"));
                    return;
                } catch (...) {
                    record_error(Error::make(std::errc::io_error,
                                             "Unknown parallel loop failure",
                                             "runtime.parallel_for"));
                    return;
                }
#endif

                const auto finished = completed->fetch_add(1, std::memory_order_relaxed) + 1;
                if (options.progress) {
#if !defined(SINDRE_NO_EXCEPTIONS)
                    try {
#endif
                        std::lock_guard<std::mutex> lock(*progress_mutex);
                        options.progress(static_cast<double>(finished) /
                                         static_cast<double>(count));
#if !defined(SINDRE_NO_EXCEPTIONS)
                    } catch (...) {
                        record_error(Error::make(std::errc::io_error,
                                                 "Parallel progress callback failed",
                                                 "runtime.parallel_for"));
                        return;
                    }
#endif
                }
            }
        };

        TaskOptions worker_options;
        worker_options.token = options.token;
        worker_options.deadline = options.deadline;
        auto submitted = pool.submit(std::move(worker), worker_options);
        if (!submitted) {
            pool.stop();
            return Result<void>::failure(submitted.error());
        }
        futures.emplace_back(std::move(submitted.value()));
    }

    for (auto &future : futures) {
        auto result = future.get();
        if (!result) record_error(result.error());
    }
    pool.stop();
    if (failed->load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock(*error_mutex);
        return Result<void>::failure(*first_error);
    }
    return Result<void>::success();
}

} // namespace runtime

} // namespace sindre::general

namespace sindre::general::dynamic_library {

namespace detail {
void *get_symbol_address(void *handle, std::string_view name) noexcept;
}

class Library {
public:
    Library() = default;
    Library(const Library &) = delete;
    Library &operator=(const Library &) = delete;
    Library(Library &&other) noexcept;
    Library &operator=(Library &&other) noexcept;
    ~Library();

    static Result<Library> open(const std::filesystem::path &path) noexcept;

    template <class Function>
    Result<Function *> get_symbol(std::string_view name) const noexcept {
        if (!handle_ || name.empty()) {
            return Result<Function *>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Invalid library or symbol", "dynamic_library.get_symbol");
        }
        auto value = detail::get_symbol_address(handle_, name);
        if (!value) {
            return Result<Function *>::failure(
                std::make_error_code(std::errc::function_not_supported),
                "Symbol not found", "dynamic_library.get_symbol");
        }
        return Result<Function *>::success(reinterpret_cast<Function *>(value));
    }

private:
    void close() noexcept;
    void *handle_ = nullptr;
};

} // namespace sindre::general::dynamic_library

namespace sindre::general::process {

struct Options {
    std::filesystem::path working_directory;
    bool capture_output = false;
    std::chrono::milliseconds timeout{0};
    CancellationToken token{};
    // POSIX shell executable. Empty uses /bin/sh. Ignored on Windows.
    std::filesystem::path shell_executable;
    // Zero disables the limit. The default prevents an unbounded child output
    // stream from exhausting the host process memory.
    std::size_t maximum_output_bytes = 16u * 1024u * 1024u;
};

struct Output {
    int exit_code = -1;
    bool signaled = false;
    std::string stdout_text;
    std::string stderr_text;
};

Result<Output> run(std::string command, Options options = {}) noexcept;

} // namespace sindre::general::process
