// Tests of StabilityUnits: the stability units bound to a flight configuration or a rocket
// (OpenRocket's CaliberUnit and PercentageOfLengthUnit made from a FlightConfiguration or a
// Rocket, and UnitGroup.stabilityUnits() / secondaryStabilityUnits() of them).
//
// The pinned values were printed by the Java probe StabilityUnitsProbe.java of Milestone 2, run 1,
// part C4, which runs OpenRocket's own classes (commit 5f164fd0e, JDK 17) on the rockets of
// TestRockets.java, each settled as the golden harness settles it (tools/openrocket-goldens,
// AutomaticDimensions.settle()), with the default stability units "cal" and "%". A caliber is a
// largest diameter, taken from the radii by a multiplication, and is pinned exactly, and so are
// the conversions that divide by it. A length comes from the bounding box of the instances of the
// configuration and is compared within a relative 1e-12, as are the conversions that divide by
// it.

#include "QtRocket/rocket/StabilityUnits.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/rocket/AutomaticDimensions.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/unit/CaliberUnit.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Error.h"
#include "rocket/TestRockets.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::CaliberUnit;
using QtRocket::ComponentChangeEvent;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::NoseCone;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::StabilityUnits;
using QtRocket::Unit;
using QtRocket::UnitGroup;
using QtRocket::unitGroup;
using QtRocket::UnitGroupId;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestRocketMaker;
using QtRocket::Test::testRocketMakers;
using QtRocket::Test::TestSimple2Stage;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

/// The relative tolerance of a length and of a conversion that divides by one.
constexpr double kLengthTolerance = 1e-12;

/// @p actual is @p expected within kLengthTolerance; an infinity or zero must be exact.
void expectLengthNear(double actual, double expected, std::string_view what)
{
    if (std::isinf(expected) || expected == 0)
    {
        EXPECT_EQ(actual, expected) << what;
        return;
    }
    EXPECT_NEAR(actual, expected, std::abs(expected) * kLengthTolerance) << what;
}

/// The unit named @p name of @p group.
[[nodiscard]] const Unit& unitNamed(const UnitGroup& group, std::string_view name)
{
    const Unit* unit = group.getUnit(name);
    EXPECT_NE(unit, nullptr) << name;
    return unit != nullptr ? *unit : group.getDefaultUnit();
}

/// @p rocket with its automatic dimensions settled, as the probe settles it.
[[nodiscard]] std::unique_ptr<Rocket> settled(std::unique_ptr<Rocket> rocket)
{
    const Result<int> passes = QtRocket::settleAutomaticDimensions(*rocket);
    EXPECT_TRUE(passes.has_value()) << passes.error().message;
    return rocket;
}

/// The maker of TestRockets.h for the Java method @p method.
[[nodiscard]] const TestRocketMaker& makerOf(std::string_view method)
{
    for (const TestRocketMaker& maker : testRocketMakers())
    {
        if (maker.method == method)
        {
            return maker;
        }
    }
    ADD_FAILURE() << "no maker " << method;
    return testRocketMakers().front();
}

// ===================================================================== the 13 test rockets

/// The caliber and the aerodynamic length of a configuration with one stage active.
struct StageOnly
{
    double caliber;
    double length;
};

/// What OpenRocket gives for one of its test rockets. Every configuration of each of them has
/// the same values, in every configuration with only one stage active as well.
struct JavaReferences
{
    /// The maker of TestRockets.java.
    std::string_view method;
    /// getConfigurationCount() + 1.
    int configurations;
    /// CaliberUnit.calculateCaliber(rocket), PercentageOfLengthUnit.getReferenceLength(rocket).
    double rocketCaliber;
    double rocketLength;
    /// The same of every configuration (getFlightConfigurationByIndex(i, true)).
    double caliber;
    double length;
    /// Of a new rocket whose configuration has had setOnlyStage(n), for every stage n.
    std::size_t              stages;
    std::array<StageOnly, 3> onlyStage;
    /// The default units of stabilityUnits(config) and secondaryStabilityUnits(config):
    /// toStringUnit(0.05) of both, then toStringUnit(-0.031) of both.
    std::array<std::string_view, 4> texts;
};

