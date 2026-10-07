#pragma once

#include <sindre/general/core.h>


#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
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

namespace sindre::general {

class CancellationToken {
    friend class CancellationSource;
    std::shared_ptr<std::atomic<bool>> state_;
    explicit CancellationToken(std::shared_ptr<std::atomic<bool>> state);
public:
    CancellationToken();
    bool cancelled() const noexcept;
    bool is_cancelled() const noexcept;
};

class CancellationSource {
    std::shared_ptr<std::atomic<bool>> state_;
public:
    CancellationSource();
    CancellationToken token() const;
    void cancel() noexcept;
    bool cancelled() const noexcept;
};

using ProgressCallback = std::function<void(double)>;

bool deadline_expired(const std::chrono::steady_clock::time_point &deadline) noexcept;

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
    explicit ThreadPool(std::size_t workers = std::thread::hardware_concurrency());
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    ~ThreadPool();

    template <class Function,
              std::enable_if_t<std::is_invocable_v<Function &, CancellationToken>, int> = 0>
    auto submit(Function function, CancellationToken token = {})
        -> Result<std::future<Result<std::invoke_result_t<Function &, CancellationToken>>>> {
        using Return = std::invoke_result_t<Function &, CancellationToken>;
#if !defined(SINDRE_NO_EXCEPTIONS)
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
#if !defined(SINDRE_NO_EXCEPTIONS)
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

    void stop() noexcept;

private:
    void worker_loop() noexcept;

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

Error async_error(std::errc code, const char *message);
template <class R, class Function>
Result<R> invoke_cancellable(Function &function, const CancellationToken &token,
                             const std::function<void(double)> &progress) {
#if defined(SINDRE_NO_EXCEPTIONS)
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
#if defined(SINDRE_NO_EXCEPTIONS)
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

} // namespace sindre::general


#include <filesystem>
#include <system_error>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace sindre::general::dynamic_library {
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
#if defined(_WIN32)
        auto value = ::GetProcAddress(handle_, std::string(name).c_str());
#else
        auto value = ::dlsym(handle_, std::string(name).c_str());
#endif
        if (!value) {
            return Result<Function *>::failure(
                std::make_error_code(std::errc::function_not_supported),
                "Symbol not found", "dynamic_library.get_symbol");
        }
        return Result<Function *>::success(reinterpret_cast<Function *>(value));
    }

private:
    void close() noexcept;
#if defined(_WIN32)
    HMODULE handle_ = nullptr;
#else
    void *handle_ = nullptr;
#endif
};
} // namespace sindre::general::dynamic_library

namespace sindre::general::process {
struct Options {
    std::filesystem::path working_directory;
    bool capture_output = false;
    std::chrono::milliseconds timeout{0};
    CancellationToken token{};
};
struct Output {
    int exit_code = -1;
    bool signaled = false;
    std::string stdout_text;
    std::string stderr_text;
};
Result<Output> run(std::string command, Options options = {}) noexcept;
} // namespace sindre::general::process
