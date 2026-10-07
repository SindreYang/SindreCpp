#include <sindre/general/runtime.h>

#include <sindre/general/system.h>

#include <cmath>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#if defined(__linux__) || defined(__APPLE__)
#include <pthread.h>
#endif
#include <dlfcn.h>
#endif

namespace sindre::general {
bool is_deadline_expired(const std::chrono::steady_clock::time_point &deadline) noexcept {
    return deadline != std::chrono::steady_clock::time_point{} && std::chrono::steady_clock::now() >= deadline;
}
namespace detail {
Error async_error(std::errc code, const char *message) { return Error::make(code, message, "general.async"); }
} // namespace detail

CancellationToken::CancellationToken()
    : state_(std::make_shared<std::atomic<bool>>(false)) {}

CancellationToken::CancellationToken(std::shared_ptr<std::atomic<bool>> state)
    : state_(std::move(state)) {}

bool CancellationToken::is_cancelled() const noexcept {
    return state_ && state_->load(std::memory_order_acquire);
}

CancellationToken CancellationSource::get_token() const noexcept { return CancellationToken(state_); }

CancellationSource::CancellationSource()
    : state_(std::make_shared<std::atomic<bool>>(false)) {}

void CancellationSource::cancel() noexcept {
    if (state_) state_->store(true, std::memory_order_release);
}

bool CancellationSource::is_cancelled() const noexcept {
    return state_ && state_->load(std::memory_order_acquire);
}

namespace runtime {

Result<void> set_thread_name(std::string_view name) noexcept {
    if (name.empty()) {
        return Result<void>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "Thread name is empty", "runtime.set_thread_name");
    }
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
#if defined(_WIN32)
        const int required = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), static_cast<int>(name.size()),
            nullptr, 0);
        if (required <= 0) {
            return Result<void>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Thread name is not valid UTF-8", "runtime.set_thread_name");
        }
        std::wstring wide(static_cast<std::size_t>(required), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(),
                                static_cast<int>(name.size()), wide.data(), required) <= 0) {
            return Result<void>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Cannot convert thread name to UTF-16", "runtime.set_thread_name");
        }
        using SetThreadDescriptionFunction = HRESULT(WINAPI *)(HANDLE, PCWSTR);
        const auto kernel32 = GetModuleHandleW(L"Kernel32.dll");
        const auto set_thread_description = kernel32 == nullptr ? nullptr :
            reinterpret_cast<SetThreadDescriptionFunction>(
                GetProcAddress(kernel32, "SetThreadDescription"));
        if (set_thread_description == nullptr) {
            return Result<void>::failure(
                std::make_error_code(std::errc::function_not_supported),
                "Windows thread naming is not supported", "runtime.set_thread_name");
        }
        const HRESULT result = set_thread_description(GetCurrentThread(), wide.c_str());
        if (FAILED(result)) {
            return Result<void>::failure(
                std::error_code(static_cast<int>(result), std::system_category()),
                "Cannot set Windows thread name", "runtime.set_thread_name");
        }
        return Result<void>::success();
#elif defined(__linux__)
        const std::string value(name);
        const int result = pthread_setname_np(pthread_self(), value.c_str());
        if (result != 0) {
            return Result<void>::failure(
                std::error_code(result, std::system_category()),
                "Cannot set Linux thread name", "runtime.set_thread_name");
        }
        return Result<void>::success();
#elif defined(__APPLE__)
        const std::string value(name);
        const int result = pthread_setname_np(value.c_str());
        if (result != 0) {
            return Result<void>::failure(
                std::error_code(result, std::system_category()),
                "Cannot set macOS thread name", "runtime.set_thread_name");
        }
        return Result<void>::success();
#else
        return Result<void>::failure(
            std::make_error_code(std::errc::function_not_supported),
            "Thread naming is not supported on this platform",
            "runtime.set_thread_name");
#endif
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<void>::failure(
            std::make_error_code(std::errc::io_error), error.what(),
            "runtime.set_thread_name");
    } catch (...) {
        return Result<void>::failure(
            std::make_error_code(std::errc::io_error),
            "Unknown thread naming failure", "runtime.set_thread_name");
    }
#endif
}

} // namespace runtime

