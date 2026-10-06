#pragma once

#if !defined(SINDRECPP_WITH_JSON)
#error "Enable SINDRECPP_WITH_JSON and link SindreCpp::General before including this header."
#endif

#include <general/core/async.hpp>
#include <general/core/json.hpp>
#include <general/core/string.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace sindrecpp::general::config {

class Config {
    std::unordered_map<std::string, std::string> values_;

    static ::sindrecpp::general::Result<void> flatten(
        const ::sindrecpp::general::json::Element &element, std::string prefix, Config &result) {
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
            return ::sindrecpp::general::Result<void>::success();
        }
        case Type::STRING: {
            auto value = element.get_string();
            if (value.error() != simdjson::SUCCESS)
                return invalid("Invalid JSON string", prefix);
            result.values_[std::move(prefix)] = std::string(value.value_unsafe());
            return ::sindrecpp::general::Result<void>::success();
        }
        case Type::INT64:
            result.values_[std::move(prefix)] = std::to_string(element.get_int64().value_unsafe());
            return ::sindrecpp::general::Result<void>::success();
        case Type::UINT64:
            result.values_[std::move(prefix)] = std::to_string(element.get_uint64().value_unsafe());
            return ::sindrecpp::general::Result<void>::success();
        case Type::DOUBLE: {
            std::ostringstream text;
            text.precision(17);
            text << element.get_double().value_unsafe();
            result.values_[std::move(prefix)] = text.str();
            return ::sindrecpp::general::Result<void>::success();
        }
        case Type::BOOL:
            result.values_[std::move(prefix)] = element.get_bool().value_unsafe() ? "true" : "false";
            return ::sindrecpp::general::Result<void>::success();
        default:
            return invalid("JSON arrays and null values are not scalar configuration entries", prefix);
        }
    }

    static ::sindrecpp::general::Result<void> invalid(std::string message, const std::string &key) {
        return ::sindrecpp::general::Result<void>::failure(
            std::make_error_code(std::errc::invalid_argument), std::move(message), "config." + key);
    }

    const std::string *find(std::string_view key) const {
        auto it = values_.find(std::string(key));
        return it == values_.end() ? nullptr : &it->second;
    }

  public:
    static Config with_defaults(std::initializer_list<std::pair<std::string, std::string>> defaults) {
        Config result;
        for (const auto &entry : defaults) result.values_[entry.first] = entry.second;
        return result;
    }

    void merge(const Config &override_values) {
        for (const auto &entry : override_values.values_) values_[entry.first] = entry.second;
    }

    static ::sindrecpp::general::Result<Config> from_json(std::string_view text) {
        return from_json(text, Config{});
    }

    static ::sindrecpp::general::Result<Config> from_json(std::string_view text,
                                                           const Config &defaults) {
        auto document = ::sindrecpp::general::json::try_parse(text);
        if (!document)
            return ::sindrecpp::general::Result<Config>::failure(document.error());
        Config result = defaults;
        auto status = flatten(document.value().root(), {}, result);
        if (!status)
            return ::sindrecpp::general::Result<Config>::failure(status.error());
        return ::sindrecpp::general::Result<Config>::success(std::move(result));
    }

    static ::sindrecpp::general::Result<Config> from_file(const std::filesystem::path &path) {
        return from_file(path, Config{});
    }

    static ::sindrecpp::general::Result<Config> from_file(const std::filesystem::path &path,
                                                          const Config &defaults) {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return ::sindrecpp::general::Result<Config>::failure(
                std::make_error_code(std::errc::no_such_file_or_directory), "Cannot open config file",
                "config.file");
        std::ostringstream text;
        text << input.rdbuf();
        if (!input.good() && !input.eof())
            return ::sindrecpp::general::Result<Config>::failure(
                std::make_error_code(std::errc::io_error), "Cannot read config file", "config.file");
        return from_json(text.str(), defaults);
    }

    void set(std::string key, std::string value) { values_[std::move(key)] = std::move(value); }

    ::sindrecpp::general::Result<void> apply_environment(std::string_view prefix = {}) {
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
        return ::sindrecpp::general::Result<void>::success();
    }

    ::sindrecpp::general::Result<std::string> get_string(std::string_view key) const {
        if (const auto *value = find(key))
            return ::sindrecpp::general::Result<std::string>::success(*value);
        return ::sindrecpp::general::Result<std::string>::failure(
            std::make_error_code(std::errc::no_such_file_or_directory), "Configuration key is missing",
            "config." + std::string(key));
    }
    std::string get_string_or(std::string_view key, std::string fallback = {}) const {
        if (const auto *value = find(key)) return *value;
        return fallback;
    }
    ::sindrecpp::general::Result<std::int64_t> get_int(std::string_view key) const {
        auto value = get_string(key);
        if (!value) return ::sindrecpp::general::Result<std::int64_t>::failure(value.error());
        auto parsed = ::sindrecpp::general::string::parse_int(value.value());
        if (!parsed)
            return ::sindrecpp::general::Result<std::int64_t>::failure(
                parsed.error().code, parsed.error().message, "config." + std::string(key));
        return parsed;
    }
    ::sindrecpp::general::Result<bool> get_bool(std::string_view key) const {
        auto value = get_string(key);
        if (!value) return ::sindrecpp::general::Result<bool>::failure(value.error());
        const auto normalized = ::sindrecpp::general::string::lower_ascii(value.value());
        if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on")
            return ::sindrecpp::general::Result<bool>::success(true);
        if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off")
            return ::sindrecpp::general::Result<bool>::success(false);
        return ::sindrecpp::general::Result<bool>::failure(
            std::make_error_code(std::errc::invalid_argument), "Invalid boolean configuration value",
            "config." + std::string(key));
    }
    ::sindrecpp::general::Result<double> get_float(std::string_view key) const {
        auto value = get_string(key);
        if (!value) return ::sindrecpp::general::Result<double>::failure(value.error());
        auto parsed = ::sindrecpp::general::string::parse_float(value.value());
        if (!parsed) return ::sindrecpp::general::Result<double>::failure(
            parsed.error().code, parsed.error().message, "config." + std::string(key));
        return parsed;
    }
    double get_float_or(std::string_view key, double fallback) const {
        auto value = find(key);
        if (!value) return fallback;
        auto parsed = ::sindrecpp::general::string::parse_float(*value);
        return parsed ? parsed.value() : fallback;
    }
};

} // namespace sindrecpp::general::config
