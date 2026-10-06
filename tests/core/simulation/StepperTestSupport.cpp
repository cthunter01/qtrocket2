#include "simulation/StepperTestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/AbstractEulerStepper.h"
#include "QtRocket/simulation/AbstractRkSimulationStepper.h"
#include "QtRocket/simulation/AbstractSimulationStepper.h"
#include "QtRocket/simulation/AccelerationData.h"
#include "QtRocket/simulation/BasicLandingStepper.h"
#include "QtRocket/simulation/BasicTumbleStepper.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/GroundStepper.h"
#include "QtRocket/simulation/Rk4SimulationStepper.h"
#include "QtRocket/simulation/Rk6SimulationStepper.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/SimulationStepper.h"
#include "QtRocket/simulation/exception/SimulationCalculationException.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/Quaternion.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/WorldCoordinate.h"
#include "rocket/TestRockets.h"
#include "simulation/JitterRemoval.h"
#include "simulation/SimulationTestSupport.h"

namespace QtRocket::Test
{

namespace
{

// ------------------------------------------------------------------------------ listeners

/// StepperProbe.RecordingListener: records the order of the computation hooks (and of
/// addFlightEvent()) in a list its clones share; changes nothing.
class RecordingListener final : public CloneableSimulationListener<RecordingListener>
{
public:
    explicit RecordingListener(std::shared_ptr<std::vector<std::string>> log)
      : m_log(std::move(log))
    {
    }

    [[nodiscard]] bool isSystemListener() const override { return true; }

    [[nodiscard]] std::optional<AccelerationData> preAccelerationCalculation(
        SimulationStatus& /*status*/) override
    {
        m_log->emplace_back("preAcceleration");
        return std::nullopt;
    }
    [[nodiscard]] std::optional<AccelerationData> postAccelerationCalculation(
        SimulationStatus& /*status*/, const AccelerationData& /*acceleration*/) override
    {
        m_log->emplace_back("postAcceleration");
        return std::nullopt;
    }
    [[nodiscard]] std::optional<AtmosphericConditions> preAtmosphericModel(
        SimulationStatus& /*status*/) override
    {
        m_log->emplace_back("preAtmosphere");
        return std::nullopt;
    }
    [[nodiscard]] std::optional<AtmosphericConditions> postAtmosphericModel(
        SimulationStatus& /*status*/, const AtmosphericConditions& /*conditions*/) override
    {
        m_log->emplace_back("postAtmosphere");
        return std::nullopt;
    }
    [[nodiscard]] std::optional<Coordinate> preWindModel(SimulationStatus& /*status*/) override
    {
        m_log->emplace_back("preWind");
        return std::nullopt;
    }
    [[nodiscard]] std::optional<Coordinate> postWindModel(SimulationStatus& /*status*/,
                                                          const Coordinate& /*wind*/) override
    {
        m_log->emplace_back("postWind");
        return std::nullopt;
    }
    [[nodiscard]] double preGravityModel(SimulationStatus& /*status*/) override
    {
        m_log->emplace_back("preGravity");
        return std::numeric_limits<double>::quiet_NaN();
    }
    [[nodiscard]] double postGravityModel(SimulationStatus& /*status*/, double /*gravity*/) override
    {
        m_log->emplace_back("postGravity");
        return std::numeric_limits<double>::quiet_NaN();
    }
    [[nodiscard]] std::optional<FlightConditions> preFlightConditions(
        SimulationStatus& /*status*/) override
    {
        m_log->emplace_back("preFlightConditions");
        return std::nullopt;
    }
    [[nodiscard]] std::optional<FlightConditions> postFlightConditions(
        SimulationStatus& /*status*/, const FlightConditions& /*flightConditions*/) override
    {
        m_log->emplace_back("postFlightConditions");
        return std::nullopt;
    }
    [[nodiscard]] std::optional<AerodynamicForces> preAerodynamicCalculation(
        SimulationStatus& /*status*/) override
    {
        m_log->emplace_back("preAerodynamics");
        return std::nullopt;
    }
    [[nodiscard]] std::optional<AerodynamicForces> postAerodynamicCalculation(
        SimulationStatus& /*status*/, const AerodynamicForces& /*forces*/) override
    {
        m_log->emplace_back("postAerodynamics");
        return std::nullopt;
    }
    [[nodiscard]] std::optional<RigidBody> preMassCalculation(SimulationStatus& /*status*/) override
    {
        m_log->emplace_back("preMass");
        return std::nullopt;
    }
    [[nodiscard]] std::optional<RigidBody> postMassCalculation(
        SimulationStatus& /*status*/, const RigidBody& /*massData*/) override
    {
        m_log->emplace_back("postMass");
        return std::nullopt;
    }
    [[nodiscard]] double preSimpleThrustCalculation(SimulationStatus& /*status*/) override
    {
        m_log->emplace_back("preThrust");
        return std::numeric_limits<double>::quiet_NaN();
    }
    [[nodiscard]] double postSimpleThrustCalculation(SimulationStatus& /*status*/,
                                                     double /*thrust*/) override
    {
        m_log->emplace_back("postThrust");
        return std::numeric_limits<double>::quiet_NaN();
    }
    [[nodiscard]] bool addFlightEvent(SimulationStatus& /*status*/,
                                      const FlightEvent& /*event*/) override
    {
        m_log->emplace_back("addFlightEvent");
        return true;
    }

private:
    std::shared_ptr<std::vector<std::string>> m_log;
};

/// StepperProbe.OverridingListener: answers the one hook its kind names; the others are left
/// alone. Not a system listener.
class OverridingListener final : public CloneableSimulationListener<OverridingListener>
{
public:
    explicit OverridingListener(std::string_view kind) : m_kind(kind) { }

