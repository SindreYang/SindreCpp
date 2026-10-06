#include <general/serialization.hpp>

#include <string_view>

int main() {
    const auto document = sindrecpp::general::json::parse(R"({"name":"SindreCpp"})");
    if (!document) return 1;
    auto name = document.value().root()["name"].get_string();
    if (name.error() != simdjson::SUCCESS) return 1;
    return name.value() == std::string_view("SindreCpp") ? 0 : 1;
}
