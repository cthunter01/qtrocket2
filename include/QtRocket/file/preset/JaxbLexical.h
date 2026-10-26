#pragma once

#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// How JAXB reads the text of an element of an .orc component preset file into the fields of
/// OpenRocket's DTOs (info.openrocket.core.preset.xml): a double or Double, an Integer, a Boolean,
/// an enum, or a String. OpenRocket unmarshals .orc files with JAXB (jaxb-runtime 4.0.6), whose
/// lexical rules are its own: they are neither Java's Double.parseDouble() and Integer.parseInt()
/// nor XML Schema's. QtRocket reimplements them from what OpenRocket answers for the edge cases of
/// the probe tools/openrocket-probes/orc-loader (its README; the case ids in brackets below), not
/// from JAXB's code.
///
/// Each function reads one text, as JAXB hands it to a field: OrcElement says which texts an
/// element hands over, and how the readings of several texts and several elements combine into
/// the value a field ends up with. A text that a function rejects is a recoverable error for JAXB:
/// it reports it to nobody (OpenRocket installs no event handler), and the field keeps the value
/// it had. The functions take UTF-8 text and never throw. parseBase64Binary() is the byte[] of a
/// <Thumbnail>, which OpenRocket reads through its own Base64Adapter.
namespace QtRocket::JaxbLexical
{

/// Whether @p c is white space to JAXB: ' ', '\t', '\n' or '\r' only. The no-break space U+00A0,
/// the thin space U+2009 and the control characters XML 1.1 allows are not ([N17], [N18], [N20],
/// [XN09], [XI11], [XT26] to [XT29]).
[[nodiscard]] constexpr bool isWhiteSpace(char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/// @p text without the white space (isWhiteSpace()) at either end. The result views the caller's
/// characters, so a temporary std::string cannot be trimmed: that overload is deleted.
[[nodiscard]] std::string_view trim(std::string_view text) noexcept;
template <std::same_as<std::string> String>
std::string_view trim(const String&& text) = delete;

/// Whether @p text is empty or white space only.
[[nodiscard]] bool isWhiteSpaceOnly(std::string_view text) noexcept;

/// A double (or a Double) from @p text, or nullopt when JAXB rejects the text:
/// - The white space at either end is trimmed ("  18.0 \n", "\t18\n", "&#13;18&#13;": [09],
///   [N07], [N08], [XN03], [XN04]); nothing else is.
/// - "NaN", "INF" and "-INF", exactly so after the trimming, are NaN and the infinities ([12],
///   [13], [N23] to [N25]); "Infinity", "inf", "+INF", "+NaN" and "-NaN" are rejected ([14],
///   [XN00] to [XN02], [N26]).
/// - Any other text whose first and last characters are both one of the digits, '.', '+' and '-'
///   is read by Java's Double.parseDouble() (Strings::javaParseDouble()): "18.", ".5", "-.5",
///   "+18", "1.8e+1", "0018", "-0" (negative zero), the hexadecimal "0x1.2p4" and "-0x1.8p1"
///   ([18], [19], [11], [16], [N00] to [N04], [N15], [N22], [XN06], [XN08]), a magnitude
///   beyond the largest double reading as an infinity and one below the smallest as zero
///   ("1e999", "1.7976931348623157e309", "1e-400": [80], [N27], [N32]). Double.parseDouble()
///   rejects "0X12", "1 8", "--18", "+-18", ".", "5.-", "1_8", "18,0" and "1e+" ([17], [N05],
///   [N09] to [N11], [N14], [N16], [N21], [XN07]), and so does this.
/// - Any other text is rejected: an empty or white-space-only one ([08], [XN05], [XN10]), one
///   that starts or ends with anything else ("abc", "18d", "18f", "1e", "e1", a no-break space,
///   Arabic-Indic digits: [07], [15], [N06], [N12], [N13], [N17] to [N20]), although
///   Double.parseDouble() reads "18d", "18f" and "Infinity".
[[nodiscard]] std::optional<double> parseDouble(std::string_view text) noexcept;

/// An Integer from @p text, or nullopt when JAXB rejects the text. JAXB reads the text
/// character by character: white space (isWhiteSpace()) is skipped wherever it is ("1 2" is 12,
/// " 6 " and "\n6\n" are 6: [I01], [I05], [XI02] to [XI04]); a digit '0' to '9' makes the value
/// ten times what it was plus the digit, wrapping around as Java's int does ("2147483648" is
/// -2147483648, "99999999999" is 1215752191, "4294967302" is 6: [I08], [I09], [XI06]); a '-'
/// anywhere makes the result negative, once ("-6", "5-", "--5", "6 -", "+-6" and "-+6" are all
/// negative: [I03], [I06], [I07], [XI00], [XI01], [XI09]; "-2147483649" is 2147483647:
/// [XI07]); a '+' is skipped ([I02]). Any other character rejects the text ("6.0", "0x6", "6e0",
/// an Arabic-Indic digit, a no-break space: [I04], [I11] to [I13], [XI11]). So an empty text,
/// white space only, "+" and "-" are all 0 ([I10], [I14], [I15], [XI08]).
[[nodiscard]] std::optional<int> parseInt(std::string_view text) noexcept;

/// What reading a text into a Boolean gives.
enum class ParsedBoolean
{
    IS_TRUE,
    IS_FALSE,
    IS_NULL,   ///< the field is set to null
    REJECTED,  ///< the field keeps the value it had
};

/// A Boolean from @p text. JAXB reads the first character after the white space (isWhiteSpace())
/// the text starts with:
/// - An empty text is null ([B15], [XB18]); one of white space only is false ([XB00] to [XB02]).
/// - '1' is true and '0' false. 't' is true when "rue" follows it and 'f' false when "alse"
///   follows it; when anything else follows (or too little), the text is null ("tr", "tx",
///   "trux", "t rue", "fals", "fx": [B08], [B12], [XB12], [XB22] to [XB25], [XB27] to [XB30]).
///   But a 't' or an 'f' that is the last character of the text rejects it ("t", " t", "f":
///   [B07], [B11], [XB14], [XB20], [XB26], [XB31]).
/// - Any other first character is false ("2", "x", "-", "Z", a no-break space, an e with an acute
///   accent: [B14], [XB03], [XB11], [XB15], [XB33], [XB35]), an upper-case 'T' too, which makes
///   "True" null by the next rule ([B06]).
/// - Then only white space may follow; anything else makes the text null ("truex", "true1",
///   "falsey", "01", "10", "0 0", "True", "yes", "falsetrue", "t ": [B06], [B09], [B10], [B13],
///   [B16] to [B18], [XB09], [XB16], [XB32]). So "true", " true ", "1\n", "false " and " 2 " read
///   as Java would expect, and a character beyond U+FFFF, two UTF-16 units to Java, is null where
///   it comes first ([XB34]).
[[nodiscard]] ParsedBoolean parseBoolean(std::string_view text) noexcept;

/// The index of @p text among the lexical names of an enum (@p names: ShapeDTO's "OGIVE", ...,
/// MaterialTypeDTO's "BULK", ..., MaterialGroupDTO's "Metals", ...), or nullopt, JAXB's null,
/// when it is none of them. The text must equal a name exactly: it is not trimmed and its case
/// counts (" OGIVE ", "\nOGIVE\n", "OGIVE" and a no-break space, "Ogive", "ogive", "" are all
/// null: [61], [62], [S01] to [S04], [XS04]).
[[nodiscard]] std::optional<std::size_t> parseEnum(
    std::string_view text, std::span<const std::string_view> names) noexcept;

/// The bytes of a base64 text as OpenRocket's Base64Adapter reads a <Thumbnail> (JAXB's
/// DatatypeConverter.parseBase64Binary()), or nullopt when it rejects the text (an exception to
/// JAXB, which reports it to nobody; the field keeps the value it had):
/// - A text with a character beyond U+007F is rejected ([XH32]), and so is one whose length is
///   not a multiple of four; the text is not trimmed, so "Q", "QQ", "QQ=", "QUJD=", "QU=JD",
///   " QUJD ", "aGVsbG8" and "aGVsbG8=" with a line feed after it are all rejected ([76],
///   [XH00] to [XH02], [XH05] to [XH07], [XH10] to [XH12], [XH28], [XH34]).
/// - Otherwise the characters 'A' to 'Z', 'a' to 'z', '0' to '9', '+' and '/' (the values 0 to
///   63) and the padding '=' are taken in groups of four, and every other character is skipped,
///   so "QU D", "QU!D" and "-_-_" make no group and give zero bytes, as an empty text does
///   ([XH03], [XH25] to [XH27]). A group a, b, c, d gives the byte a << 2 | b >> 4, then
///   b << 4 | c >> 2 unless c is a padding, then c << 6 | d unless d is a padding, each the low
///   eight bits of its value, where a padding counts as 127: "QUJD" is 41 42 43, "QQ==" 41,
///   "QUI=" 41 42, "====" ff, "Q===" 47, "=QUJ" fd 05 09, "QU=D" 41 c3 ([75], [XH04], [XH08],
///   [XH09], [XH13] to [XH19], [XH22] to [XH24], [XH29] to [XH31], [XH35], [XH36]). A padding
///   ends nothing ("QQ==QUJD" is 41 41 42 43, "QUJD====" 41 42 43 ff: [XH20], [XH21], [XH33]),
///   and the characters after the last full group are dropped.
[[nodiscard]] std::optional<std::vector<std::byte>> parseBase64Binary(std::string_view text);

}  // namespace QtRocket::JaxbLexical
