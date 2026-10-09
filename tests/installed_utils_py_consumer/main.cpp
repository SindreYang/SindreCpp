#include <sindre/utils_py.h>

#include <cstdlib>

int main() {
    sindre::utils_py::InterpreterConfig config;
    config.python_executable = SINDRE_TEST_PYTHON_EXECUTABLE;
    config.python_home = SINDRE_TEST_PYTHON_HOME;
    auto interpreter = sindre::utils_py::Interpreter::create(config);
    if (!interpreter) {
        return EXIT_FAILURE;
    }
    const auto imported = interpreter.value()->import_module("sys");
    return imported ? EXIT_SUCCESS : EXIT_FAILURE;
}
