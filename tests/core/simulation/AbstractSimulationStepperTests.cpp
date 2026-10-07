#include "QtRocket/simulation/AbstractSimulationStepper.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/GroundStepper.h"
#include "QtRocket/simulation/Rk4SimulationStepper.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/SimulationStepper.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Quaternion.h"
#include "rocket/JavaValueDifferences.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationStatusSupport.h"
#include "simulation/SimulationTestSupport.h"
#include "simulation/StepperTablePins.h"
#include "simulation/StepperTestSupport.h"

namespace
{

using QtRocket::AbstractSimulationStepper;
using QtRocket::AerodynamicForces;
using QtRocket::AtmosphericConditions;
using QtRocket::CloneableSimulationListener;
using QtRocket::Coordinate;
using QtRocket::FlightConditions;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::GroundStepper;
using QtRocket::Quaternion;
using QtRocket::RigidBody;
using QtRocket::Rk4SimulationStepper;
using QtRocket::RocketComponent;
using QtRocket::SimulationConditions;
using QtRocket::SimulationStatus;
using QtRocket::SimulationStepper;
using QtRocket::Test::bugText;
using QtRocket::Test::javaScenarioDifferences;
using QtRocket::Test::matchesPinnedValue;
using QtRocket::Test::newBranch;
using QtRocket::Test::scenarioTestName;
using QtRocket::Test::statusConfiguration;
using QtRocket::Test::stepScenarioNames;
using QtRocket::Test::StepScenarioPin;
using QtRocket::Test::stored;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;
using QtRocket::Test::StepperPins::CorrectivePin;
using QtRocket::Test::StepperPins::DampingPin;
using QtRocket::Test::StepperPins::kCorrectivePins;
using QtRocket::Test::StepperPins::kDampingPins;
using QtRocket::Test::StepperPins::kMotorMassDerivativePins;
using QtRocket::Test::StepperPins::kWindDirectionPins;
using QtRocket::Test::StepperPins::MotorMassDerivativePin;
using QtRocket::Test::StepperPins::WindDirectionPin;

using DataStore = AbstractSimulationStepper::DataStore;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

static_assert(std::is_abstract_v<AbstractSimulationStepper>);
static_assert(std::is_base_of_v<SimulationStepper, AbstractSimulationStepper>);

// OpenRocket has no test of AbstractSimulationStepper itself. The expectations are what
// probes/tier8b-steppers/StepperProbe.java (the scenarios) and StepperTableProbe.java (the
// tables, quoted where they are used) printed.

/// Whether @p actual is exactly @p expected, NaN matching NaN.
[[nodiscard]] bool same(double expected, double actual)
{
    return std::isnan(expected) ? std::isnan(actual) : actual == expected;
}

/// A status of the Alpha III on default conditions with a branch, the launch rod cleared or not
/// (StepperTableProbe.statusWithRod()).
struct RodStatus
{
    TestEstesAlphaIII                 alpha;
    std::shared_ptr<FlightDataBranch> branch = newBranch();
    SimulationStatus                  status;

