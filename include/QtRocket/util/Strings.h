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
/// Deviation: from 2^63 (9.2e18) upwards, and for subnormals, OpenRocket on JDK 17 still takes
/// its digits from FloatingDecimal's older algorithm, which often differ from the shortest ones:
/// longer, or not the closest ("8.638e20" for 8.6385e20, here "8.639e20"). The shortest digits
/// are what JDK 21+ (JDK-8300869) prints.
[[nodiscard]] std::string doubleToString(double value, int decimalPlaces, bool exponentialNotation);

/// doubleToString(value, decimalPlaces, true).
[[nodiscard]] std::string doubleToString(double value, int decimalPlaces);

/// doubleToString(value, kDefaultDecimalPlaces, true).
[[nodiscard]] std::string doubleToString(double value);

/// Java's String.format(Locale.ENGLISH, "%.<precision>f", value), which FixedPrecisionUnit
/// formats with: the value's decimal digits, taken as doubleToString() takes them, rounded half-up
/// to @p precision decimals and written in full, trailing zeros included ("1.50"; "0" for
/// precision 0; "100000000000000000000.0" for 1e20). NaN gives "NaN" and the infinities
/// "Infinity" / "-Infinity"; a negative value keeps its sign even when its digits round to zero
/// ("-0.0"), as does a negative zero. A negative @p precision counts as 0. Deviation: the digits
/// of a value from 2^63 up are the shortest ones, as in doubleToString(), so "%.0f" of
/// 6.84232346E19 is "68423234600000000000" here and "68423234599999996000" on JDK 17.
[[nodiscard]] std::string formatFixed(double value, int precision);

/// Java's String.format(Locale.ENGLISH, "%.<precision>e", value): one leading digit, a point and
/// exactly @p precision decimals (no point for precision 0), "e", the exponent's sign and at
/// least two exponent digits ("1.25e+06", "5e-04", "1.00e+100"), the digits rounded half-up as
/// formatFixed() rounds them. NaN, the infinities and the sign are as in formatFixed().
[[nodiscard]] std::string formatScientific(double value, int precision);

/// Java's Double.toString(value), the form a double takes in a string concatenation
/// (Material.toStorableString, Tick.toString): "NaN", "Infinity", "-Infinity", "0.0", "-0.0"; a
/// magnitude from 0.001 up to but excluding 1e7 as integer digits, a point and at least one
/// fraction digit ("980.0", "0.001", "123456.789"); anything else as one digit, a point, at least
/// one fraction digit, "E" and the exponent ("1.0E7", "3.0E-4", "2.6E10"). The digits are the
/// ones doubleToString() takes from Java, so an integer between 2^53 and 2^63 keeps Java's exact
/// digits (2^60 is "1.15292150460684698E18"). Deviation: from 2^63 upwards, and for subnormals,
/// JDK 17's FloatingDecimal digits often differ from the shortest ones (JDK-4511638), being
/// longer (6.8423234599999996E19 for 6.84232346E19, 9.999999999999999E22 for 1e23, 4.9E-324 for
/// Double.MIN_VALUE) or not the closest (-3.8189059803482716E25 where the shortest is
/// -3.8189059803482717E25). This prints the shortest digits, as JDK 19+ does; both forms read
/// back as the same double.
[[nodiscard]] std::string javaDoubleToString(double value);

/// Parses what Java's Double.parseDouble accepts of the values OpenRocket writes: an optional
/// sign, decimal digits with an optional fraction and exponent ("1.5", ".5", "5.", "1e-5",
/// "-2.5E3"), "NaN", "Infinity" and OpenRocket's own "Inf", each with an optional sign, and
/// whitespace (characters at or below U+0020) at either end. Anything else, including a partial
/// match such as "1.5x", hexadecimal floats and Java's "1.0d" suffixes, gives nullopt. A literal
/// below the smallest subnormal double ("1e-400") gives the zero of its sign, as in Java; one
/// above the largest double ("1e999") gives nullopt where Java gives an infinity.
[[nodiscard]] std::optional<double> parseDouble(std::string_view text) noexcept;