Result<void> sleep(double seconds, TaskOptions options) {
    if (!std::isfinite(seconds) || seconds < 0.0) {
        return Result<void>::failure(
            detail::async_error(std::errc::invalid_argument,
                                "Sleep duration must be a finite non-negative number of seconds"));
    }

    if (options.token.is_cancelled()) {
        return Result<void>::failure(
            detail::async_error(std::errc::operation_canceled,
                                "Sleep cancelled"));
    }
    if (is_deadline_expired(options.deadline)) {
        return Result<void>::failure(
            detail::async_error(std::errc::timed_out,
                                "Sleep deadline expired"));
    }

    const long double ticks =
        static_cast<long double>(seconds) *
        static_cast<long double>(std::chrono::steady_clock::period::den) /
        static_cast<long double>(std::chrono::steady_clock::period::num);
    if (ticks > static_cast<long double>(
                    std::chrono::steady_clock::duration::max().count())) {
        return Result<void>::failure(
            detail::async_error(std::errc::invalid_argument,
                                "Sleep duration is too large"));
    }

    const auto now = std::chrono::steady_clock::now();
    const auto sleep_duration = std::chrono::duration_cast<
        std::chrono::steady_clock::duration>(std::chrono::duration<long double>(seconds));
    const auto end = now + sleep_duration;
    while (std::chrono::steady_clock::now() < end) {
        if (options.token.is_cancelled()) {
            return Result<void>::failure(
                detail::async_error(std::errc::operation_canceled,
                                    "Sleep cancelled"));
        }
        if (is_deadline_expired(options.deadline)) {
            return Result<void>::failure(
                detail::async_error(std::errc::timed_out,
                                    "Sleep deadline expired"));
        }
        const auto current = std::chrono::steady_clock::now();
        auto remaining = end - current;
        if (options.deadline != std::chrono::steady_clock::time_point{}) {
            remaining = std::min(remaining, options.deadline - current);
        }
        if (remaining <= std::chrono::steady_clock::duration::zero()) break;
        std::this_thread::sleep_for(std::min(
            remaining,
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::milliseconds(10))));
    }
    if (options.token.is_cancelled()) {
        return Result<void>::failure(
            detail::async_error(std::errc::operation_canceled,
                                "Sleep cancelled"));
    }
    if (is_deadline_expired(options.deadline) &&
        std::chrono::steady_clock::now() < end) {
        return Result<void>::failure(
            detail::async_error(std::errc::timed_out,
                                "Sleep deadline expired"));
    }
    return Result<void>::success();
}

ThreadPool::ThreadPool(std::size_t workers)
    : state_(std::make_shared<State>()) {
    if (workers == 0) workers = 1;
#if defined(SINDRE_NO_EXCEPTIONS)
    for (std::size_t i = 0; i < workers; ++i) {
        auto state = state_;
        state_->workers.emplace_back([state] { worker_loop(state); });
    }
    state_->initialized = true;
#else
    try {
        for (std::size_t i = 0; i < workers; ++i) {
            auto state = state_;
            state_->workers.emplace_back([state] { worker_loop(state); });
        }
        state_->initialized = true;
    } catch (const std::exception &error) {
        state_->initialization_error = Error::make(
            std::errc::resource_unavailable_try_again, error.what(),
            "general.thread_pool.create");
        stop_state(state_);
        throw;
    } catch (...) {
        state_->initialization_error = Error::make(
            std::errc::resource_unavailable_try_again,
            "Cannot create thread-pool worker", "general.thread_pool.create");
        stop_state(state_);
        throw;
    }
#endif
}

Result<ThreadPool> ThreadPool::create(std::size_t workers) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        ThreadPool pool(workers);
        if (!pool.is_running()) return Result<ThreadPool>::failure(pool.get_error());
        return Result<ThreadPool>::success(std::move(pool));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<ThreadPool>::failure(
            Error::make(std::errc::resource_unavailable_try_again,
                        error.what(), "general.thread_pool.create"));
    } catch (...) {
        return Result<ThreadPool>::failure(
            Error::make(std::errc::resource_unavailable_try_again,
                        "Cannot create thread pool", "general.thread_pool.create"));
    }
#endif
}

ThreadPool::~ThreadPool() { stop(); }

