#include <sindre/general/system.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace sindre::general::desktop {
namespace {

template <class T>
[[maybe_unused]]
Result<T> unsupported(const char *context, const char *message = "Desktop backend is not supported") {
    return Result<T>::failure(std::make_error_code(std::errc::function_not_supported), message, context);
}

[[maybe_unused]] Result<void> unsupported_void(const char *context, const char *message = "Desktop backend is not supported") {
    return Result<void>::failure(std::make_error_code(std::errc::function_not_supported), message, context);
}

#if defined(_WIN32)

std::error_code win32_error() noexcept {
    const auto error = ::GetLastError();
    return std::error_code(static_cast<int>(error), std::system_category());
}

std::wstring utf16(std::string_view value) {
    if (value.empty()) return {};
    const int length = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                             value.data(), static_cast<int>(value.size()),
                                             nullptr, 0);
    if (length <= 0) return {};
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                              value.data(), static_cast<int>(value.size()),
                              result.data(), length) != length)
        return {};
    return result;
}

std::string utf8(const wchar_t *value, int length = -1) {
    if (!value) return {};
    const int source_length = length >= 0 ? length : static_cast<int>(::wcslen(value));
    if (source_length == 0) return {};
    const int size = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                           value, source_length, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<std::size_t>(size), '\0');
    if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                              value, source_length, result.data(), size, nullptr, nullptr) != size)
        return {};
    return result;
}

struct TrayState {
    HWND window = nullptr;
    bool active = false;
};

TrayState &tray_state() {
    static TrayState state;
    return state;
}

std::mutex &tray_mutex() {
    static std::mutex mutex;
    return mutex;
}

LRESULT CALLBACK tray_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    return ::DefWindowProcW(window, message, wparam, lparam);
}

