#include "QtRocket/simulation/AbstractEulerStepper.h"

#include <cmath>
#include <limits>
#include <optional>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/simulation/AccelerationData.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/listeners/SimulationListenerHelper.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/WorldCoordinate.h"

namespace QtRocket
{

SimulationStatus AbstractEulerStepper::initialize(SimulationStatus status)
{
    return status;
}

void AbstractEulerStepper::step(SimulationStatus& status, double maxTimeStep)
{
    status.storeData();

    // get flight conditions and calculate acceleration
    calculateFlightConditions(status, m_store);
    FlightConditions& flightConditions = required(m_store.flightConditions, "flight conditions");
    flightConditions.setAOA(std::numeric_limits<double>::quiet_NaN());
    flightConditions.setRollRate(std::numeric_limits<double>::quiet_NaN());
    flightConditions.setPitchRate(std::numeric_limits<double>::quiet_NaN());
    flightConditions.setYawRate(std::numeric_limits<double>::quiet_NaN());

    calculateAcceleration(status, m_store);

    // If the max time step was NaN, this is the final acceleration update
    // upon ground hit.  We need to save the acceleration data, then update
    // the status to reflect the fact we're laying on the ground. This final
    // status won't get written to the FlightDataBranch unless there are
    // events after landing
    if (std::isnan(maxTimeStep))
    {
        m_store.timeStep = std::numeric_limits<double>::quiet_NaN();
        m_store.storeData(status);

        landedValues(status, m_store);
        return;
    }

    // Select tentative time step
    m_store.timeStep = kRecoveryTimeStep;

    // adjust based on acceleration
    const Coordinate linearAcceleration =
        required(m_store.accelerationData, "acceleration data").getLinearAccelerationWC();
    const double absAccel = linearAcceleration.length();
    if (absAccel > MathUtil::kEpsilon)
    {
        m_store.timeStep = MathUtil::javaMin(m_store.timeStep, 1.0 / absAccel);
    }

    // Honor max step size passed in.  If the time to next event is greater than our minimum
    // we'll set our next step to just before it in order to better capture discontinuities in
    // things like chute opening
    if (maxTimeStep < m_store.timeStep)
    {
        if (maxTimeStep > kMinTimeStep)
        {
            m_store.timeStep = maxTimeStep - kMinTimeStep;
        }
        else
        {
            m_store.timeStep = maxTimeStep;
        }
    }

    // but don't let it get *too* small
    m_store.timeStep = MathUtil::javaMax(m_store.timeStep, kMinTimeStep);

    // Perform Euler integration
    EulerValues newVals = eulerIntegrate(status.getRocketPosition(), status.getRocketVelocity(),
                                         linearAcceleration, m_store.timeStep);

    // Check to see if z or either of its first two derivatives have changed sign and
    // recalculate time step to point of change if so
    double t = timeToSignChange(status, linearAcceleration, newVals);

    // once again, make sure new timestep isn't *too* small
    t = MathUtil::javaMax(t, kMinTimeStep);

    // recalculate Euler integration for position and velocity if necessary.
    if (std::abs(t - m_store.timeStep) > MathUtil::kEpsilon)
    {
        m_store.timeStep = t;

        if (maxTimeStep - m_store.timeStep < kMinTimeStep)
        {
            m_store.timeStep = maxTimeStep;
        }

        newVals = eulerIntegrate(status.getRocketPosition(), status.getRocketVelocity(),
                                 linearAcceleration, m_store.timeStep);

        // If we just landed chop off rounding error
        if (std::abs(newVals.pos.z) < MathUtil::kEpsilon)
        {
            newVals.pos = newVals.pos.setZ(0);
        }
    }

    status.setSimulationTime(status.getSimulationTime() + m_store.timeStep);

    status.setRocketPosition(newVals.pos);
    status.setRocketVelocity(newVals.vel);

    // Update the world coordinate
    const SimulationConditions& conditions = conditionsOf(status);
    WorldCoordinate             w          = conditions.getLaunchSite();
    w = addCoordinate(conditions.getGeodeticComputation(), w, status.getRocketPosition());
    status.setRocketWorldPosition(w);

    // Store values calculated for time step
    // (Java sets the time step to NaN here for a NaN maxTimeStep, which has returned above.)
    m_store.storeData(status);
}

double AbstractEulerStepper::timeToSignChange(const SimulationStatus& status,
                                              const Coordinate&       linearAcceleration,
                                              const EulerValues&      newVals)
{
    // z   -- ground hit
    // z'  -- apogee
    // z'' -- possible oscillation building up in descent rate
    // Note that it's virtually impossible for apogee to occur on the same
    // step as either ground hit or descent rate inflection, and if we get a ground hit
    // any descent rate inflection won't matter
    const double a = linearAcceleration.z;
    const double v = status.getRocketVelocity().z;
    const double z = status.getRocketPosition().z;
    double       t = m_store.timeStep;

    if (newVals.pos.z < 0)
    {
        // If I've hit the ground, the new timestep is the solution of
        // 1/2 at^2 + vt + z = 0
        t = (-v - std::sqrt((v * v) - (2 * a * z))) / a;
    }
    else if (v * newVals.vel.z < 0)
    {
        // If I've got apogee, the new timestep is the solution of
        // v + at = 0
        t = std::abs(v / a);
    }
    else
    {
        // Use jerk to estimate accleration at end of time step.  Don't really need to redo all
        // the atmospheric calculations to get it "right"; this will be close enough for our
        // purposes.
        // use chain rule to compute jerk
        // dA/dT = dA/dV * dV/dT
        const double                 cdA = required(m_store.forces, "aerodynamic forces").getCD() *
                                           status.getConfiguration().getReferenceArea();
        const AtmosphericConditions& atmosphericConditions =
            required(m_store.flightConditions, "flight conditions").getAtmosphericConditions();
        const Coordinate airSpeed = status.getRocketVelocity().add(m_store.windVelocity);
        const double     dFdV     = cdA * atmosphericConditions.getDensity() * airSpeed.length();
        Coordinate       dAdV     = Coordinate::kZero;
        if (airSpeed.length() > MathUtil::kEpsilon)
        {
            dAdV = airSpeed.normalize().multiply(
                dFdV / required(m_store.rocketMass, "rocket mass").getMass());
        }
        const Coordinate jerk            = linearAcceleration.multiply(dAdV);
        const Coordinate newAcceleration = linearAcceleration.add(jerk.multiply(m_store.timeStep));

        // If acceleration is appreciably different from 0, and changes sign during the time
        // step, oscillation is building up.
        if (newAcceleration.z * linearAcceleration.z < -MathUtil::kEpsilon)
        {
            // If acceleration oscillation is building up, the new timestep is the solution of
            // a + j*t = 0
            t = std::abs(a / jerk.z);
        }
    }

    return t;
}

void AbstractEulerStepper::calculateAcceleration(SimulationStatus& status, DataStore& store)
{
    store.thrustForce = 0;

    // note some of our forces don't end up getting set, so they're all NaN.
    AerodynamicForces newForces;
    const double      cd = computeCD(status);
    newForces.setCD(cd);
    newForces.setCDaxial(cd);
    newForces.setFrictionCD(0);
    newForces.setPressureCD(cd);
    newForces.setBaseCD(0);

    // Allow listeners to adjust recovery and tumble aerodynamics as they can in RK4/RK6.
    const AerodynamicForces& forces = store.forces.emplace(
        SimulationListenerHelper::firePostAerodynamicCalculation(status, newForces));

    const AtmosphericConditions& atmosphericConditions =
        required(store.flightConditions, "flight conditions").getAtmosphericConditions();

    //// airSpeed
    const Coordinate airSpeed = status.getRocketVelocity().add(store.windVelocity);
    const double     length   = airSpeed.length();

    // Compute drag force
    const double cdA = forces.getCD() * status.getConfiguration().getReferenceArea();

    store.dragForce = 0.5 * cdA * atmosphericConditions.getDensity() * airSpeed.length2();

    const RigidBody structureMassData = calculateStructureMass(status);
    store.motorMass                   = calculateMotorMass(status);
    const RigidBody& rocketMass = store.rocketMass.emplace(structureMassData.add(*store.motorMass));

    if (rocketMass.getMass() < MathUtil::kEpsilon)
    {
        status.abortSimulation(SimulationAbort::Cause::ACTIVE_MASS_ZERO);
    }

    // Compute drag acceleration
    Coordinate linearAcceleration;
    if (length > MathUtil::kEpsilon)
    {
        linearAcceleration = airSpeed.normalize().multiply(-store.dragForce / rocketMass.getMass());
    }

    // Add effect of gravity
    store.gravity      = modelGravity(status);
    linearAcceleration = linearAcceleration.sub(0, 0, store.gravity);

    // Add coriolis acceleration
    const Coordinate coriolisAcceleration =
        getCoriolisAcceleration(conditionsOf(status).getGeodeticComputation(),
                                status.getRocketWorldPosition(), status.getRocketVelocity());
    store.coriolisAcceleration = coriolisAcceleration;
    linearAcceleration         = linearAcceleration.add(coriolisAcceleration);

    store.accelerationData.emplace(std::nullopt, std::nullopt, linearAcceleration, Coordinate::kNul,
                                   status.getRocketOrientationQuaternion());
}

AbstractEulerStepper::EulerValues AbstractEulerStepper::eulerIntegrate(const Coordinate& pos,
                                                                       const Coordinate& v,
                                                                       const Coordinate& a,
                                                                       double            timeStep)
{
    EulerValues result;

    result.vel = v.add(a.multiply(timeStep));
    result.pos = pos.add(v.multiply(timeStep)).add(a.multiply(MathUtil::pow2(timeStep) / 2.0));

    return result;
}

}  // namespace QtRocket
