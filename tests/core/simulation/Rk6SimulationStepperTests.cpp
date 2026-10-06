#include "QtRocket/simulation/Rk6SimulationStepper.h"

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/simulation/AbstractRkSimulationStepper.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Rk4SimulationStepper.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/SimulationStepper.h"
#include "QtRocket/simulation/exception/SimulationCalculationException.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/Coordinate.h"
#include "simulation/JitterRemoval.h"
#include "simulation/StepperTestSupport.h"

namespace
{

using QtRocket::AbstractRkSimulationStepper;
using QtRocket::Coordinate;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::Rk4SimulationStepper;
using QtRocket::Rk6SimulationStepper;
using QtRocket::SimulationCalculationException;
using QtRocket::SimulationListener;
using QtRocket::SimulationStatus;
using QtRocket::SimulationStepper;
using QtRocket::Test::DeterministicStepper;
using QtRocket::Test::javaScenarioDifferences;
using QtRocket::Test::JitterRemoval;
using QtRocket::Test::scenarioTestName;
using QtRocket::Test::stepScenarioNames;
using QtRocket::Test::StepScenarioPin;

static_assert(std::is_base_of_v<SimulationStepper, Rk6SimulationStepper>);
static_assert(std::is_base_of_v<AbstractRkSimulationStepper, Rk6SimulationStepper>);
static_assert(!std::is_copy_assignable_v<Rk6SimulationStepper>);
static_assert(std::has_virtual_destructor_v<Rk6SimulationStepper>);

// OpenRocket has no test of one step of RK6SimulationStepper (RungeKuttaSimulationStepperTest,
// ported in runge_kutta_simulation_stepper_tests.cpp, tests its order on smooth problems). The
// expectations are what probes/tier8b-steppers/StepperProbe.java printed for OpenRocket's
// stepper (StepperScenarioPins.cpp; see StepperTestSupport.h).

/// The scenarios of this file: steps of the RK6 stepper with the jitter and without it.
[[nodiscard]] bool isRk6Step(const StepScenarioPin& pin)
{
    return pin.stepper == "RK6" && pin.action == "step" &&
           (pin.listener == "none" || pin.listener == "jitter-removal");
}

class Rk6Scenario : public ::testing::TestWithParam<std::string>
{ };

// The complete status after the step, the chosen time step, the data store as the seventh
// evaluation left it, every column of the row the step wrote, the events and the warnings. In
// the two scenarios whose values run out of range the message of the exception is the one of
// the RK4 stepper, where Java's RK6 stepper shows an untranslated key (see
// AbstractRkSimulationStepper, deviations).
TEST_P(Rk6Scenario, TheStepIsOpenRockets)
{
    EXPECT_EQ(javaScenarioDifferences(GetParam()), "");
}

INSTANTIATE_TEST_SUITE_P(Rk6SimulationStepper, Rk6Scenario,
                         ::testing::ValuesIn(stepScenarioNames(isRk6Step)),
                         [](const ::testing::TestParamInfo<std::string>& paramInfo) {
                             return scenarioTestName(paramInfo.param);
                         });

TEST(Rk6SimulationStepper, TheScenariosCoverAFlightWithAndWithoutTheJitter)
{
    const std::vector<std::string> expected{
        "rod-rk6",
        "thrust-rk6",
        "coast-rk6",
        "supersonic-rk6",
        "rod-rk6-nojitter",
        "thrust-rk6-nojitter",
        "coast-rk6-nojitter",
        "supersonic-rk6-nojitter",
        "landed-rk6",
        "falcon-rk6-nojitter",
        "too-large-rotation-rk6",
        "too-large-position-rk6",
    };
    EXPECT_EQ(stepScenarioNames(isRk6Step), expected);
}

using Initialized = QtRocket::Test::InitializedScenario;

TEST(Rk6SimulationStepper, AStepEvaluatesTheForcesSevenTimes)
{
    const JitterRemoval                              jitterRemoval;
    std::vector<std::shared_ptr<SimulationListener>> listeners;
    jitterRemoval.install(listeners);
    Rk6SimulationStepper stepper;
    Initialized          f("thrust-rk6", stepper, listeners);
    ASSERT_NE(f.status, nullptr);

    stepper.step(*f.status, 0.5);
    EXPECT_EQ(jitterRemoval.replacements(), 7);
    stepper.step(*f.status, 0.5);
    EXPECT_EQ(jitterRemoval.replacements(), 14);
}

TEST(Rk6SimulationStepper, AStepStoresTheStatusAtItsStartAndAdvancesItByTheTimeStep)
{
    Rk6SimulationStepper stepper;
    Initialized          f("thrust-rk6", stepper);
    ASSERT_NE(f.status, nullptr);
    const double startTime = f.status->getSimulationTime();

    stepper.step(*f.status, 0.5);

    const FlightDataBranch& branch   = *f.built.branch;
    const double            timeStep = stepper.getStore().timeStep;
    EXPECT_EQ(branch.getLength(), 1U);
    EXPECT_EQ(branch.getLast(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME)), startTime);
    EXPECT_EQ(branch.getLast(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME_STEP)), timeStep);
    EXPECT_GT(timeStep, 0.0);
    EXPECT_EQ(f.status->getSimulationTime(), startTime + timeStep);
}

