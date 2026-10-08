#include <sindre/general.h>

#include <string_view>

int main() {
    const auto document = sindre::general::json::parse(R"({"name":"sindre"})");
    if (!document) return 1;
    const auto *value = document.value().find("name");
    if (!value) return 1;
    auto name = value->get_string();
    return name && name.value() == std::string_view("sindre") ? 0 : 1;
}