// NOLINTBEGIN(modernize-use-designated-initializers): a table printed by the Java probe
// NOLINTBEGIN(modernize-use-std-numbers): OpenRocket's results, not approximations of constants
constexpr std::array<JavaReferences, 13> kJavaReferences{{
    {"makeEstesAlphaIII",
     6,
     0.024,
     0.27,
     0.024,
     0.27,
     1,
     {{{0.024, 0.27}}},
     {"2.08 cal", "18.5 %", "-1.29 cal", "-11.5 %"}},
    {"makeBeta",
     6,
     0.024,
     0.335,
     0.024,
     0.335,
     2,
     {{{0.024, 0.27}, {0.024, 0.065}}},
     {"2.08 cal", "14.9 %", "-1.29 cal", "-9.25 %"}},
    {"makeSimple2Stage",
     2,
     0.02,
     0.2,
     0.02,
     0.2,
     2,
     {{{0.02, 0.1}, {0.02, 0.1}}},
     {"2.5 cal", "25 %", "-1.55 cal", "-15.5 %"}},
    {"makeBigBlue",
     1,
     0.066,
     0.8999999999999999,
     0.066,
     0.8999999999999999,
     1,
     {{{0.066, 0.8999999999999999}}},
     {"0.758 cal", "5.56 %", "-0.47 cal", "-3.44 %"}},
    {"makeIsoHaisu",
     1,
     0.14,
     2.705,
     0.14,
     2.705,
     1,
     {{{0.14, 2.705}}},
     {"0.357 cal", "1.85 %", "-0.221 cal", "-1.15 %"}},
    {"makeFalcon9Heavy",
     2,
     0.104,
     1.364,
     0.104,
     1.364,
     3,
     {{{0.104, 0.5640000000000001}, {0.077, 0.8}, {0.077, 0.88}}},
     {"0.481 cal", "3.67 %", "-0.298 cal", "-2.27 %"}},
    {"makeMultiStageEventTestRocket",
     2,
     0.02,
     0.39,
     0.02,
     0.39,
     3,
     {{{0.02, 0.30000000000000004}, {0.02, 0.08999999999999997}, {0.014, 0.14}}},
     {"2.5 cal", "12.8 %", "-1.55 cal", "-7.95 %"}},
    {"makeEndPlateRocket",
     1,
     0.02,
     0.304,
     0.02,
     0.304,
     1,
     {{{0.02, 0.304}}},
     {"2.5 cal", "16.4 %", "-1.55 cal", "-10.2 %"}},
    {"makeEstesAlphaIIIWithPods",
     6,
     0.024,
     0.27,
     0.024,
     0.27,
     1,
     {{{0.024, 0.27}}},
     {"2.08 cal", "18.5 %", "-1.29 cal", "-11.5 %"}},
    {"makeEstesAlphaIIIWithMotorPods",
     6,
     0.024,
     0.27,
     0.024,
     0.27,
     1,
     {{{0.024, 0.27}}},
     {"2.08 cal", "18.5 %", "-1.29 cal", "-11.5 %"}},
    {"makeEstesAlphaIIIWithSecondMotor",
     6,
     0.024,
     0.27,
     0.024,
     0.27,
     1,
     {{{0.024, 0.27}}},
     {"2.08 cal", "18.5 %", "-1.29 cal", "-11.5 %"}},
    {"makeEstesAlphaIIIwithInlinePod",
     6,
     0.024,
     0.27,
     0.024,
     0.27,
     1,
     {{{0.024, 0.27}}},
     {"2.08 cal", "18.5 %", "-1.29 cal", "-11.5 %"}},
    {"makeClusterPods",
     2,
     0.05,
     0.2,
     0.05,
     0.2,
     2,
     {{{0.05, 0.2}, {0.05, 0.2}}},
     {"1 cal", "25 %", "-0.62 cal", "-15.5 %"}},
}};
// NOLINTEND(modernize-use-std-numbers)
// NOLINTEND(modernize-use-designated-initializers)

/// The probe's values of the rocket @p method makes.
[[nodiscard]] const JavaReferences* javaReferencesOf(std::string_view method)
{
    for (const JavaReferences& references : kJavaReferences)
    {
        if (references.method == method)
        {
            return &references;
        }
    }
    return nullptr;
}

/// The texts of the default units of @p primary and @p secondary against @p java.
void expectTexts(const UnitGroup& primary, const UnitGroup& secondary, const JavaReferences& java,
                 std::string_view what)
{
    EXPECT_EQ(primary.getDefaultUnit().toStringUnit(0.05), java.texts[0]) << what;
    EXPECT_EQ(secondary.getDefaultUnit().toStringUnit(0.05), java.texts[1]) << what;
    EXPECT_EQ(primary.getDefaultUnit().toStringUnit(-0.031), java.texts[2]) << what;
    EXPECT_EQ(secondary.getDefaultUnit().toStringUnit(-0.031), java.texts[3]) << what;
}

