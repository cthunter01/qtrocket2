#include "QtRocket/motor/ThrustCurveMotorSet.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"

namespace
{

using QtRocket::BugError;
using QtRocket::Coordinate;
using QtRocket::Manufacturer;
using QtRocket::Motor;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustCurveMotorSet;
using MotorPtr = std::shared_ptr<const ThrustCurveMotor>;

MotorPtr share(const ThrustCurveMotor::Builder& builder)
{
    return std::make_shared<const ThrustCurveMotor>(builder.build().value());
}

/// A motor built as ThrustCurveMotorSetTest.java builds its three: manufacturer "A", F12,
/// 24 mm x 70 mm, three points, every CG point Coordinate.NUL.
ThrustCurveMotor::Builder setTestBuilder(Motor::Type type, std::vector<double> delays,
                                         std::vector<double> thrust, std::string digest)
{
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer("A"))
        .setDesignation("F12")
        .setDescription("Desc")
        .setMotorType(type)
        .setStandardDelays(std::move(delays))
        .setDiameter(0.024)
        .setLength(0.07)
        .setTimePoints({0, 1, 2})
        .setThrustPoints(std::move(thrust))
        .setCGPoints({Coordinate::kNul, Coordinate::kNul, Coordinate::kNul})
        .setDigest(std::move(digest));
    return builder;
}

/// The motors of the pinning harness: CG at half the length, the mass falling by 0.01 over the
/// points.
ThrustCurveMotor::Builder harnessBuilder(const std::string& manufacturer, std::string designation,
                                         std::string commonName, Motor::Type type,
                                         std::vector<double> delays, std::vector<double> time,
                                         std::vector<double> thrust, std::string digest,
                                         std::string description)
{
    constexpr double        kLength = 0.07;
    std::vector<Coordinate> cg;
    cg.reserve(time.size());
    for (std::size_t i = 0; i < time.size(); i++)
    {
        cg.emplace_back(kLength / 2, 0, 0,
                        0.1 - (0.01 * static_cast<double>(i) / static_cast<double>(time.size())));
    }
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer(manufacturer))
        .setDesignation(std::move(designation))
        .setCommonName(std::move(commonName))
        .setMotorType(type)
        .setStandardDelays(std::move(delays))
        .setDiameter(0.024)
        .setLength(kLength)
        .setTimePoints(std::move(time))
        .setThrustPoints(std::move(thrust))
        .setCGPoints(std::move(cg))
        .setDigest(std::move(digest))
        .setDescription(std::move(description));
    return builder;
}

/// The digests of @p set's motors, in order.
std::vector<std::string> digests(const ThrustCurveMotorSet& set)
{
    std::vector<std::string> result;
    for (const MotorPtr& motor : set.getMotors())
    {
        result.push_back(motor->getDigest());
    }
    return result;
}

// ---- Ported from ThrustCurveMotorSetTest.java ----

TEST(ThrustCurveMotorSet, Adding)
{
    const MotorPtr motor1 = share(setTestBuilder(Motor::Type::UNKNOWN, {}, {0, 1, 0}, "digestA"));
    const MotorPtr motor2 =
        share(setTestBuilder(Motor::Type::SINGLE, {5}, {0, 1, 0}, "digestB").setCommonName("F12"));
    const ThrustCurveMotor::Builder builder3 =
        setTestBuilder(Motor::Type::HYBRID, {0}, {0, 2, 0}, "digestD");
    const MotorPtr motor3 = share(builder3);

    ThrustCurveMotorSet set;

    // Test empty set
    EXPECT_EQ(set.getManufacturer(), nullptr);
    EXPECT_EQ(set.getMotors().size(), 0U);

    // Add motor1
    EXPECT_TRUE(set.matches(*motor1));
    set.addMotor(motor1);
    EXPECT_EQ(&motor1->getManufacturer(), set.getManufacturer());
    EXPECT_EQ(motor1->getDesignation(), set.getDesignation());
    EXPECT_EQ(Motor::Type::UNKNOWN, set.getType());
    EXPECT_NEAR(motor1->getDiameter(), set.getDiameter(), 0.00001);
    EXPECT_NEAR(motor1->getLength(), set.getLength(), 0.00001);
    ASSERT_EQ(1U, set.getMotors().size());
    EXPECT_EQ(motor1, set.getMotors().at(0));
    EXPECT_TRUE(set.getDelays().empty());

    // Add motor1 again
    EXPECT_TRUE(set.matches(*motor1));
    set.addMotor(motor1);
    EXPECT_EQ(&motor1->getManufacturer(), set.getManufacturer());
    EXPECT_EQ(motor1->getDesignation(), set.getDesignation());
    EXPECT_EQ(Motor::Type::UNKNOWN, set.getType());
    EXPECT_NEAR(motor1->getDiameter(), set.getDiameter(), 0.00001);
    EXPECT_NEAR(motor1->getLength(), set.getLength(), 0.00001);
    ASSERT_EQ(1U, set.getMotors().size());
    EXPECT_EQ(motor1, set.getMotors().at(0));
    EXPECT_TRUE(set.getDelays().empty());

    // Add motor2
    EXPECT_TRUE(set.matches(*motor2));
    set.addMotor(motor2);
    EXPECT_EQ(&motor2->getManufacturer(), set.getManufacturer());
    EXPECT_EQ(motor2->getCommonName(), set.getCommonName());
    EXPECT_EQ(Motor::Type::SINGLE, set.getType());
    EXPECT_NEAR(motor2->getDiameter(), set.getDiameter(), 0.00001);
    EXPECT_NEAR(motor2->getLength(), set.getLength(), 0.00001);
    ASSERT_EQ(2U, set.getMotors().size());
    EXPECT_EQ(motor1, set.getMotors().at(0));
    EXPECT_EQ(motor2, set.getMotors().at(1));
    EXPECT_EQ(set.getDelays(), (std::vector<double>{5.0}));

    // Test that adding motor3 fails
    EXPECT_FALSE(set.matches(*motor3));
    EXPECT_THROW(set.addMotor(motor3), BugError);
    EXPECT_EQ(set.getMotorCount(), 2U);
}

