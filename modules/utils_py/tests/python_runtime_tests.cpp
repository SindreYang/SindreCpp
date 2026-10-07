#include <sindre/utils_py.h>

#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace py = pybind11;

static void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        sindre::utils_py::InterpreterConfig invalid_config;
        invalid_config.python_executable = "__sindre_python_does_not_exist__.exe";
        check(!sindre::utils_py::Interpreter::create(invalid_config),
              "invalid Python configuration was accepted");

        sindre::utils_py::InterpreterConfig runtime_config;
        runtime_config.python_executable = SINDRE_TEST_PYTHON_EXECUTABLE;
        runtime_config.python_home = SINDRE_TEST_PYTHON_HOME;
        auto runtime = sindre::utils_py::get_runtime(runtime_config);
        if (!runtime) {
            std::cerr << runtime.error().describe() << '\n';
            return 1;
        }
        check(runtime.value()->initialized(), "Python runtime is not initialized");

        auto version = runtime.value()->run_with_gil([] {
            return py::module_::import("sys").attr("version").cast<std::string>();
        });
        check(version && !version.value().empty(), "Python version lookup failed");

        {
            auto gil = runtime.value()->get_gil();
            auto builtins = py::module_::import("builtins");
            check(builtins.attr("len")(py::make_tuple(1, 2, 3)).cast<int>() == 3,
                  "GIL-protected Python call failed");
            {
                auto released = runtime.value()->get_gil_release();
                volatile int native_value = 40 + 2;
                check(native_value == 42, "GIL release scope failed");
            }
        }

        auto failed = runtime.value()->run_with_gil([]() -> int {
            throw std::runtime_error("expected callback failure");
        });
        check(!failed, "Python callback exception was not converted to Result");

        std::cout << "Python runtime tests passed: " << version.value() << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