Result<HWND> create_tray_window() noexcept {
    static constexpr wchar_t class_name[] = L"SindreGeneralDesktopTray";
    static std::once_flag registration;
    static ATOM registered = 0;
    std::call_once(registration, [] {
        WNDCLASSW window_class{};
        window_class.lpfnWndProc = tray_window_proc;
        window_class.hInstance = ::GetModuleHandleW(nullptr);
        window_class.lpszClassName = class_name;
        registered = ::RegisterClassW(&window_class);
    });
    if (!registered && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return Result<HWND>::failure(win32_error(), "Cannot register tray window class", "desktop.tray");
    const auto window = ::CreateWindowExW(0, class_name, class_name, 0,
                                          0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                          ::GetModuleHandleW(nullptr), nullptr);
    if (!window) return Result<HWND>::failure(win32_error(), "Cannot create tray window", "desktop.tray");
    return Result<HWND>::success(window);
}

Result<void> add_tray_icon(HWND window, std::string_view tooltip, bool balloon = false,
                           std::string_view title = {}, std::string_view message = {}) noexcept {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = window;
    data.uID = 1;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = WM_APP + 1;
    data.hIcon = ::LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
    const auto tip = utf16(tooltip);
    if (tip.empty() && !tooltip.empty())
        return Result<void>::failure(std::make_error_code(std::errc::illegal_byte_sequence),
                                     "Invalid UTF-8 tray tooltip", "desktop.tray");
    std::wcsncpy(data.szTip, tip.c_str(), ARRAYSIZE(data.szTip) - 1);
    if (!::Shell_NotifyIconW(NIM_ADD, &data))
        return Result<void>::failure(win32_error(), "Cannot add tray icon", "desktop.tray");
    if (balloon) {
        const auto wide_title = utf16(title);
        const auto wide_message = utf16(message);
        if ((!title.empty() && wide_title.empty()) || (!message.empty() && wide_message.empty())) {
            ::Shell_NotifyIconW(NIM_DELETE, &data);
            return Result<void>::failure(std::make_error_code(std::errc::illegal_byte_sequence),
                                         "Invalid UTF-8 notification text", "desktop.notify");
        }
        data.uFlags = NIF_INFO;
        data.dwInfoFlags = NIIF_INFO;
        std::wcsncpy(data.szInfoTitle, wide_title.c_str(), ARRAYSIZE(data.szInfoTitle) - 1);
        std::wcsncpy(data.szInfo, wide_message.c_str(), ARRAYSIZE(data.szInfo) - 1);
        if (!::Shell_NotifyIconW(NIM_MODIFY, &data)) {
            ::Shell_NotifyIconW(NIM_DELETE, &data);
            return Result<void>::failure(win32_error(), "Cannot show notification", "desktop.notify");
        }
    }
    return Result<void>::success();
}

Result<std::vector<std::filesystem::path>> parse_open_file_buffer(const std::vector<wchar_t> &buffer,
                                                                  bool multiple) {
    std::vector<std::filesystem::path> result;
    if (buffer.empty() || buffer[0] == L'\0')
        return Result<std::vector<std::filesystem::path>>::success(std::move(result));
    const auto first = std::wstring(buffer.data());
    if (!multiple || buffer[first.size() + 1] == L'\0') {
        const auto value = utf8(buffer.data());
        if (value.empty() && !first.empty())
            return Result<std::vector<std::filesystem::path>>::failure(
                std::make_error_code(std::errc::illegal_byte_sequence),
                "Invalid UTF-16 file dialog result", "desktop.file_dialog");
        result.emplace_back(value);
        return Result<std::vector<std::filesystem::path>>::success(std::move(result));
    }
    std::size_t offset = first.size() + 1;
    while (offset < buffer.size() && buffer[offset] != L'\0') {
        const std::wstring name(buffer.data() + offset);
        result.emplace_back(std::filesystem::path(first) / name);
        offset += name.size() + 1;
    }
    return Result<std::vector<std::filesystem::path>>::success(std::move(result));
}

#elif defined(__linux__)

struct CommandResult {
    int exit_code = -1;
    std::string output;
    std::string error;
};

bool command_exists(std::string_view name) {
    const auto path = std::string(name);
    const char *search = std::getenv("PATH");
    if (!search) return false;
    std::string paths(search);
    std::size_t start = 0;
    while (start <= paths.size()) {
        const auto end = paths.find(':', start);
        const auto directory = paths.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (::access((directory + "/" + path).c_str(), X_OK) == 0) return true;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return false;
}

Result<CommandResult> run_command(const std::vector<std::string> &arguments,
                                   std::chrono::milliseconds timeout = std::chrono::seconds(30),
                                   std::string_view input = {}) noexcept {
    if (arguments.empty() || !command_exists(arguments.front()))
        return unsupported<CommandResult>("desktop.command", "Required desktop command is not installed");
    int input_pipe[2]{};
    int output_pipe[2]{};
    int error_pipe[2]{};
    if (::pipe(input_pipe) != 0)
        return Result<CommandResult>::failure(std::error_code(errno, std::generic_category()),
                                              "Cannot create desktop command pipe", "desktop.command");
    if (::pipe(output_pipe) != 0) {
        ::close(input_pipe[0]); ::close(input_pipe[1]);
        return Result<CommandResult>::failure(std::error_code(errno, std::generic_category()),
                                              "Cannot create desktop command pipe", "desktop.command");
    }
    if (::pipe(error_pipe) != 0) {
        ::close(input_pipe[0]); ::close(input_pipe[1]);
        ::close(output_pipe[0]); ::close(output_pipe[1]);
        return Result<CommandResult>::failure(std::error_code(errno, std::generic_category()),
                                              "Cannot create desktop command pipe", "desktop.command");
    }
    const auto child = ::fork();
    if (child < 0) {
        ::close(input_pipe[0]); ::close(input_pipe[1]);
        ::close(output_pipe[0]); ::close(output_pipe[1]);
        ::close(error_pipe[0]); ::close(error_pipe[1]);
        return Result<CommandResult>::failure(std::error_code(errno, std::generic_category()),
                                              "Cannot start desktop command", "desktop.command");
    }
    if (child == 0) {
        ::dup2(input_pipe[0], STDIN_FILENO);
        ::dup2(output_pipe[1], STDOUT_FILENO);
        ::dup2(error_pipe[1], STDERR_FILENO);
        ::close(input_pipe[0]); ::close(input_pipe[1]);
        ::close(output_pipe[0]); ::close(output_pipe[1]);
        ::close(error_pipe[0]); ::close(error_pipe[1]);
        std::vector<char *> argv;
        argv.reserve(arguments.size() + 1);
        for (const auto &argument : arguments) argv.push_back(const_cast<char *>(argument.c_str()));
        argv.push_back(nullptr);
        ::execvp(argv.front(), argv.data());
        _exit(127);
    }
    ::close(input_pipe[0]);
    ::close(output_pipe[1]);
    ::close(error_pipe[1]);
    bool input_open = !input.empty();
    if (!input_open) ::close(input_pipe[1]);
    else ::fcntl(input_pipe[1], F_SETFL, O_NONBLOCK);
    ::fcntl(output_pipe[0], F_SETFL, O_NONBLOCK);
    ::fcntl(error_pipe[0], F_SETFL, O_NONBLOCK);
    CommandResult result;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    bool output_open = true;
    bool error_open = true;
    bool output_overflow = false;
    bool error_overflow = false;
    auto read_pipe = [](int descriptor, std::string &target, bool &open, bool &overflow) {
        char buffer[4096];
        for (;;) {
            const auto count = ::read(descriptor, buffer, sizeof(buffer));
            if (count > 0) {
                if (target.size() + static_cast<std::size_t>(count) > 1024 * 1024) {
                    overflow = true;
                    open = false;
                    return;
                }
                target.append(buffer, static_cast<std::size_t>(count));
            } else if (count == 0 || (count < 0 && errno != EAGAIN && errno != EINTR)) {
                open = false;
                return;
            } else return;
        }
    };
    std::size_t input_offset = 0;
    while (output_open || error_open || input_open) {
        pollfd descriptors[2]{};
        int count = 0;
        int output_index = -1;
        int error_index = -1;
        int input_index = -1;
        if (output_open) {
            output_index = count;
            descriptors[count++] = {output_pipe[0], POLLIN, 0};
        }
        if (error_open) {
            error_index = count;
            descriptors[count++] = {error_pipe[0], POLLIN, 0};
        }
        if (input_open) {
            input_index = count;
            descriptors[count++] = {input_pipe[1], POLLOUT, 0};
        }
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) {
            ::kill(child, SIGKILL);
            ::waitpid(child, nullptr, 0);
            if (input_open) ::close(input_pipe[1]);
            ::close(output_pipe[0]); ::close(error_pipe[0]);
            return Result<CommandResult>::failure(std::make_error_code(std::errc::timed_out),
                                                  "Desktop command timed out", "desktop.command");
        }
        (void)::poll(descriptors, static_cast<nfds_t>(count), static_cast<int>(remaining));
        if (output_open && output_index >= 0 && (descriptors[output_index].revents & (POLLIN | POLLHUP)))
            read_pipe(output_pipe[0], result.output, output_open, output_overflow);
        if (error_open && error_index >= 0 && (descriptors[error_index].revents & (POLLIN | POLLHUP)))
            read_pipe(error_pipe[0], result.error, error_open, error_overflow);
        if (output_overflow || error_overflow) {
            ::kill(child, SIGKILL);
            ::waitpid(child, nullptr, 0);
            if (input_open) ::close(input_pipe[1]);
            ::close(output_pipe[0]); ::close(error_pipe[0]);
            return Result<CommandResult>::failure(std::make_error_code(std::errc::value_too_large),
                                                  "Desktop command output is too large", "desktop.command");
        }
        if (input_open && input_index >= 0 && (descriptors[input_index].revents & POLLOUT)) {
            const auto count_to_write = std::min<std::size_t>(4096, input.size() - input_offset);
            const auto written = ::write(input_pipe[1], input.data() + input_offset, count_to_write);
            if (written > 0) input_offset += static_cast<std::size_t>(written);
            if (input_offset == input.size()) {
                ::close(input_pipe[1]);
                input_open = false;
            }
        }
    }
    ::close(output_pipe[0]);
    ::close(error_pipe[0]);
    if (::waitpid(child, &result.exit_code, 0) < 0)
        return Result<CommandResult>::failure(std::error_code(errno, std::generic_category()),
                                              "Cannot wait for desktop command", "desktop.command");
    if (WIFEXITED(result.exit_code)) result.exit_code = WEXITSTATUS(result.exit_code);
    else result.exit_code = 128;
    return Result<CommandResult>::success(std::move(result));
}

Result<void> command_status(const std::vector<std::string> &arguments, const char *context,
                            std::string_view input = {}) noexcept {
    auto result = run_command(arguments, std::chrono::seconds(30), input);
    if (!result) return Result<void>::failure(result.error().with_context(context));
    if (result.value().exit_code == 0) return Result<void>::success();
    return Result<void>::failure(std::make_error_code(std::errc::io_error),
                                 result.value().error.empty() ? "Desktop command failed" : result.value().error,
                                 context);
}

std::vector<std::string> file_dialog_command(std::string_view action,
                                              const std::filesystem::path &initial,
                                              bool multiple,
                                              std::string_view suggested) {
    if (command_exists("zenity")) {
        std::vector<std::string> result{"zenity", "--file-selection"};
        if (action == "save") result.push_back("--save");
        if (action == "directory") result.push_back("--directory");
        if (multiple) result.insert(result.end(), {"--multiple", "--separator=\n"});
        if (!initial.empty()) result.push_back("--filename=" + path::to_utf8(initial));
        if (!suggested.empty()) result.push_back("--filename=" + std::string(suggested));
        return result;
    }
    if (command_exists("kdialog")) {
        std::vector<std::string> result{"kdialog"};
        if (action == "open") result.push_back(multiple ? "--getopenfilename" : "--getopenfilename");
        else if (action == "save") result.push_back("--getsavefilename");
        else result.push_back("--getexistingdirectory");
        result.push_back(initial.empty() ? "." : path::to_utf8(initial));
        return result;
    }
    return {};
}

#endif

} // namespace

