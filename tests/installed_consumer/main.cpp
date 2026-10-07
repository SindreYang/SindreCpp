#include <sindre/general.h>
#include <sindre/general/network.h>

#include <iostream>

int main() {
    const auto parsed = sindre::general::string::parse_int("42");
    const auto target = sindre::general::network::parse("https://example.com/api?q=1");
    if (!parsed || parsed.value() != 42 || !target || target.value().get_host() != "example.com")
        return 1;
    std::cout << sindre::general::library_abi() << '\n';
    return 0;
}
