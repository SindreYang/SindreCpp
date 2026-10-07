#include <sindre/general/core.h>
#if defined(SINDRE_WITH_JSON)
#include <sindre/general/system.h>
#include <simdjson.h>
#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <exception>
#include <limits>
#include <random>
#include <system_error>
#include <type_traits>

#if defined(SINDRE_WITH_ZLIB)
#include <zlib.h>
#endif
#if defined(SINDRE_WITH_JSON)
#include <simdjson.h>
#endif

namespace sindre::general::codec {

namespace {

template <class T, class Function>
Result<T> codec_boundary(const char *context, Function &&function) {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
        return std::forward<Function>(function)();
    } catch (const std::bad_alloc &) {
        return Result<T>::failure(
            std::make_error_code(std::errc::not_enough_memory),
            "Not enough memory", context);
    } catch (const std::exception &error) {
        return Result<T>::failure(
            std::make_error_code(std::errc::io_error), error.what(), context);
    } catch (...) {
        return Result<T>::failure(
            std::make_error_code(std::errc::io_error),
            "Unknown codec failure", context);
    }
#else
    static_cast<void>(context);
    return std::forward<Function>(function)();
#endif
}

} // namespace

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

Result<std::string> try_hex(std::uint64_t value) {
    return codec_boundary<std::string>("codec.hex", [value] {
        return Result<std::string>::success(hex(value));
    });
}

Result<std::string> uuid4() {
    return codec_boundary<std::string>("codec.uuid4", [] {
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
    });
}

Result<std::string> base64_encode(const std::vector<std::uint8_t> &data) {
    return codec_boundary<std::string>("codec.base64", [&] {
        constexpr char alphabet[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        const auto groups = data.size() / 3 + (data.size() % 3 != 0 ? 1 : 0);
        if (groups > std::string().max_size() / 4)
            return Result<std::string>::failure(
                std::make_error_code(std::errc::not_enough_memory),
                "Encoded data is too large", "codec.base64");
        std::string result;
        result.reserve(groups * 4);
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
    });
}

Result<std::vector<std::uint8_t>> base64_decode(std::string_view text) {
    return codec_boundary<std::vector<std::uint8_t>>("codec.base64", [&] {
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
              ((pad_c || pad_d) && index + 4 != text.size()) ||
              (pad_c && (b & 0x0f) != 0) || (pad_d && !pad_c && (c & 0x03) != 0))
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
    });
}

Result<std::vector<std::uint8_t>> rle_compress(const std::vector<std::uint8_t> &data) {
    return codec_boundary<std::vector<std::uint8_t>>("codec.rle", [&] {
        if (data.size() > (std::vector<std::uint8_t>().max_size() / 2))
            return Result<std::vector<std::uint8_t>>::failure(
                std::make_error_code(std::errc::not_enough_memory),
                "Compressed data is too large", "codec.rle");
        std::vector<std::uint8_t> result;
        result.reserve(data.size() * 2);
        for (std::size_t index = 0; index < data.size();) {
            const auto byte = data[index];
            std::size_t count = 1;
            while (index + count < data.size() && data[index + count] == byte && count < 255) ++count;
            result.push_back(static_cast<std::uint8_t>(count));
            result.push_back(byte);
            index += count;
        }
        return Result<std::vector<std::uint8_t>>::success(std::move(result));
    });
}

