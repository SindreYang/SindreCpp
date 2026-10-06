#pragma once

#include <general/core/async.hpp>
#include <algorithm>
#include <iterator>
#include <type_traits>
#include <vector>

namespace sindrecpp::general::ranges {

template <class Range, class Value>
bool contains(const Range &range, const Value &value) {
    using std::begin;
    using std::end;
    return std::find(begin(range), end(range), value) != end(range);
}

template <class Range, class Function>
auto transform(const Range &range, Function function)
    -> std::vector<std::decay_t<decltype(function(*std::begin(range)))>> {
    using Output = std::decay_t<decltype(function(*std::begin(range)))>;
    std::vector<Output> result;
    result.reserve(static_cast<std::size_t>(std::distance(std::begin(range), std::end(range))));
    for (const auto &value : range) result.push_back(function(value));
    return result;
}

template <class Range, class Function>
::sindrecpp::general::Result<void> for_each_result(const Range &range, Function function) noexcept {
#if defined(SINDRECPP_NO_EXCEPTIONS)
    for (const auto &value : range) {
        auto status = function(value);
        if (!status) return status;
    }
    return ::sindrecpp::general::Result<void>::success();
#else
    try {
        for (const auto &value : range) {
            auto status = function(value);
            if (!status) return status;
        }
        return ::sindrecpp::general::Result<void>::success();
    } catch (const std::exception &error) {
        return ::sindrecpp::general::Result<void>::failure(
            std::make_error_code(std::errc::invalid_argument), error.what(), "ranges.for_each");
    } catch (...) {
        return ::sindrecpp::general::Result<void>::failure(
            std::make_error_code(std::errc::invalid_argument), "Unknown range operation failure",
            "ranges.for_each");
    }
#endif
}

} // namespace sindrecpp::general::ranges
