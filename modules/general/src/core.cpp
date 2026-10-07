#include <sindre/general/core.h>
#if defined(SINDRE_WITH_JSON)
#include <sindre/general/system.h>
#include <simdjson.h>
#endif

#include <sindre/general/core.h>

#if defined(SINDRE_WITH_JSON)

#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <random>
#include <system_error>

#if defined(SINDRE_WITH_ZLIB)
#include <zlib.h>
#endif
#if defined(SINDRE_WITH_JSON)
#include <simdjson.h>
#endif

namespace sindre::general::codec {

std::uint64_t fnv1a64(std::string_view text) noexcept {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto byte : text) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string hex(std::uint64_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(16, '0');
    for (int index = 15; index >= 0; --index) {
        result[static_cast<std::size_t>(index)] = digits[value & 0xf];
        value >>= 4;
    }
    return result;
}

Result<std::string> uuid4() {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::random_device entropy;
        std::array<std::uint8_t, 16> bytes{};
        for (auto &byte : bytes) byte = static_cast<std::uint8_t>(entropy());
        bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0f) | 0x40);
        bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3f) | 0x80);
        constexpr char digits[] = "0123456789abcdef";
        std::string result;
        result.reserve(36);
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            if (index == 4 || index == 6 || index == 8 || index == 10) result.push_back('-');
            result.push_back(digits[bytes[index] >> 4]);
            result.push_back(digits[bytes[index] & 0x0f]);
        }
        return Result<std::string>::success(std::move(result));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<std::string>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "codec.uuid4");
    } catch (...) {
        return Result<std::string>::failure(
            std::make_error_code(std::errc::io_error), "UUID generation failed", "codec.uuid4");
#endif
#if !defined(SINDRE_NO_EXCEPTIONS)
    }
#endif
}

Result<std::string> base64_encode(const std::vector<std::uint8_t> &data) {
    constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((data.size() + 2) / 3 * 4);
    for (std::size_t index = 0; index < data.size(); index += 3) {
        const auto a = data[index];
        const auto b = index + 1 < data.size() ? data[index + 1] : 0;
        const auto c = index + 2 < data.size() ? data[index + 2] : 0;
        const auto value = (static_cast<std::uint32_t>(a) << 16) |
                           (static_cast<std::uint32_t>(b) << 8) | c;
        result.push_back(alphabet[(value >> 18) & 63]);
        result.push_back(alphabet[(value >> 12) & 63]);
        result.push_back(index + 1 < data.size() ? alphabet[(value >> 6) & 63] : '=');
        result.push_back(index + 2 < data.size() ? alphabet[value & 63] : '=');
    }
    return Result<std::string>::success(std::move(result));
}

Result<std::vector<std::uint8_t>> base64_decode(std::string_view text) {
    const auto value = [](unsigned char byte) {
        if (byte >= 'A' && byte <= 'Z') return static_cast<int>(byte - 'A');
        if (byte >= 'a' && byte <= 'z') return static_cast<int>(byte - 'a' + 26);
        if (byte >= '0' && byte <= '9') return static_cast<int>(byte - '0' + 52);
        if (byte == '+') return 62;
        if (byte == '/') return 63;
        return -1;
    };
    if (text.size() % 4 != 0)
        return Result<std::vector<std::uint8_t>>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid Base64 length", "codec.base64");
    std::vector<std::uint8_t> result;
    result.reserve(text.size() / 4 * 3);
    for (std::size_t index = 0; index < text.size(); index += 4) {
        const auto a = value(static_cast<unsigned char>(text[index]));
        const auto b = value(static_cast<unsigned char>(text[index + 1]));
        const bool pad_c = text[index + 2] == '=';
        const bool pad_d = text[index + 3] == '=';
        const auto c = pad_c ? 0 : value(static_cast<unsigned char>(text[index + 2]));
        const auto d = pad_d ? 0 : value(static_cast<unsigned char>(text[index + 3]));
        if (a < 0 || b < 0 || c < 0 || d < 0 || (pad_c && !pad_d) ||
            ((pad_c || pad_d) && index + 4 != text.size()))
            return Result<std::vector<std::uint8_t>>::failure(
                std::make_error_code(std::errc::invalid_argument), "Invalid Base64 text", "codec.base64");
        const auto bits = (static_cast<std::uint32_t>(a) << 18) |
                          (static_cast<std::uint32_t>(b) << 12) |
                          (static_cast<std::uint32_t>(c) << 6) | static_cast<std::uint32_t>(d);
        result.push_back(static_cast<std::uint8_t>((bits >> 16) & 0xff));
        if (!pad_c) result.push_back(static_cast<std::uint8_t>((bits >> 8) & 0xff));
        if (!pad_d) result.push_back(static_cast<std::uint8_t>(bits & 0xff));
    }
    return Result<std::vector<std::uint8_t>>::success(std::move(result));
}

