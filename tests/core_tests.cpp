#include <sindrecpp/core.hpp>
#include <sindrecpp/pointer.hpp>
#include <sindrecpp/string.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "Check failed at " << __FILE__ << ':' << __LINE__ << ": " #condition << '\n'; \
    return EXIT_FAILURE; \
} } while (false)

int main() {
    auto value = sindrecpp::Result<int>::success(42);
    CHECK(value);
    CHECK(value.value() == 42);

    auto failure = sindrecpp::Result<int>::failure({{}, "expected failure"});
    CHECK(!failure);
    CHECK(failure.error().message == "expected failure");

    auto no_value = sindrecpp::Result<void>::success();
    CHECK(no_value);
    auto no_value_error = sindrecpp::Result<void>::failure({{}, "failed"});
    CHECK(!no_value_error);
    CHECK(no_value_error.error().message == "failed");

    CHECK(sindrecpp::general::string::trim(" \t hello\r\n") == "hello");
    CHECK(sindrecpp::general::string::trim(" \t\r\n").empty());
    CHECK(sindrecpp::general::string::starts_with("SindreCpp", "Sindre"));
    CHECK(sindrecpp::general::string::ends_with("SindreCpp", "Cpp"));
    CHECK(sindrecpp::general::string::replace_all("a-b-a", "a", "x") == "x-b-x");
    const auto parts = sindrecpp::general::string::split("one,two,", ',');
    CHECK(parts.size() == 3 && parts[0] == "one" && parts[2].empty());

    auto pointer = sindrecpp::general::pointer::make_unique<std::string>("owned");
    CHECK(*pointer == "owned");
    CHECK(std::string(sindrecpp::version) == "0.1.0");
}