// ---- Values pinned by running OpenRocket's ThrustCurveMotorSet ----

TEST(ThrustCurveMotorSet, DelaysOrderAndTypeMatchOpenRocket)
{
    const MotorPtr m1 = share(harnessBuilder("A", "F12", "F12", Motor::Type::UNKNOWN,
                                             {6, 2.4, -0.4}, {0, 1, 2}, {0, 1, 0}, "d1", "Desc"));
    const MotorPtr m2 =
        share(harnessBuilder("A", "F12", "F12", Motor::Type::HYBRID, {0.5, 1.5, 2.5, -0.0},
                             {0, 1, 1.5, 2}, {0, 1, 1, 0}, "d2", "Desc"));
    const MotorPtr m3 = share(harnessBuilder("A", "F12", "F12", Motor::Type::UNKNOWN,
                                             {std::numeric_limits<double>::quiet_NaN()}, {0, 1, 2},
                                             {0, 1, 0}, "d3", "A much longer description"));
    ThrustCurveMotorSet set;
    set.addMotor(m1);
    set.addMotor(m2);
    set.addMotor(m3);

    // [-0.0, 0.0, 2.0, 6.0, Infinity, NaN]: rounded half to even, -0.0 and 0.0 kept apart, the
    // plugged delay of the hybrid type, NaN last.
    const std::vector<double>& delays = set.getDelays();
    ASSERT_EQ(delays.size(), 6U);
    EXPECT_EQ(delays.at(0), 0.0);
    EXPECT_TRUE(std::signbit(delays.at(0)));
    EXPECT_EQ(delays.at(1), 0.0);
    EXPECT_FALSE(std::signbit(delays.at(1)));
    EXPECT_EQ(delays.at(2), 2.0);
    EXPECT_EQ(delays.at(3), 6.0);
    EXPECT_EQ(delays.at(4), Motor::kPluggedDelay);
    EXPECT_TRUE(std::isnan(delays.at(5)));

    EXPECT_EQ(set.getType(), Motor::Type::HYBRID);
    EXPECT_EQ(set.toString(), "ThrustCurveMotorSet[AeroTech F12, type=Hybrid, count=3]");
    // More points first, then the longer description.
    EXPECT_EQ(digests(set), (std::vector<std::string>{"d2", "d3", "d1"}));
    EXPECT_EQ(set.getTotalImpulse(), 1);
}

TEST(ThrustCurveMotorSet, EmptySet)
{
    const ThrustCurveMotorSet set;
    EXPECT_EQ(set.toString(), "ThrustCurveMotorSet[null null, type=Unknown, count=0]");
    EXPECT_EQ(set.getManufacturer(), nullptr);
    EXPECT_EQ(set.getDesignation(), "");
    EXPECT_EQ(set.getCommonName(), "");
    EXPECT_EQ(set.getCaseInfo(), "");
    EXPECT_EQ(set.getDiameter(), -1);
    EXPECT_EQ(set.getLength(), -1);
    EXPECT_EQ(set.getTotalImpulse(), 0);
    EXPECT_EQ(set.getType(), Motor::Type::UNKNOWN);
    EXPECT_TRUE(set.isAvailable());
    EXPECT_EQ(set.getMotorCount(), 0U);
    EXPECT_TRUE(set.getDelays().empty());
}

// ---- Further behaviour of OpenRocket's ThrustCurveMotorSet ----

