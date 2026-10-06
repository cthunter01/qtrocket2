#pragma once

// What the tests of the simulation steppers share: the scenarios a Java probe ran on
// OpenRocket's steppers (probes/tier8b-steppers/StepperProbe.java) with what it printed, and the
// runner that builds the same status here, does the same with it and collects the same values.
// Test-only.

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/AbstractSimulationStepper.h"
#include "QtRocket/simulation/AccelerationData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/SimulationStepper.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket::Test
{

/// One scenario of StepperProbe.java: a status built by hand on one of TestRockets' rockets,
/// what is done with it, and what OpenRocket gave.
///
/// The inputs (labels and values side by side) say how the status is made; the probe printed
/// them, so the values are exactly the doubles Java used (the orientation is given by its four
/// components for that reason). See runStepScenario() for what each one means.
///
/// The outputs are the values the probe printed afterwards, in its order: the status (time,
/// position, velocity, orientation, rotation velocity, world position, maximum z velocity), the
/// data store of the stepper (as the last evaluation of the step left it), and every column of
/// every row of the flight data branch, in the order of FlightDataBranch::getTypes() (labelled
/// "row<n>.<save key>"; the computation time, a wall clock, is left out). The texts are the
/// types of the branch in that order, the events of the branch and of the queue, the warnings,
/// what the step threw, and for some listeners what they recorded.
struct StepScenarioPin
{
    std::string_view name;
    /// The method of TestRockets.java that makes the rocket ("makeEstesAlphaIII").
    std::string_view maker;
    /// RK4, RK6, LANDING, TUMBLE or GROUND.
    std::string_view stepper;
    /// none, jitter-removal, record, or the hook an overriding listener answers.
    std::string_view listener;
    /// step, flight-conditions, thrust or landed-values.
    std::string_view                  action;
    std::span<const std::string_view> inputLabels;
    std::span<const double>           inputs;
    std::span<const std::string_view> outputLabels;
    std::span<const double>           outputs;
    std::span<const std::string_view> texts;

    constexpr StepScenarioPin(std::string_view scenarioName, std::string_view makerMethod,
                              std::string_view stepperName, std::string_view listenerKind,
                              std::string_view                  actionName,
                              std::span<const std::string_view> labelsOfInputs,
                              std::span<const double>           valuesOfInputs,
                              std::span<const std::string_view> labelsOfOutputs,
                              std::span<const double>           valuesOfOutputs,
                              std::span<const std::string_view> expectedTexts) noexcept
      : name(scenarioName),
        maker(makerMethod),
        stepper(stepperName),
        listener(listenerKind),
        action(actionName),
        inputLabels(labelsOfInputs),
        inputs(valuesOfInputs),
        outputLabels(labelsOfOutputs),
        outputs(valuesOfOutputs),
        texts(expectedTexts)
    {
    }

    /// The input @p label; a test failure and NaN when the scenario has none.
    [[nodiscard]] double input(std::string_view label) const;

    /// input(@p label) != 0.
    [[nodiscard]] bool flag(std::string_view label) const;
};

/// Every scenario of the probe, in its order (StepperScenarioPins.cpp, generated from the
/// probe's output by probes/tier8b-steppers/gen_step_tests.py).
[[nodiscard]] std::span<const StepScenarioPin> stepScenarioPins();

/// The scenario named @p name; a test failure and null when there is none.
[[nodiscard]] const StepScenarioPin* findStepScenario(std::string_view name);

/// The scenarios whose name starts with @p prefix.
[[nodiscard]] std::vector<const StepScenarioPin*> stepScenariosStartingWith(
    std::string_view prefix);

/// What a scenario gave here: the outputs and the texts, as StepScenarioPin describes them.
struct StepScenarioResult
{
    std::vector<std::pair<std::string, double>> outputs;
    std::vector<std::string>                    texts;
};

/// A status built as a scenario says, with everything it refers to: the rocket, the simulation
/// its conditions belong to, the conditions (those of the status before initialize(), whose
/// listener list holds the caller's listeners) and the flight data branch.
struct ScenarioStatus
{
    std::unique_ptr<Rocket>               rocket;
    std::unique_ptr<Simulation>           simulation;
    std::shared_ptr<SimulationConditions> conditions;
    std::shared_ptr<FlightDataBranch>     branch;
    std::unique_ptr<SimulationStatus>     status;
};

/// The rocket of the method @p maker of TestRockets.java; a test failure and an empty rocket
/// for an unknown method.
[[nodiscard]] std::unique_ptr<Rocket> makeScenarioRocket(std::string_view maker);

/// The configuration of @p rocket a scenario simulates (StepperProbe.configOf()): the selected
/// configuration, or the first test configuration (TEST_FCID_0) when the selected one is the
/// default configuration and the rocket has that one.
[[nodiscard]] FlightConfiguration& scenarioConfiguration(Rocket& rocket);

/// Builds the status of @p pin, before the stepper's initialize(): the rocket (with the nozzle
/// exit diameter "nozzleExitDiameter" in every motor of the configuration when it is positive),
/// a Simulation with the options "rodLength", "rodAngle", "rodDirection", "latitude",
/// "longitude", "altitude", "timeStep", "maxAngleStep", a steady wind "windAverage" from
/// "windDirection", the geodetic computation "geodetic" (its ordinal), the random seed "seed",
/// the ISA atmosphere and WGS gravity; the conditions of those options with @p listeners; and a
/// status of a clone of the configuration with a new branch, the orientation ("qw" ... "qz"),
/// position ("px" ...) and its world position, velocity ("vx" ...), rotation velocity
/// ("rvx" ...), the flags "motorIgnited", "liftoff", "rodCleared" (cleared at "rodClearedTime"),
/// "apogee" and "tumbling", the time "time", every motor ignited at "ignitionTime" and burnt out
/// at "burnoutTime" (unless NaN), only the stage "onlyStage" active (unless negative) and, for
/// "deployRecovery", every recovery device of the active stages deployed.
[[nodiscard]] ScenarioStatus buildScenarioStatus(
    const StepScenarioPin& pin, const std::vector<std::shared_ptr<SimulationListener>>& listeners);

/// The status of the scenario @p name after initialize() of @p stepper, with what it refers to;
/// `status` is null (after a test failure) when there is no such scenario.
struct InitializedScenario
{
    ScenarioStatus                    built;
    std::unique_ptr<SimulationStatus> status;

    InitializedScenario(std::string_view name, SimulationStepper& stepper,
                        const std::vector<std::shared_ptr<SimulationListener>>& listeners = {});
};

/// Sets the nozzle exit diameter @p diameter(the motor's diameter) in the motor configuration of
/// every active mount of @p configuration that has a motor, and brings the configuration up to
/// date. Java sets it in `configuration.getActiveMotors()`, which are the mounts' own objects;
/// the configuration's motors are copies here (see FlightConfiguration).
void setNozzleExitDiameters(FlightConfiguration&                 configuration,
                            const std::function<double(double)>& diameter);

/// Ignites every motor of @p status at @p time (Java: `for (MotorClusterState state :
/// status.getMotors()) state.ignite(time)`).
void igniteAllMotors(const SimulationStatus& status, double time);

/// Burns every motor of @p status out at @p time.
void burnOutAllMotors(const SimulationStatus& status, double time);

/// Deploys every recovery device of the active stages of @p status.
void deployRecoveryDevices(SimulationStatus& status);

/// A new stepper of the kind @p name (RK4, RK6, LANDING, TUMBLE or GROUND); a test failure and
/// null for another name.
[[nodiscard]] std::unique_ptr<AbstractSimulationStepper> makeScenarioStepper(std::string_view name);

/// The data store of @p stepper (the steppers keep it in three different classes).
[[nodiscard]] AbstractSimulationStepper::DataStore& scenarioStore(
    AbstractSimulationStepper& stepper);

/// Runs @p pin as the probe did: builds the status with the listener of the scenario, has the
/// stepper initialize it, sets the previous time step ("previousTimeStep", unless NaN), does
/// the action ("steps" steps of at most "maxTimeStep", or one of the direct calls) and collects
/// the outputs and the texts.
[[nodiscard]] StepScenarioResult runStepScenario(const StepScenarioPin& pin);

/// The tolerances with which a scenario is compared with Java's values, relative to the larger
/// magnitude and absolute (for the values that cancel to nearly zero).
///
/// How they were derived. A scenario's outputs are the result of up to seven evaluations of the
/// whole force model (the atmosphere, the aerodynamics, the mass, the rotations), each with
/// dozens of transcendental functions, whose last bit differs between Java and the C library, and
/// with sums that Java forms in the hash order of the components, which changes from one Java
/// run to the next. Differences of nearly equal values (the moment about the centre of gravity,
/// the change of the rotation velocity) amplify those last bits. Measured: of the 13088 values
/// the probe prints, 581 differ between four runs of the Java probe itself, by up to 1.5e-13
/// relative; of the 12335 values pinned here, 761 differ between this port (Linux, glibc) and
/// Java, by up to 2.4e-13, and the rest are equal to the last bit. The other math libraries
/// this code runs with (Apple's, the UCRT) differ from Java in other bits. 1e-10 leaves two to
/// three orders of margin over what was measured; a wrong constant, formula, branch or order of
/// the stages changes a value by far more.
inline constexpr double kStepRelativeTolerance = 1e-10;
inline constexpr double kStepAbsoluteTolerance = 1e-13;

/// The differences between what the probe printed for @p pin and @p result, one line each;
/// empty when there is none. The labels and their order must be the same, the texts equal, and
/// each value the pinned one: NaN with NaN, an infinity with the same infinity, and otherwise
/// within @p relativeTolerance of the larger magnitude or within @p absoluteTolerance (for the
/// values that cancel to nearly zero).
///
/// @p javaTextReplacements: pairs (Java's text, the text expected here) for the texts that
/// differ from Java's on purpose.
[[nodiscard]] std::string stepScenarioDifferences(
    const StepScenarioPin& pin, const StepScenarioResult& result, double relativeTolerance,
    double                                                         absoluteTolerance,
    std::span<const std::pair<std::string_view, std::string_view>> javaTextReplacements = {});

/// The value of the data store field @p field; a BugError, which fails the test, when it is
/// empty. (A plain `if` is what clang-tidy's unchecked-optional-access check follows; it cannot
/// see through ASSERT_TRUE.)
template <class T>
[[nodiscard]] const T& stored(const std::optional<T>& field)
{
    if (!field.has_value())
    {
        bug("The field of the data store is empty");
    }
    return *field;
}

/// A listener that counts its clones, in a counter they share: how the tests see whether a
/// stepper's initialize() copied the status (Java's copy constructor clones the listeners).
class CloneCountingListener final : public CloneableSimulationListener<CloneCountingListener>
{
public:
    CloneCountingListener() = default;
    CloneCountingListener(const CloneCountingListener& other)
      : CloneableSimulationListener<CloneCountingListener>(other), m_clones(other.m_clones)
    {
        ++*m_clones;
    }
    CloneCountingListener& operator=(const CloneCountingListener&) = delete;
    CloneCountingListener(CloneCountingListener&&)                 = delete;
    CloneCountingListener& operator=(CloneCountingListener&&)      = delete;
    ~CloneCountingListener() override                              = default;

    /// How many clones were made of this listener and of its clones.
    [[nodiscard]] int clones() const noexcept { return *m_clones; }

private:
    std::shared_ptr<int> m_clones{std::make_shared<int>(0)};
};

/// RungeKuttaSimulationStepperTest's test adapters (DeterministicRK4Stepper and
/// DeterministicRK6Stepper): the Runge-Kutta stepper @p Base with the rocket's forces replaced
/// by a smooth, deterministic acceleration model, which is evaluated at each stage. The
/// integration machinery of the stepper is the production one.
template <class Base>
class DeterministicStepper final : public Base
{
public:
    /// The linear acceleration in world coordinates at a status.
    using AccelerationModel = std::function<Coordinate(const SimulationStatus&)>;

    explicit DeterministicStepper(AccelerationModel accelerationModel)
      : m_accelerationModel(std::move(accelerationModel))
    {
    }

    /// Populates the fields consumed by the time step selection and the flight data storage
    /// (Java: setDeterministicAcceleration()).
    void calculateAcceleration(SimulationStatus&                     status,
                               AbstractSimulationStepper::DataStore& store) override
    {
        const Coordinate acceleration = m_accelerationModel(status);
        store.accelerationData.emplace(std::nullopt, std::nullopt, acceleration, Coordinate::kZero,
                                       status.getRocketOrientationQuaternion());
        store.gravity              = 0.0;
        store.thrustForce          = 0.0;
        store.dragForce            = 0.0;
        store.coriolisAcceleration = Coordinate::kZero;
    }

private:
    AccelerationModel m_accelerationModel;
};

/// Runs the scenario named @p name and returns its differences from Java's values, with the
/// tolerances above and with the two texts that differ from Java's on purpose replaced:
/// - Java's RK6 stepper throws its "values too large" exception with the untranslated key
///   "error.valuesTooLarge" as the message (see AbstractRkSimulationStepper); the message here
///   is the text of the RK4 stepper;
/// - the BugError of addCoordinate() for a NaN location prints the location and the numbers as
///   WorldCoordinate::toString() and std::format do (see there), not as Java does.
/// Empty when the scenario reproduces OpenRocket; a line saying so when there is no such
/// scenario.
[[nodiscard]] std::string javaScenarioDifferences(std::string_view name);

/// The names of the scenarios for which @p select holds, in the probe's order.
[[nodiscard]] std::vector<std::string> stepScenarioNames(bool (*select)(const StepScenarioPin&));

/// @p name as the name of a parameterised test: every character that is not a letter or a digit
/// becomes an underscore.
[[nodiscard]] std::string scenarioTestName(std::string_view name);

}  // namespace QtRocket::Test