bool is_elevated() noexcept {
#if defined(_WIN32)
    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const bool result = ::GetTokenInformation(token, TokenElevation, &elevation,
                                              sizeof(elevation), &size) != FALSE &&
                        elevation.TokenIsElevated != 0;
    ::CloseHandle(token);
    return result;
#elif defined(__linux__)
    return ::geteuid() == 0;
#else
    return false;
#endif
}

Result<void> request_elevation(std::string_view executable) noexcept {
    if (executable.empty())
        return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                     "Executable is empty", "desktop.elevation");
#if defined(_WIN32)
    const auto file = utf16(executable);
    if (file.empty()) return Result<void>::failure(std::make_error_code(std::errc::illegal_byte_sequence),
                                                   "Invalid UTF-8 executable path", "desktop.elevation");
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = file.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (!::ShellExecuteExW(&info)) return Result<void>::failure(win32_error(),
        "Cannot request elevation", "desktop.elevation");
    if (info.hProcess) ::CloseHandle(info.hProcess);
    return Result<void>::success();
#elif defined(__linux__)
    return command_status({"pkexec", std::string(executable)}, "desktop.elevation");
#else
    return unsupported_void("desktop.elevation");
#endif
}

Result<void> set_clipboard_text(std::string_view text) noexcept {
#if defined(_WIN32)
    const auto wide = utf16(text);
    if (!text.empty() && wide.empty()) return Result<void>::failure(
        std::make_error_code(std::errc::illegal_byte_sequence), "Invalid UTF-8 clipboard text", "desktop.clipboard");
    if (!::OpenClipboard(nullptr)) return Result<void>::failure(win32_error(), "Cannot open clipboard", "desktop.clipboard");
    if (!::EmptyClipboard()) { ::CloseClipboard(); return Result<void>::failure(win32_error(), "Cannot empty clipboard", "desktop.clipboard"); }
    const auto bytes = (wide.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) { ::CloseClipboard(); return Result<void>::failure(std::make_error_code(std::errc::not_enough_memory), "Cannot allocate clipboard memory", "desktop.clipboard"); }
    auto *destination = static_cast<wchar_t *>(::GlobalLock(memory));
    if (!destination) { ::GlobalFree(memory); ::CloseClipboard(); return Result<void>::failure(win32_error(), "Cannot lock clipboard memory", "desktop.clipboard"); }
    std::memcpy(destination, wide.c_str(), bytes);
    ::GlobalUnlock(memory);
    if (!::SetClipboardData(CF_UNICODETEXT, memory)) { ::GlobalFree(memory); ::CloseClipboard(); return Result<void>::failure(win32_error(), "Cannot set clipboard data", "desktop.clipboard"); }
    ::CloseClipboard();
    return Result<void>::success();
#elif defined(__linux__)
    if (command_exists("wl-copy") && std::getenv("WAYLAND_DISPLAY")) {
        return command_status({"wl-copy", "--type", "text/plain;charset=utf-8"}, "desktop.clipboard", text);
    }
    if (command_exists("xclip") && std::getenv("DISPLAY")) {
        return command_status({"xclip", "-selection", "clipboard", "-in"}, "desktop.clipboard", text);
    }
    if (command_exists("xsel") && std::getenv("DISPLAY")) {
        return command_status({"xsel", "--clipboard", "--input"}, "desktop.clipboard", text);
    }
    return unsupported_void("desktop.clipboard", "No supported Linux clipboard command is installed");
#else
    return unsupported_void("desktop.clipboard");
#endif
}