    [[nodiscard]] std::optional<FlightConditions> preFlightConditions(
        SimulationStatus& status) override
    {
        if (m_kind != "pre-flight-conditions")
        {
            return std::nullopt;
        }
        FlightConditions c(status.getConfiguration());
        c.setAtmosphericConditions(AtmosphericConditions(281.5, 92000));
        c.setTheta(0.3);
        c.setAOA(0.1);
        c.setMach(0.25);
        c.setRollRate(1.5);
        c.setPitchRate(0.2);
        c.setYawRate(-0.1);
        return c;
    }

    [[nodiscard]] std::optional<FlightConditions> postFlightConditions(
        SimulationStatus& /*status*/, const FlightConditions& flightConditions) override
    {
        if (m_kind == "post-flight-conditions")
        {
            FlightConditions changed = flightConditions;
            changed.setAOA(changed.getAOA() + 0.01);
            changed.setTheta(changed.getTheta() + 0.2);
            changed.setPitchRate(0.3);
            changed.setYawRate(0.4);
            return changed;
        }
        if (m_kind == "post-flight-conditions-same")
        {
            // Returns conditions that equal the given ones: nothing is replaced.
            return flightConditions;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<AerodynamicForces> preAerodynamicCalculation(
        SimulationStatus& /*status*/) override
    {
        if (m_kind != "pre-aerodynamics")
        {
            return std::nullopt;
        }
        AerodynamicForces f = AerodynamicForces{}.zero();
        f.setCP(Coordinate(0.21, 0, 0, 9.5));
        f.setCN(0.5);
        f.setCm(1.0);
        f.setCside(0.1);
        f.setCyaw(0.2);
        f.setCroll(0.01);
        f.setCDaxial(0.6);
        f.setCD(0.65);
        f.setFrictionCD(0.3);
        f.setPressureCD(0.25);
        f.setBaseCD(0.1);
        return f;
    }

    [[nodiscard]] std::optional<AerodynamicForces> postAerodynamicCalculation(
        SimulationStatus& /*status*/, const AerodynamicForces& forces) override
    {
        if (m_kind != "post-aerodynamics")
        {
            return std::nullopt;
        }
        AerodynamicForces changed = forces;
        changed.setCDaxial(changed.getCDaxial() * 2);
        changed.setCD(changed.getCD() * 2);
        return changed;
    }

    [[nodiscard]] double preSimpleThrustCalculation(SimulationStatus& /*status*/) override
    {
        return m_kind == "pre-thrust" ? 12.5 : std::numeric_limits<double>::quiet_NaN();
    }

    [[nodiscard]] double postSimpleThrustCalculation(SimulationStatus& /*status*/,
                                                     double thrust) override
    {
        return m_kind == "post-thrust" ? thrust * 1.1 : std::numeric_limits<double>::quiet_NaN();
    }

    [[nodiscard]] double preGravityModel(SimulationStatus& /*status*/) override
    {
        return m_kind == "pre-gravity" ? 9.5 : std::numeric_limits<double>::quiet_NaN();
    }

    [[nodiscard]] double postGravityModel(SimulationStatus& /*status*/, double gravity) override
    {
        return m_kind == "post-gravity" ? gravity * 0.5 : std::numeric_limits<double>::quiet_NaN();
    }

    [[nodiscard]] std::optional<Coordinate> preWindModel(SimulationStatus& /*status*/) override
    {
        if (m_kind != "pre-wind")
        {
            return std::nullopt;
        }
        return Coordinate(1, 2, 0.5);
    }

    [[nodiscard]] std::optional<Coordinate> postWindModel(SimulationStatus& /*status*/,
                                                          const Coordinate& wind) override
    {
        if (m_kind != "post-wind")
        {
            return std::nullopt;
        }
        return wind.add(0.5, -0.25, 0.125);
    }

    [[nodiscard]] std::optional<AtmosphericConditions> preAtmosphericModel(
        SimulationStatus& /*status*/) override
    {
        if (m_kind != "pre-atmosphere")
        {
            return std::nullopt;
        }
        return AtmosphericConditions(280, 90000);
    }

    [[nodiscard]] std::optional<AtmosphericConditions> postAtmosphericModel(
        SimulationStatus& /*status*/, const AtmosphericConditions& c) override
    {
        if (m_kind != "post-atmosphere")
        {
            return std::nullopt;
        }
        return AtmosphericConditions(c.getTemperature() - 10, c.getPressure() * 0.9);
    }

    [[nodiscard]] std::optional<RigidBody> preMassCalculation(SimulationStatus& /*status*/) override
    {
        if (m_kind == "zero-mass")
        {
            // The structure and the motors each weigh nothing: the rocket has no mass.
            return RigidBody(Coordinate(0.15, 0, 0, 0), 0, 0);
        }
        if (m_kind != "pre-mass")
        {
            return std::nullopt;
        }
        return RigidBody(Coordinate(0.15, 0, 0, 0.02), 1e-6, 1e-4);
    }

    [[nodiscard]] std::optional<RigidBody> postMassCalculation(SimulationStatus& /*status*/,
                                                               const RigidBody& massData) override
    {
        if (m_kind != "post-mass")
        {
            return std::nullopt;
        }
        return massData.scaleMass(1.5);
    }

    [[nodiscard]] std::optional<AccelerationData> preAccelerationCalculation(
        SimulationStatus& status) override
    {
        if (m_kind != "pre-acceleration")
        {
            return std::nullopt;
        }
        return AccelerationData(std::nullopt, std::nullopt, Coordinate(0.5, -0.25, 12),
                                Coordinate(0.1, 0.2, 0.3), status.getRocketOrientationQuaternion());
    }

    [[nodiscard]] std::optional<AccelerationData> postAccelerationCalculation(
        SimulationStatus& /*status*/, const AccelerationData& acceleration) override
    {
        if (m_kind != "post-acceleration")
        {
            return std::nullopt;
        }
        return AccelerationData(
            std::nullopt, std::nullopt, acceleration.getLinearAccelerationWC().multiply(0.5),
            acceleration.getRotationalAccelerationWC().multiply(2), acceleration.getRotation());
    }

private:
    std::string m_kind;
};

// -------------------------------------------------------------------------------- outputs

/// Collects the outputs in the probe's order.
class Outputs
{
public:
    void add(std::string label, double value) { m_values.emplace_back(std::move(label), value); }

    void add(std::string_view label, const Coordinate& c)
    {
        add(std::format("{}.x", label), c.x);
        add(std::format("{}.y", label), c.y);
        add(std::format("{}.z", label), c.z);
    }

    [[nodiscard]] std::vector<std::pair<std::string, double>> take() { return std::move(m_values); }

private:
    std::vector<std::pair<std::string, double>> m_values;
};

/// The data of an event as the probe prints it: "warning:<text>" (the warning as the status's
/// warning set holds it now, which is the object Java's event shares with the set),
/// "abort:<CAUSE>", "-" for no data, or the simple name of the Java class of the data.
struct EventDataText
{
    const WarningSet* warnings;

    [[nodiscard]] std::string operator()(std::monostate /*none*/) const { return "-"; }
    [[nodiscard]] std::string operator()(const std::shared_ptr<MotorClusterState>& /*state*/) const
    {
        return "MotorClusterState";
    }
    [[nodiscard]] std::string operator()(const std::shared_ptr<const Warning>& warning) const
    {
        const Warning* stored = warnings == nullptr ? nullptr : warnings->find(*warning);
        return "warning:" + (stored == nullptr ? warning->toString() : stored->toString());
    }
    [[nodiscard]] std::string operator()(const SimulationAbort& abort) const
    {
        return std::format("abort:{}", causeName(abort.cause()));
    }
    [[nodiscard]] std::string operator()(const FlightEvent::AltitudeChange& /*change*/) const
    {
        return "Pair";
    }
    [[nodiscard]] std::string operator()(const std::string& /*text*/) const { return "String"; }
};

/// StepperProbe.eventText().
[[nodiscard]] std::string eventText(const FlightEvent& event, const WarningSet* warnings)
{
    const std::string source =
        event.getSource() == nullptr ? std::string{"-"} : event.getSource()->getName();
    return std::format("{} @{} src={} data={}", name(event.getType()),
                       Strings::javaDoubleToString(event.getTime()), source,
                       std::visit(EventDataText{warnings}, event.getData()));
}

/// The outputs of the data store, in the probe's order.
void addStoreOutputs(Outputs& out, const AbstractSimulationStepper::DataStore& store)
{
    out.add("store.timeStep", store.timeStep);
    out.add("store.gravity", store.gravity);
    out.add("store.thrustForce", store.thrustForce);
    out.add("store.thrustCorrection", store.thrustCorrection);
    out.add("store.dragForce", store.dragForce);
    out.add("store.lateralPitchRate", store.lateralPitchRate);
    out.add("store.windVelocity", store.windVelocity);
    if (store.thetaRotation.has_value())
    {
        out.add("store.thetaRotation.sin", store.thetaRotation->sin());
        out.add("store.thetaRotation.cos", store.thetaRotation->cos());
    }
    if (store.launchRodDirection.has_value())
    {
        out.add("store.launchRodDirection", *store.launchRodDirection);
    }
    if (store.coriolisAcceleration.has_value())
    {
        out.add("store.coriolisAcceleration", *store.coriolisAcceleration);
    }
    if (store.rocketMass.has_value())
    {
        out.add("store.rocketMass.cg", store.rocketMass->getCM().x);
        out.add("store.rocketMass.mass", store.rocketMass->getMass());
        out.add("store.rocketMass.longitudinalInertia", store.rocketMass->getLongitudinalInertia());
        out.add("store.rocketMass.rotationalInertia", store.rocketMass->getRotationalInertia());
    }
    if (store.motorMass.has_value())
    {
        out.add("store.motorMass.cg", store.motorMass->getCM().x);
        out.add("store.motorMass.mass", store.motorMass->getMass());
    }
    if (store.forces.has_value())
    {
        out.add("store.forces.CN", store.forces->getCN());
        out.add("store.forces.Cm", store.forces->getCm());
        out.add("store.forces.Cside", store.forces->getCside());
        out.add("store.forces.Cyaw", store.forces->getCyaw());
        out.add("store.forces.Croll", store.forces->getCroll());
        out.add("store.forces.CDaxial", store.forces->getCDaxial());
        out.add("store.forces.CD", store.forces->getCD());
        out.add("store.forces.cp.x", store.forces->getCP().x);
        out.add("store.forces.cp.weight", store.forces->getCP().weight);
    }
    if (store.flightConditions.has_value())
    {
        const FlightConditions& fc = *store.flightConditions;
        out.add("store.flightConditions.aoa", fc.getAOA());
        out.add("store.flightConditions.sinAOA", fc.getSinAOA());
        out.add("store.flightConditions.theta", fc.getTheta());
        out.add("store.flightConditions.mach", fc.getMach());
        out.add("store.flightConditions.rollRate", fc.getRollRate());
        out.add("store.flightConditions.pitchRate", fc.getPitchRate());
        out.add("store.flightConditions.yawRate", fc.getYawRate());
        out.add("store.flightConditions.refLength", fc.getRefLength());
        out.add("store.flightConditions.nozzleExitArea", fc.getThrustingNozzleExitArea());
        out.add("store.flightConditions.temperature",
                fc.getAtmosphericConditions().getTemperature());
        out.add("store.flightConditions.pressure", fc.getAtmosphericConditions().getPressure());
    }
    if (store.accelerationData.has_value())
    {
        out.add("store.acceleration.linearWC", store.accelerationData->getLinearAccelerationWC());
        out.add("store.acceleration.rotationalWC",
                store.accelerationData->getRotationalAccelerationWC());
    }
    if (store.flightConditions.has_value())
    {
        out.add("store.flightConditions.sincAOA", store.flightConditions->getSincAOA());
        out.add("store.flightConditions.velocity", store.flightConditions->getVelocity());
    }
}

/// The outputs of the status, in the probe's order.
void addStatusOutputs(Outputs& out, const SimulationStatus& status)
{
    out.add("status.time", status.getSimulationTime());
    out.add("status.position", status.getRocketPosition());
    out.add("status.velocity", status.getRocketVelocity());
    out.add("status.q.w", status.getRocketOrientationQuaternion().w());
    out.add("status.q.x", status.getRocketOrientationQuaternion().x());
    out.add("status.q.y", status.getRocketOrientationQuaternion().y());
    out.add("status.q.z", status.getRocketOrientationQuaternion().z());
    out.add("status.rotationVelocity", status.getRocketRotationVelocity());
    out.add("status.world.latitude", status.getRocketWorldPosition().getLatitudeRad());
    out.add("status.world.longitude", status.getRocketWorldPosition().getLongitudeRad());
    out.add("status.world.altitude", status.getRocketWorldPosition().getAltitude());
    out.add("status.maxZVelocity", status.getMaxZVelocity());
}

/// The rows of the branch, in the probe's order, and the "types" text.
[[nodiscard]] std::string addBranchOutputs(Outputs& out, const FlightDataBranch& branch)
{
    const std::vector<const FlightDataType*> types     = branch.getTypes();
    std::string                              typeNames = "types";
    for (const FlightDataType* type : types)
    {
        typeNames += ' ';
        typeNames += type->getSaveKey();
    }
    out.add("branch.rows", static_cast<double>(branch.getLength()));
    const FlightDataType& computationTime =
        FlightDataType::builtin(FlightDataTypeId::TYPE_COMPUTATION_TIME);
    for (std::size_t row = 0; row < branch.getLength(); row++)
    {
        for (const FlightDataType* type : types)
        {
            if (type == &computationTime)
            {
                continue;  // the wall clock
            }
            out.add(
                std::format("row{}.{}", row, type->getSaveKey()),
                branch.getByIndex(*type, row).value_or(std::numeric_limits<double>::quiet_NaN()));
        }
    }
    return typeNames;
}

/// Does the action of @p pin with @p stepper on @p status. Returns the thrust of the "thrust"
/// action, NaN for the others.
[[nodiscard]] double doAction(const StepScenarioPin& pin, AbstractSimulationStepper& stepper,
                              SimulationStatus& status)
{
    AbstractSimulationStepper::DataStore& store = scenarioStore(stepper);
    if (pin.action == "step")
    {
        const int steps = static_cast<int>(pin.input("steps"));
        for (int i = 0; i < steps; i++)
        {
            stepper.step(status, pin.input("maxTimeStep"));
        }
    }
    else if (pin.action == "flight-conditions")
    {
        stepper.calculateFlightConditions(status, store);
    }
    else if (pin.action == "thrust")
    {
        stepper.calculateFlightConditions(status, store);
        auto* rk = dynamic_cast<AbstractRkSimulationStepper*>(&stepper);
        if (rk == nullptr)
        {
            ADD_FAILURE() << "the thrust action needs a Runge-Kutta stepper";
            return std::numeric_limits<double>::quiet_NaN();
        }
        return rk->calculateThrust(status, store);
    }
    else if (pin.action == "landed-values")
    {
        stepper.landedValues(status, store);
    }
    else if (pin.action == "empty-store")
    {
        status.storeData();
        const AbstractSimulationStepper::DataStore empty;
        empty.storeData(status);
    }
    else
    {
        ADD_FAILURE() << "unknown action " << pin.action;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

/// Whether @p actual is the pinned value @p expected (see stepScenarioDifferences()).
[[nodiscard]] bool matchesPin(double expected, double actual, double relativeTolerance,
                              double absoluteTolerance)
{
    if (std::isnan(expected) || std::isnan(actual))
    {
        return std::isnan(expected) && std::isnan(actual);
    }
    if (std::isinf(expected) || std::isinf(actual))
    {
        return expected == actual;
    }
    const double difference = std::abs(actual - expected);
    return difference <= relativeTolerance * std::max(std::abs(expected), std::abs(actual)) ||
           difference <= absoluteTolerance;
}

}  // namespace

// ---------------------------------------------------------------------------------- pins

double StepScenarioPin::input(std::string_view label) const
{
    for (std::size_t i = 0; i < inputLabels.size() && i < inputs.size(); i++)
    {
        if (inputLabels[i] == label)
        {
            return inputs[i];
        }
    }
    ADD_FAILURE() << "the scenario " << name << " has no input " << label;
    return std::numeric_limits<double>::quiet_NaN();
}

bool StepScenarioPin::flag(std::string_view label) const
{
    return input(label) != 0.0;
}

const StepScenarioPin* findStepScenario(std::string_view name)
{
    for (const StepScenarioPin& pin : stepScenarioPins())
    {
        if (pin.name == name)
        {
            return &pin;
        }
    }
    ADD_FAILURE() << "no scenario " << name;
    return nullptr;
}

std::vector<const StepScenarioPin*> stepScenariosStartingWith(std::string_view prefix)
{
    std::vector<const StepScenarioPin*> pins;
    for (const StepScenarioPin& pin : stepScenarioPins())
    {
        if (pin.name.starts_with(prefix))
        {
            pins.push_back(&pin);
        }
    }
    return pins;
}

// -------------------------------------------------------------------------------- running

std::unique_ptr<Rocket> makeScenarioRocket(std::string_view maker)
{
    for (const TestRocketMaker& candidate : testRocketMakers())
    {
        if (candidate.method == maker)
        {
            return candidate.make();
        }
    }
    ADD_FAILURE() << "no test rocket " << maker;
    return std::make_unique<Rocket>();
}

FlightConfiguration& scenarioConfiguration(Rocket& rocket)
{
    FlightConfiguration& selected = rocket.getSelectedConfiguration();
    if (!selected.getFlightConfigurationId().isDefaultId())
    {
        return selected;
    }
    if (rocket.containsFlightConfigurationId(testFcid(0)))
    {
        return rocket.getFlightConfiguration(testFcid(0));
    }
    return selected;
}

std::unique_ptr<AbstractSimulationStepper> makeScenarioStepper(std::string_view name)
{
    if (name == "RK4")
    {
        return std::make_unique<Rk4SimulationStepper>();
    }
    if (name == "RK6")
    {
        return std::make_unique<Rk6SimulationStepper>();
    }
    if (name == "LANDING")
    {
        return std::make_unique<BasicLandingStepper>();
    }
    if (name == "TUMBLE")
    {
        return std::make_unique<BasicTumbleStepper>();
    }
    if (name == "GROUND")
    {
        return std::make_unique<GroundStepper>();
    }
    ADD_FAILURE() << "unknown stepper " << name;
    return nullptr;
}

AbstractSimulationStepper::DataStore& scenarioStore(AbstractSimulationStepper& stepper)
{
    if (auto* rk = dynamic_cast<AbstractRkSimulationStepper*>(&stepper); rk != nullptr)
    {
        return rk->getStore();
    }
    if (auto* euler = dynamic_cast<AbstractEulerStepper*>(&stepper); euler != nullptr)
    {
        return euler->getStore();
    }
    return dynamic_cast<GroundStepper&>(stepper).getStore();
}

void setNozzleExitDiameters(FlightConfiguration&                 configuration,
                            const std::function<double(double)>& diameter)
{
    for (RocketComponent* component : configuration.getAllActiveComponents())
    {
        auto* mount = dynamic_cast<MotorMount*>(component);
        if (mount == nullptr || !mount->getMotorConfig(configuration.getId()).hasMotor())
        {
            continue;
        }
        MotorConfiguration& motorConfiguration = mount->getMotorConfig(configuration.getId());
        const Result<void>  set                = motorConfiguration.setNozzleExitDiameter(
            diameter(motorConfiguration.getMotor()->getDiameter()));
        EXPECT_TRUE(set.has_value()) << component->getName();
    }
    configuration.update();
}

void igniteAllMotors(const SimulationStatus& status, double time)
{
    for (const std::shared_ptr<MotorClusterState>& motor : status.getMotors())
    {
        motor->ignite(time);
    }
}

void burnOutAllMotors(const SimulationStatus& status, double time)
{
    for (const std::shared_ptr<MotorClusterState>& motor : status.getMotors())
    {
        motor->burnOut(time);
    }
}

void deployRecoveryDevices(SimulationStatus& status)
{
    for (const RocketComponent* component : status.getConfiguration().getActiveComponents())
    {
        const auto* device = dynamic_cast<const RecoveryDevice*>(component);
        if (device != nullptr)
        {
            status.getDeployedRecoveryDevices().add(device);
        }
    }
}

namespace
{

/// The options of the scenario @p pin.
void setScenarioOptions(SimulationOptions& o, const StepScenarioPin& pin)
{
    o.setLaunchIntoWind(false);
    o.setLaunchRodLength(pin.input("rodLength"));
    o.setLaunchRodAngle(pin.input("rodAngle"));
    o.setLaunchRodDirection(pin.input("rodDirection"));
    o.setLaunchLatitude(pin.input("latitude"));
    o.setLaunchLongitude(pin.input("longitude"));
    o.setLaunchAltitude(pin.input("altitude"));
    o.setIsaAtmosphere(true);
    o.setGeodeticComputation(
        kAllGeodeticComputationStrategies.at(static_cast<std::size_t>(pin.input("geodetic"))));
    o.setTimeStep(pin.input("timeStep"));
    o.setMaxSimulationTime(600);
    o.setMaximumStepAngle(pin.input("maxAngleStep"));
    o.setRandomSeedFixed(true);
    o.setRandomSeed(static_cast<int>(pin.input("seed")));
    o.setGravityModelType(GravityModelType::WGS);
    o.setWindModelType(WindModelType::AVERAGE);
    o.getAverageWindModel().setAverage(pin.input("windAverage"));
    o.getAverageWindModel().setStandardDeviation(0.0);
    o.getAverageWindModel().setDirection(pin.input("windDirection"));
}

/// The kinematics, the flags and the time of the scenario @p pin, set in the probe's order.
void setScenarioState(SimulationStatus& status, const SimulationConditions& conditions,
                      const StepScenarioPin& pin)
{
    const Coordinate position(pin.input("px"), pin.input("py"), pin.input("pz"));
    status.setRocketOrientationQuaternion(
        Quaternion(pin.input("qw"), pin.input("qx"), pin.input("qy"), pin.input("qz")));
    status.setRocketPosition(position);
    status.setRocketWorldPosition(
        addCoordinate(conditions.getGeodeticComputation(), conditions.getLaunchSite(), position));
    status.setRocketVelocity(Coordinate(pin.input("vx"), pin.input("vy"), pin.input("vz")));
    status.setRocketRotationVelocity(
        Coordinate(pin.input("rvx"), pin.input("rvy"), pin.input("rvz")));
    status.setMotorIgnited(pin.flag("motorIgnited"));
    status.setLiftoff(pin.flag("liftoff"));
    if (pin.flag("rodCleared"))
    {
        status.setSimulationTime(pin.input("rodClearedTime"));
        status.setLaunchRodCleared(true);
    }
    status.setSimulationTime(pin.input("time"));
    status.setApogeeReached(pin.flag("apogee"));
    status.setTumbling(pin.flag("tumbling"));
}

/// The motors, the stages and the recovery devices of the scenario @p pin.
void setScenarioMotorsAndStages(SimulationStatus& status, const StepScenarioPin& pin)
{
    const double ignitionTime = pin.input("ignitionTime");
    if (!std::isnan(ignitionTime))
    {
        igniteAllMotors(status, ignitionTime);
    }
    const double burnoutTime = pin.input("burnoutTime");
    if (!std::isnan(burnoutTime))
    {
        burnOutAllMotors(status, burnoutTime);
    }
    const int onlyStage = static_cast<int>(pin.input("onlyStage"));
    if (onlyStage >= 0)
    {
        status.getConfiguration().setOnlyStage(onlyStage);
    }
    if (pin.flag("deployRecovery"))
    {
        deployRecoveryDevices(status);
    }
}

}  // namespace

ScenarioStatus buildScenarioStatus(
    const StepScenarioPin& pin, const std::vector<std::shared_ptr<SimulationListener>>& listeners)
{
    ScenarioStatus built;
    built.rocket                            = makeScenarioRocket(pin.maker);
    FlightConfiguration& config             = scenarioConfiguration(*built.rocket);
    const double         nozzleExitDiameter = pin.input("nozzleExitDiameter");
    if (nozzleExitDiameter > 0)
    {
        setNozzleExitDiameters(
            config, [nozzleExitDiameter](double /*motorDiameter*/) { return nozzleExitDiameter; });
    }

    built.simulation = std::make_unique<Simulation>(*built.rocket);
    built.simulation->setFlightConfigurationId(config.getId());
    setScenarioOptions(built.simulation->getOptions(), pin);

    Result<SimulationConditions> made = built.simulation->getOptions().toSimulationConditions();
    if (!made.has_value())
    {
        ADD_FAILURE() << pin.name << ": " << made.error().message;
        return built;
    }
    built.conditions = std::make_shared<SimulationConditions>(std::move(*made));
    built.conditions->setSimulation(built.simulation.get());
    built.conditions->getSimulationListenerList() = listeners;

    built.status = std::make_unique<SimulationStatus>(
        std::make_shared<FlightConfiguration>(config.clone()), built.conditions);
    built.branch = std::make_shared<FlightDataBranch>(
        "probe", std::initializer_list<std::reference_wrapper<const FlightDataType>>{
                     FlightDataType::builtin(FlightDataTypeId::TYPE_TIME)});
    built.status->setFlightDataBranch(built.branch);
    setScenarioState(*built.status, *built.conditions, pin);
    setScenarioMotorsAndStages(*built.status, pin);
    return built;
}

InitializedScenario::InitializedScenario(
    std::string_view name, SimulationStepper& stepper,
    const std::vector<std::shared_ptr<SimulationListener>>& listeners)
{
    const StepScenarioPin* pin = findStepScenario(name);
    if (pin == nullptr)
    {
        return;
    }
    built = buildScenarioStatus(*pin, listeners);
    if (built.status != nullptr)
    {
        status = std::make_unique<SimulationStatus>(stepper.initialize(std::move(*built.status)));
    }
}

StepScenarioResult runStepScenario(const StepScenarioPin& pin)
{
    StepScenarioResult result;

    const auto          log = std::make_shared<std::vector<std::string>>();
    const JitterRemoval jitterRemoval;
    std::vector<std::shared_ptr<SimulationListener>> listeners;
    if (pin.listener == "jitter-removal")
    {
        jitterRemoval.install(listeners);
    }
    else if (pin.listener == "record")
    {
        listeners.push_back(std::make_shared<RecordingListener>(log));
    }
    else if (pin.listener != "none")
    {
        listeners.push_back(std::make_shared<OverridingListener>(pin.listener));
    }

    ScenarioStatus                                   built   = buildScenarioStatus(pin, listeners);
    const std::unique_ptr<AbstractSimulationStepper> stepper = makeScenarioStepper(pin.stepper);
    if (built.status == nullptr || stepper == nullptr)
    {
        return result;
    }

    SimulationStatus                      status = stepper->initialize(std::move(*built.status));
    AbstractSimulationStepper::DataStore& store  = scenarioStore(*stepper);
    const double                          previousTimeStep = pin.input("previousTimeStep");
    if (!std::isnan(previousTimeStep))
    {
        store.timeStep = previousTimeStep;
    }

    std::string thrown = "none";
    double      thrust = std::numeric_limits<double>::quiet_NaN();
    try
    {
        thrust = doAction(pin, *stepper, status);
    }
    catch (const SimulationCalculationException& e)
    {
        thrown = std::format("SimulationCalculationException: {}", e.getMessage().value_or("null"));
    }
    catch (const SimulationException& e)
    {
        thrown = std::format("SimulationException: {}", e.getMessage().value_or("null"));
    }
    catch (const BugError& e)
    {
        // Java's BugException, whose message starts with "BUG: " too.
        thrown = std::format("BugException: BUG: {}", bugText([&e] { throw e; }));
        // The sign of a NaN that an invalid operation gave depends on the processor (x86 sets
        // it, ARM does not), and std::format prints it.
        for (std::size_t at = thrown.find("-nan"); at != std::string::npos;
             at             = thrown.find("-nan", at))
        {
            thrown.erase(at, 1);
        }
    }

    Outputs out;
    addStatusOutputs(out, status);
    if (pin.action == "thrust")
    {
        out.add("thrust", thrust);
    }
    addStoreOutputs(out, store);
    result.texts.push_back(addBranchOutputs(out, *built.branch));
    result.outputs = out.take();

    const WarningSet* warnings = status.getWarnings().get();
    for (const FlightEvent& event : built.branch->getEvents())
    {
        result.texts.push_back("branchEvent " + eventText(event, warnings));
    }
    for (const FlightEvent& event : status.getEventQueue())
    {
        result.texts.push_back("queuedEvent " + eventText(event, warnings));
    }
    if (warnings != nullptr)
    {
        for (const Warning& warning : *warnings)
        {
            result.texts.push_back("warning " + warning.toString());
        }
    }
    result.texts.push_back("thrown " + thrown);
    if (pin.listener == "jitter-removal")
    {
        result.texts.push_back(std::format("replacements {}", jitterRemoval.replacements()));
    }
    if (pin.listener == "record")
    {
        std::string hooks = "hooks";
        for (const std::string& hook : *log)
        {
            hooks += ' ';
            hooks += hook;
        }
        result.texts.push_back(std::move(hooks));
    }
    return result;
}

std::string stepScenarioDifferences(
    const StepScenarioPin& pin, const StepScenarioResult& result, double relativeTolerance,
    double                                                         absoluteTolerance,
    std::span<const std::pair<std::string_view, std::string_view>> javaTextReplacements)
{
    std::string differences;
    if (!pin.outputs.empty())
    {
        if (pin.outputLabels.size() != pin.outputs.size())
        {
            differences += "  the pin has as many labels as values\n";
        }
        if (result.outputs.size() != pin.outputs.size())
        {
            differences += std::format("  expected {} outputs, got {}\n", pin.outputs.size(),
                                       result.outputs.size());
        }
        const std::size_t count =
            std::min({pin.outputs.size(), pin.outputLabels.size(), result.outputs.size()});
        for (std::size_t i = 0; i < count; i++)
        {
            const auto& [label, value] = result.outputs[i];
            if (label != pin.outputLabels[i])
            {
                differences += std::format("  output {}: expected the label {}, got {}\n", i,
                                           pin.outputLabels[i], label);
                break;
            }
            if (!matchesPin(pin.outputs[i], value, relativeTolerance, absoluteTolerance))
            {
                differences +=
                    std::format("  {}: expected {}, got {} (relative difference {:.3g})\n", label,
                                pin.outputs[i], value,
                                std::abs(value - pin.outputs[i]) /
                                    std::max(std::abs(value), std::abs(pin.outputs[i])));
            }
        }
    }

    if (result.texts.size() != pin.texts.size())
    {
        differences +=
            std::format("  expected {} texts, got {}\n", pin.texts.size(), result.texts.size());
    }
    for (std::size_t i = 0; i < std::min(pin.texts.size(), result.texts.size()); i++)
    {
        std::string_view expected = pin.texts[i];
        for (const auto& [javaText, textHere] : javaTextReplacements)
        {
            if (expected == javaText)
            {
                expected = textHere;
            }
        }
        if (result.texts[i] != expected)
        {
            differences += std::format("  text {}: expected \"{}\", got \"{}\"\n", i, expected,
                                       result.texts[i]);
        }
    }
    return differences;
}

std::string javaScenarioDifferences(std::string_view name)
{
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 2> kReplacements{{
        {"thrown SimulationCalculationException: error.valuesTooLarge",
         "thrown SimulationCalculationException: Simulation values exceeded limits.  Try "
         "selecting a shorter time step."},
        {"thrown BugException: BUG: addCoordinate resulted in NaN location:  "
         "location=WorldCoordinate[lat=28.610000000000003, lon=-80.6, alt=100.0] "
         "delta=(-Infinity,-Infinity,Infinity) newLat=NaN newLon=NaN",
         "thrown BugException: BUG: addCoordinate resulted in NaN location:  "
         "location=WorldCoordinate[lat=28.610000000000003, lon=-80.6, alt=100] "
         "delta=(-Infinity,-Infinity,Infinity) newLat=nan newLon=nan"},
    }};
    for (const StepScenarioPin& pin : stepScenarioPins())
    {
        if (pin.name == name)
        {
            return stepScenarioDifferences(pin, runStepScenario(pin), kStepRelativeTolerance,
                                           kStepAbsoluteTolerance, kReplacements);
        }
    }
    return std::format("  there is no scenario {}\n", name);
}

std::vector<std::string> stepScenarioNames(bool (*select)(const StepScenarioPin&))
{
    std::vector<std::string> names;
    for (const StepScenarioPin& pin : stepScenarioPins())
    {
        if (select(pin))
        {
            names.emplace_back(pin.name);
        }
    }
    return names;
}

std::string scenarioTestName(std::string_view name)
{
    std::string testName{name};
    for (char& c : testName)
    {
        const bool letterOrDigit =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (!letterOrDigit)
        {
            c = '_';
        }
    }
    return testName;
}

}  // namespace QtRocket::Test