Result<std::vector<std::uint8_t>> rle_decompress(const std::vector<std::uint8_t> &data) {
    return codec_boundary<std::vector<std::uint8_t>>("codec.rle", [&] {
        if (data.size() % 2 != 0)
            return Result<std::vector<std::uint8_t>>::failure(
                std::make_error_code(std::errc::invalid_argument), "Invalid simple compressed data", "codec.rle");
        const auto max_size = std::vector<std::uint8_t>().max_size();
        std::size_t output_size = 0;
        for (std::size_t index = 0; index < data.size(); index += 2) {
            if (data[index] > max_size - output_size)
                return Result<std::vector<std::uint8_t>>::failure(
                    std::make_error_code(std::errc::not_enough_memory),
                    "Decompressed data is too large", "codec.rle");
            output_size += data[index];
        }
        std::vector<std::uint8_t> result;
        result.reserve(output_size);
        for (std::size_t index = 0; index < data.size(); index += 2) {
            if (!data[index])
                return Result<std::vector<std::uint8_t>>::failure(
                    std::make_error_code(std::errc::invalid_argument), "Zero run length", "codec.rle");
            result.insert(result.end(), data[index], data[index + 1]);
        }
        return Result<std::vector<std::uint8_t>>::success(std::move(result));
    });
}

#if defined(SINDRE_WITH_ZLIB)
Result<std::vector<std::uint8_t>> zlib_compress(
    const std::vector<std::uint8_t> &data, int level) {
    return codec_boundary<std::vector<std::uint8_t>>("codec.zlib", [&] {
        if (level != Z_DEFAULT_COMPRESSION && (level < Z_BEST_SPEED || level > Z_BEST_COMPRESSION))
            return Result<std::vector<std::uint8_t>>::failure(
                std::make_error_code(std::errc::invalid_argument), "Invalid zlib compression level", "codec.zlib");
        if (data.size() > static_cast<std::size_t>((std::numeric_limits<uLong>::max)()))
            return Result<std::vector<std::uint8_t>>::failure(
                std::make_error_code(std::errc::value_too_large),
                "Input is too large for zlib", "codec.zlib");
        uLongf size = compressBound(static_cast<uLong>(data.size()));
        std::vector<std::uint8_t> result(static_cast<std::size_t>(size));
        if (compress2(result.data(), &size, data.data(), static_cast<uLong>(data.size()), level) != Z_OK)
            return Result<std::vector<std::uint8_t>>::failure(
                std::make_error_code(std::errc::io_error), "zlib compression failed", "codec.zlib");
        result.resize(static_cast<std::size_t>(size));
        return Result<std::vector<std::uint8_t>>::success(std::move(result));
    });
}

Result<std::vector<std::uint8_t>> zlib_decompress(
    const std::vector<std::uint8_t> &data, std::size_t max_output) {
    return codec_boundary<std::vector<std::uint8_t>>("codec.zlib", [&] {
        if (max_output == 0)
            return Result<std::vector<std::uint8_t>>::failure(
                std::make_error_code(std::errc::invalid_argument), "Invalid zlib output limit", "codec.zlib");
        if (data.size() > static_cast<std::size_t>((std::numeric_limits<uLong>::max)()) ||
            max_output > static_cast<std::size_t>((std::numeric_limits<uLongf>::max)()))
            return Result<std::vector<std::uint8_t>>::failure(
                std::make_error_code(std::errc::value_too_large),
                "Input or output limit is too large for zlib", "codec.zlib");
        const auto doubled_size = data.size() > (std::numeric_limits<std::size_t>::max)() / 2
                                      ? (std::numeric_limits<std::size_t>::max)()
                                      : data.size() * 2;
        const auto initial_size = (std::min)(max_output, (std::max)(std::size_t{64}, doubled_size));
        std::vector<std::uint8_t> result(initial_size);
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
    });
}
#endif

} // namespace sindre::general::codec

