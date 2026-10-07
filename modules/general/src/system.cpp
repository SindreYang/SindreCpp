#include <sindre/general/system.h>
#include <sindre/general/string.h>

#if defined(SINDRE_WITH_JSON)

#include <sindre/general/string.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace sindre::general::config {

Result<void> Config::flatten(const json::Element &element, std::string prefix, Config &result) {
    using Type = simdjson::dom::element_type;
    switch (element.type()) {
    case Type::OBJECT: {
        auto object = element.get_object();
        if (object.error() != simdjson::SUCCESS)
            return invalid("Invalid JSON object", prefix);
        for (auto field : object.value_unsafe()) {
            const std::string key(field.key);
            const auto full = prefix.empty() ? key : prefix + "." + key;
            auto status = flatten(field.value, full, result);
            if (!status) return status;
        }
        return Result<void>::success();
    }
    case Type::STRING: {
        auto value = element.get_string();
        if (value.error() != simdjson::SUCCESS)
            return invalid("Invalid JSON string", prefix);
        result.values_[std::move(prefix)] = std::string(value.value_unsafe());
        return Result<void>::success();
    }
    case Type::INT64:
        result.values_[std::move(prefix)] = std::to_string(element.get_int64().value_unsafe());
        return Result<void>::success();
    case Type::UINT64:
        result.values_[std::move(prefix)] = std::to_string(element.get_uint64().value_unsafe());
        return Result<void>::success();
    case Type::DOUBLE: {
        std::ostringstream text;
        text.precision(17);
        text << element.get_double().value_unsafe();
        result.values_[std::move(prefix)] = text.str();
        return Result<void>::success();
    }
    case Type::BOOL:
        result.values_[std::move(prefix)] = element.get_bool().value_unsafe() ? "true" : "false";
        return Result<void>::success();
    default:
        return invalid("JSON arrays and null values are not scalar configuration entries", prefix);
    }
}

Result<void> Config::invalid(std::string message, const std::string &key) {
    return Result<void>::failure(
        std::make_error_code(std::errc::invalid_argument), std::move(message), "config." + key);
}

const std::string *Config::find(std::string_view key) const {
    auto it = values_.find(std::string(key));
    return it == values_.end() ? nullptr : &it->second;
}

Config Config::with_defaults(std::initializer_list<std::pair<std::string, std::string>> defaults) {
    Config result;
    for (const auto &entry : defaults) result.values_[entry.first] = entry.second;
    return result;
}

void Config::merge(const Config &override_values) {
    for (const auto &entry : override_values.values_) values_[entry.first] = entry.second;
}

Result<Config> Config::from_json(std::string_view text) {
    return from_json(text, Config{});
}

Result<Config> Config::from_json(std::string_view text, const Config &defaults) {
    auto document = json::try_parse(text);
    if (!document) return Result<Config>::failure(document.error());
    Config result = defaults;
    auto status = flatten(document.value().root(), {}, result);
    if (!status) return Result<Config>::failure(status.error());
    return Result<Config>::success(std::move(result));
}

Result<Config> Config::from_file(const std::filesystem::path &path) {
    return from_file(path, Config{});
}

Result<Config> Config::from_file(const std::filesystem::path &path, const Config &defaults) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return Result<Config>::failure(
            std::make_error_code(std::errc::no_such_file_or_directory), "Cannot open config file",
            "config.file");
    std::ostringstream text;
    text << input.rdbuf();
    if (!input.good() && !input.eof())
        return Result<Config>::failure(
            std::make_error_code(std::errc::io_error), "Cannot read config file", "config.file");
    return from_json(text.str(), defaults);
}

void Config::set(std::string key, std::string value) {
    values_[std::move(key)] = std::move(value);
}

Result<void> Config::apply_environment(std::string_view prefix) {
    for (auto &[key, value] : values_) {
        std::string name(prefix);
        if (!name.empty()) name.push_back('_');
        for (const char c : key) {
            name.push_back(c == '.' ? '_' :
                          static_cast<char>(c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c));
        }
#if defined(_WIN32)
        char *environment = nullptr;
        std::size_t environment_size = 0;
        if (_dupenv_s(&environment, &environment_size, name.c_str()) == 0 && environment) {
            value = environment;
            std::free(environment);
        }
#else
        if (const char *environment = std::getenv(name.c_str())) value = environment;
#endif
    }
    return Result<void>::success();
}

Result<std::string> Config::get_string(std::string_view key) const {
    if (const auto *value = find(key)) return Result<std::string>::success(*value);
    return Result<std::string>::failure(
        std::make_error_code(std::errc::no_such_file_or_directory), "Configuration key is missing",
        "config." + std::string(key));
}

std::string Config::get_string_or(std::string_view key, std::string fallback) const {
    if (const auto *value = find(key)) return *value;
    return fallback;
}

Result<std::int64_t> Config::get_int(std::string_view key) const {
    auto value = get_string(key);
    if (!value) return Result<std::int64_t>::failure(value.error());
    auto parsed = string::parse_int(value.value());
    if (!parsed)
        return Result<std::int64_t>::failure(
            parsed.error().code, parsed.error().message, "config." + std::string(key));
    return parsed;
}

