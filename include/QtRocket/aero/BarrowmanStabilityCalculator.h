#pragma once

#include <memory>
#include <numbers>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/StabilityCalculator.h"
#include "QtRocket/aero/StabilityForceBreakdown.h"
#include "QtRocket/aero/barrowman/ComponentCalcMap.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

class FlightConditions;
class FlightConfiguration;
class ForceMap;
class InstanceMap;
class RocketComponent;
class WarningSet;

/// The stability half of the extended Barrowman method (OpenRocket's
/// aerodynamics/BarrowmanStabilityCalculator): the normal and side forces, the pitch, yaw and
/// roll moments and the CP of a rocket, summed over the calculations of its components
/// (aero/barrowman/), the pitch and yaw damping moments, and the geometry checks.
///
/// Non-axial forces: for every instance of every active component with a calculation, the
/// calculation's local forces; the CP is taken to the rocket's frame with the instance's
/// transformation and put on the axis (y = z = 0), the pitching moment becomes
/// CN * CP.x / reference length, and the instances are merged (AerodynamicForces::merge(): the
/// CNa-weighted CP, the sums of the coefficients).
///
/// Force analysis: the same per component, gathered by a walk of the component tree from the
/// rocket. The component map gets the forces of every aerodynamic component and of every
/// assembly (an assembly has none of its own) in pre-order; the assembly map gets, for every
/// component visited, the forces of its whole subtree, children first, so that the rocket's entry
/// comes last and is the total. An inactive stage is left out with its components, but its
/// top-level child stages that are active (boosters that fly without their core) are visited.
///
/// Damping moments: the multiplier 0.275 * d / (Aref * Lref) * (cg^4 + (L - cg)^4), with L the
/// summed length and d the summed planform area over L of the active symmetric components, plus
/// for each active fin set 0.6 * min(fin count, 4) * planform area * |x - cg|^3 / (Aref * Lref),
/// x being the absolute position of the fin set's midchord (FinSetCalc::getMidchordPos()); times
/// 3, times (rate / velocity)^2, capped at the total moment coefficient (MathUtil::min, which
/// keeps the damping value when the total is NaN), with the sign of the rate. L and d are cached
/// at the first call, until voidAerodynamicCache().
///
/// The cache: a ComponentCalcMap built at the first calculation, and the damping L and d. As in
/// Java this calculator does not look at the rocket's modification ids itself; the
/// BarrowmanCalculator that owns it calls voidAerodynamicCache() when they change, and a caller
/// that uses the calculator on its own must do the same after changing the rocket.
///
/// Summation order: OpenRocket's InstanceMap is a ConcurrentHashMap keyed by the components, so
/// Java sums the components in an order that follows their random ids; here the active
/// instances are in tree order (see InstanceMap). Sums over the components can therefore differ
/// from Java's, and from one Java run to the next, in the last bits.
///
/// Kept from OpenRocket, on purpose:
/// - The damping L and d are those of the configuration of the first call: a change of the
///   active stages alone (a stage separation) does not void them.
/// - checkGeometry() compares diameters and positions as the default length unit prints them
///   (UnitGroup's process-wide default unit), so what counts as a discontinuity or a gap depends
///   on the user's unit, as in OpenRocket's GUI.
///
/// Deviations from OpenRocket:
/// - Java's null WarningSet is resolved by the caller (see StabilityCalculator), so the
///   calculator has no ignoreWarningSet of its own.
/// - A warning names its components by snapshots of their ids and names (see MessageSource).
/// - BugError where Java throws a NullPointerException: in the force analysis for an aerodynamic
///   component or an assembly without a calculation (one that joined the rocket after the
///   calculations were made, the cache not having been voided since; the total forces go without
///   it, as in Java) or without an active instance (a configuration that is out of date); in the
///   damping moments for a fin set without a calculation; in checkGeometry() for an open forward
///   end when no stage is active (only reachable by checking a component of an inactive stage on
///   its own), and for a symmetric component without a parent.
/// - A component without an absolute location, which no component tree has, throws BugError
///   (Java: an array index error).
class BarrowmanStabilityCalculator final : public StabilityCalculator
{
public:
    /// The stall angle in radians: 17.5 degrees (STALL_ANGLE).
    static constexpr double kStallAngle = 17.5 * std::numbers::pi / 180;

    BarrowmanStabilityCalculator() = default;