namespace sindre::general::versioning {

namespace {

bool is_identifier_character(unsigned char character) noexcept {
    return (character >= '0' && character <= '9') ||
           (character >= 'A' && character <= 'Z') ||
           (character >= 'a' && character <= 'z') || character == '-';
}

bool is_numeric_identifier(std::string_view value) noexcept {
    if (value.empty()) return false;
    for (const auto character : value) {
        if (character < '0' || character > '9') return false;
    }
    return true;
}

bool valid_identifier_list(std::string_view value, bool reject_numeric_leading_zero) noexcept {
    if (value.empty()) return false;
    std::size_t begin = 0;
    for (;;) {
        const auto end = value.find('.', begin);
        const auto identifier = value.substr(
            begin, end == std::string_view::npos ? end : end - begin);
        if (identifier.empty()) return false;
        for (const auto character : identifier) {
            if (!is_identifier_character(static_cast<unsigned char>(character))) return false;
        }
        if (reject_numeric_leading_zero && is_numeric_identifier(identifier) &&
            identifier.size() > 1 && identifier.front() == '0')
            return false;
        if (end == std::string_view::npos) return true;
        begin = end + 1;
    }
}

int compare_identifiers(std::string_view left, std::string_view right) noexcept {
    const bool left_numeric = is_numeric_identifier(left);
    const bool right_numeric = is_numeric_identifier(right);
    if (left_numeric != right_numeric) return left_numeric ? -1 : 1;
    if (left_numeric) {
        while (left.size() > 1 && left.front() == '0') left.remove_prefix(1);
        while (right.size() > 1 && right.front() == '0') right.remove_prefix(1);
        if (left.size() != right.size()) return left.size() < right.size() ? -1 : 1;
    }
    if (left == right) return 0;
    return left < right ? -1 : 1;
}

int compare_prerelease(std::string_view left, std::string_view right) noexcept {
    if (left.empty() != right.empty()) return left.empty() ? 1 : -1;
    if (left.empty()) return 0;
    std::size_t left_begin = 0;
    std::size_t right_begin = 0;
    for (;;) {
        const auto left_end = left.find('.', left_begin);
        const auto right_end = right.find('.', right_begin);
        const auto comparison = compare_identifiers(
            left.substr(left_begin, left_end == std::string_view::npos
                                      ? left_end : left_end - left_begin),
            right.substr(right_begin, right_end == std::string_view::npos
                                       ? right_end : right_end - right_begin));
        if (comparison != 0) return comparison;
        if (left_end == std::string_view::npos || right_end == std::string_view::npos) {
            if (left_end == right_end) return 0;
            return left_end == std::string_view::npos ? -1 : 1;
        }
        left_begin = left_end + 1;
        right_begin = right_end + 1;
    }
}

Result<Version> invalid_version(std::errc code, const char *message) {
    return Result<Version>::failure(std::make_error_code(code), message, "version.parse");
}

} // namespace

bool operator==(const Version &left, const Version &right) noexcept {
    return left.major == right.major && left.minor == right.minor &&
           left.patch == right.patch && left.prerelease == right.prerelease;
}

bool operator!=(const Version &left, const Version &right) noexcept { return !(left == right); }

bool operator<(const Version &left, const Version &right) noexcept {
    if (left.major != right.major) return left.major < right.major;
    if (left.minor != right.minor) return left.minor < right.minor;
    if (left.patch != right.patch) return left.patch < right.patch;
    return compare_prerelease(left.prerelease, right.prerelease) < 0;
}

bool operator>(const Version &left, const Version &right) noexcept { return right < left; }
bool operator<=(const Version &left, const Version &right) noexcept { return !(right < left); }
bool operator>=(const Version &left, const Version &right) noexcept { return !(left < right); }

Result<Version> parse(std::string_view text) {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        if (text.empty())
            return invalid_version(std::errc::invalid_argument, "Version must not be empty");
        const auto first_dot = text.find('.');
        const auto second_dot = first_dot == std::string_view::npos
                                    ? std::string_view::npos
                                    : text.find('.', first_dot + 1);
        if (first_dot == std::string_view::npos || second_dot == std::string_view::npos)
            return invalid_version(std::errc::invalid_argument, "SemVer requires major.minor.patch");

        Version result;
        const auto parse_core_number = [](std::string_view part, int &output) {
            if (part.empty() || (part.size() > 1 && part.front() == '0'))
                return std::errc::invalid_argument;
            const auto parsed = std::from_chars(part.data(), part.data() + part.size(), output);
            if (parsed.ec != std::errc{}) return parsed.ec;
            return parsed.ptr == part.data() + part.size()
                       ? std::errc{}
                       : std::errc::invalid_argument;
        };

        const auto major_status = parse_core_number(text.substr(0, first_dot), result.major);
        if (major_status != std::errc{})
            return invalid_version(major_status == std::errc::result_out_of_range
                                       ? std::errc::result_out_of_range
                                       : std::errc::invalid_argument,
                                   "Invalid major version number");
        const auto minor_status = parse_core_number(
            text.substr(first_dot + 1, second_dot - first_dot - 1), result.minor);
        if (minor_status != std::errc{})
            return invalid_version(minor_status == std::errc::result_out_of_range
                                       ? std::errc::result_out_of_range
                                       : std::errc::invalid_argument,
                                   "Invalid minor version number");

        const auto patch_and_suffix = text.substr(second_dot + 1);
        const auto suffix_begin = patch_and_suffix.find_first_of("-+");
        const auto patch_text = patch_and_suffix.substr(0, suffix_begin);
        if (patch_text.empty())
            return invalid_version(std::errc::invalid_argument, "Invalid patch version number");
        const auto patch_status = parse_core_number(patch_text, result.patch);
        if (patch_status != std::errc{})
            return invalid_version(patch_status == std::errc::result_out_of_range
                                       ? std::errc::result_out_of_range
                                       : std::errc::invalid_argument,
                                   "Invalid patch version number");
        if (suffix_begin != std::string_view::npos) {
            const auto suffix = patch_and_suffix.substr(suffix_begin);
            const auto build_separator = suffix.find('+');
            if (suffix.front() == '-') {
                const auto prerelease_end = build_separator == std::string_view::npos
                                                ? suffix.size() : build_separator;
                const auto prerelease = suffix.substr(1, prerelease_end - 1);
                if (!valid_identifier_list(prerelease, true))
                    return invalid_version(std::errc::invalid_argument, "Invalid SemVer prerelease");
                result.prerelease.assign(prerelease.data(), prerelease.size());
            } else if (suffix.front() != '+') {
                return invalid_version(std::errc::invalid_argument, "Invalid SemVer suffix");
            }
            if (build_separator != std::string_view::npos) {
                const auto build = suffix.substr(build_separator + 1);
                if (!valid_identifier_list(build, false))
                    return invalid_version(std::errc::invalid_argument, "Invalid SemVer build metadata");
                result.build.assign(build.data(), build.size());
            } else if (suffix.front() == '+') {
                const auto build = suffix.substr(1);
                if (!valid_identifier_list(build, false))
                    return invalid_version(std::errc::invalid_argument, "Invalid SemVer build metadata");
                result.build.assign(build.data(), build.size());
            }
        }
        return Result<Version>::success(std::move(result));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::bad_alloc &) {
        return invalid_version(std::errc::not_enough_memory, "Not enough memory while parsing version");
    } catch (...) {
        return invalid_version(std::errc::invalid_argument, "Version parsing failed");
    }
#endif
}

