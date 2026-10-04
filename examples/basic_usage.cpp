#include <sindrecpp/sindrecpp.hpp>

#include <iostream>
#include <string>
#include <vector>

#if defined(SINDRECPP_WITH_LOG)
#include <sindrecpp/log.hpp>
#endif
#if defined(SINDRECPP_WITH_GUI)
#include <sindrecpp/gui.hpp>
#endif
#if defined(SINDRECPP_WITH_PYTHON)
#include <sindrecpp/python.hpp>
#endif
#if defined(SINDRECPP_WITH_HTTP)
#include <sindrecpp/http.hpp>
#endif
#if defined(SINDRECPP_WITH_JSON)
#include <sindrecpp/json.hpp>
#endif
#if defined(SINDRECPP_WITH_CLI)
#include <sindrecpp/cli.hpp>
#endif
#if defined(SINDRECPP_WITH_MATH)
#include <sindrecpp/math.hpp>
#endif

int main() {
    auto text = sindrecpp::string::trim("  SindreCpp  ");
    auto owner = sindrecpp::pointer::make_unique<std::string>(text);
    std::cout << *owner << " " << sindrecpp::version << '\n';

#if defined(SINDRECPP_WITH_LOG)
    sindrecpp::log::info("Hello from SindreCpp {}", sindrecpp::version);
    auto file_logger = sindrecpp::log::rotating_file("sindrecpp-example", "sindrecpp-example.log");
    file_logger->info("Rotating file logging is ready");
    sindrecpp::log::native::drop("sindrecpp-example");
#endif
#if defined(SINDRECPP_WITH_GUI)
    sindrecpp::gui::Context context;
#endif
#if defined(SINDRECPP_WITH_PYTHON)
    sindrecpp::python::Interpreter interpreter;
    auto answer = sindrecpp::python::native::eval("1 + 1");
    if (answer.cast<int>() != 2) return 1;
#endif
#if defined(SINDRECPP_WITH_HTTP)
    using HttpClient = sindrecpp::http::Client;
    (void)sizeof(HttpClient);
#endif
#if defined(SINDRECPP_WITH_JSON)
    auto document = sindrecpp::json::parse(R"({"name":"SindreCpp"})");
    auto name = document.root()["name"].get_string();
    if (name.error() != simdjson::SUCCESS || name.value() != "SindreCpp") return 2;
#endif
#if defined(SINDRECPP_WITH_CLI)
    sindrecpp::cli::ArgumentParser cli_parser("SindreCpp example");
    (void)cli_parser;
#endif
#if defined(SINDRECPP_WITH_MATH)
    auto point = sindrecpp::math::Vector3::Zero();
    (void)point;
#endif
}

#if defined(SINDRECPP_WITH_PYTHON)
[[maybe_unused]] static void python_numpy_compile_smoke() {
    const auto array = sindrecpp::python::array_from_vector(std::vector<int>{1, 2, 3});
    const auto values = sindrecpp::python::vector_from_array<int>(array);
    (void)values;
}
#endif
