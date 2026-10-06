#include "QtRocket/simulation/GroundStepper.h"

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/simulation/AbstractSimulationStepper.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationStatusSupport.h"
#include "simulation/SimulationTestSupport.h"
#include "simulation/StepperTestSupport.h"

namespace
{

using QtRocket::AbstractSimulationStepper;
using QtRocket::Coordinate;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::GroundStepper;
using QtRocket::SimulationConditions;
using QtRocket::SimulationStatus;
using QtRocket::Test::bugText;
using QtRocket::Test::CloneCountingListener;
using QtRocket::Test::InitializedScenario;
using QtRocket::Test::javaScenarioDifferences;
using QtRocket::Test::newBranch;
using QtRocket::Test::scenarioTestName;
using QtRocket::Test::statusConfiguration;
using QtRocket::Test::stepScenarioNames;
using QtRocket::Test::StepScenarioPin;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;

static_assert(std::is_base_of_v<AbstractSimulationStepper, GroundStepper>);
static_assert(!std::is_abstract_v<GroundStepper>);
static_assert(!std::is_copy_assignable_v<GroundStepper>);

// OpenRocket has no test of GroundStepper. The expectations are what
// probes/tier8b-steppers/StepperProbe.java printed for steps of OpenRocket's stepper
// (StepperScenarioPins.cpp).

/// The scenarios of this file: steps of the ground stepper.
[[nodiscard]] bool isGroundStep(const StepScenarioPin& pin)
{
    return pin.stepper == "GROUND" && pin.action == "step";
}

class GroundScenario : public ::testing::TestWithParam<std::string>
{ };

// The status afterwards and every column of every row: a step of 0.5 s (two rows), steps at
// and below twice the minimum time step (one row), two steps in a row (four rows), and the
// order of the hooks.
TEST_P(GroundScenario, TheStepIsOpenRockets)
{
    EXPECT_EQ(javaScenarioDifferences(GetParam()), "");
}

INSTANTIATE_TEST_SUITE_P(GroundStepper, GroundScenario,
                         ::testing::ValuesIn(stepScenarioNames(isGroundStep)),
                         [](const ::testing::TestParamInfo<std::string>& paramInfo) {
                             return scenarioTestName(paramInfo.param);
                         });

TEST(GroundStepper, TheScenariosCoverLongAndShortSteps)
{
    const std::vector<std::string> expected{"ground", "ground-short", "ground-limit",
                                            "ground-two-steps", "hooks-ground"};
    EXPECT_EQ(stepScenarioNames(isGroundStep), expected);
}

/// The values of the built-in type @p id in the rows of @p branch.
[[nodiscard]] std::vector<double> column(const FlightDataBranch& branch, FlightDataTypeId id)
{
    return branch.get(FlightDataType::builtin(id)).value_or(std::vector<double>{});
}

// What the pins of "ground" say, by name: a long step is split into a first point a minimum
// time step later and a second one at the end of the step, both at altitude 0 and at rest.
TEST(GroundStepper, ALongStepRecordsTheRocketAtRestAtOnceAndAtTheEndOfTheStep)
{
    GroundStepper       stepper;
    InitializedScenario f("ground", stepper);
    ASSERT_NE(f.status, nullptr);
    const double start = f.status->getSimulationTime();
    ASSERT_GT(f.status->getRocketPosition().z, 0.0);

    stepper.step(*f.status, 0.5);

    const FlightDataBranch& branch = *f.built.branch;
    ASSERT_EQ(branch.getLength(), 2U);
    const std::vector<double> times = column(branch, FlightDataTypeId::TYPE_TIME);
    EXPECT_EQ(times.at(0), start + 0.001);
    EXPECT_EQ(times.at(1), (start + 0.001) + (0.5 - 0.001));
    EXPECT_EQ(f.status->getSimulationTime(), times.at(1));
    EXPECT_EQ(column(branch, FlightDataTypeId::TYPE_ALTITUDE), (std::vector<double>{0.0, 0.0}));
    EXPECT_EQ(column(branch, FlightDataTypeId::TYPE_VELOCITY_TOTAL),
              (std::vector<double>{0.0, 0.0}));
    // The time step of a row is written when the next row is opened: the first row holds the
    // step that led to the second, and the last row a NaN.
    const std::vector<double> timeSteps = column(branch, FlightDataTypeId::TYPE_TIME_STEP);
    EXPECT_EQ(timeSteps.at(0), 0.5 - 0.001);
    EXPECT_TRUE(std::isnan(timeSteps.at(1)));
    EXPECT_TRUE(f.status->getRocketVelocity().exactlyEquals(Coordinate::kZero));
    EXPECT_EQ(f.status->getRocketPosition().z, 0.0);
}

TEST(GroundStepper, AStepOfUpToTwoMinimumTimeStepsIsOnePoint)
{
    GroundStepper       stepper;
    InitializedScenario f("ground-limit", stepper);
    ASSERT_NE(f.status, nullptr);
    const double start = f.status->getSimulationTime();

    // Java: `timeStep > 2 * MIN_TIME_STEP`; exactly twice is not above.
    stepper.step(*f.status, 0.002);
    EXPECT_EQ(f.built.branch->getLength(), 1U);
    EXPECT_EQ(f.status->getSimulationTime(), start + 0.002);

    stepper.step(*f.status, 0.0021);
    EXPECT_EQ(f.built.branch->getLength(), 3U);
}

// Before the step there may be a row (the last row of the landing stepper): its time step, a
// NaN until then, is the first thing the ground stepper writes.
TEST(GroundStepper, TheStepWritesItsTimeStepToTheRowBeforeIt)
{
    GroundStepper       stepper;
    InitializedScenario f("ground", stepper);
    ASSERT_NE(f.status, nullptr);
    f.status->storeData();
    f.built.branch->setValue(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME_STEP),
                             std::numeric_limits<double>::quiet_NaN());

