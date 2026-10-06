#include "QtRocket/simulation/AbstractSimulationStepper.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/aero/AerodynamicCalculator.h"
#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/mass/MassCalculator.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/models/AtmosphericModel.h"
#include "QtRocket/models/GravityModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/AccelerationData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/listeners/SimulationListenerHelper.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Quaternion.h"
#include "QtRocket/util/Rotation2D.h"

namespace QtRocket
{

namespace
{

/// The builtin flight data type @p id.
[[nodiscard]] const FlightDataType& type(FlightDataTypeId id)
{
    return FlightDataType::builtin(id);
}

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

/// The NaN message of checkNaN(), up to where the three overloads differ.
[[nodiscard]] std::string nanMessage(std::string_view var)
{
    return std::format(
        "Simulation resulted in not-a-number (NaN) value for {}, please report a bug", var);
}

/// The thetaRotation and the lateral pitch rate of flight conditions a listener gave (Java:
/// new Rotation2D(theta) and Math.hypot(pitchRate, yawRate)).
void setRotationFromConditions(AbstractSimulationStepper::DataStore& store,
                               const FlightConditions&               conditions)
{
    store.thetaRotation = Rotation2D(conditions.getTheta());
    // Java's Math.hypot (not MathUtil.hypot, which the model's own branch uses).
    store.lateralPitchRate = std::hypot(conditions.getPitchRate(), conditions.getYawRate());
}

}  // namespace

// ================================================================================ DataStore

void AbstractSimulationStepper::DataStore::storeData(SimulationStatus& status) const
{
    FlightDataBranch& dataBranch = branchOf(status);

    dataBranch.setValue(type(FlightDataTypeId::TYPE_THRUST_FORCE), thrustForce);
    dataBranch.setValue(type(FlightDataTypeId::TYPE_THRUST_CORRECTION), thrustCorrection);
    dataBranch.setValue(type(FlightDataTypeId::TYPE_GRAVITY), gravity);
    dataBranch.setValue(type(FlightDataTypeId::TYPE_DRAG_FORCE), dragForce);

    dataBranch.setValue(type(FlightDataTypeId::TYPE_WIND_VELOCITY), windVelocity.length());
    dataBranch.setValue(type(FlightDataTypeId::TYPE_WIND_DIRECTION),
                        getWindDirection(windVelocity));
    dataBranch.setValue(type(FlightDataTypeId::TYPE_TIME_STEP), timeStep);

    if (coriolisAcceleration.has_value())
    {
        dataBranch.setValue(type(FlightDataTypeId::TYPE_CORIOLIS_ACCELERATION),
                            coriolisAcceleration->length());
    }

    if (accelerationData.has_value())
    {
        const Coordinate wc = accelerationData->getLinearAccelerationWC();
        dataBranch.setValue(type(FlightDataTypeId::TYPE_ACCELERATION_XY),
                            MathUtil::hypot(wc.x, wc.y));

        dataBranch.setValue(type(FlightDataTypeId::TYPE_ACCELERATION_TOTAL), wc.length());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_ACCELERATION_Z), wc.z);
        dataBranch.setValue(type(FlightDataTypeId::TYPE_ACCELERATION_X), wc.x);
        dataBranch.setValue(type(FlightDataTypeId::TYPE_ACCELERATION_Y), wc.y);