/// The configuration @p index of @p rocket and its stability units against @p java.
void expectConfiguration(Rocket& rocket, int index, const JavaReferences& java)
{
    const std::string          what = std::format("{} configuration {}", java.method, index);
    const FlightConfiguration& configuration = rocket.getFlightConfigurationByIndex(index, true);
    EXPECT_EQ(StabilityUnits::calculateCaliber(configuration), java.caliber) << what;
    expectLengthNear(StabilityUnits::getPercentageReferenceLength(configuration), java.length,
                     what);

    const std::unique_ptr<UnitGroup::StabilityUnitGroup> primary =
        StabilityUnits::stabilityUnits(configuration);
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> secondary =
        StabilityUnits::secondaryStabilityUnits(configuration);
    expectTexts(*primary, *secondary, java, what);
    EXPECT_EQ(unitNamed(*primary, "cal").toUnit(0.1), 0.1 / java.caliber) << what;
    EXPECT_EQ(unitNamed(*secondary, "cal").toUnit(0.1), 0.1 / java.caliber) << what;
    EXPECT_EQ(unitNamed(*primary, "cal").fromUnit(2), 2 * java.caliber) << what;
    expectLengthNear(unitNamed(*primary, "%").toUnit(0.1), 0.1 / java.length / 0.01, what);
    expectLengthNear(unitNamed(*secondary, "%").toUnit(0.1), 0.1 / java.length / 0.01, what);
    expectLengthNear(unitNamed(*primary, "%").fromUnit(20), 20 * java.length * 0.01, what);
    // The percentage unit of the group is the one it hands out by name.
    EXPECT_EQ(&primary->getPercentageOfLengthUnit(), &unitNamed(*primary, "%")) << what;
}

/// A new rocket of @p maker whose configuration @p index has only the stage @p stage active,
/// against @p java.
void expectOnlyStage(const TestRocketMaker& maker, int index, std::size_t stage,
                     const JavaReferences& java)
{
    const std::string what =
        std::format("{} configuration {} only stage {}", java.method, index, stage);
    const std::unique_ptr<Rocket> rocket = settled(maker.make());
    FlightConfiguration& configuration   = rocket->getFlightConfigurationByIndex(index, true);
    configuration.setOnlyStage(static_cast<int>(stage));
    EXPECT_EQ(configuration.getActiveStageCount(), 1) << what;
    EXPECT_EQ(StabilityUnits::calculateCaliber(configuration), java.onlyStage.at(stage).caliber)
        << what;
    expectLengthNear(StabilityUnits::getPercentageReferenceLength(configuration),
                     java.onlyStage.at(stage).length, what);
}

/// One test rocket of TestRockets.h against the probe's values.
class StabilityUnitsOfTestRockets : public ::testing::TestWithParam<TestRocketMaker>
{
protected:
    DefaultUnitsGuard m_units;
};

TEST_P(StabilityUnitsOfTestRockets, MatchOpenRocket)
{
    const TestRocketMaker&        maker  = GetParam();
    const JavaReferences*         java   = javaReferencesOf(maker.method);
    const std::unique_ptr<Rocket> rocket = settled(maker.make());
    ASSERT_NE(java, nullptr) << maker.method;
    ASSERT_EQ(rocket->getConfigurationCount() + 1, java->configurations);
    ASSERT_EQ(rocket->getStageCount(), java->stages);

    EXPECT_EQ(StabilityUnits::calculateCaliber(*rocket), java->rocketCaliber);
    expectLengthNear(StabilityUnits::getPercentageReferenceLength(*rocket), java->rocketLength,
                     "rocket");
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> rocketUnits =
        StabilityUnits::stabilityUnits(*rocket);
    EXPECT_EQ(unitNamed(*rocketUnits, "cal").toUnit(0.1), 0.1 / java->rocketCaliber);
    expectLengthNear(unitNamed(*rocketUnits, "%").toUnit(0.1), 0.1 / java->rocketLength / 0.01,
                     "rocket %");

    for (int index = 0; index < java->configurations; index++)
    {
        expectConfiguration(*rocket, index, *java);
        for (std::size_t stage = 0; stage < java->stages; stage++)
        {
            expectOnlyStage(maker, index, stage, *java);
        }
    }
}

/// The test name of @p info: the Java method.
[[nodiscard]] std::string makerName(const ::testing::TestParamInfo<TestRocketMaker>& info)
{
    return std::string{info.param.method};
}

INSTANTIATE_TEST_SUITE_P(OpenRocketProbe, StabilityUnitsOfTestRockets,
                         ::testing::ValuesIn(testRocketMakers()), makerName);