Result<bool> Config::get_bool(std::string_view key) const {
    auto value = get_string(key);
    if (!value) return Result<bool>::failure(value.error());
    const auto normalized = string::lower_ascii(value.value());
    if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on")
        return Result<bool>::success(true);
    if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off")
        return Result<bool>::success(false);
    return Result<bool>::failure(
        std::make_error_code(std::errc::invalid_argument), "Invalid boolean configuration value",
        "config." + std::string(key));
}

Result<double> Config::get_float(std::string_view key) const {
    auto value = get_string(key);
    if (!value) return Result<double>::failure(value.error());
    auto parsed = string::parse_float(value.value());
    if (!parsed) return Result<double>::failure(
        parsed.error().code, parsed.error().message, "config." + std::string(key));
    return parsed;
}

double Config::get_float_or(std::string_view key, double fallback) const {
    auto value = find(key);
    if (!value) return fallback;
    auto parsed = string::parse_float(*value);
    return parsed ? parsed.value() : fallback;
}

} // namespace sindre::general::config

#endif

// ---- merged from directory.cpp ----

#include <algorithm>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace sindre::general {

namespace detail {

Error directory_error(std::error_code code, std::string message,
                             std::string context) {
    return Error{code, std::move(message), std::move(context)};
}

bool glob_component_match(std::string_view pattern,
                                 std::string_view value) noexcept {
    std::size_t pattern_index = 0;
    std::size_t value_index = 0;
    std::size_t star_index = std::string_view::npos;
    std::size_t star_value_index = 0;

    while (value_index < value.size()) {
        if (pattern_index < pattern.size() &&
            (pattern[pattern_index] == '?' || pattern[pattern_index] == value[value_index])) {
            ++pattern_index;
            ++value_index;
        } else if (pattern_index < pattern.size() && pattern[pattern_index] == '*') {
            star_index = pattern_index++;
            star_value_index = value_index;
        } else if (star_index != std::string_view::npos) {
            pattern_index = star_index + 1;
            value_index = ++star_value_index;
        } else {
            return false;
        }
    }

    while (pattern_index < pattern.size() && pattern[pattern_index] == '*')
        ++pattern_index;
    return pattern_index == pattern.size();
}

bool glob_has_wildcard(std::string_view value) noexcept {
    return value.find_first_of("*?") != std::string_view::npos;
}

std::filesystem::path join_directory_path(const std::filesystem::path &base,
                                                  const std::filesystem::path &part) {
    return base.empty() ? part : base / part;
}

void sort_directory_paths(std::vector<std::filesystem::path> &paths) {
    std::sort(paths.begin(), paths.end(), [](const auto &left, const auto &right) {
        return path::to_utf8(left) < path::to_utf8(right);
    });
}

} // namespace detail

Result<std::vector<std::filesystem::path>>
list_directory(const std::filesystem::path &directory, bool recursive) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::error_code error;
        if (!std::filesystem::is_directory(directory, error)) {
            if (!error)
                error = std::make_error_code(std::errc::not_a_directory);
            return Result<std::vector<std::filesystem::path>>::failure(
                detail::directory_error(error, "Directory does not exist", "filesystem.list_directory"));
        }

        std::vector<std::filesystem::path> result;
        if (recursive) {
            std::filesystem::recursive_directory_iterator iterator(directory, error);
            if (error)
                return Result<std::vector<std::filesystem::path>>::failure(
                    detail::directory_error(error, "Cannot open directory", "filesystem.list_directory"));
            const std::filesystem::recursive_directory_iterator end;
            for (; iterator != end; iterator.increment(error)) {
                if (error)
                    return Result<std::vector<std::filesystem::path>>::failure(
                        detail::directory_error(error, "Cannot enumerate directory",
                                                "filesystem.list_directory"));
                result.push_back(iterator->path());
            }
        } else {
            std::filesystem::directory_iterator iterator(directory, error);
            if (error)
                return Result<std::vector<std::filesystem::path>>::failure(
                    detail::directory_error(error, "Cannot open directory", "filesystem.list_directory"));
            const std::filesystem::directory_iterator end;
            for (; iterator != end; iterator.increment(error)) {
                if (error)
                    return Result<std::vector<std::filesystem::path>>::failure(
                        detail::directory_error(error, "Cannot enumerate directory",
                                                "filesystem.list_directory"));
                result.push_back(iterator->path());
            }
        }

        detail::sort_directory_paths(result);
        return Result<std::vector<std::filesystem::path>>::success(std::move(result));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<std::vector<std::filesystem::path>>::failure(
            detail::directory_error(std::make_error_code(std::errc::io_error), error.what(),
                                    "filesystem.list_directory"));
    } catch (...) {
        return Result<std::vector<std::filesystem::path>>::failure(
            detail::directory_error(std::make_error_code(std::errc::io_error),
                                    "Cannot enumerate directory", "filesystem.list_directory"));
    }
#endif
}