std::string to_string(const Version &value) {
    std::string result = std::to_string(value.major) + "." + std::to_string(value.minor) + "." +
                         std::to_string(value.patch);
    if (!value.prerelease.empty()) result += "-" + value.prerelease;
    if (!value.build.empty()) result += "+" + value.build;
    return result;
}

Result<std::string> try_to_string(const Version &value) {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        return Result<std::string>::success(to_string(value));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::bad_alloc &) {
        return Result<std::string>::failure(
            std::make_error_code(std::errc::not_enough_memory),
            "Not enough memory", "version.to_string");
    } catch (const std::exception &error) {
        return Result<std::string>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "version.to_string");
    } catch (...) {
        return Result<std::string>::failure(
            std::make_error_code(std::errc::io_error),
            "Unknown version formatting failure", "version.to_string");
    }
#endif
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

namespace {

sindre::general::Error value_error(const char *message, const char *context) {
    return sindre::general::Error{std::make_error_code(std::errc::invalid_argument), message, context};
}

void append_json_string(std::string &output, std::string_view value) {
    static constexpr char hex[] = "0123456789abcdef";
    output.push_back('"');
    for (const unsigned char character : value) {
        switch (character) {
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            if (character < 0x20u) {
                output += "\\u00";
                output.push_back(hex[(character >> 4u) & 0x0fu]);
                output.push_back(hex[character & 0x0fu]);
            } else {
                output.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    output.push_back('"');
}

Result<void> append_json_value(std::string &output, const Value &value) {
    const auto &storage = value.get_storage();
    return std::visit([&output](const auto &item) -> Result<void> {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            output += "null";
        } else if constexpr (std::is_same_v<T, bool>) {
            output += item ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::string>) {
            append_json_string(output, item);
        } else if constexpr (std::is_same_v<T, double>) {
            if (!std::isfinite(item))
                return Result<void>::failure(
                    std::make_error_code(std::errc::invalid_argument),
                    "JSON number must be finite", "json.stringify");
            std::ostringstream text;
            text.precision(17);
            text << item;
            output += text.str();
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Object>>) {
            output.push_back('{');
            bool first = true;
            for (const auto &entry : *item) {
                if (!first) output.push_back(',');
                first = false;
                append_json_string(output, entry.first);
                output.push_back(':');
                auto status = append_json_value(output, entry.second);
                if (!status) return status;
            }
            output.push_back('}');
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Array>>) {
            output.push_back('[');
            bool first = true;
            for (const auto &entry : *item) {
                if (!first) output.push_back(',');
                first = false;
                auto status = append_json_value(output, entry);
                if (!status) return status;
            }
            output.push_back(']');
        } else {
            output += std::to_string(item);
        }
        return Result<void>::success();
    }, storage);
}

} // namespace

Value::Value() : storage_(nullptr) {}
Value::Value(std::nullptr_t) : storage_(nullptr) {}
Value::Value(bool value) : storage_(value) {}
Value::Value(float value) : storage_(static_cast<double>(value)) {}
Value::Value(double value) : storage_(value) {}
Value::Value(const char *value) : storage_(std::string(value ? value : "")) {}
Value::Value(std::string value) : storage_(std::move(value)) {}
Value::Value(std::string_view value) : storage_(std::string(value)) {}
Value::Value(Object value) : storage_(std::make_shared<Object>(std::move(value))) {}
Value::Value(Array value) : storage_(std::make_shared<Array>(std::move(value))) {}

Value::Value(const Value &other) {
    storage_ = std::visit([](const auto &item) -> Storage {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, std::shared_ptr<Object>>)
            return std::make_shared<Object>(*item);
        else if constexpr (std::is_same_v<T, std::shared_ptr<Array>>)
            return std::make_shared<Array>(*item);
        else
            return item;
    }, other.storage_);
}

Value::Value(Value &&other) noexcept = default;
Value &Value::operator=(Value &&other) noexcept = default;
Value::~Value() = default;

Value &Value::operator=(const Value &other) {
    if (this == &other) return *this;
    Value copy(other);
    storage_ = std::move(copy.storage_);
    return *this;
}

bool Value::is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(storage_); }
bool Value::is_bool() const noexcept { return std::holds_alternative<bool>(storage_); }
bool Value::is_integer() const noexcept {
    return std::holds_alternative<std::int64_t>(storage_) ||
           std::holds_alternative<std::uint64_t>(storage_);
}
bool Value::is_number() const noexcept { return is_integer() || std::holds_alternative<double>(storage_); }
bool Value::is_string() const noexcept { return std::holds_alternative<std::string>(storage_); }
bool Value::is_object() const noexcept {
    return std::holds_alternative<std::shared_ptr<Object>>(storage_);
}
bool Value::is_array() const noexcept {
    return std::holds_alternative<std::shared_ptr<Array>>(storage_);
}

