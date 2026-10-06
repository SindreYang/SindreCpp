#pragma once

#include <general/core/async.hpp>

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

namespace sindrecpp::general::process {

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

inline Error failure(std::errc code, std::string message, std::string context = "process.run") {
    return Error::make(code, std::move(message), std::move(context));
}

inline ::sindrecpp::general::Result<Output> run(std::string command, Options options = {}) noexcept {
    if (command.empty())
        return ::sindrecpp::general::Result<Output>::failure(
            failure(std::errc::invalid_argument, "Process command is empty"));
    if (options.timeout < options.timeout.zero())
        return ::sindrecpp::general::Result<Output>::failure(
            failure(std::errc::invalid_argument, "Process timeout is negative"));

#if defined(_WIN32)
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    HANDLE output_read = nullptr, output_write = nullptr;
    if (options.capture_output && !::CreatePipe(&output_read, &output_write, &security, 0))
        return ::sindrecpp::general::Result<Output>::failure(
            failure(std::errc::io_error, "Cannot create process output pipe"));
    if (output_read) ::SetHandleInformation(output_read, HANDLE_FLAG_INHERIT, 0);
    std::wstring wide_command;
#if !defined(SINDRECPP_NO_EXCEPTIONS)
    try {
#endif
        const auto length = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, command.data(),
                                                  static_cast<int>(command.size()), nullptr, 0);
        if (length <= 0) {
            if (output_read) ::CloseHandle(output_read);
            if (output_write) ::CloseHandle(output_write);
            return ::sindrecpp::general::Result<Output>::failure(
                failure(std::errc::illegal_byte_sequence, "Command is not valid UTF-8"));
        }
        wide_command.resize(static_cast<std::size_t>(length));
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, command.data(),
                              static_cast<int>(command.size()), wide_command.data(), length);
        wide_command.push_back(L'\0');
#if !defined(SINDRECPP_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        if (output_read) ::CloseHandle(output_read);
        if (output_write) ::CloseHandle(output_write);
        return ::sindrecpp::general::Result<Output>::failure(
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
        return ::sindrecpp::general::Result<Output>::failure(
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
            return ::sindrecpp::general::Result<Output>::failure(
                failure(std::errc::operation_canceled, "Process cancelled"));
        }
        if (options.timeout.count() > 0 && std::chrono::steady_clock::now() - started >= options.timeout) {
            ::TerminateProcess(process.hProcess, static_cast<UINT>(WAIT_TIMEOUT));
            ::WaitForSingleObject(process.hProcess, INFINITE);
            ::CloseHandle(process.hThread); ::CloseHandle(process.hProcess);
            if (output_read) ::CloseHandle(output_read);
            return ::sindrecpp::general::Result<Output>::failure(
                failure(std::errc::timed_out, "Process timed out"));
        }
        const auto state = ::WaitForSingleObject(process.hProcess, 10);
        if (state == WAIT_OBJECT_0) break;
        if (state == WAIT_FAILED) {
            ::CloseHandle(process.hThread); ::CloseHandle(process.hProcess);
            if (output_read) ::CloseHandle(output_read);
            return ::sindrecpp::general::Result<Output>::failure(
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
    return ::sindrecpp::general::Result<Output>::success(std::move(result));
#else
    int output_pipe[2] = {-1, -1};
    int error_pipe[2] = {-1, -1};
    if (options.capture_output && (::pipe(output_pipe) != 0 || ::pipe(error_pipe) != 0)) {
        if (output_pipe[0] >= 0) { ::close(output_pipe[0]); ::close(output_pipe[1]); }
        if (error_pipe[0] >= 0) { ::close(error_pipe[0]); ::close(error_pipe[1]); }
        return ::sindrecpp::general::Result<Output>::failure(
            failure(std::errc::io_error, "Cannot create process output pipe"));
    }
    const pid_t child = ::fork();
    if (child < 0) {
        if (output_pipe[0] >= 0) { ::close(output_pipe[0]); ::close(output_pipe[1]); }
        if (error_pipe[0] >= 0) { ::close(error_pipe[0]); ::close(error_pipe[1]); }
        return ::sindrecpp::general::Result<Output>::failure(
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
            return ::sindrecpp::general::Result<Output>::failure(failure(std::errc::io_error, "Cannot wait for process"));
        }
        if (options.token.cancelled()) {
            ::kill(child, SIGTERM); ::waitpid(child, &status, 0);
            if (options.capture_output) { ::close(output_pipe[0]); ::close(error_pipe[0]); }
            return ::sindrecpp::general::Result<Output>::failure(failure(std::errc::operation_canceled, "Process cancelled"));
        }
        if (options.timeout.count() > 0 && std::chrono::steady_clock::now() - started >= options.timeout) {
            ::kill(child, SIGTERM); ::waitpid(child, &status, 0);
            if (options.capture_output) ::close(output_pipe[0]);
            return ::sindrecpp::general::Result<Output>::failure(failure(std::errc::timed_out, "Process timed out"));
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
    return ::sindrecpp::general::Result<Output>::success(std::move(result));
#endif
}

} // namespace sindrecpp::general::process
