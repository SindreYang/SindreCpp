#include <sindre/general/cli.h>
#include <sindre/general/string.h>

#include <algorithm>
#include <cctype>
#include <cwchar>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>
#include <utility>

#if defined(SINDRE_WITH_CLI) && !defined(SINDRE_NO_EXCEPTIONS)
#include <argparse/argparse.hpp>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <crt_externs.h>
#endif

namespace sindre::general::cli {

namespace {

using NameMap = std::unordered_map<std::string, std::string>;

struct ValidatedSpecification {
    NameMap names;
    std::unordered_map<std::string, std::size_t> options;
};

Result<void> invalid(std::string message, std::string context = "cli.specification") {
    return Result<void>::failure(std::make_error_code(std::errc::invalid_argument),
                                 std::move(message), std::move(context));
}

Result<ValidatedSpecification> validate(const Specification &spec) {
    if (spec.settings.program_name.empty())
        return Result<ValidatedSpecification>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "Program name is empty", "cli.specification.program_name");

    ValidatedSpecification result;
    const auto register_name = [&](std::string_view name, std::string canonical,
                                   bool option_name) -> Result<void> {
        if (name.empty() || std::any_of(name.begin(), name.end(), [](unsigned char value) {
                return std::isspace(value) != 0 || value == '=';
            }))
            return invalid("CLI name contains invalid characters");
        if (option_name) {
            if (name.front() != '-' || (name.size() == 1) ||
                (name.size() > 1 && name[1] == '-' && name.size() == 2))
                return invalid("CLI option name is invalid: " + std::string(name));
        } else if (name.front() == '-') {
            return invalid("CLI positional name is invalid: " + std::string(name));
        }
        if (!result.names.emplace(std::string(name), std::move(canonical)).second)
            return invalid("Duplicate CLI name: " + std::string(name));
        return Result<void>::success();
    };

    for (std::size_t index = 0; index < spec.options.size(); ++index) {
        const auto &option = spec.options[index];
        if (option.name.rfind("--", 0) != 0 || option.name.size() <= 2)
            return Result<ValidatedSpecification>::failure(
                invalid("CLI option must use a long name: " + option.name).error());
        if (option.required && option.default_value)
            return Result<ValidatedSpecification>::failure(
                invalid("Required CLI option cannot have a default: " + option.name).error());
        if (!option.requires_value && option.default_value) {
            const auto normalized = string::lower_ascii(*option.default_value);
            if (normalized != "true" && normalized != "false" &&
                normalized != "1" && normalized != "0" &&
                normalized != "yes" && normalized != "no" &&
                normalized != "on" && normalized != "off")
                return Result<ValidatedSpecification>::failure(
                    invalid("Invalid boolean default for CLI option: " + option.name).error());
        }
        auto registered = register_name(option.name, option.name, true);
        if (!registered) return Result<ValidatedSpecification>::failure(registered.error());
        result.options.emplace(option.name, index);
        for (const auto &alias : option.aliases) {
            registered = register_name(alias, option.name, true);
            if (!registered) return Result<ValidatedSpecification>::failure(registered.error());
        }
    }

    if (spec.settings.enable_help) {
        for (const auto &name : {std::string("-h"), std::string("--help")}) {
            if (result.names.count(name))
                return Result<ValidatedSpecification>::failure(
                    invalid("CLI option is reserved: " + name).error());
        }
    }
    if (spec.settings.enable_version && !spec.settings.version.empty()) {
        if (result.names.count("--version"))
            return Result<ValidatedSpecification>::failure(
                invalid("CLI option is reserved: --version").error());
    }

