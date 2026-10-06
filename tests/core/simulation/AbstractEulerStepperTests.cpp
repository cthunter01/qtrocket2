#include "QtRocket/simulation/AbstractEulerStepper.h"

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/simulation/AbstractSimulationStepper.h"
#include "QtRocket/simulation/AccelerationData.h"
#include "QtRocket/simulation/BasicLandingStepper.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Quaternion.h"
#include "rocket/TestRockets.h"
#include "simulation/JitterRemoval.h"
#include "simulation/SimulationStatusSupport.h"
#include "simulation/SimulationTestSupport.h"
#include "simulation/StepperTestSupport.h"

namespace
{

using QtRocket::AbstractEulerStepper;
using QtRocket::AbstractSimulationStepper;
using QtRocket::AerodynamicForces;
using QtRocket::BasicLandingStepper;
using QtRocket::Coordinate;
using QtRocket::FlightConditions;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::SimulationConditions;
using QtRocket::SimulationListener;
using QtRocket::SimulationStatus;
using QtRocket::Test::bugText;
using QtRocket::Test::CloneCountingListener;
using QtRocket::Test::InitializedScenario;
using QtRocket::Test::javaScenarioDifferences;
using QtRocket::Test::JitterRemoval;
using QtRocket::Test::newBranch;
using QtRocket::Test::scenarioTestName;
using QtRocket::Test::statusConfiguration;
using QtRocket::Test::stepScenarioNames;
using QtRocket::Test::StepScenarioPin;
using QtRocket::Test::stored;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;

static_assert(std::is_abstract_v<AbstractEulerStepper>);
static_assert(std::is_base_of_v<AbstractSimulationStepper, AbstractEulerStepper>);

// OpenRocket has no test of AbstractEulerStepper itself. The expectations are what
// probes/tier8b-steppers/StepperProbe.java printed for one step (or three) of OpenRocket's
// BasicLandingStepper on statuses built by hand (StepperScenarioPins.cpp).

/// The scenarios of this file: steps of the landing stepper.
[[nodiscard]] bool isLandingStep(const StepScenarioPin& pin)
{
    return pin.stepper == "LANDING" && pin.action == "step";
}

class LandingScenario : public ::testing::TestWithParam<std::string>
{ };

// The status after the step, the chosen time step, the data store and every column of the
// row. The scenarios take every way the time step has of being chosen:
// - "landing": descending under the parachute in a wind, 1 / |acceleration|;
// - "landing-fast", "landing-very-fast", "landing-rising": so much drag that the step is the
//   minimum time step;
// - "landing-last-step": the step that would end below the ground ends on it (z is exactly 0);
// - "landing-terminal-velocity": the acceleration would change sign within the step, so the
//   step is cut to where it would be zero (oscillation avoidance);
// - "landing-apogee": the vertical velocity would change sign, so the step ends at the apogee;
// - "landing-before-event": the step ends a minimum time step before the next event;
// - "landing-at-event", "landing-min-step": the next event is closer than the minimum time
//   step, which is taken all the same;
// - "landing-at-rest", "landing-nothing-deployed": without drag, a free fall;
// - "landing-three-steps": three steps in a row;
// - "landing-ground-hit": the extra step upon ground hit, with a NaN time step;
// - "landing-listener": a listener doubles the drag coefficient of the step;
// - "landing-zero-mass": a rocket without mass aborts the simulation and then fails;
// - "landing-wgs84": the world position on the ellipsoid;
// - "hooks-landing": the order of the hooks of a step.
TEST_P(LandingScenario, TheStepIsOpenRockets)
{
    EXPECT_EQ(javaScenarioDifferences(GetParam()), "");
}

INSTANTIATE_TEST_SUITE_P(AbstractEulerStepper, LandingScenario,
                         ::testing::ValuesIn(stepScenarioNames(isLandingStep)),
                         [](const ::testing::TestParamInfo<std::string>& paramInfo) {
                             return scenarioTestName(paramInfo.param);
                         });

TEST(AbstractEulerStepper, TheScenariosCoverEveryRuleOfTheTimeStep)
{
    const std::vector<std::string> expected{
        "landing",
        "landing-fast",
        "landing-very-fast",
        "landing-last-step",
        "landing-terminal-velocity",
        "landing-before-event",
        "landing-at-event",
        "landing-min-step",
        "landing-apogee",
        "landing-rising",
        "landing-at-rest",
        "landing-three-steps",
        "landing-ground-hit",
        "landing-nothing-deployed",
        "landing-listener",
        "landing-zero-mass",
        "hooks-landing",
        "landing-wgs84",
    };
    EXPECT_EQ(stepScenarioNames(isLandingStep), expected);
}

TEST(AbstractEulerStepper, TheTentativeTimeStepIsJavas)
{
    EXPECT_EQ(AbstractEulerStepper::kRecoveryTimeStep, 0.5);
}

/// The time step a landing stepper takes from the state of the scenario @p name, and where the
/// status is afterwards.
struct StepTaken
{
    double     timeStep{std::numeric_limits<double>::quiet_NaN()};
    double     time{std::numeric_limits<double>::quiet_NaN()};
    Coordinate position;
    Coordinate velocity;
    double     rowTimeStep{std::numeric_limits<double>::quiet_NaN()};
};

[[nodiscard]] StepTaken stepOf(std::string_view name, double maxTimeStep)
{
    StepTaken           taken;
    BasicLandingStepper stepper;
    InitializedScenario f(name, stepper);
    if (f.status == nullptr)
    {
        return taken;
    }
    const double startTime = f.status->getSimulationTime();
    stepper.step(*f.status, maxTimeStep);
    taken.timeStep = stepper.getStore().timeStep;
    taken.time     = f.status->getSimulationTime() - startTime;
    taken.position = f.status->getRocketPosition();
    taken.velocity = f.status->getRocketVelocity();
    taken.rowTimeStep =
        f.built.branch->getLast(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME_STEP));
    return taken;
}

