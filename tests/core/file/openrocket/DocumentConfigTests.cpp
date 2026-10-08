#include "QtRocket/file/openrocket/DocumentConfig.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/openrocket/AnglePositionSetter.h"
#include "QtRocket/file/openrocket/AxialPositionSetter.h"
#include "QtRocket/file/openrocket/BooleanSetter.h"
#include "QtRocket/file/openrocket/ClusterConfigurationSetter.h"
#include "QtRocket/file/openrocket/ColorSetter.h"
#include "QtRocket/file/openrocket/ComponentPresetSetter.h"
#include "QtRocket/file/openrocket/DoubleSetter.h"
#include "QtRocket/file/openrocket/EnumSetter.h"
#include "QtRocket/file/openrocket/FinTabPositionSetter.h"
#include "QtRocket/file/openrocket/IntSetter.h"
#include "QtRocket/file/openrocket/MaterialSetter.h"
#include "QtRocket/file/openrocket/OverrideSetter.h"
#include "QtRocket/file/openrocket/RadiusPositionSetter.h"
#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/file/openrocket/StringSetter.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"
#include "file/openrocket/SetterTestSupport.h"

// The expectations are what OpenRocket's DocumentConfig answers (probe DocumentConfigProbe of
// part D4), and for parseDouble() and parseInt() what the JDK's Double.parseDouble and
// Integer.parseInt answer (probe Numbers of the review fixes, probes/tier9a-fix-document/out/
// Numbers.out, JDK 17). The tests of the component table and the setter table, at the end, say
// where theirs come from.

