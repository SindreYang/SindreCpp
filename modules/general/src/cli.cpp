#include <sindre/general/cli.h>

#include <fstream>
#include <iterator>
#include <cstdint>
#include <cwchar>
#include <type_traits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace sindre::general::cli {

#if !defined(SINDRE_WITH_CLI) || defined(SINDRE_NO_EXCEPTIONS)
ArgumentParser::ArgumentParser(std::string) {}
#endif

const std::string *Arguments::find(std::string_view name) const {
    auto it = values.find(std::string(name));
    return it == values.end() ? nullptr : &it->second;
}

std::string Arguments::get_string(std::string_view name, std::string fallback) const {
    const auto *value = find(name);
    return value ? *value : std::move(fallback);
}

bool Arguments::has(std::string_view name) const { return find(name) != nullptr; }

Result<std::int64_t> Arguments::get_int(std::string_view name) const {
    const auto *value = find(name);
    if (!value) return Result<std::int64_t>::failure(
        std::make_error_code(std::errc::no_such_file_or_directory), "CLI value is missing", "cli.value");
    return string::parse_int(*value);
}

Result<double> Arguments::get_float(std::string_view name) const {
    const auto *value = find(name);
    if (!value) return Result<double>::failure(
        std::make_error_code(std::errc::no_such_file_or_directory), "CLI value is missing", "cli.value");
    return string::parse_float(*value);
}

Result<bool> Arguments::get_bool(std::string_view name) const {
    const auto *value = find(name);
    if (!value) return Result<bool>::failure(
        std::make_error_code(std::errc::no_such_file_or_directory), "CLI value is missing", "cli.value");
    const auto normalized = string::lower_ascii(*value);
    if (normalized == "true" || normalized == "1" || normalized == "yes")
        return Result<bool>::success(true);
    if (normalized == "false" || normalized == "0" || normalized == "no")
        return Result<bool>::success(false);
    return Result<bool>::failure(
        std::make_error_code(std::errc::invalid_argument), "Invalid boolean CLI value", "cli.value");
}

::sindre::general::Result<Arguments>
parse(std::vector<std::string_view> tokens, std::vector<Option> options) {
#if !defined(SINDRE_WITH_CLI)
    (void)tokens;
    (void)options;
    return ::sindre::general::Result<Arguments>::failure(
        std::make_error_code(std::errc::function_not_supported),
        "CLI support is not enabled", "cli.parse");
#else
    Arguments result;
    std::unordered_map<std::string, Option> known;
    for (auto option : options) {
        if (option.name.empty() || option.name.front() != '-')
            return ::sindre::general::Result<Arguments>::failure(
                std::make_error_code(std::errc::invalid_argument), "Invalid CLI option",
                "cli.parse");
        result.values[option.name] = option.default_value;
        known.emplace(option.name, std::move(option));
    }
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const std::string key(tokens[i]);
        auto it = known.find(key);
        if (it == known.end())
            return ::sindre::general::Result<Arguments>::failure(
                std::make_error_code(std::errc::invalid_argument), "Unknown CLI option: " + key,
                "cli.parse");
        if (it->second.requires_value) {
            if (++i >= tokens.size())
                return ::sindre::general::Result<Arguments>::failure(
                    std::make_error_code(std::errc::invalid_argument),
                    "Missing value for CLI option: " + key, "cli.parse");
            result.values[key] = std::string(tokens[i]);
        } else {
            result.values[key] = "true";
        }
    }
    return ::sindre::general::Result<Arguments>::success(std::move(result));
#endif
}

::sindre::general::Result<Arguments>
parse_current(std::vector<Option> options) noexcept {
#if !defined(SINDRE_WITH_CLI)
    (void)options;
    return ::sindre::general::Result<Arguments>::failure(
        std::make_error_code(std::errc::function_not_supported),
        "CLI support is not enabled", "cli.current");
#else
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::vector<std::string> owned;
#if defined(_WIN32)
        const auto *wide_command_line = ::GetCommandLineW();
        const auto wide_length = wide_command_line ? static_cast<int>(wcslen(wide_command_line)) : 0;
        std::string current;
        if (wide_length > 0) {
            const auto length = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_command_line,
                                                      wide_length, nullptr, 0, nullptr, nullptr);
            if (length <= 0) return ::sindre::general::Result<Arguments>::failure(
                std::make_error_code(std::errc::illegal_byte_sequence), "Invalid process command line", "cli.current");
            current.resize(static_cast<std::size_t>(length));
            ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_command_line, wide_length,
                                  current.data(), length, nullptr, nullptr);
        }
        const auto *command_line = current.c_str();
        if (!command_line)
            return ::sindre::general::Result<Arguments>::failure(
                std::make_error_code(std::errc::invalid_argument), "Cannot read process command line", "cli.current");
        bool quoted = false;
        std::string token;
        for (std::size_t i = 0; i <= current.size(); ++i) {
            const char c = i < current.size() ? current[i] : ' ';
            if (c == '"') { quoted = !quoted; continue; }
            if (!quoted && (c == ' ' || c == '\t')) {
                if (!token.empty()) { owned.push_back(std::move(token)); token.clear(); }
            } else token.push_back(c);
        }
#else
        std::ifstream input("/proc/self/cmdline", std::ios::binary);
        if (!input)
            return ::sindre::general::Result<Arguments>::failure(
                std::make_error_code(std::errc::io_error), "Cannot read process command line", "cli.current");
        std::string bytes((std::istreambuf_iterator<char>(input)), {});
        std::size_t begin = 0;
        while (begin < bytes.size()) {
            const auto end = bytes.find('\0', begin);
            owned.emplace_back(bytes.substr(begin, end == std::string::npos ? end : end - begin));
            if (end == std::string::npos) break;
            begin = end + 1;
        }
#endif
        std::vector<std::string_view> tokens;
        for (std::size_t i = owned.empty() ? 0 : 1; i < owned.size(); ++i) tokens.emplace_back(owned[i]);
        return parse(tokens, std::move(options));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return ::sindre::general::Result<Arguments>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "cli.current");
    } catch (...) {
        return ::sindre::general::Result<Arguments>::failure(
            std::make_error_code(std::errc::io_error), "Cannot parse process command line", "cli.current");
    }
#endif
#endif
}

} // namespace sindre::general::cli