// ======================================================================= reading lazily

TEST(StabilityUnits, AnExistingGroupSeesTheChangesOfTheRocket)
{
    const DefaultUnitsGuard guard;
    const TestEstesAlphaIII alpha;
    const Result<int>       passes = QtRocket::settleAutomaticDimensions(*alpha.rocket);
    ASSERT_TRUE(passes.has_value());
    const FlightConfiguration& configuration = alpha.rocket->getSelectedConfiguration();
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> primary =
        StabilityUnits::stabilityUnits(configuration);
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> secondary =
        StabilityUnits::secondaryStabilityUnits(configuration);
    const Unit& calibers = unitNamed(*primary, "cal");
    const Unit& percent  = unitNamed(*secondary, "%");
    EXPECT_EQ(calibers.toUnit(0.1), 4.166666666666667);
    expectLengthNear(percent.toUnit(0.1), 37.03703703703703, "before");

    // A new radius: the caliber follows, the length stays.
    alpha.body->setOuterRadius(0.015);
    EXPECT_EQ(StabilityUnits::calculateCaliber(configuration), 0.03);
    EXPECT_EQ(calibers.toUnit(0.1), 3.3333333333333335);
    expectLengthNear(percent.toUnit(0.1), 37.03703703703703, "radius 0.015");

    // A new length.
    alpha.body->setLength(0.3);
    expectLengthNear(StabilityUnits::getPercentageReferenceLength(configuration), 0.37, "length");
    EXPECT_EQ(calibers.toUnit(0.1), 3.3333333333333335);
    expectLengthNear(percent.toUnit(0.1), 27.027027027027028, "length 0.3");

    // A frozen rocket draws its modification id before it queues the event.
    alpha.rocket->freeze();
    alpha.body->setOuterRadius(0.02);
    EXPECT_EQ(calibers.toUnit(0.1), 2.5);
    alpha.rocket->thaw();
    EXPECT_EQ(calibers.toUnit(0.1), 2.5);
}

/// What the probe printed for a rocket with more than one stage, for the stability units of its
/// selected configuration: the caliber unit's and the percentage unit's toUnit(0.1) with every
/// stage active, after setOnlyStage(last stage), after setAllStages(), after toggleStage(0),
/// after toggleStage(0) again and after a rocket event; and the configuration's caliber and
/// length after setAllStages() and after the first toggleStage(0).
struct JavaStageToggles
{
    std::string_view                     method;
    std::array<std::array<double, 2>, 6> units;
    std::array<double, 2>                afterSetAllStages;
    std::array<double, 2>                afterToggle;
};

// NOLINTBEGIN(modernize-use-designated-initializers): a table printed by the Java probe
constexpr std::array<JavaStageToggles, 5> kJavaStageToggles{{
    {"makeBeta",
     {{{4.166666666666667, 29.850746268656714},
       {4.166666666666667, 153.84615384615384},
       {4.166666666666667, 153.84615384615384},
       {4.166666666666667, 153.84615384615384},
       {4.166666666666667, 29.850746268656714},
       {4.166666666666667, 29.850746268656714}}},
     {0.024, 0.065},
     {0.024, 0.065}},
    {"makeSimple2Stage",
     {{{5.0, 50.0}, {5.0, 100.0}, {5.0, 100.0}, {5.0, 100.0}, {5.0, 50.0}, {5.0, 50.0}}},
     {0.02, 0.1},
     {0.02, 0.1}},
    {"makeFalcon9Heavy",
     {{{0.9615384615384617, 7.331378299120235},
       {1.2987012987012987, 11.363636363636365},
       {1.2987012987012987, 11.363636363636365},
       {1.2987012987012987, 11.363636363636365},
       {0.9615384615384617, 7.331378299120235},
       {0.9615384615384617, 7.331378299120235}}},
     {0.104, 0.88},
     {0.077, 0.88}},
    {"makeMultiStageEventTestRocket",
     {{{5.0, 25.641025641025642},
       {7.142857142857143, 71.42857142857143},
       {7.142857142857143, 71.42857142857143},
       {5.0, 71.42857142857143},
       {5.0, 25.641025641025642},
       {5.0, 25.641025641025642}}},
     {0.02, 0.14},
     {0.02, 0.14}},
    {"makeClusterPods",
     {{{2.0, 50.0}, {2.0, 50.0}, {2.0, 50.0}, {10.0, kInfinity}, {2.0, 50.0}, {2.0, 50.0}}},
     {0.05, 0.2},
     {0.01, 0.0}},
}};
// NOLINTEND(modernize-use-designated-initializers)