namespace
{

using QtRocket::BugError;
using QtRocket::ComponentKind;
using QtRocket::DocumentConfig;
using QtRocket::ElementHandler;
using QtRocket::ErrorCode;
using QtRocket::LineStyle;
using QtRocket::Result;
using QtRocket::RocketComponent;
using QtRocket::Setter;
using QtRocket::Simulation;
using QtRocket::SimulationAbort;
using QtRocket::WarningSet;
using QtRocket::Test::SetterFixture;

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

// ===================================================== the component and the setter table
//
// The tables below are OpenRocket's own: what the probe SetterProbe of tier 9b, part R1 prints
// of DocumentConfig.constructors and DocumentConfig.setters (--keys) and of the search of
// ComponentParameterHandler.closeElement() for every component class and every element name
// (--walk). scripts/make_registry_tables.py of the probe writes them.

/// One entry of OpenRocket's constructor table.
struct JavaConstructor
{
    std::string_view element;
    std::string_view className;
};

/// One entry of OpenRocket's setter table that has a setter.
struct JavaSetter
{
    std::string_view key;
    std::string_view setterClass;
};

/// What OpenRocket's search finds for a component class: "found <n> refused <m> :" and then
/// " <element>=<class of the entry>" for a setter and " <element>=!<class>" for a null entry,
/// by element name.
struct JavaWalk
{
    ComponentKind    kind;
    std::string_view entries;
};

// "constructor <element> <class>" of SetterProbe --keys.
constexpr auto kJavaConstructors = std::to_array<JavaConstructor>({
    {.element = "bodytube", .className = "BodyTube"},
    {.element = "boosterset", .className = "ParallelStage"},
    {.element = "bulkhead", .className = "Bulkhead"},
    {.element = "centeringring", .className = "CenteringRing"},
    {.element = "ellipticalfinset", .className = "EllipticalFinSet"},
    {.element = "engineblock", .className = "EngineBlock"},
    {.element = "freeformfinset", .className = "FreeformFinSet"},
    {.element = "innertube", .className = "InnerTube"},
    {.element = "launchlug", .className = "LaunchLug"},
    {.element = "masscomponent", .className = "MassComponent"},
    {.element = "nosecone", .className = "NoseCone"},
    {.element = "parachute", .className = "Parachute"},
    {.element = "parallelstage", .className = "ParallelStage"},
    {.element = "podset", .className = "PodSet"},
    {.element = "railbutton", .className = "RailButton"},
    {.element = "shockcord", .className = "ShockCord"},
    {.element = "stage", .className = "AxialStage"},
    {.element = "streamer", .className = "Streamer"},
    {.element = "transition", .className = "Transition"},
    {.element = "trapezoidfinset", .className = "TrapezoidFinSet"},
    {.element = "tubecoupler", .className = "TubeCoupler"},
    {.element = "tubefinset", .className = "TubeFinSet"},
});

// "setter <key> <setter class>" of SetterProbe --keys, the null entries left out.
constexpr auto kJavaSetters = std::to_array<JavaSetter>({
    {.key = "AxialStage:separationaltitude", .setterClass = "DoubleSetter"},
    {.key = "AxialStage:separationdelay", .setterClass = "DoubleSetter"},
    {.key = "AxialStage:separationevent", .setterClass = "EnumSetter"},
    {.key = "BodyComponent:length", .setterClass = "DoubleSetter"},
    {.key = "BodyTube:radius", .setterClass = "DoubleSetter"},
    {.key = "Bulkhead:outerradius", .setterClass = "DoubleSetter"},
    {.key = "CenteringRing:innerradius", .setterClass = "DoubleSetter"},
    {.key = "CenteringRing:outerradius", .setterClass = "DoubleSetter"},
    {.key = "EllipticalFinSet:height", .setterClass = "DoubleSetter"},
    {.key = "EllipticalFinSet:rootchord", .setterClass = "DoubleSetter"},
    {.key = "EngineBlock:outerradius", .setterClass = "DoubleSetter"},
    {.key = "ExternalComponent:finish", .setterClass = "EnumSetter"},
    {.key = "ExternalComponent:material", .setterClass = "MaterialSetter"},
    {.key = "FinSet:angleoffset", .setterClass = "AnglePositionSetter"},
    {.key = "FinSet:cant", .setterClass = "DoubleSetter"},
    {.key = "FinSet:crosssection", .setterClass = "EnumSetter"},
    {.key = "FinSet:filletmaterial", .setterClass = "MaterialSetter"},
    {.key = "FinSet:filletradius", .setterClass = "DoubleSetter"},
    {.key = "FinSet:fincount", .setterClass = "IntSetter"},
    {.key = "FinSet:instancecount", .setterClass = "IntSetter"},
    {.key = "FinSet:radiusoffset", .setterClass = "RadiusPositionSetter"},
    {.key = "FinSet:rotation", .setterClass = "DoubleSetter"},
    {.key = "FinSet:tabheight", .setterClass = "DoubleSetter"},
    {.key = "FinSet:tablength", .setterClass = "DoubleSetter"},
    {.key = "FinSet:tabposition", .setterClass = "FinTabPositionSetter"},
    {.key = "FinSet:thickness", .setterClass = "DoubleSetter"},
    {.key = "InnerTube:clusterconfiguration", .setterClass = "ClusterConfigurationSetter"},
    {.key = "InnerTube:clusterrotation", .setterClass = "DoubleSetter"},
    {.key = "InnerTube:clusterscale", .setterClass = "DoubleSetter"},
    {.key = "InnerTube:outerradius", .setterClass = "DoubleSetter"},
    {.key = "LaunchLug:angleoffset", .setterClass = "AnglePositionSetter"},
    {.key = "LaunchLug:instancecount", .setterClass = "IntSetter"},
    {.key = "LaunchLug:instanceseparation", .setterClass = "DoubleSetter"},
    {.key = "LaunchLug:length", .setterClass = "DoubleSetter"},
    {.key = "LaunchLug:radialdirection", .setterClass = "DoubleSetter"},
    {.key = "LaunchLug:radius", .setterClass = "DoubleSetter"},
    {.key = "LaunchLug:thickness", .setterClass = "DoubleSetter"},
    {.key = "MassComponent:mass", .setterClass = "DoubleSetter"},
    {.key = "MassComponent:masscomponenttype", .setterClass = "EnumSetter"},
    {.key = "MassObject:packedlength", .setterClass = "DoubleSetter"},
    {.key = "MassObject:packedradius", .setterClass = "DoubleSetter"},
    {.key = "MassObject:radialdirection", .setterClass = "DoubleSetter"},
    {.key = "MassObject:radialposition", .setterClass = "DoubleSetter"},
    {.key = "NoseCone:isflipped", .setterClass = "BooleanSetter"},
    {.key = "Parachute:diameter", .setterClass = "DoubleSetter"},
    {.key = "Parachute:linecount", .setterClass = "IntSetter"},
    {.key = "Parachute:linelength", .setterClass = "DoubleSetter"},
    {.key = "Parachute:linematerial", .setterClass = "MaterialSetter"},
    {.key = "Parachute:preset", .setterClass = "ComponentPresetSetter"},
    {.key = "ParallelStage:angleoffset", .setterClass = "AnglePositionSetter"},
    {.key = "ParallelStage:instancecount", .setterClass = "IntSetter"},
    {.key = "ParallelStage:radiusoffset", .setterClass = "RadiusPositionSetter"},
    {.key = "PodSet:angleoffset", .setterClass = "AnglePositionSetter"},
    {.key = "PodSet:instancecount", .setterClass = "IntSetter"},
    {.key = "PodSet:radiusoffset", .setterClass = "RadiusPositionSetter"},
    {.key = "RadiusRingComponent:innerradius", .setterClass = "DoubleSetter"},
    {.key = "RadiusRingComponent:instancecount", .setterClass = "IntSetter"},
    {.key = "RadiusRingComponent:instanceseparation", .setterClass = "DoubleSetter"},
    {.key = "RailButton:angleoffset", .setterClass = "AnglePositionSetter"},
    {.key = "RailButton:baseheight", .setterClass = "DoubleSetter"},
    {.key = "RailButton:flangeheight", .setterClass = "DoubleSetter"},
    {.key = "RailButton:height", .setterClass = "DoubleSetter"},
    {.key = "RailButton:innerdiameter", .setterClass = "DoubleSetter"},
    {.key = "RailButton:instancecount", .setterClass = "IntSetter"},
    {.key = "RailButton:instanceseparation", .setterClass = "DoubleSetter"},
    {.key = "RailButton:outerdiameter", .setterClass = "DoubleSetter"},
    {.key = "RailButton:screwheight", .setterClass = "DoubleSetter"},
    {.key = "RecoveryDevice:cd", .setterClass = "DoubleSetter"},
    {.key = "RecoveryDevice:deployaltitude", .setterClass = "DoubleSetter"},
    {.key = "RecoveryDevice:deploydelay", .setterClass = "DoubleSetter"},
    {.key = "RecoveryDevice:deployevent", .setterClass = "EnumSetter"},
    {.key = "RecoveryDevice:isdrogue", .setterClass = "BooleanSetter"},
    {.key = "RecoveryDevice:material", .setterClass = "MaterialSetter"},
    {.key = "RingComponent:length", .setterClass = "DoubleSetter"},
    {.key = "RingComponent:radialdirection", .setterClass = "DoubleSetter"},
    {.key = "RingComponent:radialposition", .setterClass = "DoubleSetter"},
    {.key = "Rocket:customreference", .setterClass = "DoubleSetter"},
    {.key = "Rocket:designer", .setterClass = "StringSetter"},
    {.key = "Rocket:designtype", .setterClass = "EnumSetter"},
    {.key = "Rocket:kitname", .setterClass = "StringSetter"},
    {.key = "Rocket:referencetype", .setterClass = "EnumSetter"},
    {.key = "Rocket:revision", .setterClass = "StringSetter"},
    {.key = "RocketComponent:axialoffset", .setterClass = "AxialPositionSetter"},
    {.key = "RocketComponent:color", .setterClass = "ColorSetter"},
    {.key = "RocketComponent:comment", .setterClass = "StringSetter"},
    {.key = "RocketComponent:id", .setterClass = "StringSetter"},
    {.key = "RocketComponent:linestyle", .setterClass = "EnumSetter"},
    {.key = "RocketComponent:name", .setterClass = "StringSetter"},
    {.key = "RocketComponent:overridecd", .setterClass = "OverrideSetter"},
    {.key = "RocketComponent:overridecg", .setterClass = "OverrideSetter"},
    {.key = "RocketComponent:overridemass", .setterClass = "OverrideSetter"},
    {.key = "RocketComponent:overridesubcomponents", .setterClass = "BooleanSetter"},
    {.key = "RocketComponent:overridesubcomponentscd", .setterClass = "BooleanSetter"},
    {.key = "RocketComponent:overridesubcomponentscg", .setterClass = "BooleanSetter"},
    {.key = "RocketComponent:overridesubcomponentsmass", .setterClass = "BooleanSetter"},
    {.key = "RocketComponent:position", .setterClass = "AxialPositionSetter"},
    {.key = "RocketComponent:preset", .setterClass = "ComponentPresetSetter"},
    {.key = "ShockCord:cordlength", .setterClass = "DoubleSetter"},
    {.key = "ShockCord:material", .setterClass = "MaterialSetter"},
    {.key = "Streamer:striplength", .setterClass = "DoubleSetter"},
    {.key = "Streamer:stripwidth", .setterClass = "DoubleSetter"},
    {.key = "StructuralComponent:material", .setterClass = "MaterialSetter"},
    {.key = "SymmetricComponent:thickness", .setterClass = "DoubleSetter"},
    {.key = "ThicknessRingComponent:thickness", .setterClass = "DoubleSetter"},
    {.key = "Transition:aftradius", .setterClass = "DoubleSetter"},
    {.key = "Transition:aftshouldercapped", .setterClass = "BooleanSetter"},
    {.key = "Transition:aftshoulderlength", .setterClass = "DoubleSetter"},
    {.key = "Transition:aftshoulderradius", .setterClass = "DoubleSetter"},
    {.key = "Transition:aftshoulderthickness", .setterClass = "DoubleSetter"},
    {.key = "Transition:foreradius", .setterClass = "DoubleSetter"},
    {.key = "Transition:foreshouldercapped", .setterClass = "BooleanSetter"},
    {.key = "Transition:foreshoulderlength", .setterClass = "DoubleSetter"},
    {.key = "Transition:foreshoulderradius", .setterClass = "DoubleSetter"},
    {.key = "Transition:foreshoulderthickness", .setterClass = "DoubleSetter"},
    {.key = "Transition:shape", .setterClass = "EnumSetter"},
    {.key = "Transition:shapeclipped", .setterClass = "BooleanSetter"},
    {.key = "Transition:shapeparameter", .setterClass = "DoubleSetter"},
    {.key = "TrapezoidFinSet:height", .setterClass = "DoubleSetter"},
    {.key = "TrapezoidFinSet:rootchord", .setterClass = "DoubleSetter"},
    {.key = "TrapezoidFinSet:sweeplength", .setterClass = "DoubleSetter"},
    {.key = "TrapezoidFinSet:tipchord", .setterClass = "DoubleSetter"},
    {.key = "TubeCoupler:outerradius", .setterClass = "DoubleSetter"},
    {.key = "TubeFinSet:angleoffset", .setterClass = "AnglePositionSetter"},
    {.key = "TubeFinSet:fincount", .setterClass = "IntSetter"},
    {.key = "TubeFinSet:instancecount", .setterClass = "IntSetter"},
    {.key = "TubeFinSet:length", .setterClass = "DoubleSetter"},
    {.key = "TubeFinSet:radius", .setterClass = "DoubleSetter"},
    {.key = "TubeFinSet:radiusoffset", .setterClass = "RadiusPositionSetter"},
    {.key = "TubeFinSet:rotation", .setterClass = "DoubleSetter"},
    {.key = "TubeFinSet:thickness", .setterClass = "DoubleSetter"},
});

// The null entries of SetterProbe --keys.
constexpr auto kJavaRefused = std::to_array<std::string_view>({
    "NoseCone:foreradius",
    "NoseCone:foreshouldercapped",
    "NoseCone:foreshoulderlength",
    "NoseCone:foreshoulderradius",
    "NoseCone:foreshoulderthickness",
});

// "walk <class> found <n> refused <m> : <element>=<owner> ..." of SetterProbe --walk.
constexpr auto kJavaWalks = std::to_array<JavaWalk>({
    {.kind = ComponentKind::ROCKET,
     .entries =
         "found 21 refused 0 : axialoffset=RocketComponent color=RocketComponent "
         "comment=RocketComponent customreference=Rocket designer=Rocket designtype=Rocket "
         "id=RocketComponent kitname=Rocket linestyle=RocketComponent name=RocketComponent "
         "overridecd=RocketComponent overridecg=RocketComponent overridemass=RocketComponent "
         "overridesubcomponents=RocketComponent overridesubcomponentscd=RocketComponent "
         "overridesubcomponentscg=RocketComponent overridesubcomponentsmass=RocketComponent "
         "position=RocketComponent preset=RocketComponent referencetype=Rocket revision=Rocket"},
    {.kind    = ComponentKind::AXIAL_STAGE,
     .entries = "found 18 refused 0 : axialoffset=RocketComponent color=RocketComponent "
                "comment=RocketComponent id=RocketComponent linestyle=RocketComponent "
                "name=RocketComponent overridecd=RocketComponent overridecg=RocketComponent "
                "overridemass=RocketComponent overridesubcomponents=RocketComponent "
                "overridesubcomponentscd=RocketComponent overridesubcomponentscg=RocketComponent "
                "overridesubcomponentsmass=RocketComponent position=RocketComponent "
                "preset=RocketComponent separationaltitude=AxialStage separationdelay=AxialStage "
                "separationevent=AxialStage"},
    {.kind = ComponentKind::PARALLEL_STAGE,
     .entries =
         "found 21 refused 0 : angleoffset=ParallelStage axialoffset=RocketComponent "
         "color=RocketComponent comment=RocketComponent id=RocketComponent "
         "instancecount=ParallelStage linestyle=RocketComponent name=RocketComponent "
         "overridecd=RocketComponent overridecg=RocketComponent overridemass=RocketComponent "
         "overridesubcomponents=RocketComponent overridesubcomponentscd=RocketComponent "
         "overridesubcomponentscg=RocketComponent overridesubcomponentsmass=RocketComponent "
         "position=RocketComponent preset=RocketComponent radiusoffset=ParallelStage "
         "separationaltitude=AxialStage separationdelay=AxialStage separationevent=AxialStage"},
    {.kind = ComponentKind::POD_SET,
     .entries =
         "found 18 refused 0 : angleoffset=PodSet axialoffset=RocketComponent "
         "color=RocketComponent comment=RocketComponent id=RocketComponent instancecount=PodSet "
         "linestyle=RocketComponent name=RocketComponent overridecd=RocketComponent "
         "overridecg=RocketComponent overridemass=RocketComponent "
         "overridesubcomponents=RocketComponent overridesubcomponentscd=RocketComponent "
         "overridesubcomponentscg=RocketComponent overridesubcomponentsmass=RocketComponent "
         "position=RocketComponent preset=RocketComponent radiusoffset=PodSet"},
    {.kind    = ComponentKind::BODY_TUBE,
     .entries = "found 20 refused 0 : axialoffset=RocketComponent color=RocketComponent "
                "comment=RocketComponent finish=ExternalComponent id=RocketComponent "
                "length=BodyComponent linestyle=RocketComponent material=ExternalComponent "
                "name=RocketComponent overridecd=RocketComponent overridecg=RocketComponent "
                "overridemass=RocketComponent overridesubcomponents=RocketComponent "
                "overridesubcomponentscd=RocketComponent overridesubcomponentscg=RocketComponent "
                "overridesubcomponentsmass=RocketComponent position=RocketComponent "
                "preset=RocketComponent radius=BodyTube thickness=SymmetricComponent"},
    {.kind    = ComponentKind::TRANSITION,
     .entries = "found 32 refused 0 : aftradius=Transition aftshouldercapped=Transition "
                "aftshoulderlength=Transition aftshoulderradius=Transition "
                "aftshoulderthickness=Transition axialoffset=RocketComponent color=RocketComponent "
                "comment=RocketComponent finish=ExternalComponent foreradius=Transition "
                "foreshouldercapped=Transition foreshoulderlength=Transition "
                "foreshoulderradius=Transition foreshoulderthickness=Transition id=RocketComponent "
                "length=BodyComponent linestyle=RocketComponent material=ExternalComponent "
                "name=RocketComponent overridecd=RocketComponent overridecg=RocketComponent "
                "overridemass=RocketComponent overridesubcomponents=RocketComponent "
                "overridesubcomponentscd=RocketComponent overridesubcomponentscg=RocketComponent "
                "overridesubcomponentsmass=RocketComponent position=RocketComponent "
                "preset=RocketComponent shape=Transition shapeclipped=Transition "
                "shapeparameter=Transition thickness=SymmetricComponent"},
    {.kind    = ComponentKind::NOSE_CONE,
     .entries = "found 28 refused 5 : aftradius=Transition aftshouldercapped=Transition "
                "aftshoulderlength=Transition aftshoulderradius=Transition "
                "aftshoulderthickness=Transition axialoffset=RocketComponent color=RocketComponent "
                "comment=RocketComponent finish=ExternalComponent foreradius=!NoseCone "
                "foreshouldercapped=!NoseCone foreshoulderlength=!NoseCone "
                "foreshoulderradius=!NoseCone foreshoulderthickness=!NoseCone id=RocketComponent "
                "isflipped=NoseCone length=BodyComponent linestyle=RocketComponent "
                "material=ExternalComponent name=RocketComponent overridecd=RocketComponent "
                "overridecg=RocketComponent overridemass=RocketComponent "
                "overridesubcomponents=RocketComponent overridesubcomponentscd=RocketComponent "
                "overridesubcomponentscg=RocketComponent overridesubcomponentsmass=RocketComponent "
                "position=RocketComponent preset=RocketComponent shape=Transition "
                "shapeclipped=Transition shapeparameter=Transition thickness=SymmetricComponent"},
    {.kind = ComponentKind::TRAPEZOID_FIN_SET,
     .entries =
         "found 34 refused 0 : angleoffset=FinSet axialoffset=RocketComponent cant=FinSet "
         "color=RocketComponent comment=RocketComponent crosssection=FinSet filletmaterial=FinSet "
         "filletradius=FinSet fincount=FinSet finish=ExternalComponent height=TrapezoidFinSet "
         "id=RocketComponent instancecount=FinSet linestyle=RocketComponent "
         "material=ExternalComponent name=RocketComponent overridecd=RocketComponent "
         "overridecg=RocketComponent overridemass=RocketComponent "
         "overridesubcomponents=RocketComponent overridesubcomponentscd=RocketComponent "
         "overridesubcomponentscg=RocketComponent overridesubcomponentsmass=RocketComponent "
         "position=RocketComponent preset=RocketComponent radiusoffset=FinSet "
         "rootchord=TrapezoidFinSet rotation=FinSet sweeplength=TrapezoidFinSet tabheight=FinSet "
         "tablength=FinSet tabposition=FinSet thickness=FinSet tipchord=TrapezoidFinSet"},
    {.kind = ComponentKind::ELLIPTICAL_FIN_SET,
     .entries =
         "found 32 refused 0 : angleoffset=FinSet axialoffset=RocketComponent cant=FinSet "
         "color=RocketComponent comment=RocketComponent crosssection=FinSet filletmaterial=FinSet "
         "filletradius=FinSet fincount=FinSet finish=ExternalComponent height=EllipticalFinSet "
         "id=RocketComponent instancecount=FinSet linestyle=RocketComponent "
         "material=ExternalComponent name=RocketComponent overridecd=RocketComponent "
         "overridecg=RocketComponent overridemass=RocketComponent "
         "overridesubcomponents=RocketComponent overridesubcomponentscd=RocketComponent "
         "overridesubcomponentscg=RocketComponent overridesubcomponentsmass=RocketComponent "
         "position=RocketComponent preset=RocketComponent radiusoffset=FinSet "
         "rootchord=EllipticalFinSet rotation=FinSet tabheight=FinSet tablength=FinSet "
         "tabposition=FinSet thickness=FinSet"},
    {.kind = ComponentKind::FREEFORM_FIN_SET,
     .entries =
         "found 30 refused 0 : angleoffset=FinSet axialoffset=RocketComponent cant=FinSet "
         "color=RocketComponent comment=RocketComponent crosssection=FinSet filletmaterial=FinSet "
         "filletradius=FinSet fincount=FinSet finish=ExternalComponent id=RocketComponent "
         "instancecount=FinSet linestyle=RocketComponent material=ExternalComponent "
         "name=RocketComponent overridecd=RocketComponent overridecg=RocketComponent "
         "overridemass=RocketComponent overridesubcomponents=RocketComponent "
         "overridesubcomponentscd=RocketComponent overridesubcomponentscg=RocketComponent "
         "overridesubcomponentsmass=RocketComponent position=RocketComponent "
         "preset=RocketComponent radiusoffset=FinSet rotation=FinSet tabheight=FinSet "
         "tablength=FinSet tabposition=FinSet thickness=FinSet"},
    {.kind = ComponentKind::TUBE_FIN_SET,
     .entries =
         "found 25 refused 0 : angleoffset=TubeFinSet axialoffset=RocketComponent "
         "color=RocketComponent comment=RocketComponent fincount=TubeFinSet "
         "finish=ExternalComponent id=RocketComponent instancecount=TubeFinSet length=TubeFinSet "
         "linestyle=RocketComponent material=ExternalComponent name=RocketComponent "
         "overridecd=RocketComponent overridecg=RocketComponent overridemass=RocketComponent "
         "overridesubcomponents=RocketComponent overridesubcomponentscd=RocketComponent "
         "overridesubcomponentscg=RocketComponent overridesubcomponentsmass=RocketComponent "
         "position=RocketComponent preset=RocketComponent radius=TubeFinSet "
         "radiusoffset=TubeFinSet rotation=TubeFinSet thickness=TubeFinSet"},
    {.kind = ComponentKind::LAUNCH_LUG,
     .entries =
         "found 24 refused 0 : angleoffset=LaunchLug axialoffset=RocketComponent "
         "color=RocketComponent comment=RocketComponent finish=ExternalComponent "
         "id=RocketComponent instancecount=LaunchLug instanceseparation=LaunchLug length=LaunchLug "
         "linestyle=RocketComponent material=ExternalComponent name=RocketComponent "
         "overridecd=RocketComponent overridecg=RocketComponent overridemass=RocketComponent "
         "overridesubcomponents=RocketComponent overridesubcomponentscd=RocketComponent "
         "overridesubcomponentscg=RocketComponent overridesubcomponentsmass=RocketComponent "
         "position=RocketComponent preset=RocketComponent radialdirection=LaunchLug "
         "radius=LaunchLug thickness=LaunchLug"},
    {.kind = ComponentKind::RAIL_BUTTON,
     .entries =
         "found 26 refused 0 : angleoffset=RailButton axialoffset=RocketComponent "
         "baseheight=RailButton color=RocketComponent comment=RocketComponent "
         "finish=ExternalComponent flangeheight=RailButton height=RailButton id=RocketComponent "
         "innerdiameter=RailButton instancecount=RailButton instanceseparation=RailButton "
         "linestyle=RocketComponent material=ExternalComponent name=RocketComponent "
         "outerdiameter=RailButton overridecd=RocketComponent overridecg=RocketComponent "
         "overridemass=RocketComponent overridesubcomponents=RocketComponent "
         "overridesubcomponentscd=RocketComponent overridesubcomponentscg=RocketComponent "
         "overridesubcomponentsmass=RocketComponent position=RocketComponent "
         "preset=RocketComponent screwheight=RailButton"},
    {.kind    = ComponentKind::INNER_TUBE,
     .entries = "found 24 refused 0 : axialoffset=RocketComponent clusterconfiguration=InnerTube "
                "clusterrotation=InnerTube clusterscale=InnerTube color=RocketComponent "
                "comment=RocketComponent id=RocketComponent length=RingComponent "
                "linestyle=RocketComponent material=StructuralComponent name=RocketComponent "
                "outerradius=InnerTube overridecd=RocketComponent overridecg=RocketComponent "
                "overridemass=RocketComponent overridesubcomponents=RocketComponent "
                "overridesubcomponentscd=RocketComponent overridesubcomponentscg=RocketComponent "
                "overridesubcomponentsmass=RocketComponent position=RocketComponent "
                "preset=RocketComponent radialdirection=RingComponent radialposition=RingComponent "
                "thickness=ThicknessRingComponent"},
    {.kind    = ComponentKind::TUBE_COUPLER,
     .entries = "found 21 refused 0 : axialoffset=RocketComponent color=RocketComponent "
                "comment=RocketComponent id=RocketComponent length=RingComponent "
                "linestyle=RocketComponent material=StructuralComponent name=RocketComponent "
                "outerradius=TubeCoupler overridecd=RocketComponent overridecg=RocketComponent "
                "overridemass=RocketComponent overridesubcomponents=RocketComponent "
                "overridesubcomponentscd=RocketComponent overridesubcomponentscg=RocketComponent "
                "overridesubcomponentsmass=RocketComponent position=RocketComponent "
                "preset=RocketComponent radialdirection=RingComponent radialposition=RingComponent "
                "thickness=ThicknessRingComponent"},
    {.kind    = ComponentKind::ENGINE_BLOCK,
     .entries = "found 21 refused 0 : axialoffset=RocketComponent color=RocketComponent "
                "comment=RocketComponent id=RocketComponent length=RingComponent "
                "linestyle=RocketComponent material=StructuralComponent name=RocketComponent "
                "outerradius=EngineBlock overridecd=RocketComponent overridecg=RocketComponent "
                "overridemass=RocketComponent overridesubcomponents=RocketComponent "
                "overridesubcomponentscd=RocketComponent overridesubcomponentscg=RocketComponent "
                "overridesubcomponentsmass=RocketComponent position=RocketComponent "
                "preset=RocketComponent radialdirection=RingComponent radialposition=RingComponent "
                "thickness=ThicknessRingComponent"},
    {.kind    = ComponentKind::CENTERING_RING,
     .entries = "found 23 refused 0 : axialoffset=RocketComponent color=RocketComponent "
                "comment=RocketComponent id=RocketComponent innerradius=CenteringRing "
                "instancecount=RadiusRingComponent instanceseparation=RadiusRingComponent "
                "length=RingComponent linestyle=RocketComponent material=StructuralComponent "
                "name=RocketComponent outerradius=CenteringRing overridecd=RocketComponent "
                "overridecg=RocketComponent overridemass=RocketComponent "
                "overridesubcomponents=RocketComponent overridesubcomponentscd=RocketComponent "
                "overridesubcomponentscg=RocketComponent overridesubcomponentsmass=RocketComponent "
                "position=RocketComponent preset=RocketComponent radialdirection=RingComponent "
                "radialposition=RingComponent"},
    {.kind    = ComponentKind::BULKHEAD,
     .entries = "found 23 refused 0 : axialoffset=RocketComponent color=RocketComponent "
                "comment=RocketComponent id=RocketComponent innerradius=RadiusRingComponent "
                "instancecount=RadiusRingComponent instanceseparation=RadiusRingComponent "
                "length=RingComponent linestyle=RocketComponent material=StructuralComponent "
                "name=RocketComponent outerradius=Bulkhead overridecd=RocketComponent "
                "overridecg=RocketComponent overridemass=RocketComponent "
                "overridesubcomponents=RocketComponent overridesubcomponentscd=RocketComponent "
                "overridesubcomponentscg=RocketComponent overridesubcomponentsmass=RocketComponent "
                "position=RocketComponent preset=RocketComponent radialdirection=RingComponent "
                "radialposition=RingComponent"},
    {.kind = ComponentKind::MASS_COMPONENT,
     .entries =
         "found 21 refused 0 : axialoffset=RocketComponent color=RocketComponent "
         "comment=RocketComponent id=RocketComponent linestyle=RocketComponent mass=MassComponent "
         "masscomponenttype=MassComponent name=RocketComponent overridecd=RocketComponent "
         "overridecg=RocketComponent overridemass=RocketComponent "
         "overridesubcomponents=RocketComponent overridesubcomponentscd=RocketComponent "
         "overridesubcomponentscg=RocketComponent overridesubcomponentsmass=RocketComponent "
         "packedlength=MassObject packedradius=MassObject position=RocketComponent "
         "preset=RocketComponent radialdirection=MassObject radialposition=MassObject"},
    {.kind = ComponentKind::SHOCK_CORD,
     .entries =
         "found 21 refused 0 : axialoffset=RocketComponent color=RocketComponent "
         "comment=RocketComponent cordlength=ShockCord id=RocketComponent "
         "linestyle=RocketComponent material=ShockCord name=RocketComponent "
         "overridecd=RocketComponent overridecg=RocketComponent overridemass=RocketComponent "
         "overridesubcomponents=RocketComponent overridesubcomponentscd=RocketComponent "
         "overridesubcomponentscg=RocketComponent overridesubcomponentsmass=RocketComponent "
         "packedlength=MassObject packedradius=MassObject position=RocketComponent "
         "preset=RocketComponent radialdirection=MassObject radialposition=MassObject"},
    {.kind = ComponentKind::PARACHUTE,
     .entries =
         "found 29 refused 0 : axialoffset=RocketComponent cd=RecoveryDevice color=RocketComponent "
         "comment=RocketComponent deployaltitude=RecoveryDevice deploydelay=RecoveryDevice "
         "deployevent=RecoveryDevice diameter=Parachute id=RocketComponent isdrogue=RecoveryDevice "
         "linecount=Parachute linelength=Parachute linematerial=Parachute "
         "linestyle=RocketComponent material=RecoveryDevice name=RocketComponent "
         "overridecd=RocketComponent overridecg=RocketComponent overridemass=RocketComponent "
         "overridesubcomponents=RocketComponent overridesubcomponentscd=RocketComponent "
         "overridesubcomponentscg=RocketComponent overridesubcomponentsmass=RocketComponent "
         "packedlength=MassObject packedradius=MassObject position=RocketComponent "
         "preset=Parachute radialdirection=MassObject radialposition=MassObject"},
    {.kind = ComponentKind::STREAMER,
     .entries =
         "found 27 refused 0 : axialoffset=RocketComponent cd=RecoveryDevice color=RocketComponent "
         "comment=RocketComponent deployaltitude=RecoveryDevice deploydelay=RecoveryDevice "
         "deployevent=RecoveryDevice id=RocketComponent isdrogue=RecoveryDevice "
         "linestyle=RocketComponent material=RecoveryDevice name=RocketComponent "
         "overridecd=RocketComponent overridecg=RocketComponent overridemass=RocketComponent "
         "overridesubcomponents=RocketComponent overridesubcomponentscd=RocketComponent "
         "overridesubcomponentscg=RocketComponent overridesubcomponentsmass=RocketComponent "
         "packedlength=MassObject packedradius=MassObject position=RocketComponent "
         "preset=RocketComponent radialdirection=MassObject radialposition=MassObject "
         "striplength=Streamer stripwidth=Streamer"},
});
/// The Java class of the component createComponent() makes for @p element, or "null".
[[nodiscard]] std::string madeClass(std::string_view element)
{
    const std::unique_ptr<RocketComponent> component = DocumentConfig::createComponent(element);
    return component == nullptr ? "null" : std::string(className(component->kind()));
}

/// "<element> <class>" for every element of the component table.
[[nodiscard]] Texts constructorsOfTheTable()
{
    Texts rows;
    for (const std::string_view element : DocumentConfig::componentElements())
    {
        rows.push_back(std::format("{} {}", element, madeClass(element)));
    }
    return rows;
}

/// "<element> <class>" for every entry of OpenRocket's constructor table.
[[nodiscard]] Texts javaConstructors()
{
    Texts rows;
    for (const JavaConstructor& row : kJavaConstructors)
    {
        rows.push_back(std::format("{} {}", row.element, row.className));
    }
    return rows;
}

TEST(DocumentConfig, MakesTheComponentsOfOpenRocketsConstructorTable)
{
    EXPECT_EQ(DocumentConfig::componentElements().size(), 22U);
    EXPECT_EQ(constructorsOfTheTable(), javaConstructors());
    // The two names of a booster set.
    EXPECT_EQ(madeClass("boosterset"), "ParallelStage");
    EXPECT_EQ(madeClass("parallelstage"), "ParallelStage");
    EXPECT_EQ(madeClass("stage"), "AxialStage");
}

TEST(DocumentConfig, MakesNoComponentForAnyOtherName)
{
    // "rocket" is the element of the document's one rocket, not of a component to make
    // (cases1.out, unknown-component: "Unknown element sleeve, ignoring."). Names are compared
    // exactly.
    EXPECT_EQ(each({"rocket", "sleeve", "", "BodyTube", "Bodytube", "bodytube ", " bodytube",
                    "Stage", "axialstage", "subcomponents", "finset"},
                   madeClass),
              Texts(11, "null"));
}

TEST(DocumentConfig, MakesANewComponentAtEveryCall)
{
    const std::unique_ptr<RocketComponent> first  = DocumentConfig::createComponent("bodytube");
    const std::unique_ptr<RocketComponent> second = DocumentConfig::createComponent("bodytube");
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first.get(), second.get());
    EXPECT_NE(first->getId(), second->getId());
    // As the default constructor makes it: no parent, no children, the name of its class.
    EXPECT_EQ(first->getParent(), nullptr);
    EXPECT_EQ(first->getChildCount(), 0U);
    EXPECT_EQ(first->getName(), "Body Tube");
}