TEST(ThrustCurveMotorSet, DuplicateDigestKeepsTheBetterDescription)
{
    auto withDescription = [](std::string digest, std::string description) {
        return share(harnessBuilder("A", "F12", "F12", Motor::Type::SINGLE, {}, {0, 1, 2},
                                    {0, 1, 0}, std::move(digest), std::move(description)));
    };

    ThrustCurveMotorSet set;
    const MotorPtr      noComment = withDescription("dup", "");
    set.addMotor(noComment);

    // An empty old comment is replaced in place.
    const MotorPtr commented = withDescription("dup", "From  the\tmanufacturer");
    set.addMotor(commented);
    ASSERT_EQ(set.getMotorCount(), 1U);
    EXPECT_EQ(set.getMotors().front(), commented);

    // The same comment up to whitespace, or none at all, is dropped.
    set.addMotor(withDescription("dup", " From the manufacturer\n"));
    set.addMotor(withDescription("dup", "   "));
    ASSERT_EQ(set.getMotorCount(), 1U);
    EXPECT_EQ(set.getMotors().front(), commented);

    // A different comment keeps both, the longer comment first.
    const MotorPtr other = withDescription("dup", "Another measurement, much longer");
    set.addMotor(other);
    EXPECT_EQ(set.getMotors(), (std::vector<MotorPtr>{other, commented}));

    // Another digest is always added.
    const MotorPtr second = withDescription("other", "");
    set.addMotor(second);
    EXPECT_EQ(set.getMotors(), (std::vector<MotorPtr>{other, commented, second}));
}

TEST(ThrustCurveMotorSet, SameDigestDifferentDesignationCaseIsAdded)
{
    ThrustCurveMotorSet set;
    set.addMotor(share(harnessBuilder("A", "F12T", "F12", Motor::Type::SINGLE, {}, {0, 1, 2},
                                      {0, 1, 0}, "dup", "")));
    // The set matches designations ignoring case, but a present motor must have the very same
    // designation; String.compareTo puts upper case first.
    const MotorPtr lower = share(harnessBuilder("A", "f12t", "F12", Motor::Type::SINGLE, {},
                                                {0, 1, 2}, {0, 1, 0}, "dup", ""));
    EXPECT_TRUE(set.matches(*lower));
    set.addMotor(lower);
    ASSERT_EQ(set.getMotorCount(), 2U);
    EXPECT_EQ(set.getMotors().back(), lower);
}

/// A set of one motor with a case, to test which variants of that motor match it.
class ThrustCurveMotorSetMatchingTest : public ::testing::Test
{
protected:
    ThrustCurveMotorSetMatchingTest()
    {
        m_base.setCaseInfo("RMS-24/40");
        m_set.addMotor(share(m_base));
    }

    [[nodiscard]] bool matches(const ThrustCurveMotor::Builder& builder) const
    {
        return m_set.matches(builder.build().value());
    }

    /// A copy of the motor's builder to change.
    [[nodiscard]] ThrustCurveMotor::Builder variant() const { return m_base; }

    ThrustCurveMotor::Builder m_base =
        harnessBuilder("A", "F12", "F12", Motor::Type::SINGLE, {}, {0, 1, 2}, {0, 1, 0}, "m", "");
    ThrustCurveMotorSet m_set;
};

TEST_F(ThrustCurveMotorSetMatchingTest, TheSameMotorMatches)
{
    EXPECT_EQ(m_set.getCaseInfo(), "RMS-24/40");
    EXPECT_TRUE(matches(m_base));
}

TEST_F(ThrustCurveMotorSetMatchingTest, ManufacturerMustBeTheSameObject)
{
    EXPECT_FALSE(matches(variant().setManufacturer(Manufacturer::getManufacturer("Estes"))));
    // An alias of the same manufacturer gives the same object.
    EXPECT_TRUE(matches(variant().setManufacturer(Manufacturer::getManufacturer("AT-RMS"))));
}

TEST_F(ThrustCurveMotorSetMatchingTest, DimensionsWithinMathUtilTolerance)
{
    // Dimensions equal within MathUtil's relative 1e-8, no further.
    EXPECT_TRUE(matches(variant().setDiameter(0.024 * (1 + 1e-9))));
    EXPECT_FALSE(matches(variant().setDiameter(0.0241)));
    EXPECT_FALSE(matches(variant().setLength(0.0701)));
}

TEST_F(ThrustCurveMotorSetMatchingTest, TypesAgreeUnlessOneIsUnknown)
{
    EXPECT_TRUE(matches(variant().setMotorType(Motor::Type::UNKNOWN)));
    EXPECT_FALSE(matches(variant().setMotorType(Motor::Type::RELOAD)));
}

