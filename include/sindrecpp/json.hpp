#pragma once

#if !defined(SINDRECPP_WITH_JSON)
#error "Enable SINDRECPP_WITH_JSON and link SindreCpp::Json before including this header."
#endif

#include <simdjson.h>

#include <string_view>

namespace sindrecpp::json {

using Parser = simdjson::dom::parser;
using Element = simdjson::dom::element;
using Object = simdjson::dom::object;
using Array = simdjson::dom::array;
using Error = simdjson::error_code;
namespace native = simdjson;

class Document {
public:
    explicit Document(std::string_view json)
        : input_(json), root_(parser_.parse(input_).value()) {}

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;
    Document(Document&&) = delete;
    Document& operator=(Document&&) = delete;

    const Element& root() const noexcept { return root_; }

private:
    simdjson::padded_string input_;
    Parser parser_;
    Element root_;
};

inline Document parse(std::string_view json) { return Document(json); }

} // namespace sindrecpp::json