/// The kinds whose own element name does not make a component of the kind.
[[nodiscard]] Texts kindsWithoutAnElement()
{
    Texts kinds;
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        const std::unique_ptr<RocketComponent> component =
            DocumentConfig::createComponent(xmlName(kind));
        if (component == nullptr || component->kind() != kind)
        {
            kinds.emplace_back(componentKindName(kind));
        }
    }
    return kinds;
}

TEST(DocumentConfig, EveryKindButTheRocketIsMadeByItsOwnElement)
{
    EXPECT_EQ(kindsWithoutAnElement(), Texts{"ROCKET"});
}

/// The class and the element of a key "Class:element".
struct KeyParts
{
    std::string_view owner;
    std::string_view element;
};

[[nodiscard]] KeyParts split(std::string_view key)
{
    const std::size_t colon = key.find(':');
    return {.owner = key.substr(0, colon), .element = key.substr(colon + 1)};
}

/// The setter of the entry @p key, or null when no walk ends there with a setter: what the
/// walk finds for a kind that has the key's class among its classes and no entry for the
/// element nearer by.
[[nodiscard]] const Setter* setterOf(std::string_view key)
{
    const KeyParts parts = split(key);
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        const DocumentConfig::SetterLookup lookup = DocumentConfig::findSetter(kind, parts.element);
        if (lookup.setter != nullptr && lookup.owner == parts.owner)
        {
            return lookup.setter;
        }
    }
    return nullptr;
}