Result<std::vector<std::uint8_t>> simple_compress(const std::vector<std::uint8_t> &data) {
    std::vector<std::uint8_t> result;
    for (std::size_t index = 0; index < data.size();) {
        const auto byte = data[index];
        std::size_t count = 1;
        while (index + count < data.size() && data[index + count] == byte && count < 255) ++count;
        result.push_back(static_cast<std::uint8_t>(count));
        result.push_back(byte);
        index += count;
    }
    return Result<std::vector<std::uint8_t>>::success(std::move(result));
}

Result<std::vector<std::uint8_t>> simple_decompress(const std::vector<std::uint8_t> &data) {
    if (data.size() % 2 != 0)
        return Result<std::vector<std::uint8_t>>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid simple compressed data", "codec.rle");
    std::vector<std::uint8_t> result;
    for (std::size_t index = 0; index < data.size(); index += 2) {
        if (!data[index])
            return Result<std::vector<std::uint8_t>>::failure(
                std::make_error_code(std::errc::invalid_argument), "Zero run length", "codec.rle");
        result.insert(result.end(), data[index], data[index + 1]);
    }
    return Result<std::vector<std::uint8_t>>::success(std::move(result));
}

#if defined(SINDRE_WITH_ZLIB)
Result<std::vector<std::uint8_t>> zlib_compress(
    const std::vector<std::uint8_t> &data, int level) noexcept {
    if (level != Z_DEFAULT_COMPRESSION && (level < Z_BEST_SPEED || level > Z_BEST_COMPRESSION))
        return Result<std::vector<std::uint8_t>>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid zlib compression level", "codec.zlib");
    uLongf size = compressBound(static_cast<uLong>(data.size()));
    std::vector<std::uint8_t> result(static_cast<std::size_t>(size));
    if (compress2(result.data(), &size, data.data(), static_cast<uLong>(data.size()), level) != Z_OK)
        return Result<std::vector<std::uint8_t>>::failure(
            std::make_error_code(std::errc::io_error), "zlib compression failed", "codec.zlib");
    result.resize(static_cast<std::size_t>(size));
    return Result<std::vector<std::uint8_t>>::success(std::move(result));
}

Result<std::vector<std::uint8_t>> zlib_decompress(
    const std::vector<std::uint8_t> &data, std::size_t max_output) noexcept {
    if (max_output == 0)
        return Result<std::vector<std::uint8_t>>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid zlib output limit", "codec.zlib");
    std::vector<std::uint8_t> result((std::max)(std::size_t{64}, data.size() * 2));
    result.resize((std::min)(result.size(), max_output));
    int status = Z_BUF_ERROR;
    uLongf size = static_cast<uLongf>(result.size());
    while (status == Z_BUF_ERROR && result.size() < max_output) {
        status = uncompress(result.data(), &size, data.data(), static_cast<uLong>(data.size()));
        if (status == Z_BUF_ERROR) {
            result.resize((std::min)(max_output, result.size() * 2));
            size = static_cast<uLongf>(result.size());
        }
    }
    if (status != Z_OK)
        return Result<std::vector<std::uint8_t>>::failure(
            std::make_error_code(std::errc::invalid_argument), "zlib decompression failed", "codec.zlib");
    result.resize(static_cast<std::size_t>(size));
    return Result<std::vector<std::uint8_t>>::success(std::move(result));
}
#endif

} // namespace sindre::general::codec