        const Coordinate rc = accelerationData->getLinearAccelerationRC();
        dataBranch.setValue(type(FlightDataTypeId::TYPE_ACCELERATION_BODYZ), rc.z);
        dataBranch.setValue(type(FlightDataTypeId::TYPE_ACCELERATION_BODYX), rc.x);
        dataBranch.setValue(type(FlightDataTypeId::TYPE_ACCELERATION_BODYY), rc.y);
    }

    if (rocketMass.has_value())
    {
        const double weight = rocketMass->getMass() * gravity;
        dataBranch.setValue(type(FlightDataTypeId::TYPE_THRUST_WEIGHT_RATIO), thrustForce / weight);
        dataBranch.setValue(type(FlightDataTypeId::TYPE_CG_LOCATION), rocketMass->getCM().x);
        dataBranch.setValue(type(FlightDataTypeId::TYPE_MASS), rocketMass->getMass());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_LONGITUDINAL_INERTIA),
                            rocketMass->getLongitudinalInertia());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_ROTATIONAL_INERTIA),
                            rocketMass->getRotationalInertia());
    }

    if (motorMass.has_value())
    {
        dataBranch.setValue(type(FlightDataTypeId::TYPE_MOTOR_MASS), motorMass->getMass());
    }

    if (flightConditions.has_value())
    {
        const AtmosphericConditions& atmosphere = flightConditions->getAtmosphericConditions();
        const double                 re =
            (flightConditions->getVelocity() * status.getConfiguration().getLengthAerodynamic() /
             atmosphere.getKinematicViscosity());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_REYNOLDS_NUMBER), re);
        dataBranch.setValue(type(FlightDataTypeId::TYPE_MACH_NUMBER), flightConditions->getMach());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_REFERENCE_LENGTH),
                            flightConditions->getRefLength());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_REFERENCE_AREA),
                            flightConditions->getRefArea());

        dataBranch.setValue(type(FlightDataTypeId::TYPE_PITCH_RATE),
                            flightConditions->getPitchRate());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_YAW_RATE), flightConditions->getYawRate());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_ROLL_RATE),
                            flightConditions->getRollRate());

        dataBranch.setValue(type(FlightDataTypeId::TYPE_AOA), flightConditions->getAOA());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_AIR_TEMPERATURE),
                            atmosphere.getTemperature());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_AIR_PRESSURE), atmosphere.getPressure());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_AIR_DENSITY), atmosphere.getDensity());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_SPEED_OF_SOUND), atmosphere.getMachSpeed());
    }

    const DampingMomentComponents dampingMoment =
        computeDampingMomentCoefficientComponents(status, dataBranch);
    dataBranch.setValue(type(FlightDataTypeId::TYPE_DAMPING_MOMENT_COEFF), dampingMoment.total);
    dataBranch.setValue(type(FlightDataTypeId::TYPE_DAMPING_MOMENT_COEFF_AERODYNAMIC),
                        dampingMoment.aerodynamic);
    dataBranch.setValue(type(FlightDataTypeId::TYPE_DAMPING_MOMENT_COEFF_PROPULSIVE),
                        dampingMoment.propulsive);

    const double correctiveMomentCoefficient = computeCorrectiveMomentCoefficient(status);
    dataBranch.setValue(type(FlightDataTypeId::TYPE_CORRECTIVE_MOMENT_COEFF),
                        correctiveMomentCoefficient);

    dataBranch.setValue(
        type(FlightDataTypeId::TYPE_DAMPING_RATIO),
        computeDampingRatio(status, dampingMoment.total, correctiveMomentCoefficient));
    dataBranch.setValue(type(FlightDataTypeId::TYPE_NATURAL_FREQUENCY),
                        computeNaturalFrequency(status, correctiveMomentCoefficient));

    if (forces.has_value())
    {
        dataBranch.setValue(type(FlightDataTypeId::TYPE_DRAG_COEFF), forces->getCD());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_AXIAL_DRAG_COEFF), forces->getCDaxial());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_FRICTION_DRAG_COEFF),
                            forces->getFrictionCD());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_PRESSURE_DRAG_COEFF),
                            forces->getPressureCD());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_BASE_DRAG_COEFF), forces->getBaseCD());
    }

    if (status.isLaunchRodCleared() && forces.has_value())
    {
        // Java tests forces.getCP() for null here and below, which it never is.
        const Coordinate cp = forces->getCP();
        dataBranch.setValue(type(FlightDataTypeId::TYPE_CP_LOCATION), cp.x);
        dataBranch.setValue(type(FlightDataTypeId::TYPE_CNA), cp.weight);
        dataBranch.setValue(type(FlightDataTypeId::TYPE_NORMAL_FORCE_COEFF), forces->getCN());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_SIDE_FORCE_COEFF), forces->getCside());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_ROLL_MOMENT_COEFF), forces->getCroll());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_ROLL_FORCING_COEFF),
                            forces->getCrollForce());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_ROLL_DAMPING_COEFF),
                            forces->getCrollDamp());
        dataBranch.setValue(type(FlightDataTypeId::TYPE_PITCH_DAMPING_MOMENT_COEFF),
                            forces->getPitchDampingMoment());

        if (rocketMass.has_value() && flightConditions.has_value())
        {
            dataBranch.setValue(type(FlightDataTypeId::TYPE_STABILITY),
                                (cp.x - rocketMass->getCM().x) / flightConditions->getRefLength());
            dataBranch.setValue(type(FlightDataTypeId::TYPE_PITCH_MOMENT_COEFF),
                                forces->getCm() - (forces->getCN() * rocketMass->getCM().x /
                                                   flightConditions->getRefLength()));
            dataBranch.setValue(type(FlightDataTypeId::TYPE_YAW_MOMENT_COEFF),
                                forces->getCyaw() - (forces->getCside() * rocketMass->getCM().x /
                                                     flightConditions->getRefLength()));
        }
    }
}

