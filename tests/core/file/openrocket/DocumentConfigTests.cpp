#include "QtRocket/file/openrocket/DocumentConfig.h"

#include <cmath>
#include <format>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Strings.h"

// The expectations are what OpenRocket's DocumentConfig answers (probe DocumentConfigProbe of
// part D4), and for parseDouble() and parseInt() what the JDK's Double.parseDouble and
// Integer.parseInt answer (probe Numbers of the review fixes, probes/tier9a-fix-document/out/
// Numbers.out, JDK 17).

namespace
{

using QtRocket::DocumentConfig;
using QtRocket::ErrorCode;
using QtRocket::LineStyle;
using QtRocket::Result;
using QtRocket::Simulation;
using QtRocket::SimulationAbort;

using Texts = std::vector<std::string>;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

/// @p function of every one of @p texts.
template <class Function>
[[nodiscard]] Texts each(std::initializer_list<std::string_view> texts, Function function)
{
    Texts results;
    for (const std::string_view text : texts)
    {
        results.emplace_back(function(text));
    }
    return results;
}

// DocumentConfigTest.testFileVersionDivisor: OpenRocket compares the loader's constant with the
// saver's. Here there is one constant, which a saver takes from the loader's table, so what is
// left to assert is its value and what follows from it.
static_assert(DocumentConfig::kFileVersionDivisor == 100);

// DocumentConfigTest.testFileVersionDivisor
TEST(DocumentConfigTest, FileVersionDivisor)
{
    EXPECT_EQ(DocumentConfig::kFileVersionDivisor, 100);
    // "1.10" is 110: the major version is the quotient and the minor one the remainder.
    EXPECT_EQ((1 * DocumentConfig::kFileVersionDivisor) + 10, 110);
    EXPECT_EQ(110 / DocumentConfig::kFileVersionDivisor, 1);
    EXPECT_EQ(110 % DocumentConfig::kFileVersionDivisor, 10);
}

/// The versions the loader knows, each followed by a comma.
[[nodiscard]] std::string supportedVersions()
{
    std::string joined;
    for (const std::string_view version : DocumentConfig::kSupportedVersions)
    {
        joined += std::string(version) + ",";
    }
    return joined;
}

/// "yes" or "no": whether @p version is one the loader knows.
[[nodiscard]] std::string_view supported(std::string_view version)
{
    return DocumentConfig::isSupportedVersion(version) ? "yes" : "no";
}

TEST(DocumentConfig, KnowsTheTwelveFormatVersions)
{
    // "SUPPORTED_VERSIONS = 1.0,1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,1.10,1.11"
    EXPECT_EQ(supportedVersions(), "1.0,1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,1.10,1.11,");
    EXPECT_EQ(DocumentConfig::kSupportedVersions.size(), 12U);

    EXPECT_EQ(each({"1.0", "1.4", "1.9", "1.10", "1.11"}, supported), Texts(5, "yes"));
    EXPECT_EQ(each({"1.12", "2.0", "1", "1.1 ", " 1.1", "01.1", "1.01", ""}, supported),
              Texts(8, "no"));
}

/// The name of the status findEnum() finds for @p text, or "null".
[[nodiscard]] std::string status(std::optional<std::string_view> text)
{
    const std::optional<Simulation::Status> found = DocumentConfig::findEnum(
        text, Simulation::kAllStatuses, [](Simulation::Status value) { return name(value); });
    return found.has_value() ? std::string(name(*found)) : "null";
}

/// status() of a text that is there.
[[nodiscard]] std::string statusOf(std::string_view text)
{
    return status(text);
}

TEST(DocumentConfig, FindsAnEnumByItsNameInLowerCaseWithoutUnderscores)
{
    EXPECT_EQ(
        each({"uptodate", "loaded", "outdated", "external", "notsimulated", "cantrun", "aborted"},
             statusOf),
        (Texts{"UPTODATE", "LOADED", "OUTDATED", "EXTERNAL", "NOT_SIMULATED", "CANT_RUN",
               "ABORTED"}));
    // Trimmed as String.trim() trims.
    EXPECT_EQ(status(" uptodate "), "UPTODATE");
    EXPECT_EQ(status("\tloaded\n"), "LOADED");
    // And nothing else: not the constant's own name, no other case, no other white space.
    EXPECT_EQ(each({"", "UPTODATE", "up_to_date", "not_simulated", "NOT_SIMULATED", "bogus",
                    "upto date", "\xC2\xA0loaded"},
                   statusOf),
              Texts(8, "null"));
    EXPECT_EQ(status(std::nullopt), "null");
}

[[nodiscard]] std::optional<SimulationAbort::Cause> cause(std::string_view text)
{
    return DocumentConfig::findEnum(text, SimulationAbort::kAllCauses, &QtRocket::causeName);
}

[[nodiscard]] std::optional<LineStyle> style(std::string_view text)
{
    return DocumentConfig::findEnum(text, QtRocket::kAllLineStyles, &QtRocket::lineStyleName);
}

TEST(DocumentConfig, FindsTheConstantsOfOtherEnums)
{
    EXPECT_EQ(cause("nomotorsdefined"),
              std::optional<SimulationAbort::Cause>(SimulationAbort::Cause::NO_MOTORS_DEFINED));
    EXPECT_EQ(cause("tumbleunderthrust"),
              std::optional<SimulationAbort::Cause>(SimulationAbort::Cause::TUMBLE_UNDER_THRUST));
    EXPECT_EQ(cause("NOMOTORSDEFINED"), std::nullopt);
    EXPECT_EQ(cause("no_motors_defined"), std::nullopt);

    EXPECT_EQ(style("solid"), std::optional<LineStyle>(LineStyle::SOLID));
    EXPECT_EQ(style("dashed"), std::optional<LineStyle>(LineStyle::DASHED));
    EXPECT_EQ(style("dotted"), std::optional<LineStyle>(LineStyle::DOTTED));
    EXPECT_EQ(style("dashdot"), std::optional<LineStyle>(LineStyle::DASHDOT));
    EXPECT_EQ(style("dash_dot"), std::nullopt);
    EXPECT_EQ(style("SOLID"), std::nullopt);
}

TEST(DocumentConfig, FindEnumTakesTheFirstOfTwoConstantsWithOneSpelling)
{
    // Two names that differ by an underscore are one name in a file: Java returns the first in
    // declaration order.
    struct Named
    {
        std::string_view name;
        int              value;
    };
    const std::vector<Named>   constants{{.name = "A_B", .value = 1}, {.name = "AB", .value = 2}};
    const std::optional<Named> found =
        DocumentConfig::findEnum("ab", constants, [](const Named& named) { return named.name; });
    EXPECT_EQ(found.has_value() ? found->value : 0, 1);
    EXPECT_FALSE(DocumentConfig::findEnum("ab", std::vector<Named>{}, [](const Named& named) {
                     return named.name;
                 }).has_value());
}

/// A constant whose Java name is built when it is asked for, as a name function may: a
/// std::string of its own, too long for the small-string buffer, so that reading it after it
/// is gone is reading freed memory (the asan preset tells).
[[nodiscard]] std::string builtName(int constant)
{
    return std::format("A_CONSTANT_WITH_A_LONG_NAME_{}", constant);
}

// findEnum() used to keep a view of the name function's result in a variable: a view of a
// temporary std::string, gone before it was compared.
TEST(DocumentConfig, FindEnumTakesANameFunctionThatReturnsAString)
{
    const std::vector<int> constants{1, 2, 3};
    EXPECT_EQ(DocumentConfig::findEnum("aconstantwithalongname2", constants, &builtName),
              std::optional<int>(2));
    EXPECT_EQ(DocumentConfig::findEnum(" aconstantwithalongname3 ", constants,
                                       [](int constant) { return builtName(constant); }),
              std::optional<int>(3));
    EXPECT_EQ(DocumentConfig::findEnum("aconstantwithalongname4", constants, &builtName),
              std::nullopt);
    // A C string is a name too.
    EXPECT_EQ(DocumentConfig::findEnum("fixed", constants,
                                       [](int /*constant*/) -> const char* { return "FIXED"; }),
              std::optional<int>(1));
}

/// stringToDouble() of @p text as Java prints the result, or "<code>: <message>".
[[nodiscard]] std::string number(std::optional<std::string_view> text)
{
    const Result<double> value = DocumentConfig::stringToDouble(text);
    if (!value)
    {
        return std::string(QtRocket::toString(value.error().code)) + ": " + value.error().message;
    }
    return QtRocket::Strings::javaDoubleToString(*value);
}

/// number() of a text that is there.
[[nodiscard]] std::string numberOf(std::string_view text)
{
    return number(text);
}

TEST(DocumentConfig, StringToDoubleReadsTheThreeWordsOfTheFileFormat)
{
    EXPECT_EQ(each({"NaN", "nan", "NAN", " NaN ", "-NaN", "+NaN"}, numberOf), Texts(6, "NaN"));
    EXPECT_EQ(each({"Inf", "inf", "INF", "Infinity", "+Infinity", "1e400"}, numberOf),
              Texts(6, "Infinity"));
    EXPECT_EQ(each({"-Inf", "-inf", "-Infinity", "-1e400"}, numberOf), Texts(4, "-Infinity"));
    // "Inf" is OpenRocket's own word, compared untrimmed and without a plus sign.
    EXPECT_EQ(number("+Inf"), R"(INVALID_ARGUMENT: For input string: "+Inf")");
    EXPECT_EQ(number(" Inf"), R"(INVALID_ARGUMENT: For input string: "Inf")");
    EXPECT_EQ(number("Inf "), R"(INVALID_ARGUMENT: For input string: "Inf")");
    EXPECT_EQ(number("infinity"), R"(INVALID_ARGUMENT: For input string: "infinity")");
}

TEST(DocumentConfig, StringToDoubleIsJavasParseDoubleOtherwise)
{
    EXPECT_EQ(number("1.5"), "1.5");
    EXPECT_EQ(number(" 1.5 "), "1.5");
    EXPECT_EQ(number("1e3"), "1000.0");
    EXPECT_EQ(number("-0"), "-0.0");
    EXPECT_EQ(number("0x1p3"), "8.0");
    EXPECT_EQ(number("1.5d"), "1.5");
    EXPECT_EQ(number("1.5f"), "1.5");
    EXPECT_EQ(number("5."), "5.0");
    EXPECT_EQ(number(".5"), "0.5");
    EXPECT_EQ(number("1e-400"), "0.0");
}

TEST(DocumentConfig, StringToDoubleFailsWithJavasMessage)
{
    EXPECT_EQ(number(std::nullopt), "INVALID_ARGUMENT: null string");
    EXPECT_EQ(number(""), "INVALID_ARGUMENT: empty String");
    EXPECT_EQ(number(" "), "INVALID_ARGUMENT: empty String");
    EXPECT_EQ(number("\t\n"), "INVALID_ARGUMENT: empty String");
    EXPECT_EQ(number("abc"), R"(INVALID_ARGUMENT: For input string: "abc")");
    // The message shows the text trimmed.
    EXPECT_EQ(number(" abc "), R"(INVALID_ARGUMENT: For input string: "abc")");
    EXPECT_EQ(number("1,5"), R"(INVALID_ARGUMENT: For input string: "1,5")");
    EXPECT_EQ(number("1.5 2"), R"(INVALID_ARGUMENT: For input string: "1.5 2")");
    EXPECT_EQ(number("."), R"(INVALID_ARGUMENT: For input string: ".")");
    EXPECT_EQ(number("+"), R"(INVALID_ARGUMENT: For input string: "+")");
    EXPECT_EQ(number("--1"), R"(INVALID_ARGUMENT: For input string: "--1")");
    EXPECT_EQ(number("1_0"), R"(INVALID_ARGUMENT: For input string: "1_0")");
    EXPECT_EQ(number("NaNx"), R"(INVALID_ARGUMENT: For input string: "NaNx")");
    // A no-break space is no white space to trim.
    EXPECT_EQ(number("\xC2\xA0"
                     "1"),
              "INVALID_ARGUMENT: For input string: \"\xC2\xA0"
              "1\"");
    EXPECT_EQ(DocumentConfig::stringToDouble("abc").error().code, ErrorCode::INVALID_ARGUMENT);
}

// FloatingDecimal.readJavaFormatString() has a third message: a second point among the digits
// and points the number starts with. Numbers.out: "D [1..2] NFE [multiple points]" and the
// lines after it; the same through DocumentConfig.stringToDouble (the review's sweep of 10558
// texts).
TEST(DocumentConfig, StringToDoubleFailsWithMultiplePointsAsJava)
{
    EXPECT_EQ(each({"1..2", "..", "-..", "+..", "0.0.0", "1.2.3e5", " 1..2 ", "1.2.", "00.0.",
                    "-.5.", "\t1..2\n", "1..2abc", "..e"},
                   numberOf),
              Texts(13, "INVALID_ARGUMENT: multiple points"));
    // Not when the number starts as a word or a hexadecimal number does, when something else
    // ends the digits first, or when there is one point only.
    EXPECT_EQ(
        each({"x..", "1e1..", "0x1..p1", "-0x..", "N..", "Infinity..", "1.2e3.4", "1.5d.", "."},
             numberOf),
        (Texts{R"(INVALID_ARGUMENT: For input string: "x..")",
               R"(INVALID_ARGUMENT: For input string: "1e1..")",
               R"(INVALID_ARGUMENT: For input string: "0x1..p1")",
               R"(INVALID_ARGUMENT: For input string: "-0x..")",
               R"(INVALID_ARGUMENT: For input string: "N..")",
               R"(INVALID_ARGUMENT: For input string: "Infinity..")",
               R"(INVALID_ARGUMENT: For input string: "1.2e3.4")",
               R"(INVALID_ARGUMENT: For input string: "1.5d.")",
               R"(INVALID_ARGUMENT: For input string: ".")"}));
}

/// parseDouble() of @p text as Java prints the result, or "<code>: <message>".
[[nodiscard]] std::string parsedDouble(std::string_view text)
{
    const Result<double> value = DocumentConfig::parseDouble(text);
    if (!value)
    {
        return std::string(QtRocket::toString(value.error().code)) + ": " + value.error().message;
    }
    return QtRocket::Strings::javaDoubleToString(*value);
}

// Double.parseDouble itself, for the handlers that let its exception fail the load
// (AppearanceHandler, WarningHandler, WindHandler). Numbers.out, the "D" lines.
TEST(DocumentConfig, ParseDoubleIsJavasWithItsMessages)
{
    EXPECT_EQ(each({"1.5", " 2.5 ", "-0.0", "1e3", "0x1p3", "1.5d", "NaN", "-Infinity", "Infinity",
                    "1e400", "1e-400", "0X.8P-3"},
                   parsedDouble),
              (Texts{"1.5", "2.5", "-0.0", "1000.0", "8.0", "1.5", "NaN", "-Infinity", "Infinity",
                     "Infinity", "0.0", "0.0625"}));
    // OpenRocket's own words are no numbers here: stringToDouble() reads them, this does not.
    EXPECT_EQ(each({"Inf", "-Inf", "inf", "nan"}, parsedDouble),
              (Texts{R"(INVALID_ARGUMENT: For input string: "Inf")",
                     R"(INVALID_ARGUMENT: For input string: "-Inf")",
                     R"(INVALID_ARGUMENT: For input string: "inf")",
                     R"(INVALID_ARGUMENT: For input string: "nan")"}));
    EXPECT_EQ(each({"", "  "}, parsedDouble), Texts(2, "INVALID_ARGUMENT: empty String"));
    EXPECT_EQ(each({"1..2", "..", "-..", "0.0.0", "1.2.3e5", " 1..2 "}, parsedDouble),
              Texts(6, "INVALID_ARGUMENT: multiple points"));
    EXPECT_EQ(
        each({"x", "x..", ".", "+", "-", "1.5x", "1,5", "1e", "--1", "0x", "I", "N"}, parsedDouble),
        (Texts{R"(INVALID_ARGUMENT: For input string: "x")",
               R"(INVALID_ARGUMENT: For input string: "x..")",
               R"(INVALID_ARGUMENT: For input string: ".")",
               R"(INVALID_ARGUMENT: For input string: "+")",
               R"(INVALID_ARGUMENT: For input string: "-")",
               R"(INVALID_ARGUMENT: For input string: "1.5x")",
               R"(INVALID_ARGUMENT: For input string: "1,5")",
               R"(INVALID_ARGUMENT: For input string: "1e")",
               R"(INVALID_ARGUMENT: For input string: "--1")",
               R"(INVALID_ARGUMENT: For input string: "0x")",
               R"(INVALID_ARGUMENT: For input string: "I")",
               R"(INVALID_ARGUMENT: For input string: "N")"}));
    // The message shows the text trimmed.
    EXPECT_EQ(parsedDouble(" 1.5x "), R"(INVALID_ARGUMENT: For input string: "1.5x")");
    EXPECT_EQ(DocumentConfig::parseDouble("x").error().code, ErrorCode::INVALID_ARGUMENT);
}

/// parseInt() of @p text, or "<code>: <message>".
[[nodiscard]] std::string parsedInt(std::optional<std::string_view> text)
{
    const Result<int> value = DocumentConfig::parseInt(text);
    if (!value)
    {
        return std::string(QtRocket::toString(value.error().code)) + ": " + value.error().message;
    }
    return std::to_string(*value);
}

/// parsedInt() of a text that is there.
[[nodiscard]] std::string parsedIntOf(std::string_view text)
{
    return parsedInt(text);
}

// Integer.parseInt itself (the stage number of a motor configuration, the colours of the photo
// settings). Numbers.out, the "I" lines.
TEST(DocumentConfig, ParseIntIsJavasWithItsMessages)
{
    EXPECT_EQ(each({"12", "-12", "+7", "007", "-0", "2147483647", "-2147483648"}, parsedIntOf),
              (Texts{"12", "-12", "7", "7", "0", "2147483647", "-2147483648"}));
    // The message shows the text as it is: nothing is trimmed, an empty text included.
    EXPECT_EQ(each({"", " 4", "4 ", "x", "+", "-", "1.0", "0x10", "1_000", "++1", "1e3",
                    "2147483648", "-2147483649", "99999999999999999999"},
                   parsedIntOf),
              (Texts{R"(INVALID_ARGUMENT: For input string: "")",
                     R"(INVALID_ARGUMENT: For input string: " 4")",
                     R"(INVALID_ARGUMENT: For input string: "4 ")",
                     R"(INVALID_ARGUMENT: For input string: "x")",
                     R"(INVALID_ARGUMENT: For input string: "+")",
                     R"(INVALID_ARGUMENT: For input string: "-")",
                     R"(INVALID_ARGUMENT: For input string: "1.0")",
                     R"(INVALID_ARGUMENT: For input string: "0x10")",
                     R"(INVALID_ARGUMENT: For input string: "1_000")",
                     R"(INVALID_ARGUMENT: For input string: "++1")",
                     R"(INVALID_ARGUMENT: For input string: "1e3")",
                     R"(INVALID_ARGUMENT: For input string: "2147483648")",
                     R"(INVALID_ARGUMENT: For input string: "-2147483649")",
                     R"(INVALID_ARGUMENT: For input string: "99999999999999999999")"}));
    // "I [<null>] NFE [Cannot parse null string]": a missing attribute.
    EXPECT_EQ(parsedInt(std::nullopt), "INVALID_ARGUMENT: Cannot parse null string");
    // Deviation: Java reads the decimal digits of every script ("I [fullwidth 12] value 12",
    // "I [Arabic-Indic 12] value 12"); here they are no digits.
    EXPECT_EQ(parsedInt("\xEF\xBC\x91\xEF\xBC\x92"),
              "INVALID_ARGUMENT: For input string: \"\xEF\xBC\x91\xEF\xBC\x92\"");
    EXPECT_EQ(parsedInt("\xD9\xA1\xD9\xA2"),
              "INVALID_ARGUMENT: For input string: \"\xD9\xA1\xD9\xA2\"");
}

/// "no" when parseFiniteDouble() gives @p text no value, else the value as Java prints it.
[[nodiscard]] std::string finite(std::string_view text)
{
    const std::optional<double> value = DocumentConfig::parseFiniteDouble(text);
    return value.has_value() ? QtRocket::Strings::javaDoubleToString(*value) : "no";
}

TEST(DocumentConfig, ParseFiniteDoubleRefusesWhatIsNoFiniteNumber)
{
    EXPECT_EQ(DocumentConfig::parseFiniteDouble("1.5"), std::optional<double>(1.5));
    EXPECT_EQ(DocumentConfig::parseFiniteDouble(" -2.5e3 "), std::optional<double>(-2500.0));
    EXPECT_EQ(DocumentConfig::parseFiniteDouble("0"), std::optional<double>(0.0));
    EXPECT_EQ(DocumentConfig::parseFiniteDouble("0x1p3"), std::optional<double>(8.0));
    EXPECT_EQ(DocumentConfig::parseFiniteDouble("1.7976931348623157E308"),
              std::optional<double>(std::numeric_limits<double>::max()));
    // Everything OpenRocket would store and a simulation could not run with.
    EXPECT_EQ(each({"NaN", "Infinity", "-Infinity", "+Infinity", "1e400", "-1e400", "abc", "", " ",
                    "Inf", "1,5", "none"},
                   finite),
              Texts(12, "no"));
    // Where stringToDouble() gives the infinity, this gives nothing.
    EXPECT_EQ(DocumentConfig::stringToDouble("Infinity").value(), kInfinity);
    EXPECT_TRUE(std::isnan(DocumentConfig::stringToDouble("NaN").value()));
}

}  // namespace
