#include "QtRocket/file/openrocket/DocumentConfig.h"

#include <cmath>
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
// part D4).

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
