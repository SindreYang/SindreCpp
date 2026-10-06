#pragma once

#include <general/core/async.hpp>

#include <array>
#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#if defined(SINDRECPP_WITH_ZLIB)
#include <zlib.h>
#endif

namespace sindrecpp::general::codec {

inline std::uint64_t fnv1a64(std::string_view text) noexcept {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto byte : text) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 1099511628211ull;
    }
    return hash;
}

inline std::string hex(std::uint64_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(16, '0');
    for (int i = 15; i >= 0; --i) {
        result[std::size_t(i)] = digits[value & 0xf];
        value >>= 4;
    }
    return result;
}

inline ::sindrecpp::general::Result<std::string> uuid4() {
#if defined(SINDRECPP_NO_EXCEPTIONS)
    std::random_device entropy;
    std::array<std::uint8_t, 16> bytes{};
    for (auto &byte : bytes) byte = static_cast<std::uint8_t>(entropy());
    bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0f) | 0x40);
    bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3f) | 0x80);
    constexpr char digits[] = "0123456789abcdef";
    std::string result; result.reserve(36);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) result.push_back('-');
        result.push_back(digits[bytes[i] >> 4]); result.push_back(digits[bytes[i] & 0x0f]);
    }
    return ::sindrecpp::general::Result<std::string>::success(std::move(result));
#else
    try {
        std::random_device entropy;
        std::array<std::uint8_t, 16> bytes{};
        for (auto &byte : bytes)
            byte = static_cast<std::uint8_t>(entropy());
        bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0f) | 0x40);
        bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3f) | 0x80);
        constexpr char digits[] = "0123456789abcdef";
        std::string result;
        result.reserve(36);
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) result.push_back('-');
            result.push_back(digits[bytes[i] >> 4]);
            result.push_back(digits[bytes[i] & 0x0f]);
        }
        return ::sindrecpp::general::Result<std::string>::success(std::move(result));
    } catch (const std::exception &error) {
        return ::sindrecpp::general::Result<std::string>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "codec.uuid4");
    }
#endif
}