/// What GoogleTest prints for the case of a test that failed: its maker.
// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const JavaStageToggles& java, std::ostream* out)
{
    *out << java.method;
}

/// The step @p step of @p java: the units @p calibers and @p percent convert 0.1 as OpenRocket's.
void expectUnits(const Unit& calibers, const Unit& percent, const JavaStageToggles& java,
                 std::size_t step)
{
    const std::string what = std::format("{} step {}", java.method, step);
    EXPECT_EQ(calibers.toUnit(0.1), java.units.at(step)[0]) << what;
    expectLengthNear(percent.toUnit(0.1), java.units.at(step)[1], what);
}

/// @p configuration's caliber and length are @p expected.
void expectReferences(const FlightConfiguration&   configuration,
                      const std::array<double, 2>& expected, std::string_view what)
{
    EXPECT_EQ(StabilityUnits::calculateCaliber(configuration), expected[0]) << what;
    expectLengthNear(StabilityUnits::getPercentageReferenceLength(configuration), expected[1],
                     what);
}

class StabilityUnitsStageToggles : public ::testing::TestWithParam<JavaStageToggles>
{
protected:
    DefaultUnitsGuard m_units;
};

TEST_P(StabilityUnitsStageToggles, AreSeenUnlessNoModificationIdChanges)
{
    const JavaStageToggles&       java          = GetParam();
    const std::unique_ptr<Rocket> rocket        = settled(makerOf(java.method).make());
    FlightConfiguration&          configuration = rocket->getSelectedConfiguration();
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> primary =
        StabilityUnits::stabilityUnits(configuration);
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> secondary =
        StabilityUnits::secondaryStabilityUnits(configuration);
    const Unit& calibers = unitNamed(*primary, "cal");
    const Unit& percent  = unitNamed(*secondary, "%");
    expectUnits(calibers, percent, java, 0);

    configuration.setOnlyStage(configuration.getStageCount() - 1);
    expectUnits(calibers, percent, java, 1);

    // setAllStages() draws no modification id: the units keep the references of the single
    // stage, as OpenRocket's do (and so does the configuration's own aerodynamic length).
    configuration.setAllStages();
    expectUnits(calibers, percent, java, 2);
    expectReferences(configuration, java.afterSetAllStages, "after setAllStages()");

    configuration.toggleStage(0);
    expectUnits(calibers, percent, java, 3);
    expectReferences(configuration, java.afterToggle, "after toggleStage(0)");

    configuration.toggleStage(0);
    expectUnits(calibers, percent, java, 4);

    RocketComponent& root = *rocket;
    root.fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
    expectUnits(calibers, percent, java, 5);
}

/// The test name of @p info: the Java method.
[[nodiscard]] std::string toggleName(const ::testing::TestParamInfo<JavaStageToggles>& info)
{
    return std::string{info.param.method};
}

INSTANTIATE_TEST_SUITE_P(OpenRocketProbe, StabilityUnitsStageToggles,
                         ::testing::ValuesIn(kJavaStageToggles), toggleName);

TEST(StabilityUnits, NothingIsReadAgainWhileTheEventsAreDisabled)
{
    const DefaultUnitsGuard    guard;
    Rocket                     rocket;
    auto&                      stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&                      body  = stage.addChild(std::make_unique<BodyTube>(0.5, 0.02, 0.001));
    const FlightConfiguration& configuration = rocket.getSelectedConfiguration();
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> primary =
        StabilityUnits::stabilityUnits(configuration);
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> secondary =
        StabilityUnits::secondaryStabilityUnits(configuration);
    const Unit& calibers = unitNamed(*primary, "cal");
    const Unit& percent  = unitNamed(*secondary, "%");

    // Built without events, the configuration has no active stage yet.
    EXPECT_EQ(configuration.getActiveStageCount(), 0);
    EXPECT_EQ(StabilityUnits::calculateCaliber(configuration), CaliberUnit::kDefaultCaliber);
    EXPECT_EQ(StabilityUnits::getPercentageReferenceLength(configuration), 0.0);
    EXPECT_EQ(calibers.toUnit(0.1), 10.0);
    EXPECT_EQ(percent.toUnit(0.1), kInfinity);

    body.setOuterRadius(0.03);
    body.setLength(0.6);
    EXPECT_EQ(calibers.toUnit(0.1), 10.0);
    EXPECT_EQ(percent.toUnit(0.1), kInfinity);

    rocket.enableEvents();
    EXPECT_EQ(StabilityUnits::calculateCaliber(configuration), 0.06);
    expectLengthNear(StabilityUnits::getPercentageReferenceLength(configuration), 0.6, "length");
    EXPECT_EQ(calibers.toUnit(0.1), 1.6666666666666667);
    expectLengthNear(percent.toUnit(0.1), 16.666666666666668, "enabled");
}

