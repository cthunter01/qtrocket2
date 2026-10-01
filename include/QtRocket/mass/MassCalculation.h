#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "QtRocket/mass/CMAnalysisEntry.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket
{

class FlightConfiguration;
class MotorClusterState;
class MotorConfiguration;
class RocketComponent;

/// One mass calculation over a flight configuration (OpenRocket's masscalc/MassCalculation): it
/// accumulates the centre of mass of a component subtree (rocket frame, weight = mass) and the
/// rigid bodies whose inertias calculateMomentOfInertia() moves to that centre. MassCalculator
/// runs it from the rocket; the calculation copies itself (copy()) for every subtree and motor
/// cluster and merges the copies back.
///
/// Passes (calculateAssembly()):
/// - Structure (calculateStructure()): a recursive walk from the root. Every instance i of a
///   component places its children with parentTransform * translation(instance location i) *
///   axialRotation(instance angle i); an active component (FlightConfiguration::
///   isComponentActive()) adds its own CG (getComponentCG() + getPosition() through the parent's
///   transformation; a component's own instances are part of its CG) and a body with its unit
///   inertias times its mass. The walk descends into inactive components too, as Java does, and
///   only the active ones add mass.
/// - Overrides, exactly as Java: a mass override replaces the component's mass (for a component
///   that is not massive, such as an assembly, its CG is the CG of its children); with
///   "override subcomponents" it replaces the subtree's mass, the children's bodies are scaled by
///   override / (component mass + children's mass) (0 when that is not above kMinMass), so the
///   inertia follows the mass, and the children's centre of mass keeps its position with weight
///   0; without it, the component's own inertia uses the override mass. A CG override sets x to
///   the component's front (rocket frame) plus the override x; with "override subcomponents" the
///   children's centre of mass moves to that x as well.
/// - Motors (calculateMotors()): only the active motor mounts are visited, through the
///   configuration's active instances: for each instance context of a mount with instance number
///   0 (one per parent instance), one cluster of all the mount's instances is added at the
///   parent instance's transformation: mass per motor times the instance count, x = mount x +
///   motor x (MotorConfiguration::getX()) + the motor's CG x, on the axis; rotational inertia
///   unit Ixx times the mass, plus m d^2 for each instance offset when there is more than one
///   instance; longitudinal inertia unit Iyy times the mass. The motor time is the calculation's
///   time, or, with motor states (a simulation), each state's getMotorTime(). The mass and CG
///   come from Motor::getTotalMass()/getCMx() at the motor time (LAUNCH, MOTOR), at
///   Motor::kPseudoTimeBurnout (BURNOUT), or, for a type without the casings (only STRUCTURE,
///   whose calculateAssembly() skips the motor pass, so only through a direct
///   calculateMotors()), the propellant alone: the total minus the burnout mass and CG.
///
/// CM analysis: with an analysis map (MassCalculator::getCMAnalysis()), every active component
/// gets a row (CMAnalysisEntry): a physical component its own CG and the mass of its first
/// instance; an assembly the CG of its subtree including the motors of its mounts below it, and
/// that mass per instance of the assembly; a motor (by designation) the mass per motor and the
/// average CG of its clusters.
///
/// The configuration, the motor states, the components and the analysis map are referred to,
/// not owned: they must outlive the calculation, and the configuration must be up to date. The
/// constructor therefore rejects a temporary configuration, root or container of motor states
/// (Java keeps a strong reference to them); MassCalculator's entry points, whose calculation ends
/// with the call, take temporaries.
///
/// Deviations from OpenRocket:
/// - Java's fields are package-private and MassCalculator reads them; here they have getters.
/// - getLongitudinalInertia() and getRotationalInertia() read Java's `inertia` field, which only
///   reset() ever sets (to RigidBody.EMPTY), so they return 0 as in Java; the inertia is
///   calculateMomentOfInertia()'s result.
/// - The debug `prefix` field (only read by commented-out prints) and the unused Type(double)
///   constructor are not ported.
/// - The motor states are a span of pointers; nullopt is Java's null list (a static
///   calculation, which takes the configuration's active motors). A null pointer in it is a
///   BugError (Java: NullPointerException).
/// - Java's hashCode() is hashCode() and the std::hash specialisation below.
class MassCalculation
{
public:
    /// What a calculation includes (Java: Type, with the public flags includesStructure,
    /// includesMotorCasing and includesPropellant, see the functions below).
    enum class Type
    {
        STRUCTURE,  ///< structure only: no motor data, even with active motors
        MOTOR,      ///< motor casings and propellant, no structure
        BURNOUT,    ///< structure and motor casings
        LAUNCH,     ///< structure, motor casings and propellant
    };

    /// Type.values(), in declaration order.
    static constexpr std::array<Type, 4> kAllTypes{Type::STRUCTURE, Type::MOTOR, Type::BURNOUT,
                                                   Type::LAUNCH};

    /// Below this mass a centre of mass counts as empty (Java's MIN_MASS = MathUtil.EPSILON).
    static constexpr double kMinMass = MathUtil::kEpsilon;

    /// Whether @p type includes the structure: STRUCTURE, BURNOUT and LAUNCH.
    [[nodiscard]] static constexpr bool includesStructure(Type type) noexcept
    {
        return type != Type::MOTOR;
    }

    /// Whether @p type includes the motor casings: MOTOR, BURNOUT and LAUNCH.
    [[nodiscard]] static constexpr bool includesMotorCasing(Type type) noexcept
    {
        return type != Type::STRUCTURE;
    }

    /// Whether @p type includes the propellant: MOTOR and LAUNCH.
    [[nodiscard]] static constexpr bool includesPropellant(Type type) noexcept
    {
        return type == Type::MOTOR || type == Type::LAUNCH;
    }

    /// A calculation of @p type for @p config at @p time (simulation time, or a motor
    /// pseudo-time such as Motor::kPseudoTimeLaunch) over the subtree of @p root placed by
    /// @p transform. @p activeMotors are the motor states of a simulation, or nullopt for a
    /// static calculation (the configuration's active motors at @p time); @p analysisMap
    /// collects CM analysis rows when not null. Starts empty (reset()).
    MassCalculation(Type type, const FlightConfiguration& config, double time,
                    std::optional<std::span<const MotorClusterState* const>> activeMotors,
                    const RocketComponent& root, const Transformation& transform,
                    CMAnalysisMap* analysisMap);

    /// Rejected: the calculation would keep a reference to the temporary configuration.
    MassCalculation(Type type, const FlightConfiguration&& config, double time,
                    std::optional<std::span<const MotorClusterState* const>> activeMotors,
                    const RocketComponent& root, const Transformation& transform,
                    CMAnalysisMap* analysisMap) = delete;

    /// Rejected: the calculation would keep a reference to the temporary root.
    MassCalculation(Type type, const FlightConfiguration& config, double time,
                    std::optional<std::span<const MotorClusterState* const>> activeMotors,
                    const RocketComponent&& root, const Transformation& transform,
                    CMAnalysisMap* analysisMap) = delete;

    /// Rejected: the calculation would keep a span into the temporary container of motor states
    /// (such as a std::vector returned by value). A span or an lvalue container is taken.
    template <typename Motors>
        requires(!std::is_lvalue_reference_v<Motors> && std::ranges::range<Motors> &&
                 !std::ranges::borrowed_range<Motors>)
    MassCalculation(Type type, const FlightConfiguration& config, double time,
                    Motors&& activeMotors, const RocketComponent& root,
                    const Transformation& transform, CMAnalysisMap* analysisMap) = delete;

    // ------------------------------------------------------------------- accumulation

    /// Adds @p other's centre of mass (addMass()) and bodies to this calculation. Merging a
    /// calculation into itself doubles its mass and its bodies, as Java's does.
    void merge(const MassCalculation& other);

    /// Adds a body for calculateMomentOfInertia().
    void addInertia(const RigidBody& data);

    /// Scales the mass and inertias of every body by the non-negative @p factor (a mass
    /// override that covers the subcomponents keeps their distribution and rescales it).
    void scaleInertia(double factor);

    /// Adds a point mass: when this calculation is still empty (mass below kMinMass) the centre
    /// of mass becomes @p pointMass, else the mass-weighted average of the two.
    void addMass(const Coordinate& pointMass);

    /// Adds @p mass to the mass without moving the centre of mass.
    void addMass(double mass);

    /// A new, empty calculation with this one's type, configuration, time, motor states and
    /// analysis map, over the subtree of @p root placed by @p transform.
    [[nodiscard]] MassCalculation copy(const RocketComponent& root,
                                       const Transformation&  transform) const;

    /// Rejected: the copy would keep a reference to the temporary root.
    [[nodiscard]] MassCalculation copy(const RocketComponent&& root,
                                       const Transformation&   transform) const = delete;

    // ---------------------------------------------------------------------- results

    /// The centre of mass, weight = mass.
    [[nodiscard]] const Coordinate& getCM() const noexcept { return m_centerOfMass; }
    void setCM(const Coordinate& newCM) noexcept { m_centerOfMass = newCM; }

    /// The mass, the centre of mass's weight.
    [[nodiscard]] double getMass() const noexcept { return m_centerOfMass.weight; }
    void setMass(double mass) noexcept { m_centerOfMass = m_centerOfMass.setWeight(mass); }

    /// Java's `inertia.Iyy`: always 0 (see the class comment).
    [[nodiscard]] double getLongitudinalInertia() const noexcept { return m_inertia.getIyy(); }

    /// Java's `inertia.Ixx`: always 0 (see the class comment).
    [[nodiscard]] double getRotationalInertia() const noexcept { return m_inertia.getIxx(); }

    /// The bodies collected so far.
    [[nodiscard]] const std::vector<RigidBody>& getBodies() const noexcept { return m_bodies; }

    /// The number of bodies collected so far.
    [[nodiscard]] std::size_t size() const noexcept { return m_bodies.size(); }

    /// Back to empty: no mass at the origin, no bodies.
    void reset() noexcept;

    // ------------------------------------------------------------------ parameters

    [[nodiscard]] Type                       getType() const noexcept { return m_type; }
    [[nodiscard]] const FlightConfiguration& getConfig() const noexcept { return *m_config; }
    [[nodiscard]] double getSimulationTime() const noexcept { return m_simulationTime; }
    [[nodiscard]] const RocketComponent& getRoot() const noexcept { return *m_root; }
    [[nodiscard]] const Transformation&  getTransform() const noexcept { return m_transform; }
    /// The motor states, or nullopt for a static calculation.
    [[nodiscard]] const std::optional<std::span<const MotorClusterState* const>>& getActiveMotors()
        const noexcept
    {
        return m_activeMotors;
    }
    /// The analysis map, or null.
    [[nodiscard]] CMAnalysisMap* getAnalysisMap() const noexcept { return m_analysisMap; }

    // ------------------------------------------------------------------ calculation

    /// Runs the structure pass (when the type includes the structure) and the motor pass (when
    /// it includes the casings or the propellant) on copies, and merges them into this one.
    /// @return this calculation
    MassCalculation& calculateAssembly();

    /// The structure pass over the root's subtree (see the class comment).
    /// @return this calculation
    MassCalculation& calculateStructure();

    /// The motor pass: the clusters of every active motor (the motor states', or the
    /// configuration's active motors for a static calculation).
    /// @return this calculation
    MassCalculation& calculateMotors();

    /// The rigid body of the whole calculation: the centre of mass, and the sum of every body's
    /// inertias after rebase() to it (the parallel axis theorem); Izz = Iyy. The second step of
    /// the two-step inertia calculation (the centre of mass first, while gathering).
    [[nodiscard]] RigidBody calculateMomentOfInertia() const;

    // ---------------------------------------------------------------------- object

    /// Java's equals(): equal centres of mass (Coordinate::operator==), the same configuration
    /// id, the same time (==) and the same type.
    [[nodiscard]] bool operator==(const MassCalculation& other) const noexcept;

    /// Java's hashCode(): the centre of mass's, (int)((x + y + z) * 100000) (the partner of
    /// operator==, which compares the centres of mass with a tolerance, so equal calculations
    /// can still hash differently across a bucket edge, as in Java).
    [[nodiscard]] std::int32_t hashCode() const noexcept;

    /// "cm= <mass>g@[<x>,<y>,<z>]" with six decimals each (Java's "%.6f" in an English locale).
    [[nodiscard]] std::string toCMDebug() const;

    /// toCMDebug().
    [[nodiscard]] std::string toString() const { return toCMDebug(); }

private:
    /// Adds the cluster of @p motorConfig's mount (this calculation's root) for one parent
    /// instance; @p motorState is null for a static calculation.
    MassCalculation& calculateMountData(const MotorConfiguration& motorConfig,
                                        const MotorClusterState*  motorState);

    /// The active @p component's own part of the structure pass (Java: the body of
    /// calculateStructure()'s isComponentActive() branch): its CG and mass placed by
    /// @p parentTransform, with its mass and CG overrides, its analysis row (a physical
    /// component), and its body. An override that covers the subcomponents rewrites @p children,
    /// the calculation of its subtree.
    void addComponentData(const RocketComponent& component, const Transformation& parentTransform,
                          MassCalculation& children);

    /// Adds every parent-instanced cluster of @p motorConfig.
    void calculateMotorInstances(const MotorConfiguration& motorConfig,
                                 const MotorClusterState*  motorState);

    /// Adds @p motorCM (one parent instance's cluster) to the analysis row of every active
    /// assembly above @p mount (the motor pass does not walk the assemblies).
    void updateAssemblyMotorMass(const RocketComponent& mount, const Coordinate& motorCM);

    /// The analysis row of @p component, which calculateStructure() has made.
    /// @throws BugError when there is none.
    [[nodiscard]] CMAnalysisEntry& analysisEntry(const RocketComponent& component) const;

    const FlightConfiguration*                               m_config;
    double                                                   m_simulationTime;
    std::optional<std::span<const MotorClusterState* const>> m_activeMotors;
    const RocketComponent*                                   m_root;
    Transformation                                           m_transform;
    Type                                                     m_type;

    /// The centre of mass only.
    Coordinate m_centerOfMass;
    /// Java's `inertia`: only reset() sets it.
    RigidBody m_inertia{RigidBody::kEmpty};
    /// The bodies (centre of mass and inertias) for calculateMomentOfInertia().
    std::vector<RigidBody> m_bodies;

    CMAnalysisMap* m_analysisMap;
};

/// The name of a calculation type, Java's enum constant name: "STRUCTURE", "MOTOR", "BURNOUT" or
/// "LAUNCH".
[[nodiscard]] std::string_view name(MassCalculation::Type type) noexcept;

}  // namespace QtRocket

/// Java's hashCode() (MassCalculation::hashCode()).
template <>
struct std::hash<QtRocket::MassCalculation>
{
    [[nodiscard]] std::size_t operator()(
        const QtRocket::MassCalculation& calculation) const noexcept
    {
        return static_cast<std::size_t>(calculation.hashCode());
    }
};
