#include "QtRocket/simulation/listeners/AbstractSimulationListener.h"

#include <limits>
#include <memory>
#include <optional>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/simulation/AccelerationData.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

//// SimulationListener ////

void AbstractSimulationListener::startSimulation(SimulationStatus& /*status*/)
{
    // No-op
}

void AbstractSimulationListener::endSimulation(SimulationStatus& /*status*/,
                                               const SimulationException* /*exception*/)
{
    // No-op
}

void AbstractSimulationListener::startSimulationBranch(SimulationStatus& /*status*/)
{
    // No-op
}

void AbstractSimulationListener::endSimulationBranch(SimulationStatus& /*status*/,
                                                     const SimulationException* /*exception*/)
{
    // No-op
}

bool AbstractSimulationListener::preStep(SimulationStatus& /*status*/)
{
    return true;
}

void AbstractSimulationListener::postStep(SimulationStatus& /*status*/)
{
    // No-op
}

bool AbstractSimulationListener::isSystemListener() const
{
    return false;
}

std::shared_ptr<SimulationListener> AbstractSimulationListener::clone() const
{
    // Java's Object.clone() copies the object's own class; a copy made here would be an
    // AbstractSimulationListener, without the subclass's hooks and state.
    if (typeid(*this) != typeid(AbstractSimulationListener))
    {
        bug("clone() is not overridden by a simulation listener: derive it from "
            "CloneableSimulationListener");
    }
    return std::make_shared<AbstractSimulationListener>(CopyKey{}, *this);
}

//// SimulationEventListener ////

bool AbstractSimulationListener::addFlightEvent(SimulationStatus& /*status*/,
                                                const FlightEvent& /*event*/)
{
    return true;
}

bool AbstractSimulationListener::handleFlightEvent(SimulationStatus& /*status*/,
                                                   const FlightEvent& /*event*/)
{
    return true;
}

bool AbstractSimulationListener::motorIgnition(SimulationStatus& /*status*/,
                                               const MotorConfigurationId& /*motorId*/,
                                               const MotorMount& /*mount*/,
                                               MotorClusterState& /*instance*/)
{
    return true;
}

bool AbstractSimulationListener::recoveryDeviceDeployment(SimulationStatus& /*status*/,
                                                          const RecoveryDevice& /*recoveryDevice*/)
{
    return true;
}

//// SimulationComputationListener ////

std::optional<AccelerationData> AbstractSimulationListener::preAccelerationCalculation(
    SimulationStatus& /*status*/)
{
    return std::nullopt;
}

std::optional<AerodynamicForces> AbstractSimulationListener::preAerodynamicCalculation(
    SimulationStatus& /*status*/)
{
    return std::nullopt;
}

std::optional<AtmosphericConditions> AbstractSimulationListener::preAtmosphericModel(
    SimulationStatus& /*status*/)
{
    return std::nullopt;
}

std::optional<FlightConditions> AbstractSimulationListener::preFlightConditions(
    SimulationStatus& /*status*/)
{
    return std::nullopt;
}

double AbstractSimulationListener::preGravityModel(SimulationStatus& /*status*/)
{
    return std::numeric_limits<double>::quiet_NaN();
}

std::optional<RigidBody> AbstractSimulationListener::preMassCalculation(
    SimulationStatus& /*status*/)
{
    return std::nullopt;
}

double AbstractSimulationListener::preSimpleThrustCalculation(SimulationStatus& /*status*/)
{
    return std::numeric_limits<double>::quiet_NaN();
}

std::optional<Coordinate> AbstractSimulationListener::preWindModel(SimulationStatus& /*status*/)
{
    return std::nullopt;
}

std::optional<AccelerationData> AbstractSimulationListener::postAccelerationCalculation(
    SimulationStatus& /*status*/, const AccelerationData& /*acceleration*/)
{
    return std::nullopt;
}

std::optional<AerodynamicForces> AbstractSimulationListener::postAerodynamicCalculation(
    SimulationStatus& /*status*/, const AerodynamicForces& /*forces*/)
{
    return std::nullopt;
}

std::optional<AtmosphericConditions> AbstractSimulationListener::postAtmosphericModel(
    SimulationStatus& /*status*/, const AtmosphericConditions& /*atmosphericConditions*/)
{
    return std::nullopt;
}

std::optional<FlightConditions> AbstractSimulationListener::postFlightConditions(
    SimulationStatus& /*status*/, const FlightConditions& /*flightConditions*/)
{
    return std::nullopt;
}

double AbstractSimulationListener::postGravityModel(SimulationStatus& /*status*/,
                                                    double /*gravity*/)
{
    return std::numeric_limits<double>::quiet_NaN();
}

std::optional<RigidBody> AbstractSimulationListener::postMassCalculation(
    SimulationStatus& /*status*/, const RigidBody& /*massData*/)
{
    return std::nullopt;
}

double AbstractSimulationListener::postSimpleThrustCalculation(SimulationStatus& /*status*/,
                                                               double /*thrust*/)
{
    return std::numeric_limits<double>::quiet_NaN();
}

std::optional<Coordinate> AbstractSimulationListener::postWindModel(SimulationStatus& /*status*/,
                                                                    const Coordinate& /*wind*/)
{
    return std::nullopt;
}

}  // namespace QtRocket
