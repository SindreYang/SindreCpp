#include <sindre/utils_py/python.h>

#include <pybind11/embed.h>

#include <mutex>
#include <system_error>

namespace sindre::utils_py {
namespace {
namespace native = pybind11;

::sindre::general::Error interpreter_error(std::errc code, std::string message,
                                           std::string context = "utils_py.interpreter") {
    return ::sindre::general::Error::make(code, std::move(message), std::move(context));
}

::sindre::general::Result<InterpreterConfig> normalize_config(
    const InterpreterConfig &input) noexcept {
    try {
        InterpreterConfig result = input;
        auto normalize = [](const std::filesystem::path &value, bool directory,
                            const char *context) -> ::sindre::general::Result<std::filesystem::path> {
            if (value.empty()) return ::sindre::general::Result<std::filesystem::path>::success({});
            std::error_code code;
            auto path = value.is_absolute() ? value : std::filesystem::absolute(value, code);
            if (code || !std::filesystem::exists(path, code) || code)
                return ::sindre::general::Result<std::filesystem::path>::failure(
                    interpreter_error(std::errc::no_such_file_or_directory,
                                      "Python path does not exist", context));
            if (directory != std::filesystem::is_directory(path, code))
                return ::sindre::general::Result<std::filesystem::path>::failure(
                    interpreter_error(std::errc::invalid_argument,
                                      "Python path has the wrong type", context));
            return ::sindre::general::Result<std::filesystem::path>::success(path.lexically_normal());
        };
        auto executable = normalize(input.python_executable, false, "utils_py.executable");
        if (!executable) return ::sindre::general::Result<InterpreterConfig>::failure(executable.error());
        auto home = normalize(input.python_home, true, "utils_py.home");
        if (!home) return ::sindre::general::Result<InterpreterConfig>::failure(home.error());
        result.python_executable = std::move(executable.value());
        result.python_home = std::move(home.value());
        result.module_search_paths.clear();
        for (const auto &path : input.module_search_paths) {
            auto normalized = normalize(path, true, "utils_py.module_search");
            if (!normalized) return ::sindre::general::Result<InterpreterConfig>::failure(normalized.error());
            result.module_search_paths.push_back(std::move(normalized.value()));
        }
        return ::sindre::general::Result<InterpreterConfig>::success(std::move(result));
    } catch (const std::exception &error) {
        return ::sindre::general::Result<InterpreterConfig>::failure(
            interpreter_error(std::errc::invalid_argument, error.what(), "utils_py.config"));
    } catch (...) {
        return ::sindre::general::Result<InterpreterConfig>::failure(
            interpreter_error(std::errc::invalid_argument, "Cannot normalize Python configuration",
                              "utils_py.config"));
    }
}

std::shared_ptr<Interpreter> &runtime_slot() {
    static auto *slot = new std::shared_ptr<Interpreter>();
    return *slot;
}
std::mutex &runtime_mutex() {
    static auto *mutex = new std::mutex();
    return *mutex;
}

PyStatus set_path(PyConfig *config, wchar_t **destination,
                  const std::filesystem::path &path) noexcept {
    try { return PyConfig_SetString(config, destination, path.wstring().c_str()); }
    catch (...) { return PyStatus_Error("Cannot convert Python path"); }
}
PyStatus append_path(PyConfig *config, const std::filesystem::path &path) noexcept {
    try { return PyWideStringList_Append(&config->module_search_paths, path.wstring().c_str()); }
    catch (...) { return PyStatus_Error("Cannot convert Python module path"); }
}

} // namespace

struct GilGuardImpl { std::unique_ptr<native::gil_scoped_acquire> guard; };
struct GilReleaseGuardImpl { std::unique_ptr<native::gil_scoped_release> guard; };
struct InterpreterImpl { std::unique_ptr<native::scoped_interpreter> guard; };

GilGuard::~GilGuard() { delete static_cast<GilGuardImpl *>(impl_); }
GilReleaseGuard::~GilReleaseGuard() { delete static_cast<GilReleaseGuardImpl *>(impl_); }

GilGuard Interpreter::get_gil() const {
    auto impl = std::make_unique<GilGuardImpl>();
    impl->guard = std::make_unique<native::gil_scoped_acquire>();
    return GilGuard(impl.release());
}
GilReleaseGuard Interpreter::get_gil_release() const {
    auto impl = std::make_unique<GilReleaseGuardImpl>();
    impl->guard = std::make_unique<native::gil_scoped_release>();
    return GilReleaseGuard(impl.release());
}

::sindre::general::Result<std::shared_ptr<Interpreter>>
Interpreter::create(const InterpreterConfig &config) noexcept {
    auto normalized = normalize_config(config);
    if (!normalized) return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(normalized.error());
    const auto &options = normalized.value();
    PyPreConfig preconfig{};
    PyPreConfig_InitIsolatedConfig(&preconfig);
    preconfig.utf8_mode = 1;
    auto status = Py_PreInitialize(&preconfig);
    if (PyStatus_Exception(status))
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(
            interpreter_error(std::errc::invalid_argument, "CPython pre-initialization failed"));
    PyConfig pyconfig{};
    PyConfig_InitIsolatedConfig(&pyconfig);
    pyconfig.parse_argv = 0;
    pyconfig.install_signal_handlers = 0;
    const auto program = options.python_executable.empty() ? std::filesystem::path("python") : options.python_executable;
    status = set_path(&pyconfig, &pyconfig.program_name, program);
    if (PyStatus_Exception(status)) { PyConfig_Clear(&pyconfig); return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(interpreter_error(std::errc::invalid_argument, "Cannot set Python program name")); }
    if (!options.python_executable.empty()) {
        status = set_path(&pyconfig, &pyconfig.executable, options.python_executable);
        if (PyStatus_Exception(status)) { PyConfig_Clear(&pyconfig); return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(interpreter_error(std::errc::invalid_argument, "Cannot set Python executable")); }
    }
    if (!options.python_home.empty()) {
        status = set_path(&pyconfig, &pyconfig.home, options.python_home);
        if (PyStatus_Exception(status)) { PyConfig_Clear(&pyconfig); return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(interpreter_error(std::errc::invalid_argument, "Cannot set Python home")); }
    }
    if (!options.module_search_paths.empty()) {
        pyconfig.module_search_paths_set = 1;
        for (const auto &path : options.module_search_paths) {
            status = append_path(&pyconfig, path);
            if (PyStatus_Exception(status)) { PyConfig_Clear(&pyconfig); return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(interpreter_error(std::errc::invalid_argument, "Cannot set Python module path")); }
        }
    }
    try {
        auto impl = std::make_unique<InterpreterImpl>();
        impl->guard = std::make_unique<native::scoped_interpreter>(&pyconfig, 0, nullptr, false);
        PyConfig_Clear(&pyconfig);
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::success(
            std::shared_ptr<Interpreter>(new Interpreter(impl.release())));
    } catch (const std::exception &error) {
        PyConfig_Clear(&pyconfig);
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(interpreter_error(std::errc::io_error, error.what()));
    } catch (...) {
        PyConfig_Clear(&pyconfig);
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(interpreter_error(std::errc::io_error, "CPython initialization failed"));
    }
}

::sindre::general::Result<std::shared_ptr<Interpreter>> Interpreter::attach() noexcept {
    if (!Py_IsInitialized())
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(
            interpreter_error(std::errc::operation_not_permitted, "Python is not initialized"));
    try {
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::success(
            std::shared_ptr<Interpreter>(new Interpreter(new InterpreterImpl())));
    } catch (const std::exception &error) {
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(interpreter_error(std::errc::io_error, error.what()));
    } catch (...) {
        return ::sindre::general::Result<std::shared_ptr<Interpreter>>::failure(interpreter_error(std::errc::io_error, "Cannot attach Python"));
    }
}

Interpreter::Interpreter(Interpreter &&other) noexcept
    : impl_(other.impl_), initialized_(other.initialized_) {
    other.impl_ = nullptr;
    other.initialized_ = false;
}
Interpreter &Interpreter::operator=(Interpreter &&other) noexcept {
    if (this != &other) {
        delete static_cast<InterpreterImpl *>(impl_);
        impl_ = other.impl_;
        initialized_ = other.initialized_;
        other.impl_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}
Interpreter::~Interpreter() { delete static_cast<InterpreterImpl *>(impl_); }

::sindre::general::Result<std::shared_ptr<Interpreter>>
get_runtime(const InterpreterConfig &config) noexcept {
    std::lock_guard lock(runtime_mutex());
    auto &slot = runtime_slot();
    if (slot) return ::sindre::general::Result<std::shared_ptr<Interpreter>>::success(slot);
    auto result = Py_IsInitialized() ? Interpreter::attach() : Interpreter::create(config);
    if (!result) return result;
    slot = result.value();
    return ::sindre::general::Result<std::shared_ptr<Interpreter>>::success(slot);
}

} // namespace sindre::utils_py