    for (const auto &positional : spec.positionals) {
        auto registered = register_name(positional.name, positional.name, false);
        if (!registered) return Result<ValidatedSpecification>::failure(registered.error());
    }
    return Result<ValidatedSpecification>::success(std::move(result));
}

std::string canonical_name(const ValidatedSpecification &validated, std::string_view name) {
    const auto it = validated.names.find(std::string(name));
    return it == validated.names.end() ? std::string{} : it->second;
}

std::optional<bool> parse_boolean_token(std::string_view value) {
    const auto normalized = string::lower_ascii(value);
    if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on")
        return true;
    if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off")
        return false;
    return std::nullopt;
}

#if defined(SINDRE_NO_EXCEPTIONS)
bool is_negative_numeric_token(std::string_view value) {
    if (value.size() < 2 || value.front() != '-') return false;
    return static_cast<bool>(string::parse_int(value)) ||
           static_cast<bool>(string::parse_float(value));
}
#endif

#if defined(SINDRE_WITH_CLI) && !defined(SINDRE_NO_EXCEPTIONS)
std::string option_help(const Option &option) {
    if (option.aliases.empty()) return option.help;
    std::string result = option.help;
    if (!result.empty()) result += " ";
    result += "(aliases: ";
    for (std::size_t index = 0; index < option.aliases.size(); ++index) {
        if (index != 0) result += ", ";
        result += option.aliases[index];
    }
    result += ")";
    return result;
}
#endif

#if defined(SINDRE_NO_EXCEPTIONS)
std::string format_fallback_help(const Specification &spec) {
    std::ostringstream output;
    output << "Usage: " << spec.settings.program_name;
    if (!spec.options.empty()) output << " [options]";
    for (const auto &positional : spec.positionals)
        output << ' ' << (positional.required ? '<' : '[') << positional.name
               << (positional.required ? '>' : ']');
    output << '\n';
    if (!spec.settings.description.empty()) output << '\n' << spec.settings.description << '\n';
    if (spec.settings.enable_help || (!spec.settings.version.empty() && spec.settings.enable_version)) {
        output << "\nOptions:\n";
        if (spec.settings.enable_help) output << "  -h, --help\tShow this help message\n";
        if (spec.settings.enable_version && !spec.settings.version.empty())
            output << "  --version\tShow version information\n";
    }
    for (const auto &option : spec.options) {
        output << "  " << option.name;
        for (const auto &alias : option.aliases) output << ", " << alias;
        if (option.requires_value) output << " <value>";
        if (option.required) output << " [required]";
        if (option.default_value) output << " [default: " << *option.default_value << ']';
        if (!option.help.empty()) output << "\t" << option.help;
        output << '\n';
    }
    if (!spec.positionals.empty()) {
        output << "\nArguments:\n";
        for (const auto &positional : spec.positionals)
            output << "  " << positional.name << (positional.required ? " [required]" : "")
                   << "\t" << positional.help << '\n';
    }
    return output.str();
}
#endif

std::optional<Action> find_action(const std::vector<std::string_view> &tokens,
                                  const Specification &spec,
                                  const ValidatedSpecification &validated) {
    if (!spec.settings.enable_help && !spec.settings.enable_version) return std::nullopt;
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        const auto token = tokens[index];
        if (token == "--") break;
        if (spec.settings.enable_help && (token == "-h" || token == "--help"))
            return Action::help;
        if (spec.settings.enable_version && !spec.settings.version.empty() && token == "--version")
            return Action::version;
        const auto separator = token.find('=');
        const auto option_token = token.substr(0, separator);
        const auto canonical = canonical_name(validated, option_token);
        if (!canonical.empty()) {
            continue;
        }
    }
    return std::nullopt;
}

void initialize_defaults(Arguments &result, const Specification &spec) {
    for (const auto &option : spec.options) {
        if (option.requires_value) {
            if (option.default_value) result.values.emplace(option.name, *option.default_value);
        } else {
            result.values.emplace(option.name, option.default_value.value_or("false"));
        }
    }
}

