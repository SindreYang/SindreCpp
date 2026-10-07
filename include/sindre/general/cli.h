#pragma once
#include <sindre/general/core.h>
#include <sindre/general/string.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#if defined(SINDRE_WITH_CLI) && !defined(SINDRE_NO_EXCEPTIONS)
#include <argparse/argparse.hpp>
#endif
namespace sindre::general::cli {
#if defined(SINDRE_WITH_CLI) && !defined(SINDRE_NO_EXCEPTIONS)
using ArgumentParser = argparse::ArgumentParser;
namespace native = argparse;
#else
class ArgumentParser { public: explicit ArgumentParser(std::string); };
namespace native {}
#endif
struct Arguments {
    std::unordered_map<std::string, std::string> values;
    const std::string *find(std::string_view name) const;
    std::string get_string(std::string_view name, std::string fallback = {}) const;
    bool has(std::string_view name) const;
    Result<std::int64_t> get_int(std::string_view name) const;
    Result<double> get_float(std::string_view name) const;
    Result<bool> get_bool(std::string_view name) const;
};
struct Option { std::string name; bool requires_value = true; std::string default_value; };
Result<Arguments> parse(std::vector<std::string_view> tokens, std::vector<Option> options = {});
Result<Arguments> parse_current(std::vector<Option> options = {}) noexcept;
}