Result<std::vector<std::filesystem::path>>
glob(std::string_view pattern) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        const auto pattern_path = path::from_utf8(pattern);
        const auto relative_pattern = pattern_path.relative_path();
        std::vector<std::filesystem::path> components;
        for (const auto &component : relative_pattern)
            components.push_back(component);

        const auto root = pattern_path.root_path();
        std::vector<std::filesystem::path> result;
        std::function<Result<void>(const std::filesystem::path &, std::size_t)> walk;
        walk = [&](const std::filesystem::path &current, std::size_t index) -> Result<void> {
            if (index == components.size()) {
                if (!current.empty()) {
                    std::error_code error;
                    if (std::filesystem::exists(current, error) && !error)
                        result.push_back(current);
                    else if (error && error != std::errc::no_such_file_or_directory)
                        return Result<void>::failure(detail::directory_error(
                            error, "Cannot inspect glob result", "filesystem.glob"));
                }
                return Result<void>::success();
            }

            const auto component = path::to_utf8(components[index]);
            if (component == "**") {
                auto zero_levels = walk(current, index + 1);
                if (!zero_levels) return zero_levels;

                const auto scan_directory = current.empty()
                                                ? std::filesystem::path(".")
                                                : current;
                std::error_code error;
                std::filesystem::directory_iterator iterator(scan_directory, error);
                if (error == std::errc::no_such_file_or_directory)
                    return Result<void>::success();
                if (error)
                    return Result<void>::failure(detail::directory_error(
                        error, "Cannot open glob directory", "filesystem.glob"));
                const std::filesystem::directory_iterator end;
                for (; iterator != end; iterator.increment(error)) {
                    if (error)
                        return Result<void>::failure(detail::directory_error(
                            error, "Cannot enumerate glob directory", "filesystem.glob"));
                    std::error_code status_error;
                    if (std::filesystem::is_directory(iterator->path(), status_error) &&
                        !status_error) {
                        const auto child = current.empty() ? iterator->path().filename()
                                                           : iterator->path();
                        auto nested = walk(child, index);
                        if (!nested) return nested;
                    }
                }
                return Result<void>::success();
            }

            const auto scan_directory = current.empty()
                                            ? std::filesystem::path(".")
                                            : current;
            if (detail::glob_has_wildcard(component)) {
                std::error_code error;
                std::filesystem::directory_iterator iterator(scan_directory, error);
                if (error == std::errc::no_such_file_or_directory)
                    return Result<void>::success();
                if (error)
                    return Result<void>::failure(detail::directory_error(
                        error, "Cannot open glob directory", "filesystem.glob"));
                const std::filesystem::directory_iterator end;
                for (; iterator != end; iterator.increment(error)) {
                    if (error)
                        return Result<void>::failure(detail::directory_error(
                            error, "Cannot enumerate glob directory", "filesystem.glob"));
                    const auto name = path::to_utf8(iterator->path().filename());
                    if (!detail::glob_component_match(component, name)) continue;
                    const auto child = current.empty() ? iterator->path().filename()
                                                       : iterator->path();
                    auto nested = walk(child, index + 1);
                    if (!nested) return nested;
                }
                return Result<void>::success();
            }

            const auto child = detail::join_directory_path(current, components[index]);
            std::error_code error;
            if (!std::filesystem::exists(child, error) || error)
                return error && error != std::errc::no_such_file_or_directory
                           ? Result<void>::failure(detail::directory_error(
                                 error, "Cannot inspect glob path", "filesystem.glob"))
                           : Result<void>::success();
            return walk(child, index + 1);
        };

        auto completed = walk(root, 0);
        if (!completed)
            return Result<std::vector<std::filesystem::path>>::failure(completed.error());
        detail::sort_directory_paths(result);
        return Result<std::vector<std::filesystem::path>>::success(std::move(result));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<std::vector<std::filesystem::path>>::failure(
            detail::directory_error(std::make_error_code(std::errc::io_error), error.what(),
                                    "filesystem.glob"));
    } catch (...) {
        return Result<std::vector<std::filesystem::path>>::failure(
            detail::directory_error(std::make_error_code(std::errc::io_error),
                                    "Cannot match glob pattern", "filesystem.glob"));
    }
#endif
}

} // namespace sindre::general

// ---- merged from file.cpp ----

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

namespace sindre::general {

namespace file_detail {

Error make_error(std::error_code code, std::string message,
                        std::string context) {
    return Error{code, std::move(message), std::move(context)};
}

Result<std::ifstream> open_binary(const std::filesystem::path &path,
                                          const char *context) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return Result<std::ifstream>::failure(make_error(
                std::make_error_code(std::errc::no_such_file_or_directory),
                "Cannot open file", context));
        return Result<std::ifstream>::success(std::move(input));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<std::ifstream>::failure(make_error(
            std::make_error_code(std::errc::io_error), error.what(), context));
    } catch (...) {
        return Result<std::ifstream>::failure(make_error(
            std::make_error_code(std::errc::io_error), "Cannot open file", context));
    }
#endif
}