    explicit RodStatus(bool rodCleared)
      : status(statusConfiguration(*alpha.rocket, testFcid(0)),
               std::make_shared<SimulationConditions>())
    {
        status.setFlightDataBranch(branch);
        if (rodCleared)
        {
            status.setLaunchRodCleared(true);
        }
    }
};

// ================================================================================ the store

TEST(AbstractSimulationStepper, TheMinimumTimeStepIsJavas)
{
    EXPECT_EQ(AbstractSimulationStepper::kMinTimeStep, 0.001);
}

TEST(AbstractSimulationStepperDataStore, ANewStoreHoldsNothing)
{
    const DataStore store;
    EXPECT_TRUE(std::isnan(store.timeStep));
    EXPECT_FALSE(store.accelerationData.has_value());
    EXPECT_FALSE(store.flightConditions.has_value());
    EXPECT_FALSE(store.rocketMass.has_value());
    EXPECT_FALSE(store.motorMass.has_value());
    EXPECT_FALSE(store.coriolisAcceleration.has_value());
    EXPECT_FALSE(store.launchRodDirection.has_value());
    EXPECT_FALSE(store.forces.has_value());
    EXPECT_TRUE(std::isnan(store.windVelocity.x));
    EXPECT_TRUE(std::isnan(store.windVelocity.y));
    EXPECT_TRUE(std::isnan(store.windVelocity.z));
    EXPECT_EQ(store.windVelocity.weight, 0.0);
    EXPECT_TRUE(std::isnan(store.gravity));
    EXPECT_TRUE(std::isnan(store.thrustForce));
    EXPECT_TRUE(std::isnan(store.thrustCorrection));
    EXPECT_TRUE(std::isnan(store.dragForce));
    EXPECT_TRUE(std::isnan(store.lateralPitchRate));
    EXPECT_FALSE(store.thetaRotation.has_value());
}

TEST(AbstractSimulationStepperDataStore, StoringNeedsAFlightDataBranch)
{
    const TestEstesAlphaIII alpha;
    SimulationStatus        status(statusConfiguration(*alpha.rocket, testFcid(0)),
                                   std::make_shared<SimulationConditions>());
    const DataStore         store;
    // Java: a NullPointerException.
    EXPECT_EQ(bugText([&] { store.storeData(status); }),
              "The simulation status has no flight data branch to store the step's data to");
}

// What a store without anything in it writes: the four forces, the wind, the time step and the
// six stability values, and nothing else. (The values, and the order of the columns, which is
// the order in which Java sets them and decides between the two types that sort alike, the
// thrust and the thrust correction, are pinned by the "empty-store" scenarios and by the "types"
// text of every scenario.)
TEST(AbstractSimulationStepperDataStore, AnEmptyStoreWritesTheForcesTheWindAndTheStabilityValues)
{
    RodStatus f(true);
    f.status.storeData();
    const std::size_t statusColumns = f.branch->getTypes().size();
    const DataStore   store;
    store.storeData(f.status);

    EXPECT_EQ(f.branch->getTypes().size(), statusColumns + 13);
    for (const FlightDataTypeId id :
         {FlightDataTypeId::TYPE_THRUST_FORCE, FlightDataTypeId::TYPE_THRUST_CORRECTION,
          FlightDataTypeId::TYPE_GRAVITY, FlightDataTypeId::TYPE_DRAG_FORCE,
          FlightDataTypeId::TYPE_WIND_VELOCITY, FlightDataTypeId::TYPE_WIND_DIRECTION,
          FlightDataTypeId::TYPE_TIME_STEP, FlightDataTypeId::TYPE_DAMPING_MOMENT_COEFF,
          FlightDataTypeId::TYPE_DAMPING_MOMENT_COEFF_AERODYNAMIC,
          FlightDataTypeId::TYPE_DAMPING_MOMENT_COEFF_PROPULSIVE,
          FlightDataTypeId::TYPE_CORRECTIVE_MOMENT_COEFF, FlightDataTypeId::TYPE_DAMPING_RATIO,
          FlightDataTypeId::TYPE_NATURAL_FREQUENCY})
    {
        EXPECT_TRUE(f.branch->hasType(FlightDataType::builtin(id)))
            << FlightDataType::builtin(id).getName();
    }
    EXPECT_FALSE(f.branch->hasType(FlightDataType::builtin(FlightDataTypeId::TYPE_MASS)));
    EXPECT_FALSE(f.branch->hasType(FlightDataType::builtin(FlightDataTypeId::TYPE_AOA)));
    EXPECT_FALSE(f.branch->hasType(FlightDataType::builtin(FlightDataTypeId::TYPE_DRAG_COEFF)));
}

// ======================================================================= stability values

/// The rows of the damping table whose damping ratio or natural frequency is not exactly
/// Java's, one line each.
[[nodiscard]] std::string dampingDifferences()
{
    std::string differences;
    for (std::size_t i = 0; i < kDampingPins.size(); i++)
    {
        const DampingPin& pin = kDampingPins.at(i);
        const RodStatus   f(pin.rodCleared);
        DataStore         store;
        if (pin.hasMass)
        {
            // A NaN inertia passes RigidBody's check, as in Java.
            store.rocketMass.emplace(Coordinate(0.2, 0, 0, 0.05), 1e-5, pin.inertia);
        }
        const double zeta = store.computeDampingRatio(f.status, pin.dampingMomentCoefficient,
                                                      pin.correctiveMomentCoefficient);
        const double omega =
            store.computeNaturalFrequency(f.status, pin.correctiveMomentCoefficient);
        if (!same(pin.dampingRatio, zeta) || !same(pin.naturalFrequency, omega))
        {
            differences += std::format("  row {}: expected {} and {}, got {} and {}\n", i,
                                       pin.dampingRatio, pin.naturalFrequency, zeta, omega);
        }
    }
    return differences;
}

// StepperTableProbe, "damping ratio and natural frequency" (the private methods, called by
// reflection): Cdm / (2 sqrt(Ccm I)) and sqrt(Ccm / I), with every way they have of being 0 or
// NaN. A division and a square root: exactly Java's.
TEST(AbstractSimulationStepperDataStore, TheDampingRatioAndTheNaturalFrequencyAreJavas)
{
    EXPECT_EQ(kDampingPins.size(), 17U);
    EXPECT_EQ(dampingDifferences(), "");
}

TEST(AbstractSimulationStepperDataStore, TheDampingRatioByCase)
{
    const RodStatus flying(true);
    const RodStatus onTheRod(false);
    DataStore       store;

    // Without a rocket mass: NaN, and for the frequency 0 until the rod is cleared.
    EXPECT_TRUE(std::isnan(store.computeDampingRatio(flying.status, 0.003, 0.9)));
    EXPECT_TRUE(std::isnan(store.computeDampingRatio(onTheRod.status, 0.003, 0.9)));
    EXPECT_TRUE(std::isnan(store.computeNaturalFrequency(flying.status, 0.9)));
    EXPECT_EQ(store.computeNaturalFrequency(onTheRod.status, 0.9), 0.0);

    store.rocketMass.emplace(Coordinate(0.2, 0, 0, 0.05), 1e-5, 0.5);
    // On the rod both are 0.
    EXPECT_EQ(store.computeDampingRatio(onTheRod.status, 0.25, 8.0), 0.0);
    EXPECT_EQ(store.computeNaturalFrequency(onTheRod.status, 8.0), 0.0);
    // In flight: 0.25 / (2 sqrt(8 * 0.5)) and sqrt(8 / 0.5).
    EXPECT_EQ(store.computeDampingRatio(flying.status, 0.25, 8.0), 0.0625);
    EXPECT_EQ(store.computeNaturalFrequency(flying.status, 8.0), 4.0);
    // An unstable rocket (a negative corrective moment) has neither.
    EXPECT_TRUE(std::isnan(store.computeDampingRatio(flying.status, 0.25, -8.0)));
    EXPECT_TRUE(std::isnan(store.computeNaturalFrequency(flying.status, -8.0)));
    // No corrective moment: no damping ratio, and a frequency of 0.
    EXPECT_TRUE(std::isnan(store.computeDampingRatio(flying.status, 0.25, 0.0)));
    EXPECT_EQ(store.computeNaturalFrequency(flying.status, 0.0), 0.0);
}

/// The rows of the motor mass derivative table whose result is not exactly Java's.
[[nodiscard]] std::string motorMassDerivativeDifferences()
{
    std::string differences;
    for (const MotorMassDerivativePin& pin : kMotorMassDerivativePins)
    {
        const FlightDataType& time = FlightDataType::builtin(FlightDataTypeId::TYPE_TIME);
        const FlightDataType& motorMass =
            FlightDataType::builtin(FlightDataTypeId::TYPE_MOTOR_MASS);
        FlightDataBranch branch("b", {time});
        if (pin.withMotorMass)
        {
            branch.addType(motorMass);
        }
        for (int row = 0; row < pin.rowCount; row++)
        {
            const auto index = static_cast<std::size_t>(row) * 2;
            branch.addPoint();
            branch.setValue(time, pin.rows.at(index));
            if (pin.withMotorMass)
            {
                branch.setValue(motorMass, pin.rows.at(index + 1));
            }
        }
        const double derivative = DataStore::computeMotorMassDerivative(branch);
        if (!same(pin.derivative, derivative))
        {
            differences +=
                std::format("  {}: expected {}, got {}\n", pin.label, pin.derivative, derivative);
        }
    }
    return differences;
}

// StepperTableProbe, "motor mass derivative": the difference quotient of the last two points,
// NaN without two points, without the column, or when the time does not advance.
TEST(AbstractSimulationStepperDataStore, TheMotorMassDerivativeIsJavas)
{
    EXPECT_EQ(kMotorMassDerivativePins.size(), 11U);
    EXPECT_EQ(motorMassDerivativeDifferences(), "");
}

/// The rows of the wind direction table whose result is not Java's: exactly for the zeros and
/// the NaNs, within 1e-12 for the others (an arc tangent).
[[nodiscard]] std::string windDirectionDifferences()
{
    std::string differences;
    for (const WindDirectionPin& pin : kWindDirectionPins)
    {
        const double direction = DataStore::getWindDirection(Coordinate(pin.x, pin.y, pin.z));
        const bool   exact     = std::isnan(pin.direction) || pin.direction == 0.0;
        const bool   matches =
            exact ? same(pin.direction, direction) : matchesPinnedValue(pin.direction, direction);
        if (!matches)
        {
            differences += std::format("  ({}, {}, {}): expected {}, got {}\n", pin.x, pin.y, pin.z,
                                       pin.direction, direction);
        }
    }
    return differences;
}

// StepperTableProbe, "wind direction": 0 is north, pi / 2 east; no wind is 0; a wind a hair
// west of north is just below 2 pi.
TEST(AbstractSimulationStepperDataStore, TheWindDirectionIsJavas)
{
    EXPECT_EQ(kWindDirectionPins.size(), 16U);
    EXPECT_EQ(windDirectionDifferences(), "");
    EXPECT_EQ(DataStore::getWindDirection(Coordinate(0, 1, 0)), 0.0);
    EXPECT_NEAR(DataStore::getWindDirection(Coordinate(1, 0, 0)), std::numbers::pi / 2, 1e-15);
}

/// The store of a row of the corrective moment table.
[[nodiscard]] DataStore correctiveStore(const CorrectivePin& pin)
{
    DataStore store;
    if (pin.hasConditions)
    {
        FlightConditions& conditions = store.flightConditions.emplace();
        conditions.setAtmosphericConditions(AtmosphericConditions(pin.temperature, pin.pressure));
        conditions.setMach(pin.mach);
        conditions.setRefLength(pin.refLength);
        conditions.setAOA(pin.aoa);
    }
    if (pin.hasMass)
    {
        store.rocketMass.emplace(Coordinate(pin.cg, 0, 0, 0.05), 1e-5, 2e-4);
    }
    if (pin.hasForces)
    {
        AerodynamicForces& forces = store.forces.emplace(AerodynamicForces{}.zero());
        forces.setCP(Coordinate(pin.cpX, 0, 0, pin.cna));
    }
    return store;
}

/// The rows of the corrective moment table whose result is not Java's, within 1e-12 (the
/// reference area holds pi, and the CP goes through a multiplication and a division).
[[nodiscard]] std::string correctiveDifferences()
{
    std::string differences;
    for (std::size_t i = 0; i < kCorrectivePins.size(); i++)
    {
        const CorrectivePin& pin = kCorrectivePins.at(i);
        const RodStatus      f(pin.rodCleared);
        const DataStore      store = correctiveStore(pin);
        const double         ccm   = store.computeCorrectiveMomentCoefficient(f.status);
        if (!matchesPinnedValue(pin.expected, ccm))
        {
            differences += std::format("  row {}: expected {}, got {}\n", i, pin.expected, ccm);
        }
    }
    return differences;
}

// StepperTableProbe, "corrective moment coefficient": 0.5 rho v^2 A CNa (CP - CG), 0 until the
// launch rod is cleared, NaN without the data or with a NaN angle of attack.
TEST(AbstractSimulationStepperDataStore, TheCorrectiveMomentCoefficientIsJavas)
{
    EXPECT_EQ(kCorrectivePins.size(), 12U);
    EXPECT_EQ(correctiveDifferences(), "");
}

TEST(AbstractSimulationStepperDataStore, TheDampingMomentIsZeroOnTheRodAndNaNWithoutData)
{
    const RodStatus flying(true);
    const RodStatus onTheRod(false);
    DataStore       store;

    // Without flight conditions or a rocket mass: NaN, on the rod too.
    DataStore::DampingMomentComponents c =
        store.computeDampingMomentCoefficientComponents(onTheRod.status, *onTheRod.branch);
    EXPECT_TRUE(std::isnan(c.total));
    EXPECT_TRUE(std::isnan(c.aerodynamic));
    EXPECT_TRUE(std::isnan(c.propulsive));

    store.flightConditions.emplace();
    store.rocketMass.emplace(Coordinate(0.2, 0, 0, 0.05), 1e-5, 2e-4);
    c = store.computeDampingMomentCoefficientComponents(onTheRod.status, *onTheRod.branch);
    EXPECT_EQ(c.total, 0.0);
    EXPECT_EQ(c.aerodynamic, 0.0);
    EXPECT_EQ(c.propulsive, 0.0);

    // On the ground the angle of attack is NaN, and so are the three values.
    store.flightConditions->setAOA(kNaN);
    c = store.computeDampingMomentCoefficientComponents(flying.status, *flying.branch);
    EXPECT_TRUE(std::isnan(c.total));
    EXPECT_TRUE(std::isnan(c.aerodynamic));
    EXPECT_TRUE(std::isnan(c.propulsive));

    // The propulsive part alone: nothing without two points of motor mass.
    EXPECT_EQ(store.computePropulsiveDampingMomentCoefficient(flying.status, *flying.branch), 0.0);
}

// ================================================================================ checkNaN

// StepperTableProbe, "checkNaN": Java's messages. The message for a number ends with a full
// stop; those for a coordinate and a quaternion go on with the value instead.
TEST(AbstractSimulationStepper, CheckNaNThrowsJavasMessages)
{
    EXPECT_EQ(bugText([] { AbstractSimulationStepper::checkNaN(1.5, "x"); }), "<none>");
    EXPECT_EQ(bugText([] { AbstractSimulationStepper::checkNaN(kInf, "x"); }), "<none>");
    EXPECT_EQ(bugText([] { AbstractSimulationStepper::checkNaN(kNaN, "store.timeStep"); }),
              "Simulation resulted in not-a-number (NaN) value for store.timeStep, please report "
              "a bug.");

    EXPECT_EQ(bugText([] { AbstractSimulationStepper::checkNaN(Coordinate(1, 2, 3, 4), "wind"); }),
              "<none>");
    EXPECT_EQ(bugText([] { AbstractSimulationStepper::checkNaN(Coordinate(1, kNaN, 3), "wind"); }),
              "Simulation resulted in not-a-number (NaN) value for wind, please report a bug, "
              "c=(1.00000,NaN,3.00000)");
    // A NaN weight counts, and is not printed (the coordinate is not "weighted").
    EXPECT_EQ(
        bugText([] { AbstractSimulationStepper::checkNaN(Coordinate(1, 2, 3, kNaN), "params.a"); }),
        "Simulation resulted in not-a-number (NaN) value for params.a, please report a "
        "bug, c=(1.00000,2.00000,3.00000)");

    EXPECT_EQ(bugText([] { AbstractSimulationStepper::checkNaN(Quaternion(1, 0, 0, 0), "q"); }),
              "<none>");
    // Java prints "q=Quaternion[NaN,0.500000,0.250000,0.000000,norm=NaN]": the quaternion's
    // toString() spells NaN differently here (see Quaternion::toString()).
    EXPECT_EQ(bugText([] {
                  AbstractSimulationStepper::checkNaN(Quaternion(kNaN, 0.5, 0.25, 0),
                                                      "orientation");
              }),
              "Simulation resulted in not-a-number (NaN) value for orientation, please report a "
              "bug, q=" +
                  Quaternion(kNaN, 0.5, 0.25, 0).toString());
}

// ========================================================================== the mass cache

/// The body tube of the Alpha III in the rocket of @p status.
[[nodiscard]] RocketComponent* bodyTubeOf(SimulationStatus& status)
{
    for (RocketComponent* component : status.getConfiguration().getAllComponents())
    {
        if (dynamic_cast<QtRocket::BodyTube*>(component) != nullptr)
        {
            return component;
        }
    }
    return nullptr;
}

// StepperTableProbe, "the structure mass cache": OpenRocket computes the structure mass once per
// modification id of the flight configuration. A change of a component's mass does not change
// that id, so the stepper goes on with the old mass until the configuration changes; a new
// stepper computes the new one.
//   first mass 0.025268291846128787 cg 0.19176843580003
//   configuration modID after the mass override: same
//   same stepper after the override: mass 0.025268291846128787
//   new stepper after the override: mass 0.5222304974138137 cg 0.17105327282024477
//   same stepper after updateModID: mass 0.5222304974138137
//   motor mass (armed) 0.0164 cg 0.23800000000000002
TEST(AbstractSimulationStepper, TheStructureMassIsCachedByTheConfigurationsModificationId)
{
    RodStatus            f(false);
    Rk4SimulationStepper stepper;

    const RigidBody first = stepper.calculateStructureMass(f.status);
    EXPECT_TRUE(matchesPinnedValue(0.025268291846128787, first.getMass()));
    EXPECT_TRUE(matchesPinnedValue(0.19176843580003, first.getCM().x));

    RocketComponent* body = bodyTubeOf(f.status);
    ASSERT_NE(body, nullptr);
    const QtRocket::ModId before = f.status.getConfiguration().getModId();
    body->setMassOverridden(true);
    body->setOverrideMass(0.5);
    EXPECT_EQ(f.status.getConfiguration().getModId(), before);

    // The same stepper: the cached mass. A new stepper: the new mass.
    EXPECT_EQ(stepper.calculateStructureMass(f.status).getMass(), first.getMass());
    Rk4SimulationStepper other;
    const RigidBody      fresh = other.calculateStructureMass(f.status);
    EXPECT_TRUE(matchesPinnedValue(0.5222304974138137, fresh.getMass()));
    EXPECT_TRUE(matchesPinnedValue(0.17105327282024477, fresh.getCM().x));

    // A new modification id of the configuration (a stage separates): recomputed.
    f.status.getConfiguration().updateModId();
    EXPECT_EQ(stepper.calculateStructureMass(f.status).getMass(), fresh.getMass());

    // The motor mass is computed every time (armed: the whole motor).
    const RigidBody motor = stepper.calculateMotorMass(f.status);
    EXPECT_TRUE(matchesPinnedValue(0.0164, motor.getMass()));
    EXPECT_TRUE(matchesPinnedValue(0.23800000000000002, motor.getCM().x));
}

/// Answers preMassCalculation() with a fixed body while it is switched on (the switch is shared
/// with its clones).
class MassOverride final : public CloneableSimulationListener<MassOverride>
{
public:
    MassOverride() = default;