template <class S>
[[nodiscard]] bool isA(const Setter& setter)
{
    return dynamic_cast<const S*>(&setter) != nullptr;
}

/// The Java name of the class of @p setter when it is one of the seven that serve one kind of
/// element each (the position, fin tab, cluster, material and preset setters).
[[nodiscard]] std::string_view specialSetterClass(const Setter& setter)
{
    if (isA<QtRocket::AxialPositionSetter>(setter))
    {
        return "AxialPositionSetter";
    }
    if (isA<QtRocket::RadiusPositionSetter>(setter))
    {
        return "RadiusPositionSetter";
    }
    if (isA<QtRocket::AnglePositionSetter>(setter))
    {
        return "AnglePositionSetter";
    }
    if (isA<QtRocket::FinTabPositionSetter>(setter))
    {
        return "FinTabPositionSetter";
    }
    if (isA<QtRocket::ClusterConfigurationSetter>(setter))
    {
        return "ClusterConfigurationSetter";
    }
    if (isA<QtRocket::MaterialSetter>(setter))
    {
        return "MaterialSetter";
    }
    if (isA<QtRocket::ComponentPresetSetter>(setter))
    {
        return "ComponentPresetSetter";
    }
    return "a class the test does not know";
}

/// The Java name of the class of @p setter.
[[nodiscard]] std::string_view setterClass(const Setter& setter)
{
    if (isA<QtRocket::DoubleSetter>(setter))
    {
        return "DoubleSetter";
    }
    if (isA<QtRocket::IntSetter>(setter))
    {
        return "IntSetter";
    }
    if (isA<QtRocket::BooleanSetter>(setter))
    {
        return "BooleanSetter";
    }
    if (isA<QtRocket::StringSetter>(setter))
    {
        return "StringSetter";
    }
    if (isA<QtRocket::OverrideSetter>(setter))
    {
        return "OverrideSetter";
    }
    if (isA<QtRocket::ColorSetter>(setter))
    {
        return "ColorSetter";
    }
    if (isA<QtRocket::EnumSetter>(setter))
    {
        return "EnumSetter";
    }
    return specialSetterClass(setter);
}