void Value::ensure_object() {
    if (!is_object()) storage_ = std::make_shared<Object>();
}

Value &Value::operator[](std::string_view key) {
    ensure_object();
    return (*std::get<std::shared_ptr<Object>>(storage_))[key];
}

const Value *Value::find(std::string_view key) const noexcept {
    const auto *object = std::get_if<std::shared_ptr<Object>>(&storage_);
    return object && *object ? (*object)->find(key) : nullptr;
}

const Value *Value::at(std::size_t index) const noexcept {
    const auto *array = std::get_if<std::shared_ptr<Array>>(&storage_);
    return array && *array ? (*array)->at(index) : nullptr;
}

Object *Value::get_object() noexcept {
    const auto *object = std::get_if<std::shared_ptr<Object>>(&storage_);
    return object && *object ? object->get() : nullptr;
}
const Object *Value::get_object() const noexcept {
    const auto *object = std::get_if<std::shared_ptr<Object>>(&storage_);
    return object && *object ? object->get() : nullptr;
}
Array *Value::get_array() noexcept {
    const auto *array = std::get_if<std::shared_ptr<Array>>(&storage_);
    return array && *array ? array->get() : nullptr;
}
const Array *Value::get_array() const noexcept {
    const auto *array = std::get_if<std::shared_ptr<Array>>(&storage_);
    return array && *array ? array->get() : nullptr;
}