std::string hex_bytes(const std::uint8_t *bytes, std::size_t size) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        result.push_back(digits[bytes[i] >> 4]);
        result.push_back(digits[bytes[i] & 0x0f]);
    }
    return result;
}

class Md5 {
public:
    Md5() noexcept
        : state_{0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u} {}

    void update(const std::uint8_t *data, std::size_t size) noexcept {
        total_bytes_ += size;
        while (size != 0) {
            const auto count = (std::min)(size, block_.size() - buffered_);
            std::memcpy(block_.data() + buffered_, data, count);
            buffered_ += count;
            data += count;
            size -= count;
            if (buffered_ == block_.size()) {
                transform(block_.data());
                buffered_ = 0;
            }
        }
    }

    std::string finish() noexcept {
        const auto bit_length = static_cast<std::uint64_t>(total_bytes_) * 8u;
        block_[buffered_++] = 0x80;
        if (buffered_ > 56) {
            std::fill(block_.begin() + buffered_, block_.end(), 0);
            transform(block_.data());
            buffered_ = 0;
        }
        std::fill(block_.begin() + buffered_, block_.begin() + 56, 0);
        for (std::size_t i = 0; i < sizeof(bit_length); ++i)
            block_[56 + i] = static_cast<std::uint8_t>(bit_length >> (i * 8));
        transform(block_.data());

        std::array<std::uint8_t, 16> digest{};
        for (std::size_t i = 0; i < state_.size(); ++i)
            for (std::size_t byte = 0; byte < 4; ++byte)
                digest[i * 4 + byte] = static_cast<std::uint8_t>(state_[i] >> (byte * 8));
        return hex_bytes(digest.data(), digest.size());
    }

private:
    static constexpr std::array<std::uint32_t, 64> shifts{
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

    static constexpr std::array<std::uint32_t, 64> constants{
        0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu,
        0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
        0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
        0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
        0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
        0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
        0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
        0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
        0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
        0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
        0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u,
        0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
        0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u,
        0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
        0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
        0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u};

    static constexpr std::uint32_t rotate_left(std::uint32_t value,
                                                 std::uint32_t count) noexcept {
        return (value << count) | (value >> (32 - count));
    }

    void transform(const std::uint8_t *block) noexcept {
        std::array<std::uint32_t, 16> words{};
        for (std::size_t i = 0; i < words.size(); ++i)
            for (std::size_t byte = 0; byte < 4; ++byte)
                words[i] |= static_cast<std::uint32_t>(block[i * 4 + byte]) << (byte * 8);

        auto a = state_[0];
        auto b = state_[1];
        auto c = state_[2];
        auto d = state_[3];
        for (std::uint32_t i = 0; i < 64; ++i) {
            std::uint32_t function = 0;
            std::uint32_t index = 0;
            if (i < 16) {
                function = (b & c) | ((~b) & d);
                index = i;
            } else if (i < 32) {
                function = (d & b) | ((~d) & c);
                index = (5 * i + 1) % 16;
            } else if (i < 48) {
                function = b ^ c ^ d;
                index = (3 * i + 5) % 16;
            } else {
                function = c ^ (b | (~d));
                index = (7 * i) % 16;
            }
            const auto next = d;
            d = c;
            c = b;
            b = b + rotate_left(a + function + constants[i] + words[index], shifts[i]);
            a = next;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
    }

    std::array<std::uint32_t, 4> state_{};
    std::array<std::uint8_t, 64> block_{};
    std::size_t buffered_ = 0;
    std::uint64_t total_bytes_ = 0;
};

class Sha256 {
public:
    Sha256() noexcept
        : state_{0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u} {}

    void update(const std::uint8_t *data, std::size_t size) noexcept {
        total_bytes_ += size;
        while (size != 0) {
            const auto count = (std::min)(size, block_.size() - buffered_);
            std::memcpy(block_.data() + buffered_, data, count);
            buffered_ += count;
            data += count;
            size -= count;
            if (buffered_ == block_.size()) {
                transform(block_.data());
                buffered_ = 0;
            }
        }
    }

    std::string finish() noexcept {
        const auto bit_length = static_cast<std::uint64_t>(total_bytes_) * 8u;
        block_[buffered_++] = 0x80;
        if (buffered_ > 56) {
            std::fill(block_.begin() + buffered_, block_.end(), 0);
            transform(block_.data());
            buffered_ = 0;
        }
        std::fill(block_.begin() + buffered_, block_.begin() + 56, 0);
        for (std::size_t i = 0; i < sizeof(bit_length); ++i)
            block_[56 + i] = static_cast<std::uint8_t>(bit_length >> ((7 - i) * 8));
        transform(block_.data());

        std::array<std::uint8_t, 32> digest{};
        for (std::size_t i = 0; i < state_.size(); ++i)
            for (std::size_t byte = 0; byte < 4; ++byte)
                digest[i * 4 + byte] = static_cast<std::uint8_t>(state_[i] >> ((3 - byte) * 8));
        return hex_bytes(digest.data(), digest.size());
    }

private:
    static constexpr std::array<std::uint32_t, 64> constants{
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
        0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
        0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
        0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

    static constexpr std::uint32_t rotate_right(std::uint32_t value,
                                                  std::uint32_t count) noexcept {
        return (value >> count) | (value << (32 - count));
    }

    void transform(const std::uint8_t *block) noexcept {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t i = 0; i < 16; ++i)
            words[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
                       (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
                       (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
                       static_cast<std::uint32_t>(block[i * 4 + 3]);
        for (std::size_t i = 16; i < words.size(); ++i) {
            const auto s0 = rotate_right(words[i - 15], 7) ^ rotate_right(words[i - 15], 18) ^
                            (words[i - 15] >> 3);
            const auto s1 = rotate_right(words[i - 2], 17) ^ rotate_right(words[i - 2], 19) ^
                            (words[i - 2] >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }

        auto a = state_[0];
        auto b = state_[1];
        auto c = state_[2];
        auto d = state_[3];
        auto e = state_[4];
        auto f = state_[5];
        auto g = state_[6];
        auto h = state_[7];
        for (std::size_t i = 0; i < words.size(); ++i) {
            const auto s1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
            const auto choose = (e & f) ^ ((~e) & g);
            const auto temp1 = h + s1 + choose + constants[i] + words[i];
            const auto s0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temp2 = s0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{};
    std::array<std::uint8_t, 64> block_{};
    std::size_t buffered_ = 0;
    std::uint64_t total_bytes_ = 0;
};

template <class Hash>
Result<std::string> hash_file(const std::filesystem::path &path,
                                     const char *context) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        auto input = open_binary(path, context);
        if (!input) return Result<std::string>::failure(input.error());

        Hash hash;
        std::array<std::uint8_t, 64 * 1024> buffer{};
        while (input.value().read(reinterpret_cast<char *>(buffer.data()),
                                  static_cast<std::streamsize>(buffer.size())) ||
               input.value().gcount() != 0) {
            hash.update(buffer.data(), static_cast<std::size_t>(input.value().gcount()));
        }
        if (!input.value().eof())
            return Result<std::string>::failure(make_error(
                std::make_error_code(std::errc::io_error), "Cannot read file", context));
        return Result<std::string>::success(hash.finish());
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<std::string>::failure(make_error(
            std::make_error_code(std::errc::io_error), error.what(), context));
    } catch (...) {
        return Result<std::string>::failure(make_error(
            std::make_error_code(std::errc::io_error), "Cannot hash file", context));
    }
#endif
}

} // namespace file_detail

Result<std::uint64_t> file_size(const std::filesystem::path &path) noexcept {
    std::error_code error;
    const auto value = std::filesystem::file_size(path, error);
    if (error)
        return Result<std::uint64_t>::failure(
            file_detail::make_error(error, "Cannot read file size", "filesystem.file_size"));
    return Result<std::uint64_t>::success(static_cast<std::uint64_t>(value));
}

Result<bool> file_exists(const std::filesystem::path &path) noexcept {
    std::error_code error;
    const auto value = std::filesystem::exists(path, error);
    if (error)
        return Result<bool>::failure(
            file_detail::make_error(error, "Cannot inspect file", "filesystem.file_exists"));
    return Result<bool>::success(value);
}

Result<FileInfo> file_info(const std::filesystem::path &path) noexcept {
    std::error_code error;
    FileInfo result;
    result.path = path;
    result.regular_file = std::filesystem::is_regular_file(path, error);
    if (error)
        return Result<FileInfo>::failure(
            file_detail::make_error(error, "Cannot inspect file", "filesystem.file_info"));
    result.directory = std::filesystem::is_directory(path, error);
    if (error)
        return Result<FileInfo>::failure(
            file_detail::make_error(error, "Cannot inspect file", "filesystem.file_info"));
    if (result.regular_file) {
        const auto size = std::filesystem::file_size(path, error);
        if (error)
            return Result<FileInfo>::failure(
                file_detail::make_error(error, "Cannot read file size", "filesystem.file_info"));
        result.size = static_cast<std::uint64_t>(size);
    }
    result.modified = std::filesystem::last_write_time(path, error);
    if (error)
        return Result<FileInfo>::failure(
            file_detail::make_error(error, "Cannot read file time", "filesystem.file_info"));
    return Result<FileInfo>::success(std::move(result));
}

Result<bool> files_equal(const std::filesystem::path &left,
                                const std::filesystem::path &right) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        auto left_size = ::sindre::general::file_size(left);
        if (!left_size) return Result<bool>::failure(left_size.error());
        auto right_size = ::sindre::general::file_size(right);
        if (!right_size) return Result<bool>::failure(right_size.error());
        if (left_size.value() != right_size.value()) return Result<bool>::success(false);

        auto left_input = file_detail::open_binary(left, "filesystem.files_equal");
        if (!left_input) return Result<bool>::failure(left_input.error());
        auto right_input = file_detail::open_binary(right, "filesystem.files_equal");
        if (!right_input) return Result<bool>::failure(right_input.error());

        std::array<char, 64 * 1024> left_buffer{};
        std::array<char, 64 * 1024> right_buffer{};
        for (;;) {
            left_input.value().read(left_buffer.data(), static_cast<std::streamsize>(left_buffer.size()));
            right_input.value().read(right_buffer.data(), static_cast<std::streamsize>(right_buffer.size()));
            const auto left_count = left_input.value().gcount();
            const auto right_count = right_input.value().gcount();
            if (left_count != right_count ||
                !std::equal(left_buffer.begin(), left_buffer.begin() + left_count,
                            right_buffer.begin()))
                return Result<bool>::success(false);
            if (left_count == 0) break;
        }
        if ((!left_input.value().eof() && left_input.value().fail()) ||
            (!right_input.value().eof() && right_input.value().fail()))
            return Result<bool>::failure(file_detail::make_error(
                std::make_error_code(std::errc::io_error), "Cannot compare files",
                "filesystem.files_equal"));
        return Result<bool>::success(true);
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<bool>::failure(file_detail::make_error(
            std::make_error_code(std::errc::io_error), error.what(), "filesystem.files_equal"));
    } catch (...) {
        return Result<bool>::failure(file_detail::make_error(
            std::make_error_code(std::errc::io_error), "Cannot compare files",
            "filesystem.files_equal"));
    }
#endif
}

Result<std::string> file_md5(const std::filesystem::path &path) noexcept {
    return file_detail::hash_file<file_detail::Md5>(path, "filesystem.file_md5");
}

Result<std::string> file_sha256(const std::filesystem::path &path) noexcept {
    return file_detail::hash_file<file_detail::Sha256>(path, "filesystem.file_sha256");
}

} // namespace sindre::general

// ---- merged from system.cpp ----

// System implementation boundary for filesystem and platform services.

#include <cstdlib>
#include <fstream>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <thread>
#include <utility>
#include <sindre/general/string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace sindre::general::system {

Result<std::string> environment(std::string_view name) noexcept {
    if (name.empty()) return Result<std::string>::failure(
        std::make_error_code(std::errc::invalid_argument), "Environment name is empty", "system.environment");
#if defined(_WIN32)
    char *value = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&value, &size, std::string(name).c_str()) != 0 || !value)
        return Result<std::string>::failure(
            std::make_error_code(std::errc::no_such_file_or_directory), "Environment variable is missing", "system.environment");
    std::string result(value, size ? size - 1 : 0);
    std::free(value);
    return Result<std::string>::success(std::move(result));
#else
    if (const char *value = std::getenv(std::string(name).c_str()))
        return Result<std::string>::success(value);
    return Result<std::string>::failure(
        std::make_error_code(std::errc::no_such_file_or_directory), "Environment variable is missing", "system.environment");
#endif
}

