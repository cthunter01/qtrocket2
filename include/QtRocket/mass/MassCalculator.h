#pragma once

#include <span>

#include "QtRocket/mass/CMAnalysisEntry.h"
#include "QtRocket/mass/MassCalculation.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"

namespace QtRocket
{

class FlightConfiguration;
class MotorClusterState;

/// The mass properties of a flight configuration (OpenRocket's masscalc/MassCalculator): static
/// entry points that run a MassCalculation from the rocket (identity transformation) and return
/// the rigid body of the result (centre of mass in the rocket frame, weight = mass; inertias
/// about the centre of mass), and the per-component CM analysis.
///
/// The configuration must be up to date (its active instances and motors), as in Java. Nothing is
/// cached: OpenRocket's MassCalculator keeps no cache either (its cache fields are commented out,
/// and its ModID is the constant ZERO), so every call reflects the rocket as it is; the
/// simulation caches the structure mass itself, by the configuration's modification id.
///
/// The in-flight calculation (Java: calculate(Type, SimulationStatus) and
/// calculateMotor(SimulationStatus)) takes what it read from the status: the configuration, the
/// simulation time and the active motor states (SimulationStatus.getActiveMotors()) as pointers,
/// which a std::vector of const or non-const MotorClusterState pointers converts to. The
/// simulation's SimulationStatus wrappers call these entry points, which accept temporaries (such
/// as getActiveMotors() returned by value), since their calculation ends with the call; a
/// MassCalculation built directly keeps references and rejects temporaries.
class MassCalculator
{
public:
    /// Below this mass a centre of mass counts as empty (Java: MIN_MASS = MathUtil.EPSILON).
    static constexpr double kMinMass = MathUtil::kEpsilon;

    /// The structure: no motors, no propellant (STRUCTURE at Motor::kPseudoTimeEmpty).
    [[nodiscard]] static RigidBody calculateStructure(const FlightConfiguration& config);

    /// The rocket at burnout: the structure and the motor casings (BURNOUT at
    /// Motor::kPseudoTimeBurnout), so for solid motors without the propellant.
    [[nodiscard]] static RigidBody calculateBurnout(const FlightConfiguration& config);

    /// The motors at launch, casings and propellant, without the structure (MOTOR at
    /// Motor::kPseudoTimeLaunch).
    [[nodiscard]] static RigidBody calculateMotor(const FlightConfiguration& config);

    /// The rocket at launch: structure, motor casings and propellant (LAUNCH at
    /// Motor::kPseudoTimeLaunch).
    [[nodiscard]] static RigidBody calculateLaunch(const FlightConfiguration& config);

    /// The motors in flight (Java: calculateMotor(SimulationStatus)): MOTOR at
    /// @p simulationTime, each motor at its own time since ignition.
    [[nodiscard]] static RigidBody calculateMotor(
        const FlightConfiguration& config, double simulationTime,
        std::span<const MotorClusterState* const> activeMotors);

    /// A calculation of @p type in flight (Java: calculate(Type, SimulationStatus)): the motors
    /// are @p activeMotors, each at its own time since ignition at @p simulationTime.
    [[nodiscard]] static RigidBody calculate(
        MassCalculation::Type type, const FlightConfiguration& config, double simulationTime,
        std::span<const MotorClusterState* const> activeMotors);

    /// A static calculation of @p type with the configuration's active motors at @p time (a
    /// motor pseudo-time).
    [[nodiscard]] static RigidBody calculate(MassCalculation::Type      type,
                                             const FlightConfiguration& config, double time);

    /// The CG of every active component of @p config, of its motors and of the whole rocket at
    /// launch (LAUNCH at Motor::kPseudoTimeLaunch, with the configuration's active motors), keyed
    /// by CMAnalysisEntry::keyOf(). A physical component's row holds its own mass (one instance)
    /// and the CG of all its instances; an assembly's row its subtree with the motors and
    /// propellant below it; a motor's row (by designation) the mass of one motor and the CG of
    /// all of them; the rocket's row, replaced at the end, the totals of the whole calculation.
    ///
    /// Java documents this as deprecated in spirit: one row per component (or per designation)
    /// merges the instances of a component, and motors with the same designation share a row,
    /// so the analysis is misleading for complex rockets.
    [[nodiscard]] static CMAnalysisMap getCMAnalysis(const FlightConfiguration& config);

    /// ModId::zero(): the calculator has no state (Java's getModID()).
    [[nodiscard]] static constexpr ModId getModId() noexcept { return ModId::zero(); }
    /// getModId() (the Monitorable concept).
    [[nodiscard]] static constexpr ModId modId() noexcept { return ModId::zero(); }
};

}  // namespace QtRocket