// What the pins say, by name: the rules of the time step.
TEST(AbstractEulerStepper, TheLastStepEndsOnTheGround)
{
    const StepTaken taken = stepOf("landing-last-step", 10.0);
    EXPECT_EQ(taken.position.z, 0.0);
    EXPECT_LT(taken.velocity.z, 0.0);
    EXPECT_GT(taken.timeStep, AbstractSimulationStepper::kMinTimeStep);
    EXPECT_LT(taken.timeStep, 0.03);
    // The row of the step holds the time step that was taken.
    EXPECT_EQ(taken.rowTimeStep, taken.timeStep);
}

TEST(AbstractEulerStepper, TheStepStopsShortOfTheNextEvent)
{
    // The acceleration allows more than 0.05 s; the next event is 0.05 s away.
    const StepTaken free = stepOf("landing-at-rest", 10.0);
    EXPECT_GT(free.timeStep, 0.05);
    const StepTaken before = stepOf("landing-at-rest", 0.05);
    EXPECT_EQ(before.timeStep, 0.05 - AbstractSimulationStepper::kMinTimeStep);
    // An event closer than the minimum time step: the minimum time step.
    const StepTaken close = stepOf("landing-at-rest", 0.0004);
    EXPECT_EQ(close.timeStep, AbstractSimulationStepper::kMinTimeStep);
    const StepTaken atMinimum = stepOf("landing-at-rest", 0.001);
    EXPECT_EQ(atMinimum.timeStep, AbstractSimulationStepper::kMinTimeStep);
}

TEST(AbstractEulerStepper, TheStepEndsAtApogee)
{
    const StepTaken taken = stepOf("landing-apogee", 10.0);
    // The vertical velocity at the end of the step is zero (v + a t with t = |v / a|), up to
    // the rounding of the product.
    EXPECT_NEAR(taken.velocity.z, 0.0, 1e-15);
    EXPECT_GT(taken.position.z, 150.0);
}

TEST(AbstractEulerStepper, ANaNTimeStepRecordsTheImpactAndLandsTheRocket)
{
    BasicLandingStepper stepper;
    InitializedScenario f("landing-ground-hit", stepper);
    ASSERT_NE(f.status, nullptr);
    const double startTime = f.status->getSimulationTime();

    stepper.step(*f.status, std::numeric_limits<double>::quiet_NaN());

    const FlightDataBranch& branch = *f.built.branch;
    EXPECT_EQ(branch.getLength(), 1U);
    // The row: the descent at the moment of impact, with the drag of the parachute.
    EXPECT_EQ(branch.getLast(FlightDataType::builtin(FlightDataTypeId::TYPE_VELOCITY_Z)), -4.5);
    EXPECT_GT(branch.getLast(FlightDataType::builtin(FlightDataTypeId::TYPE_DRAG_FORCE)), 0.0);
    EXPECT_TRUE(
        std::isnan(branch.getLast(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME_STEP))));
    // The status: on the ground, at rest, at the same time.
    EXPECT_EQ(f.status->getSimulationTime(), startTime);
    EXPECT_EQ(f.status->getRocketPosition().z, 0.0);
    EXPECT_TRUE(f.status->getRocketVelocity().exactlyEquals(Coordinate::kZero));
}

// The step computes no attitude: the angle of attack and the rates of the flight conditions
// are NaN, the forces hold the drag coefficient only, and there is no rotation.
TEST(AbstractEulerStepper, AStepHasNoAttitudeDynamics)
{
    BasicLandingStepper stepper;
    InitializedScenario f("landing", stepper);
    ASSERT_NE(f.status, nullptr);
    const QtRocket::Quaternion orientation = f.status->getRocketOrientationQuaternion();

    stepper.step(*f.status, 10.0);

    const AbstractSimulationStepper::DataStore& store      = stepper.getStore();
    const FlightConditions&                     conditions = stored(store.flightConditions);
    const AerodynamicForces&                    forces     = stored(store.forces);
    EXPECT_TRUE(std::isnan(conditions.getAOA()));
    EXPECT_TRUE(std::isnan(conditions.getRollRate()));
    EXPECT_TRUE(std::isnan(conditions.getPitchRate()));
    EXPECT_TRUE(std::isnan(conditions.getYawRate()));
    EXPECT_EQ(store.thrustForce, 0.0);
    const double cd = forces.getCD();
    EXPECT_GT(cd, 100.0);
    EXPECT_EQ(forces.getCDaxial(), cd);
    EXPECT_EQ(forces.getPressureCD(), cd);
    EXPECT_EQ(forces.getFrictionCD(), 0.0);
    EXPECT_EQ(forces.getBaseCD(), 0.0);
    EXPECT_TRUE(std::isnan(forces.getCN()));
    EXPECT_TRUE(stored(store.accelerationData)
                    .getRotationalAccelerationWC()
                    .exactlyEquals(Coordinate::kZero));
    EXPECT_EQ(f.status->getRocketOrientationQuaternion(), orientation);
}