/// "<key> <setter class>" for every entry of the setter table that has a setter.
[[nodiscard]] Texts settersOfTheTable()
{
    Texts rows;
    for (const std::string_view key : DocumentConfig::setterKeys())
    {
        const Setter* const setter = setterOf(key);
        rows.push_back(std::format("{} {}", key,
                                   setter == nullptr ? "no walk ends here" : setterClass(*setter)));
    }
    return rows;
}

/// "<key> <setter class>" for every entry of OpenRocket's table that has a setter.
[[nodiscard]] Texts javaSetters()
{
    Texts rows;
    for (const JavaSetter& row : kJavaSetters)
    {
        rows.push_back(std::format("{} {}", row.key, row.setterClass));
    }
    return rows;
}

[[nodiscard]] Texts asTexts(std::span<const std::string_view> views)
{
    return {views.begin(), views.end()};
}

TEST(DocumentConfig, TheSetterTableHasOpenRocketsEntriesWithItsSetterClasses)
{
    // OpenRocket: 135 entries, 5 of them null.
    EXPECT_EQ(kJavaSetters.size(), 130U);
    EXPECT_EQ(kJavaRefused.size(), 5U);
    // Every entry is one of OpenRocket's, with a setter of the class OpenRocket has there, and
    // can be reached by the walk of some kind.
    EXPECT_EQ(settersOfTheTable(), javaSetters());
    // 108 entries of the seven generic setter classes and 22 of the seven others.
    EXPECT_EQ(DocumentConfig::setterKeys().size(), 130U);
}

