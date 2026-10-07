#pragma once

#include <sindre/general/core.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(SINDRE_WITH_STRING)
#include <cs_string.h>
#endif

namespace sindre::general::string {

/// @brief 表示未找到或不受限的位置。
inline constexpr std::size_t npos = std::string_view::npos;

/// @brief 删除首尾 ASCII 空白并返回原文本视图。
std::string_view trim(std::string_view text) noexcept;
/// @brief 删除首尾 ASCII 空白并返回新字符串。
std::string trim_copy(std::string_view text);
/// @brief 查询文本前缀。
bool starts_with(std::string_view text, std::string_view prefix) noexcept;
/// @brief 查询文本后缀。
bool ends_with(std::string_view text, std::string_view suffix) noexcept;
/// @brief 查询子串。
bool contains(std::string_view text, std::string_view needle) noexcept;
std::size_t count(std::string_view text, std::string_view needle) noexcept;
std::string_view lstrip(std::string_view text, std::string_view chars = " \t\n\r\f\v") noexcept;
std::string_view rstrip(std::string_view text, std::string_view chars = " \t\n\r\f\v") noexcept;
std::string_view strip(std::string_view text, std::string_view chars = " \t\n\r\f\v") noexcept;
/// @brief 替换全部非重叠匹配。
std::string replace_all(std::string_view text, std::string_view from, std::string_view to);
/// @brief 按单字节分隔符返回原文本视图。
std::vector<std::string_view> split_view(std::string_view text, char delimiter);
std::vector<std::string> split(std::string_view text, char delimiter);
std::vector<std::string> split(std::string_view text, std::string_view delimiter,
                               bool keep_empty = true);
std::vector<std::string> rsplit(std::string_view text, std::string_view delimiter,
                                std::size_t max_splits = npos, bool keep_empty = true);
std::string repeat(std::string_view text, std::size_t times);
/// @brief 将 ASCII 字母转换为小写，其他 UTF-8 字节保持不变。
std::string lower_ascii(std::string_view text);
/// @brief 将 ASCII 字母转换为大写，其他 UTF-8 字节保持不变。
std::string upper_ascii(std::string_view text);
/// @brief 校验文本是否为合法 UTF-8。
bool valid_utf8(std::string_view text) noexcept;
Result<std::string> normalize_utf8(std::string_view text);
Result<std::size_t> count_code_points(std::string_view text);
/// @brief 严格解析整数；空白允许出现在首尾。
Result<std::int64_t> parse_int(std::string_view text, int base = 10);
/// @brief 严格解析有限浮点数。
Result<double> parse_float(std::string_view text);
/// @brief 按分隔符拼接字符串视图。
std::string join(const std::vector<std::string_view> &parts, std::string_view separator);
std::string join(std::initializer_list<std::string_view> parts, std::string_view separator);
std::string join(const std::vector<std::string> &parts, std::string_view separator);

/// @brief 轻量拥有型字符串，适合在 C++ API 间传递 UTF-8 文本。
class String {
public:
    String();
    String(const char *text);
    String(std::string text);
    String(std::string_view text);

    const std::string &str() const noexcept;
    std::string_view view() const noexcept;
    const char *c_str() const noexcept;
    const char *data() const noexcept;
    bool empty() const noexcept;
    std::size_t size() const noexcept;
    std::size_t size_bytes() const noexcept;
    String trim() const;
    String lstrip() const;
    String rstrip() const;
    String strip(std::string_view chars = " \t\n\r\f\v") const;
    String lower() const;
    String upper() const;
    bool starts_with(std::string_view prefix) const noexcept;
    bool ends_with(std::string_view suffix) const noexcept;
    bool contains(std::string_view needle) const noexcept;
    std::size_t count(std::string_view needle) const noexcept;
    String replace(std::string_view from, std::string_view to) const;
    std::vector<std::string> split(std::string_view delimiter, bool keep_empty = true) const;
    std::vector<std::string> rsplit(std::string_view delimiter, std::size_t max_splits = npos,
                                    bool keep_empty = true) const;
    String repeat(std::size_t times) const;
    bool is_valid_utf8() const noexcept;
    Result<std::size_t> size_code_points() const;
    Result<std::int64_t> to_int(int base = 10) const;
    Result<double> to_float() const;
    operator std::string_view() const noexcept;

private:
    std::string value_;
};

template <class Part>
inline void concat_append(std::string &result, Part &&part) {
    using Value = std::decay_t<Part>;
    if constexpr (std::is_arithmetic_v<Value>) result += std::to_string(part);
    else if constexpr (std::is_same_v<Value, String>) result.append(part.view());
    else if constexpr (std::is_convertible_v<Part, std::string_view>)
        result.append(std::string_view(std::forward<Part>(part)));
    else result.append(std::forward<Part>(part));
}

template <class... Parts>
inline std::string concat(Parts &&...parts) {
    std::string result;
    (concat_append(result, std::forward<Parts>(parts)), ...);
    return result;
}

#if defined(SINDRE_WITH_STRING)
namespace native = CsString;
using Utf8String = CsString::CsString;
using Utf16String = CsString::CsString_utf16;
#endif

} // namespace sindre::general::string

#if defined(SINDRE_WITH_RE2)
#include <memory>
#include <re2/re2.h>
namespace sindre::general::regex {
class Regex {
public:
    static Result<Regex> compile(std::string pattern) noexcept;
    bool match(std::string_view text) const noexcept;
    Result<bool> try_match(std::string_view text) const noexcept;
    Result<std::string> replace(std::string_view text, std::string_view replacement) const noexcept;
private:
    std::shared_ptr<::re2::RE2> pattern_;
};
}
#endif
