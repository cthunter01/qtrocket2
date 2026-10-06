// Ports OpenRocket's RungeKuttaSimulationStepperTest (core/src/test/java/info/openrocket/core/
// simulation/RungeKuttaSimulationStepperTest.java): numerical release-gate tests for the
// Runge-Kutta simulation steppers.
//
// These tests exercise the actual step() implementations, while replacing rocket forces with
// smooth, deterministic acceleration models (QtRocket::Test::DeterministicStepper, Java's
// DeterministicRK4Stepper and DeterministicRK6Stepper). Full-flight simulations contain motor,
// deployment, and event discontinuities, so they cannot reliably demonstrate the formal order of
// an integration method.

#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <numbers>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/simulation/AbstractRkSimulationStepper.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Rk4SimulationStepper.h"
#include "QtRocket/simulation/Rk6SimulationStepper.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/MathUtil.h"
#include "rocket/TestRockets.h"
#include "simulation/StepperTestSupport.h"

namespace
{

using QtRocket::AbstractRkSimulationStepper;
using QtRocket::Coordinate;
using QtRocket::FlightConfiguration;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::Result;
using QtRocket::Rk4SimulationStepper;
using QtRocket::Rk6SimulationStepper;
using QtRocket::Simulation;
using QtRocket::SimulationConditions;
using QtRocket::SimulationStatus;
using QtRocket::SimulationStepperMethod;
using QtRocket::Test::DeterministicStepper;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;

constexpr double                kAnalyticTolerance  = 1.0e-10;
constexpr double                kOscillatorDuration = 4.0;
constexpr std::array<double, 3> kConvergenceTimeSteps{0.4, 0.2, 0.1};

/// A smooth acceleration function evaluated at each Runge-Kutta stage (Java: AccelerationModel).
using AccelerationModel = std::function<Coordinate(const SimulationStatus&)>;

/// The position and the velocity at the end of an integration.
struct Integrated
{
    Coordinate position;
    Coordinate velocity;
    bool       valid{false};
};

/// Java: createStepper().
[[nodiscard]] std::unique_ptr<AbstractRkSimulationStepper> createStepper(
    SimulationStepperMethod method, AccelerationModel accelerationModel)
{
    switch (method)
    {
        case SimulationStepperMethod::RK4:
            return std::make_unique<DeterministicStepper<Rk4SimulationStepper>>(
                std::move(accelerationModel));
        case SimulationStepperMethod::RK6:
            return std::make_unique<DeterministicStepper<Rk6SimulationStepper>>(
                std::move(accelerationModel));
    }
    return nullptr;
}

/// Integrates a deterministic second-order system using the production stepper (Java:
/// integrate()).
///
/// Java's `new Simulation(rocket)` takes the options of the test preferences; here they are the
/// built-in ones. The two differ in values this integration does not use (the launch rod, the
/// wind, which is set to calm in both): the acceleration is the model's, and the time step is
/// the given one, since the rocket does not rotate.
[[nodiscard]] Integrated integrate(SimulationStepperMethod  method,
                                   const AccelerationModel& accelerationModel,
                                   const Coordinate&        initialPosition,
                                   const Coordinate& initialVelocity, double timeStep,
                                   double duration)
{
    Integrated result;
    const int  stepCount = static_cast<int>(QtRocket::MathUtil::javaRound(duration / timeStep));
    // Test duration must be an integer number of timesteps
    if (std::abs(duration - (stepCount * timeStep)) > 1.0e-12)
    {
        ADD_FAILURE() << "Test duration must be an integer number of timesteps";
        return result;
    }

    const TestEstesAlphaIII alpha;
    Simulation              simulation(*alpha.rocket);
    simulation.setFlightConfigurationId(testFcid(0));
    simulation.getOptions().setIsaAtmosphere(true);
    simulation.getOptions().setTimeStep(timeStep);
    simulation.getOptions().getAverageWindModel().setAverage(0.0);
    simulation.getOptions().getAverageWindModel().setStandardDeviation(0.0);

    Result<SimulationConditions> made = simulation.getOptions().toSimulationConditions();
    if (!made.has_value())
    {
        ADD_FAILURE() << made.error().message;
        return result;
    }
    const auto conditions = std::make_shared<SimulationConditions>(std::move(*made));
    conditions->setSimulation(&simulation);
    const FlightConfiguration& configuration = simulation.getActiveConfiguration();
    SimulationStatus           status(std::make_shared<FlightConfiguration>(configuration.clone()),
                                      conditions);
    status.setFlightDataBranch(std::make_shared<FlightDataBranch>(
        "Numerical integration",
        std::initializer_list<std::reference_wrapper<const FlightDataType>>{
            FlightDataType::builtin(FlightDataTypeId::TYPE_TIME)}));
    status.setRocketPosition(initialPosition);
    status.setRocketVelocity(initialVelocity);
    status.setRocketWorldPosition(addCoordinate(conditions->getGeodeticComputation(),
                                                conditions->getLaunchSite(), initialPosition));
    status.setLiftoff(true);
    status.setLaunchRodCleared(true);

    const std::unique_ptr<AbstractRkSimulationStepper> stepper =
        createStepper(method, accelerationModel);
    status = stepper->initialize(std::move(status));
    for (int i = 0; i < stepCount; i++)
    {
        stepper->step(status, timeStep);
    }
    result.position = status.getRocketPosition();
    result.velocity = status.getRocketVelocity();
    result.valid    = true;
    return result;
}

/// Java: oscillatorError().
[[nodiscard]] double oscillatorError(const Integrated& result, double time)
{
    const double positionError = result.position.z - std::cos(time);
    const double velocityError = result.velocity.z + std::sin(time);
    return std::hypot(positionError, velocityError);
}

/// The smooth harmonic oscillator x'' = -x.
[[nodiscard]] Coordinate oscillator(const SimulationStatus& status)
{
    return Coordinate(0, 0, -status.getRocketPosition().z);
}

/// The observed orders of @p method between the convergence time steps: halving the time step
/// should reduce the error by approximately 2^p for order p.
[[nodiscard]] std::array<double, 2> observedOrders(SimulationStepperMethod method)
{
    std::array<double, kConvergenceTimeSteps.size()> errors{};
    for (std::size_t i = 0; i < kConvergenceTimeSteps.size(); i++)
    {
        const Integrated result =
            integrate(method, oscillator, Coordinate(0, 0, 1), Coordinate::kZero,
                      kConvergenceTimeSteps.at(i), kOscillatorDuration);
        errors.at(i) = oscillatorError(result, kOscillatorDuration);
    }
    std::array<double, 2> orders{};
    for (std::size_t i = 0; i + 1 < errors.size(); i++)
    {
        orders.at(i) = std::log(errors.at(i) / errors.at(i + 1)) / std::numbers::ln2;
    }
    return orders;
}

/// The acceleration of the ballistic test.
constexpr Coordinate kBallisticAcceleration(2.0, 1.0, -9.81);

/// Java: `status -> acceleration`.
[[nodiscard]] Coordinate constantAcceleration(const SimulationStatus& /*status*/)
{
    return kBallisticAcceleration;
}

class RungeKuttaSimulationStepper : public ::testing::TestWithParam<SimulationStepperMethod>
{ };

// RungeKuttaSimulationStepperTest.testConstantAccelerationMatchesBallisticSolution (for RK4 and
// RK6): a ballistic trajectory under constant acceleration has a closed-form solution. This
// also guards the weighted-stage accumulation that previously failed in RK6.
TEST_P(RungeKuttaSimulationStepper, ConstantAccelerationMatchesBallisticSolution)
{
    const Coordinate initialPosition(1.0, -2.0, 100.0);
    const Coordinate initialVelocity(5.0, -3.0, 30.0);
    const Coordinate acceleration = kBallisticAcceleration;
    const double     duration     = 2.0;

    const Integrated result = integrate(GetParam(), constantAcceleration, initialPosition,
                                        initialVelocity, 0.2, duration);
    ASSERT_TRUE(result.valid);

    const Coordinate expectedPosition(initialPosition.x + (initialVelocity.x * duration) +
                                          (acceleration.x * duration * duration / 2.0),
                                      initialPosition.y + (initialVelocity.y * duration) +
                                          (acceleration.y * duration * duration / 2.0),
                                      initialPosition.z + (initialVelocity.z * duration) +
                                          (acceleration.z * duration * duration / 2.0));
    const Coordinate expectedVelocity(initialVelocity.x + (acceleration.x * duration),
                                      initialVelocity.y + (acceleration.y * duration),
                                      initialVelocity.z + (acceleration.z * duration));

    EXPECT_NEAR(expectedPosition.x, result.position.x, kAnalyticTolerance)
        << "ballistic position x";
    EXPECT_NEAR(expectedPosition.y, result.position.y, kAnalyticTolerance)
        << "ballistic position y";
    EXPECT_NEAR(expectedPosition.z, result.position.z, kAnalyticTolerance)
        << "ballistic position z";
    EXPECT_NEAR(expectedVelocity.x, result.velocity.x, kAnalyticTolerance)
        << "ballistic velocity x";
    EXPECT_NEAR(expectedVelocity.y, result.velocity.y, kAnalyticTolerance)
        << "ballistic velocity y";
    EXPECT_NEAR(expectedVelocity.z, result.velocity.z, kAnalyticTolerance)
        << "ballistic velocity z";
}

// RungeKuttaSimulationStepperTest.testSmoothProblemConvergesAtExpectedOrder (stepperOrders():
// RK4 at least 3.7, RK6 at least 5.5): verify the observed order on the smooth harmonic
// oscillator x'' = -x.
TEST_P(RungeKuttaSimulationStepper, SmoothProblemConvergesAtExpectedOrder)
{
    const double minimumOrder          = GetParam() == SimulationStepperMethod::RK4 ? 3.7 : 5.5;
    const std::array<double, 2> orders = observedOrders(GetParam());
    EXPECT_GE(orders.at(0), minimumOrder) << "between timesteps 0.4 and 0.2";
    EXPECT_GE(orders.at(1), minimumOrder) << "between timesteps 0.2 and 0.1";
}

INSTANTIATE_TEST_SUITE_P(RungeKuttaSimulationStepperTest, RungeKuttaSimulationStepper,
                         ::testing::ValuesIn(QtRocket::kAllSimulationStepperMethods),
                         [](const ::testing::TestParamInfo<SimulationStepperMethod>& paramInfo) {
                             return std::string{
                                 QtRocket::simulationStepperMethodName(paramInfo.param)};
                         });

// RungeKuttaSimulationStepperTest.testRK4AndRK6ConvergeToSameSolution: RK4 and RK6 are
// independent approximations of the same equations and must approach the same state as their
// timestep decreases.
TEST(RungeKuttaSimulationStepperTest, RK4AndRK6ConvergeToSameSolution)
{
    const Coordinate initialPosition(0, 0, 1);

    const Integrated rk4 = integrate(SimulationStepperMethod::RK4, oscillator, initialPosition,
                                     Coordinate::kZero, 0.05, kOscillatorDuration);
    const Integrated rk6 = integrate(SimulationStepperMethod::RK6, oscillator, initialPosition,
                                     Coordinate::kZero, 0.05, kOscillatorDuration);
    ASSERT_TRUE(rk4.valid);
    ASSERT_TRUE(rk6.valid);

    EXPECT_NEAR(rk4.position.x, rk6.position.x, 1.0e-6) << "RK4/RK6 oscillator position x";
    EXPECT_NEAR(rk4.position.y, rk6.position.y, 1.0e-6) << "RK4/RK6 oscillator position y";
    EXPECT_NEAR(rk4.position.z, rk6.position.z, 1.0e-6) << "RK4/RK6 oscillator position z";
    EXPECT_NEAR(rk4.velocity.x, rk6.velocity.x, 1.0e-6) << "RK4/RK6 oscillator velocity x";
    EXPECT_NEAR(rk4.velocity.y, rk6.velocity.y, 1.0e-6) << "RK4/RK6 oscillator velocity y";
    EXPECT_NEAR(rk4.velocity.z, rk6.velocity.z, 1.0e-6) << "RK4/RK6 oscillator velocity z";
}

}  // namespace