    void setOn(bool on) const noexcept { *m_on = on; }

    [[nodiscard]] std::optional<RigidBody> preMassCalculation(SimulationStatus& /*status*/) override
    {
        if (!*m_on)
        {
            return std::nullopt;
        }
        return RigidBody(Coordinate(0.1, 0, 0, 2.5), 1e-3, 1e-2);
    }

private:
    std::shared_ptr<bool> m_on{std::make_shared<bool>(true)};
};

// "Call pre-listener - if it overrides, skip the cache entirely": the listener's mass is not
// kept, and the cache is not filled by it.
TEST(AbstractSimulationStepper, AListenersMassBypassesTheCache)
{
    const TestEstesAlphaIII alpha;
    const auto              conditions = std::make_shared<SimulationConditions>();
    const auto              listener   = std::make_shared<MassOverride>();
    conditions->getSimulationListenerList().push_back(listener);
    SimulationStatus status(statusConfiguration(*alpha.rocket, testFcid(0)), conditions);
    status.setFlightDataBranch(newBranch());
    Rk4SimulationStepper stepper;

    EXPECT_EQ(stepper.calculateStructureMass(status).getMass(), 2.5);
    // The motor mass asks the same hook.
    EXPECT_EQ(stepper.calculateMotorMass(status).getMass(), 2.5);

    listener->setOn(false);
    EXPECT_TRUE(
        matchesPinnedValue(0.025268291846128787, stepper.calculateStructureMass(status).getMass()));
    listener->setOn(true);
    EXPECT_EQ(stepper.calculateStructureMass(status).getMass(), 2.5);
}

// ============================================================================== the models

// New SimulationConditions have no models: Java throws a NullPointerException from each.
TEST(AbstractSimulationStepper, AMissingModelIsABug)
{
    RodStatus            f(false);
    Rk4SimulationStepper stepper;

    EXPECT_EQ(bugText([&] { static_cast<void>(stepper.modelAtmosphericConditions(f.status)); }),
              "The simulation conditions have no atmospheric model");
    EXPECT_EQ(bugText([&] { static_cast<void>(stepper.modelWindVelocity(f.status)); }),
              "The simulation conditions have no wind model");
    EXPECT_EQ(bugText([&] { static_cast<void>(stepper.modelGravity(f.status)); }),
              "The simulation conditions have no gravity model");
}

// ============================================================================ the scenarios

/// The scenarios of this file: the functions of the base class called directly on a status.
[[nodiscard]] bool isDirectScenario(const StepScenarioPin& pin)
{
    return pin.action == "flight-conditions" || pin.action == "landed-values" ||
           pin.action == "empty-store";
}

class DirectScenario : public ::testing::TestWithParam<std::string>
{ };

// "flight-conditions-...": calculateFlightConditions() at rest, below the velocity and the
// lateral airspeed thresholds, flying backwards, in a wind, from a landing stepper, and with
// the nozzles of thrusting motors (also on the two rockets whose own configuration does not
// list every motor: see setNozzleExitDiameters()). "landed-values-...": landedValues() from three
// kinds of stepper. "empty-store...": a store without anything in it written to a point of the
// branch, in flight and on the rod.
TEST_P(DirectScenario, TheResultIsOpenRockets)
{
    EXPECT_EQ(javaScenarioDifferences(GetParam()), "");
}

INSTANTIATE_TEST_SUITE_P(AbstractSimulationStepper, DirectScenario,
                         ::testing::ValuesIn(stepScenarioNames(isDirectScenario)),
                         [](const ::testing::TestParamInfo<std::string>& paramInfo) {
                             return scenarioTestName(paramInfo.param);
                         });

TEST(AbstractSimulationStepper, TheScenariosCoverTheThresholdsOfTheFlightConditions)
{
    const std::vector<std::string> expected{
        "flight-conditions-rest",
        "flight-conditions-slow",
        "flight-conditions-axial",
        "flight-conditions-lateral-tiny",
        "flight-conditions-lateral-small",
        "flight-conditions-lateral",
        "flight-conditions-backwards",
        "flight-conditions-sideways-slow",
        "flight-conditions-wind",
        "flight-conditions-landing-stepper",
        "flight-conditions-nozzles",
        "flight-conditions-nozzles-motor-pods",
        "flight-conditions-nozzles-second-motor",
        "empty-store",
        "empty-store-on-the-rod",
        "landed-values-rk4",
        "landed-values-landing",
        "landed-values-ground",
        "landed-values-falcon",
    };
    EXPECT_EQ(stepScenarioNames(isDirectScenario), expected);
}

/// What calculateFlightConditions() makes of a lateral airspeed.
struct LateralConditions
{
    double theta{kNaN};
    double pitchRate{kNaN};
    double yawRate{kNaN};
    double rollRate{kNaN};
    double lateralPitchRate{kNaN};  ///< of the store: what limits the time step
    bool   calm{false};             ///< the wind of the status is exactly zero
};

/// calculateFlightConditions() of @p stepper for @p status flying at 20 m/s along its axis and
/// at @p lateral m/s across it (along its y axis), turning at (0.1, 0.2, 0.3) rad/s. The
/// orientation is the identity, so that the airspeed in rocket coordinates is the velocity and
/// its lateral part is @p lateral to the last bit (the square root of its square).
[[nodiscard]] LateralConditions conditionsAtLateralAirspeed(Rk4SimulationStepper& stepper,
                                                            SimulationStatus&     status,
                                                            double                lateral)
{
    DataStore& store = QtRocket::Test::scenarioStore(stepper);
    status.setRocketOrientationQuaternion(Quaternion(1, 0, 0, 0));
    status.setRocketRotationVelocity(Coordinate(0.1, 0.2, 0.3));
    status.setRocketVelocity(Coordinate(0, lateral, 20.0));
    stepper.calculateFlightConditions(status, store);
    const FlightConditions& conditions = stored(store.flightConditions);
    return {.theta            = conditions.getTheta(),
            .pitchRate        = conditions.getPitchRate(),
            .yawRate          = conditions.getYawRate(),
            .rollRate         = conditions.getRollRate(),
            .lateralPitchRate = store.lateralPitchRate,
            .calm             = store.windVelocity.exactlyEquals(Coordinate::kZero)};
}

// The two thresholds of the lateral airspeed in calculateFlightConditions(), to the last bit:
//   if (len > 0.0001) the direction of the lateral airspeed, else none (theta 0);
//   if (len < 0.001) no pitch and no yaw rate, else those of the rocket.
// The scenarios above stay a factor of two and more away from them (0.086 mm/s, 0.5 mm/s and
// 0.36 m/s). The second one decides where a weathercocked rocket flies without pitch damping
// (the "hunting" of the stable-step simulation goldens, rule H of simulation_golden_tests.cpp),
// and that comparison leaves the rows nearest to the threshold to this test: it does not
// compare the pitch rate of a row whose lateral airspeed is within 0.15 mm/s of it.
TEST(AbstractSimulationStepper, TheThresholdsOfTheLateralAirspeedAreExact)
{
    Rk4SimulationStepper                stepper;
    QtRocket::Test::InitializedScenario f("flight-conditions-lateral-small", stepper);
    ASSERT_NE(f.status, nullptr);

    // At 1 mm/s the rates are the rocket's; one bit below they are zero.
    const LateralConditions at = conditionsAtLateralAirspeed(stepper, *f.status, 0.001);
    ASSERT_TRUE(at.calm) << "the scenario has a wind";
    EXPECT_EQ(at.pitchRate, 0.2);
    EXPECT_EQ(at.yawRate, 0.1);
    EXPECT_EQ(at.rollRate, 0.3);
    EXPECT_EQ(at.lateralPitchRate, QtRocket::MathUtil::hypot(0.1, 0.2));
    const LateralConditions below =
        conditionsAtLateralAirspeed(stepper, *f.status, std::nextafter(0.001, 0.0));
    EXPECT_EQ(below.pitchRate, 0.0);
    EXPECT_EQ(below.yawRate, 0.0);
    EXPECT_EQ(below.rollRate, 0.3) << "the roll rate is kept";
    EXPECT_EQ(below.lateralPitchRate, 0.0);
    EXPECT_EQ(below.theta, std::numbers::pi / 2) << "the direction is known down to 0.1 mm/s";

    // At 0.1 mm/s there is no direction yet; one bit above there is.
    EXPECT_EQ(conditionsAtLateralAirspeed(stepper, *f.status, 0.0001).theta, 0.0);
    EXPECT_EQ(conditionsAtLateralAirspeed(stepper, *f.status, std::nextafter(0.0001, 1.0)).theta,
              std::numbers::pi / 2);
}

// landedValues(): what the pins of the "landed-values" scenarios say, by name.
TEST(AbstractSimulationStepper, TheLandedValuesPutTheRocketOnTheGround)
{
    GroundStepper                       stepper;
    QtRocket::Test::InitializedScenario f("landed-values-ground", stepper);
    ASSERT_NE(f.status, nullptr);
    const Coordinate before = f.status->getRocketPosition();
    ASSERT_GT(before.z, 0.0);

    stepper.landedValues(*f.status, stepper.getStore());

    const DataStore& store = stepper.getStore();
    EXPECT_TRUE(f.status->getRocketPosition().exactlyEquals(Coordinate(before.x, before.y, 0)));
    EXPECT_TRUE(f.status->getRocketVelocity().exactlyEquals(Coordinate::kZero));
    EXPECT_TRUE(std::isnan(store.timeStep));
    EXPECT_EQ(store.thrustForce, 0.0);
    EXPECT_EQ(store.thrustCorrection, 0.0);
    EXPECT_EQ(store.dragForce, 0.0);
    EXPECT_GT(store.gravity, 9.7);
    const FlightConditions& conditions = stored(store.flightConditions);
    EXPECT_TRUE(std::isnan(conditions.getAOA()));
    EXPECT_EQ(conditions.getRollRate(), 0.0);
    EXPECT_EQ(conditions.getPitchRate(), 0.0);
    EXPECT_EQ(conditions.getYawRate(), 0.0);
    const AerodynamicForces& forces = stored(store.forces);
    EXPECT_TRUE(std::isnan(forces.getCD()));
    EXPECT_TRUE(std::isnan(forces.getCDaxial()));
    EXPECT_TRUE(std::isnan(forces.getCN()));
    EXPECT_TRUE(
        stored(store.accelerationData).getLinearAccelerationWC().exactlyEquals(Coordinate::kZero));
    EXPECT_TRUE(stored(store.coriolisAcceleration).exactlyEquals(Coordinate::kZero));
    // The mass is that of the structure and the (burnt-out) motor.
    EXPECT_GT(stored(store.rocketMass).getMass(), stored(store.motorMass).getMass());
}

}  // namespace