AbstractSimulationStepper::DataStore::DampingMomentComponents
AbstractSimulationStepper::DataStore::computeDampingMomentCoefficientComponents(
    const SimulationStatus& status, const FlightDataBranch& dataBranch) const
{
    constexpr double kNotANumber = std::numeric_limits<double>::quiet_NaN();
    if (!flightConditions.has_value() || !rocketMass.has_value())
    {
        return {.total = kNotANumber, .aerodynamic = kNotANumber, .propulsive = kNotANumber};
    }

    if (!status.isLaunchRodCleared())
    {
        return {.total = 0.0, .aerodynamic = 0.0, .propulsive = 0.0};
    }

    // Keep ground/landed values consistent with other aero-derived quantities that are not
    // computed. (When the rocket is on the ground, FlightConditions.AOA is set to NaN by
    // landedValues().)
    if (std::isnan(flightConditions->getAOA()))
    {
        return {.total = kNotANumber, .aerodynamic = kNotANumber, .propulsive = kNotANumber};
    }

    const double aerodynamic = computeAerodynamicDampingMomentCoefficient(status);
    const double propulsive  = computePropulsiveDampingMomentCoefficient(status, dataBranch);
    return {
        .total = aerodynamic + propulsive, .aerodynamic = aerodynamic, .propulsive = propulsive};
}

double AbstractSimulationStepper::DataStore::computeAerodynamicDampingMomentCoefficient(
    const SimulationStatus& status) const
{
    if (!flightConditions.has_value() || !rocketMass.has_value())
    {
        bug("The aerodynamic damping moment needs flight conditions and a rocket mass");
    }
    double                                        aerodynamicPart = 0;
    const double                                  cg              = rocketMass->getCM().x;
    const std::shared_ptr<AerodynamicCalculator>& aerocalc =
        conditionsOf(status).getAerodynamicCalculator();
    if (aerocalc == nullptr)
    {
        bug("The simulation conditions have no aerodynamic calculator");
    }
    const ForceMap forceAnalysis =
        aerocalc->getForceAnalysis(status.getConfiguration(), *flightConditions, nullptr);
    for (const ForceMap::Entry& entry : forceAnalysis)
    {
        const RocketComponent* comp = entry.first;
        if (comp == nullptr || !comp->isAerodynamic())
        {
            continue;
        }
        // Java also skips null forces and a null CP, which do not exist here.
        const Coordinate componentCp = entry.second.getCP();

        const double cna = componentCp.weight;
        const double z   = componentCp.x;  // Distance from rocket tip to component CP
        aerodynamicPart += cna * MathUtil::pow2(z - cg);
    }

    const double v   = flightConditions->getVelocity();
    const double rho = flightConditions->getAtmosphericConditions().getDensity();
    const double ar  = flightConditions->getRefArea();
    aerodynamicPart  = 0.5 * rho * v * ar * aerodynamicPart;

    return aerodynamicPart;
}

