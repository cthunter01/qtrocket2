#include "QtRocket/motor/MotorCorrelation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"
#include "QtRocket/util/BugError.h"
#include "goldens/GoldenData.h"
#include "motor/MotorCorrelationPins.h"
#include "motor/TestMotorDatabase.h"
#include "rocket/JavaValueDifferences.h"

namespace
{

using QtRocket::BugError;
using QtRocket::Manufacturer;
using QtRocket::Motor;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustCurveMotorSet;
using QtRocket::MotorCorrelation::crossCorrelation;
using QtRocket::MotorCorrelation::similarity;
using QtRocket::Test::JavaValueDifferences;
using QtRocket::Test::kMotorCorrelationDigestPins;
using QtRocket::Test::kMotorCorrelationPairPins;
using QtRocket::Test::kMotorCorrelationSetPins;
using QtRocket::Test::MotorCorrelationPairPin;
using QtRocket::Test::MotorCorrelationSetPin;

/// MotorCorrelationTest.EPSILON.
constexpr double kEpsilon = 1e-9;

/// ThrustCurveMotorSelectionPanel.MOTOR_SIMILARITY_THRESHOLD: "Hide very similar thrust curves"
/// keeps a curve whose similarity with the selected one is below it.
constexpr double kMotorSimilarityThreshold =
    QtRocket::ThrustCurveMotorSelection::kMotorSimilarityThreshold;

/// MotorCorrelationTest's StubMotor: a motor whose thrust is a function of the time and whose
/// estimates are given; everything else is a constant.
class StubMotor final : public Motor
{
public:
    StubMotor(std::function<double(double)> thrustFunction, double averageThrust, double burnTime,
              double totalImpulse)
      : m_thrustFunction(std::move(thrustFunction)),
        m_averageThrust(averageThrust),
        m_burnTime(burnTime),
        m_totalImpulse(totalImpulse)
    {
    }

