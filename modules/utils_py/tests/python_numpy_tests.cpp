#include <sindre/utils_py.h>
#include <iostream>
#include <stdexcept>

static void check(bool b) {
    if (!b)
        throw std::runtime_error("NumPy conversion test failed");
}

int main() {
    sindre::utils_py::InterpreterConfig invalid_config;
    invalid_config.python_executable = std::filesystem::path("__sindre_python_does_not_exist__.exe");
    auto invalid = sindre::utils_py::Interpreter::create(invalid_config);
    check(!invalid);

    sindre::utils_py::InterpreterConfig runtime_config;
    runtime_config.python_executable = SINDRE_TEST_PYTHON_EXECUTABLE;
    runtime_config.python_home = SINDRE_TEST_PYTHON_HOME;
    auto interpreter = sindre::utils_py::Interpreter::create(runtime_config);
    if (!interpreter) {
        std::cerr << interpreter.error().describe() << '\n';
        return 1;
    }
    try {
        auto runtime = sindre::utils_py::get_runtime(runtime_config);
        check(runtime && runtime.value()->initialized());

        auto gil = interpreter.value()->get_gil();
        auto callback = interpreter.value()->run_with_gil([] { return 42; });
        check(callback && callback.value() == 42);
        {
            auto released = interpreter.value()->get_gil_release();
            volatile int native_value = 40 + 2;
            check(native_value == 42);
        }

        const auto input = sindre::utils_py::array_from_vector<float>({1.0F, 2.0F, 3.0F, 4.0F});
        check(input.get_dtype() == sindre::utils_py::DType::float32);
        check(input.get_shape().size() == 1 && input.get_shape()[0] == 4);
        auto output = sindre::utils_py::vector_from_array<float>(input);
        check(output && output.value().size() == 4 && output.value()[3] == 4.0F);
        auto mismatch = sindre::utils_py::vector_from_array<double>(input);
        check(!mismatch);
        std::cout << "Array/Python bridge tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
