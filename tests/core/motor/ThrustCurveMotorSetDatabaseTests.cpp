#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorDatabase.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"

namespace
{

using QtRocket::BugError;
using QtRocket::Coordinate;
using QtRocket::Manufacturer;
using QtRocket::Motor;
using QtRocket::MotorDatabase;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustCurveMotorSet;
using QtRocket::ThrustCurveMotorSetDatabase;
using MotorPtr = std::shared_ptr<const ThrustCurveMotor>;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// A motor built as the pinning harness builds them: CG at half the length, the mass falling by
/// 0.01 over the points.
MotorPtr harnessMotor(const std::string& manufacturer, std::string designation,
                      std::string commonName, Motor::Type type, std::vector<double> delays,
                      double diameter, double length, std::vector<double> time,
                      std::vector<double> thrust, std::string digest, std::string description)
{
    std::vector<Coordinate> cg;
    cg.reserve(time.size());
    for (std::size_t i = 0; i < time.size(); i++)
    {
        cg.emplace_back(length / 2, 0, 0,
                        0.1 - (0.01 * static_cast<double>(i) / static_cast<double>(time.size())));
    }
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer(manufacturer))
        .setDesignation(std::move(designation))
        .setCommonName(std::move(commonName))
        .setMotorType(type)
        .setStandardDelays(std::move(delays))
        .setDiameter(diameter)
        .setLength(length)
        .setTimePoints(std::move(time))
        .setThrustPoints(std::move(thrust))
        .setCGPoints(std::move(cg))
        .setDigest(std::move(digest))
        .setDescription(std::move(description));
    return std::make_shared<const ThrustCurveMotor>(builder.build().value());
}

/// "simple name:designation/digest" of each motor, space-separated, as the harness printed them.
std::string names(const std::vector<MotorPtr>& motors)
{
    std::string out;
    for (const MotorPtr& motor : motors)
    {
        if (!out.empty())
        {
            out += ' ';
        }
        out += motor->getManufacturer().getSimpleName() + ":" + motor->getDesignation() + "/" +
               motor->getDigest();
    }
    return out;
}

/// The database of the pinning harness.
class ThrustCurveMotorSetDatabaseTest : public ::testing::Test
{
protected:
    ThrustCurveMotorSetDatabaseTest()
    {
        m_database.addMotor(harnessMotor("A", "F12", "F12", Motor::Type::UNKNOWN, {6, 2.4, -0.4},
                                         0.024, 0.07, {0, 1, 2}, {0, 1, 0}, "d1", "Desc"));
        m_database.addMotor(harnessMotor("A", "F12", "F12", Motor::Type::HYBRID,
                                         {0.5, 1.5, 2.5, -0.0}, 0.024, 0.07, {0, 1, 1.5, 2},
                                         {0, 1, 1, 0}, "d2", "Desc"));
        m_database.addMotor(harnessMotor("A", "F12", "F12", Motor::Type::UNKNOWN, {kNaN}, 0.024,
                                         0.07, {0, 1, 2}, {0, 1, 0}, "d3",
                                         "A much longer description"));
        m_database.addMotor(harnessMotor("Estes", "C6-5", "C6", Motor::Type::SINGLE, {0, 3, 5},
                                         0.018, 0.07, {0, 0.5, 1.8}, {0, 14, 0}, "e1", ""));
        m_database.addMotor(harnessMotor("Estes", "C6-5", "C6", Motor::Type::SINGLE, {7}, 0.018,
                                         0.07, {0, 0.4, 1.9}, {0, 13, 0}, "e2", ""));
        m_database.addMotor(harnessMotor("CTI", "G80-7A", "G80", Motor::Type::RELOAD, {7}, 0.029,
                                         0.124, {0, 0.5, 1.5}, {0, 100, 0}, "c1", ""));
        m_database.addMotor(harnessMotor("AeroTech", "G80T", "G80", Motor::Type::SINGLE, {4, 7, 10},
                                         0.029, 0.124, {0, 0.5, 1.5}, {0, 110, 0}, "a1", ""));
    }

    [[nodiscard]] std::string find(std::optional<std::string_view> digest,
                                   std::optional<Motor::Type>      type,
                                   std::optional<std::string_view> manufacturer,
                                   std::optional<std::string_view> designation, double diameter,
                                   double length) const
    {
        return names(m_database.findThrustCurveMotors(digest, type, manufacturer, designation,
                                                      diameter, length));
    }