#if defined(SINDRE_NO_EXCEPTIONS)
Result<Arguments> parse_fallback(const std::vector<std::string_view> &tokens,
                                 const Specification &spec,
                                 const ValidatedSpecification &validated) {
    Arguments result;
    initialize_defaults(result, spec);
    if (const auto action = find_action(tokens, spec, validated)) {
        result.action = *action;
        result.action_text = *action == Action::help
            ? format_fallback_help(spec) : spec.settings.version;
        return Result<Arguments>::success(std::move(result));
    }

    bool positional_mode = false;
    std::size_t positional_index = 0;
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        const auto token = tokens[index];
        if (!positional_mode && token == "--") {
            positional_mode = true;
            continue;
        }
        if (!positional_mode && !token.empty() && token.front() == '-') {
            const auto separator = token.find('=');
            const auto option_token = token.substr(0, separator);
            const auto canonical = canonical_name(validated, option_token);
            if (canonical.empty()) return Result<Arguments>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Unknown CLI option: " + std::string(option_token), "cli.parse");
            if (!result.provided.emplace(canonical).second) return Result<Arguments>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Duplicate CLI option: " + canonical, "cli.parse." + canonical);
            const auto &option = spec.options[validated.options.at(canonical)];
            if (option.requires_value) {
                std::string value;
                if (separator != std::string_view::npos) value = std::string(token.substr(separator + 1));
                else if (++index >= tokens.size() || tokens[index] == "--" ||
                         (!tokens[index].empty() && tokens[index].front() == '-' &&
                          !is_negative_numeric_token(tokens[index])))
                    return Result<Arguments>::failure(
                    std::make_error_code(std::errc::invalid_argument),
                    "Missing value for CLI option: " + canonical, "cli.parse." + canonical);
                else value = std::string(tokens[index]);
                result.values[canonical] = std::move(value);
            } else {
                if (separator == std::string_view::npos) result.values[canonical] = "true";
                else {
                    const auto parsed = parse_boolean_token(token.substr(separator + 1));
                    if (!parsed) return Result<Arguments>::failure(
                        std::make_error_code(std::errc::invalid_argument),
                        "Invalid boolean CLI value for: " + canonical, "cli.parse." + canonical);
                    result.values[canonical] = *parsed ? "true" : "false";
                }
            }
            continue;
        }
        if (positional_index >= spec.positionals.size()) return Result<Arguments>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "Unexpected positional CLI argument: " + std::string(token), "cli.parse");
        result.positionals.emplace_back(token);
        ++positional_index;
    }

    for (const auto &option : spec.options) {
        if (option.required && !result.provided.count(option.name)) return Result<Arguments>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "Required CLI option is missing: " + option.name, "cli.parse." + option.name);
    }
    for (std::size_t index = positional_index; index < spec.positionals.size(); ++index) {
        if (spec.positionals[index].required) return Result<Arguments>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "Required positional argument is missing: " + spec.positionals[index].name,
            "cli.parse." + spec.positionals[index].name);
    }
    return Result<Arguments>::success(std::move(result));
}
#endif

#if defined(SINDRE_WITH_CLI) && !defined(SINDRE_NO_EXCEPTIONS)
struct PreparedArgparseInput {
    std::vector<std::string> tokens;
    std::unordered_map<std::string, std::string> explicit_booleans;
    std::unordered_map<std::string, std::string> positional_replacements;
};

Result<PreparedArgparseInput> prepare_argparse_input(
    const std::vector<std::string_view> &tokens,
    const Specification &spec,
    const ValidatedSpecification &validated) {
    PreparedArgparseInput result;
    result.tokens.reserve(tokens.size() + 1);
    bool positional_mode = false;
    for (const auto token : tokens) {
        if (positional_mode) {
            const auto marker = "__sindre_cli_literal_" +
                std::to_string(result.positional_replacements.size()) + "__";
            result.positional_replacements.emplace(marker, std::string(token));
            result.tokens.emplace_back(marker);
            continue;
        }
        if (token == "--") {
            positional_mode = true;
            continue;
        }
        const auto separator = token.find('=');
        if (separator == std::string_view::npos) {
            result.tokens.emplace_back(token);
            continue;
        }
        const auto option_token = token.substr(0, separator);
        const auto canonical = canonical_name(validated, option_token);
        if (canonical.empty()) {
            result.tokens.emplace_back(token);
            continue;
        }
        const auto &option = spec.options[validated.options.at(canonical)];
        if (option.requires_value) {
            // argparse splits long --name=value itself. Normalize the short
            // alias form because argparse intentionally only splits long names.
            if (option_token.size() == 2 && option_token.front() == '-') {
                result.tokens.emplace_back(option_token);
                result.tokens.emplace_back(token.substr(separator + 1));
            } else {
                result.tokens.emplace_back(token);
            }
            continue;
        }

        const auto boolean = parse_boolean_token(token.substr(separator + 1));
        if (!boolean) return Result<PreparedArgparseInput>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "Invalid boolean CLI value for: " + canonical, "cli.parse." + canonical);
        if (!result.explicit_booleans.emplace(canonical, *boolean ? "true" : "false").second)
            return Result<PreparedArgparseInput>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Duplicate CLI option: " + canonical, "cli.parse." + canonical);
    }
    return Result<PreparedArgparseInput>::success(std::move(result));
}

argparse::Argument &add_argparse_option(argparse::ArgumentParser &parser, const Option &option) {
    auto &argument = parser.add_argument(option.name);
    for (const auto &alias : option.aliases) parser.add_hidden_alias_for(argument, alias);
    return argument;
}

