#pragma once

#include <general/core/async.hpp>
#include <array>
#include <cctype>
#include <charconv>
#include <limits>
#include <algorithm>
#include <vector>
#include <string>
#include <string_view>

namespace sindrecpp::general::versioning {

struct Version {
    int major = 0, minor = 0, patch = 0;
    std::string suffix;
    friend bool operator==(const Version &left, const Version &right) {
        return left.major == right.major && left.minor == right.minor &&
               left.patch == right.patch && left.suffix == right.suffix;
    }
    friend bool operator<(const Version &left, const Version &right) {
        if (left.major != right.major) return left.major < right.major;
        if (left.minor != right.minor) return left.minor < right.minor;
        if (left.patch != right.patch) return left.patch < right.patch;
        // A release is newer than its pre-release forms. Pre-release
        // identifiers follow SemVer's numeric-before-alpha ordering.
        if (left.suffix.empty() != right.suffix.empty()) return !left.suffix.empty();
        if (left.suffix.empty()) return false;
        auto tokenize = [](std::string_view value) {
            if (!value.empty() && (value.front() == '-' || value.front() == '+')) value.remove_prefix(1);
            std::vector<std::string_view> parts;
            std::size_t begin = 0;
            while (begin <= value.size()) {
                const auto end = value.find('.', begin);
                parts.push_back(value.substr(begin, end == std::string_view::npos ? end : end - begin));
                if (end == std::string_view::npos) break;
                begin = end + 1;
            }
            return parts;
        };
        const auto a = tokenize(left.suffix);
        const auto b = tokenize(right.suffix);
        for (std::size_t i = 0; i < (std::min)(a.size(), b.size()); ++i) {
            const bool an = !a[i].empty() && std::all_of(a[i].begin(), a[i].end(),
                [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
            const bool bn = !b[i].empty() && std::all_of(b[i].begin(), b[i].end(),
                [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
            if (an && bn) {
                const auto ai = std::string(a[i]);
                const auto bi = std::string(b[i]);
                if (ai.size() != bi.size()) return ai.size() < bi.size();
                if (ai != bi) return ai < bi;
            } else if (an != bn) {
                return an;
            } else if (a[i] != b[i]) {
                return a[i] < b[i];
            }
        }
        return a.size() < b.size();
    }
    friend bool operator>(const Version &left, const Version &right) { return right < left; }
    friend bool operator<=(const Version &left, const Version &right) { return !(right < left); }
    friend bool operator>=(const Version &left, const Version &right) { return !(left < right); }
};

inline ::sindrecpp::general::Result<Version> parse(std::string_view text) noexcept {
    Version result; std::array<int *, 3> fields{&result.major, &result.minor, &result.patch};
    std::size_t start = 0;
    for (std::size_t field = 0; field != fields.size(); ++field) {
        const auto end = text.find('.', start); const auto part = text.substr(start, end - start);
        if (part.empty()) return ::sindrecpp::general::Result<Version>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid version", "version.parse");
        std::size_t numeric_end = 0;
        while (numeric_end < part.size() && std::isdigit(static_cast<unsigned char>(part[numeric_end]))) ++numeric_end;
        if (!numeric_end || (field != 2 && numeric_end != part.size()))
            return ::sindrecpp::general::Result<Version>::failure(
                std::make_error_code(std::errc::invalid_argument), "Invalid version number", "version.parse");
        int value = 0;
        const auto parsed = std::from_chars(part.data(), part.data() + numeric_end, value);
        if (parsed.ec != std::errc{} || value < 0)
            return ::sindrecpp::general::Result<Version>::failure(
                parsed.ec == std::errc::result_out_of_range ? std::make_error_code(std::errc::result_out_of_range) :
                std::make_error_code(std::errc::invalid_argument), "Invalid version number", "version.parse");
        if (field == 2 && numeric_end != part.size()) {
            result.suffix = std::string(part.substr(numeric_end));
            if (result.suffix.front() != '-' && result.suffix.front() != '+')
                return ::sindrecpp::general::Result<Version>::failure(
                    std::make_error_code(std::errc::invalid_argument), "Invalid version suffix", "version.parse");
        }
        *fields[field] = value;
        if (end == std::string_view::npos) {
            if (field != 2) {
                return ::sindrecpp::general::Result<Version>::failure(
                    std::make_error_code(std::errc::invalid_argument),
                    "Version requires major.minor.patch", "version.parse");
            }
            break;
        }
        start = end + 1;
    }
    return ::sindrecpp::general::Result<Version>::success(std::move(result));
}

inline std::string to_string(const Version &value) {
    return std::to_string(value.major) + "." + std::to_string(value.minor) + "." +
           std::to_string(value.patch) + value.suffix;
}

} // namespace sindrecpp::general::versioning
