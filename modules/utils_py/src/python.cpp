#include <sindre/utils_py/python.h>

#include <mutex>
#include <system_error>

namespace sindre::utils_py {
namespace {

::sindre::general::Error interpreter_error(std::errc code,
                                           std::string message,
                                           std::string context = "utils_py.interpreter") {
    return ::sindre::general::Error::make(code, std::move(message), std::move(context));
}

::sindre::general::Result<std::filesystem::path>
normalize_path(const std::filesystem::path &value, bool directory, const char *name) noexcept {
    if (value.empty()) return ::sindre::general::Result<std::filesystem::path>::success({});

    try {
        std::error_code code;
        auto result = value.is_absolute() ? value : std::filesystem::absolute(value, code);
        if (code) {
            return ::sindre::general::Result<std::filesystem::path>::failure(
                interpreter_error(std::errc::invalid_argument,
                                  "Cannot resolve Python path: " + code.message(), name));
        }
        result = result.lexically_normal();
        if (!std::filesystem::exists(result, code) || code) {
            return ::sindre::general::Result<std::filesystem::path>::failure(
                interpreter_error(std::errc::no_such_file_or_directory,
                                  "Python path does not exist", name));
        }
        const bool valid_kind = directory ? std::filesystem::is_directory(result, code)
                                          : std::filesystem::is_regular_file(result, code);
        if (code || !valid_kind) {
            return ::sindre::general::Result<std::filesystem::path>::failure(
                interpreter_error(std::errc::invalid_argument,
                                  "Python path has the wrong type", name));
        }
        return ::sindre::general::Result<std::filesystem::path>::success(std::move(result));
    } catch (const std::exception &error) {
        return ::sindre::general::Result<std::filesystem::path>::failure(
            interpreter_error(std::errc::invalid_argument, error.what(), name));
    } catch (...) {
        return ::sindre::general::Result<std::filesystem::path>::failure(
            interpreter_error(std::errc::invalid_argument, "Cannot normalize Python path", name));
    }
}

::sindre::general::Result<InterpreterConfig>
normalize_config(const InterpreterConfig &input) noexcept {
    try {
        InterpreterConfig result = input;

        auto executable = normalize_path(input.python_executable, false, "utils_py.executable");
        if (!executable)
            return ::sindre::general::Result<InterpreterConfig>::failure(executable.error());
        result.python_executable = std::move(executable.value());

        auto home = normalize_path(input.python_home, true, "utils_py.home");
        if (!home) return ::sindre::general::Result<InterpreterConfig>::failure(home.error());
        result.python_home = std::move(home.value());

        result.module_search_paths.clear();
        result.module_search_paths.reserve(input.module_search_paths.size());
        for (const auto &path : input.module_search_paths) {
            auto normalized = normalize_path(path, true, "utils_py.module_search");
            if (!normalized)
                return ::sindre::general::Result<InterpreterConfig>::failure(normalized.error());
            result.module_search_paths.push_back(std::move(normalized.value()));
        }
        return ::sindre::general::Result<InterpreterConfig>::success(std::move(result));
    } catch (const std::exception &error) {
        return ::sindre::general::Result<InterpreterConfig>::failure(
            interpreter_error(std::errc::invalid_argument, error.what(), "utils_py.config"));
    } catch (...) {
        return ::sindre::general::Result<InterpreterConfig>::failure(
            interpreter_error(std::errc::invalid_argument,
                              "Cannot normalize interpreter configuration", "utils_py.config"));
    }
}

std::string status_message(const PyStatus &status, std::string fallback) {
    if (status.err_msg && *status.err_msg) return status.err_msg;
    return fallback;
}

PyStatus set_path(PyConfig *config, wchar_t **destination,
                  const std::filesystem::path &path) noexcept {
    try {
        const auto wide = path.wstring();
        return PyConfig_SetString(config, destination, wide.c_str());
    } catch (...) {
        return PyStatus_Error("Cannot convert Python path to wchar_t");
    }
}

PyStatus append_path(PyConfig *config, const std::filesystem::path &path) noexcept {
    try {
        const auto wide = path.wstring();
        return PyWideStringList_Append(&config->module_search_paths, wide.c_str());
    } catch (...) {
        return PyStatus_Error("Cannot convert Python module path to wchar_t");
    }
}

std::shared_ptr<Interpreter> &runtime_slot() {
    // Deliberately keep the slot alive until process termination. A DLL
    // unload must not finalize Python while another module can still refer to
    // the interpreter.
    static auto *slot = new std::shared_ptr<Interpreter>();
    return *slot;
}

std::mutex &runtime_mutex() {
    static auto *mutex = new std::mutex();
    return *mutex;
}

} // namespace

::sindre::general::Result<std::shared_ptr<Interpreter>>
Interpreter::create(const InterpreterConfig &config) noexcept {
    auto normalized = normalize_config(config);
    if (!normalized)
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(normalized.error());

    const auto &options = normalized.value();
    PyPreConfig preconfig{};
    PyPreConfig_InitIsolatedConfig(&preconfig);
    preconfig.utf8_mode = 1;

    auto status = Py_PreInitialize(&preconfig);
    if (PyStatus_Exception(status)) {
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(
            interpreter_error(std::errc::invalid_argument,
                              status_message(status, "CPython pre-initialization failed")));
    }

    PyConfig pyconfig{};
    PyConfig_InitIsolatedConfig(&pyconfig);
    pyconfig.parse_argv = 0;
    pyconfig.install_signal_handlers = 0;

    auto fail_config = [&pyconfig](const PyStatus &failure, std::string fallback) {
        const auto message = status_message(failure, std::move(fallback));
        PyConfig_Clear(&pyconfig);
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(
            interpreter_error(std::errc::invalid_argument, message));
    };

    const auto program = options.python_executable.empty()
                              ? std::filesystem::path("python")
                              : options.python_executable;
    status = set_path(&pyconfig, &pyconfig.program_name, program);
    if (PyStatus_Exception(status)) return fail_config(status, "Cannot set CPython program name");

    if (!options.python_executable.empty()) {
        status = set_path(&pyconfig, &pyconfig.executable, options.python_executable);
        if (PyStatus_Exception(status)) return fail_config(status, "Cannot set CPython executable");
    }
    if (!options.python_home.empty()) {
        status = set_path(&pyconfig, &pyconfig.home, options.python_home);
        if (PyStatus_Exception(status)) return fail_config(status, "Cannot set CPython home");
    }
    if (!options.module_search_paths.empty()) {
        pyconfig.module_search_paths_set = 1;
        for (const auto &path : options.module_search_paths) {
            status = append_path(&pyconfig, path);
            if (PyStatus_Exception(status))
                return fail_config(status, "Cannot set CPython module search path");
        }
    }

    try {
        auto guard = std::make_unique<native::scoped_interpreter>(&pyconfig, 0, nullptr, false);
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::success(
            std::shared_ptr<Interpreter>(new Interpreter(std::move(guard), true)));
    } catch (const std::exception &error) {
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(
            interpreter_error(std::errc::io_error, error.what()));
    } catch (...) {
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(
            interpreter_error(std::errc::io_error, "CPython initialization failed"));
    }
}

::sindre::general::Result<std::shared_ptr<Interpreter>> Interpreter::attach() noexcept {
    if (!Py_IsInitialized()) {
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(
            interpreter_error(std::errc::operation_not_permitted,
                              "Cannot attach because CPython is not initialized"));
    }
    try {
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::success(
            std::shared_ptr<Interpreter>(
                new Interpreter(std::unique_ptr<native::scoped_interpreter>{}, true)));
    } catch (const std::exception &error) {
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(
            interpreter_error(std::errc::resource_unavailable_try_again, error.what()));
    } catch (...) {
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(
            interpreter_error(std::errc::resource_unavailable_try_again,
                              "Cannot create CPython attachment"));
    }
}

Interpreter::Interpreter(std::unique_ptr<native::scoped_interpreter> guard,
                         bool initialized) noexcept
    : guard_(std::move(guard)), initialized_(initialized) {}

Interpreter::Interpreter(Interpreter &&other) noexcept
    : guard_(std::move(other.guard_)), initialized_(other.initialized_) {
    other.initialized_ = false;
}

Interpreter &Interpreter::operator=(Interpreter &&other) noexcept {
    if (this == &other) return *this;
    guard_ = std::move(other.guard_);
    initialized_ = other.initialized_;
    other.initialized_ = false;
    return *this;
}

Interpreter::~Interpreter() = default;

::sindre::general::Result<std::shared_ptr<Interpreter>>
get_runtime(const InterpreterConfig &config) noexcept {
    std::lock_guard<std::mutex> lock(runtime_mutex());
    auto &slot = runtime_slot();
    if (slot)
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::success(slot);

    auto result = Py_IsInitialized() ? Interpreter::attach() : Interpreter::create(config);
    if (!result)
        return result;
    slot = result.value();
    return ::sindre::general::Result<std::shared_ptr<Interpreter>>::success(slot);
}

} // namespace sindre::utils_py