TEST(DocumentConfig, RefusesTheFiveElementsOfANoseConeThatOpenRocketRefuses)
{
    EXPECT_EQ(asTexts(DocumentConfig::refusedKeys()), asTexts(kJavaRefused));

    // The nose cone is refused what a transition may have (cases1.out, nosecone-disallowed:
    // "Unknown parameter type 'foreradius' for Nose Cone, ignoring.").
    const DocumentConfig::SetterLookup refused =
        DocumentConfig::findSetter(ComponentKind::NOSE_CONE, "foreradius");
    EXPECT_EQ(refused.setter, nullptr);
    EXPECT_EQ(refused.owner, "NoseCone");
    EXPECT_TRUE(refused.isRefused());

    const DocumentConfig::SetterLookup allowed =
        DocumentConfig::findSetter(ComponentKind::TRANSITION, "foreradius");
    EXPECT_NE(allowed.setter, nullptr);
    EXPECT_EQ(allowed.owner, "Transition");
    EXPECT_FALSE(allowed.isRefused());
}

/// Every element name of OpenRocket's setter table, sorted.
[[nodiscard]] std::vector<std::string_view> javaElements()
{
    std::vector<std::string_view> elements;
    elements.reserve(kJavaSetters.size() + kJavaRefused.size());
    for (const JavaSetter& row : kJavaSetters)
    {
        elements.push_back(split(row.key).element);
    }
    for (const std::string_view key : kJavaRefused)
    {
        elements.push_back(split(key).element);
    }
    std::ranges::sort(elements);
    const auto duplicates = std::ranges::unique(elements);
    elements.erase(duplicates.begin(), duplicates.end());
    return elements;
}

/// What the walk for @p kind finds for every element name, as the probe prints it:
/// "<class> found <n> refused <m> : <element>=<owner> <element>=!<owner> ...".
[[nodiscard]] std::string walkOf(ComponentKind kind)
{
    int         found   = 0;
    int         refused = 0;
    std::string entries;
    for (const std::string_view element : javaElements())
    {
        const DocumentConfig::SetterLookup lookup = DocumentConfig::findSetter(kind, element);
        if (lookup.owner.empty())
        {
            continue;
        }
        (lookup.isRefused() ? refused : found) += 1;
        entries += std::format(" {}={}{}", element, lookup.isRefused() ? "!" : "", lookup.owner);
    }
    return std::format("{} found {} refused {} :{}", className(kind), found, refused, entries);
}

