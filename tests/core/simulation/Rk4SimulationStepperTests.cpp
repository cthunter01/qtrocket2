#include "QtRocket/simulation/Rk4SimulationStepper.h"

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/simulation/AbstractRkSimulationStepper.h"
#include "QtRocket/simulation/AbstractSimulationStepper.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/SimulationStepper.h"
#include "QtRocket/simulation/exception/SimulationCalculationException.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/Coordinate.h"
#include "simulation/JitterRemoval.h"
#include "simulation/SimulationTestSupport.h"
#include "simulation/StepperTestSupport.h"

namespace
{

using QtRocket::AbstractRkSimulationStepper;
using QtRocket::Coordinate;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::Rk4SimulationStepper;
using QtRocket::SimulationCalculationException;
using QtRocket::SimulationListener;
using QtRocket::SimulationStatus;
using QtRocket::SimulationStepper;
using QtRocket::Test::bugText;
using QtRocket::Test::buildScenarioStatus;
using QtRocket::Test::DeterministicStepper;
using QtRocket::Test::findStepScenario;
using QtRocket::Test::javaScenarioDifferences;
using QtRocket::Test::JitterRemoval;
using QtRocket::Test::ScenarioStatus;
using QtRocket::Test::scenarioTestName;
using QtRocket::Test::stepScenarioNames;
using QtRocket::Test::StepScenarioPin;
using QtRocket::Test::stored;

// A stepper is used through the interface and is not assigned.
static_assert(std::is_base_of_v<SimulationStepper, Rk4SimulationStepper>);
static_assert(std::is_base_of_v<AbstractRkSimulationStepper, Rk4SimulationStepper>);
static_assert(!std::is_copy_assignable_v<Rk4SimulationStepper>);
static_assert(std::has_virtual_destructor_v<Rk4SimulationStepper>);

// OpenRocket has no test of one step of RK4SimulationStepper. The expectations are what
// probes/tier8b-steppers/StepperProbe.java printed for OpenRocket's stepper on statuses built by
// hand (StepperScenarioPins.cpp; see StepperTestSupport.h).

/// The scenarios of this file: steps of the RK4 stepper with the jitter and without it.
[[nodiscard]] bool isRk4Step(const StepScenarioPin& pin)
{
    return pin.stepper == "RK4" && pin.action == "step" &&
           (pin.listener == "none" || pin.listener == "jitter-removal");
}

class Rk4Scenario : public ::testing::TestWithParam<std::string>
{ };

// The complete status after the step, the chosen time step, the data store as the fourth
// evaluation left it, every column of the row the step wrote, the events and the warnings.
TEST_P(Rk4Scenario, TheStepIsOpenRockets)
{
    EXPECT_EQ(javaScenarioDifferences(GetParam()), "");
}

INSTANTIATE_TEST_SUITE_P(Rk4SimulationStepper, Rk4Scenario,
                         ::testing::ValuesIn(stepScenarioNames(isRk4Step)),
                         [](const ::testing::TestParamInfo<std::string>& paramInfo) {
                             return scenarioTestName(paramInfo.param);
                         });

TEST(Rk4SimulationStepper, TheScenariosCoverAFlightWithAndWithoutTheJitter)
{
    const std::vector<std::string> expected{
        "rod-rk4",
        "thrust-rk4",
        "coast-rk4",
        "supersonic-rk4",
        "rod-rk4-nojitter",
        "thrust-rk4-nojitter",
        "coast-rk4-nojitter",
        "supersonic-rk4-nojitter",
        "coast-large-aoa-rk4",
        "pad-rk4",
        "landed-rk4",
        "thrust-rk4-two-steps",
        "thrust-rk4-short",
        "falcon-rk4",
        "clusterpods-rk4",
        "multistage-rk4",
        "too-large-rotation-rk4",
        "too-large-position-rk4",
    };
    EXPECT_EQ(stepScenarioNames(isRk4Step), expected);
}

// ============================================================================ test support

using Initialized = QtRocket::Test::InitializedScenario;

/// The value of the built-in type @p id in the last row of @p branch.
[[nodiscard]] double last(const FlightDataBranch& branch, FlightDataTypeId id)
{
    return branch.getLast(FlightDataType::builtin(id));
}

/// The pitch moment coefficient the store holds after one step of a new stepper from the
/// "thrust" state, with the random seed @p seed.
[[nodiscard]] double pitchMomentAfterOneStep(int seed)
{
    Rk4SimulationStepper   stepper;
    const StepScenarioPin* pin = findStepScenario("thrust-rk4");
    if (pin == nullptr)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    ScenarioStatus built = buildScenarioStatus(*pin, {});
    built.conditions->setRandomSeed(seed);
    SimulationStatus status = stepper.initialize(std::move(*built.status));
    stepper.step(status, 0.5);
    return stored(stepper.getStore().forces).getCm();
}

// ================================================================================= a step

TEST(Rk4SimulationStepper, AStepStoresTheStatusAtItsStartAndAdvancesItByTheTimeStep)
{
    Rk4SimulationStepper stepper;
    Initialized          f("thrust-rk4", stepper);
    ASSERT_NE(f.status, nullptr);
    const double startTime     = f.status->getSimulationTime();
    const double startAltitude = f.status->getRocketPosition().z;

    stepper.step(*f.status, 0.5);

    const FlightDataBranch& branch   = *f.built.branch;
    const double            timeStep = stepper.getStore().timeStep;
    EXPECT_EQ(branch.getLength(), 1U);
    EXPECT_EQ(last(branch, FlightDataTypeId::TYPE_TIME), startTime);
    EXPECT_EQ(last(branch, FlightDataTypeId::TYPE_ALTITUDE), startAltitude);
    EXPECT_EQ(last(branch, FlightDataTypeId::TYPE_TIME_STEP), timeStep);
    EXPECT_GT(timeStep, 0.0);
    EXPECT_EQ(f.status->getSimulationTime(), startTime + timeStep);
    EXPECT_GT(f.status->getRocketPosition().z, startAltitude);

    // The next step opens the next row, at the new time.
    stepper.step(*f.status, 0.5);
    EXPECT_EQ(branch.getLength(), 2U);
    EXPECT_EQ(last(branch, FlightDataTypeId::TYPE_TIME), startTime + timeStep);
}

TEST(Rk4SimulationStepper, AStepEvaluatesTheForcesFourTimes)
{
    const JitterRemoval                              jitterRemoval;
    std::vector<std::shared_ptr<SimulationListener>> listeners;
    jitterRemoval.install(listeners);
    Rk4SimulationStepper stepper;
    Initialized          f("thrust-rk4", stepper, listeners);
    ASSERT_NE(f.status, nullptr);

    stepper.step(*f.status, 0.5);
    EXPECT_EQ(jitterRemoval.replacements(), 4);
    stepper.step(*f.status, 0.5);
    EXPECT_EQ(jitterRemoval.replacements(), 8);
}

TEST(Rk4SimulationStepper, ANaNTimeStepRecordsTheStatusAndPutsTheRocketOnTheGround)
{
    Rk4SimulationStepper stepper;
    Initialized          f("landed-rk4", stepper);
    ASSERT_NE(f.status, nullptr);
    const double     startTime = f.status->getSimulationTime();
    const Coordinate start     = f.status->getRocketPosition();

    stepper.step(*f.status, std::numeric_limits<double>::quiet_NaN());

    // The row holds the status and the forces at the moment of impact.
    const FlightDataBranch& branch = *f.built.branch;
    EXPECT_EQ(branch.getLength(), 1U);
    EXPECT_EQ(last(branch, FlightDataTypeId::TYPE_ALTITUDE), start.z);
    EXPECT_EQ(last(branch, FlightDataTypeId::TYPE_VELOCITY_Z), -18.0);
    EXPECT_TRUE(std::isnan(last(branch, FlightDataTypeId::TYPE_TIME_STEP)));
    // (The rocket comes down tail first: the axial drag is negative.)
    EXPECT_LT(last(branch, FlightDataTypeId::TYPE_DRAG_FORCE), 0.0);

    // The status lies where it came down, and no time has passed.
    EXPECT_EQ(f.status->getSimulationTime(), startTime);
    EXPECT_TRUE(f.status->getRocketPosition().exactlyEquals(Coordinate(start.x, start.y, 0)));
    EXPECT_TRUE(f.status->getRocketVelocity().exactlyEquals(Coordinate::kZero));
    EXPECT_TRUE(std::isnan(stepper.getStore().timeStep));
    EXPECT_EQ(stepper.getStore().dragForce, 0.0);
}

// ================================================================================ the jitter

TEST(Rk4SimulationStepper, TheJitterFollowsTheRandomSeed)
{
    const double seedZero  = pitchMomentAfterOneStep(0);
    const double again     = pitchMomentAfterOneStep(0);
    const double otherSeed = pitchMomentAfterOneStep(-7);
    EXPECT_EQ(seedZero, again);
    EXPECT_NE(seedZero, otherSeed);
    // The two differ by the jitter of the fourth evaluation only: less than its full range.
    EXPECT_LT(std::abs(seedZero - otherSeed), 2 * AbstractRkSimulationStepper::kPitchYawRandom);
}

TEST(Rk4SimulationStepper, InitializeStartsTheJitterAnew)
{
    // Two steppers from the same state draw the same jitter; one stepper that goes on draws
    // the next numbers, and after initialize() the first ones again.
    Rk4SimulationStepper first;
    Initialized          a("thrust-rk4", first);
    ASSERT_NE(a.status, nullptr);
    first.step(*a.status, 0.5);
    const double firstStep = stored(first.getStore().forces).getCm();

    Rk4SimulationStepper second;
    Initialized          b("thrust-rk4", second);
    ASSERT_NE(b.status, nullptr);
    second.step(*b.status, 0.5);
    EXPECT_EQ(stored(second.getStore().forces).getCm(), firstStep);

    // The same stepper, initialized again with the same state.
    Initialized c("thrust-rk4", first);
    ASSERT_NE(c.status, nullptr);
    first.step(*c.status, 0.5);
    EXPECT_EQ(stored(first.getStore().forces).getCm(), firstStep);
}

// =========================================================================== out of range

/// A deterministic stepper (no aerodynamics) on the "thrust" state with the velocity
/// @p velocity, stepped once; the message of the SimulationCalculationException, "none"
/// without one.
[[nodiscard]] std::string thrownForVelocity(const Coordinate& velocity)
{
    DeterministicStepper<Rk4SimulationStepper> stepper(
        [](const SimulationStatus& /*status*/) { return Coordinate::kZero; });
    Initialized f("thrust-rk4", stepper);
    if (f.status == nullptr)
    {
        return "no status";
    }
    f.status->setRocketVelocity(velocity);
    try
    {
        stepper.step(*f.status, 0.01);
    }
    catch (const SimulationCalculationException& e)
    {
        if (e.getFlightDataBranch() != f.built.branch)
        {
            return "the exception does not carry the branch of the status";
        }
        return e.getMessage().value_or("null");
    }
    return "none";
}

// The squares of the velocity, the position and the rotation velocity must not exceed 1e18
// (Java: `> 1.0e18`). The aerodynamics are meaningless long before a velocity of 1e9 m/s, so
// this limit is reached with a stepper without them; the limits of the position and of the
// rotation velocity are among the scenarios.
TEST(Rk4SimulationStepper, AVelocityOutOfRangeEndsTheStepWithACalculationException)
{
    EXPECT_EQ(thrownForVelocity(Coordinate(0, 0, 2.0e9)),
              "Simulation values exceeded limits.  Try selecting a shorter time step.");
    EXPECT_EQ(thrownForVelocity(Coordinate(1.1e9, 0, 0)),
              AbstractRkSimulationStepper::kValuesTooLarge);
    // 1e9 squared is 1e18 exactly, which is not beyond the limit.
    EXPECT_EQ(thrownForVelocity(Coordinate(0, 0, 1.0e9)), "none");
    EXPECT_EQ(thrownForVelocity(Coordinate(0, 0, 42)), "none");
}

// Java: IllegalArgumentException("Stepping backwards in time, timestep=" + timeStep). A time
// step can only become negative when the time step of the conditions is: the step is then at
// least a twentieth of that.
TEST(Rk4SimulationStepper, ANegativeTimeStepIsABug)
{
    DeterministicStepper<Rk4SimulationStepper> stepper(
        [](const SimulationStatus& /*status*/) { return Coordinate::kZero; });
    const StepScenarioPin* pin = findStepScenario("thrust-rk4");
    ASSERT_NE(pin, nullptr);
    ScenarioStatus built = buildScenarioStatus(*pin, {});
    ASSERT_NE(built.status, nullptr);
    built.conditions->setTimeStep(-1.0);
    SimulationStatus status = stepper.initialize(std::move(*built.status));

    EXPECT_EQ(bugText([&] { stepper.step(status, -0.5); }),
              "Stepping backwards in time, timestep=-0.05");
}

}  // namespace
