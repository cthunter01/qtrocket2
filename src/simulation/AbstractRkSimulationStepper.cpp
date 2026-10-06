#include "QtRocket/simulation/AbstractRkSimulationStepper.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "QtRocket/aero/AerodynamicCalculator.h"
#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/simulation/AccelerationData.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/exception/SimulationCalculationException.h"
#include "QtRocket/simulation/listeners/SimulationListenerHelper.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/JavaRandom.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Quaternion.h"
#include "QtRocket/util/Rotation2D.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/WorldCoordinate.h"

namespace QtRocket
{

SimulationStatus AbstractRkSimulationStepper::initialize(SimulationStatus original)
{
    SimulationStatus status(original);
    // Copy the existing warnings
    status.setWarnings(original.getWarnings());

    const SimulationConditions& sim = conditionsOf(original);

    m_store.launchRodDirection =
        Coordinate(std::sin(sim.getLaunchRodAngle()) *
                       std::cos((std::numbers::pi / 2.0) - sim.getLaunchRodDirection()),
                   std::sin(sim.getLaunchRodAngle()) *
                       std::sin((std::numbers::pi / 2.0) - sim.getLaunchRodDirection()),
                   std::cos(sim.getLaunchRodAngle()));

    // Java: new Random(seed ^ SEED_RANDOMIZATION), an int widened to the long seed.
    const std::int32_t seed = sim.getRandomSeed() ^ kSeedRandomization;
    m_random.emplace(std::int64_t{seed});

    return status;
}

double AbstractRkSimulationStepper::computeTimeStep(const SimulationStatus& status,
                                                    double maxTimeStep, const RkParameters& k1)
{
    const SimulationConditions& conditions = conditionsOf(status);
    const FlightConditions&     flightConditions =
        required(m_store.flightConditions, "flight conditions");
    const AccelerationData& accelerationData =
        required(m_store.accelerationData, "acceleration data");

    /*
     * Select the actual time step to use.  It is the minimum of the following:
     *  dt[0]:  the user-specified time step (or 1/5th of it if still on the launch rod)
     *  dt[1]:  the value of maxTimeStep
     *  dt[2]:  the maximum pitch step angle limit
     *  dt[3]:  the maximum roll step angle limit
     *  dt[4]:  the maximum roll rate change limit
     *  dt[5]:  the maximum pitch change limit
     *  dt[6]:  1/10th of the launch rod length if still on the launch rod
     *  dt[7]:  1.50 times the previous time step
     *
     * The limits #5 and #6 are required since near the steady-state roll rate the roll rate
     * may oscillate significantly even between the sub-steps of the RK4 integration.
     *
     * The step is still at least 1/20th of the user-selected time step.
     */
    std::array<double, 8> dt{};
    dt.fill(std::numeric_limits<double>::max());

    // If the user selected a really small timestep, use MIN_TIME_STEP instead.
    dt[0] = MathUtil::max(conditions.getTimeStep(), kMinTimeStep);
    dt[1] = maxTimeStep;
    dt[2] = conditions.getMaximumAngleStep() / m_store.lateralPitchRate;
    dt[3] = std::abs(kMaxRollStepAngle / flightConditions.getRollRate());
    const Coordinate rotationalAccelerationRC = accelerationData.getRotationalAccelerationRC();
    dt[4] = std::abs(kMaxRollRateChange / rotationalAccelerationRC.z);
    dt[5] = std::abs(kMaxPitchYawChange / MathUtil::max(std::abs(rotationalAccelerationRC.x),
                                                        std::abs(rotationalAccelerationRC.y)));
    if (!status.isLaunchRodCleared())
    {
        dt[0] /= 5.0;
        dt[6] = conditions.getLaunchRodLength() / k1.v.length() / 10;
    }
    dt[7] = 1.5 * m_store.timeStep;

    double timeStep = std::numeric_limits<double>::max();
    for (const double limit : dt)
    {
        // Java's comparison as it is written: a NaN limit does not count.
        // NOLINTNEXTLINE(readability-use-std-min-max): see above
        if (limit < timeStep)
        {
            timeStep = limit;
        }
    }

    // If our selected time step is too close to our next scheduled event,
    // (passed in as maxTimeStep) adjust
    const double minTimeStep = conditions.getTimeStep() / 20;

    if (std::abs(maxTimeStep - timeStep) < minTimeStep)
    {
        timeStep = maxTimeStep;
    }

    // If we've wound up with a too-small timestep, increase it avoid numerical instability
    // even at the cost of not being *quite* on an event
    // NOLINTNEXTLINE(readability-use-std-min-max): Java's comparison as it is written
    if (timeStep < minTimeStep)
    {
        timeStep = minTimeStep;
    }

    return timeStep;
}

AbstractRkSimulationStepper::RkParameters AbstractRkSimulationStepper::computeParameters(
    SimulationStatus& status, DataStore& store)
{
    RkParameters params;

    calculateAcceleration(status, store);

    const AccelerationData& accelerationData =
        required(store.accelerationData, "acceleration data");
    params.a  = accelerationData.getLinearAccelerationWC();
    params.ra = accelerationData.getRotationalAccelerationWC();
    params.v  = status.getRocketVelocity();
    params.rv = status.getRocketRotationVelocity();

    checkNaN(params.a, "params.a");
    checkNaN(params.ra, "params.ra");
    checkNaN(params.v, "params.v");
    checkNaN(params.rv, "params.rv");

    return params;
}

void AbstractRkSimulationStepper::calculateAcceleration(SimulationStatus& status, DataStore& store)
{
    // Call pre-listeners
    store.accelerationData = SimulationListenerHelper::firePreAccelerationCalculation(status);

    // Calculate acceleration (if not overridden by pre-listeners)
    if (!store.accelerationData.has_value())
    {
        store.accelerationData = computeAcceleration(status, store);
    }

    // Call post-listeners
    store.accelerationData =
        SimulationListenerHelper::firePostAccelerationCalculation(status, *store.accelerationData);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): Java's instance method
double AbstractRkSimulationStepper::calculateThrust(SimulationStatus& status, DataStore& store)
{
    store.thrustCorrection = 0;

    // Pre-listeners
    double thrust = SimulationListenerHelper::firePreThrustCalculation(status);
    if (!std::isnan(thrust))
    {
        return thrust;
    }

    thrust = 0;
    const std::vector<std::shared_ptr<MotorClusterState>> activeMotorList =
        status.getActiveMotors();
    for (const std::shared_ptr<MotorClusterState>& currentMotorState : activeMotorList)
    {
        thrust += currentMotorState->getThrust(status.getSimulationTime());
    }

    /*
     * Standard thrust curves provide motor thrust at standard pressure (101.325 kPa).
     * According to the rocket thrust equation, total thrust is given by F0 + (P0 - Pa)A where
     * F0 is the thrust at standard pressure, P0 is the standard pressure, Pa is the current
     * atmospheric pressure, and A is the area of the motor nozzle. This altitude correction is
     * applied here.
     */
    if (thrust > 0)
    {
        const FlightConditions& flightConditions =
            required(store.flightConditions, "flight conditions");
        const double area = flightConditions.getThrustingNozzleExitArea();
        if (area > 0)
        {
            // Correct motor thrust for air pressure
            store.thrustCorrection =
                area * (AtmosphericConditions::kStandardPressure -
                        flightConditions.getAtmosphericConditions().getPressure());
            thrust += store.thrustCorrection;
        }
    }

    // Post-listeners
    thrust = SimulationListenerHelper::firePostThrustCalculation(status, thrust);

    checkNaN(thrust, "thrust");

    return thrust;
}

AccelerationData AbstractRkSimulationStepper::computeAcceleration(SimulationStatus& status,
                                                                  DataStore&        store)
{
    // Calculate mass data
    const RigidBody structureMassData = calculateStructureMass(status);

    store.motorMass             = calculateMotorMass(status);
    const RigidBody& rocketMass = store.rocketMass.emplace(structureMassData.add(*store.motorMass));

    if (rocketMass.getMass() < MathUtil::kEpsilon)
    {
        status.abortSimulation(SimulationAbort::Cause::ACTIVE_MASS_ZERO);
    }

    // Compute the forces affecting the rocket
    calculateForces(status, store);
    const AerodynamicForces& forces = required(store.forces, "aerodynamic forces");
    const FlightConditions&  flightConditions =
        required(store.flightConditions, "flight conditions");
    const Rotation2D& thetaRotation = required(store.thetaRotation, "theta rotation");

    // Calculate the forces from the aerodynamic coefficients

    const double dynP      = (0.5 * flightConditions.getAtmosphericConditions().getDensity() *
                              MathUtil::pow2(flightConditions.getVelocity()));
    const double refArea   = flightConditions.getRefArea();
    const double refLength = flightConditions.getRefLength();

    // Linear forces in rocket coordinates
    store.dragForce    = forces.getCDaxial() * dynP * refArea;
    const double fN    = forces.getCN() * dynP * refArea;
    const double fSide = forces.getCside() * dynP * refArea;

    store.thrustForce   = calculateThrust(status, store);
    const double forceZ = store.thrustForce - store.dragForce;

    Coordinate linearAcceleration(-fN / rocketMass.getMass(), -fSide / rocketMass.getMass(),
                                  forceZ / rocketMass.getMass());

    linearAcceleration = thetaRotation.rotateZ(linearAcceleration);

    // Convert into rocket world coordinates
    linearAcceleration = status.getRocketOrientationQuaternion().rotate(linearAcceleration);

    // add effect of gravity
    store.gravity      = modelGravity(status);
    linearAcceleration = linearAcceleration.sub(0, 0, store.gravity);

    // add effect of Coriolis acceleration
    const Coordinate coriolisAcceleration =
        getCoriolisAcceleration(conditionsOf(status).getGeodeticComputation(),
                                status.getRocketWorldPosition(), status.getRocketVelocity());
    store.coriolisAcceleration = coriolisAcceleration;
    linearAcceleration         = linearAcceleration.add(coriolisAcceleration);

    Coordinate angularAcceleration;

    // If we haven't taken off yet, don't sink into the ground
    if (!status.isLiftoff())
    {
        if (linearAcceleration.z < 0)
        {
            linearAcceleration = Coordinate();
        }
    }
    else if (!status.isLaunchRodCleared())
    {
        // If still on the launch rod, project acceleration onto launch rod direction and
        // set angular acceleration to zero.

        const Coordinate& rodDirection = required(store.launchRodDirection, "launch rod direction");
        const double      projection   = linearAcceleration.dot(rodDirection);
        linearAcceleration = Coordinate(rodDirection.x * projection, rodDirection.y * projection,
                                        rodDirection.z * projection, 0.0);
    }
    else
    {
        // Shift moments to CG
        const double cm = forces.getCm() - (forces.getCN() * rocketMass.getCM().x / refLength);
        const double cyaw =
            forces.getCyaw() - (forces.getCside() * rocketMass.getCM().x / refLength);

        // Compute moments
        const double momX = -cyaw * dynP * refArea * refLength;
        const double momY = cm * dynP * refArea * refLength;
        const double momZ = forces.getCroll() * dynP * refArea * refLength;

        // Compute angular acceleration in rocket coordinates
        angularAcceleration = Coordinate(momX / rocketMass.getLongitudinalInertia(),
                                         momY / rocketMass.getLongitudinalInertia(),
                                         momZ / rocketMass.getRotationalInertia());

        angularAcceleration = thetaRotation.rotateZ(angularAcceleration);

        // Convert to world coordinates
        angularAcceleration = status.getRocketOrientationQuaternion().rotate(angularAcceleration);
    }

    return {std::nullopt, std::nullopt, linearAcceleration, angularAcceleration,
            status.getRocketOrientationQuaternion()};
}

void AbstractRkSimulationStepper::calculateForces(SimulationStatus& status, DataStore& store)
{
    // Call pre-listeners
    store.forces = SimulationListenerHelper::firePreAerodynamicCalculation(status);
    if (store.forces.has_value())
    {
        return;
    }

    // Compute flight conditions
    calculateFlightConditions(status, store);
    const FlightConditions& flightConditions =
        required(store.flightConditions, "flight conditions");

    /*
     * Check whether to store warnings or not.  Warnings are ignored when on the
     * launch rod or 0.25 seconds after departure, and when the velocity has dropped
     * below 20% of the max. velocity.
     */
    std::optional<WarningSet> warnings;
    if (status.recordWarnings())
    {
        warnings.emplace();
    }

    // Calculate aerodynamic forces
    const std::shared_ptr<AerodynamicCalculator>& calculator =
        conditionsOf(status).getAerodynamicCalculator();
    if (calculator == nullptr)
    {
        bug("The simulation conditions have no aerodynamic calculator");
    }
    AerodynamicForces forces = calculator->getAerodynamicForces(
        status.getConfiguration(), flightConditions, warnings.has_value() ? &*warnings : nullptr);
    if (warnings.has_value())
    {
        status.addWarnings(*warnings);
    }

    // Add very small randomization to yaw & pitch moments to prevent over-perfect flight
    if (!m_random.has_value())
    {
        bug("The stepper calculates forces before initialize() seeded its random source");
    }
    forces.setCm(forces.getCm() + (kPitchYawRandom * 2 * (m_random->nextDouble() - 0.5)));
    forces.setCyaw(forces.getCyaw() + (kPitchYawRandom * 2 * (m_random->nextDouble() - 0.5)));

    // Call post-listeners
    store.forces = SimulationListenerHelper::firePostAerodynamicCalculation(status, forces);
}

std::optional<AbstractRkSimulationStepper::RkParameters> AbstractRkSimulationStepper::startStep(
    SimulationStatus& status, double maxTimeStep)
{
    status.storeData();

    /*
     * Get the current atmospheric conditions
     */
    calculateFlightConditions(status, m_store);

    /*
     * Perform the integration.  Decide the time step length after the first step.
     */

    //// First position, k1 = f(t, y)

    const RkParameters k1 = computeParameters(status, m_store);

    // If maxTimeStep is NaN we'll just record sim params and leave
    if (std::isnan(maxTimeStep))
    {
        m_store.timeStep = maxTimeStep;
        m_store.storeData(status);

        landedValues(status, m_store);
        return std::nullopt;
    }

    m_store.timeStep = computeTimeStep(status, maxTimeStep, k1);

    m_store.storeData(status);
    checkNaN(m_store.timeStep, "store.timeStep");

    return k1;
}

AbstractRkSimulationStepper::RkParameters AbstractRkSimulationStepper::computeStage(
    const SimulationStatus& status, double time, std::span<const StageTerm> terms)
{
    SimulationStatus status2 = status.clone();
    status2.setSimulationTime(time);

    Coordinate position = status.getRocketPosition();
    for (const StageTerm& term : terms)
    {
        position = position.addScaled(term.k->v, term.scale);
    }
    status2.setRocketPosition(position);

    Coordinate velocity = status.getRocketVelocity();
    for (const StageTerm& term : terms)
    {
        velocity = velocity.addScaled(term.k->a, term.scale);
    }
    status2.setRocketVelocity(velocity);

    // One setter call per term, as in Java (each draws a modification id).
    for (const StageTerm& term : terms)
    {
        status2.setRocketOrientationQuaternion(
            status2.getRocketOrientationQuaternion().multiplyLeft(
                Quaternion::rotation(term.k->rv.multiply(term.scale))));
    }

    Coordinate rotationVelocity = status.getRocketRotationVelocity();
    for (const StageTerm& term : terms)
    {
        rotationVelocity = rotationVelocity.addScaled(term.k->ra, term.scale);
    }
    status2.setRocketRotationVelocity(rotationVelocity);

    return computeParameters(status2, m_store);
}

void AbstractRkSimulationStepper::finishStep(SimulationStatus& status, const Coordinate& deltaV,
                                             const Coordinate& deltaP, const Coordinate& deltaR,
                                             const Coordinate& deltaO) const
{
    status.setRocketVelocity(status.getRocketVelocity().add(deltaV));
    status.setRocketPosition(status.getRocketPosition().add(deltaP));
    status.setRocketRotationVelocity(status.getRocketRotationVelocity().add(deltaR));
    status.setRocketOrientationQuaternion(status.getRocketOrientationQuaternion()
                                              .multiplyLeft(Quaternion::rotation(deltaO))
                                              .normalizeIfNecessary());

    const SimulationConditions& conditions = conditionsOf(status);
    WorldCoordinate             w          = conditions.getLaunchSite();
    w = addCoordinate(conditions.getGeodeticComputation(), w, status.getRocketPosition());
    status.setRocketWorldPosition(w);

    if (!(0 <= m_store.timeStep))
    {
        // Also catches NaN
        bug(std::format("Stepping backwards in time, timestep={}",
                        Strings::javaDoubleToString(m_store.timeStep)));
    }
    status.setSimulationTime(status.getSimulationTime() + m_store.timeStep);

    // Verify that values don't run out of range
    if (status.getRocketVelocity().length2() > 1.0e18 ||
        status.getRocketPosition().length2() > 1.0e18 ||
        status.getRocketRotationVelocity().length2() > 1.0e18)
    {
        throw SimulationCalculationException(std::string{kValuesTooLarge},
                                             status.getFlightDataBranch());
    }
}

}  // namespace QtRocket