Result<void> set_environment(std::string_view name, std::string_view value) noexcept {
    if (name.empty()) return Result<void>::failure(
        std::make_error_code(std::errc::invalid_argument), "Environment name is empty", "system.set_environment");
#if defined(_WIN32)
    if (_putenv_s(std::string(name).c_str(), std::string(value).c_str()) != 0)
#else
    if (::setenv(std::string(name).c_str(), std::string(value).c_str(), 1) != 0)
#endif
        return Result<void>::failure(
            std::make_error_code(std::errc::permission_denied), "Cannot set environment variable", "system.set_environment");
    return Result<void>::success();
}

Information information() {
    Information result;
#if defined(_WIN32)
    result.os = "Windows";
#elif defined(__linux__)
    result.os = "Linux";
#elif defined(__APPLE__)
    result.os = "macOS";
#else
    result.os = "Unknown";
#endif
#if defined(_M_X64) || defined(__x86_64__)
    result.architecture = "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    result.architecture = "arm64";
#else
    result.architecture = "unknown";
#endif
#if defined(__clang__)
    result.compiler = "clang";
#elif defined(_MSC_VER)
    result.compiler = "msvc";
#elif defined(__GNUC__)
    result.compiler = "gcc";
#else
    result.compiler = "unknown";
#endif
    result.cpu_count = std::thread::hardware_concurrency();
#if defined(_WIN32)
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (::GlobalMemoryStatusEx(&memory)) result.memory_bytes = memory.ullTotalPhys;
#else
    const auto pages = ::sysconf(_SC_PHYS_PAGES);
    const auto page_size = ::sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0)
        result.memory_bytes = static_cast<std::uint64_t>(pages) * page_size;