TEST(StabilityUnits, ARocketGroupFollowsTheRocketsModificationIdOnly)
{
    const DefaultUnitsGuard guard;
    const TestBeta          beta;
    const Result<int>       passes = QtRocket::settleAutomaticDimensions(*beta.rocket);
    ASSERT_TRUE(passes.has_value());
    Rocket&                                              rocket = *beta.rocket;
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> primary =
        StabilityUnits::stabilityUnits(rocket);
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> secondary =
        StabilityUnits::secondaryStabilityUnits(rocket);
    const Unit& calibers = unitNamed(*primary, "cal");
    const Unit& percent  = unitNamed(*secondary, "%");
    EXPECT_EQ(calibers.toUnit(0.1), 4.166666666666667);
    expectLengthNear(percent.toUnit(0.1), 29.850746268656714, "selected");

    // A stage toggle of the selected configuration fires no rocket event: the percentage keeps
    // the length of both stages.
    rocket.getSelectedConfiguration().setOnlyStage(0);
    expectLengthNear(StabilityUnits::getPercentageReferenceLength(rocket), 0.27, "only 0");
    expectLengthNear(percent.toUnit(0.1), 29.850746268656714, "only 0");

    // Selecting another configuration does.
    rocket.setSelectedConfiguration(rocket.getFlightConfigurationByIndex(0, true).getId());
    expectLengthNear(StabilityUnits::getPercentageReferenceLength(rocket), 0.335, "default");
    expectLengthNear(percent.toUnit(0.1), 29.850746268656714, "default");

    // The caliber of the rocket counts every stage, an inactive one too.
    auto nose = std::make_unique<NoseCone>();
    nose->setBaseRadius(0.05);
    nose->setAftRadiusAutomatic(false);
    beta.boosterStage->addChild(std::move(nose));
    rocket.getSelectedConfiguration().setOnlyStage(0);
    EXPECT_EQ(calibers.toUnit(0.1), 1.0);
    EXPECT_EQ(StabilityUnits::calculateCaliber(rocket), 0.1);
    EXPECT_EQ(StabilityUnits::calculateCaliber(rocket.getSelectedConfiguration()), 0.024);
}

TEST(StabilityUnits, BoundUnitsEqualTheProcessWideOnes)
{
    const DefaultUnitsGuard    guard;
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& configuration = alpha.rocket->getSelectedConfiguration();
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> primary =
        StabilityUnits::stabilityUnits(configuration);
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> secondary =
        StabilityUnits::secondaryStabilityUnits(configuration);
    const UnitGroup& stability = unitGroup(UnitGroupId::STABILITY);
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> constant = UnitGroup::stabilityUnits(0.5);

    // Unit.equals compares the class, the multiplier and the name (the probe: all true but the
    // caliber unit against the percentage unit).
    EXPECT_TRUE(unitNamed(*primary, "cal").equals(unitNamed(*secondary, "cal")));
    EXPECT_TRUE(unitNamed(*primary, "%").equals(unitNamed(*secondary, "%")));
    EXPECT_FALSE(unitNamed(*primary, "cal").equals(unitNamed(*primary, "%")));
    EXPECT_TRUE(unitNamed(*primary, "cal").equals(unitNamed(stability, "cal")));
    EXPECT_TRUE(unitNamed(*primary, "cal").equals(unitNamed(*constant, "cal")));
    EXPECT_TRUE(primary->equals(stability));
    EXPECT_TRUE(primary->equals(*secondary));
    EXPECT_EQ(primary->getUnitCount(), 6);
    EXPECT_EQ(primary->getDefaultUnit().getUnit(), "cal");
    EXPECT_EQ(secondary->getDefaultUnit().getUnit(), "%");

    // The plain units are UNITS_STABILITY's own, and a default set here is set there too.
    EXPECT_EQ(&primary->getUnit(0), &stability.getUnit(0));
    primary->setDefaultUnit(2);
    EXPECT_EQ(stability.getDefaultUnit().getUnit(), "m");
}