inline ::sindrecpp::general::Result<std::string> base64_encode(const std::vector<std::uint8_t> &data) {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((data.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < data.size(); i += 3) {
        const std::uint32_t a = data[i];
        const std::uint32_t b = i + 1 < data.size() ? data[i + 1] : 0;
        const std::uint32_t c = i + 2 < data.size() ? data[i + 2] : 0;
        const auto value = (a << 16) | (b << 8) | c;
        result.push_back(alphabet[(value >> 18) & 63]);
        result.push_back(alphabet[(value >> 12) & 63]);
        result.push_back(i + 1 < data.size() ? alphabet[(value >> 6) & 63] : '=');
        result.push_back(i + 2 < data.size() ? alphabet[value & 63] : '=');
    }
    return ::sindrecpp::general::Result<std::string>::success(std::move(result));
}

inline ::sindrecpp::general::Result<std::vector<std::uint8_t>> base64_decode(std::string_view text) {
    auto value = [](unsigned char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    if (text.size() % 4 != 0)
        return ::sindrecpp::general::Result<std::vector<std::uint8_t>>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid Base64 length", "codec.base64");
    std::vector<std::uint8_t> result;
    result.reserve(text.size() / 4 * 3);
    for (std::size_t i = 0; i < text.size(); i += 4) {
        const int a = value(static_cast<unsigned char>(text[i]));
        const int b = value(static_cast<unsigned char>(text[i + 1]));
        const bool pad_c = text[i + 2] == '=';
        const bool pad_d = text[i + 3] == '=';
        const int c = pad_c ? 0 : value(static_cast<unsigned char>(text[i + 2]));
        const int d = pad_d ? 0 : value(static_cast<unsigned char>(text[i + 3]));
        if (a < 0 || b < 0 || c < 0 || d < 0 || (pad_c && !pad_d) ||
            ((pad_c || pad_d) && i + 4 != text.size()))
            return ::sindrecpp::general::Result<std::vector<std::uint8_t>>::failure(
                std::make_error_code(std::errc::invalid_argument), "Invalid Base64 text", "codec.base64");
        const auto bits = (static_cast<std::uint32_t>(a) << 18) |
                          (static_cast<std::uint32_t>(b) << 12) |
                          (static_cast<std::uint32_t>(c) << 6) | static_cast<std::uint32_t>(d);
        result.push_back(static_cast<std::uint8_t>((bits >> 16) & 0xff));
        if (!pad_c) result.push_back(static_cast<std::uint8_t>((bits >> 8) & 0xff));
        if (!pad_d) result.push_back(static_cast<std::uint8_t>(bits & 0xff));
    }
    return ::sindrecpp::general::Result<std::vector<std::uint8_t>>::success(std::move(result));
}

inline ::sindrecpp::general::Result<std::vector<std::uint8_t>>
simple_compress(const std::vector<std::uint8_t> &data) {
    std::vector<std::uint8_t> result;
    for (std::size_t i = 0; i < data.size();) {
        const auto value = data[i];
        std::size_t count = 1;
        while (i + count < data.size() && data[i + count] == value && count < 255) ++count;
        result.push_back(static_cast<std::uint8_t>(count));
        result.push_back(value);
        i += count;
    }
    return ::sindrecpp::general::Result<std::vector<std::uint8_t>>::success(std::move(result));
}

inline ::sindrecpp::general::Result<std::vector<std::uint8_t>>
simple_decompress(const std::vector<std::uint8_t> &data) {
    if (data.size() % 2 != 0)
        return ::sindrecpp::general::Result<std::vector<std::uint8_t>>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid simple compressed data", "codec.rle");
    std::vector<std::uint8_t> result;
    for (std::size_t i = 0; i < data.size(); i += 2) {
        if (!data[i])
            return ::sindrecpp::general::Result<std::vector<std::uint8_t>>::failure(
                std::make_error_code(std::errc::invalid_argument), "Zero run length", "codec.rle");
        result.insert(result.end(), data[i], data[i + 1]);
    }
    return ::sindrecpp::general::Result<std::vector<std::uint8_t>>::success(std::move(result));
}

#if defined(SINDRECPP_WITH_ZLIB)
inline ::sindrecpp::general::Result<std::vector<std::uint8_t>>
zlib_compress(const std::vector<std::uint8_t> &data, int level = Z_DEFAULT_COMPRESSION) noexcept {
    if (level != Z_DEFAULT_COMPRESSION && (level < Z_BEST_SPEED || level > Z_BEST_COMPRESSION))
        return ::sindrecpp::general::Result<std::vector<std::uint8_t>>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid zlib compression level", "codec.zlib");
    uLong bound = compressBound(static_cast<uLong>(data.size()));
    std::vector<std::uint8_t> result(static_cast<std::size_t>(bound));
    uLongf size = bound;
    const auto status = compress2(result.data(), &size, data.data(), static_cast<uLong>(data.size()), level);
    if (status != Z_OK)
        return ::sindrecpp::general::Result<std::vector<std::uint8_t>>::failure(
            std::make_error_code(std::errc::io_error), "zlib compression failed", "codec.zlib");
    result.resize(static_cast<std::size_t>(size));
    return ::sindrecpp::general::Result<std::vector<std::uint8_t>>::success(std::move(result));
}

inline ::sindrecpp::general::Result<std::vector<std::uint8_t>>
zlib_decompress(const std::vector<std::uint8_t> &data, std::size_t max_output = 256 * 1024 * 1024) noexcept {
    if (max_output == 0)
        return ::sindrecpp::general::Result<std::vector<std::uint8_t>>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid zlib output limit", "codec.zlib");
    std::vector<std::uint8_t> result((std::min<std::size_t>)((std::max<std::size_t>)(data.size() * 2, 64), max_output));
    uLongf size = static_cast<uLongf>(result.size());
    int status = Z_BUF_ERROR;
    while (status == Z_BUF_ERROR && result.size() < max_output) {
        status = uncompress(result.data(), &size, data.data(), static_cast<uLong>(data.size()));
        if (status == Z_BUF_ERROR) {
            result.resize((std::min)(max_output, result.size() * 2));
            size = static_cast<uLongf>(result.size());
        }
    }
    if (status != Z_OK)
        return ::sindrecpp::general::Result<std::vector<std::uint8_t>>::failure(
            std::make_error_code(std::errc::invalid_argument), "zlib decompression failed", "codec.zlib");
    result.resize(static_cast<std::size_t>(size));
    return ::sindrecpp::general::Result<std::vector<std::uint8_t>>::success(std::move(result));
}
#endif

} // namespace sindrecpp::general::codec