Result<std::string> get_clipboard_text() noexcept {
#if defined(_WIN32)
    if (!::OpenClipboard(nullptr)) return Result<std::string>::failure(win32_error(), "Cannot open clipboard", "desktop.clipboard");
    auto handle = ::GetClipboardData(CF_UNICODETEXT);
    if (!handle) { ::CloseClipboard(); return Result<std::string>::failure(win32_error(), "Clipboard does not contain Unicode text", "desktop.clipboard"); }
    const auto *value = static_cast<const wchar_t *>(::GlobalLock(handle));
    if (!value) { ::CloseClipboard(); return Result<std::string>::failure(win32_error(), "Cannot lock clipboard data", "desktop.clipboard"); }
    const auto result = utf8(value);
    ::GlobalUnlock(handle);
    ::CloseClipboard();
    if (result.empty() && value[0] != L'\0') return Result<std::string>::failure(std::make_error_code(std::errc::illegal_byte_sequence), "Invalid clipboard text", "desktop.clipboard");
    return Result<std::string>::success(result);
#elif defined(__linux__)
    std::vector<std::string> command;
    if (command_exists("wl-paste") && std::getenv("WAYLAND_DISPLAY")) command = {"wl-paste", "--no-newline"};
    else if (command_exists("xclip") && std::getenv("DISPLAY")) command = {"xclip", "-selection", "clipboard", "-out"};
    else if (command_exists("xsel") && std::getenv("DISPLAY")) command = {"xsel", "--clipboard", "--output"};
    else return unsupported<std::string>("desktop.clipboard", "No supported Linux clipboard command is installed");
    auto result = run_command(command);
    if (!result) return Result<std::string>::failure(result.error().with_context("desktop.clipboard"));
    if (result.value().exit_code != 0) return Result<std::string>::failure(std::make_error_code(std::errc::io_error), result.value().error, "desktop.clipboard");
    return Result<std::string>::success(std::move(result.value().output));
#else
    return unsupported<std::string>("desktop.clipboard");
#endif
}

