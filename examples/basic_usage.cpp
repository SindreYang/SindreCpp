#include <sindrecpp/sindrecpp.hpp>

#include <iostream>
#include <string>
#include <vector>

#if defined(SINDRECPP_WITH_LOG)
#include <sindrecpp/log.hpp>
#endif
#if defined(SINDRECPP_WITH_UTILS_GUI)
#include <sindrecpp/utils_gui.hpp>
#endif
#if defined(SINDRECPP_WITH_UTILS_PY)
#include <sindrecpp/utils_py.hpp>
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
#if defined(SINDRECPP_WITH_UTILS3D)
#include <sindrecpp/utils3d.hpp>
#endif

int main() {
#if defined(SINDRECPP_WITH_GENERAL)
    auto text = sindrecpp::general::string::trim("  SindreCpp  ");
    auto owner = sindrecpp::general::pointer::make_unique<std::string>(text);
    std::cout << *owner << " " << sindrecpp::general::version << '\n';
#endif

#if defined(SINDRECPP_WITH_LOG)
    sindrecpp::general::log::info("Hello from SindreCpp {}", sindrecpp::general::version);
    auto file_logger = sindrecpp::general::log::rotating_file("sindrecpp-example", "sindrecpp-example.log");
    file_logger->info("Rotating file logging is ready");
    sindrecpp::general::log::native::drop("sindrecpp-example");
#endif
#if defined(SINDRECPP_WITH_UTILS_GUI)
    sindrecpp::utils_gui::Context context;
#endif
#if defined(SINDRECPP_WITH_UTILS_PY)
    sindrecpp::utils_py::Interpreter interpreter;
    auto answer = sindrecpp::utils_py::native::eval("1 + 1");
    if (answer.cast<int>() != 2) return 1;
#endif
#if defined(SINDRECPP_WITH_HTTP)
    using HttpClient = sindrecpp::general::http::Client;
    (void)sizeof(HttpClient);
#endif
#if defined(SINDRECPP_WITH_JSON)
    auto document = sindrecpp::general::json::parse(R"({"name":"SindreCpp"})");
    auto name = document.root()["name"].get_string();
    if (name.error() != simdjson::SUCCESS || name.value() != "SindreCpp") return 2;
#endif
#if defined(SINDRECPP_WITH_CLI)
    sindrecpp::general::cli::ArgumentParser cli_parser("SindreCpp example");
    (void)cli_parser;
#endif
#if defined(SINDRECPP_WITH_UTILS3D)
    auto point = sindrecpp::utils3d::Vector3::Zero();
    (void)point;
#endif
}

#if defined(SINDRECPP_WITH_UTILS_PY)
[[maybe_unused]] static void python_numpy_compile_smoke() {
    const auto array = sindrecpp::utils_py::array_from_vector(std::vector<int>{1, 2, 3});
    const auto values = sindrecpp::utils_py::vector_from_array<int>(array);
    (void)values;
}
#endif