/// @p walk in the form of walkOf().
[[nodiscard]] std::string javaWalkOf(const JavaWalk& walk)
{
    return std::format("{} {}", className(walk.kind), walk.entries);
}

[[nodiscard]] Texts walksOfTheTable()
{
    Texts walks;
    for (const JavaWalk& walk : kJavaWalks)
    {
        walks.push_back(walkOf(walk.kind));
    }
    return walks;
}

[[nodiscard]] Texts javaWalks()
{
    Texts walks;
    for (const JavaWalk& walk : kJavaWalks)
    {
        walks.push_back(javaWalkOf(walk));
    }
    return walks;
}

// For each of the 22 kinds and each of the element names of the table: the class at which
// OpenRocket's search ends, with a setter or with a refusal, or that it finds nothing.
TEST(DocumentConfig, TheWalkForEveryKindFindsWhatOpenRocketsSearchFinds)
{
    EXPECT_EQ(kJavaWalks.size(), QtRocket::kAllComponentKinds.size());
    EXPECT_EQ(walksOfTheTable(), javaWalks());
}

/// Where the walk for @p kind ends for @p element: "<class>" with a setter, "!<class>" with
/// a refusal, "" when no class knows the element.
[[nodiscard]] std::string ownerOf(ComponentKind kind, std::string_view element)
{
    const DocumentConfig::SetterLookup lookup = DocumentConfig::findSetter(kind, element);
    return std::format("{}{}", lookup.isRefused() ? "!" : "", lookup.owner);
}

TEST(DocumentConfig, TheWalkEndsAtTheNearestClassThatHasAnEntry)
{
    // A nose cone is a transition, a symmetric component, a body component, an external
    // component and a rocket component, and has an entry or two of its own.
    EXPECT_EQ(ownerOf(ComponentKind::NOSE_CONE, "isflipped"), "NoseCone");
    EXPECT_EQ(ownerOf(ComponentKind::NOSE_CONE, "foreshoulderlength"), "!NoseCone");
    EXPECT_EQ(ownerOf(ComponentKind::NOSE_CONE, "aftshoulderlength"), "Transition");
    EXPECT_EQ(ownerOf(ComponentKind::NOSE_CONE, "shape"), "Transition");
    EXPECT_EQ(ownerOf(ComponentKind::NOSE_CONE, "thickness"), "SymmetricComponent");
    EXPECT_EQ(ownerOf(ComponentKind::NOSE_CONE, "length"), "BodyComponent");
    EXPECT_EQ(ownerOf(ComponentKind::NOSE_CONE, "finish"), "ExternalComponent");
    EXPECT_EQ(ownerOf(ComponentKind::NOSE_CONE, "name"), "RocketComponent");

    // The same element is another entry for each class that has one.
    EXPECT_EQ(ownerOf(ComponentKind::BODY_TUBE, "thickness"), "SymmetricComponent");
    EXPECT_EQ(ownerOf(ComponentKind::TRAPEZOID_FIN_SET, "thickness"), "FinSet");
    EXPECT_EQ(ownerOf(ComponentKind::TUBE_FIN_SET, "thickness"), "TubeFinSet");
    EXPECT_EQ(ownerOf(ComponentKind::LAUNCH_LUG, "thickness"), "LaunchLug");
    EXPECT_EQ(ownerOf(ComponentKind::TUBE_COUPLER, "thickness"), "ThicknessRingComponent");
    EXPECT_EQ(ownerOf(ComponentKind::BODY_TUBE, "length"), "BodyComponent");
    EXPECT_EQ(ownerOf(ComponentKind::LAUNCH_LUG, "length"), "LaunchLug");
    EXPECT_EQ(ownerOf(ComponentKind::TUBE_FIN_SET, "length"), "TubeFinSet");
    EXPECT_EQ(ownerOf(ComponentKind::ENGINE_BLOCK, "length"), "RingComponent");
    EXPECT_EQ(ownerOf(ComponentKind::TRAPEZOID_FIN_SET, "height"), "TrapezoidFinSet");
    EXPECT_EQ(ownerOf(ComponentKind::ELLIPTICAL_FIN_SET, "height"), "EllipticalFinSet");
    EXPECT_EQ(ownerOf(ComponentKind::RAIL_BUTTON, "height"), "RailButton");

    // A booster set is a stage.
    EXPECT_EQ(ownerOf(ComponentKind::PARALLEL_STAGE, "separationdelay"), "AxialStage");
    EXPECT_EQ(ownerOf(ComponentKind::PARALLEL_STAGE, "instancecount"), "ParallelStage");
    EXPECT_EQ(ownerOf(ComponentKind::AXIAL_STAGE, "instancecount"), "");

    // A centering ring has an inner radius of its own; a bulkhead takes the entry of the class
    // above both.
    EXPECT_EQ(ownerOf(ComponentKind::CENTERING_RING, "innerradius"), "CenteringRing");
    EXPECT_EQ(ownerOf(ComponentKind::BULKHEAD, "innerradius"), "RadiusRingComponent");
    EXPECT_EQ(ownerOf(ComponentKind::INNER_TUBE, "innerradius"), "");
}

TEST(DocumentConfig, FindsNothingForAnElementThatNoClassOfTheKindKnows)
{
    const DocumentConfig::SetterLookup unknown =
        DocumentConfig::findSetter(ComponentKind::BODY_TUBE, "bogus");
    EXPECT_EQ(unknown.setter, nullptr);
    EXPECT_TRUE(unknown.owner.empty());
    EXPECT_FALSE(unknown.isRefused());

    // The element of another class.
    EXPECT_EQ(ownerOf(ComponentKind::BODY_TUBE, "fincount"), "");
    EXPECT_EQ(ownerOf(ComponentKind::ROCKET, "length"), "");
    EXPECT_EQ(ownerOf(ComponentKind::AXIAL_STAGE, "designer"), "");
    EXPECT_EQ(ownerOf(ComponentKind::STREAMER, "diameter"), "");
    // A refusal is the nose cone's alone.
    EXPECT_EQ(ownerOf(ComponentKind::BODY_TUBE, "foreradius"), "");
    // Names are compared exactly.
    EXPECT_EQ(ownerOf(ComponentKind::BODY_TUBE, "Length"), "");
    EXPECT_EQ(ownerOf(ComponentKind::BODY_TUBE, "length "), "");
    EXPECT_EQ(ownerOf(ComponentKind::BODY_TUBE, ""), "");
    EXPECT_EQ(ownerOf(ComponentKind::BODY_TUBE, "BodyComponent:length"), "");
    // The elements with a handler of their own are no parameters.
    EXPECT_EQ(ownerOf(ComponentKind::BODY_TUBE, "subcomponents"), "");
    EXPECT_EQ(ownerOf(ComponentKind::BODY_TUBE, "motormount"), "");
    EXPECT_EQ(ownerOf(ComponentKind::FREEFORM_FIN_SET, "finpoints"), "");
    EXPECT_EQ(ownerOf(ComponentKind::ROCKET, "motorconfiguration"), "");
    EXPECT_EQ(ownerOf(ComponentKind::NOSE_CONE, "appearance"), "");
}