#endif
    return result;
}

} // namespace sindre::general::system

namespace sindre::general::path {

std::string to_utf8(const std::filesystem::path &value) {
#if defined(__cpp_char8_t)
    const auto text = value.u8string();
    return {reinterpret_cast<const char *>(text.data()), text.size()};
#else
    return value.u8string();
#endif
}

std::filesystem::path from_utf8(std::string_view value) {
#if defined(_WIN32)
    return std::filesystem::u8path(value);
#else
    return std::filesystem::path(std::string(value));
#endif
}

Result<std::string> try_to_utf8(const std::filesystem::path &value) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try { return Result<std::string>::success(to_utf8(value)); }
    catch (const std::exception &error) {
        return Result<std::string>::failure(
            std::make_error_code(std::errc::illegal_byte_sequence), error.what(), "path.to_utf8");
    } catch (...) {
        return Result<std::string>::failure(
            std::make_error_code(std::errc::illegal_byte_sequence), "Cannot convert path to UTF-8", "path.to_utf8");
    }
#else
    return Result<std::string>::success(to_utf8(value));
#endif
}

Result<std::string> read_text(const std::filesystem::path &value) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
        std::ifstream input(value, std::ios::binary);
        if (!input) return Result<std::string>::failure(
            std::make_error_code(std::errc::no_such_file_or_directory), "Cannot open file", "path.read_text");
        std::ostringstream content;
        content << input.rdbuf();
        if (!input.good() && !input.eof()) return Result<std::string>::failure(
            std::make_error_code(std::errc::io_error), "Cannot read file", "path.read_text");
        auto result = content.str();
        if (!string::valid_utf8(result)) return Result<std::string>::failure(
            std::make_error_code(std::errc::illegal_byte_sequence), "File is not UTF-8", "path.read_text");
        return Result<std::string>::success(std::move(result));
    } catch (const std::exception &error) {
        return Result<std::string>::failure(std::make_error_code(std::errc::io_error), error.what(), "path.read_text");
    } catch (...) {
        return Result<std::string>::failure(
            std::make_error_code(std::errc::io_error), "Cannot read file", "path.read_text");
    }