TEST_F(ThrustCurveMotorSetMatchingTest, NamesAndCaseInfoIgnoreCase)
{
    EXPECT_TRUE(matches(variant().setDesignation("f12")));
    EXPECT_FALSE(matches(variant().setDesignation("F12T")));
    EXPECT_FALSE(matches(variant().setCommonName("F13")));
    EXPECT_TRUE(matches(variant().setCaseInfo("rms-24/40")));
    EXPECT_FALSE(matches(variant().setCaseInfo("")));
}

TEST(ThrustCurveMotorSet, UnknownFirstMotorTakesTheNextType)
{
    ThrustCurveMotorSet set;
    set.addMotor(share(harnessBuilder("A", "F12", "F12", Motor::Type::UNKNOWN, {}, {0, 1, 2},
                                      {0, 1, 0}, "u", "")));
    EXPECT_EQ(set.getType(), Motor::Type::UNKNOWN);
    set.addMotor(share(
        harnessBuilder("A", "F12", "F12", Motor::Type::RELOAD, {}, {0, 1, 2}, {0, 1, 0}, "r", "")));
    EXPECT_EQ(set.getType(), Motor::Type::RELOAD);
    // An unknown motor still matches; the type stays.
    set.addMotor(share(harnessBuilder("A", "F12", "F12", Motor::Type::UNKNOWN, {}, {0, 1, 2},
                                      {0, 1, 0}, "u2", "")));
    EXPECT_EQ(set.getType(), Motor::Type::RELOAD);
    EXPECT_EQ(set.getMotorCount(), 3U);
    // Not hybrid, so no plugged delay.
    EXPECT_TRUE(set.getDelays().empty());
}

TEST(ThrustCurveMotorSet, FirstMotorFixesTheSetsProperties)
{
    ThrustCurveMotor::Builder builder =
        harnessBuilder("Estes", "C6-5", "C6-5", Motor::Type::SINGLE, {5, 3, 5}, {0, 0.5, 1.8},
                       {0, 14, 0}, "e", "");
    builder.setAvailability(false).setCaseInfo("none");
    ThrustCurveMotorSet set;
    set.addMotor(share(builder));
    EXPECT_EQ(set.getManufacturer(), &Manufacturer::getManufacturer("Estes"));
    EXPECT_EQ(set.getDesignation(), "C6-5");
    EXPECT_EQ(set.getCommonName(), "C6");
    EXPECT_EQ(set.getDiameter(), 0.024);
    EXPECT_EQ(set.getLength(), 0.07);
    EXPECT_EQ(set.getTotalImpulse(), 13);  // Math.round(12.6)
    EXPECT_EQ(set.getCaseInfo(), "none");
    EXPECT_FALSE(set.isAvailable());
    EXPECT_EQ(set.getDelays(), (std::vector<double>{3, 5}));
}

/// A set of one motor with the given identity and diameter.
ThrustCurveMotorSet setOf(const std::string& manufacturer, std::string designation, double diameter)
{
    ThrustCurveMotor::Builder builder =
        harnessBuilder(manufacturer, std::move(designation), "", Motor::Type::SINGLE, {}, {0, 1, 2},
                       {0, 1, 0}, "x", "");
    builder.setDiameter(diameter);
    ThrustCurveMotorSet set;
    set.addMotor(share(builder));
    return set;
}

TEST(ThrustCurveMotorSet, CompareTo)
{
    const ThrustCurveMotorSet aeroTechF12 = setOf("A", "F12", 0.024);
    const ThrustCurveMotorSet aeroTechG80 = setOf("A", "G80", 0.024);
    const ThrustCurveMotorSet estesC6     = setOf("Estes", "C6", 0.018);
    const ThrustCurveMotorSet wideF12     = setOf("A", "F12", 0.029);

    EXPECT_EQ(aeroTechF12.compareTo(aeroTechF12), 0);
    EXPECT_LT(aeroTechF12.compareTo(estesC6), 0);
    EXPECT_GT(estesC6.compareTo(aeroTechG80), 0);
    EXPECT_LT(aeroTechF12.compareTo(aeroTechG80), 0);
    // Double.compare of the diameters: -1, 0 or 1.
    EXPECT_EQ(aeroTechF12.compareTo(wideF12), -1);
    EXPECT_EQ(wideF12.compareTo(aeroTechF12), 1);
}

TEST(ThrustCurveMotorSet, CompareToAnEmptySetIsABug)
{
    const ThrustCurveMotorSet aeroTechF12 = setOf("A", "F12", 0.024);
    const ThrustCurveMotorSet empty;
    EXPECT_THROW(static_cast<void>(empty.compareTo(aeroTechF12)), BugError);
    EXPECT_THROW(static_cast<void>(aeroTechF12.compareTo(empty)), BugError);
}

TEST(ThrustCurveMotorSet, NullMotorIsABug)
{
    ThrustCurveMotorSet set;
    EXPECT_THROW(set.addMotor(nullptr), BugError);
    EXPECT_EQ(set.getMotorCount(), 0U);
}

}  // namespace