// Java: `return status;`. The status that comes back is the one that went in: the same
// conditions, listeners and configuration, nothing cloned.
TEST(AbstractEulerStepper, InitializeGivesTheStatusBackAsItIs)
{
    const TestEstesAlphaIII alpha;
    const auto              conditions = std::make_shared<SimulationConditions>();
    const auto              counter    = std::make_shared<CloneCountingListener>();
    conditions->getSimulationListenerList().push_back(counter);
    SimulationStatus original(statusConfiguration(*alpha.rocket, testFcid(0)), conditions);
    const std::shared_ptr<FlightDataBranch> branch = newBranch();
    original.setFlightDataBranch(branch);
    original.setSimulationTime(7.5);
    const QtRocket::FlightConfiguration* configuration = &original.getConfiguration();
    const QtRocket::ModId                modId         = original.getModId();

    BasicLandingStepper    stepper;
    const SimulationStatus status = stepper.initialize(std::move(original));

    EXPECT_EQ(status.getSimulationConditions(), conditions);
    EXPECT_EQ(counter->clones(), 0);
    EXPECT_EQ(&status.getConfiguration(), configuration);
    EXPECT_EQ(status.getFlightDataBranch(), branch);
    EXPECT_EQ(status.getSimulationTime(), 7.5);
    EXPECT_EQ(status.getModId(), modId);
}

// Java: Coordinate.normalize() throws an IllegalStateException below a length of 1e-7, and the
// stepper normalizes an airspeed above MathUtil.EPSILON (1e-8).
TEST(AbstractEulerStepper, AnAirspeedTooSmallToNormalizeIsABug)
{
    BasicLandingStepper stepper;
    InitializedScenario f("landing-at-rest", stepper);
    ASSERT_NE(f.status, nullptr);
    f.status->setRocketVelocity(Coordinate(0, 0, -5e-8));
    EXPECT_EQ(bugText([&] { stepper.step(*f.status, 10.0); }), "Cannot normalize zero coordinate");

    // Below MathUtil::kEpsilon there is no drag to direct: the step is a free fall.
    BasicLandingStepper other;
    InitializedScenario g("landing-at-rest", other);
    ASSERT_NE(g.status, nullptr);
    g.status->setRocketVelocity(Coordinate(0, 0, -5e-9));
    EXPECT_EQ(bugText([&] { other.step(*g.status, 10.0); }), "<none>");
}

// The steppers of the descent fire the post-aerodynamic-calculation hook too, with forces that
// hold a drag coefficient only. The jitter removal of the golden tests (JitterRemoval.h) must
// leave those alone, as the Java harness does: it has no jitter to remove there.
TEST(AbstractEulerStepper, TheJitterRemovalLeavesTheForcesOfADescentAlone)
{
    const JitterRemoval                              jitterRemoval;
    std::vector<std::shared_ptr<SimulationListener>> listeners;
    jitterRemoval.install(listeners);
    BasicLandingStepper stepper;
    InitializedScenario f("landing", stepper, listeners);
    BasicLandingStepper plain;
    InitializedScenario g("landing", plain);
    ASSERT_NE(f.status, nullptr);
    ASSERT_NE(g.status, nullptr);

    stepper.step(*f.status, 10.0);
    plain.step(*g.status, 10.0);

    EXPECT_EQ(jitterRemoval.replacements(), 0);
    EXPECT_EQ(stepper.getStore().timeStep, plain.getStore().timeStep);
    EXPECT_EQ(stepper.getStore().dragForce, plain.getStore().dragForce);
    EXPECT_TRUE(f.status->getRocketPosition().exactlyEquals(g.status->getRocketPosition()));
    EXPECT_TRUE(f.status->getRocketVelocity().exactlyEquals(g.status->getRocketVelocity()));
    // No listener affected the simulation: there is no warning.
    EXPECT_TRUE(f.built.branch->getEvents().empty());
}

TEST(AbstractEulerStepper, TheAccelerationNeedsFlightConditions)
{
    BasicLandingStepper stepper;
    InitializedScenario f("landing", stepper);
    ASSERT_NE(f.status, nullptr);
    // Java: a NullPointerException.
    EXPECT_EQ(bugText([&] { stepper.calculateAcceleration(*f.status, stepper.getStore()); }),
              "The data store has no flight conditions");
}

}  // namespace