#else
    std::ifstream input(value, std::ios::binary);
    if (!input) return Result<std::string>::failure(
        std::make_error_code(std::errc::no_such_file_or_directory), "Cannot open file", "path.read_text");
    std::ostringstream content;
    content << input.rdbuf();
    if (!input.good() && !input.eof()) return Result<std::string>::failure(
        std::make_error_code(std::errc::io_error), "Cannot read file", "path.read_text");
    auto result = content.str();
    if (!string::valid_utf8(result)) return Result<std::string>::failure(
        std::make_error_code(std::errc::illegal_byte_sequence), "File is not UTF-8", "path.read_text");
    return Result<std::string>::success(std::move(result));
#endif
}

Result<void> write_text(const std::filesystem::path &value, std::string_view text) noexcept {
    if (!string::valid_utf8(text)) return Result<void>::failure(
        std::make_error_code(std::errc::illegal_byte_sequence), "Text is not UTF-8", "path.write_text");
#if defined(SINDRE_NO_EXCEPTIONS)
    std::ofstream output(value, std::ios::binary | std::ios::trunc);
    if (!output) return Result<void>::failure(
        std::make_error_code(std::errc::permission_denied), "Cannot open file", "path.write_text");
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) return Result<void>::failure(
        std::make_error_code(std::errc::io_error), "Cannot write file", "path.write_text");
    return Result<void>::success();
#else
    try {
        std::ofstream output(value, std::ios::binary | std::ios::trunc);
        if (!output) return Result<void>::failure(
            std::make_error_code(std::errc::permission_denied), "Cannot open file", "path.write_text");
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!output) return Result<void>::failure(
            std::make_error_code(std::errc::io_error), "Cannot write file", "path.write_text");
        return Result<void>::success();
    } catch (const std::exception &error) {
        return Result<void>::failure(std::make_error_code(std::errc::io_error), error.what(), "path.write_text");
    } catch (...) {
        return Result<void>::failure(
            std::make_error_code(std::errc::io_error), "Cannot write file", "path.write_text");
    }
#endif
}

Result<std::filesystem::path> require_file(const std::filesystem::path &value) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(value, error)) return Result<std::filesystem::path>::failure(
        error ? error : std::make_error_code(std::errc::no_such_file_or_directory),
        "Regular file does not exist", "path.require_file");
    return Result<std::filesystem::path>::success(value);
}

} // namespace sindre::general::path

namespace sindre::general::desktop {

bool is_elevated() noexcept {
#if defined(_WIN32)
    return false;
#else
    return ::geteuid() == 0;
#endif
}

Result<void> request_elevation(std::string_view) noexcept {
    return Result<void>::failure(std::make_error_code(std::errc::function_not_supported),
        "Elevation requires an application-specific launcher", "desktop.elevation");
}
Result<void> clipboard_set(std::string_view) noexcept {
    return Result<void>::failure(std::make_error_code(std::errc::function_not_supported),
        "Clipboard backend is not enabled", "desktop.clipboard");
}
Result<std::string> clipboard_get() noexcept {
    return Result<std::string>::failure(std::make_error_code(std::errc::function_not_supported),
        "Clipboard backend is not enabled", "desktop.clipboard");
}
Result<void> notify(std::string_view, std::string_view) noexcept {
    return Result<void>::failure(std::make_error_code(std::errc::function_not_supported),
        "Notification backend is not enabled", "desktop.notify");
}
Result<void> tray_start(std::string_view) noexcept {
    return Result<void>::failure(std::make_error_code(std::errc::function_not_supported),
        "Tray backend is not enabled", "desktop.tray");
}

} // namespace sindre::general::desktop