void ThreadPool::stop_state(const std::shared_ptr<State> &state) noexcept {
    if (!state) return;
    std::lock_guard<std::mutex> lifecycle_lock(state->lifecycle_mutex);
    std::vector<QueuedTask> pending;
    {
        std::lock_guard<std::mutex> lock(state->queue_mutex);
        if (state->stopping) return;
        state->stopping = true;
        state->shutdown->store(true, std::memory_order_release);
        while (!state->queue.empty()) {
            pending.emplace_back(std::move(state->queue.front()));
            state->queue.pop();
        }
    }
    for (auto &task : pending) {
#if !defined(SINDRE_NO_EXCEPTIONS)
        try {
#endif
            task.cancel();
#if !defined(SINDRE_NO_EXCEPTIONS)
        } catch (...) {
        }
#endif
    }
    state->condition.notify_all();
    const auto current_id = std::this_thread::get_id();
    for (auto &worker : state->workers) {
        if (!worker.joinable()) continue;
        if (worker.get_id() == current_id) {
            worker.detach();
        } else {
            worker.join();
        }
    }
    state->workers.clear();
}

void ThreadPool::stop() noexcept { stop_state(state_); }

bool ThreadPool::is_running() const noexcept {
    if (!state_) return false;
    std::lock_guard<std::mutex> lock(state_->queue_mutex);
    return state_->initialized && !state_->stopping;
}

Error ThreadPool::get_error() const {
    if (!state_) return Error::make(
        std::errc::operation_canceled, "Thread pool is not initialized",
        "general.thread_pool");
    std::lock_guard<std::mutex> lock(state_->queue_mutex);
    return state_->initialization_error;
}

void ThreadPool::worker_loop(const std::shared_ptr<State> &state) noexcept {
    for (;;) {
        QueuedTask task;
        {
            std::unique_lock<std::mutex> lock(state->queue_mutex);
            state->condition.wait(lock, [&] {
                return state->stopping || !state->queue.empty();
            });
            if (state->stopping && state->queue.empty()) return;
            task = std::move(state->queue.front());
            state->queue.pop();
        }
#if defined(SINDRE_NO_EXCEPTIONS)
        task.run();
#else
        try {
            task.run();
        } catch (...) {
        }
#endif
}
}
} // namespace sindre::general

namespace sindre::general::dynamic_library {

namespace detail {
void *get_symbol_address(void *handle, std::string_view name) noexcept {
    if (!handle || name.empty()) return nullptr;
#if defined(_WIN32)
    return reinterpret_cast<void *>(::GetProcAddress(
        reinterpret_cast<HMODULE>(handle), std::string(name).c_str()));
#else
    return ::dlsym(handle, std::string(name).c_str());
#endif
}
} // namespace detail

Library::Library(Library &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
Library &Library::operator=(Library &&other) noexcept {
    if (this != &other) { close(); handle_ = std::exchange(other.handle_, nullptr); }
    return *this;
}
Library::~Library() { close(); }

Result<Library> Library::open(const std::filesystem::path &path) noexcept {
    Library result;
#if defined(_WIN32)
    result.handle_ = reinterpret_cast<void *>(::LoadLibraryW(path.c_str()));
#else
    result.handle_ = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
    if (!result.handle_) return Result<Library>::failure(
        std::make_error_code(std::errc::no_such_file_or_directory), "Cannot load dynamic library", "dynamic_library.open");
    return Result<Library>::success(std::move(result));
}

void Library::close() noexcept {
    if (!handle_) return;
#if defined(_WIN32)
    ::FreeLibrary(reinterpret_cast<HMODULE>(handle_));
#else
    ::dlclose(handle_);
#endif
    handle_ = nullptr;
}

} // namespace sindre::general::dynamic_library

// Runtime implementation boundary for execution and process services.

// ---- merged from process.cpp ----

#include <chrono>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace sindre::general::process {

Error failure(std::errc code, std::string message, std::string context = "process.run") {
    return Error::make(code, std::move(message), std::move(context));
}

::sindre::general::Result<Output> run(std::string command, Options options) noexcept {
    if (command.empty())
        return ::sindre::general::Result<Output>::failure(
            failure(std::errc::invalid_argument, "Process command is empty"));
    if (options.timeout < options.timeout.zero())
        return ::sindre::general::Result<Output>::failure(
            failure(std::errc::invalid_argument, "Process timeout is negative"));

#if defined(_WIN32)
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    HANDLE output_read = nullptr, output_write = nullptr;
    if (options.capture_output && !::CreatePipe(&output_read, &output_write, &security, 0))
        return ::sindre::general::Result<Output>::failure(
            failure(std::errc::io_error, "Cannot create process output pipe"));
    if (output_read) ::SetHandleInformation(output_read, HANDLE_FLAG_INHERIT, 0);
    std::wstring wide_command;
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        const auto length = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, command.data(),
                                                  static_cast<int>(command.size()), nullptr, 0);
        if (length <= 0) {
            if (output_read) ::CloseHandle(output_read);
            if (output_write) ::CloseHandle(output_write);
            return ::sindre::general::Result<Output>::failure(
                failure(std::errc::illegal_byte_sequence, "Command is not valid UTF-8"));
        }
        wide_command.resize(static_cast<std::size_t>(length));
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, command.data(),
                              static_cast<int>(command.size()), wide_command.data(), length);
        wide_command.push_back(L'\0');
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        if (output_read) ::CloseHandle(output_read);
        if (output_write) ::CloseHandle(output_write);
        return ::sindre::general::Result<Output>::failure(
            failure(std::errc::io_error, error.what(), "process.encoding"));
    }