    [[nodiscard]] Type               getMotorType() const override { return Type::SINGLE; }
    [[nodiscard]] const std::string& getCode() const override { return m_code; }
    [[nodiscard]] const std::string& getCommonName() const override { return m_commonName; }
    [[nodiscard]] std::string        getCommonName(double /*delay*/) const override
    {
        return getCommonName();
    }
    [[nodiscard]] const std::string& getDesignation() const override { return m_designation; }
    [[nodiscard]] std::string        getDesignation(double /*delay*/) const override
    {
        return getDesignation();
    }
    [[nodiscard]] const std::string& getDescription() const override { return m_description; }
    [[nodiscard]] double             getDiameter() const override { return 0.0; }
    [[nodiscard]] double             getLength() const override { return 0.0; }
    [[nodiscard]] const std::string& getDigest() const override { return m_digest; }
    [[nodiscard]] double             getLaunchCGx() const override { return 0.0; }
    [[nodiscard]] double             getBurnoutCGx() const override { return 0.0; }
    [[nodiscard]] double             getLaunchMass() const override { return 0.0; }
    [[nodiscard]] double             getBurnoutMass() const override { return 0.0; }
    [[nodiscard]] double             getBurnTimeEstimate() const override { return m_burnTime; }
    [[nodiscard]] double getAverageThrustEstimate() const override { return m_averageThrust; }
    [[nodiscard]] double getMaxThrustEstimate() const override { return m_averageThrust; }
    [[nodiscard]] double getTotalImpulseEstimate() const override { return m_totalImpulse; }
    [[nodiscard]] double getBurnTime() const override { return m_burnTime; }
    [[nodiscard]] double getThrust(double motorTime) const override
    {
        return m_thrustFunction(motorTime);
    }
    [[nodiscard]] double getTotalMass(double /*motorTime*/) const override { return 0.0; }
    [[nodiscard]] double getPropellantMass(double /*motorTime*/) const override { return 0.0; }
    [[nodiscard]] double getCMx(double /*motorTime*/) const override { return 0.0; }
    [[nodiscard]] double getUnitIxx() const override { return 0.0; }
    [[nodiscard]] double getUnitIyy() const override { return 0.0; }
    [[nodiscard]] double getUnitIzz() const override { return 0.0; }

private:
    std::function<double(double)> m_thrustFunction;
    double                        m_averageThrust;
    double                        m_burnTime;
    double                        m_totalImpulse;
    std::string                   m_code{"stub"};
    std::string                   m_commonName{"Stub Motor"};
    std::string                   m_designation{"Stub"};
    std::string                   m_description{"Test motor"};
    std::string                   m_digest{"digest"};
};

/// MotorCorrelationTest.constantMotor(): @p thrustLevel up to @p burnTime, then nothing.
StubMotor constantMotor(double thrustLevel, double burnTime)
{
    const double totalImpulse = thrustLevel * burnTime;
    return {[thrustLevel, burnTime](double t) { return t <= burnTime ? thrustLevel : 0.0; },
            thrustLevel, burnTime, totalImpulse};
}

// ---- Ported from MotorCorrelationTest.java ----

TEST(MotorCorrelation, IdenticalMotorsHaveFullSimilarity)
{
    const StubMotor motor = constantMotor(10.0, 1.5);
    EXPECT_NEAR(1.0, similarity(motor, motor), kEpsilon);
    EXPECT_NEAR(1.0, crossCorrelation(motor, motor), kEpsilon);
}

TEST(MotorCorrelation, AverageThrustDifferenceLimitsSimilarity)
{
    const StubMotor stronger = constantMotor(10.0, 1.0);
    const StubMotor weaker   = constantMotor(5.0, 1.0);

    const double motorSimilarity = similarity(stronger, weaker);
    EXPECT_NEAR(0.5, motorSimilarity, kEpsilon);
}

TEST(MotorCorrelation, CrossCorrelationHandlesZeroThrustCurves)
{
    const StubMotor zeroA = constantMotor(0.0, 1.0);
    const StubMotor zeroB = constantMotor(0.0, 1.0);

    EXPECT_NEAR(1.0, crossCorrelation(zeroA, zeroB), kEpsilon);
    EXPECT_NEAR(1.0, similarity(zeroA, zeroB), kEpsilon);
}

TEST(MotorCorrelation, CrossCorrelationRejectsNegativeThrustValues)
{
    const StubMotor negative([](double /*t*/) { return -1.0; }, 5.0, 1.0, 5.0);
    const StubMotor positive = constantMotor(5.0, 1.0);

    EXPECT_THROW(static_cast<void>(crossCorrelation(negative, positive)), BugError);
}

// ---- QtRocket's own ----

TEST(MotorCorrelation, TheNegativeThrustErrorNamesBothThrusts)
{
    // Java's message, the thrusts as Double.toString writes them: those of the first sample.
    const StubMotor positive = constantMotor(5.0, 1.0);
    const StubMotor negative([](double t) { return t < 0.5 ? 0.25 : -2.0; }, 5.0, 1.0, 5.0);
    try
    {
        static_cast<void>(similarity(positive, negative));
        ADD_FAILURE() << "no BugError";
    }
    catch (const BugError& error)
    {
        EXPECT_NE(std::string_view(error.what()).find("Negative thrust, t1=5.0 t2=-2.0 ("),
                  std::string_view::npos)
            << error.what();
    }
}

TEST(MotorCorrelation, TheBurnTimeRatioCountsTwice)
{
    // The same curve, so the cross-correlation and the other ratios are exactly 1: the burn times
    // 1.0 and 0.4 give 2 * 0.4, and 1.0 and 0.6 give 2 * 0.6 = 1.2, which does not limit it.
    const StubMotor longer = constantMotor(10.0, 1.0);
    const auto      thrust = [](double t) { return t <= 1.0 ? 10.0 : 0.0; };
    const StubMotor shorter(thrust, 10.0, 0.4, 10.0);
    const StubMotor slightlyShorter(thrust, 10.0, 0.6, 10.0);

    EXPECT_EQ(similarity(longer, shorter), 2 * 0.4);
    EXPECT_EQ(similarity(shorter, longer), 2 * 0.4);
    EXPECT_EQ(similarity(longer, slightlyShorter), 1.0);
}

TEST(MotorCorrelation, TheTotalImpulseRatioLimitsTheSimilarity)
{
    const StubMotor full = constantMotor(10.0, 1.0);
    const StubMotor half([](double t) { return t <= 1.0 ? 10.0 : 0.0; }, 10.0, 1.0, 2.5);

    EXPECT_EQ(similarity(full, half), 0.25);
    EXPECT_EQ(similarity(half, full), 0.25);
}

TEST(MotorCorrelation, TwoZeroEstimatesAreAlike)
{
    // diff() gives 1.0 when the larger value is zero; the curves are the same.
    const auto      thrust = [](double t) { return t <= 1.0 ? 3.0 : 0.0; };
    const StubMotor first(thrust, 0.0, 0.0, 0.0);
    const StubMotor second(thrust, 0.0, 0.0, 0.0);

    EXPECT_EQ(similarity(first, second), 1.0);
}

TEST(MotorCorrelation, ACurveWithoutThrustDoesNotCorrelateWithOneWithThrust)
{
    const StubMotor none   = constantMotor(0.0, 1.0);
    const StubMotor thrust = constantMotor(5.0, 1.0);

    EXPECT_EQ(crossCorrelation(none, thrust), 0.0);
    EXPECT_EQ(crossCorrelation(thrust, none), 0.0);
    EXPECT_EQ(similarity(none, thrust), 0.0);
}

TEST(MotorCorrelation, ANanEstimateMakesTheSimilarityNan)
{
    // Java's Math.min passes a NaN on, and MathUtil.equals(NaN, 0) is false: a NaN similarity,
    // which is not below the chooser's threshold (the curve is hidden as very similar).
    constexpr double kNaN   = std::numeric_limits<double>::quiet_NaN();
    const auto       thrust = [](double t) { return t <= 1.0 ? 10.0 : 0.0; };
    const StubMotor  known  = constantMotor(10.0, 1.0);
    const StubMotor  unknownAverage(thrust, kNaN, 1.0, 10.0);
    const StubMotor  unknownBurnTime(thrust, 10.0, kNaN, 10.0);
    const StubMotor  unknownImpulse(thrust, 10.0, 1.0, kNaN);

    EXPECT_TRUE(std::isnan(similarity(known, unknownAverage)));
    EXPECT_TRUE(std::isnan(similarity(unknownBurnTime, known)));
    EXPECT_TRUE(std::isnan(similarity(known, unknownImpulse)));
    EXPECT_FALSE(similarity(known, unknownImpulse) < kMotorSimilarityThreshold);
}

TEST(MotorCorrelation, ANanThrustMakesTheCrossCorrelationNan)
{
    const StubMotor known = constantMotor(10.0, 1.0);
    const StubMotor unknown([](double /*t*/) { return std::numeric_limits<double>::quiet_NaN(); },
                            10.0, 1.0, 10.0);

    EXPECT_TRUE(std::isnan(crossCorrelation(known, unknown)));
    EXPECT_TRUE(std::isnan(crossCorrelation(unknown, known)));
}

// ---- Against what OpenRocket computes (MotorCorrelationPins.h) ----

/// The times at which crossCorrelation() asks for the thrust, when both of its motors are one motor
/// that records them: every time is asked twice in a row (a failure when not).
std::vector<double> recordedSampleTimes()
{
    std::vector<double> asked;
    const StubMotor     recorder(
        [&asked](double t) {
            asked.push_back(t);
            return 0.0;
        },
        0.0, 0.0, 0.0);
    if (crossCorrelation(recorder, recorder) != 1.0)
    {
        ADD_FAILURE() << "two motors without thrust do not correlate fully";
    }

    std::vector<double> times;
    for (std::size_t i = 0; i + 1 < asked.size(); i += 2)
    {
        if (asked[i] != asked[i + 1])
        {
            ADD_FAILURE() << "the motors are asked at different times at the sample " << i / 2;
        }
        times.push_back(asked[i]);
    }
    if (asked.size() % 2 != 0)
    {
        ADD_FAILURE() << "the motors are asked a different number of times";
    }
    return times;
}

/// The sum of @p values, added in their order (as the Java probe adds the sample times).
double sumInOrder(std::span<const double> values)
{
    double sum = 0;
    for (const double value : values)
    {
        sum += value;
    }
    return sum;
}

/// How many curves the motor sets of the bundled database hold, in all.
std::size_t bundledCurveCount()
{
    std::size_t count = 0;
    for (const ThrustCurveMotorSet& set : QtRocket::Test::bundledMotorDatabase().getMotorSets())
    {
        count += set.getMotorCount();
    }
    return count;
}

/// The motor sets of more than one curve of the bundled database, a line each: the index, the
/// manufacturer's display name, the designation and the digests of the curves.
std::string bundledMultiCurveSets()
{
    std::string text;
    const auto& sets = QtRocket::Test::bundledMotorDatabase().getMotorSets();
    for (std::size_t index = 0; index < sets.size(); ++index)
    {
        const ThrustCurveMotorSet& set = sets[index];
        if (set.getMotorCount() < 2)
        {
            continue;
        }
        const Manufacturer* manufacturer = set.getManufacturer();
        text += std::format("{} {} {}:", index,
                            manufacturer != nullptr ? manufacturer->getDisplayName() : "(none)",
                            set.getDesignation());
        for (const std::shared_ptr<const ThrustCurveMotor>& motor : set.getMotors())
        {
            text += std::format(" {}", motor->getDigest());
        }
        text += '\n';
    }
    return text;
}

/// The same lines for the pinned sets.
std::string pinnedMultiCurveSets()
{
    std::string                             text;
    const std::span<const std::string_view> digests(kMotorCorrelationDigestPins);
    for (const MotorCorrelationSetPin& pin : kMotorCorrelationSetPins)
    {
        text += std::format("{} {} {}:", pin.setIndex, pin.manufacturer, pin.designation);
        for (const std::string_view digest : digests.subspan(pin.firstDigest, pin.curveCount))
        {
            text += std::format(" {}", digest);
        }
        text += '\n';
    }
    return text;
}

/// The pinned motor set @p pin in the bundled database, or null when the database has no such.
const ThrustCurveMotorSet* bundledMotorSet(const MotorCorrelationSetPin& pin)
{
    const auto& sets = QtRocket::Test::bundledMotorDatabase().getMotorSets();
    return pin.setIndex < sets.size() ? &sets[pin.setIndex] : nullptr;
}

/// How the similarity of the pair of @p pin, and for a pair whose first curve comes first in the
/// set its cross-correlation, differ from OpenRocket's, bit for bit, and whether "Hide very similar
/// thrust curves" decides otherwise; empty when they do not. The other order of a pair runs
/// crossCorrelation() in that order, through similarity().
std::string pairDifferences(const MotorCorrelationPairPin& pin)
{
    const MotorCorrelationSetPin& setPin = kMotorCorrelationSetPins.at(pin.set);
    const ThrustCurveMotorSet*    set    = bundledMotorSet(setPin);
    if (set == nullptr || pin.first >= set->getMotorCount() || pin.second >= set->getMotorCount())
    {
        return std::format("{} {}: no curves {} and {} in the bundled database\n",
                           setPin.manufacturer, setPin.designation, pin.first, pin.second);
    }
    const ThrustCurveMotor& first  = *set->getMotors()[pin.first];
    const ThrustCurveMotor& second = *set->getMotors()[pin.second];

    JavaValueDifferences differences;
    const double         motorSimilarity = similarity(first, second);
    differences.pinned("similarity", pin.similarity, motorSimilarity, 0.0);
    if ((motorSimilarity < kMotorSimilarityThreshold) !=
        (pin.similarity < kMotorSimilarityThreshold))
    {
        differences.problem("\"Hide very similar thrust curves\" decides otherwise");
    }
    if (pin.first < pin.second)
    {
        differences.pinned("crossCorrelation", pin.crossCorrelation,
                           crossCorrelation(first, second), 0.0);
    }
    if (differences.text().empty())
    {
        return {};
    }
    return std::format("{} {} ({}, {}):\n{}", setPin.manufacturer, setPin.designation, pin.first,
                       pin.second, differences.text());
}

/// The pinned pairs whose reverse is missing or has other values than they have.
std::string asymmetricPinnedPairs()
{
    std::string                                    text;
    const std::span<const MotorCorrelationPairPin> pins(kMotorCorrelationPairPins);
    for (const MotorCorrelationPairPin& pin : pins)
    {
        const auto reverse =
            std::ranges::find_if(pins, [&pin](const MotorCorrelationPairPin& other) {
                return other.set == pin.set && other.first == pin.second &&
                       other.second == pin.first;
            });
        if (reverse == pins.end() || reverse->similarity != pin.similarity ||
            reverse->crossCorrelation != pin.crossCorrelation)
        {
            text += std::format("set {} ({}, {})\n", pin.set, pin.first, pin.second);
        }
    }
    return text;
}

/// How many of the pinned pairs lie below the chooser's threshold.
std::size_t pinnedPairsBelowTheThreshold()
{
    std::size_t count = 0;
    for (const MotorCorrelationPairPin& pin : kMotorCorrelationPairPins)
    {
        if (pin.similarity < kMotorSimilarityThreshold)
        {
            ++count;
        }
    }
    return count;
}

/// How many shards the pairs are split into (each a ctest test of its own, so that they run in
/// parallel: a correlation takes some 30 ms in a debug build, 483 of them some 16 s).
constexpr std::size_t kPairShards = 8;

/// Whether the pair of @p pin belongs to the shard @p shard: its set's position in the pins is
/// @p shard modulo kPairShards.
bool isInShard(const MotorCorrelationPairPin& pin, std::size_t shard)
{
    return pin.set % kPairShards == shard;
}

/// The differences of the pairs of the shard @p shard (pairDifferences()).
std::string shardDifferences(std::size_t shard)
{
    std::string differences;
    for (const MotorCorrelationPairPin& pin : kMotorCorrelationPairPins)
    {
        if (isInShard(pin, shard))
        {
            differences += pairDifferences(pin);
        }
    }
    return differences;
}

/// How many pairs the shard @p shard holds.
std::size_t shardPairCount(std::size_t shard)
{
    std::size_t count = 0;
    for (const MotorCorrelationPairPin& pin : kMotorCorrelationPairPins)
    {
        if (isInShard(pin, shard))
        {
            ++count;
        }
    }
    return count;
}

/// The pins come from the OpenRocket of the goldens.
TEST(MotorCorrelationPinsGolden, ComeFromTheOpenRocketOfTheGoldens)
{
    const auto manifest = QtRocket::Test::loadGoldenManifest();
    ASSERT_TRUE(manifest.has_value()) << manifest.error().toString();
    EXPECT_EQ(QtRocket::Test::kMotorCorrelationPinsCommit, manifest->openrocketCommit);
    EXPECT_EQ(kMotorCorrelationSetPins.size(), 107U);
    EXPECT_EQ(kMotorCorrelationPairPins.size(), 322U);
}

/// crossCorrelation() asks both motors for their thrust at the times OpenRocket asks at: 0, then
/// the time accumulated by adding 0.01 again and again, while it is below 1000.
TEST(MotorCorrelationPinsGolden, SamplesTheThrustAtOpenRocketsTimes)
{
    const std::vector<double> times = recordedSampleTimes();
    ASSERT_EQ(times.size(), QtRocket::Test::kCorrelationPinsSampleCount);
    EXPECT_EQ(times[0], 0.0);
    EXPECT_EQ(times[1], QtRocket::Test::kCorrelationPinsSecondSampleTime);
    EXPECT_EQ(times.back(), QtRocket::Test::kCorrelationPinsLastSampleTime);
    EXPECT_EQ(sumInOrder(times), QtRocket::Test::kCorrelationPinsSampleTimeSum);
}

/// The bundled database holds the motor sets OpenRocket's does, with the same curves in the same
/// order.
TEST(MotorCorrelationPinsGolden, TheBundledDatabaseHoldsOpenRocketsSets)
{
    EXPECT_EQ(QtRocket::Test::bundledMotorDatabase().getMotorSets().size(),
              QtRocket::Test::kCorrelationPinsMotorSets);
    EXPECT_EQ(bundledCurveCount(), QtRocket::Test::kCorrelationPinsMotors);
    EXPECT_EQ(bundledMultiCurveSets(), pinnedMultiCurveSets());
}

/// OpenRocket's similarities agree in both orders of a pair, also bit for bit; 214 of the 322 lie
/// below the chooser's threshold (the curve is shown).
TEST(MotorCorrelationPinsGolden, OpenRocketsSimilaritiesAreSymmetric)
{
    EXPECT_EQ(asymmetricPinnedPairs(), "");
    EXPECT_EQ(pinnedPairsBelowTheThreshold(), 214U);
}

class MotorCorrelationPairsGolden : public ::testing::TestWithParam<std::size_t>
{ };

/// Every ordered pair of curves of the shard's sets has OpenRocket's similarity, bit for bit, and
/// so the same "Hide very similar thrust curves" decision, and its cross-correlation.
TEST_P(MotorCorrelationPairsGolden, GivesOpenRocketsValuesForTheCurvesOfASet)
{
    EXPECT_GT(shardPairCount(GetParam()), 0U);
    EXPECT_EQ(shardDifferences(GetParam()), "");
}

INSTANTIATE_TEST_SUITE_P(Shards, MotorCorrelationPairsGolden,
                         ::testing::Range<std::size_t>(0, kPairShards));

}  // namespace