    ThrustCurveMotorSetDatabase m_database;
};

// ---- Values pinned by running OpenRocket's ThrustCurveMotorSetDatabase ----

TEST_F(ThrustCurveMotorSetDatabaseTest, SetsMatchOpenRocket)
{
    const std::vector<ThrustCurveMotorSet>& sets = m_database.getMotorSets();
    ASSERT_EQ(sets.size(), 4U);
    EXPECT_EQ(sets.at(0).toString(), "ThrustCurveMotorSet[AeroTech F12, type=Hybrid, count=3]");
    EXPECT_EQ(sets.at(1).toString(), "ThrustCurveMotorSet[Estes C6-5, type=Single-use, count=2]");
    EXPECT_EQ(sets.at(1).getDelays(), (std::vector<double>{0, 3, 5, 7}));
    EXPECT_EQ(sets.at(2).toString(),
              "ThrustCurveMotorSet[Cesaroni Technology Inc. G80-7A, type=Reloadable, count=1]");
    EXPECT_EQ(sets.at(2).getDelays(), (std::vector<double>{7}));
    EXPECT_EQ(sets.at(3).toString(),
              "ThrustCurveMotorSet[AeroTech G80T, type=Single-use, count=1]");
    EXPECT_EQ(sets.at(3).getDelays(), (std::vector<double>{4, 7, 10}));
}

TEST_F(ThrustCurveMotorSetDatabaseTest, FindMotorsMatchesOpenRocket)
{
    EXPECT_EQ(find(std::nullopt, std::nullopt, std::nullopt, std::nullopt, kNaN, kNaN),
              "AeroTech:F12/d2 AeroTech:F12/d3 AeroTech:F12/d1 Estes:C6-5/e1 Estes:C6-5/e2 "
              "Cesaroni Technology:G80-7A/c1 AeroTech:G80T/a1");
    EXPECT_EQ(find("e2", std::nullopt, std::nullopt, std::nullopt, kNaN, kNaN), "Estes:C6-5/e2");
    // The digest alone wins when the description does not match it.
    EXPECT_EQ(find("e2", std::nullopt, "AT", std::nullopt, kNaN, kNaN), "Estes:C6-5/e2");
    EXPECT_EQ(find("e2", std::nullopt, "Estes", std::nullopt, kNaN, kNaN), "Estes:C6-5/e2");
    EXPECT_EQ(find(std::nullopt, Motor::Type::RELOAD, std::nullopt, std::nullopt, kNaN, kNaN),
              "Cesaroni Technology:G80-7A/c1");
    // The set's type counts: d1 and d3 are UNKNOWN themselves.
    EXPECT_EQ(find(std::nullopt, Motor::Type::HYBRID, std::nullopt, std::nullopt, kNaN, kNaN),
              "AeroTech:F12/d2 AeroTech:F12/d3 AeroTech:F12/d1");
    EXPECT_EQ(find(std::nullopt, std::nullopt, std::nullopt, "g80", kNaN, kNaN),
              "Cesaroni Technology:G80-7A/c1 AeroTech:G80T/a1");
    // A designation that contains the common name matches too.
    EXPECT_EQ(find(std::nullopt, std::nullopt, std::nullopt, "G80-7A-14", kNaN, kNaN),
              "Cesaroni Technology:G80-7A/c1 AeroTech:G80T/a1");
    EXPECT_EQ(find(std::nullopt, std::nullopt, std::nullopt, std::nullopt, 0.0235, kNaN),
              "AeroTech:F12/d2 AeroTech:F12/d3 AeroTech:F12/d1");
    EXPECT_EQ(find(std::nullopt, std::nullopt, std::nullopt, std::nullopt, 0.0239, 0.0751), "");
    EXPECT_EQ(find("zz", std::nullopt, "nobody", std::nullopt, kNaN, kNaN), "");
}

// ---- Further behaviour ----

TEST_F(ThrustCurveMotorSetDatabaseTest, FullMatchesComeFirst)
{
    // Both the digest and the description match: only those motors.
    EXPECT_EQ(find("a1", std::nullopt, "aerotech", "G80", kNaN, kNaN), "AeroTech:G80T/a1");
    // No digest given: the description matches, whatever the digests.
    EXPECT_EQ(find(std::nullopt, std::nullopt, "aerotech", "G80", kNaN, kNaN), "AeroTech:G80T/a1");
    // A digest that matches nothing: the description decides.
    EXPECT_EQ(find("nothing", std::nullopt, "Estes", std::nullopt, kNaN, kNaN),
              "Estes:C6-5/e1 Estes:C6-5/e2");
}