Result<void> send_notification(std::string_view title, std::string_view message) noexcept {
#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(tray_mutex());
    auto &state = tray_state();
    bool temporary = false;
    if (!state.window) {
        auto window = create_tray_window();
        if (!window) return Result<void>::failure(window.error().with_context("desktop.notify"));
        state.window = window.value();
        temporary = true;
    }
    const auto status = add_tray_icon(state.window, "sindre", true, title, message);
    if (temporary) {
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof(data);
        data.hWnd = state.window;
        data.uID = 1;
        ::Shell_NotifyIconW(NIM_DELETE, &data);
        ::DestroyWindow(state.window);
        state.window = nullptr;
    }
    return status;
#elif defined(__linux__)
    if (command_exists("notify-send")) return command_status({"notify-send", std::string(title), std::string(message)}, "desktop.notify");
    if (command_exists("kdialog")) return command_status({"kdialog", "--passivepopup", std::string(message), "5", "--title", std::string(title)}, "desktop.notify");
    return unsupported_void("desktop.notify", "No supported Linux notification command is installed");
#else
    return unsupported_void("desktop.notify");
#endif
}

Result<MessageBoxResult> show_message_box(std::string_view title, std::string_view message,
                                          MessageBoxType type) noexcept {
#if defined(_WIN32)
    UINT flags = MB_OK;
    if (type == MessageBoxType::warning) flags |= MB_ICONWARNING;
    else if (type == MessageBoxType::error) flags |= MB_ICONERROR;
    else if (type == MessageBoxType::question) flags = MB_YESNO | MB_ICONQUESTION;
    const auto wide_title = utf16(title);
    const auto wide_message = utf16(message);
    if ((!title.empty() && wide_title.empty()) || (!message.empty() && wide_message.empty()))
        return Result<MessageBoxResult>::failure(std::make_error_code(std::errc::illegal_byte_sequence), "Invalid UTF-8 message box text", "desktop.message_box");
    const int result = ::MessageBoxW(nullptr, wide_message.c_str(), wide_title.c_str(), flags);
    if (result == IDYES) return Result<MessageBoxResult>::success(MessageBoxResult::yes);
    if (result == IDNO) return Result<MessageBoxResult>::success(MessageBoxResult::no);
    if (result == IDCANCEL) return Result<MessageBoxResult>::success(MessageBoxResult::cancel);
    if (result == IDOK) return Result<MessageBoxResult>::success(MessageBoxResult::ok);
    return Result<MessageBoxResult>::failure(win32_error(), "Message box failed", "desktop.message_box");
#elif defined(__linux__)
    const std::string kind = type == MessageBoxType::error ? "error" : type == MessageBoxType::warning ? "warning" : type == MessageBoxType::question ? "question" : "info";
    if (command_exists("zenity")) {
        auto result = run_command({"zenity", "--" + kind, "--title", std::string(title), "--text", std::string(message)});
        if (!result) return Result<MessageBoxResult>::failure(result.error().with_context("desktop.message_box"));
        if (result.value().exit_code == 0) return Result<MessageBoxResult>::success(type == MessageBoxType::question ? MessageBoxResult::yes : MessageBoxResult::ok);
        return Result<MessageBoxResult>::success(type == MessageBoxType::question ? MessageBoxResult::no : MessageBoxResult::cancel);
    }
    if (command_exists("kdialog")) {
        const auto command = type == MessageBoxType::question ? "--yesno" : type == MessageBoxType::error ? "--error" : type == MessageBoxType::warning ? "--sorry" : "--msgbox";
        auto result = run_command({"kdialog", command, std::string(message), "--title", std::string(title)});
        if (!result) return Result<MessageBoxResult>::failure(result.error().with_context("desktop.message_box"));
        if (result.value().exit_code == 0) return Result<MessageBoxResult>::success(type == MessageBoxType::question ? MessageBoxResult::yes : MessageBoxResult::ok);
        return Result<MessageBoxResult>::success(type == MessageBoxType::question ? MessageBoxResult::no : MessageBoxResult::cancel);
    }
    return unsupported<MessageBoxResult>("desktop.message_box", "No supported Linux dialog command is installed");
#else
    return unsupported<MessageBoxResult>("desktop.message_box");
#endif
}

