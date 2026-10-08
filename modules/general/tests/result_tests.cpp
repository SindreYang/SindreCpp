#include <sindre/general/core.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

using sindre::general::Error;
using sindre::general::Result;

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "Check failed: " #condition << " at line " << __LINE__ << '\n'; \
    return EXIT_FAILURE; \
} } while (false)

template <class R, class = void> struct has_temporary_data : std::false_type {};
template <class R> struct has_temporary_data<R, std::void_t<decltype(std::declval<R&&>().data())>>
    : std::true_type {};
template <class R, class = void> struct has_temporary_error_ptr : std::false_type {};
template <class R> struct has_temporary_error_ptr<R,
    std::void_t<decltype(std::declval<R&&>().error_ptr())>> : std::true_type {};
template <class R, class F, class = void> struct can_chain : std::false_type {};
template <class R, class F> struct can_chain<R, F,
    std::void_t<decltype(std::declval<R>().and_then(std::declval<F>()))>> : std::true_type {};

struct ReturnsInt { int operator()(int) const { return 1; } };
struct ReturnsResult { Result<int> operator()(int) const { return Result<int>::success(1); } };
struct RvalueOnlyContinuation {
    Result<int> operator()(int input) && { return Result<int>::success(input + 1); }
    Result<int> operator()(int) & = delete;
};

static_assert(!has_temporary_data<Result<int>>::value);
static_assert(!has_temporary_error_ptr<Result<int>>::value);
static_assert(!has_temporary_error_ptr<Result<void>>::value);
static_assert(!can_chain<Result<int>&, ReturnsInt>::value);
static_assert(can_chain<Result<int>&, ReturnsResult>::value);
static_assert(std::is_same_v<decltype(std::declval<Result<std::string>&&>().value()),
                             std::string>);
static_assert(std::is_same_v<decltype(std::declval<const Result<std::string>&&>().value()),
                             std::string>);
static_assert(std::is_same_v<decltype(std::declval<Result<int>&&>().error()), Error>);

int main() {
    auto value = Result<std::string>::success("hello");
    CHECK(value && value->size() == 5 && *value.data() == "hello");
    const auto copied = value;
    CHECK(copied && copied->size() == 5 && copied.value_ptr() != nullptr);
    auto extracted = Result<std::string>::success("temporary").value();
    CHECK(extracted == "temporary");

    auto owned = Result<std::unique_ptr<int>>::success(std::make_unique<int>(42));
    auto pointer = std::move(owned).value();
    CHECK(pointer && *pointer == 42);

    auto failure = Result<int>::failure(
        Error::make(std::errc::permission_denied, "denied", "result.test"));
    CHECK(!failure && failure.data() == nullptr && failure.error_ptr() != nullptr);
    auto propagated = std::move(failure).and_then([](int) { return Result<void>::success(); });
    CHECK(!propagated && propagated.error().code ==
        std::make_error_code(std::errc::permission_denied) &&
        propagated.error().context == "result.test");

    auto chain = Result<int>::success(3)
        .and_then([](int input) { return Result<int>::success(input + 2); })
        .and_then([](int input) { return Result<std::string>::success(std::to_string(input)); });
    CHECK(chain && chain.value() == "5");
    auto forwarded = Result<int>::success(4).and_then(RvalueOnlyContinuation{});
    CHECK(forwarded && forwarded.value() == 5);
    auto moved_value = Result<std::unique_ptr<int>>::success(std::make_unique<int>(8))
        .and_then([](std::unique_ptr<int> input) {
            return Result<int>::success(*input + 1);
        });
    CHECK(moved_value && moved_value.value() == 9);

    const auto no_value = Result<void>::success();
    no_value.value();
    auto from_void = no_value.and_then([] { return Result<int>::success(7); });
    CHECK(from_void && from_void.value() == 7);
    auto void_failure = Result<void>::failure(
        Error::make(std::errc::io_error, "disk", "result.void"));
    auto void_propagated = std::move(void_failure).and_then([] { return Result<int>::success(9); });
    CHECK(!void_propagated && void_propagated.error().context == "result.void");
    const auto const_void_failure = Result<void>::failure(
        Error::make(std::errc::io_error, "disk", "result.const_void"));
    auto const_void_propagated = const_void_failure.and_then([] {
        return Result<int>::success(10);
    });
    CHECK(!const_void_propagated &&
        const_void_propagated.error().context == "result.const_void");

#if !defined(SINDRE_NO_EXCEPTIONS)
    auto system_failure = Result<void>::success().and_then([]() -> Result<int> {
        throw std::system_error(std::make_error_code(std::errc::timed_out), "timeout");
    });
    CHECK(!system_failure && system_failure.error().code ==
        std::make_error_code(std::errc::timed_out));
    auto memory_failure = Result<int>::success(1).and_then([](int) -> Result<int> {
        throw std::bad_alloc();
    });
    CHECK(!memory_failure && memory_failure.error().code ==
        std::make_error_code(std::errc::not_enough_memory));
    auto ordinary_failure = Result<int>::success(1).and_then([](int) -> Result<int> {
        throw std::runtime_error("continuation failed");
    });
    CHECK(!ordinary_failure && ordinary_failure.error().context == "result.and_then" &&
        ordinary_failure.error().message == "continuation failed");
    auto unknown_failure = Result<int>::success(1).and_then([](int) -> Result<int> {
        throw 7;
    });
    CHECK(!unknown_failure && unknown_failure.error().context == "result.and_then");
#endif
    return EXIT_SUCCESS;
}
