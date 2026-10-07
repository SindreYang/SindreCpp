#include <sindre/general.h>

#include <string_view>

int main() {
    const auto document = sindre::general::json::try_parse(R"({"name":"sindre"})");
    if (!document) return 1;
    auto name = document.value().root()["name"].get_string();
    if (name.error() != simdjson::SUCCESS) return 1;
    return name.value() == std::string_view("sindre") ? 0 : 1;
}