Result<std::vector<std::filesystem::path>> open_file_dialog(std::filesystem::path initial, bool multiple) noexcept {
#if defined(_WIN32)
    std::vector<wchar_t> buffer(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (multiple) dialog.Flags |= OFN_ALLOWMULTISELECT;
    const auto directory = initial.empty() ? std::wstring{} : initial.wstring();
    dialog.lpstrInitialDir = directory.empty() ? nullptr : directory.c_str();
    if (!::GetOpenFileNameW(&dialog)) {
        return ::CommDlgExtendedError() == 0 ? Result<std::vector<std::filesystem::path>>::failure(std::make_error_code(std::errc::operation_canceled), "File selection was cancelled", "desktop.file_dialog") : Result<std::vector<std::filesystem::path>>::failure(std::error_code(::CommDlgExtendedError(), std::system_category()), "File dialog failed", "desktop.file_dialog");
    }
    return parse_open_file_buffer(buffer, multiple);
#elif defined(__linux__)
    auto command = file_dialog_command("open", initial, multiple, {});
    if (command.empty()) return unsupported<std::vector<std::filesystem::path>>("desktop.file_dialog", "No supported Linux file dialog command is installed");
    auto result = run_command(command);
    if (!result) return Result<std::vector<std::filesystem::path>>::failure(result.error().with_context("desktop.file_dialog"));
    if (result.value().exit_code != 0) return Result<std::vector<std::filesystem::path>>::failure(std::make_error_code(std::errc::operation_canceled), "File selection was cancelled", "desktop.file_dialog");
    std::vector<std::filesystem::path> paths;
    std::istringstream lines(result.value().output);
    std::string line;
    while (std::getline(lines, line)) if (!line.empty()) {
        auto path = path::try_from_utf8(line);
        if (!path) return Result<std::vector<std::filesystem::path>>::failure(path.error().with_context("desktop.file_dialog"));
        paths.push_back(std::move(path.value()));
    }
    return Result<std::vector<std::filesystem::path>>::success(std::move(paths));
#else
    return unsupported<std::vector<std::filesystem::path>>("desktop.file_dialog");
#endif
}

Result<std::filesystem::path> save_file_dialog(std::filesystem::path initial, std::string suggested) noexcept {
#if defined(_WIN32)
    std::vector<wchar_t> buffer(32768, L'\0');
    const auto initial_text = initial.empty() ? std::wstring{} : initial.wstring();
    const auto suggested_text = utf16(suggested);
    const auto filename = suggested_text.empty() ? initial_text : suggested_text;
    std::copy(filename.begin(), filename.end(), buffer.begin());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!::GetSaveFileNameW(&dialog)) return ::CommDlgExtendedError() == 0 ? Result<std::filesystem::path>::failure(std::make_error_code(std::errc::operation_canceled), "File selection was cancelled", "desktop.file_dialog") : Result<std::filesystem::path>::failure(std::error_code(::CommDlgExtendedError(), std::system_category()), "File dialog failed", "desktop.file_dialog");
    const auto value = utf8(buffer.data());
    if (value.empty()) return Result<std::filesystem::path>::failure(std::make_error_code(std::errc::illegal_byte_sequence), "Invalid UTF-16 file dialog result", "desktop.file_dialog");
    return Result<std::filesystem::path>::success(path::from_utf8(value));
#elif defined(__linux__)
    auto command = file_dialog_command("save", initial, false, suggested);
    if (command.empty()) return unsupported<std::filesystem::path>("desktop.file_dialog", "No supported Linux file dialog command is installed");
    auto result = run_command(command);
    if (!result) return Result<std::filesystem::path>::failure(result.error().with_context("desktop.file_dialog"));
    if (result.value().exit_code != 0 || result.value().output.empty()) return Result<std::filesystem::path>::failure(std::make_error_code(std::errc::operation_canceled), "File selection was cancelled", "desktop.file_dialog");
    auto value = result.value().output;
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) value.pop_back();
    return path::try_from_utf8(value);
