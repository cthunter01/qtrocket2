#include "QtRocket/file/motor/RockSimMotorWriter.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/motor/RaspMotorLoader.h"
#include "QtRocket/file/motor/RockSimMotorLoader.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorDigest.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"

namespace
{

using QtRocket::Coordinate;
using QtRocket::Manufacturer;
using QtRocket::Motor;
using QtRocket::MotorDigest;
using QtRocket::Result;
using QtRocket::RockSimMotorLoader;
using QtRocket::RockSimMotorWriter;
using QtRocket::ThrustCurveMotor;

constexpr double kPlugged = Motor::kPluggedDelay;

/// The motor of TestMotorLoader.testRockSimMotorWriterRoundTrip.
[[nodiscard]] ThrustCurveMotor makeG80(Motor::Type type)
{
    ThrustCurveMotor::Builder builder;
    // Estes has a known default type, so UNKNOWN only survives if it was explicit.
    builder.setManufacturer(Manufacturer::getManufacturer("Estes"))
        .setDesignation("G80")
        .setDescription("Test motor")
        .setMotorType(type)
        .setStandardDelays({4, 7, kPlugged})
        .setDiameter(0.029)
        .setLength(0.124)
        .setTimePoints({0, 0.5, 1.0, 1.5, 2.0})
        .setThrustPoints({0, 80, 75, 40, 0})
        .setCGPoints({Coordinate(0.062, 0, 0, 0.100), Coordinate(0.060, 0, 0, 0.085),
                      Coordinate(0.058, 0, 0, 0.070), Coordinate(0.055, 0, 0, 0.055),
                      Coordinate(0.050, 0, 0, 0.040)})
        .setDigest("test_digest_abc");
    return builder.build().value();
}

/// The single motor RockSimMotorLoader reads from @p text.
[[nodiscard]] ThrustCurveMotor reload(std::string_view text)
{
    const Result<std::vector<ThrustCurveMotor::Builder>> motors =
        RockSimMotorLoader().load(QtRocket::stringToBytes(text), "test.rse");
    EXPECT_TRUE(motors) << motors.error().toString();
    EXPECT_EQ(motors.value().size(), 1U) << "Expected exactly one motor from round-trip";
    return motors.value().front().build().value();
}

/// The single motor of the test file @p name.
template <class Loader>
[[nodiscard]] ThrustCurveMotor loadTestFile(std::string_view name)
{
    const Result<std::vector<std::byte>> bytes =
        QtRocket::readFile(QtRocket::Test::testDataDir() / "motors" / name);
    EXPECT_TRUE(bytes);
    const Result<std::vector<ThrustCurveMotor::Builder>> motors =
        Loader().load(bytes.value(), name);
    EXPECT_TRUE(motors);
    EXPECT_EQ(motors.value().size(), 1U);
    return motors.value().front().build().value();
}

/// The points where @p actual differs from @p expected by more than @p tolerance ("i: e vs a"),
/// or "" when none does; a length mismatch is one too.
[[nodiscard]] std::string mismatches(const std::vector<double>& expected,
                                     const std::vector<double>& actual, double tolerance)
{
    if (expected.size() != actual.size())
    {
        return std::format("{} points vs {}", expected.size(), actual.size());
    }
    std::string text;
    for (std::size_t i = 0; i < expected.size(); i++)
    {
        if (!(std::abs(expected[i] - actual[i]) <= tolerance))
        {
            text += std::format("{}: {} vs {}; ", i, expected[i], actual[i]);
        }
    }
    return text;
}

// TestMotorLoader.testRockSimMotorWriterRoundTrip, for every Motor.Type
class RockSimMotorWriterRoundTrip : public ::testing::TestWithParam<Motor::Type>
{ };

TEST_P(RockSimMotorWriterRoundTrip, KeepsTheMotor)
{
    const ThrustCurveMotor original = makeG80(GetParam());

    const ThrustCurveMotor loaded = reload(RockSimMotorWriter::write(original));

    // Verify key properties survived the round-trip
    EXPECT_EQ(original.getDesignation(), loaded.getDesignation());
    EXPECT_EQ(original.getManufacturer().getSimpleName(), loaded.getManufacturer().getSimpleName());
    EXPECT_EQ(original.getMotorType(), loaded.getMotorType());
    EXPECT_EQ(original.getTimePoints().size(), loaded.getTimePoints().size());
    EXPECT_NEAR(original.getDiameter(), loaded.getDiameter(), 1e-6);
    EXPECT_NEAR(original.getLength(), loaded.getLength(), 1e-6);
    EXPECT_NEAR(original.getLaunchMass(), loaded.getLaunchMass(), 1e-6);
    EXPECT_LT(std::abs(original.getPropellantMass() - loaded.getPropellantMass()), 1e-4)
        << "Propellant mass mismatch";

    // Verify thrust curve data
    EXPECT_EQ(mismatches(original.getTimePoints(), loaded.getTimePoints(), 1e-6), "")
        << "Time mismatch";
    EXPECT_EQ(mismatches(original.getThrustPoints(), loaded.getThrustPoints(), 1e-3), "")
        << "Thrust mismatch";
}

TEST_P(RockSimMotorWriterRoundTrip, KeepsTheDigestDelaysAndDescription)
{
    const ThrustCurveMotor original = makeG80(GetParam());
    const ThrustCurveMotor loaded   = reload(RockSimMotorWriter::write(original));

    // Beyond OpenRocket's test: the digest of what is read back is the motor's own.
    EXPECT_EQ(loaded.getDigest(), MotorDigest::digestMotor(original));
    EXPECT_EQ(loaded.getDigest(), "c001360a9c5888956976339228ee7158");
    EXPECT_EQ(loaded.getStandardDelays(), (std::vector<double>{4, 7, kPlugged}));
    EXPECT_EQ(loaded.getDescription(), "Test motor");
}

INSTANTIATE_TEST_SUITE_P(EveryMotorType, RockSimMotorWriterRoundTrip,
                         ::testing::ValuesIn(Motor::kAllTypes),
                         [](const ::testing::TestParamInfo<Motor::Type>& typeInfo) {
                             return std::string(QtRocket::enumName(typeInfo.param));
                         });

// TestMotorLoader.testRockSimMotorWriterRoundTripFromFile
TEST(RockSimMotorWriter, RoundTripFromFile)
{
    // Load an existing .rse file
    const ThrustCurveMotor original = loadTestFile<RockSimMotorLoader>("test2.rse");

    // Write it back out and parse it back
    const ThrustCurveMotor reloaded = reload(RockSimMotorWriter::write(original));

    // Verify key properties
    EXPECT_EQ(original.getDesignation(), reloaded.getDesignation());
    EXPECT_EQ(original.getTimePoints().size(), reloaded.getTimePoints().size());
    EXPECT_NEAR(original.getDiameter(), reloaded.getDiameter(), 1e-6);
    EXPECT_NEAR(original.getLength(), reloaded.getLength(), 1e-6);
    EXPECT_NEAR(original.getLaunchMass(), reloaded.getLaunchMass(), 1e-4);
}

TEST(RockSimMotorWriter, WritesOpenRocketsText)
{
    // RockSimMotorWriter.write() of the round-trip motor, from OpenRocket 5f164fd0e.
    EXPECT_EQ(RockSimMotorWriter::write(makeG80(Motor::Type::SINGLE)),
              "<engine-database>\n"
              " <engine-list>\n"
              "  <engine mfg=\"Estes\" code=\"G80\" Type=\"single-use\" dia=\"29\" len=\"124\" "
              "initWt=\"100\" propWt=\"60\" delays=\"4,7,1000\" auto-calc-mass=\"0\" "
              "auto-calc-cg=\"0\">\n"
              "   <comments>Test motor</comments>\n"
              "   <data>\n"
              "    <eng-data t=\"0\" f=\"0\" m=\"100\" cg=\"62\"/>\n"
              "    <eng-data t=\"0.5\" f=\"80\" m=\"85\" cg=\"60\"/>\n"
              "    <eng-data t=\"1\" f=\"75\" m=\"70\" cg=\"58\"/>\n"
              "    <eng-data t=\"1.5\" f=\"40\" m=\"55\" cg=\"55\"/>\n"
              "    <eng-data t=\"2\" f=\"0\" m=\"40\" cg=\"50\"/>\n"
              "   </data>\n"
              "  </engine>\n"
              " </engine-list>\n"
              "</engine-database>\n");
}

TEST(RockSimMotorWriter, EscapesTextAndRoundsNumbersAsOpenRocket)
{
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer("A&B \"Rockets\" <x>"))
        .setDesignation("Q1<2>&\"")
        .setDescription("line <1> & \"two\"\nthree")
        .setMotorType(Motor::Type::UNKNOWN)
        .setDiameter(0.0123456)
        .setLength(12.5)
        .setTimePoints({0, 0.0004, 1.23456789})
        .setThrustPoints({0, 12345.678, 0})
        .setCGPoints({Coordinate(0.0000005, 0, 0, 20.5), Coordinate(6.25, 0, 0, 10.00004),
                      Coordinate(6.25, 0, 0, 0)});
    const Result<ThrustCurveMotor> motor = builder.build();
    ASSERT_TRUE(motor) << motor.error().toString();

