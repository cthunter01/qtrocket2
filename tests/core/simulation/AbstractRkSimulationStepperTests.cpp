#include "QtRocket/simulation/AbstractRkSimulationStepper.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/simulation/AbstractSimulationStepper.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/Rk4SimulationStepper.h"
#include "QtRocket/simulation/Rk6SimulationStepper.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Quaternion.h"
#include "rocket/JavaValueDifferences.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationStatusSupport.h"
#include "simulation/SimulationTestSupport.h"
#include "simulation/StepperTablePins.h"
#include "simulation/StepperTestSupport.h"

namespace
{

using QtRocket::AbstractRkSimulationStepper;
using QtRocket::AbstractSimulationStepper;
using QtRocket::Coordinate;
using QtRocket::Quaternion;
using QtRocket::Rk4SimulationStepper;
using QtRocket::Rk6SimulationStepper;
using QtRocket::SimulationConditions;
using QtRocket::SimulationStatus;
using QtRocket::Test::bugText;
using QtRocket::Test::CloneCountingListener;
using QtRocket::Test::InitializedScenario;
using QtRocket::Test::javaScenarioDifferences;
using QtRocket::Test::matchesJavaValue;
using QtRocket::Test::newBranch;
using QtRocket::Test::scenarioTestName;
using QtRocket::Test::statusConfiguration;
using QtRocket::Test::stepScenarioNames;
using QtRocket::Test::StepScenarioPin;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;
using QtRocket::Test::StepperPins::kRodDirectionPins;
using QtRocket::Test::StepperPins::kTimeStepPins;
using QtRocket::Test::StepperPins::RodDirectionPin;
using QtRocket::Test::StepperPins::TimeStepPin;

// The base class is abstract; Rk4SimulationStepper and Rk6SimulationStepper are its steppers.
static_assert(std::is_abstract_v<AbstractRkSimulationStepper>);
static_assert(std::is_base_of_v<AbstractSimulationStepper, AbstractRkSimulationStepper>);

// OpenRocket's tests reach this class through MotorClusterPodsTest and
// MotorPressureCorrectionTest (ported in motor_cluster_pods_tests.cpp and
// motor_pressure_correction_tests.cpp). The other expectations are what
// probes/tier8b-steppers/StepperTableProbe.java and StepperProbe.java printed.

// =============================================================================== constants

// StepperTableProbe, "constants": Java's values (the three angles are constant expressions,
// 2 * 28.32 * Math.PI / 180 and so on, which are exact to compare).
TEST(AbstractRkSimulationStepper, TheConstantsAreJavas)
{
    EXPECT_EQ(AbstractRkSimulationStepper::kSeedRandomization, 602120223);
    EXPECT_EQ(AbstractRkSimulationStepper::kRecommendedTimeStep, 0.05);
    EXPECT_EQ(AbstractRkSimulationStepper::kRecommendedMaxTime, 1200.0);
    EXPECT_EQ(AbstractRkSimulationStepper::kRecommendedAngleStep, 0.05235987755982988);
    EXPECT_EQ(AbstractRkSimulationStepper::kPitchYawRandom, 5.0E-4);
    EXPECT_EQ(AbstractRkSimulationStepper::kMaxRollStepAngle, 0.9885544883295881);
    EXPECT_EQ(AbstractRkSimulationStepper::kMaxRollRateChange, 0.03490658503988659);
    EXPECT_EQ(AbstractRkSimulationStepper::kMaxPitchYawChange, 0.06981317007977318);
    EXPECT_EQ(AbstractRkSimulationStepper::kValuesTooLarge,
              "Simulation values exceeded limits.  Try selecting a shorter time step.");
}

// ============================================================================ the time step

/// computeTimeStep() of a new RK4 stepper for the inputs of @p pin, set up as the probe did:
/// a status of the Alpha III on new conditions with the time step, the maximum angle step and
/// the launch rod length, the rod cleared or not, and a store with the lateral pitch rate, the
/// roll rate, the rotational acceleration in rocket coordinates and the previous time step.
[[nodiscard]] double timeStepFor(const TimeStepPin& pin)
{
    const TestEstesAlphaIII alpha;
    const auto              conditions = std::make_shared<SimulationConditions>();
    conditions->setTimeStep(pin.conditionsTimeStep);
    conditions->setMaximumAngleStep(pin.maxAngleStep);
    conditions->setLaunchRodLength(pin.rodLength);
    SimulationStatus status(statusConfiguration(*alpha.rocket, testFcid(0)), conditions);
    if (pin.rodCleared)
    {
        status.setLaunchRodCleared(true);
    }

    Rk4SimulationStepper                  stepper;
    AbstractSimulationStepper::DataStore& store = stepper.getStore();
    store.lateralPitchRate                      = pin.lateralPitchRate;
    store.flightConditions.emplace().setRollRate(pin.rollRate);
    store.accelerationData.emplace(Coordinate::kZero, Coordinate(pin.raX, pin.raY, pin.raZ),
                                   std::nullopt, std::nullopt, Quaternion());
    store.timeStep = pin.previousTimeStep;
    AbstractRkSimulationStepper::RkParameters k1;
    k1.v = Coordinate(pin.vx, pin.vy, pin.vz);
    return stepper.computeTimeStep(status, pin.maxTimeStep, k1);
}

/// The rows of the time step table whose result is not exactly Java's, one line each.
[[nodiscard]] std::string timeStepDifferences()
{
    std::string differences;
    for (std::size_t i = 0; i < kTimeStepPins.size(); i++)
    {
        const TimeStepPin& pin    = kTimeStepPins.at(i);
        const double       actual = timeStepFor(pin);
        const bool same = std::isnan(pin.expected) ? std::isnan(actual) : actual == pin.expected;
        if (!same)
        {
            differences += std::format("  row {}: expected {}, got {}\n", i, pin.expected, actual);
        }
    }
    return differences;
}

// Every limit in turn (the user's step, maxTimeStep, the pitch angle, the roll angle, the roll
// rate change, the pitch and yaw rate change, the two limits of the launch rod, 1.5 times the
// previous step), the step that is too close to maxTimeStep, the step that is too small, the
// user's step below the minimum, and the NaN and infinite limits that do not count. The
// function divides, multiplies and compares only, so the results are exactly Java's.
TEST(AbstractRkSimulationStepper, TheTimeStepIsJavasForEveryLimit)
{
    EXPECT_EQ(kTimeStepPins.size(), 46U);
    EXPECT_EQ(timeStepDifferences(), "");
}

// A few rows of the table by name, so that a reader sees what the limits are. The arguments:
// the conditions' time step, their maximum angle step and launch rod length, whether the rod is
// cleared, the lateral pitch rate, the roll rate, the rotational acceleration (x, y, z), the
// velocity (x, y, z), the previous time step and maxTimeStep (the last one, the expected value
// of a pin, is not used here).
TEST(AbstractRkSimulationStepper, TheLimitsOfTheTimeStep)
{
    constexpr double kThreeDegrees = 3 * std::numbers::pi / 180;
    constexpr double kNone         = std::numeric_limits<double>::quiet_NaN();

    // The user's step, and a fifth of it on the launch rod.
    EXPECT_EQ(timeStepFor({0.05, kThreeDegrees, 1, true, 0, 0, 0, 0, 0, 0, 0, 30, kNone, 1.0, 0}),
              0.05);
    EXPECT_EQ(timeStepFor({0.05, kThreeDegrees, 1, false, 0, 0, 0, 0, 0, 0, 0, 3, kNone, 1.0, 0}),
              0.01);
    // A tenth of the launch rod: 3 m at 20 m/s.
    EXPECT_EQ(timeStepFor({0.1, kThreeDegrees, 3, false, 0, 0, 0, 0, 0, 0, 0, 20, kNone, 1.0, 0}),
              0.015);
    // The angle step: 0.02 rad at 0.9 rad/s.
    EXPECT_EQ(timeStepFor({0.05, 0.02, 1, true, 0.9, 0, 0, 0, 0, 0, 0, 30, kNone, 1.0, 0}),
              0.02 / 0.9);
    // 1.5 times the previous step.
    EXPECT_EQ(timeStepFor({0.05, kThreeDegrees, 1, true, 0, 0, 0, 0, 0, 0, 0, 30, 0.01, 1.0, 0}),
              0.015);
    // Within a twentieth of the user's step of maxTimeStep: maxTimeStep, even beyond the
    // user's step.
    EXPECT_EQ(timeStepFor({0.05, kThreeDegrees, 1, true, 0, 0, 0, 0, 0, 0, 0, 30, kNone, 0.051, 0}),
              0.051);
    // Never below a twentieth of the user's step, not even for the next event.
    EXPECT_EQ(timeStepFor({0.05, kThreeDegrees, 1, true, 0, 0, 0, 0, 0, 0, 0, 30, kNone, 0.001, 0}),
              0.0025);
    // A user's step below the minimum time step counts as the minimum.
    EXPECT_EQ(timeStepFor({0.0005, kThreeDegrees, 1, true, 0, 0, 0, 0, 0, 0, 0, 30, kNone, 1.0, 0}),
              0.001);
}

TEST(AbstractRkSimulationStepper, TheTimeStepNeedsTheStoreOfAStep)
{
    const TestEstesAlphaIII alpha;
    SimulationStatus        status(statusConfiguration(*alpha.rocket, testFcid(0)),
                                   std::make_shared<SimulationConditions>());
    Rk4SimulationStepper    stepper;
    const AbstractRkSimulationStepper::RkParameters k1;

    // Java: a NullPointerException in both cases.
    EXPECT_EQ(bugText([&] { static_cast<void>(stepper.computeTimeStep(status, 1.0, k1)); }),
              "The data store has no flight conditions");
    stepper.getStore().flightConditions.emplace();
    EXPECT_EQ(bugText([&] { static_cast<void>(stepper.computeTimeStep(status, 1.0, k1)); }),
              "The data store has no acceleration data");
}

// =============================================================================== initialize

/// The launch rod directions that are not Java's, one line each.
[[nodiscard]] std::string rodDirectionDifferences()
{
    std::string differences;
    for (const RodDirectionPin& pin : kRodDirectionPins)
    {
        const TestEstesAlphaIII alpha;
        const auto              conditions = std::make_shared<SimulationConditions>();
        conditions->setLaunchRodAngle(pin.rodAngle);
        conditions->setLaunchRodDirection(pin.rodDirection);
        SimulationStatus status(statusConfiguration(*alpha.rocket, testFcid(0)), conditions);
        status.setFlightDataBranch(newBranch());
        Rk4SimulationStepper stepper;
        static_cast<void>(stepper.initialize(std::move(status)));
        const std::optional<Coordinate>& rod = stepper.getStore().launchRodDirection;
        // Sines and cosines: within 1e-12, and 1e-15 for a component that cancels to zero.
        if (!rod.has_value() || !matchesJavaValue(pin.x, rod->x) ||
            !matchesJavaValue(pin.y, rod->y) || !matchesJavaValue(pin.z, rod->z))
        {
            differences += std::format("  angle {} direction {}: expected ({}, {}, {})\n",
                                       pin.rodAngle, pin.rodDirection, pin.x, pin.y, pin.z);
        }
    }
    return differences;
}

// StepperTableProbe, "launch rod direction of initialize()".
TEST(AbstractRkSimulationStepper, InitializeSetsTheDirectionOfTheLaunchRod)
{
    EXPECT_EQ(kRodDirectionPins.size(), 9U);
    EXPECT_EQ(rodDirectionDifferences(), "");
}

// Java: `new SimulationStatus(original)` and `status.setWarnings(original.getWarnings())`.
TEST(AbstractRkSimulationStepper, InitializeGivesACopyOfTheStatusThatKeepsItsWarnings)
{
    const TestEstesAlphaIII alpha;
    const auto              conditions = std::make_shared<SimulationConditions>();
    const auto              counter    = std::make_shared<CloneCountingListener>();
    conditions->getSimulationListenerList().push_back(counter);
    SimulationStatus original(statusConfiguration(*alpha.rocket, testFcid(0)), conditions);
    const std::shared_ptr<QtRocket::FlightDataBranch> branch = newBranch();
    original.setFlightDataBranch(branch);
    original.setSimulationTime(1.25);
    original.setRocketPosition(Coordinate(1, 2, 3));
    const std::shared_ptr<QtRocket::WarningSet> warnings      = original.getWarnings();
    const QtRocket::FlightConfiguration*        configuration = &original.getConfiguration();

    Rk4SimulationStepper   stepper;
    const SimulationStatus status = stepper.initialize(std::move(original));

    // Its own conditions, with a clone of the listener, and its own configuration.
    EXPECT_NE(status.getSimulationConditions(), conditions);
    EXPECT_EQ(counter->clones(), 1);
    ASSERT_EQ(status.getSimulationConditions()->getSimulationListenerList().size(), 1U);
    EXPECT_NE(status.getSimulationConditions()->getSimulationListenerList().front(), counter);
    EXPECT_NE(&status.getConfiguration(), configuration);
    // The same branch, the same warning set, the same values.
    EXPECT_EQ(status.getFlightDataBranch(), branch);
    EXPECT_EQ(status.getWarnings(), warnings);
    EXPECT_EQ(status.getSimulationTime(), 1.25);
    EXPECT_TRUE(status.getRocketPosition().exactlyEquals(Coordinate(1, 2, 3)));
}

TEST(AbstractRkSimulationStepper, TheForcesNeedAnInitializedStepper)
{
    const TestEstesAlphaIII                alpha;
    const QtRocket::Test::StepScenarioPin* pin = QtRocket::Test::findStepScenario("thrust-rk4");
    ASSERT_NE(pin, nullptr);
    QtRocket::Test::ScenarioStatus built = QtRocket::Test::buildScenarioStatus(*pin, {});
    ASSERT_NE(built.status, nullptr);

    // Java: a NullPointerException, the random source being null until initialize().
    Rk4SimulationStepper stepper;
    EXPECT_EQ(bugText([&] { stepper.calculateForces(*built.status, stepper.getStore()); }),
              "The stepper calculates forces before initialize() seeded its random source");
}

// ================================================================ the thrust and listeners

/// The scenarios of this file: what the Runge-Kutta steppers do with listeners that answer a
/// hook, the order of the hooks, and the thrust.
[[nodiscard]] bool isRkListenerScenario(const StepScenarioPin& pin)
{
    const bool rk       = pin.stepper == "RK4" || pin.stepper == "RK6";
    const bool listener = pin.listener != "none" && pin.listener != "jitter-removal";
    return rk && pin.action == "step" && listener;
}

[[nodiscard]] bool isThrustScenario(const StepScenarioPin& pin)
{
    return pin.action == "thrust";
}

class RkListenerScenario : public ::testing::TestWithParam<std::string>
{ };

// One RK4 step of a coasting rocket with a listener that answers one hook (each `pre` and each
// `post` hook in turn): the listener's value is used as Java uses it, the simulation warns that
// a listener affected it, and the hooks that the answer makes superfluous are not called.
// "hooks-...": the order of every hook of one step, for RK4, RK6 and on the launch rod.
// "zero-mass-rk4": a rocket without mass aborts the simulation (a SIM_ABORT event) and then
// fails with a NaN acceleration, as in Java.
TEST_P(RkListenerScenario, TheStepIsOpenRockets)
{
    EXPECT_EQ(javaScenarioDifferences(GetParam()), "");
}

INSTANTIATE_TEST_SUITE_P(AbstractRkSimulationStepper, RkListenerScenario,
                         ::testing::ValuesIn(stepScenarioNames(isRkListenerScenario)),
                         [](const ::testing::TestParamInfo<std::string>& paramInfo) {
                             return scenarioTestName(paramInfo.param);
                         });

class ThrustScenario : public ::testing::TestWithParam<std::string>
{ };

// calculateThrust() after calculateFlightConditions(): the thrust of the active motors, the
// correction for the air pressure with known nozzles (one motor, clusters, several stages), no
// thrust before ignition and after burnout, and the listeners' thrust.
TEST_P(ThrustScenario, TheThrustIsOpenRockets)
{
    EXPECT_EQ(javaScenarioDifferences(GetParam()), "");
}

INSTANTIATE_TEST_SUITE_P(AbstractRkSimulationStepper, ThrustScenario,
                         ::testing::ValuesIn(stepScenarioNames(isThrustScenario)),
                         [](const ::testing::TestParamInfo<std::string>& paramInfo) {
                             return scenarioTestName(paramInfo.param);
                         });

TEST(AbstractRkSimulationStepper, TheScenariosCoverEveryHook)
{
    const std::vector<std::string> listeners{
        "hooks-rk4",
        "hooks-rk6",
        "hooks-rod-rk4",
        "listener-pre-flight-conditions",
        "listener-post-flight-conditions",
        "listener-post-flight-conditions-same",
        "listener-pre-aerodynamics",
        "listener-post-aerodynamics",
        "listener-pre-thrust",
        "listener-post-thrust",
        "listener-pre-gravity",
        "listener-post-gravity",
        "listener-pre-wind",
        "listener-post-wind",
        "listener-pre-atmosphere",
        "listener-post-atmosphere",
        "listener-pre-mass",
        "listener-post-mass",
        "listener-pre-acceleration",
        "listener-post-acceleration",
        "zero-mass-rk4",
    };
    EXPECT_EQ(stepScenarioNames(isRkListenerScenario), listeners);

    const std::vector<std::string> thrusts{
        "thrust-alpha",
        "thrust-alpha-nozzle",
        "thrust-falcon-nozzle",
        "thrust-clusterpods-nozzle",
        "thrust-multistage-nozzle",
        "thrust-not-ignited",
        "thrust-burnt-out",
        "thrust-below-standard-altitude",
        "thrust-listener-pre-thrust",
        "thrust-listener-post-thrust",
    };
    EXPECT_EQ(stepScenarioNames(isThrustScenario), thrusts);
}

// What the pins of the three "hooks" scenarios say, spelled out for one evaluation: the mass
// (twice: the structure and the motors), the aerodynamics with the flight conditions inside
// them, the thrust, gravity; all between the two acceleration hooks.
TEST(AbstractRkSimulationStepper, TheHooksOfOneEvaluationComeInJavasOrder)
{
    const StepScenarioPin* pin = QtRocket::Test::findStepScenario("hooks-rk4");
    ASSERT_NE(pin, nullptr);
    const QtRocket::Test::StepScenarioResult result = QtRocket::Test::runStepScenario(*pin);
    ASSERT_FALSE(result.texts.empty());
    const std::string& hooks = result.texts.back();

    const std::string flightConditions =
        "preFlightConditions preAtmosphere postAtmosphere preWind postWind postFlightConditions";
    const std::string evaluation =
        "preAcceleration preMass postMass preMass postMass "
        "preAerodynamics " +
        flightConditions +
        " postAerodynamics preThrust postThrust preGravity "
        "postGravity postAcceleration";
    // The step: the flight conditions of the status, then four evaluations.
    EXPECT_EQ(hooks, "hooks " + flightConditions + " " + evaluation + " " + evaluation + " " +
                         evaluation + " " + evaluation);
}

TEST(AbstractRkSimulationStepper, TheThrustNeedsFlightConditionsOnlyWhenThereIsThrust)
{
    Rk4SimulationStepper stepper;
    InitializedScenario  thrusting("thrust-alpha-nozzle", stepper);
    ASSERT_NE(thrusting.status, nullptr);
    // Java: a NullPointerException.
    EXPECT_EQ(bugText([&] {
                  static_cast<void>(stepper.calculateThrust(*thrusting.status, stepper.getStore()));
              }),
              "The data store has no flight conditions");

    Rk4SimulationStepper other;
    InitializedScenario  armed("thrust-not-ignited", other);
    ASSERT_NE(armed.status, nullptr);
    EXPECT_EQ(other.calculateThrust(*armed.status, other.getStore()), 0.0);
    EXPECT_EQ(other.getStore().thrustCorrection, 0.0);
}

// computeParameters(): the derivatives are the accelerations of the store in world coordinates
// and the velocities of the status, and a NaN in any of them is a bug.
TEST(AbstractRkSimulationStepper, TheParametersAreTheAccelerationsAndTheVelocities)
{
    using Deterministic = QtRocket::Test::DeterministicStepper<Rk4SimulationStepper>;
    Deterministic       stepper([](const SimulationStatus& status) {
        return Coordinate(1, 2, status.getSimulationTime());
    });
    InitializedScenario f("thrust-rk4", stepper);
    ASSERT_NE(f.status, nullptr);
    f.status->setRocketVelocity(Coordinate(4, 5, 6));
    f.status->setRocketRotationVelocity(Coordinate(0.1, 0.2, 0.3));

    const AbstractRkSimulationStepper::RkParameters k =
        stepper.computeParameters(*f.status, stepper.getStore());
    EXPECT_TRUE(k.a.exactlyEquals(Coordinate(1, 2, f.status->getSimulationTime())));
    EXPECT_TRUE(k.ra.exactlyEquals(Coordinate::kZero));
    EXPECT_TRUE(k.v.exactlyEquals(Coordinate(4, 5, 6)));
    EXPECT_TRUE(k.rv.exactlyEquals(Coordinate(0.1, 0.2, 0.3)));

    f.status->setRocketVelocity(Coordinate(4, std::numeric_limits<double>::quiet_NaN(), 6));
    EXPECT_EQ(bugText([&] {
                  static_cast<void>(stepper.computeParameters(*f.status, stepper.getStore()));
              }),
              "Simulation resulted in not-a-number (NaN) value for params.v, please report a "
              "bug, c=(4.00000,NaN,6.00000)");
}

// The stages of a step are evaluated on clones of the status that share its flight data branch
// and warning set, and the original status is not moved until the end: a deterministic
// acceleration model sees the times of the four RK4 stages and of the seven RK6 stages.
TEST(AbstractRkSimulationStepper, TheStagesAreEvaluatedAtJavasTimes)
{
    const auto times = std::make_shared<std::vector<double>>();
    const auto model = [times](const SimulationStatus& status) {
        times->push_back(status.getSimulationTime());
        return Coordinate::kZero;
    };

    QtRocket::Test::DeterministicStepper<Rk4SimulationStepper> rk4(model);
    InitializedScenario                                        four("thrust-rk4", rk4);
    ASSERT_NE(four.status, nullptr);
    const double t = four.status->getSimulationTime();
    rk4.step(*four.status, 0.02);
    const double h = rk4.getStore().timeStep;
    EXPECT_EQ(h, 0.02);
    EXPECT_EQ(*times, (std::vector<double>{t, t + (h / 2), t + (h / 2), t + h}));

    times->clear();
    QtRocket::Test::DeterministicStepper<Rk6SimulationStepper> rk6(model);
    InitializedScenario                                        six("thrust-rk6", rk6);
    ASSERT_NE(six.status, nullptr);
    rk6.step(*six.status, 0.02);
    EXPECT_EQ(*times, (std::vector<double>{t, t + (h / 3), t + (h * 2 / 3), t + (h * 1 / 3),
                                           t + (h * 1 / 2), t + (h * 1 / 2), t + h}));
}

}  // namespace