void Value::set(std::string key, Value value) {
    ensure_object();
    (*std::get<std::shared_ptr<Object>>(storage_))[key] = std::move(value);
}

void Value::push_back(Value value) {
    if (!is_array()) storage_ = std::make_shared<Array>();
    std::get<std::shared_ptr<Array>>(storage_)->push_back(std::move(value));
}

Result<std::string> Value::get_string() const {
    const auto *value = std::get_if<std::string>(&storage_);
    return value ? Result<std::string>::success(*value) :
        Result<std::string>::failure(value_error("JSON value is not a string", "json.get_string"));
}

Result<std::int64_t> Value::get_int() const {
    if (const auto *value = std::get_if<std::int64_t>(&storage_))
        return Result<std::int64_t>::success(*value);
    if (const auto *value = std::get_if<std::uint64_t>(&storage_)) {
        if (*value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            return Result<std::int64_t>::success(static_cast<std::int64_t>(*value));
    }
    return Result<std::int64_t>::failure(value_error("JSON value is not an integer", "json.get_int"));
}

Result<double> Value::get_float() const {
    if (const auto *value = std::get_if<double>(&storage_)) return Result<double>::success(*value);
    if (const auto integer = get_int()) return Result<double>::success(static_cast<double>(integer.value()));
    return Result<double>::failure(value_error("JSON value is not a number", "json.get_float"));
}

Result<bool> Value::get_bool() const {
    const auto *value = std::get_if<bool>(&storage_);
    return value ? Result<bool>::success(*value) :
        Result<bool>::failure(value_error("JSON value is not a boolean", "json.get_bool"));
}

Result<std::string> Value::to_json() const noexcept { return stringify(*this); }

Object::Object(std::initializer_list<Field> fields) {
    for (const auto &field : fields) values_[field.first] = field.second;
}
Object::Object(Fields fields) {
    for (auto &field : fields) values_[std::move(field.first)] = std::move(field.second);
}
Value &Object::operator[](std::string_view key) { return values_[std::string(key)]; }
const Value *Object::find(std::string_view key) const noexcept {
    auto it = values_.find(std::string(key));
    return it == values_.end() ? nullptr : &it->second;
}
void Object::set(std::string key, Value value) { values_[std::move(key)] = std::move(value); }

Array::Array(std::initializer_list<Value> values) : values_(values) {}
void Array::push_back(Value value) { values_.push_back(std::move(value)); }

Object object(std::initializer_list<Field> fields) { return Object(fields); }
Array array(std::initializer_list<Value> values) { return Array(values); }

Result<std::string> stringify(const Value &value) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::string result;
        auto status = append_json_value(result, value);
        if (!status) return Result<std::string>::failure(status.error());
        return Result<std::string>::success(std::move(result));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::bad_alloc &) {
        return Result<std::string>::failure(
            std::make_error_code(std::errc::not_enough_memory),
            "Not enough memory", "json.stringify");
    } catch (const std::exception &error) {
        return Result<std::string>::failure(
            std::make_error_code(std::errc::invalid_argument), error.what(), "json.stringify");
    } catch (...) {
        return Result<std::string>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "Unknown JSON serialization failure", "json.stringify");
    }
