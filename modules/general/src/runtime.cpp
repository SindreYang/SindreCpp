#include <sindre/general/runtime.h>

#include <sindre/general/system.h>
#include <fstream>

namespace sindre::general {
bool deadline_expired(const std::chrono::steady_clock::time_point &deadline) noexcept {
    return deadline != std::chrono::steady_clock::time_point{} && std::chrono::steady_clock::now() >= deadline;
}
namespace detail {
Error async_error(std::errc code, const char *message) { return Error::make(code, message, "general.async"); }
} // namespace detail

CancellationToken::CancellationToken()
    : state_(std::make_shared<std::atomic<bool>>(false)) {}

CancellationToken::CancellationToken(std::shared_ptr<std::atomic<bool>> state)
    : state_(std::move(state)) {}

bool CancellationToken::cancelled() const noexcept {
    return state_ && state_->load(std::memory_order_acquire);
}

bool CancellationToken::is_cancelled() const noexcept { return cancelled(); }

CancellationToken CancellationSource::token() const { return CancellationToken(state_); }

CancellationSource::CancellationSource()
    : state_(std::make_shared<std::atomic<bool>>(false)) {}

void CancellationSource::cancel() noexcept {
    state_->store(true, std::memory_order_release);
}

bool CancellationSource::cancelled() const noexcept {
    return state_->load(std::memory_order_acquire);
}

ThreadPool::ThreadPool(std::size_t workers) {
    if (workers == 0) workers = 1;
#if defined(SINDRE_NO_EXCEPTIONS)
    for (std::size_t i = 0; i < workers; ++i) workers_.emplace_back([this] { worker_loop(); });
#else
    try {
        for (std::size_t i = 0; i < workers; ++i) workers_.emplace_back([this] { worker_loop(); });
    } catch (...) {
        stop();
    }
#endif
}

ThreadPool::~ThreadPool() { stop(); }

void ThreadPool::stop() noexcept {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return;
        stopping_ = true;
    }
    condition_.notify_all();
    for (auto &worker : workers_) if (worker.joinable()) worker.join();
    workers_.clear();
}

void ThreadPool::worker_loop() noexcept {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) return;
            task = std::move(queue_.front());
            queue_.pop();
        }
#if defined(SINDRE_NO_EXCEPTIONS)
        task();
#else
        try { task(); } catch (...) {}
#endif
}
}
} // namespace sindre::general

namespace sindre::general::startup {

Result<std::filesystem::path> location(std::string_view name) noexcept {
    if (name.empty()) return Result<std::filesystem::path>::failure(
        std::make_error_code(std::errc::invalid_argument), "Startup name is empty", "startup.location");
#if defined(_WIN32)
    auto base = system::environment("APPDATA");
    if (!base) return Result<std::filesystem::path>::failure(base.error());
    return Result<std::filesystem::path>::success(
        std::filesystem::path(base.value()) / "Microsoft/Windows/Start Menu/Programs/Startup" /
        (std::string(name) + ".cmd"));
#else
    auto home = system::environment("HOME");
    if (!home) return Result<std::filesystem::path>::failure(home.error());
    return Result<std::filesystem::path>::success(
        std::filesystem::path(home.value()) / ".config/systemd/user" / (std::string(name) + ".service"));
#endif
}

Result<void> enable(std::string name, std::string command) noexcept {
    if (command.empty()) return Result<void>::failure(
        std::make_error_code(std::errc::invalid_argument), "Startup command is empty", "startup.enable");
    auto entry = location(name);
    if (!entry) return Result<void>::failure(entry.error());
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::filesystem::create_directories(entry.value().parent_path());
        std::ofstream output(entry.value(), std::ios::trunc);
#if defined(_WIN32)
        output << "@echo off\n" << command << "\n";
#else
        output << "[Unit]\nDescription=" << name << "\nAfter=graphical-session.target\n"
               << "[Service]\nType=simple\nExecStart=" << command << "\n"
               << "[Install]\nWantedBy=default.target\n";
#endif
        if (!output) return Result<void>::failure(
            std::make_error_code(std::errc::permission_denied), "Cannot write startup entry", "startup.enable");
        return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<void>::failure(std::make_error_code(std::errc::io_error), error.what(), "startup.enable");
    } catch (...) {
        return Result<void>::failure(std::make_error_code(std::errc::io_error), "Cannot write startup entry", "startup.enable");
    }
#endif
}

Result<void> disable(std::string_view name) noexcept {
    auto entry = location(name);
    if (!entry) return Result<void>::failure(entry.error());
    std::error_code error;
    std::filesystem::remove(entry.value(), error);
    if (error) return Result<void>::failure(error, "Cannot remove startup entry", "startup.disable");
    return Result<void>::success();
}

} // namespace sindre::general::startup

namespace sindre::general::dynamic_library {

Library::Library(Library &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
Library &Library::operator=(Library &&other) noexcept {
    if (this != &other) { close(); handle_ = std::exchange(other.handle_, nullptr); }
    return *this;
}
Library::~Library() { close(); }

Result<Library> Library::open(const std::filesystem::path &path) noexcept {
    Library result;
#if defined(_WIN32)
    result.handle_ = ::LoadLibraryW(path.c_str());
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
    ::FreeLibrary(handle_);
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
        if (options.token.cancelled()) {
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
        if (!options.working_directory.empty()) ::chdir(options.working_directory.c_str());
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
        if (options.token.cancelled()) {
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
