#include <sindrecpp/json.hpp>

#include <string_view>

int main() {
    const auto document = sindrecpp::json::parse(R"({"name":"SindreCpp"})");
    auto name = document.root()["name"].get_string();
    if (name.error() != simdjson::SUCCESS) return 1;
    return name.value() == std::string_view("SindreCpp") ? 0 : 1;
}