#else
    return unsupported<std::filesystem::path>("desktop.file_dialog");
#endif
}

Result<std::filesystem::path> select_directory_dialog(std::filesystem::path initial) noexcept {
#if defined(_WIN32)
    BROWSEINFOW dialog{};
    dialog.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    const auto initial_text = initial.empty() ? std::wstring{} : initial.wstring();
    dialog.lParam = reinterpret_cast<LPARAM>(initial_text.c_str());
    const auto item = ::SHBrowseForFolderW(&dialog);
    if (!item) return Result<std::filesystem::path>::failure(std::make_error_code(std::errc::operation_canceled), "Directory selection was cancelled", "desktop.file_dialog");
    std::vector<wchar_t> buffer(MAX_PATH + 1, L'\0');
    const bool converted = ::SHGetPathFromIDListW(item, buffer.data()) != FALSE;
    ::CoTaskMemFree(item);
    if (!converted) return Result<std::filesystem::path>::failure(win32_error(), "Cannot read selected directory", "desktop.file_dialog");
    const auto value = utf8(buffer.data());
    if (value.empty()) return Result<std::filesystem::path>::failure(std::make_error_code(std::errc::illegal_byte_sequence), "Invalid UTF-16 directory result", "desktop.file_dialog");
    return Result<std::filesystem::path>::success(path::from_utf8(value));
#elif defined(__linux__)
    auto command = file_dialog_command("directory", initial, false, {});
    if (command.empty()) return unsupported<std::filesystem::path>("desktop.file_dialog", "No supported Linux directory dialog command is installed");
    auto result = run_command(command);
    if (!result) return Result<std::filesystem::path>::failure(result.error().with_context("desktop.file_dialog"));
    if (result.value().exit_code != 0 || result.value().output.empty()) return Result<std::filesystem::path>::failure(std::make_error_code(std::errc::operation_canceled), "Directory selection was cancelled", "desktop.file_dialog");
    auto value = result.value().output;
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) value.pop_back();
    return path::try_from_utf8(value);