#endif
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    if (options.capture_output) {
        startup.dwFlags |= STARTF_USESTDHANDLES;
        startup.hStdOutput = output_write;
        startup.hStdError = output_write;
        startup.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
    }
    PROCESS_INFORMATION process{};
    const std::wstring directory = options.working_directory.empty()
        ? std::wstring{} : options.working_directory.wstring();
    const BOOL created = ::CreateProcessW(nullptr, wide_command.data(), nullptr, nullptr, options.capture_output,
                                          CREATE_NO_WINDOW, nullptr,
                                          directory.empty() ? nullptr : directory.c_str(), &startup, &process);
    if (output_write) ::CloseHandle(output_write);
    if (!created) {
        if (output_read) ::CloseHandle(output_read);
        return ::sindre::general::Result<Output>::failure(
            failure(std::errc::no_such_process, "Cannot start process"));
    }
    Output result;
    const auto started = std::chrono::steady_clock::now();
    for (;;) {
        if (options.token.is_cancelled()) {
            ::TerminateProcess(process.hProcess, static_cast<UINT>(ERROR_CANCELLED));
            ::WaitForSingleObject(process.hProcess, INFINITE);
            ::CloseHandle(process.hThread); ::CloseHandle(process.hProcess);
            if (output_read) ::CloseHandle(output_read);
            return ::sindre::general::Result<Output>::failure(
                failure(std::errc::operation_canceled, "Process cancelled"));
        }
        if (options.timeout.count() > 0 && std::chrono::steady_clock::now() - started >= options.timeout) {
            ::TerminateProcess(process.hProcess, static_cast<UINT>(WAIT_TIMEOUT));
            ::WaitForSingleObject(process.hProcess, INFINITE);
            ::CloseHandle(process.hThread); ::CloseHandle(process.hProcess);
            if (output_read) ::CloseHandle(output_read);
            return ::sindre::general::Result<Output>::failure(
                failure(std::errc::timed_out, "Process timed out"));
        }
        const auto state = ::WaitForSingleObject(process.hProcess, 10);
        if (state == WAIT_OBJECT_0) break;
        if (state == WAIT_FAILED) {
            ::CloseHandle(process.hThread); ::CloseHandle(process.hProcess);
            if (output_read) ::CloseHandle(output_read);
            return ::sindre::general::Result<Output>::failure(
                failure(std::errc::io_error, "Cannot wait for process"));
        }
    }
    DWORD exit_code = 1;
    ::GetExitCodeProcess(process.hProcess, &exit_code);
    result.exit_code = static_cast<int>(exit_code);
    if (output_read) {
        char buffer[4096]; DWORD count = 0;
        while (::ReadFile(output_read, buffer, sizeof(buffer), &count, nullptr) && count)
            result.stdout_text.append(buffer, buffer + count);
        ::CloseHandle(output_read);
    }
    ::CloseHandle(process.hThread); ::CloseHandle(process.hProcess);
    return ::sindre::general::Result<Output>::success(std::move(result));
