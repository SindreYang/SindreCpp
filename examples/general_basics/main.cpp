#include <general/index.hpp>

#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

namespace general = sindrecpp::general;

static int report_failure(std::string_view operation, const general::Error &error) {
    std::cerr << operation << " failed: " << error.describe() << '\n';
    return 1;
}

int main() {
    const auto title = general::string::String("  SindreCpp  ")
                           .trim()
                           .replace("Cpp", "Toolkit");
    const auto number = general::string::parse_int(" 42 ");
    const auto ratio = general::string::parse_float(" 3.14 ");
    if (!number) return report_failure("parse integer", number.error());
    if (!ratio) return report_failure("parse float", ratio.error());

    const auto version = general::versioning::parse("1.2.3");
    if (!version) return report_failure("parse version", version.error());

    const std::vector<std::uint8_t> payload{'s', 'i', 'n', 'd', 'r', 'e'};
    const auto encoded = general::codec::base64_encode(payload);
    if (!encoded) return report_failure("encode base64", encoded.error());
    const auto decoded = general::codec::base64_decode(encoded.value());
    if (!decoded) return report_failure("decode base64", decoded.error());

    bool cleaned_up = false;
    {
        auto guard = general::scope_guard([&] { cleaned_up = true; });
    }
    if (!cleaned_up) {
        std::cerr << "scope guard did not run\n";
        return 1;
    }

    auto temporary = general::temp::File::create("sindrecpp-example-");
    if (!temporary) return report_failure("create temporary file", temporary.error());
    const auto written = general::path::write_text(temporary.value().path(), "SindreCpp UTF-8 text\n");
    if (!written) return report_failure("write UTF-8 text", written.error());
    const auto read = general::path::read_text(temporary.value().path());
    if (!read) return report_failure("read UTF-8 text", read.error());

    std::cout << "title: " << title.str() << '\n'
              << "number: " << number.value() << '\n'
              << "ratio: " << ratio.value() << '\n'
              << "version: " << general::versioning::to_string(version.value()) << '\n'
              << "base64: " << encoded.value() << '\n'
              << "file: " << general::path::to_utf8(temporary.value().path())
              << " (" << read.value().size() << " bytes)\n";
    return 0;
}