namespace sindre::general::file_watch {

struct Watcher::Impl {
    struct Snapshot {
        bool exists = false;
        std::filesystem::file_time_type modified{};
        std::uintmax_t size = 0;
        bool operator==(const Snapshot &other) const {
            return exists == other.exists && modified == other.modified && size == other.size;
        }
    };
    std::filesystem::path path;
    std::chrono::milliseconds interval;
    std::function<void(const Event &)> callback;
    std::thread worker;
    mutable std::mutex mutex;
    std::atomic<bool> stopping{false};
    std::optional<Error> error;
    Snapshot snapshot() const noexcept {
        std::error_code code;
        Snapshot result;
        result.exists = std::filesystem::is_regular_file(path, code);
        if (code || !result.exists) return result;
        result.modified = std::filesystem::last_write_time(path, code);
        if (code) return Snapshot{};
        result.size = std::filesystem::file_size(path, code);
        return code ? Snapshot{} : result;
    }
    void loop() noexcept {
        auto previous = snapshot();
        while (!stopping.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(interval);
            const auto current = snapshot();
            if (current == previous) continue;
            previous = current;
#if defined(SINDRE_NO_EXCEPTIONS)
            callback(Event{path, current.exists});
#else
            try { callback(Event{path, current.exists}); }
            catch (const std::exception &caught) {
                std::lock_guard<std::mutex> lock(mutex);
                error = Error{std::make_error_code(std::errc::io_error), caught.what(), "file_watch.callback"};
            } catch (...) {
                std::lock_guard<std::mutex> lock(mutex);
                error = Error{std::make_error_code(std::errc::io_error), "Unknown watcher callback failure", "file_watch.callback"};
            }
#endif
        }
    }
};

Watcher::Watcher(std::filesystem::path path, std::chrono::milliseconds interval)
    : impl_(std::make_unique<Impl>()) { impl_->path = std::move(path); impl_->interval = interval; }
Watcher::~Watcher() { stop(); }
Result<void> Watcher::start(std::function<void(const Event &)> callback) {
    if (!callback || impl_->interval.count() <= 0) return Result<void>::failure(
        std::make_error_code(std::errc::invalid_argument), "Invalid watcher options", "file_watch.start");
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->worker.joinable()) return Result<void>::failure(
        std::make_error_code(std::errc::device_or_resource_busy), "Watcher is already running", "file_watch.start");
    impl_->callback = std::move(callback);
    impl_->stopping.store(false, std::memory_order_release);
#if !defined(SINDRE_NO_EXCEPTIONS)
    try { impl_->worker = std::thread([this] { impl_->loop(); }); }
    catch (const std::exception &error) { return Result<void>::failure(
        std::make_error_code(std::errc::resource_unavailable_try_again), error.what(), "file_watch.start"); }
#else
    impl_->worker = std::thread([this] { impl_->loop(); });
#endif
    return Result<void>::success();
}
void Watcher::stop() noexcept {
    impl_->stopping.store(true, std::memory_order_release);
    if (impl_->worker.joinable() && impl_->worker.get_id() != std::this_thread::get_id()) impl_->worker.join();
}
std::optional<Error> Watcher::error() const { std::lock_guard<std::mutex> lock(impl_->mutex); return impl_->error; }

} // namespace sindre::general::file_watch

namespace sindre::general::temp {

File::File(File &&other) noexcept : path_(std::move(other.path_)), remove_(std::exchange(other.remove_, false)) {}
File &File::operator=(File &&other) noexcept {
    if (this != &other) { cleanup(); path_ = std::move(other.path_); remove_ = std::exchange(other.remove_, false); }
    return *this;
}
File::~File() { cleanup(); }
Result<File> File::create(std::string_view prefix, std::string_view suffix) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        File result;
        const auto directory = std::filesystem::temp_directory_path();
        std::random_device entropy;
        for (int attempt = 0; attempt < 32; ++attempt) {
            const auto name = std::string(prefix) + codec::hex(
                (static_cast<std::uint64_t>(entropy()) << 32) ^ entropy() ^ static_cast<std::uint64_t>(attempt)) + std::string(suffix);
            result.path_ = directory / name;
            std::ofstream output(result.path_, std::ios::binary | std::ios::out | std::ios::app);
            if (!output) continue;
            output.close();
            result.remove_ = true;
            return Result<File>::success(std::move(result));
        }
        return Result<File>::failure(std::make_error_code(std::errc::file_exists), "Cannot create unique temporary file", "temp.create");
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<File>::failure(std::make_error_code(std::errc::io_error), error.what(), "temp.create");
    } catch (...) {
        return Result<File>::failure(std::make_error_code(std::errc::io_error), "Unknown temporary file failure", "temp.create");
#endif
#if !defined(SINDRE_NO_EXCEPTIONS)
    }
#endif
}
const std::filesystem::path &File::path() const noexcept { return path_; }
void File::keep() noexcept { remove_ = false; }
void File::cleanup() noexcept { if (remove_ && !path_.empty()) std::filesystem::remove(path_); }

} // namespace sindre::general::temp