#else
    return unsupported<std::filesystem::path>("desktop.file_dialog");
#endif
}

Result<void> start_tray(std::string_view tooltip) noexcept {
#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(tray_mutex());
    auto &state = tray_state();
    if (state.active) return Result<void>::success();
    auto window = create_tray_window();
    if (!window) return Result<void>::failure(window.error());
    auto status = add_tray_icon(window.value(), tooltip.empty() ? "sindre" : tooltip);
    if (!status) { ::DestroyWindow(window.value()); return status; }
    state.window = window.value();
    state.active = true;
    return Result<void>::success();
#elif defined(__linux__)
    if (!command_exists("yad")) return unsupported_void("desktop.tray", "Linux tray requires yad");
    return command_status({"yad", "--notification", "--text", std::string(tooltip)}, "desktop.tray");
#else
    return unsupported_void("desktop.tray");
#endif
}

Result<void> stop_tray() noexcept {
#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(tray_mutex());
    auto &state = tray_state();
    if (!state.window) { state.active = false; return Result<void>::success(); }
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = state.window;
    data.uID = 1;
    ::Shell_NotifyIconW(NIM_DELETE, &data);
    ::DestroyWindow(state.window);
    state.window = nullptr;
    state.active = false;
    return Result<void>::success();
#elif defined(__linux__)
    return Result<void>::success();
#else
    return unsupported_void("desktop.tray");
#endif
}

} // namespace sindre::general::desktop