double AbstractSimulationStepper::DataStore::computePropulsiveDampingMomentCoefficient(
    const SimulationStatus& status, const FlightDataBranch& dataBranch) const
{
    const double motorMassDerivative = computeMotorMassDerivative(dataBranch);
    if (std::isnan(motorMassDerivative))
    {
        return 0;
    }

    // The damping equation uses the positive mass-expulsion rate, while the remaining motor
    // mass decreases during the burn. Therefore, massExpulsionRate = -d(motor mass)/dt.
    const double massExpulsionRate = -motorMassDerivative;

    if (!rocketMass.has_value())
    {
        bug("The propulsive damping moment needs a rocket mass");
    }
    const double cg = rocketMass->getCM().x;

    // Find the furthest-aft nozzle location in the current configuration, measured from the
    // rocket tip.
    double                     nozzleDistance = 0;
    const FlightConfiguration& config         = status.getConfiguration();
    for (const MotorConfiguration& inst : config.getActiveMotors())
    {
        const std::shared_ptr<const Motor>& motor = inst.getMotor();
        if (motor == nullptr)
        {
            bug("An active motor configuration has no motor");
        }
        const double x = inst.getX() + motor->getLaunchCGx();
        // Java: if (x > nozzleDistance) nozzleDistance = x;
        nozzleDistance = std::max(nozzleDistance, x);
    }

    return massExpulsionRate * MathUtil::pow2(nozzleDistance - cg);
}

double AbstractSimulationStepper::DataStore::computeMotorMassDerivative(
    const FlightDataBranch& dataBranch)
{
    const std::vector<double>* motorMassView =
        dataBranch.getView(type(FlightDataTypeId::TYPE_MOTOR_MASS));
    const std::vector<double>* timeView = dataBranch.getView(type(FlightDataTypeId::TYPE_TIME));
    if (motorMassView == nullptr || timeView == nullptr)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Check that we have enough samples
    const std::size_t n = std::min(motorMassView->size(), timeView->size());
    if (n < 2)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double dt = timeView->at(n - 1) - timeView->at(n - 2);
    if (!(dt > 0))
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Note: peak of flight mentions gram/s, but we use kg/s
    return (motorMassView->at(n - 1) - motorMassView->at(n - 2)) / dt;
}

double AbstractSimulationStepper::DataStore::computeCorrectiveMomentCoefficient(
    const SimulationStatus& status) const
{
    // Java also tests forces.getCP() for null, which it never is.
    if (!flightConditions.has_value() || !rocketMass.has_value() || !forces.has_value())
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    if (!status.isLaunchRodCleared())
    {
        return 0.0;
    }

    if (std::isnan(flightConditions->getAOA()))
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double rho = flightConditions->getAtmosphericConditions().getDensity();
    const double v   = flightConditions->getVelocity();
    const double ar  = flightConditions->getRefArea();

    const Coordinate forcesCp = forces->getCP();
    const double     cna      = forcesCp.weight;
    const double     cp       = forcesCp.x;
    const double     cg       = rocketMass->getCM().x;

    return 0.5 * rho * MathUtil::pow2(v) * ar * cna * (cp - cg);
}

double AbstractSimulationStepper::DataStore::computeDampingRatio(
    const SimulationStatus& status, double dampingMomentCoefficient,
    double correctiveMomentCoefficient) const
{
    if (!rocketMass.has_value())
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    if (!status.isLaunchRodCleared())
    {
        return 0.0;
    }

    if (std::isnan(dampingMomentCoefficient) || std::isnan(correctiveMomentCoefficient))
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double longitudinalInertia = rocketMass->getLongitudinalInertia();
    if (longitudinalInertia <= 0)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    // For a stable rocket Ccm > 0 (CP behind CG).  If the product is non-positive, zeta is
    // undefined.
    const double product = correctiveMomentCoefficient * longitudinalInertia;
    if (std::isnan(product) || product <= 0)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double denominator = 2.0 * std::sqrt(product);
    if (std::isnan(denominator) || denominator <= 0)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return dampingMomentCoefficient / denominator;
}

double AbstractSimulationStepper::DataStore::computeNaturalFrequency(
    const SimulationStatus& status, double correctiveMomentCoefficient) const
{
    if (!status.isLaunchRodCleared())
    {
        return 0.0;
    }

    if (!rocketMass.has_value() || std::isnan(correctiveMomentCoefficient))
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double longitudinalInertia = rocketMass->getLongitudinalInertia();
    if (longitudinalInertia <= 0)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double ratio = correctiveMomentCoefficient / longitudinalInertia;
    if (std::isnan(ratio) || ratio < 0)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return std::sqrt(ratio);
}