/// The values of the forces in the last rows of @p a and @p b that are not exactly equal.
[[nodiscard]] std::string firstRowDifferences(const FlightDataBranch& a, const FlightDataBranch& b)
{
    std::string differences;
    for (const FlightDataTypeId id :
         {FlightDataTypeId::TYPE_DRAG_FORCE, FlightDataTypeId::TYPE_ACCELERATION_TOTAL,
          FlightDataTypeId::TYPE_PITCH_MOMENT_COEFF, FlightDataTypeId::TYPE_AOA,
          FlightDataTypeId::TYPE_CORRECTIVE_MOMENT_COEFF, FlightDataTypeId::TYPE_TIME_STEP})
    {
        const FlightDataType& type = FlightDataType::builtin(id);
        if (a.getLast(type) != b.getLast(type))
        {
            differences += type.getName() + " ";
        }
    }
    return differences;
}

// The first evaluation, the time step and the row are those of the RK4 stepper: the two
// steppers differ in the stages after it only. (The same state, the same seed: the first
// jitter is the same too.)
TEST(Rk6SimulationStepper, TheFirstEvaluationAndTheTimeStepAreThoseOfRk4)
{
    Rk4SimulationStepper rk4;
    Initialized          four("coast-rk4", rk4);
    Rk6SimulationStepper rk6;
    Initialized          six("coast-rk4", rk6);
    ASSERT_NE(four.status, nullptr);
    ASSERT_NE(six.status, nullptr);

    rk4.step(*four.status, 0.5);
    rk6.step(*six.status, 0.5);

    EXPECT_EQ(rk4.getStore().timeStep, rk6.getStore().timeStep);
    EXPECT_EQ(firstRowDifferences(*four.built.branch, *six.built.branch), "");
    // The results of the two methods differ, a little.
    const double z4 = four.status->getRocketPosition().z;
    const double z6 = six.status->getRocketPosition().z;
    EXPECT_NE(z4, z6);
    EXPECT_NEAR(z4, z6, 1e-6);
}

/// A deterministic stepper (no aerodynamics) on the "thrust" state with the velocity
/// @p velocity, stepped once; the message of the SimulationCalculationException, "none"
/// without one.
[[nodiscard]] std::string thrownForVelocity(const Coordinate& velocity)
{
    DeterministicStepper<Rk6SimulationStepper> stepper(
        [](const SimulationStatus& /*status*/) { return Coordinate::kZero; });
    Initialized f("thrust-rk6", stepper);
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

// Deviation from OpenRocket: Java's RK6 stepper has no translation of its own and throws with
// the key "error.valuesTooLarge" as the message; here it is the text of the RK4 stepper.
TEST(Rk6SimulationStepper, AVelocityOutOfRangeEndsTheStepWithTheMessageOfRk4)
{
    EXPECT_EQ(thrownForVelocity(Coordinate(0, 0, 2.0e9)),
              "Simulation values exceeded limits.  Try selecting a shorter time step.");
    EXPECT_EQ(thrownForVelocity(Coordinate(0, 0, 1.0e9)), "none");
}

TEST(Rk6SimulationStepper, ANaNTimeStepRecordsTheStatusAndPutsTheRocketOnTheGround)
{
    Rk6SimulationStepper stepper;
    Initialized          f("landed-rk6", stepper);
    ASSERT_NE(f.status, nullptr);
    const double startTime = f.status->getSimulationTime();

    stepper.step(*f.status, std::numeric_limits<double>::quiet_NaN());

    EXPECT_EQ(f.built.branch->getLength(), 1U);
    EXPECT_EQ(f.status->getSimulationTime(), startTime);
    EXPECT_EQ(f.status->getRocketPosition().z, 0.0);
    EXPECT_TRUE(f.status->getRocketVelocity().exactlyEquals(Coordinate::kZero));
    EXPECT_TRUE(std::isnan(stepper.getStore().timeStep));
}

}  // namespace
