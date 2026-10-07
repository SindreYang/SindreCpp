#include <sindre/general/cli.h>

#include <cstdlib>
#include <iostream>
#include <string>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "Check failed at " << __FILE__ << ':' << __LINE__ \
                  << ": " #condition << '\n'; \
        return EXIT_FAILURE; \
    } \
} while (false)

namespace {

sindre::general::cli::Specification make_spec() {
    sindre::general::cli::Specification spec;
    spec.settings.program_name = "sindre-cli-test";
    spec.settings.description = "CLI parser test";
    spec.settings.version = "1.2.3";
    spec.add_option("--name", "-n")
        .required()
        .help("display name");
    spec.add_option("--count", "-c")
        .default_value("7")
        .help("item count");
    spec.add_flag("--verbose", "-v")
        .help("enable verbose output");
    spec.add_option("--ratio")
        .help("floating point ratio");
    spec.add_positional("input")
        .required()
        .help("input file");
    return spec;
}

} // namespace

int main() {
    const auto spec = make_spec();

    const auto parsed = sindre::general::cli::parse(
        {"--name", "中文", "--verbose", "input.txt"}, spec);
    CHECK(parsed);
    CHECK(parsed.value().get_string("--name") == "中文");
    CHECK(parsed.value().get_int("--count") && parsed.value().get_int("--count").value() == 7);
    CHECK(parsed.value().get_bool("--verbose") && parsed.value().get_bool("--verbose").value());
    CHECK(parsed.value().has("--count") && !parsed.value().is_set("--count"));
    CHECK(parsed.value().is_set("--name") && parsed.value().is_set("--verbose"));
    CHECK(parsed.value().positionals.size() == 1 && parsed.value().positionals[0] == "input.txt");

    const auto equals = sindre::general::cli::parse(
        {"-n=equal", "--count=-1", "--verbose=off", "--", "-literal"}, spec);
    CHECK(equals);
    CHECK(equals.value().get_string("--name") == "equal");
    CHECK(equals.value().get_int("--count"));
    CHECK(equals.value().get_int("--count").value() == -1);
    CHECK(equals.value().get_bool("--verbose"));
    CHECK(!equals.value().get_bool("--verbose").value());
    CHECK(equals.value().positionals.size() == 1);
    CHECK(equals.value().positionals[0] == "-literal");

    CHECK(!sindre::general::cli::parse({"--name", "x", "--name", "y", "input.txt"}, spec));
    CHECK(!sindre::general::cli::parse({"--name", "x", "--unknown", "input.txt"}, spec));
    CHECK(!sindre::general::cli::parse({"--name", "input.txt"}, spec));
    CHECK(!sindre::general::cli::parse({"--name", "x", "input.txt", "extra"}, spec));
    CHECK(!sindre::general::cli::parse({"--name", "x", "--verbose=maybe", "input.txt"}, spec));

    auto invalid_spec = spec;
    invalid_spec.options[0].default_value = "fallback";
    CHECK(!sindre::general::cli::parse({}, invalid_spec));
    invalid_spec = spec;
    invalid_spec.options.push_back({"--name", {}, true, false, std::nullopt, "duplicate"});
    CHECK(!sindre::general::cli::parse({}, invalid_spec));
    invalid_spec = spec;
    invalid_spec.options[0].name.clear();
    CHECK(!sindre::general::cli::parse({}, invalid_spec));
    invalid_spec = spec;
    invalid_spec.options[0].aliases = {"name"};
    CHECK(!sindre::general::cli::parse({}, invalid_spec));
    invalid_spec = spec;
    invalid_spec.options[1].aliases = {"-n"};
    CHECK(!sindre::general::cli::parse({}, invalid_spec));
    invalid_spec = spec;
    invalid_spec.positionals.push_back({"input", false, "duplicate"});
    CHECK(!sindre::general::cli::parse({}, invalid_spec));
    invalid_spec = spec;
    invalid_spec.options.push_back({"--help", {}, false, false, std::nullopt, "reserved"});
    CHECK(!sindre::general::cli::parse({}, invalid_spec));
    invalid_spec = spec;
    invalid_spec.options.push_back({"--version", {}, false, false, std::nullopt, "reserved"});
    CHECK(!sindre::general::cli::parse({}, invalid_spec));

    const auto help = sindre::general::cli::parse({"--help"}, spec);
    CHECK(help && help.value().wants_help() &&
          help.value().action_text.find("--name") != std::string::npos);
    const auto version = sindre::general::cli::parse({"--version"}, spec);
    CHECK(version && version.value().wants_version() && version.value().action_text == "1.2.3");
    const auto help_without_required = sindre::general::cli::parse({"--name", "--help"}, spec);
    CHECK(help_without_required && help_without_required.value().wants_help());
    const auto version_without_required = sindre::general::cli::parse({"--name", "--version"}, spec);
    CHECK(version_without_required && version_without_required.value().wants_version());

    auto invalid_value_spec = spec;
    invalid_value_spec.options[3].default_value = "bad";
    const auto invalid_value = sindre::general::cli::parse({"--name", "x", "input.txt"}, invalid_value_spec);
    CHECK(invalid_value && !invalid_value.value().get_float("--ratio") &&
          invalid_value.value().get_float("--ratio").error().context.find("--ratio") != std::string::npos);
    const auto missing_value = parsed.value().get_int("--missing");
    CHECK(!missing_value && missing_value.error().context.find("--missing") != std::string::npos);
    CHECK(!sindre::general::cli::parse({"--name", "x"}, spec));

    const auto current = sindre::general::cli::parse_current(spec);
    CHECK(current);
    CHECK(current.value().get_string("--name") == "中文");
    CHECK(current.value().get_int("--count") && current.value().get_int("--count").value() == -1);
    CHECK(current.value().get_bool("--verbose") && current.value().get_bool("--verbose").value());
    CHECK(current.value().positionals.size() == 1 &&
          current.value().positionals[0] == "sindre-cli-input.txt");
    return EXIT_SUCCESS;
}