double AbstractSimulationStepper::DataStore::getWindDirection(const Coordinate& windVector)
{
    // Math.atan2(y, x) returns the angle in radians measured counterclockwise from the positive
    // x-axis. But we want the angle clockwise from North (positive y-axis)
    const double angle = std::atan2(windVector.x, windVector.y);
    return MathUtil::reduce2Pi(angle);
}

// =================================================================== AbstractSimulationStepper

const SimulationConditions& AbstractSimulationStepper::conditionsOf(const SimulationStatus& status)
{
    const std::shared_ptr<SimulationConditions>& conditions = status.getSimulationConditions();
    if (conditions == nullptr)
    {
        bug("The simulation status has no simulation conditions");
    }
    return *conditions;
}

void AbstractSimulationStepper::calculateFlightConditions(SimulationStatus& status,
                                                          DataStore&        store)
{
    // Call pre listeners, allow complete override
    store.flightConditions = SimulationListenerHelper::firePreFlightConditions(status);
    if (store.flightConditions.has_value())
    {
        setThrustingNozzleExitAreas(status, *store.flightConditions);
        // Compute the store values
        setRotationFromConditions(store, *store.flightConditions);
        return;
    }

    //// Atmospheric conditions
    const AtmosphericConditions atmosphere = modelAtmosphericConditions(status);
    FlightConditions& conditions = store.flightConditions.emplace(status.getConfiguration());
    conditions.setAtmosphericConditions(atmosphere);

    //// Local wind speed and direction
    store.windVelocity  = modelWindVelocity(status);
    Coordinate airSpeed = status.getRocketVelocity().add(store.windVelocity);
    airSpeed            = status.getRocketOrientationQuaternion().invRotate(airSpeed);

    // Lateral direction:
    const double len = MathUtil::hypot(airSpeed.x, airSpeed.y);
    if (len > 0.0001)
    {
        store.thetaRotation = Rotation2D(airSpeed.y / len, airSpeed.x / len);
        conditions.setTheta(std::atan2(airSpeed.y, airSpeed.x));
    }
    else
    {
        store.thetaRotation = Rotation2D::identity();
        conditions.setTheta(0);
    }

    const double velocity = airSpeed.length();
    conditions.setVelocity(velocity);
    if (velocity > 0.01)
    {
        // aoa must be calculated from the monotonous cosine
        // sine can be calculated by a simple division
        conditions.setAOA(std::acos(airSpeed.z / velocity), len / velocity);
    }
    else
    {
        conditions.setAOA(0);
    }

    // Roll, pitch and yaw rate
    const Coordinate rot =
        status.getRocketOrientationQuaternion().invRotate(status.getRocketRotationVelocity());

    conditions.setRollRate(rot.z);
    if (len < 0.001)
    {
        conditions.setPitchRate(0);
        conditions.setYawRate(0);
        store.lateralPitchRate = 0;
    }
    else
    {
        conditions.setPitchRate(rot.y);
        conditions.setYawRate(rot.x);
        store.lateralPitchRate = MathUtil::hypot(rot.x, rot.y);
    }

    // Make the instantaneous propulsion state available to post-flight-condition
    // listeners and, subsequently, to the aerodynamic drag calculator.
    setThrustingNozzleExitAreas(status, conditions);

    // Call post listeners
    bool replaced = false;
    conditions =
        SimulationListenerHelper::firePostFlightConditions(status, std::move(conditions), replaced);
    if (replaced)
    {
        // Listeners changed the values, recalculate data store
        setRotationFromConditions(store, conditions);
    }
    setThrustingNozzleExitAreas(status, conditions);
}