/// Whether @p setter, applied to @p component with the element text @p text, succeeds.
[[nodiscard]] bool succeeds(const Setter& setter, RocketComponent& component, std::string_view text,
                            SetterFixture& fixture)
{
    WarningSet warnings;
    return setter.set(component, text, {}, warnings, fixture.context()).has_value();
}

// A setter casts the component to the class of its key. The walk hands it a component of
// that class; a caller that does not gets a BugError (Java: a BugException).
TEST(DocumentConfig, ASetterAppliedToAComponentOfAnotherClassIsAProgrammingError)
{
    SetterFixture    fixture;
    RocketComponent& cone = fixture.make(ComponentKind::NOSE_CONE);

    const Setter* const radius =
        DocumentConfig::findSetter(ComponentKind::BODY_TUBE, "radius").setter;
    ASSERT_NE(radius, nullptr);
    EXPECT_THROW(static_cast<void>(succeeds(*radius, cone, "0.01", fixture)), BugError);
    EXPECT_THROW(static_cast<void>(succeeds(*radius, cone, "auto", fixture)), BugError);

    const Setter* const designer =
        DocumentConfig::findSetter(ComponentKind::ROCKET, "designer").setter;
    ASSERT_NE(designer, nullptr);
    EXPECT_THROW(static_cast<void>(succeeds(*designer, cone, "x", fixture)), BugError);

    // A text the setter refuses never reaches the component: no cast, no error.
    EXPECT_TRUE(succeeds(*radius, cone, "abc", fixture));
    // And the component of the right class is set.
    EXPECT_TRUE(succeeds(*radius, fixture.tube(), "0.01", fixture));
    EXPECT_EQ(fixture.tube().getOuterRadius(), 0.01);
}

// cases1.out, bad-id: "THROWN ... RocketLoadException: Exception loading stream: Invalid UUID
// string: not-a-uuid". The loader of run 9c puts the prefix before the message.
TEST(DocumentConfig, AnIdThatIsNoUuidFailsTheLoadWithJavasMessage)
{
    SetterFixture        fixture;
    const QtRocket::Uuid before = fixture.tube().getId();
    WarningSet           warnings;
    const Setter* const  id = DocumentConfig::findSetter(ComponentKind::BODY_TUBE, "id").setter;
    ASSERT_NE(id, nullptr);

    const Result<void> failed =
        id->set(fixture.tube(), "not-a-uuid", {}, warnings, fixture.context());
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(failed.error().message, "Invalid UUID string: not-a-uuid");
    EXPECT_EQ(fixture.tube().getId(), before);
    EXPECT_TRUE(warnings.empty());

    // cases1.out, short-id: "1-2-3-4-5" is an id to Java.
    const Result<void> changed =
        id->set(fixture.tube(), "1-2-3-4-5", {}, warnings, fixture.context());
    EXPECT_TRUE(changed.has_value());
    EXPECT_EQ(fixture.tube().getId().toString(), "00000001-0002-0003-0004-000000000005");
    EXPECT_TRUE(warnings.empty());
}

TEST(DocumentConfig, AttributeIsTheValueOfAnAttributeOrNothing)
{
    using Value = std::optional<std::string_view>;
    const ElementHandler::Attributes attributes{{"method", "top"}, {"empty", ""}, {"Type", "x"}};

    EXPECT_EQ(DocumentConfig::attribute(attributes, "method"), Value("top"));
    // An attribute without a value is there.
    EXPECT_EQ(DocumentConfig::attribute(attributes, "empty"), Value(""));
    EXPECT_EQ(DocumentConfig::attribute(attributes, "type"), std::nullopt);
    EXPECT_EQ(DocumentConfig::attribute(attributes, "METHOD"), std::nullopt);
    EXPECT_EQ(DocumentConfig::attribute(attributes, ""), std::nullopt);
    EXPECT_EQ(DocumentConfig::attribute(ElementHandler::Attributes{}, "method"), std::nullopt);
}

/// The text of DocumentConfig::configurationId() for an element whose configid is @p configId.
[[nodiscard]] std::string idOf(std::string_view configId)
{
    return DocumentConfig::configurationId({{"configid", std::string(configId)}}).toString();
}

// The ids are OpenRocket's (HandlerProbe.java of part R3, out/MotorConfigurationHandler.java.out,
// case mc-text-configids).
TEST(DocumentConfig, ConfigurationIdIsTheIdOpenRocketMakesOfTheAttribute)
{
    // A UUID as java.util.UUID.fromString() reads one, short groups and capital letters included.
    EXPECT_EQ(idOf("11111111-2222-3333-4444-555555555555"), "11111111-2222-3333-4444-555555555555");
    EXPECT_EQ(idOf("AAAAAAAA-2222-3333-4444-555555555555"), "aaaaaaaa-2222-3333-4444-555555555555");
    EXPECT_EQ(idOf("1-2-3-4-5"), "00000001-0002-0003-0004-000000000005");
    // Any other text: new UUID(0, text.hashCode()), as the oldest files name their
    // configurations. The hash is Java's, of the UTF-16 form, and may be negative.
    EXPECT_EQ(idOf("abc"), "00000000-0000-0000-0000-000000017862");
    EXPECT_EQ(idOf("def"), "00000000-0000-0000-0000-000000018405");
    EXPECT_EQ(idOf("zzzzzzzzzz"), "00000000-0000-0000-ffff-ffffa1c42c40");
    EXPECT_EQ(idOf(" "), "00000000-0000-0000-0000-000000000020");
    EXPECT_EQ(idOf("\u00e4\u20ac\U0001d11e"), "00000000-0000-0000-0000-000000fd55b2");
}

TEST(DocumentConfig, ConfigurationIdIsANewRandomIdWithoutAText)
{
    // No attribute, and an empty one: a random id each time, never one of the two reserved ids.
    const QtRocket::FlightConfigurationId none  = DocumentConfig::configurationId({});
    const QtRocket::FlightConfigurationId again = DocumentConfig::configurationId({});
    const QtRocket::FlightConfigurationId empty =
        DocumentConfig::configurationId({{"configid", ""}});
    EXPECT_NE(none, again);
    EXPECT_NE(none, empty);
    EXPECT_TRUE(none.isValid());
    EXPECT_FALSE(none.isDefaultId());
    EXPECT_TRUE(empty.isValid());
    EXPECT_FALSE(empty.isDefaultId());
    // Other attributes are not looked at, and the name is compared exactly.
    EXPECT_NE(DocumentConfig::configurationId({{"ConfigId", "abc"}, {"id", "abc"}}).toString(),
              "00000000-0000-0000-0000-000000017862");
}

// In OpenRocket no text makes one of the two reserved ids (it tells them by the identity of
// their key objects); here ids are values, and a text that spells out a reserved key is that
// id. Each handler says what it does with them.
TEST(DocumentConfig, ConfigurationIdCanBeAReservedIdWhenTheTextSpellsItOut)
{
    const QtRocket::FlightConfigurationId error =
        DocumentConfig::configurationId({{"configid", "ffffffff-f4f2-f1f0-0000-0000000009b9"}});
    EXPECT_FALSE(error.isValid());
    EXPECT_EQ(error, QtRocket::FlightConfigurationId::errorId());

    const QtRocket::FlightConfigurationId standard =
        DocumentConfig::configurationId({{"configid", "ffffffff-f4f2-f1f0-0000-00000000162c"}});
    EXPECT_TRUE(standard.isValid());
    EXPECT_TRUE(standard.isDefaultId());
    EXPECT_EQ(standard, QtRocket::FlightConfigurationId::defaultValueId());
}

}  // namespace