Result<Arguments> parse_argparse(const std::vector<std::string_view> &tokens,
                                 const Specification &spec,
                                 const ValidatedSpecification &validated) {
    argparse::ArgumentParser parser(
        spec.settings.program_name, spec.settings.version,
        argparse::default_arguments::none, false);
    if (!spec.settings.description.empty()) parser.add_description(spec.settings.description);
    for (const auto &option : spec.options) {
        auto &argument = add_argparse_option(parser, option);
        if (!option.help.empty()) argument.help(option_help(option));
        if (option.requires_value) {
            if (option.default_value) argument.default_value(*option.default_value);
            if (option.required) argument.required();
        } else {
            const auto default_value = option.default_value
                ? string::lower_ascii(*option.default_value) : "false";
            argument.default_value(default_value == "true" || default_value == "1" ||
                                   default_value == "yes" || default_value == "on")
                .implicit_value(true).nargs(0);
        }
    }
    for (const auto &positional : spec.positionals) {
        auto &argument = parser.add_argument(positional.name);
        if (!positional.help.empty()) argument.help(positional.help);
        if (!positional.required) argument.nargs(argparse::nargs_pattern::optional).default_value(std::string{});
    }

    if (const auto action = find_action(tokens, spec, validated)) {
        Arguments result;
        initialize_defaults(result, spec);
        result.action = *action;
        result.action_text = *action == Action::help ? parser.help().str() : spec.settings.version;
        return Result<Arguments>::success(std::move(result));
    }

    auto prepared = prepare_argparse_input(tokens, spec, validated);
    if (!prepared) return Result<Arguments>::failure(prepared.error());

    std::vector<std::string> arguments;
    arguments.reserve(prepared.value().tokens.size() + 1);
    arguments.push_back(spec.settings.program_name);
    for (const auto &token : prepared.value().tokens) arguments.emplace_back(token);
    parser.parse_args(arguments);

    Arguments result;
    initialize_defaults(result, spec);
    for (const auto &option : spec.options) {
        const bool used = parser.is_used(option.name);
        const auto explicit_boolean = prepared.value().explicit_booleans.find(option.name);
        if (!option.requires_value && explicit_boolean != prepared.value().explicit_booleans.end()) {
            if (used) return Result<Arguments>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Duplicate CLI option: " + option.name, "cli.parse." + option.name);
            result.values[option.name] = explicit_boolean->second;
        } else if (option.requires_value) {
            if (used || option.default_value)
                result.values[option.name] = parser.get<std::string>(option.name);
        } else {
            result.values[option.name] = parser.get<bool>(option.name) ? "true" : "false";
        }
        if (used || explicit_boolean != prepared.value().explicit_booleans.end())
            result.provided.emplace(option.name);
    }
    for (const auto &positional : spec.positionals) {
        auto value = parser.get<std::string>(positional.name);
        const auto replacement = prepared.value().positional_replacements.find(value);
        if (replacement != prepared.value().positional_replacements.end())
            value = replacement->second;
        if (positional.required || !value.empty()) result.positionals.push_back(std::move(value));
    }
    return Result<Arguments>::success(std::move(result));
}
#endif

#if defined(_WIN32)
Result<std::string> wide_to_utf8(const wchar_t *value, int length) noexcept {
    if (!value || length < 0) return Result<std::string>::failure(
        std::make_error_code(std::errc::invalid_argument), "Invalid Windows argument", "cli.current");
    const auto size = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, length,
                                            nullptr, 0, nullptr, nullptr);
    if (size <= 0) return Result<std::string>::failure(
        std::make_error_code(std::errc::illegal_byte_sequence),
        "Invalid Windows UTF-16 argument", "cli.current");
    std::string result(static_cast<std::size_t>(size), '\0');
    if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, length,
                              result.data(), size, nullptr, nullptr) <= 0)
        return Result<std::string>::failure(
            std::make_error_code(std::errc::illegal_byte_sequence),
            "Cannot convert Windows argument to UTF-8", "cli.current");
    return Result<std::string>::success(std::move(result));
}
#endif

Result<std::vector<std::string>> current_arguments() noexcept {
#if defined(_WIN32)
    int count = 0;
    auto *wide = ::CommandLineToArgvW(::GetCommandLineW(), &count);
    if (!wide) return Result<std::vector<std::string>>::failure(
        std::error_code(static_cast<int>(::GetLastError()), std::system_category()),
        "Cannot read process command line", "cli.current");
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        auto converted = wide_to_utf8(wide[index], static_cast<int>(wcslen(wide[index])));
        if (!converted) {
            ::LocalFree(wide);
            return Result<std::vector<std::string>>::failure(converted.error());
        }
        result.push_back(std::move(converted.value()));
    }
    ::LocalFree(wide);
    return Result<std::vector<std::string>>::success(std::move(result));