void AbstractSimulationStepper::setThrustingNozzleExitAreas(const SimulationStatus& status,
                                                            FlightConditions&       conditions)
{
    std::vector<FlightConditions::NozzleExitArea> areasByAssembly;
    for (const std::shared_ptr<MotorClusterState>& motorState : status.getActiveMotors())
    {
        const MotorClusterState& state              = *motorState;
        const double             nozzleExitDiameter = state.getNozzleExitDiameter();
        if (!state.isThrusting() || nozzleExitDiameter <= 0)
        {
            continue;
        }

        const double nozzleExitRadius = nozzleExitDiameter / 2;
        const double area =
            state.getMotorCount() * std::numbers::pi * MathUtil::pow2(nozzleExitRadius);
        const ComponentAssembly& assembly = asComponent(state.getMount()).getAssembly();
        // Java: areasByAssembly.merge(assembly, area, Double::sum), keyed by equals().
        const auto existing = std::ranges::find_if(
            areasByAssembly, [&assembly](const FlightConditions::NozzleExitArea& entry) {
                return entry.first->equals(assembly);
            });
        if (existing == areasByAssembly.end())
        {
            areasByAssembly.emplace_back(&assembly, area);
        }
        else
        {
            existing->second = existing->second + area;
        }
    }
    conditions.setThrustingNozzleExitAreas(areasByAssembly);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): Java's instance method
AtmosphericConditions AbstractSimulationStepper::modelAtmosphericConditions(
    SimulationStatus& status)
{
    // Call pre-listener
    std::optional<AtmosphericConditions> override =
        SimulationListenerHelper::firePreAtmosphericModel(status);
    if (override.has_value())
    {
        return *override;
    }

    // Compute conditions
    const SimulationConditions& simulationConditions = conditionsOf(status);
    const double                altitude =
        status.getRocketPosition().z + simulationConditions.getLaunchSite().getAltitude();
    const std::shared_ptr<const AtmosphericModel>& model =
        simulationConditions.getAtmosphericModel();
    if (model == nullptr)
    {
        bug("The simulation conditions have no atmospheric model");
    }
    AtmosphericConditions conditions = model->getConditions(altitude);

    // Call post-listener
    conditions = SimulationListenerHelper::firePostAtmosphericModel(status, conditions);

    checkNaN(conditions.getPressure(), "conditions.getPressure()");
    checkNaN(conditions.getTemperature(), "conditions.getTemperature()");

    return conditions;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): Java's instance method
Coordinate AbstractSimulationStepper::modelWindVelocity(SimulationStatus& status)
{
    // Call pre-listener
    const std::optional<Coordinate> override = SimulationListenerHelper::firePreWindModel(status);
    if (override.has_value())
    {
        return *override;
    }

    // Compute conditions
    const SimulationConditions& simulationConditions = conditionsOf(status);
    const double                altitudeAGL          = status.getRocketPosition().z;
    const double altitudeMSL = altitudeAGL + simulationConditions.getLaunchSite().getAltitude();
    const std::shared_ptr<WindModel>& model = simulationConditions.getWindModel();
    if (model == nullptr)
    {
        bug("The simulation conditions have no wind model");
    }
    Coordinate wind = model->getWindVelocity(status.getSimulationTime(), altitudeMSL, altitudeAGL);

    // Call post-listener
    wind = SimulationListenerHelper::firePostWindModel(status, wind);

    checkNaN(wind, "wind");

    return wind;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): Java's instance method
double AbstractSimulationStepper::modelGravity(SimulationStatus& status)
{
    // Call pre-listener
    double gravity = SimulationListenerHelper::firePreGravityModel(status);
    if (!std::isnan(gravity))
    {
        return gravity;
    }

    // Compute conditions
    const std::shared_ptr<const GravityModel>& model = conditionsOf(status).getGravityModel();
    if (model == nullptr)
    {
        bug("The simulation conditions have no gravity model");
    }
    gravity = model->getGravity(status.getRocketWorldPosition());

    // Call post-listener
    gravity = SimulationListenerHelper::firePostGravityModel(status, gravity);

    checkNaN(gravity, "gravity");

    return gravity;
}

