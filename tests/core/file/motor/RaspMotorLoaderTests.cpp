#include "QtRocket/file/motor/RaspMotorLoader.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/motor/AbstractMotorLoader.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"

namespace
{

using QtRocket::AbstractMotorLoader;
using QtRocket::ErrorCode;
using QtRocket::Motor;
using QtRocket::RaspMotorLoader;
using QtRocket::Result;
using QtRocket::ThrustCurveMotor;

using Doubles = std::vector<double>;

constexpr double kInf = std::numeric_limits<double>::infinity();

// The digests of TestMotorLoader.java and of OpenRocket's RASPMotorLoader on the files below
// (pinned by running OpenRocket 5f164fd0e on them).
constexpr std::string_view kDigest1  = "e523030bc96d5e63313b5723aaea267d";  // test1.eng
constexpr std::string_view kDigestA8 = "8e6fb7d51ee11a4a56ab77a3467bcffe";  // kA8 below

/// A plain A8 in RASP format.
constexpr std::string_view kA8 = "A8 18 70 3 0.003 0.016 Estes\n0 0\n0.5 5\n1 0\n";

/// AbstractMotorLoader's protected helpers, made callable.
class Helpers final : public AbstractMotorLoader
{
public:
    using AbstractMotorLoader::calculateMass;
    using AbstractMotorLoader::finalizeThrustCurve;
    using AbstractMotorLoader::readLines;
    using AbstractMotorLoader::sortLists;
    using AbstractMotorLoader::split;

protected:
    [[nodiscard]] Result<std::vector<ThrustCurveMotor::Builder>> loadText(
        std::string_view /*text*/, std::string_view /*filename*/) const override
    {
        return std::vector<ThrustCurveMotor::Builder>{};
    }
    [[nodiscard]] Charset getDefaultCharset() const noexcept override { return Charset::UTF_8; }
};

/// @p text (ISO-8859-1 bytes) read by a RaspMotorLoader as "x.eng".
[[nodiscard]] Result<std::vector<ThrustCurveMotor::Builder>> load(std::string_view text)
{
    return RaspMotorLoader().load(QtRocket::stringToBytes(text), "x.eng");
}

/// The motors of @p text, built; fails the test when loading or building fails.
[[nodiscard]] std::vector<ThrustCurveMotor> loadMotors(std::string_view text)
{
    std::vector<ThrustCurveMotor>                        motors;
    const Result<std::vector<ThrustCurveMotor::Builder>> builders = load(text);
    EXPECT_TRUE(builders) << builders.error().toString();
    if (!builders)
    {
        return motors;
    }
    for (const ThrustCurveMotor::Builder& builder : *builders)
    {
        Result<ThrustCurveMotor> motor = builder.build();
        EXPECT_TRUE(motor) << motor.error().toString();
        if (motor)
        {
            motors.push_back(std::move(*motor));
        }
    }
    return motors;
}

/// The single motor of @p text, built (an exception fails the test when there is none).
[[nodiscard]] ThrustCurveMotor loadOne(std::string_view text)
{
    std::vector<ThrustCurveMotor> motors = loadMotors(text);
    EXPECT_EQ(motors.size(), 1U) << text;
    return std::move(motors.at(0));
}

/// Why the single motor of @p text does not build, or "" when it does.
[[nodiscard]] std::string buildError(std::string_view text)
{
    const std::vector<ThrustCurveMotor::Builder> builders = load(text).value();
    EXPECT_EQ(builders.size(), 1U) << text;
    const Result<ThrustCurveMotor> motor = builders.at(0).build();
    return motor ? std::string() : motor.error().message;
}

/// The failure message of loading @p text.
[[nodiscard]] std::string loadError(std::string_view text)
{
    const Result<std::vector<ThrustCurveMotor::Builder>> builders = load(text);
    EXPECT_FALSE(builders);
    if (builders)
    {
        return {};
    }
    EXPECT_EQ(builders.error().code, ErrorCode::PARSE);
    return builders.error().message;
}

// ---------------------------------------------------------------- AbstractMotorLoader

TEST(AbstractMotorLoader, RemoveDelay)
{
    EXPECT_EQ(AbstractMotorLoader::removeDelay("B6-5"), "B6");
    EXPECT_EQ(AbstractMotorLoader::removeDelay("B6-0"), "B6");
    EXPECT_EQ(AbstractMotorLoader::removeDelay("H128W-14"), "H128W");
    EXPECT_EQ(AbstractMotorLoader::removeDelay("H128W-P"), "H128W");
    EXPECT_EQ(AbstractMotorLoader::removeDelay("H128W-p"), "H128W");
    EXPECT_EQ(AbstractMotorLoader::removeDelay("A-B-7"), "A-B");
    EXPECT_EQ(AbstractMotorLoader::removeDelay("-5"), "");
    // Not a delay
    EXPECT_EQ(AbstractMotorLoader::removeDelay("B6"), "B6");
    EXPECT_EQ(AbstractMotorLoader::removeDelay("B6-"), "B6-");
    EXPECT_EQ(AbstractMotorLoader::removeDelay("B6-PP"), "B6-PP");
    EXPECT_EQ(AbstractMotorLoader::removeDelay("B6-5a"), "B6-5a");
    EXPECT_EQ(AbstractMotorLoader::removeDelay("B6-S"), "B6-S");
    EXPECT_EQ(AbstractMotorLoader::removeDelay("B6-5 "), "B6-5 ");
    EXPECT_EQ(AbstractMotorLoader::removeDelay(""), "");
    // Java's '.' matches no line terminator, so the pattern fails on one before the delay.
    EXPECT_EQ(AbstractMotorLoader::removeDelay("B6\n-5"), "B6\n-5");
    EXPECT_EQ(AbstractMotorLoader::removeDelay("B\r6-5"), "B\r6-5");
    EXPECT_EQ(AbstractMotorLoader::removeDelay("B\u20286-5"), "B\u20286-5");
    EXPECT_EQ(AbstractMotorLoader::removeDelay("B\u00856-5"), "B\u00856-5");
    EXPECT_EQ(AbstractMotorLoader::removeDelay("\u00E9B6-5"), "\u00E9B6");
}

TEST(AbstractMotorLoader, CalculateMassScalesTheTrapezoidalImpulse)
{
    // Impulse 5 + 5 = 10, scaled to the 0.5 of propellant.
    const std::vector<double> mass =
        Helpers::calculateMass(Doubles{0, 1, 2}, Doubles{0, 10, 0}, 1.0, 0.5);
    ASSERT_EQ(mass.size(), 3U);
    EXPECT_DOUBLE_EQ(mass[0], 1.0);
    EXPECT_DOUBLE_EQ(mass[1], 0.75);
    EXPECT_DOUBLE_EQ(mass[2], 0.5);

    // Uneven steps: 0.5 * (0 + 4) * 1 = 2, then 0.5 * (4 + 2) * 2 = 6.
    const std::vector<double> uneven =
        Helpers::calculateMass(Doubles{0, 1, 3}, Doubles{0, 4, 2}, 0.2, 0.08);
    ASSERT_EQ(uneven.size(), 3U);
    EXPECT_DOUBLE_EQ(uneven[0], 0.2);
    EXPECT_DOUBLE_EQ(uneven[1], 0.2 - (2 * (0.08 / 8)));
    EXPECT_DOUBLE_EQ(uneven[2], (0.2 - (2 * (0.08 / 8))) - (6 * (0.08 / 8)));
}

TEST(AbstractMotorLoader, CalculateMassFloorsAtZero)
{
    const std::vector<double> mass =
        Helpers::calculateMass(Doubles{0, 1, 2}, Doubles{0, 10, 0}, 0.1, 0.4);
    ASSERT_EQ(mass.size(), 3U);
    EXPECT_DOUBLE_EQ(mass[0], 0.1);
    EXPECT_DOUBLE_EQ(mass[1], 0.0);
    EXPECT_DOUBLE_EQ(mass[2], 0.0);
}

TEST(AbstractMotorLoader, CalculateMassWithoutImpulseIsNaN)
{
    // prop / 0 is infinite and 0 * infinity is NaN, which no comparison clamps.
    const std::vector<double> mass = Helpers::calculateMass(Doubles{0, 1}, Doubles{0, 0}, 1.0, 0.5);
    ASSERT_EQ(mass.size(), 2U);
    EXPECT_DOUBLE_EQ(mass[0], 1.0);
    EXPECT_TRUE(std::isnan(mass[1]));

    const std::vector<double> single = Helpers::calculateMass(Doubles{0}, Doubles{5}, 1.0, 0.5);
    EXPECT_EQ(single, std::vector<double>{1.0});
}

TEST(AbstractMotorLoader, SplitOnWhitespace)
{
    using Tokens = std::vector<std::string>;
    EXPECT_EQ(Helpers::split("F32 24 124 5-10-15-P .0377 .0695 RV"),
              (Tokens{"F32", "24", "124", "5-10-15-P", ".0377", ".0695", "RV"}));
    EXPECT_EQ(Helpers::split("  a \t b\x0B\fc  "), (Tokens{"a", "b", "c"}));
    EXPECT_EQ(Helpers::split(""), Tokens{});
    EXPECT_EQ(Helpers::split(" \t "), Tokens{});
    // A no-break space (U+00A0) is not Java regex whitespace.
    EXPECT_EQ(Helpers::split("a\u00A0b"), Tokens{"a\u00A0b"});
}

TEST(AbstractMotorLoader, SplitOnDelimiters)
{
    using Tokens = std::vector<std::string>;
    EXPECT_EQ(Helpers::split("5-10-15-P", "-,"), (Tokens{"5", "10", "15", "P"}));
    EXPECT_EQ(Helpers::split("-5,,7-", "-,"), (Tokens{"5", "7"}));
    EXPECT_EQ(Helpers::split(",", "-,"), Tokens{});
    EXPECT_EQ(Helpers::split("None", "-,"), Tokens{"None"});
}

TEST(AbstractMotorLoader, SortListsIsStableAndCarriesTheOtherLists)
{
    std::vector<double> time{0.3, 0.1, 0.2, 0.1};
    std::vector<double> thrust{3, 1, 2, 4};
    std::vector<double> other{30, 10, 20, 40};
    Helpers::sortLists(time, {thrust, other});
    EXPECT_EQ(time, (std::vector<double>{0.1, 0.1, 0.2, 0.3}));
    EXPECT_EQ(thrust, (std::vector<double>{1, 4, 2, 3}));
    EXPECT_EQ(other, (std::vector<double>{10, 40, 20, 30}));
}

TEST(AbstractMotorLoader, SortListsMovesNothingPastNaN)
{
    // OpenRocket compares neighbours with <, which is false for NaN either way.
    std::vector<double> time{2, std::numeric_limits<double>::quiet_NaN(), 1, 0};
    std::vector<double> thrust{20, 99, 10, 0};
    Helpers::sortLists(time, {thrust});
    EXPECT_DOUBLE_EQ(time[0], 2);
    EXPECT_TRUE(std::isnan(time[1]));
    EXPECT_DOUBLE_EQ(time[2], 0);
    EXPECT_DOUBLE_EQ(time[3], 1);
    EXPECT_EQ(thrust, (std::vector<double>{20, 99, 0, 10}));
}

TEST(AbstractMotorLoader, FinalizeAddsAZeroPointInFront)
{
    std::vector<double> time{0.5, 1};
    std::vector<double> thrust{5, 0};
    std::vector<double> mass{7, 8};
    ASSERT_TRUE(Helpers::finalizeThrustCurve(time, thrust, {mass}));
    EXPECT_EQ(time, (std::vector<double>{0, 0.5, 1}));
    EXPECT_EQ(thrust, (std::vector<double>{0, 5, 0}));
    EXPECT_EQ(mass, (std::vector<double>{7, 7, 8}));
}

TEST(AbstractMotorLoader, FinalizeTakesTimesWithinEpsilonOfZeroAsZero)
{
    std::vector<double> time{1e-9, 1};
    std::vector<double> thrust{5, 0};
    ASSERT_TRUE(Helpers::finalizeThrustCurve(time, thrust));
    EXPECT_EQ(time, (std::vector<double>{1e-9, 1}));
}

TEST(AbstractMotorLoader, FinalizeDropsTheFirstOfTwoZeroTimesFromTimeAndThrustOnly)
{
    std::vector<double> time{0, 0, 1};
    std::vector<double> thrust{0, 4, 0};
    std::vector<double> mass{1, 2, 3};
    ASSERT_TRUE(Helpers::finalizeThrustCurve(time, thrust, {mass}));
    EXPECT_EQ(time, (std::vector<double>{0, 1}));
    EXPECT_EQ(thrust, (std::vector<double>{4, 0}));
    EXPECT_EQ(mass, (std::vector<double>{1, 2, 3}));  // as OpenRocket leaves it
}

TEST(AbstractMotorLoader, FinalizeDropsDuplicatePoints)
{
    std::vector<double> time{0, 0.5, 0.5, 0.5, 1};
    std::vector<double> thrust{0, 5, 5, 5, 0};
    std::vector<double> mass{1, 2, 3, 4, 5};
    ASSERT_TRUE(Helpers::finalizeThrustCurve(time, thrust, {mass}));
    EXPECT_EQ(time, (std::vector<double>{0, 0.5, 1}));
    EXPECT_EQ(thrust, (std::vector<double>{0, 5, 0}));
    EXPECT_EQ(mass, (std::vector<double>{1, 4, 5}));

    // The same time with another thrust is kept.
    std::vector<double> sameTime{0, 0.5, 0.5, 1};
    std::vector<double> otherThrust{0, 5, 6, 0};
    ASSERT_TRUE(Helpers::finalizeThrustCurve(sameTime, otherThrust));
    EXPECT_EQ(sameTime.size(), 4U);
}

TEST(AbstractMotorLoader, FinalizeDropsTheZeroOfTwoFinalPoints)
{
    std::vector<double> time{0, 0.5, 1, 1};
    std::vector<double> thrust{0, 5, 3, 0};
    std::vector<double> mass{1, 2, 3, 4};
    ASSERT_TRUE(Helpers::finalizeThrustCurve(time, thrust, {mass}));
    EXPECT_EQ(time, (std::vector<double>{0, 0.5, 1}));
    EXPECT_EQ(thrust, (std::vector<double>{0, 5, 3}));
    EXPECT_EQ(mass, (std::vector<double>{1, 2, 3}));

    std::vector<double> zeroFirst{0, 0.5, 1, 1};
    std::vector<double> zeroFirstThrust{0, 5, 0, 3};
    std::vector<double> zeroFirstMass{1, 2, 3, 4};
    ASSERT_TRUE(Helpers::finalizeThrustCurve(zeroFirst, zeroFirstThrust, {zeroFirstMass}));
    EXPECT_EQ(zeroFirst, (std::vector<double>{0, 0.5, 1}));
    EXPECT_EQ(zeroFirstThrust, (std::vector<double>{0, 5, 3}));
    EXPECT_EQ(zeroFirstMass, (std::vector<double>{1, 2, 4}));
}

TEST(AbstractMotorLoader, FinalizeLeavesAnEmptyCurveAlone)
{
    std::vector<double> time;
    std::vector<double> thrust;
    EXPECT_TRUE(Helpers::finalizeThrustCurve(time, thrust));
    EXPECT_TRUE(time.empty());
}

TEST(AbstractMotorLoader, FinalizeFailsWhereOpenRocketReadsPastOnePoint)
{
    std::vector<double> time{0};
    std::vector<double> thrust{5};
    const Result<void>  single = Helpers::finalizeThrustCurve(time, thrust);
    ASSERT_FALSE(single);
    EXPECT_EQ(single.error().code, ErrorCode::PARSE);
    EXPECT_EQ(single.error().message, "Index 1 out of bounds for length 1");

    std::vector<double> twice{0, 0};
    std::vector<double> twiceThrust{5, 5};
    const Result<void>  both = Helpers::finalizeThrustCurve(twice, twiceThrust);
    ASSERT_FALSE(both);
    EXPECT_EQ(both.error().message, "Index -1 out of bounds for length 1");

    // A single point after zero gets its zero point and is fine.
    std::vector<double> late{0.5};
    std::vector<double> lateThrust{5};
    ASSERT_TRUE(Helpers::finalizeThrustCurve(late, lateThrust));
    EXPECT_EQ(late, (std::vector<double>{0, 0.5}));
}

TEST(AbstractMotorLoader, ReadLinesAsBufferedReader)
{
    using Lines = std::vector<std::string_view>;
    EXPECT_EQ(Helpers::readLines(""), Lines{});
    EXPECT_EQ(Helpers::readLines("a"), Lines{"a"});
    EXPECT_EQ(Helpers::readLines("a\nb\n"), (Lines{"a", "b"}));
    EXPECT_EQ(Helpers::readLines("a\r\nb\r\n"), (Lines{"a", "b"}));
    EXPECT_EQ(Helpers::readLines("a\rb"), (Lines{"a", "b"}));
    EXPECT_EQ(Helpers::readLines("a\n\nb"), (Lines{"a", "", "b"}));
    EXPECT_EQ(Helpers::readLines("\n"), Lines{""});
    EXPECT_EQ(Helpers::readLines("a\r\r\n"), (Lines{"a", ""}));
    EXPECT_EQ(Helpers::readLines("a\n\r"), (Lines{"a", ""}));
}

TEST(AbstractMotorLoader, DecodesInTheLoadersCharacterSet)
{
    const std::vector<std::byte> bytes = QtRocket::stringToBytes("caf\xE9");
    EXPECT_EQ(AbstractMotorLoader::decode(bytes, AbstractMotorLoader::Charset::ISO_8859_1),
              "caf\u00E9");
    EXPECT_EQ(AbstractMotorLoader::decode(bytes, AbstractMotorLoader::Charset::UTF_8), "caf\uFFFD");
}

// ---------------------------------------------------------------- RaspMotorLoader

// TestMotorLoader.testRASPMotorLoader
TEST(RaspMotorLoader, LoadsTest1)
{
    const Result<std::vector<std::byte>> bytes =
        QtRocket::readFile(QtRocket::Test::testDataDir() / "motors" / "test1.eng");
    ASSERT_TRUE(bytes);
    const Result<std::vector<ThrustCurveMotor::Builder>> motors =
        RaspMotorLoader().load(*bytes, "/file/motor/test1.eng");
    ASSERT_TRUE(motors) << motors.error().toString();
    ASSERT_EQ(motors->size(), 1U);
    const Result<ThrustCurveMotor> motor = motors->front().build();
    ASSERT_TRUE(motor);
    EXPECT_EQ(motor->getDigest(), kDigest1);
    EXPECT_EQ(motor->getDesignation(), "D10");
    EXPECT_EQ(motor->getManufacturer().getDisplayName(), "AeroTech");
    EXPECT_EQ(motor->getStandardDelays(), std::vector<double>{7});
    EXPECT_DOUBLE_EQ(motor->getDiameter(), 0.018);
    EXPECT_DOUBLE_EQ(motor->getLength(), 0.07);
    EXPECT_EQ(motor->getTimePoints().size(), 24U);  // a (0, 0) point in front
    EXPECT_DOUBLE_EQ(motor->getLaunchMass(), 0.0259);
    EXPECT_DOUBLE_EQ(motor->getBurnoutMass(), 0.0161);
    EXPECT_DOUBLE_EQ(motor->getLaunchCGx(), 0.035);
}

TEST(RaspMotorLoader, ReadsCommentsHeaderDelaysAndData)
{
    const std::vector<ThrustCurveMotor> motors = loadMotors(
        "; comment one\n;  comment two  \n"
        "B6-0 18 70 0-3-5-P-100 0.0062 0.0249 Estes\n0.1 5\n0.5 10\n1.0 0\n");
    ASSERT_EQ(motors.size(), 1U);
    const ThrustCurveMotor& motor = motors.front();
    EXPECT_EQ(motor.getDigest(), "aea03c3e824afa8a207c6962aa6192da");
    EXPECT_EQ(motor.getDesignation(), "B6");
    EXPECT_EQ(motor.getCommonName(), "B6");
    EXPECT_EQ(motor.getCode(), "");
    EXPECT_EQ(motor.getDescription(), "comment one\ncomment two");
    EXPECT_EQ(motor.getManufacturer().getSimpleName(), "Estes");
    EXPECT_EQ(motor.getMotorType(), Motor::Type::SINGLE);  // Estes makes single-use motors
    EXPECT_EQ(motor.getStandardDelays(), (std::vector<double>{0, 3, 5, kInf}));
    EXPECT_EQ(motor.getTimePoints(), (std::vector<double>{0, 0.1, 0.5, 1.0}));
    EXPECT_EQ(motor.getThrustPoints(), (std::vector<double>{0, 5, 10, 0}));
    EXPECT_DOUBLE_EQ(motor.getLaunchMass(), 0.0249);
    EXPECT_DOUBLE_EQ(motor.getBurnoutMass(), 0.018699999999999998);
}

TEST(RaspMotorLoader, AcceptsEveryLineEnding)
{
    const std::string text =
        "; comment one\n;  comment two  \n"
        "B6-0 18 70 0-3-5-P-100 0.0062 0.0249 Estes\n0.1 5\n0.5 10\n1.0 0\n";
    for (const std::string_view ending : {"\r\n", "\r"})
    {
        std::string converted;
        for (const char c : text)
        {
            converted += c == '\n' ? std::string(ending) : std::string(1, c);
        }
        const std::vector<ThrustCurveMotor> motors = loadMotors(converted);
        ASSERT_EQ(motors.size(), 1U);
        EXPECT_EQ(motors.front().getDigest(), "aea03c3e824afa8a207c6962aa6192da");
        EXPECT_EQ(motors.front().getDescription(), "comment one\ncomment two");
    }
}

TEST(RaspMotorLoader, ReadsSeveralMotors)
{
    const std::vector<ThrustCurveMotor> motors =
        loadMotors(std::string(kA8) + ";next\nB4 18 70 None 0.004 0.02 Estes\n0.2 3\n0.8 0\n");
    ASSERT_EQ(motors.size(), 2U);
    EXPECT_EQ(motors[0].getDigest(), kDigestA8);
    EXPECT_EQ(motors[0].getDescription(), "");
    EXPECT_EQ(motors[1].getDigest(), "ac265c3503aba6955d225b35a670ecde");
    EXPECT_EQ(motors[1].getDescription(), "next");
    EXPECT_TRUE(motors[1].getStandardDelays().empty());
}

TEST(RaspMotorLoader, ToleratesMissingFinalNewlineTabsAndBlankLines)
{
    for (const std::string_view text :
         {"A8 18 70 3 0.003 0.016 Estes\n0 0\n0.5 5\n1 0",
          "A8\t18\t70\t3\t0.003\t0.016\tEstes\n\t0\t0\n0.5 5\n1 0\n",
          "\n\nA8 18 70 3 0.003 0.016 Estes\n\n0 0\n   \n0.5 5\n1 0\n\n"})
    {
        const std::vector<ThrustCurveMotor> motors = loadMotors(text);
        ASSERT_EQ(motors.size(), 1U) << text;
        EXPECT_EQ(motors.front().getDigest(), kDigestA8) << text;
    }
}

TEST(RaspMotorLoader, NoMotorsInAnEmptyOrCommentOnlyFile)
{
    for (const std::string_view text : {"", "\n\n", ";a\n;b\n\n"})
    {
        const Result<std::vector<ThrustCurveMotor::Builder>> motors = load(text);
        ASSERT_TRUE(motors);
        EXPECT_TRUE(motors->empty());
    }
}

TEST(RaspMotorLoader, ReadsNumbersAsJava)
{
    // Double.parseDouble takes hexadecimal floats and type suffixes.
    const std::vector<ThrustCurveMotor> motors =
        loadMotors("A8 0x12p0 70 3 0.003d 0.016 Estes\n0 0\n0.5 5\n1 0\n");
    ASSERT_EQ(motors.size(), 1U);
    EXPECT_EQ(motors.front().getDigest(), kDigestA8);
    EXPECT_DOUBLE_EQ(motors.front().getDiameter(), 0.018);
}

TEST(RaspMotorLoader, DelayForms)
{
    const std::vector<ThrustCurveMotor> plugged =
        loadMotors("A8 18 70 plugged,3,x 0.003 0.016 Estes\n0 0\n0.5 5\n1 0\n");
    ASSERT_EQ(plugged.size(), 1U);
    EXPECT_EQ(plugged.front().getStandardDelays(), (std::vector<double>{3, kInf}));

    const std::vector<ThrustCurveMotor> placeholder =
        loadMotors("A8 18 70 100 0.003 0.016 Estes\n0 0\n0.5 5\n1 0\n");
    ASSERT_EQ(placeholder.size(), 1U);
    EXPECT_TRUE(placeholder.front().getStandardDelays().empty());

    const std::vector<ThrustCurveMotor> unsorted =
        loadMotors("A8 18 70 P,7,3,0 0.003 0.016 Estes\n0 0\n0.5 5\n1 0\n");
    ASSERT_EQ(unsorted.size(), 1U);
    EXPECT_EQ(unsorted.front().getStandardDelays(), (std::vector<double>{0, 3, 7, kInf}));
}

TEST(RaspMotorLoader, SortsTheCurve)
{
    const std::vector<ThrustCurveMotor> motors =
        loadMotors("A8 18 70 3 0.003 0.016 Estes\n0.5 5\n0.2 3\n1.0 0\n");
    ASSERT_EQ(motors.size(), 1U);
    EXPECT_EQ(motors.front().getDigest(), "31b1d0ab14e3a8c0d28eefa624b0a03e");
    EXPECT_EQ(motors.front().getTimePoints(), (std::vector<double>{0, 0.2, 0.5, 1.0}));
    EXPECT_EQ(motors.front().getThrustPoints(), (std::vector<double>{0, 3, 5, 0}));
}

TEST(RaspMotorLoader, DropsTheFirstOfTwoPointsAtTimeZero)
{
    const ThrustCurveMotor motor =
        loadOne("A8 18 70 3 0.003 0.016 Estes\n0 0\n0 4\n0.5 5\n1.0 0\n");
    EXPECT_EQ(motor.getDigest(), "9481ca293c7e7e1552812bc461c2096d");
    EXPECT_EQ(motor.getThrustPoints(), (std::vector<double>{4, 5, 0}));
}

TEST(RaspMotorLoader, DropsADuplicatePoint)
{
    EXPECT_EQ(loadOne("A8 18 70 3 0.003 0.016 Estes\n0 0\n0.5 5\n0.5 5\n1.0 0\n").getDigest(),
              kDigestA8);
}

TEST(RaspMotorLoader, DropsTheZeroOfTwoFinalPoints)
{
    // Whichever comes first.
    const ThrustCurveMotor zeroLast =
        loadOne("A8 18 70 3 0.003 0.016 Estes\n0 0\n0.5 5\n1.0 3\n1.0 0\n");
    EXPECT_EQ(zeroLast.getDigest(), "c38041eeb9583d825e19571ab2434fb2");
    EXPECT_EQ(zeroLast.getThrustPoints(), (std::vector<double>{0, 5, 3}));
    const ThrustCurveMotor zeroFirst =
        loadOne("A8 18 70 3 0.003 0.016 Estes\n0 0\n0.5 5\n1.0 0\n1.0 3\n");
    EXPECT_EQ(zeroFirst.getDigest(), "c38041eeb9583d825e19571ab2434fb2");
    EXPECT_EQ(zeroFirst.getThrustPoints(), (std::vector<double>{0, 5, 3}));
}

TEST(RaspMotorLoader, KeepsTheDelayInTheDesignationOnRequest)
{
    const Result<std::vector<ThrustCurveMotor::Builder>> builders = RaspMotorLoader().load(
        QtRocket::stringToBytes("B6-0 18 70 0 0.0062 0.0249 Estes\n0 0\n0.5 5\n1 0\n"), "x.eng",
        false);
    ASSERT_TRUE(builders);
    ASSERT_EQ(builders->size(), 1U);
    const Result<ThrustCurveMotor> motor = builders->front().build();
    ASSERT_TRUE(motor);
    EXPECT_EQ(motor->getDesignation(), "B6-0");
    EXPECT_EQ(motor->getCommonName(), "B6");
    EXPECT_EQ(motor->getDigest(), "2cf3d1b4b3b126e742448a2098f0e7f9");
}

TEST(RaspMotorLoader, ReadsIso88591)
{
    const std::vector<ThrustCurveMotor> motors =
        loadMotors(";caf\xE9\nA8 18 70 3 0.003 0.016 Est\xE9s\n0 0\n0.5 5\n1 0\n");
    ASSERT_EQ(motors.size(), 1U);
    EXPECT_EQ(motors.front().getDescription(), "caf\u00E9");
    EXPECT_EQ(motors.front().getManufacturer().getDisplayName(), "Est\u00E9s");
    EXPECT_EQ(motors.front().getMotorType(), Motor::Type::UNKNOWN);
    EXPECT_EQ(motors.front().getDigest(), kDigestA8);
}

TEST(RaspMotorLoader, RejectsAHeaderWithoutSevenFields)
{
    EXPECT_EQ(loadError("A8 18 70 3 0.003 0.016\n0 0\n1 0\n"),
              "Illegal file format. Motor header line must contain 7 fields:<br>&nbsp designation "
              "diameter length delays propellantWeight totalWeight manufacturer");
    EXPECT_EQ(loadError("A8 18 70 3 0.003 0.016 Estes extra\n0 0\n1 0\n"),
              loadError("A8 18 70 3 0.003 0.016\n0 0\n1 0\n"));
}

TEST(RaspMotorLoader, RejectsDataLinesWithoutTwoFields)
{
    constexpr std::string_view kMessage =
        "Illegal file format.<br>Data should only have 2 entries: a time and thrust value.";
    EXPECT_EQ(loadError("A8 18 70 3 0.003 0.016 Estes\n0 0 1\n1 0\n"), kMessage);
    EXPECT_EQ(loadError("A8 18 70 3 0.003 0.016 Estes\n0 0\n5\n1 0\n"), kMessage);
    // Only a ';' in the first column starts a comment.
    EXPECT_EQ(loadError("A8 18 70 3 0.003 0.016 Estes\n0 0\n0.5 5\n ;x\n1 0\n"), kMessage);
}

TEST(RaspMotorLoader, RejectsValuesThatAreNotNumbers)
{
    constexpr std::string_view kMessage =
        "Illegal file format. Could not convert value to a "
        "number.<br>Verify that each number is correctly "
        "formatted.";
    EXPECT_EQ(loadError("A8 18x 70 3 0.003 0.016 Estes\n0 0\n1 0\n"), kMessage);
    EXPECT_EQ(loadError("A8 18 70 3 0.003x 0.016 Estes\n0 0\n1 0\n"), kMessage);
    EXPECT_EQ(loadError("A8 18 70 3 0.003 0,016 Estes\n0 0\n1 0\n"), kMessage);
    EXPECT_EQ(loadError("A8 18 70 3 0.003 0.016 Estes\n0 z\n1 0\n"), kMessage);
    EXPECT_EQ(loadError("A8 18 70 3 0.003 0.016 Estes\nInf 0\n1 0\n"), kMessage);
}

TEST(RaspMotorLoader, RejectsMorePropellantThanTotalWeight)
{
    EXPECT_EQ(loadError("A8 18 70 3 0.03 0.016 Estes\n0 0\n1 0\n"),
              "Propellant weight exceeds total weight in RASP file x.eng");
}

TEST(RaspMotorLoader, RejectsTooShortCurves)
{
    constexpr std::string_view kMessage = "Illegal file format, too short thrust-curve.";
    EXPECT_EQ(loadError("A8 18 70 3 0.003 0.016 Estes\n0.5 5\n"), kMessage);
    EXPECT_EQ(loadError("A8 18 70 3 0.003 0.016 Estes\n"), kMessage);
    EXPECT_EQ(loadError("A8 18 70 3 0.003 0.016 Estes\n0 0\n1 0\n;\nB4 18 70 3 0.003 0.016 Estes"),
              kMessage);
}

TEST(RaspMotorLoader, FailsOnACurveThatCollapsesToOnePoint)
{
    // OpenRocket throws IndexOutOfBoundsException here.
    EXPECT_EQ(loadError("A8 18 70 3 0.003 0.016 Estes\n0 5\n0 5\n"),
              "Index -1 out of bounds for length 1");
}

TEST(RaspMotorLoader, LeavesInvalidCurvesToTheBuilder)
{
    // The loader accepts these; ThrustCurveMotor rejects them when built, as in OpenRocket.
    EXPECT_EQ(buildError("A8 18 70 3 0.003 0.016 Estes\n0 0\n0.5 -5\n1 0\n"), "Negative thrust.");
    // A NaN time, and a curve without impulse (NaN masses), give NaN CGs.
    EXPECT_NE(buildError("A8 18 70 3 0.003 0.016 Estes\n0 0\nNaN 5\n0.5 5\n1 0\n"), "");
    EXPECT_NE(buildError("B6-0 18 70 0 0.0062 0.0249 Estes\n0 0\n1 0\n"), "");
}

}  // namespace