    stepper.step(*f.status, 0.5);

    const std::vector<double> timeSteps = column(*f.built.branch, FlightDataTypeId::TYPE_TIME_STEP);
    ASSERT_EQ(timeSteps.size(), 3U);
    EXPECT_EQ(timeSteps.at(0), 0.001);
    EXPECT_EQ(timeSteps.at(1), 0.5 - 0.001);
    EXPECT_TRUE(std::isnan(timeSteps.at(2)));
}

// Java: `return status;`.
TEST(GroundStepper, InitializeGivesTheStatusBackAsItIs)
{
    const TestEstesAlphaIII alpha;
    const auto              conditions = std::make_shared<SimulationConditions>();
    const auto              counter    = std::make_shared<CloneCountingListener>();
    conditions->getSimulationListenerList().push_back(counter);
    SimulationStatus original(statusConfiguration(*alpha.rocket, testFcid(0)), conditions);
    original.setFlightDataBranch(newBranch());
    original.setSimulationTime(7.5);
    const QtRocket::FlightConfiguration* configuration = &original.getConfiguration();

    GroundStepper          stepper;
    const SimulationStatus status = stepper.initialize(std::move(original));

    EXPECT_EQ(status.getSimulationConditions(), conditions);
    EXPECT_EQ(counter->clones(), 0);
    EXPECT_EQ(&status.getConfiguration(), configuration);
    EXPECT_EQ(status.getSimulationTime(), 7.5);
}

TEST(GroundStepper, TheAccelerationIsNothing)
{
    GroundStepper       stepper;
    InitializedScenario f("ground", stepper);
    ASSERT_NE(f.status, nullptr);
    const QtRocket::ModId before = f.status->getModId();

    stepper.calculateAcceleration(*f.status, stepper.getStore());

    EXPECT_FALSE(stepper.getStore().accelerationData.has_value());
    EXPECT_EQ(f.status->getModId(), before);
}

TEST(GroundStepper, AStepNeedsAFlightDataBranch)
{
    const TestEstesAlphaIII        alpha;
    QtRocket::Test::ScenarioStatus built;
    const StepScenarioPin*         pin = QtRocket::Test::findStepScenario("ground");
    ASSERT_NE(pin, nullptr);
    built = QtRocket::Test::buildScenarioStatus(*pin, {});
    ASSERT_NE(built.status, nullptr);
    built.status->setFlightDataBranch(nullptr);
    GroundStepper stepper;
    // Java: a NullPointerException.
    EXPECT_EQ(bugText([&] { stepper.step(*built.status, 0.5); }),
              "The simulation status has no flight data branch to store the step's data to");
}

}  // namespace