TEST(StabilityUnits, ADesignWithoutBodiesHasTheDefaultCaliberAndNoLength)
{
    const DefaultUnitsGuard guard;
    Rocket                  rocket;
    rocket.enableEvents();
    const FlightConfiguration& configuration = rocket.getSelectedConfiguration();
    EXPECT_EQ(StabilityUnits::calculateCaliber(configuration), CaliberUnit::kDefaultCaliber);
    EXPECT_EQ(StabilityUnits::getPercentageReferenceLength(configuration), 0.0);
    EXPECT_EQ(StabilityUnits::calculateCaliber(rocket), CaliberUnit::kDefaultCaliber);
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> secondary =
        StabilityUnits::secondaryStabilityUnits(configuration);
    EXPECT_EQ(unitNamed(*secondary, "%").toUnit(0.1), kInfinity);
    // U+221E INFINITY.
    EXPECT_EQ(secondary->getDefaultUnit().toStringUnit(0.1), "\xE2\x88\x9E %");
    EXPECT_EQ(secondary->getDefaultUnit().toStringUnit(0.0), "NaN %");

    auto& stage = rocket.addChild(std::make_unique<AxialStage>());
    EXPECT_EQ(StabilityUnits::calculateCaliber(configuration), CaliberUnit::kDefaultCaliber);
    EXPECT_EQ(StabilityUnits::getPercentageReferenceLength(configuration), 0.0);
    EXPECT_EQ(unitNamed(*secondary, "%").toUnit(0.1), kInfinity);

    // A diameter below 0.1 mm is no caliber.
    stage.addChild(std::make_unique<BodyTube>(0.1, 0.00004, 0));
    EXPECT_EQ(StabilityUnits::calculateCaliber(configuration), CaliberUnit::kDefaultCaliber);
    expectLengthNear(StabilityUnits::getPercentageReferenceLength(configuration), 0.1, "tiny");
}

/// What the probe FixerProbe.java (Milestone 2, run 1, the fixer of group C; OpenRocket 5f164fd0e,
/// JDK 17) printed for a rocket of one stage with one body tube 0.1 m long of the outer radius
/// @p radius: CaliberUnit.calculateCaliber() of the selected configuration and of the rocket, and
/// the caliber unit's toUnit(0.1).
struct JavaCaliberFloor
{
    double radius;
    double caliber;
    double toUnit;
};

// NOLINTBEGIN(modernize-use-designated-initializers): a table printed by the Java probe
constexpr std::array<JavaCaliberFloor, 3> kJavaCaliberFloors{{
    // 0.5 mm is a caliber, and so is 0.1 mm exactly (Java's test is cal < 0.0001); 0.08 mm is not.
    {0.00025, 0.0005, 200.0},
    {0.00005, 0.0001, 1000.0},
    {0.00004, 0.01, 10.0},
}};
// NOLINTEND(modernize-use-designated-initializers)

/// The caliber of a rocket with one body tube of @p java's radius against the probe.
void expectCaliberFloor(const JavaCaliberFloor& java)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    stage.addChild(std::make_unique<BodyTube>(0.1, java.radius, 0));
    rocket.enableEvents();
    const FlightConfiguration& configuration = rocket.getSelectedConfiguration();
    EXPECT_EQ(StabilityUnits::calculateCaliber(configuration), java.caliber) << java.radius;
    EXPECT_EQ(StabilityUnits::calculateCaliber(rocket), java.caliber) << java.radius;
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> primary =
        StabilityUnits::stabilityUnits(configuration);
    EXPECT_EQ(unitNamed(*primary, "cal").toUnit(0.1), java.toUnit) << java.radius;
}

TEST(StabilityUnits, ACaliberOfATenthOfAMillimetreOrMoreIsKept)
{
    const DefaultUnitsGuard guard;
    for (const JavaCaliberFloor& java : kJavaCaliberFloors)
    {
        expectCaliberFloor(java);
    }
}