#endif
}

Result<std::string> build(const Fields &fields) noexcept {
    return stringify(Value(Object(fields)));
}

Result<Value> parse_element(const Element &element, std::string context) {
    using Type = simdjson::dom::element_type;
    switch (element.type()) {
    case Type::NULL_VALUE: return Result<Value>::success(Value(nullptr));
    case Type::BOOL: return Result<Value>::success(Value(element.get_bool().value_unsafe()));
    case Type::INT64: return Result<Value>::success(Value(element.get_int64().value_unsafe()));
    case Type::UINT64: return Result<Value>::success(Value(element.get_uint64().value_unsafe()));
    case Type::DOUBLE: return Result<Value>::success(Value(element.get_double().value_unsafe()));
    case Type::STRING: return Result<Value>::success(Value(std::string(element.get_string().value_unsafe())));
    case Type::ARRAY: {
        Array result;
        auto native_array = element.get_array();
        if (native_array.error() != simdjson::SUCCESS)
            return Result<Value>::failure(value_error("Invalid JSON array", context.c_str()));
        for (const auto child : native_array.value_unsafe()) {
            auto value = parse_element(child, context + "[]");
            if (!value) return value;
            result.push_back(std::move(value.value()));
        }
        return Result<Value>::success(Value(std::move(result)));
    }
    case Type::OBJECT: {
        Object result;
        auto native_object = element.get_object();
        if (native_object.error() != simdjson::SUCCESS)
            return Result<Value>::failure(value_error("Invalid JSON object", context.c_str()));
        for (const auto field : native_object.value_unsafe()) {
            auto value = parse_element(field.value, context + "." + std::string(field.key));
            if (!value) return value;
            result.set(std::string(field.key), std::move(value.value()));
        }
        return Result<Value>::success(Value(std::move(result)));
    }
    default:
        return Result<Value>::failure(value_error("Unsupported JSON value", context.c_str()));
    }
}

Result<Value> parse(std::string_view text) {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        auto document = try_parse(text);
        if (!document) return Result<Value>::failure(document.error());
        return parse_element(document.value().root(), "json");
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::bad_alloc &) {
        return Result<Value>::failure(std::make_error_code(std::errc::not_enough_memory),
                                      "Not enough memory", "json.parse");
    } catch (const std::exception &error) {
        return Result<Value>::failure(std::make_error_code(std::errc::invalid_argument),
                                      error.what(), "json.parse");
    } catch (...) {
        return Result<Value>::failure(std::make_error_code(std::errc::io_error),
                                      "Unknown JSON parsing failure", "json.parse");
    }
#endif
}

Result<Document> try_parse(std::string_view json) {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        auto state = std::make_shared<Document::State>(json);
        if (state->error != simdjson::SUCCESS) return Result<Document>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "Invalid JSON: " + std::string(simdjson::error_message(state->error)), "json.parse");
        return Result<Document>::success(Document(std::move(state)));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<Document>::failure(std::make_error_code(std::errc::invalid_argument), error.what(), "json.parse");
    } catch (...) {
        return Result<Document>::failure(std::make_error_code(std::errc::io_error), "Unknown JSON parsing failure", "json.parse");
    }
#endif
}
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