#elif defined(__linux__)
    std::ifstream input("/proc/self/cmdline", std::ios::binary);
    if (!input) return Result<std::vector<std::string>>::failure(
        std::make_error_code(std::errc::io_error), "Cannot read process command line", "cli.current");
    std::string bytes((std::istreambuf_iterator<char>(input)), {});
    std::vector<std::string> result;
    std::size_t begin = 0;
    while (begin < bytes.size()) {
        const auto end = bytes.find('\0', begin);
        const auto value = bytes.substr(begin, end == std::string::npos ? end : end - begin);
        if (!string::valid_utf8(value)) return Result<std::vector<std::string>>::failure(
            std::make_error_code(std::errc::illegal_byte_sequence),
            "Process command line is not UTF-8", "cli.current");
        result.push_back(value);
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return Result<std::vector<std::string>>::success(std::move(result));
#elif defined(__APPLE__)
    const auto argc = *_NSGetArgc();
    const auto argv = *_NSGetArgv();
    if (argc < 0 || !argv) return Result<std::vector<std::string>>::failure(
        std::make_error_code(std::errc::io_error), "Cannot read process command line", "cli.current");
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        const std::string value = argv[index] ? argv[index] : std::string{};
        if (!string::valid_utf8(value)) return Result<std::vector<std::string>>::failure(
            std::make_error_code(std::errc::illegal_byte_sequence),
            "Process command line is not UTF-8", "cli.current");
        result.push_back(value);
    }
    return Result<std::vector<std::string>>::success(std::move(result));
#else
    return Result<std::vector<std::string>>::failure(
        std::make_error_code(std::errc::function_not_supported),
        "Current process arguments are not supported on this platform", "cli.current");
#endif
}

} // namespace

OptionBuilder::OptionBuilder(Specification &specification, std::size_t index) noexcept
    : specification_(&specification), index_(index) {}

PositionalBuilder::PositionalBuilder(Specification &specification, std::size_t index) noexcept
    : specification_(&specification), index_(index) {}

OptionBuilder Specification::add_option(std::string name) {
    options.push_back(Option{std::move(name), {}, true, false, std::nullopt, {}});
    return OptionBuilder(*this, options.size() - 1);
}

OptionBuilder Specification::add_option(std::string name, std::string alias) {
    auto builder = add_option(std::move(name));
    builder.add_alias(std::move(alias));
    return builder;
}

OptionBuilder Specification::add_option(
    std::string name, std::initializer_list<std::string> aliases) {
    auto builder = add_option(std::move(name));
    builder.set_aliases(aliases);
    return builder;
}

OptionBuilder Specification::add_flag(std::string name) {
    auto builder = add_option(std::move(name));
    builder.requires_value(false);
    return builder;
}

OptionBuilder Specification::add_flag(std::string name, std::string alias) {
    auto builder = add_option(std::move(name), std::move(alias));
    builder.requires_value(false);
    return builder;
}

OptionBuilder Specification::add_flag(
    std::string name, std::initializer_list<std::string> aliases) {
    auto builder = add_option(std::move(name), aliases);
    builder.requires_value(false);
    return builder;
}

PositionalBuilder Specification::add_positional(std::string name) {
    positionals.push_back(Positional{std::move(name), true, {}});
    return PositionalBuilder(*this, positionals.size() - 1);
}

OptionBuilder &OptionBuilder::add_alias(std::string alias) {
    specification_->options[index_].aliases.push_back(std::move(alias));
    return *this;
}

OptionBuilder &OptionBuilder::set_aliases(
    std::initializer_list<std::string> aliases) {
    specification_->options[index_].aliases = aliases;
    return *this;
}

OptionBuilder &OptionBuilder::requires_value(bool value) noexcept {
    specification_->options[index_].requires_value = value;
    return *this;
}

OptionBuilder &OptionBuilder::required(bool value) noexcept {
    specification_->options[index_].required = value;
    return *this;
}

OptionBuilder &OptionBuilder::default_value(std::string value) {
    specification_->options[index_].default_value = std::move(value);
    return *this;
}

OptionBuilder &OptionBuilder::help(std::string value) {
    specification_->options[index_].help = std::move(value);
    return *this;
}

