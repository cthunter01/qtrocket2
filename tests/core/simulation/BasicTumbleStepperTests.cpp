#include "QtRocket/simulation/BasicTumbleStepper.h"

#include <array>
#include <format>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/AbstractEulerStepper.h"
#include "QtRocket/simulation/AbstractSimulationStepper.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "rocket/JavaValueDifferences.h"
#include "rocket/TestRockets.h"
#include "simulation/StepperTablePins.h"
#include "simulation/StepperTestSupport.h"

namespace
{

using QtRocket::AbstractEulerStepper;
using QtRocket::AbstractSimulationStepper;
using QtRocket::BasicTumbleStepper;
using QtRocket::FinSet;
using QtRocket::FlightConfiguration;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::SimulationConditions;
using QtRocket::SimulationStatus;
using QtRocket::Test::InitializedScenario;
using QtRocket::Test::javaScenarioDifferences;
using QtRocket::Test::makeScenarioRocket;
using QtRocket::Test::matchesPinnedValue;
using QtRocket::Test::scenarioConfiguration;
using QtRocket::Test::scenarioTestName;
using QtRocket::Test::stepScenarioNames;
using QtRocket::Test::StepScenarioPin;
using QtRocket::Test::testFcid;
using QtRocket::Test::StepperPins::DragCoefficientPin;
using QtRocket::Test::StepperPins::FinCountPin;
using QtRocket::Test::StepperPins::kDragCoefficientPins;
using QtRocket::Test::StepperPins::kFinCountPins;

static_assert(std::is_base_of_v<AbstractEulerStepper, BasicTumbleStepper>);
static_assert(!std::is_abstract_v<BasicTumbleStepper>);
static_assert(!std::is_copy_assignable_v<BasicTumbleStepper>);

// OpenRocket has no test of BasicTumbleStepper. The expectations are what
// probes/tier8b-steppers/StepperTableProbe.java ("computeCD", "tumble CD by fin count") and
// StepperProbe.java (the steps) printed.

// The constants of OpenRocket's technical documentation, as BasicTumbleStepper.java has them.
TEST(BasicTumbleStepper, TheConstantsAreJavas)
{
    EXPECT_EQ(BasicTumbleStepper::kCdFin, 1.42);
    EXPECT_EQ(BasicTumbleStepper::kCdBt, 0.56);
    const std::array<double, 8> finEfficiency{0.0, 0.5, 1.0, 1.41, 1.81, 1.73, 1.90, 1.85};
    EXPECT_EQ(BasicTumbleStepper::kFinEfficiency, finEfficiency);
}

/// The tumble CD of the rocket of @p maker in the scenarios' configuration, with only the stage
/// @p onlyStage active when it is not negative.
[[nodiscard]] double tumbleCd(std::string_view maker, int onlyStage)
{
    const std::unique_ptr<Rocket> rocket = makeScenarioRocket(maker);
    SimulationStatus              status(
        std::make_shared<FlightConfiguration>(scenarioConfiguration(*rocket).clone()),
        std::make_shared<SimulationConditions>());
    if (onlyStage >= 0)
    {
        status.getConfiguration().setOnlyStage(onlyStage);
    }
    BasicTumbleStepper stepper;
    return stepper.computeCD(status);
}

/// The rows of the drag coefficient table whose tumble CD is not Java's, within 1e-12: the
/// instances are summed in tree order, Java's in the hash order of its instance map.
[[nodiscard]] std::string tumbleDifferences()
{
    std::string differences;
    for (const DragCoefficientPin& pin : kDragCoefficientPins)
    {
        const double cd = tumbleCd(pin.maker, pin.onlyStage);
        if (!matchesPinnedValue(pin.tumble, cd))
        {
            differences += std::format("  {} stage {}: expected {}, got {}\n", pin.maker,
                                       pin.onlyStage, pin.tumble, cd);
        }
    }
    return differences;
}

// The thirteen test rockets, whole and stage by stage: three fins (the Alpha III), four (Big
// Blue), two fin sets (Iso-Haisu), pods with one fin each and with motors, boosters with
// several instances, and rockets without fins.
TEST(BasicTumbleStepper, TheDragCoefficientIsJavasForEveryTestRocket)
{
    EXPECT_EQ(kDragCoefficientPins.size(), 25U);
    EXPECT_EQ(tumbleDifferences(), "");
}

/// The Alpha III's tumble CDs with 1 to 8 fins that are not Java's, one line each.
[[nodiscard]] std::string finCountDifferences()
{
    std::string differences;
    for (const FinCountPin& pin : kFinCountPins)
    {
        const std::unique_ptr<Rocket> rocket = makeScenarioRocket("makeEstesAlphaIII");
        for (RocketComponent* component : rocket->getSelectedConfiguration().getAllComponents())
        {
            auto* fins = dynamic_cast<FinSet*>(component);
            if (fins != nullptr)
            {
                fins->setFinCount(pin.finCount);
            }
        }
        SimulationStatus   status(std::make_shared<FlightConfiguration>(
                                      rocket->getFlightConfiguration(testFcid(0)).clone()),
                                  std::make_shared<SimulationConditions>());
        BasicTumbleStepper stepper;
        const double       cd = stepper.computeCD(status);
        if (!matchesPinnedValue(pin.tumble, cd))
        {
            differences +=
                std::format("  {} fins: expected {}, got {}\n", pin.finCount, pin.tumble, cd);
        }
    }
    return differences;
}

// The fin efficiency by fin count: the area of a set counts with finEff[n] / n per fin, and a
// set with more fins than the table has (8) with its last entry (that of 7).
TEST(BasicTumbleStepper, TheFinEfficiencyFollowsTheFinCount)
{
    EXPECT_EQ(kFinCountPins.size(), 8U);
    EXPECT_EQ(finCountDifferences(), "");
}

/// The scenarios of this file: steps of the tumble stepper.
[[nodiscard]] bool isTumbleStep(const StepScenarioPin& pin)
{
    return pin.stepper == "TUMBLE" && pin.action == "step";
}

class TumbleScenario : public ::testing::TestWithParam<std::string>
{ };

// One step of a tumbling rocket: the Alpha III (three fins), Big Blue (four), the Alpha III
// with pods, the cluster pods, the end plate rocket, Iso-Haisu, the separated boosters of the
// Falcon 9 Heavy, the extra step upon ground hit, and the cluster pods at their terminal
// velocity, free and before an event (the tentative time step: see below).
TEST_P(TumbleScenario, TheStepIsOpenRockets)
{
    EXPECT_EQ(javaScenarioDifferences(GetParam()), "");
}

INSTANTIATE_TEST_SUITE_P(BasicTumbleStepper, TumbleScenario,
                         ::testing::ValuesIn(stepScenarioNames(isTumbleStep)),
                         [](const ::testing::TestParamInfo<std::string>& paramInfo) {
                             return scenarioTestName(paramInfo.param);
                         });

TEST(BasicTumbleStepper, TheScenariosCoverFinsPodsAndBoosters)
{
    const std::vector<std::string> expected{
        "tumble-alpha",
        "tumble-bigblue",
        "tumble-pods",
        "tumble-clusterpods",
        "tumble-endplate",
        "tumble-isohaisu",
        "tumble-falcon-boosters",
        "tumble-ground-hit",
        "tumble-terminal-velocity",
        "tumble-terminal-velocity-before-event",
    };
    EXPECT_EQ(stepScenarioNames(isTumbleStep), expected);
}

/// The time step the tumble stepper takes from the state of the scenario @p name when the next
/// event is @p maxTimeStep away, and the time that passed in the status; NaN for a scenario
/// that does not exist.
struct TumbleStep
{
    double timeStep{std::numeric_limits<double>::quiet_NaN()};
    double timePassed{std::numeric_limits<double>::quiet_NaN()};
};

[[nodiscard]] TumbleStep tumbleStepOf(std::string_view name, double maxTimeStep)
{
    TumbleStep          taken;
    BasicTumbleStepper  stepper;
    InitializedScenario f(name, stepper);
    if (f.status == nullptr)
    {
        return taken;
    }
    const double startTime = f.status->getSimulationTime();
    stepper.step(*f.status, maxTimeStep);
    taken.timeStep   = stepper.getStore().timeStep;
    taken.timePassed = f.status->getSimulationTime() - startTime;
    return taken;
}

// What the two "tumble-terminal-velocity" pins say, by name: the tentative time step of the
// Euler steppers (RECOVERY_TIME_STEP, 0.5 s) is the step a body takes that falls at its terminal
// velocity, when that is above g times one second (below it the oscillation test shortens the
// step): the cluster rocket, which tumbles at 16 m/s. It is the step of every tumbling booster
// near the end of its fall, and the other scenarios of the Euler steppers all end with a
// shorter one (1/|a|, the oscillation test, the ground, the minimum). Java: 0.5 s, and 0.299 s
// before an event that is 0.3 s away.
TEST(BasicTumbleStepper, AtTerminalVelocityTheStepIsTheTentativeTimeStep)
{
    const TumbleStep free = tumbleStepOf("tumble-terminal-velocity", 10.0);
    EXPECT_EQ(free.timeStep, 0.5);
    EXPECT_EQ(free.timeStep, AbstractEulerStepper::kRecoveryTimeStep);
    EXPECT_EQ(free.timePassed, 0.5);

    // An event nearer than the tentative step: the step ends the minimum time step before it.
    const TumbleStep before = tumbleStepOf("tumble-terminal-velocity-before-event", 0.3);
    EXPECT_EQ(before.timeStep, 0.3 - AbstractSimulationStepper::kMinTimeStep);
    EXPECT_EQ(before.timeStep, 0.299);
}

}  // namespace