    // From OpenRocket 5f164fd0e: no delays attribute without delays, three decimals, and four
    // significant digits in exponential notation below 0.001 and from 10000.
    EXPECT_EQ(RockSimMotorWriter::write(*motor),
              "<engine-database>\n"
              " <engine-list>\n"
              "  <engine mfg=\"A&amp;B &quot;Rockets&quot; &lt;x&gt;\" "
              "code=\"Q1&lt;2&gt;&amp;&quot;\" Type=\"unknown\" dia=\"12.346\" len=\"1.25e4\" "
              "initWt=\"2.05e4\" propWt=\"2.05e4\" auto-calc-mass=\"0\" auto-calc-cg=\"0\">\n"
              "   <comments>line &lt;1&gt; &amp; \"two\"\nthree</comments>\n"
              "   <data>\n"
              "    <eng-data t=\"0\" f=\"0\" m=\"2.05e4\" cg=\"5e-4\"/>\n"
              "    <eng-data t=\"4e-4\" f=\"1.235e4\" m=\"1e4\" cg=\"6250\"/>\n"
              "    <eng-data t=\"1.235\" f=\"0\" m=\"0\" cg=\"6250\"/>\n"
              "   </data>\n"
              "  </engine>\n"
              " </engine-list>\n"
              "</engine-database>\n");

