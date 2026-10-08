#include <sindre/utils_py.h>

#include <iostream>
#include <stdexcept>

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

        auto callback = runtime.value()->run_with_gil([] { return 40 + 2; });
        check(callback && callback.value() == 42, "GIL callback failed");

        {
            auto gil = runtime.value()->get_gil();
            volatile int gil_value = 40 + 2;
            check(gil_value == 42, "GIL-protected callback failed");
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

        const auto array = sindre::utils_py::array_from_vector(std::vector<double>{1.0, 2.0, 3.0});
        check(array.get_dtype() == sindre::utils_py::DType::float64,
              "array dtype conversion failed");
        check(array.get_shape().size() == 1 && array.get_shape().front() == 3,
              "array shape conversion failed");
        auto values = sindre::utils_py::vector_from_array<double>(array);
        check(values && values.value().size() == 3 && values.value()[2] == 3.0,
              "array round trip failed");

        std::cout << "Python runtime tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