PositionalBuilder &PositionalBuilder::required(bool value) noexcept {
    specification_->positionals[index_].required = value;
    return *this;
}

PositionalBuilder &PositionalBuilder::optional() noexcept {
    return required(false);
}

PositionalBuilder &PositionalBuilder::help(std::string value) {
    specification_->positionals[index_].help = std::move(value);
    return *this;
}

const std::string *Arguments::find(std::string_view name) const {
    const auto it = values.find(std::string(name));
    return it == values.end() ? nullptr : &it->second;
}

std::string Arguments::get_string(std::string_view name, std::string fallback) const {
    const auto *value = find(name);
    return value ? *value : std::move(fallback);
}

bool Arguments::has(std::string_view name) const { return find(name) != nullptr; }

bool Arguments::is_set(std::string_view name) const {
    return provided.find(std::string(name)) != provided.end();
}

Result<std::int64_t> Arguments::get_int(std::string_view name) const {
    const auto *value = find(name);
    if (!value) return Result<std::int64_t>::failure(
        std::make_error_code(std::errc::no_such_file_or_directory),
        "CLI value is missing", "cli.value." + std::string(name));
    auto result = string::parse_int(*value);
    return result ? result : Result<std::int64_t>::failure(result.error().with_context(
        "cli.value." + std::string(name)));
}

Result<double> Arguments::get_float(std::string_view name) const {
    const auto *value = find(name);
    if (!value) return Result<double>::failure(
        std::make_error_code(std::errc::no_such_file_or_directory),
        "CLI value is missing", "cli.value." + std::string(name));
    auto result = string::parse_float(*value);
    return result ? result : Result<double>::failure(result.error().with_context(
        "cli.value." + std::string(name)));
}

Result<bool> Arguments::get_bool(std::string_view name) const {
    const auto *value = find(name);
    if (!value) return Result<bool>::failure(
        std::make_error_code(std::errc::no_such_file_or_directory),
        "CLI value is missing", "cli.value." + std::string(name));
    if (const auto parsed = parse_boolean_token(*value))
        return Result<bool>::success(*parsed);
    return Result<bool>::failure(
        std::make_error_code(std::errc::invalid_argument),
        "Invalid boolean CLI value", "cli.value." + std::string(name));
}

bool Arguments::wants_help() const noexcept { return action == Action::help; }
bool Arguments::wants_version() const noexcept { return action == Action::version; }

Result<Arguments> parse(std::vector<std::string_view> tokens, const Specification &spec) {
#if !defined(SINDRE_WITH_CLI)
    (void)tokens;
    (void)spec;
    return Result<Arguments>::failure(
        std::make_error_code(std::errc::function_not_supported),
        "CLI support is not enabled", "cli.parse");
#else
    auto validated = validate(spec);
    if (!validated) return Result<Arguments>::failure(validated.error());
#if defined(SINDRE_NO_EXCEPTIONS)
    return parse_fallback(tokens, spec, validated.value());
#else
    try {
        return parse_argparse(tokens, spec, validated.value());
    } catch (const std::exception &error) {
        return Result<Arguments>::failure(
            std::make_error_code(std::errc::invalid_argument), error.what(), "cli.parse");
    } catch (...) {
        return Result<Arguments>::failure(
            std::make_error_code(std::errc::invalid_argument), "Unknown CLI parse failure", "cli.parse");
    }
#endif
#endif
}

Result<Arguments> parse_current(const Specification &spec) noexcept {
#if !defined(SINDRE_WITH_CLI)
    (void)spec;
    return Result<Arguments>::failure(
        std::make_error_code(std::errc::function_not_supported),
        "CLI support is not enabled", "cli.current");
#else
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        auto current = current_arguments();
        if (!current) return Result<Arguments>::failure(current.error());
        if (current.value().empty()) return Result<Arguments>::failure(
            std::make_error_code(std::errc::io_error),
            "Process command line is empty", "cli.current");
        std::vector<std::string_view> tokens;
        tokens.reserve(current.value().size() - 1);
        for (std::size_t index = 1; index < current.value().size(); ++index)
            tokens.emplace_back(current.value()[index]);
        return parse(std::move(tokens), spec);
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<Arguments>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "cli.current");
    } catch (...) {
        return Result<Arguments>::failure(
            std::make_error_code(std::errc::io_error), "Cannot parse process command line", "cli.current");
    }
#endif
#endif
}

} // namespace sindre::general::cli
