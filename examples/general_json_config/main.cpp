#include <general/serialization.hpp>

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace general = sindrecpp::general;

static int report_failure(std::string_view operation, const general::Error &error) {
    std::cerr << operation << " failed: " << error.describe() << '\n';
    return 1;
}

static void set_example_environment() {
#if defined(_WIN32)
    _putenv_s("SINDRECPP_EXAMPLE_APP_PORT", "9090");
#else
    setenv("SINDRECPP_EXAMPLE_APP_PORT", "9090", 1);
#endif
}

int main() {
    const auto document = general::json::try_parse(R"({"name":"SindreCpp","enabled":true})");
    if (!document) return report_failure("parse JSON", document.error());

    const auto name = document.value().root()["name"].get_string();
    if (name.error() != simdjson::SUCCESS) {
        return report_failure(
            "read JSON name",
            general::Error::make(std::errc::invalid_argument,
                                 simdjson::error_message(name.error()), "example.json.name"));
    }

    const auto defaults = general::config::Config::with_defaults({
        {"app.port", "8080"},
        {"app.enabled", "false"},
    });
    auto config = general::config::Config::from_json(
        R"({"app":{"enabled":true},"app_name":"demo"})", defaults);
    if (!config) return report_failure("load configuration", config.error());

    set_example_environment();
    const auto environment = config.value().apply_environment("SINDRECPP_EXAMPLE");
    if (!environment) return report_failure("apply environment", environment.error());

    const auto port = config.value().get_int("app.port");
    const auto enabled = config.value().get_bool("app.enabled");
    if (!port) return report_failure("read app.port", port.error());
    if (!enabled) return report_failure("read app.enabled", enabled.error());

    const auto invalid = general::json::try_parse("{broken");
    if (invalid) {
        std::cerr << "invalid JSON was accepted\n";
        return 1;
    }

    std::cout << "name: " << name.value_unsafe() << '\n'
              << "app.port: " << port.value() << '\n'
              << "app.enabled: " << (enabled.value() ? "true" : "false") << '\n'
              << "invalid JSON: " << invalid.error().describe() << '\n';
    return 0;
}