    /// A new calculator with an empty cache.
    [[nodiscard]] std::unique_ptr<StabilityCalculator> newInstance() const override;

    /// kStallAngle.
    [[nodiscard]] double getStallAngle() const override { return kStallAngle; }

    /// calculateNonAxialForces(...).getCP().
    [[nodiscard]] Coordinate getCP(const FlightConfiguration& configuration,
                                   const FlightConditions&    conditions,
                                   WarningSet&                warnings) override;

    /// The non-axial forces of the rocket: zeroed forces merged with those of every active
    /// component that has a calculation, in the order of the active instances. No component is
    /// associated with the result, and its drag coefficients are untouched (NaN).
    [[nodiscard]] AerodynamicForces calculateNonAxialForces(
        const FlightConfiguration& configuration, const FlightConditions& conditions,
        WarningSet& warnings) override;

    /// The non-axial forces per component and per subtree (see the class comment).
    /// @throws BugError see the class comment.
    [[nodiscard]] StabilityForceBreakdown getForceAnalysis(const FlightConfiguration& configuration,
                                                           const FlightConditions&    conditions,
                                                           WarningSet& warnings) override;

    /// Sets the pitch and yaw damping moments of @p total (see the class comment), about the x
    /// of the pitch centre of @p conditions.
    /// @throws BugError see the class comment.
    void calculateDampingMoments(const FlightConfiguration& configuration,
                                 const FlightConditions&    conditions,
                                 AerodynamicForces&         total) override;

    /// Walks the symmetric components under @p component in line, front to back, and adds a
    /// warning for each problem between one and the one before it:
    /// - Warning::kOpenAirframeForward, for the first one when its fore radius less its wall
    ///   thickness is above MathUtil::kEpsilon, if stage 0 is active or the bottom active stage
    ///   has a recovery device;
    /// - Warning::kDiameterDiscontinuity (both components), when the fore diameter and the
    ///   previous aft diameter print differently in the default length unit;
    /// - Warning::kZeroVolumeBody, for a length below MathUtil::kEpsilon or both radii below it;
    /// - when the front of the component and the end of the previous one print differently:
    ///   Warning::kAirframeGap (both) when the component starts behind the previous end;
    ///   otherwise Warning::kAirframeOverlap (both) when it starts at or behind the previous
    ///   front and either ends at or behind the previous end or is the last one in line;
    ///   otherwise, for the component's parent, Warning::kPodsetForward when the last component
    ///   in line ends at or ahead of the front of the first one, else Warning::kPodsetOverlap;
    /// - when they print alike: Warning::kPodsetOverlap for the component's parent when that is
    ///   a pod set or a booster set, the positions are equal within MathUtil::equals() and the
    ///   parent's own parent is the sibling that follows the previous component.
    /// The children of @p component, of each symmetric component and of each axial stage are
    /// taken in order, breadth first; an inactive stage is replaced by its active top-level child
    /// stages; a pod set or a booster set is checked on its own, starting from the component
    /// ahead of its first child when that child is symmetric (any assembly but the rocket
    /// starts so).
    /// @throws BugError see the class comment.
    void checkGeometry(const FlightConfiguration& configuration, const RocketComponent& component,
                       WarningSet& warnings) override;

    /// Drops the calculations (with their pointers to the rocket's components, see
    /// ComponentCalcMap) and the cached damping length and diameter.
    void voidAerodynamicCache() override;

private:
    /// The forces of the subtree of @p comp, entered into @p eachForces and @p assemblyForces
    /// (see the class comment).
    AerodynamicForces calculateForceAnalysis(const FlightConfiguration& configuration,
                                             const FlightConditions&    conds,
                                             const RocketComponent&     comp,
                                             const InstanceMap& instances, ForceMap& eachForces,
                                             ForceMap& assemblyForces, WarningSet& warnings);

    /// The damping multiplier about @p cgx (before the factor 3).
    [[nodiscard]] double getDampingMultiplier(const FlightConfiguration& configuration,
                                              const FlightConditions& conditions, double cgx);

    ComponentCalcMap m_calcMap;
    /// The planform area over the length, and the length, of the active symmetric components;
    /// -1 until the first getDampingMultiplier() (Java: cacheDiameter, cacheLength).
    double m_cacheDiameter{-1};
    double m_cacheLength{-1};
};

}  // namespace QtRocket