TEST_F(ThrustCurveMotorSetDatabaseTest, DimensionTolerance)
{
    // Within 5 mm either way.
    EXPECT_EQ(find(std::nullopt, std::nullopt, std::nullopt, std::nullopt, 0.0229, kNaN),
              "AeroTech:F12/d2 AeroTech:F12/d3 AeroTech:F12/d1 Estes:C6-5/e1 Estes:C6-5/e2");
    EXPECT_EQ(find(std::nullopt, std::nullopt, std::nullopt, std::nullopt, kNaN, 0.12),
              "Cesaroni Technology:G80-7A/c1 AeroTech:G80T/a1");
    EXPECT_EQ(find(std::nullopt, std::nullopt, std::nullopt, std::nullopt, 0.029, 0.124),
              "Cesaroni Technology:G80-7A/c1 AeroTech:G80T/a1");
}

TEST_F(ThrustCurveMotorSetDatabaseTest, DesignationMatchingIgnoresCase)
{
    EXPECT_EQ(find(std::nullopt, std::nullopt, std::nullopt, "c6", kNaN, kNaN),
              "Estes:C6-5/e1 Estes:C6-5/e2");
    // "C6-5".contains("6-5"), and "XC6" contains the common name "C6".
    EXPECT_EQ(find(std::nullopt, std::nullopt, std::nullopt, "6-5", kNaN, kNaN),
              "Estes:C6-5/e1 Estes:C6-5/e2");
    EXPECT_EQ(find(std::nullopt, std::nullopt, std::nullopt, "xc6", kNaN, kNaN),
              "Estes:C6-5/e1 Estes:C6-5/e2");
    EXPECT_EQ(find(std::nullopt, std::nullopt, std::nullopt, "H128", kNaN, kNaN), "");
}

TEST_F(ThrustCurveMotorSetDatabaseTest, InterfaceReturnsTheSameInstances)
{
    const MotorDatabase&                            database = m_database;
    const std::vector<std::shared_ptr<const Motor>> found =
        database.findMotors("c1", std::nullopt, std::nullopt, std::nullopt, kNaN, kNaN);
    ASSERT_EQ(found.size(), 1U);
    EXPECT_EQ(found.front().get(), m_database.getMotorSets().at(2).getMotors().front().get());
    EXPECT_EQ(found.front()->getDigest(), "c1");
}

TEST(ThrustCurveMotorSetDatabase, AddsToTheLastMatchingSet)
{
    ThrustCurveMotorSetDatabase database;
    EXPECT_TRUE(database.getMotorSets().empty());
    EXPECT_EQ(names(database.findThrustCurveMotors(std::nullopt, std::nullopt, std::nullopt,
                                                   std::nullopt, kNaN, kNaN)),
              "");

    const MotorPtr first = harnessMotor("Estes", "B6-4", "B6", Motor::Type::SINGLE, {4}, 0.018,
                                        0.07, {0, 0.2, 0.8}, {0, 12, 0}, "b1", "");
    const MotorPtr other = harnessMotor("Quest", "B6-4", "B6", Motor::Type::SINGLE, {4}, 0.018,
                                        0.07, {0, 0.2, 0.8}, {0, 12, 0}, "q1", "");
    const MotorPtr again = harnessMotor("es", "b6-4", "B6", Motor::Type::SINGLE, {6}, 0.018, 0.07,
                                        {0, 0.3, 0.8}, {0, 11, 0}, "b2", "");
    database.addMotor(first);
    database.addMotor(other);
    database.addMotor(again);

    ASSERT_EQ(database.getMotorSets().size(), 2U);
    EXPECT_EQ(database.getMotorSets().at(0).getMotors(), (std::vector<MotorPtr>{first, again}));
    EXPECT_EQ(database.getMotorSets().at(0).getDelays(), (std::vector<double>{4, 6}));
    EXPECT_EQ(database.getMotorSets().at(1).getMotors(), (std::vector<MotorPtr>{other}));

    EXPECT_THROW(database.addMotor(nullptr), BugError);
}

}  // namespace
