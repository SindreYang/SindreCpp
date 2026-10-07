#pragma once

/// @file
/// @brief 命令行参数解析和当前进程参数接口。

#include <sindre/general/core.h>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sindre::general::cli {

class OptionBuilder;
class PositionalBuilder;

struct Option {
    std::string name;
    std::vector<std::string> aliases;
    bool requires_value = true;
    bool required = false;
    std::optional<std::string> default_value;
    std::string help;
};

struct Positional {
    std::string name;
    bool required = true;
    std::string help;
};

struct ParseSettings {
    std::string program_name = "sindre";
    std::string description;
    std::string version;
    bool enable_help = true;
    bool enable_version = true;
};

struct Specification {
    ParseSettings settings;
    std::vector<Option> options;
    std::vector<Positional> positionals;

    OptionBuilder add_option(std::string name);
    OptionBuilder add_option(std::string name, std::string alias);
    OptionBuilder add_option(std::string name,
                             std::initializer_list<std::string> aliases);
    OptionBuilder add_flag(std::string name);
    OptionBuilder add_flag(std::string name, std::string alias);
    OptionBuilder add_flag(std::string name,
                           std::initializer_list<std::string> aliases);
    PositionalBuilder add_positional(std::string name);
};

class OptionBuilder {
public:
    OptionBuilder &add_alias(std::string alias);
    OptionBuilder &set_aliases(std::initializer_list<std::string> aliases);
    OptionBuilder &requires_value(bool value = true) noexcept;
    OptionBuilder &required(bool value = true) noexcept;
    OptionBuilder &default_value(std::string value);
    OptionBuilder &help(std::string value);

private:
    OptionBuilder(Specification &specification, std::size_t index) noexcept;

    Specification *specification_;
    std::size_t index_;

    friend struct Specification;
};

class PositionalBuilder {
public:
    PositionalBuilder &required(bool value = true) noexcept;
    PositionalBuilder &optional() noexcept;
    PositionalBuilder &help(std::string value);

private:
    PositionalBuilder(Specification &specification, std::size_t index) noexcept;

    Specification *specification_;
    std::size_t index_;

    friend struct Specification;
};

enum class Action {
    none,
    help,
    version
};

struct Arguments {
    std::unordered_map<std::string, std::string> values;
    std::vector<std::string> positionals;
    std::unordered_set<std::string> provided;
    Action action = Action::none;
    std::string action_text;

    const std::string *find(std::string_view name) const;
    std::string get_string(std::string_view name, std::string fallback = {}) const;
    bool has(std::string_view name) const;
    bool is_set(std::string_view name) const;
    Result<std::int64_t> get_int(std::string_view name) const;
    Result<double> get_float(std::string_view name) const;
    Result<bool> get_bool(std::string_view name) const;
    bool wants_help() const noexcept;
    bool wants_version() const noexcept;
};

Result<Arguments> parse(std::vector<std::string_view> tokens,
                        const Specification &spec);
Result<Arguments> parse_current(const Specification &spec) noexcept;

} // namespace sindre::general::cli