    // What is read back has the escaped text restored.
    const ThrustCurveMotor loaded = reload(RockSimMotorWriter::write(*motor));
    EXPECT_EQ(loaded.getManufacturer().getSimpleName(), "A&B \"Rockets\" <x>");
    EXPECT_EQ(loaded.getDesignation(), "Q1<2>&\"");
    EXPECT_EQ(loaded.getDescription(), "line <1> & \"two\"\nthree");
    EXPECT_EQ(loaded.getMotorType(), Motor::Type::UNKNOWN);
}

TEST(RockSimMotorWriter, OmitsAnEmptyDescription)
{
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer("Estes"))
        .setDesignation("A8")
        .setMotorType(Motor::Type::SINGLE)
        .setDiameter(0.018)
        .setLength(0.07)
        .setTimePoints({0, 1})
        .setThrustPoints({5, 0})
        .setCGPoints({Coordinate(0.035, 0, 0, 0.016), Coordinate(0.035, 0, 0, 0.013)});
    const std::string text = RockSimMotorWriter::write(builder.build().value());
    EXPECT_FALSE(text.contains("<comments>"));
    EXPECT_FALSE(text.contains("delays="));
    EXPECT_TRUE(text.contains(R"(initWt="16" propWt="3" auto-calc-mass="0")"));
}

TEST(RockSimMotorWriter, WritesTest2AsOpenRocketAndKeepsItsDigest)
{
    const ThrustCurveMotor original = loadTestFile<RockSimMotorLoader>("test2.rse");
    const std::string      text     = RockSimMotorWriter::write(original);

    // Lines of OpenRocket's output for the same motor.
    EXPECT_TRUE(text.starts_with(
        "<engine-database>\n <engine-list>\n  <engine mfg=\"Cesaroni Technology\" "
        "code=\"G115-13A\" Type=\"reloadable\" dia=\"38\" len=\"127\" initWt=\"195\" "
        "propWt=\"61.8\" delays=\"13\" auto-calc-mass=\"0\" auto-calc-cg=\"0\">\n   <data>\n"
        "    <eng-data t=\"0\" f=\"0\" m=\"195\" cg=\"63.5\"/>\n"
        "    <eng-data t=\"0.008\" f=\"10.345\" m=\"194.982\" cg=\"63.5\"/>\n"))
        << text;
    EXPECT_TRUE(
        text.contains("    <eng-data t=\"1.186\" f=\"96.551\" m=\"134.03\" cg=\"63.5\"/>\n"));
    EXPECT_TRUE(
        text.ends_with("    <eng-data t=\"1.24\" f=\"0\" m=\"133.2\" cg=\"63.5\"/>\n"
                       "   </data>\n  </engine>\n </engine-list>\n</engine-database>\n"));

    // Written with explicit masses and CGs, the reloaded motor digests them point by point: the
    // same digest as the original's data, which OpenRocket accepts for the original's digest.
    const ThrustCurveMotor reloaded = reload(text);
    EXPECT_EQ(reloaded.getDigest(), "b2fe203ee319ae28b9ccdad26a8f21de");
    EXPECT_EQ(reloaded.getDigest(), MotorDigest::digestMotor(original));
    EXPECT_TRUE(MotorDigest::isDigestCompatible(reloaded, original.getDigest()));
}

TEST(RockSimMotorWriter, RaspMotorSurvivesTheRoundTrip)
{
    const ThrustCurveMotor original = loadTestFile<QtRocket::RaspMotorLoader>("test1.eng");
    const std::string      text     = RockSimMotorWriter::write(original);
    EXPECT_TRUE(text.contains(R"(<engine mfg="AeroTech" code="D10" Type="unknown" dia="18" )"
                              R"(len="70" initWt="25.9" propWt="9.8" delays="7" )"))
        << text;
    EXPECT_TRUE(text.contains(R"(<eng-data t="0.007" f="23" m="25.858" cg="35"/>)"));

    const ThrustCurveMotor reloaded = reload(text);
    EXPECT_EQ(reloaded.getDigest(), "c056cf25df6751f7bb8a94bc4f64750f");
    EXPECT_EQ(reloaded.getDigest(), MotorDigest::digestMotor(original));
    // The .ork loader accepts it for the RASP digest (TIME_ARRAY, MASS_SPECIFIC, FORCE_PER_TIME).
    EXPECT_TRUE(MotorDigest::isDigestCompatible(reloaded, "e523030bc96d5e63313b5723aaea267d"));
}

}  // namespace
