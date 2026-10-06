#pragma once

#if !defined(SINDRECPP_WITH_JSON)
#error "Enable SINDRECPP_WITH_JSON and link SindreCpp::General before including this header."
#endif

#include <simdjson.h>
#include <general/core/async.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace sindrecpp::general::json {

using Parser = simdjson::dom::parser;
using Element = simdjson::dom::element;
using Object = simdjson::dom::object;
using Array = simdjson::dom::array;
using Error = simdjson::error_code;
namespace native = simdjson;

class Document {
public:
    const Element& root() const noexcept { return state_->root; }

private:
    struct State {
        explicit State(std::string_view json) : input(json) {
            auto parsed = parser.parse(input);
            error = parsed.error();
            if (error == simdjson::SUCCESS) root = parsed.value_unsafe();
        }
        simdjson::padded_string input;
        Parser parser;
        Element root;
        simdjson::error_code error = simdjson::SUCCESS;
    };
    explicit Document(std::shared_ptr<State> state) : state_(std::move(state)) {}
    friend ::sindrecpp::general::Result<Document> try_parse(std::string_view);
    std::shared_ptr<State> state_;
};

inline ::sindrecpp::general::Result<Document> try_parse(std::string_view json) {
#if defined(SINDRECPP_NO_EXCEPTIONS)
    auto state = std::make_shared<Document::State>(json);
    if (state->error != simdjson::SUCCESS)
        return ::sindrecpp::general::Result<Document>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "Invalid JSON: " + std::string(simdjson::error_message(state->error)), "json.parse");
    return ::sindrecpp::general::Result<Document>::success(Document(std::move(state)));
#else
    try {
        auto state = std::make_shared<Document::State>(json);
        if (state->error != simdjson::SUCCESS)
            return ::sindrecpp::general::Result<Document>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Invalid JSON: " + std::string(simdjson::error_message(state->error)), "json.parse");
        return ::sindrecpp::general::Result<Document>::success(Document(std::move(state)));
    } catch (const std::exception& error) {
        return ::sindrecpp::general::Result<Document>::failure(
            std::make_error_code(std::errc::invalid_argument), error.what(), "json.parse");
    } catch (...) {
        return ::sindrecpp::general::Result<Document>::failure(
            std::make_error_code(std::errc::io_error), "Unknown JSON parsing failure", "json.parse");
    }
#endif
}

inline ::sindrecpp::general::Result<Document> parse(std::string_view json) {
    return try_parse(json);
}

// Native simdjson remains available through namespace native. All SindreCpp
// parsing entry points return Result and never expose parse exceptions.
} // namespace sindrecpp::general::json
