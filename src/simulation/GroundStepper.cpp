#include "QtRocket/simulation/GroundStepper.h"

#include <memory>

#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

namespace
{

/// The flight data branch of @p status (Java: a NullPointerException without one).
[[nodiscard]] FlightDataBranch& branchOf(const SimulationStatus& status)
{
    const std::shared_ptr<FlightDataBranch>& branch = status.getFlightDataBranch();
    if (branch == nullptr)
    {
        bug("The simulation status has no flight data branch to store the step's data to");
    }
    return *branch;
}

}  // namespace

SimulationStatus GroundStepper::initialize(SimulationStatus status)
{
    return status;
}

void GroundStepper::step(SimulationStatus& status, double timeStep)
{
    const FlightDataType& timeStepType = FlightDataType::builtin(FlightDataTypeId::TYPE_TIME_STEP);

    // Set to values sitting on the ground
    landedValues(status, m_store);

    // Put in a step to immediately go to landed status
    const double time = status.getSimulationTime();
    if (timeStep > 2 * kMinTimeStep)
    {
        // Record timeStep in *current* flightDataBranch record, replacing NaN which was there
        // previously
        branchOf(status).setValue(timeStepType, kMinTimeStep);
        timeStep = timeStep - kMinTimeStep;

        status.setSimulationTime(time + kMinTimeStep);
        status.storeData();
        m_store.storeData(status);
    }

    // Set status to reflect sitting on the ground ever since the last step
    status.setSimulationTime(status.getSimulationTime() + timeStep);
    branchOf(status).setValue(timeStepType, timeStep);

    status.storeData();
    m_store.storeData(status);
}

void GroundStepper::calculateAcceleration(SimulationStatus& /*status*/, DataStore& /*store*/)
{
    // empty
}

}  // namespace QtRocket