namespace sindre::general::versioning {

bool operator==(const Version &left, const Version &right) noexcept {
    return left.major == right.major && left.minor == right.minor &&
           left.patch == right.patch && left.suffix == right.suffix;
}

bool operator!=(const Version &left, const Version &right) noexcept { return !(left == right); }

bool operator<(const Version &left, const Version &right) noexcept {
    if (left.major != right.major) return left.major < right.major;
    if (left.minor != right.minor) return left.minor < right.minor;
    if (left.patch != right.patch) return left.patch < right.patch;
    if (left.suffix.empty() != right.suffix.empty()) return !left.suffix.empty();
    if (left.suffix.empty()) return false;
    const auto tokenize = [](std::string_view value) {
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
    const auto left_parts = tokenize(left.suffix);
    const auto right_parts = tokenize(right.suffix);
    for (std::size_t index = 0; index < (std::min)(left_parts.size(), right_parts.size()); ++index) {
        const auto numeric = [](std::string_view part) {
            return !part.empty() && std::all_of(part.begin(), part.end(), [](char value) {
                return std::isdigit(static_cast<unsigned char>(value)) != 0;
            });
        };
        const bool left_numeric = numeric(left_parts[index]);
        const bool right_numeric = numeric(right_parts[index]);
        if (left_numeric && right_numeric) {
            if (left_parts[index].size() != right_parts[index].size())
                return left_parts[index].size() < right_parts[index].size();
            if (left_parts[index] != right_parts[index]) return left_parts[index] < right_parts[index];
        } else if (left_numeric != right_numeric) {
            return left_numeric;
        } else if (left_parts[index] != right_parts[index]) {
            return left_parts[index] < right_parts[index];
        }
    }
    return left_parts.size() < right_parts.size();
}

bool operator>(const Version &left, const Version &right) noexcept { return right < left; }
bool operator<=(const Version &left, const Version &right) noexcept { return !(right < left); }
bool operator>=(const Version &left, const Version &right) noexcept { return !(left < right); }

Result<Version> parse(std::string_view text) noexcept {
    Version result;
    std::array<int *, 3> fields{&result.major, &result.minor, &result.patch};
    std::size_t start = 0;
    for (std::size_t field = 0; field != fields.size(); ++field) {
        const auto end = text.find('.', start);
        const auto part = text.substr(start, end - start);
        if (part.empty()) return Result<Version>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid version", "version.parse");
        std::size_t numeric_end = 0;
        while (numeric_end < part.size() && std::isdigit(static_cast<unsigned char>(part[numeric_end]))) ++numeric_end;
        if (!numeric_end || (field != 2 && numeric_end != part.size()))
            return Result<Version>::failure(
                std::make_error_code(std::errc::invalid_argument), "Invalid version number", "version.parse");
        int value = 0;
        const auto parsed = std::from_chars(part.data(), part.data() + numeric_end, value);
        if (parsed.ec != std::errc{} || value < 0)
            return Result<Version>::failure(
                parsed.ec == std::errc::result_out_of_range
                    ? std::make_error_code(std::errc::result_out_of_range)
                    : std::make_error_code(std::errc::invalid_argument),
                "Invalid version number", "version.parse");
        if (field == 2 && numeric_end != part.size()) {
            result.suffix = std::string(part.substr(numeric_end));
            if (result.suffix.front() != '-' && result.suffix.front() != '+')
                return Result<Version>::failure(
                    std::make_error_code(std::errc::invalid_argument), "Invalid version suffix", "version.parse");
        }
        *fields[field] = value;
        if (end == std::string_view::npos) {
            if (field != 2) return Result<Version>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Version requires major.minor.patch", "version.parse");
            break;
        }
        start = end + 1;
    }
    return Result<Version>::success(std::move(result));
}

std::string to_string(const Version &value) {
    return std::to_string(value.major) + "." + std::to_string(value.minor) + "." +
           std::to_string(value.patch) + value.suffix;
}

} // namespace sindre::general::versioning

#if defined(SINDRE_WITH_JSON)
namespace sindre::general::json {
struct Document::State {
    explicit State(std::string_view value) : input(value) {
        auto parsed = parser.parse(input);
        error = parsed.error();
        if (error == simdjson::SUCCESS) root = parsed.value_unsafe();
    }
    simdjson::padded_string input;
    Parser parser;
    Element root;
    simdjson::error_code error = simdjson::SUCCESS;
};

Document::Document(std::shared_ptr<State> state) noexcept : state_(std::move(state)) {}
const Element &Document::root() const noexcept { return state_->root; }

Result<Document> try_parse(std::string_view json) {
    try {
        auto state = std::make_shared<Document::State>(json);
        if (state->error != simdjson::SUCCESS) return Result<Document>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "Invalid JSON: " + std::string(simdjson::error_message(state->error)), "json.parse");
        return Result<Document>::success(Document(std::move(state)));
    } catch (const std::exception &error) {
        return Result<Document>::failure(std::make_error_code(std::errc::invalid_argument), error.what(), "json.parse");
    } catch (...) {
        return Result<Document>::failure(std::make_error_code(std::errc::io_error), "Unknown JSON parsing failure", "json.parse");
    }
}
Result<Document> parse(std::string_view json) { return try_parse(json); }
} // namespace sindre::general::json
#endif

// Core's template/type-only pieces remain visible to callers.  This
// translation unit owns the non-template core implementation headers so the
// General runtime target has a stable compilation unit for future ABI work.

// ---- merged from library.cpp ----

#include <sindre/general/core.h>

namespace sindre::general {

const char *library_abi() noexcept {
    return "sindre.general.cxx17";
}

} // namespace sindre::general
