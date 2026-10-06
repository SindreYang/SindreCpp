#pragma once

#include <general/core/async.hpp>
#include <general/core/string.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace sindrecpp::general::path {

inline std::string to_utf8(const std::filesystem::path &value) {
#if defined(__cpp_char8_t)
    const auto text = value.u8string();
    return {reinterpret_cast<const char *>(text.data()), text.size()};
#else
    return value.u8string();
#endif
}

inline std::filesystem::path from_utf8(std::string_view value) {
#if defined(_WIN32)
    return std::filesystem::u8path(value);
#else
    return std::filesystem::path(std::string(value));
#endif
}

inline ::sindrecpp::general::Result<std::string> try_to_utf8(const std::filesystem::path &value) noexcept {
#if defined(SINDRECPP_NO_EXCEPTIONS)
    return ::sindrecpp::general::Result<std::string>::success(to_utf8(value));
#else
    try {
        return ::sindrecpp::general::Result<std::string>::success(to_utf8(value));
    } catch (const std::exception &error) {
        return ::sindrecpp::general::Result<std::string>::failure(
            std::make_error_code(std::errc::illegal_byte_sequence), error.what(), "path.to_utf8");
    } catch (...) {
        return ::sindrecpp::general::Result<std::string>::failure(
            std::make_error_code(std::errc::illegal_byte_sequence), "Cannot convert path to UTF-8", "path.to_utf8");
    }
#endif
}

inline ::sindrecpp::general::Result<std::string> read_text(const std::filesystem::path &value) noexcept {
#if defined(SINDRECPP_NO_EXCEPTIONS)
    std::ifstream input(value, std::ios::binary);
    if (!input) return ::sindrecpp::general::Result<std::string>::failure(
        std::make_error_code(std::errc::no_such_file_or_directory), "Cannot open file", "path.read_text");
    std::ostringstream content; content << input.rdbuf();
    auto result = content.str();
    if (!input.good() && !input.eof()) return ::sindrecpp::general::Result<std::string>::failure(
        std::make_error_code(std::errc::io_error), "Cannot read file", "path.read_text");
    if (!::sindrecpp::general::string::valid_utf8(result)) return ::sindrecpp::general::Result<std::string>::failure(
        std::make_error_code(std::errc::illegal_byte_sequence), "File is not UTF-8", "path.read_text");
    return ::sindrecpp::general::Result<std::string>::success(std::move(result));
#else
    try {
        std::ifstream input(value, std::ios::binary);
        if (!input)
            return ::sindrecpp::general::Result<std::string>::failure(
                std::make_error_code(std::errc::no_such_file_or_directory), "Cannot open file", "path.read_text");
        std::ostringstream content;
        content << input.rdbuf();
        if (!input.good() && !input.eof())
            return ::sindrecpp::general::Result<std::string>::failure(
                std::make_error_code(std::errc::io_error), "Cannot read file", "path.read_text");
        auto result = content.str();
        if (!::sindrecpp::general::string::valid_utf8(result))
            return ::sindrecpp::general::Result<std::string>::failure(
                std::make_error_code(std::errc::illegal_byte_sequence), "File is not UTF-8", "path.read_text");
        return ::sindrecpp::general::Result<std::string>::success(std::move(result));
    } catch (const std::exception &error) {
        return ::sindrecpp::general::Result<std::string>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "path.read_text");
    }
#endif
}

inline ::sindrecpp::general::Result<void> write_text(const std::filesystem::path &value,
                                                     std::string_view text) noexcept {
    if (!::sindrecpp::general::string::valid_utf8(text))
        return ::sindrecpp::general::Result<void>::failure(
            std::make_error_code(std::errc::illegal_byte_sequence), "Text is not UTF-8", "path.write_text");
#if defined(SINDRECPP_NO_EXCEPTIONS)
    std::ofstream output(value, std::ios::binary | std::ios::trunc);
    if (!output) return ::sindrecpp::general::Result<void>::failure(
        std::make_error_code(std::errc::permission_denied), "Cannot open file", "path.write_text");
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) return ::sindrecpp::general::Result<void>::failure(
        std::make_error_code(std::errc::io_error), "Cannot write file", "path.write_text");
    return ::sindrecpp::general::Result<void>::success();
#else
    try {
        std::ofstream output(value, std::ios::binary | std::ios::trunc);
        if (!output)
            return ::sindrecpp::general::Result<void>::failure(
                std::make_error_code(std::errc::permission_denied), "Cannot open file", "path.write_text");
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!output)
            return ::sindrecpp::general::Result<void>::failure(
                std::make_error_code(std::errc::io_error), "Cannot write file", "path.write_text");
        return ::sindrecpp::general::Result<void>::success();
    } catch (const std::exception &error) {
        return ::sindrecpp::general::Result<void>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "path.write_text");
    }
#endif
}

inline ::sindrecpp::general::Result<std::filesystem::path>
require_file(const std::filesystem::path &value) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(value, error))
        return ::sindrecpp::general::Result<std::filesystem::path>::failure(
            error ? error : std::make_error_code(std::errc::no_such_file_or_directory),
            "Regular file does not exist", "path.require_file");
    return ::sindrecpp::general::Result<std::filesystem::path>::success(value);
}

} // namespace sindrecpp::general::path
