#pragma once

#include <sindrecpp/version.hpp>

#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>

namespace sindrecpp {

struct Error {
    std::error_code code{};
    std::string message;

    explicit operator bool() const noexcept { return static_cast<bool>(code) || !message.empty(); }
};

template <class T>
class Result {
public:
    static Result success(T value) { return Result(std::in_place_index<0>, std::move(value)); }
    static Result failure(Error error) { return Result(std::in_place_index<1>, std::move(error)); }

    bool has_value() const noexcept { return value_.index() == 0; }
    explicit operator bool() const noexcept { return has_value(); }
    T& value() & { return std::get<0>(value_); }
    const T& value() const& { return std::get<0>(value_); }
    T&& value() && { return std::get<0>(std::move(value_)); }
    Error& error() & { return std::get<1>(value_); }
    const Error& error() const& { return std::get<1>(value_); }

private:
    template <class... Args>
    explicit Result(std::in_place_index_t<0> tag, Args&&... args)
        : value_(tag, std::forward<Args>(args)...) {}
    template <class... Args>
    explicit Result(std::in_place_index_t<1> tag, Args&&... args)
        : value_(tag, std::forward<Args>(args)...) {}

    std::variant<T, Error> value_;
};

template <>
class Result<void> {
public:
    static Result success() { return Result(true, {}); }
    static Result failure(Error error) { return Result(false, std::move(error)); }

    bool has_value() const noexcept { return ok_; }
    explicit operator bool() const noexcept { return ok_; }
    const Error& error() const& { return error_; }

private:
    Result(bool ok, Error error) : ok_(ok), error_(std::move(error)) {}
    bool ok_;
    Error error_;
};

} // namespace sindrecpp