/// Java's Double.parseDouble exactly: whitespace (characters at or below U+0020) trimmed at
/// either end, an optional sign, then "NaN", "Infinity", a decimal number (digits with an
/// optional point and fraction, at least one digit, and an optional exponent: "1.5", ".5", "5.",
/// "-2.5E3", "1e+05") or a hexadecimal one with a binary exponent ("0x1.8p1", "0X.8P-3"), either
/// number optionally followed by one of f, F, d, D ("1.5d", which parses as a double all the
/// same). The result is correctly rounded; a magnitude above the largest double gives an
/// infinity and one below the smallest subnormal a zero, each with the sign. Anything else,
/// OpenRocket's "Inf" included, gives nullopt (Java's NumberFormatException).
[[nodiscard]] std::optional<double> javaParseDouble(std::string_view text) noexcept;

/// Parses a decimal integer as Java's Integer.parseInt does: an optional sign and digits only, no
/// whitespace, and nullopt when the value does not fit an int.
[[nodiscard]] std::optional<int> parseInt(std::string_view text) noexcept;

/// StringUtils.convertToDouble: parses a number written with either a dot or a comma as the
/// decimal separator, taking the last of them as the separator and the others as thousands
/// separators ("1.500,61" and "1,500.61" both give 1500.61), then parses the result with
/// javaParseDouble() (Double.parseDouble), so it fails the same way.
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
/// every other byte must match exactly (Java's equalsIgnoreCase also folds non-ASCII letters;
/// see javaEqualsIgnoreCase()).
[[nodiscard]] bool equalsIgnoreAsciiCase(std::string_view a, std::string_view b) noexcept;

// The Java String operations below read UTF-8 text as the String it stands for: its code points,
// and for compareTo and hashCode their UTF-16 code units (a code point above U+FFFF is a
// surrogate pair). A byte that does not start a well-formed UTF-8 sequence reads as U+FFFD, as
// Java's decoder replaces it.

/// The code points of UTF-8 @p text, what String.codePoints() gives for the String.
[[nodiscard]] std::u32string toCodePoints(std::string_view text);

/// String.equalsIgnoreCase: the same length, and every pair of characters equal after
/// Character.toUpperCase and then Character.toLowerCase (JDK 17's Unicode 13 case data). Beyond
/// ASCII this matches "µm" with "μm" (U+00B5 and U+03BC), "Ölpapier" with "ölpapier", and the
/// Kelvin sign (U+212A) with "k", the long s (U+017F) with "s", U+0130 and U+0131 with "i".
[[nodiscard]] bool javaEqualsIgnoreCase(std::string_view a, std::string_view b) noexcept;

/// String.compareTo: the difference of the first differing UTF-16 code units, else of the
/// lengths in code units. This orders a code point above U+FFFF (a surrogate pair, D800-DFFF)
/// before U+E000-U+FFFF, where the UTF-8 bytes order it after.
[[nodiscard]] int javaCompareTo(std::string_view a, std::string_view b) noexcept;

/// String.hashCode: h = 31 * h + unit over the UTF-16 code units, wrapping as a Java int.
[[nodiscard]] int javaHashCode(std::string_view text) noexcept;

/// Splits at every @p separator, keeping empty fields: "a,,b" gives {"a", "", "b"} and "" gives
/// {""}.
[[nodiscard]] std::vector<std::string> split(std::string_view text, char separator);

/// Java's String.split(String) for a one-character separator: as split(), but trailing empty
/// fields are dropped, so "a,,b,," gives {"a", "", "b"} and "," gives {} (no fields at all);
/// text without the separator gives {text}, so "" gives {""}.
[[nodiscard]] std::vector<std::string> splitJava(std::string_view text, char separator);

/// The .ork spelling of a Java enum constant that OpenRocket's DocumentConfig.findEnum() matches
/// against: the constant's name with ASCII letters lower-cased and every '_' removed
/// ("UPPER_IGNITION" gives "upperignition"). DesignType.getStorableString() and the stage
/// separation saver write exactly this.
[[nodiscard]] std::string toOrkEnumName(std::string_view enumName);

/// DocumentConfig.findEnum()'s test for one constant: @p text, trimmed as String.trim() does,
/// equals toOrkEnumName(@p enumName). Note that the plain lower-cased name the savers write for
/// some enums ("mirror_xy") does not match a name with an underscore, exactly as in OpenRocket.
[[nodiscard]] bool orkEnumNameMatches(std::string_view text, std::string_view enumName);

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
