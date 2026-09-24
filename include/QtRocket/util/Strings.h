#pragma once

#include <concepts>
#include <cstddef>
#include <format>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// Locale-independent text helpers: OpenRocket's TextUtil and StringUtils plus the parsing the
/// .ork reader needs. Every function here treats text as UTF-8 bytes and never consults the C or
/// C++ locale, so the strings written to and read from files are the same on every machine.
namespace QtRocket::Strings
{

/// Decimal places for values shown to the user (TextUtil.DEFAULT_DECIMAL_PLACES).
inline constexpr int kDefaultDecimalPlaces = 3;
/// Decimal places for values written to .ork files (TextUtil.STORAGE_DECIMAL_PLACES).
inline constexpr int kStorageDecimalPlaces = 6;

/// Formats @p value the way OpenRocket stores doubles (TextUtil.doubleToString). A value within
/// MathUtil.EPSILON / 2 of zero gives "0", NaN gives "NaN" and the infinities "Inf" / "-Inf".
/// Otherwise the value's decimal digits, taken as Java's Formatter takes them (an integer below
/// 2^63 exactly, anything else as the shortest digits that read back as the same double), are
/// rounded half-up to @p decimalPlaces decimals, trailing zeros are dropped, and when
/// @p exponentialNotation is set a magnitude below 0.001 or of 10000 and above is written as
/// mantissa + "e" + exponent ("3.142e-5", "3.1e4"). A negative @p decimalPlaces counts as 0.
/// Deviation: from 2^63 (9.2e18) upwards OpenRocket on JDK 17 still generates digits with an
/// older algorithm that can differ from the shortest digits in the last place at a rounding tie
/// ("8.638e20" for 8.6385e20, here "8.639e20"); JDK 21+ (JDK-8300869) prints what this does.
[[nodiscard]] std::string doubleToString(double value, int decimalPlaces, bool exponentialNotation);

/// doubleToString(value, decimalPlaces, true).
[[nodiscard]] std::string doubleToString(double value, int decimalPlaces);

/// doubleToString(value, kDefaultDecimalPlaces, true).
[[nodiscard]] std::string doubleToString(double value);

/// Parses what Java's Double.parseDouble accepts of the values OpenRocket writes: an optional
/// sign, decimal digits with an optional fraction and exponent ("1.5", ".5", "5.", "1e-5",
/// "-2.5E3"), "NaN", "Infinity" and OpenRocket's own "Inf", each with an optional sign, and
/// whitespace (characters at or below U+0020) at either end. Anything else, including a partial
/// match such as "1.5x", hexadecimal floats and Java's "1.0d" suffixes, gives nullopt. A literal
/// below the smallest subnormal double ("1e-400") gives the zero of its sign, as in Java; one
/// above the largest double ("1e999") gives nullopt where Java gives an infinity.
[[nodiscard]] std::optional<double> parseDouble(std::string_view text) noexcept;

/// Parses a decimal integer as Java's Integer.parseInt does: an optional sign and digits only, no
/// whitespace, and nullopt when the value does not fit an int.
[[nodiscard]] std::optional<int> parseInt(std::string_view text) noexcept;

/// StringUtils.convertToDouble: parses a number written with either a dot or a comma as the
/// decimal separator, taking the last of them as the separator and the others as thousands
/// separators ("1.500,61" and "1,500.61" both give 1500.61). Fails like parseDouble().
[[nodiscard]] std::optional<double> convertToDouble(std::string_view text);

/// Java's String.trim(): @p text without leading and trailing characters at or below U+0020. The
/// result views the caller's characters, so a temporary std::string cannot be trimmed (the view
/// would dangle once it is destroyed): that overload is deleted.
[[nodiscard]] std::string_view trim(std::string_view text) noexcept;
template <std::same_as<std::string> String>
std::string_view trim(const String&& text) = delete;

/// StringUtils.isEmpty: true when @p text trims to nothing.
[[nodiscard]] bool isEmpty(std::string_view text) noexcept;

/// ASCII letters lower-cased; every other byte, including UTF-8 sequences, is kept as it is
/// (Java's toLowerCase(Locale.ENGLISH) also folds non-ASCII letters).
[[nodiscard]] std::string toLower(std::string_view text);

/// True when @p a and @p b are the same once ASCII letters are lower-cased as toLower() does;
/// every other byte must match exactly (Java's equalsIgnoreCase also folds non-ASCII letters).
[[nodiscard]] bool equalsIgnoreAsciiCase(std::string_view a, std::string_view b) noexcept;

/// Splits at every @p separator, keeping empty fields: "a,,b" gives {"a", "", "b"} and "" gives
/// {""}.
[[nodiscard]] std::vector<std::string> split(std::string_view text, char separator);

/// Java's String.split(String) for a one-character separator: as split(), but trailing empty
/// fields are dropped, so "a,,b,," gives {"a", "", "b"} and "," gives {} (no fields at all);
/// text without the separator gives {text}, so "" gives {""}.
[[nodiscard]] std::vector<std::string> splitJava(std::string_view text, char separator);

/// StringUtils.join: the values formatted with std::format("{}") and separated by @p separator;
/// an empty range gives "". Deviation: OpenRocket omits the separator after a leading empty
/// element; here every element is separated.
template <std::ranges::input_range Range>
    requires std::formattable<std::ranges::range_reference_t<const Range&>, char>
[[nodiscard]] std::string join(std::string_view separator, const Range& values)
{
    std::string out;
    bool        first = true;
    for (const auto& value : values)
    {
        if (!first)
        {
            out.append(separator);
        }
        first = false;
        std::format_to(std::back_inserter(out), "{}", value);
    }
    return out;
}

/// TextUtil.hexString: the bytes as lowercase hexadecimal, two characters per byte, no
/// separators ("" for no bytes). OpenRocket's bytesToHex gives the same string.
[[nodiscard]] std::string hexString(std::span<const std::byte> bytes);

/// TextUtil.asciiBytes: the US-ASCII encoding of UTF-8 @p text, with every non-ASCII code point
/// replaced by '?' as Java's encoder does.
[[nodiscard]] std::vector<std::byte> asciiBytes(std::string_view text);

/// TextUtil.escapeXML: escapes &, <, >, " as entities and ', DEL and every control character
/// except tab, newline and carriage return as numeric references ("&#39;"). The result is valid
/// XML and HTML.
[[nodiscard]] std::string escapeXml(std::string_view text);

/// StringUtils.escapeHtml: escapes &, <, >, " and ' and turns newlines into "<br>".
[[nodiscard]] std::string escapeHtml(std::string_view text);

/// StringUtils.escapeCSV: quotes @p text, doubling its quotes, when it holds a quote, a comma, a
/// carriage return or a newline; otherwise returns it unchanged.
[[nodiscard]] std::string escapeCsv(std::string_view text);

/// StringUtils.removeHTMLTags: removes every "<...>" run; a '<' without a closing '>' is kept.
[[nodiscard]] std::string removeHtmlTags(std::string_view text);

}  // namespace QtRocket::Strings