#else
    int output_pipe[2] = {-1, -1};
    int error_pipe[2] = {-1, -1};
    if (options.capture_output && (::pipe(output_pipe) != 0 || ::pipe(error_pipe) != 0)) {
        if (output_pipe[0] >= 0) { ::close(output_pipe[0]); ::close(output_pipe[1]); }
        if (error_pipe[0] >= 0) { ::close(error_pipe[0]); ::close(error_pipe[1]); }
        return ::sindre::general::Result<Output>::failure(
            failure(std::errc::io_error, "Cannot create process output pipe"));
    }
    const pid_t child = ::fork();
    if (child < 0) {
        if (output_pipe[0] >= 0) { ::close(output_pipe[0]); ::close(output_pipe[1]); }
        if (error_pipe[0] >= 0) { ::close(error_pipe[0]); ::close(error_pipe[1]); }
        return ::sindre::general::Result<Output>::failure(
            failure(std::errc::resource_unavailable_try_again, "Cannot fork process"));
    }
    if (child == 0) {
        if (!options.working_directory.empty() &&
            ::chdir(options.working_directory.c_str()) != 0) {
            // 子进程无法返回 Result；通过标准错误和约定退出码把失败传回父进程。
            constexpr char message[] = "Cannot change process working directory\n";
            const auto written = ::write(STDERR_FILENO, message, sizeof(message) - 1);
            (void)written;
            ::_exit(126);
        }
        if (options.capture_output) {
            ::dup2(output_pipe[1], STDOUT_FILENO);
            ::dup2(error_pipe[1], STDERR_FILENO);
            ::close(output_pipe[0]); ::close(output_pipe[1]);
            ::close(error_pipe[0]); ::close(error_pipe[1]);
        }
        ::execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char *>(nullptr));
        ::_exit(127);
    }
    if (options.capture_output) {
        ::close(output_pipe[1]); ::close(error_pipe[1]);
        ::fcntl(output_pipe[0], F_SETFL, O_NONBLOCK); ::fcntl(error_pipe[0], F_SETFL, O_NONBLOCK);
    }
    Output result;
    int status = 0;
    const auto started = std::chrono::steady_clock::now();
    for (;;) {
        if (options.capture_output) {
            char buffer[4096]; ssize_t count = ::read(output_pipe[0], buffer, sizeof(buffer));
            if (count > 0) result.stdout_text.append(buffer, buffer + count);
            count = ::read(error_pipe[0], buffer, sizeof(buffer));
            if (count > 0) result.stderr_text.append(buffer, buffer + count);
        }
        const auto waited = ::waitpid(child, &status, WNOHANG);
        if (waited == child) break;
        if (waited < 0) {
            if (options.capture_output) { ::close(output_pipe[0]); ::close(error_pipe[0]); }
            return ::sindre::general::Result<Output>::failure(failure(std::errc::io_error, "Cannot wait for process"));
        }
        if (options.token.is_cancelled()) {
            ::kill(child, SIGTERM); ::waitpid(child, &status, 0);
            if (options.capture_output) { ::close(output_pipe[0]); ::close(error_pipe[0]); }
            return ::sindre::general::Result<Output>::failure(failure(std::errc::operation_canceled, "Process cancelled"));
        }
        if (options.timeout.count() > 0 && std::chrono::steady_clock::now() - started >= options.timeout) {
            ::kill(child, SIGTERM); ::waitpid(child, &status, 0);
            if (options.capture_output) ::close(output_pipe[0]);
            return ::sindre::general::Result<Output>::failure(failure(std::errc::timed_out, "Process timed out"));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (options.capture_output) {
        char buffer[4096]; ssize_t count = 0;
        while ((count = ::read(output_pipe[0], buffer, sizeof(buffer))) > 0)
            result.stdout_text.append(buffer, buffer + count);
        while ((count = ::read(error_pipe[0], buffer, sizeof(buffer))) > 0)
            result.stderr_text.append(buffer, buffer + count);
        ::close(output_pipe[0]); ::close(error_pipe[0]);
    }
    if (WIFEXITED(status)) result.exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) { result.signaled = true; result.exit_code = 128 + WTERMSIG(status); }
    return ::sindre::general::Result<Output>::success(std::move(result));
#endif
}

} // namespace sindre::general::process