RigidBody AbstractSimulationStepper::calculateStructureMass(SimulationStatus& status)
{
    // Call pre-listener — if it overrides, skip the cache entirely
    const std::optional<RigidBody> override =
        SimulationListenerHelper::firePreMassCalculation(status);
    if (override.has_value())
    {
        return *override;
    }

    // Structural mass is constant between stage-separation events. Recompute only
    // when the FlightConfiguration's ModID changes (stage deactivated, etc.).
    const FlightConfiguration& configuration = status.getConfiguration();
    const ModId                currentModId  = configuration.getModId();
    if (!m_cachedStructureMass.has_value() || currentModId != m_cachedStructureConfigModId)
    {
        m_cachedStructureMass        = MassCalculator::calculateStructure(configuration);
        m_cachedStructureConfigModId = currentModId;
    }

    // Call post-listener
    const RigidBody structureMass =
        SimulationListenerHelper::firePostMassCalculation(status, *m_cachedStructureMass);

    checkNaN(structureMass.getCenterOfMass(), "structureMass.getCenterOfMass()");
    checkNaN(structureMass.getLongitudinalInertia(), "structureMass.getLongitudinalInertia()");
    checkNaN(structureMass.getRotationalInertia(), "structureMass.getRotationalInertia()");

    return structureMass;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): Java's instance method
RigidBody AbstractSimulationStepper::calculateMotorMass(SimulationStatus& status)
{
    // Call pre-listener
    const std::optional<RigidBody> override =
        SimulationListenerHelper::firePreMassCalculation(status);
    if (override.has_value())
    {
        return *override;
    }

    RigidBody motorMass = MassCalculator::calculateMotor(
        status.getConfiguration(), status.getSimulationTime(), status.getActiveMotorStates());

    // Call post-listener
    motorMass = SimulationListenerHelper::firePostMassCalculation(status, motorMass);

    checkNaN(motorMass.getCenterOfMass(), "motorMass.getCenterOfMass()");
    checkNaN(motorMass.getLongitudinalInertia(), "motorMass.getLongitudinalInertia()");
    checkNaN(motorMass.getRotationalInertia(), "motorMass.getRotationalInertia()");

    return motorMass;
}

void AbstractSimulationStepper::checkNaN(double d, std::string_view var)
{
    if (std::isnan(d))
    {
        bug(nanMessage(var) + ".");
    }
}

void AbstractSimulationStepper::checkNaN(const Coordinate& c, std::string_view var)
{
    if (c.isNaN())
    {
        bug(std::format("{}, c={}", nanMessage(var), c.toString()));
    }
}

void AbstractSimulationStepper::checkNaN(const Quaternion& q, std::string_view var)
{
    if (q.isNaN())
    {
        bug(std::format("{}, q={}", nanMessage(var), q.toString()));
    }
}

void AbstractSimulationStepper::landedValues(SimulationStatus& status, DataStore& store)
{
    store.timeStep = std::numeric_limits<double>::quiet_NaN();

    // get flight conditions
    calculateFlightConditions(status, store);
    FlightConditions& flightConditions = required(store.flightConditions, "flight conditions");
    flightConditions.setAOA(std::numeric_limits<double>::quiet_NaN());
    flightConditions.setRollRate(0);
    flightConditions.setPitchRate(0);
    flightConditions.setYawRate(0);

    // note most of our forces don't end up getting set, so they're all NaN.
    AerodynamicForces forces;
    forces.setCD(std::numeric_limits<double>::quiet_NaN());
    forces.setCDaxial(std::numeric_limits<double>::quiet_NaN());
    forces.setFrictionCD(std::numeric_limits<double>::quiet_NaN());
    forces.setPressureCD(std::numeric_limits<double>::quiet_NaN());
    forces.setBaseCD(std::numeric_limits<double>::quiet_NaN());
    store.forces = forces;

    const RigidBody structureMassData = calculateStructureMass(status);
    store.motorMass                   = calculateMotorMass(status);
    store.rocketMass                  = structureMassData.add(*store.motorMass);
    store.gravity                     = modelGravity(status);
    store.thrustForce                 = 0.0;
    store.thrustCorrection            = 0.0;
    store.dragForce                   = 0.0;
    store.coriolisAcceleration        = Coordinate::kZero;

    store.accelerationData.emplace(Coordinate::kZero, Coordinate::kZero, std::nullopt, std::nullopt,
                                   Quaternion());

    status.setRocketPosition(
        Coordinate(status.getRocketPosition().x, status.getRocketPosition().y, 0));
    status.setRocketVelocity(Coordinate::kZero);
}

}  // namespace QtRocket
