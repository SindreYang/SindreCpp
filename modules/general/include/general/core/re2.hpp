#pragma once

#include <general/core/async.hpp>
#include <memory>
#include <string>
#include <string_view>

#if defined(SINDRECPP_WITH_RE2)
#include <re2/re2.h>
#endif

namespace sindrecpp::general::re2 {

class Regex {
public:
    static ::sindrecpp::general::Result<Regex> compile(std::string pattern) noexcept {
#if defined(SINDRECPP_WITH_RE2)
#if defined(SINDRECPP_NO_EXCEPTIONS)
        Regex result; result.pattern_ = std::make_shared<::re2::RE2>(std::move(pattern));
        if (!result.pattern_->ok()) return ::sindrecpp::general::Result<Regex>::failure(
            std::make_error_code(std::errc::invalid_argument), result.pattern_->error(), "re2.compile");
        return ::sindrecpp::general::Result<Regex>::success(std::move(result));
#else
        try {
            Regex result; result.pattern_ = std::make_shared<::re2::RE2>(std::move(pattern));
            if (!result.pattern_->ok()) return ::sindrecpp::general::Result<Regex>::failure(
                std::make_error_code(std::errc::invalid_argument), result.pattern_->error(), "re2.compile");
            return ::sindrecpp::general::Result<Regex>::success(std::move(result));
        } catch (const std::exception &error) {
            return ::sindrecpp::general::Result<Regex>::failure(
                std::make_error_code(std::errc::invalid_argument), error.what(), "re2.compile");
        }
#endif
#else
        (void)pattern;
        return ::sindrecpp::general::Result<Regex>::failure(
            std::make_error_code(std::errc::function_not_supported), "RE2 support is not enabled", "re2.compile");
#endif
    }
    bool match(std::string_view text) const noexcept {
#if defined(SINDRECPP_WITH_RE2)
        return pattern_ && ::re2::RE2::FullMatch(::re2::StringPiece(text.data(), text.size()), *pattern_);
#else
        (void)text; return false;
#endif
    }

    ::sindrecpp::general::Result<bool> try_match(std::string_view text) const noexcept {
#if defined(SINDRECPP_WITH_RE2)
        if (!pattern_)
            return ::sindrecpp::general::Result<bool>::failure(
                std::make_error_code(std::errc::invalid_argument), "Regex is not initialized", "re2.match");
        return ::sindrecpp::general::Result<bool>::success(
            ::re2::RE2::FullMatch(::re2::StringPiece(text.data(), text.size()), *pattern_));
#else
        (void)text;
        return ::sindrecpp::general::Result<bool>::failure(
            std::make_error_code(std::errc::function_not_supported), "RE2 support is not enabled", "re2.match");
#endif
    }

    ::sindrecpp::general::Result<std::string> replace(std::string_view text,
                                                       std::string_view replacement) const noexcept {
#if defined(SINDRECPP_WITH_RE2)
        if (!pattern_)
            return ::sindrecpp::general::Result<std::string>::failure(
                std::make_error_code(std::errc::invalid_argument), "Regex is not initialized", "re2.replace");
        std::string result(text);
        ::re2::RE2::GlobalReplace(&result, *pattern_, std::string(replacement));
        return ::sindrecpp::general::Result<std::string>::success(std::move(result));
#else
        (void)text; (void)replacement;
        return ::sindrecpp::general::Result<std::string>::failure(
            std::make_error_code(std::errc::function_not_supported), "RE2 support is not enabled", "re2.replace");
#endif
    }

private:
#if defined(SINDRECPP_WITH_RE2)
    std::shared_ptr<::re2::RE2> pattern_;
#endif
};

} // namespace sindrecpp::general::re2