// FixerProbe.java, section N: a body tube at an infinite absolute position spans no length (a
// bounding box takes no infinite minimum, so its span is never infinity minus infinity: neither
// reference is ever NaN, and Java's NaN quirk of the sentinel, kept in BoundReference, never
// shows). Moved back while the events are disabled, the configuration's length follows once a
// bounding box is computed again (getBoundingBox() always computes it), but the units keep their
// reference until a modification id changes.
TEST(StabilityUnits, AnInfinitePositionSpansNoLength)
{
    const DefaultUnitsGuard guard;
    Rocket                  rocket;
    auto&                   stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&                   body  = stage.addChild(std::make_unique<BodyTube>(0.1, 0.01, 0.001));
    rocket.enableEvents();
    body.setAxialMethod(AxialMethod::ABSOLUTE);
    body.setAxialOffset(kInfinity);
    FlightConfiguration& configuration = rocket.getSelectedConfiguration();
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> secondary =
        StabilityUnits::secondaryStabilityUnits(configuration);
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> rocketUnits =
        StabilityUnits::stabilityUnits(rocket);
    const Unit& percent       = unitNamed(*secondary, "%");
    const Unit& rocketPercent = unitNamed(*rocketUnits, "%");
    EXPECT_EQ(body.getPosition().x, kInfinity);
    EXPECT_EQ(StabilityUnits::getPercentageReferenceLength(configuration), 0.0);
    EXPECT_EQ(percent.toUnit(0.1), kInfinity);
    EXPECT_EQ(rocketPercent.toUnit(0.1), kInfinity);

    rocket.enableEvents(false);
    body.setAxialOffset(0);
    EXPECT_EQ(body.getPosition().x, 0.0);
    EXPECT_EQ(StabilityUnits::getPercentageReferenceLength(configuration), 0.0);
    configuration.setAllStages();
    EXPECT_EQ(StabilityUnits::getPercentageReferenceLength(configuration), 0.0);
    static_cast<void>(configuration.getBoundingBox());
    EXPECT_EQ(StabilityUnits::getPercentageReferenceLength(configuration), 0.1);
    EXPECT_EQ(percent.toUnit(0.1), kInfinity);
    EXPECT_EQ(rocketPercent.toUnit(0.1), kInfinity);

    rocket.enableEvents(true);
    EXPECT_EQ(StabilityUnits::getPercentageReferenceLength(configuration), 0.1);
    EXPECT_EQ(percent.toUnit(0.1), 100.0);
    EXPECT_EQ(rocketPercent.toUnit(0.1), 100.0);
}

// ================================================================ the configuration's id

// A group bound to a configuration of the rocket reads the rocket's configuration of that id at
// every conversion: Rocket::loadFrom() (undo and redo) replaces the configuration objects and
// removeFlightConfiguration() destroys one, and the group then reads the new one, or the default
// (review of Milestone 2, run 1: the group held the replaced object). The lengths: each stage
// of the simple two-stage rocket is 0.1 m long.
TEST(StabilityUnits, AConfigurationGroupFollowsTheConfigurationOfItsId)
{
    const DefaultUnitsGuard     guard;
    const TestSimple2Stage      two;
    Rocket&                     rocket        = *two.rocket;
    FlightConfiguration&        configuration = rocket.createFlightConfiguration();
    const FlightConfigurationId id            = configuration.getId();
    configuration.setOnlyStage(0);
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> secondary =
        StabilityUnits::secondaryStabilityUnits(configuration);
    const Unit& percent = unitNamed(*secondary, "%");
    expectLengthNear(percent.toUnit(0.1), 100.0, "the sustainer");

    // An undo of a longer sustainer: the configuration of the id is another object then, with
    // the stages the saved one had.
    const std::unique_ptr<Rocket> saved = rocket.copyRocketWithOriginalId();
    two.sustainerBody->setLength(0.4);
    expectLengthNear(percent.toUnit(0.1), 25.0, "a longer sustainer");
    rocket.loadFrom(*saved);
    ASSERT_EQ(rocket.getFlightConfiguration(id).getId(), id);
    EXPECT_EQ(rocket.getFlightConfiguration(id).getActiveStageCount(), 1);
    expectLengthNear(percent.toUnit(0.1), 100.0, "after the undo");

    // Removed: the default configuration, both stages active.
    rocket.removeFlightConfiguration(id);
    expectLengthNear(percent.toUnit(0.1), 50.0, "removed");
}

// A configuration that the rocket does not hold (a copy, such as a simulation's) is bound as the
// object: the group reads it, not the rocket's configuration of its id.
TEST(StabilityUnits, AConfigurationTheRocketDoesNotHoldIsBoundAsTheObject)
{
    const DefaultUnitsGuard guard;
    const TestSimple2Stage  two;
    Rocket&                 rocket = *two.rocket;
    FlightConfiguration     own(rocket, rocket.getSelectedConfiguration().getId());
    own.setOnlyStage(1);
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> secondary =
        StabilityUnits::secondaryStabilityUnits(own);
    expectLengthNear(unitNamed(*secondary, "%").toUnit(0.1), 100.0, "the booster alone");
    expectLengthNear(
        StabilityUnits::getPercentageReferenceLength(rocket.getSelectedConfiguration()), 0.2,
        "the rocket's configuration of the id");
}

}  // namespace
