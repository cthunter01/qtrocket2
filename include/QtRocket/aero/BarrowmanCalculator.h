#pragma once

#include <memory>

#include "QtRocket/aero/AbstractAerodynamicCalculator.h"
#include "QtRocket/aero/AerodynamicCalculator.h"
#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/DragCalculator.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/aero/StabilityCalculator.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"

namespace QtRocket
{

class FlightConditions;
class FlightConfiguration;
class RocketComponent;
class WarningSet;

/// An aerodynamic calculator that uses the extended Barrowman method by delegating the stability
/// and the drag calculations to dedicated calculators (OpenRocket's
/// aerodynamics/BarrowmanCalculator): by default a BarrowmanStabilityCalculator and a
/// BarrowmanDragCalculator. Either can be replaced, which is how OpenRocket's simulations use
/// lookup tables: a LookupTableStabilityCalculator, a LookupTableDragCalculator, or both.
///
/// getAerodynamicForces(): the stability calculator's non-axial forces, the drag calculator's
/// drag, then the stability calculator's damping moments, by which Cm and Cyaw are reduced.
///
/// getForceAnalysis(): the stability calculator's breakdown, with the drag calculator's drag
/// added, per component and in total into the rocket's entry. The result holds, in the order of
/// the configuration's active instances, every active assembly (the forces of its subtree) and
/// every active aerodynamic component (its own forces, for one instance). In each entry a CP or
/// a pressure, base, friction or override CD that is NaN becomes 0, CD is the sum of the four
/// (as the getters of AerodynamicForces give them, with the component's override applied) and
/// CDaxial the drag calculator's toAxialDrag() of it. Then the drag of every assembly but the
/// rocket is replaced by that of its descendants: the pressure, base and friction CD of each
/// aerodynamic descendant and the override CD of each overridden one, each weighted with the
/// descendant's number of active instances over the assembly's, plus the override CD of every
/// overridden assembly in or at the assembly, weighted in the same way; its CD is their sum. The
/// rocket's entry keeps the drag calculator's total, which can hold corrections that belong to
/// no single assembly.
///
/// The entries of the result point at the components of the rocket (ForceMap's keys, and each
/// AerodynamicForces::getComponent(), which the drag getters read for the CD override): the map
/// must be read, or the values wanted copied out of it, before the component tree changes
/// (a component removed, Rocket::loadFrom()) and before the rocket is destroyed. Java's map keeps
/// its components alive.
///
/// A selection of stages the force analysis cannot do, as in OpenRocket: the stability
/// calculator's breakdown replaces an inactive stage by its active top-level child stages, one
/// level deep. An active stage below two inactive ones (a booster set on the body of a booster
/// set, with the outer boosters and the core switched off: FlightConfiguration::setOnlyStage()
/// of the inner set) has no entry in the breakdown, and getForceAnalysis() throws BugError
/// (Java: a NullPointerException). The stage selection is the user's, not a programming error
/// of the caller, so a caller that shows a force analysis (the component analysis, the override
/// tab through ComponentDrag) has to expect it. getCP() and getAerodynamicForces() work for
/// such a selection.
///
/// The cache: checkCache() runs first in getCP(), getAerodynamicForces() and getForceAnalysis()
/// (and so in getWorstCP()), not in checkGeometry() or getStallAngle(), as in Java; when the
/// rocket's aerodynamic or tree modification id has changed, both calculators drop their cached
/// data, with it every pointer to a component of the rocket (see ComponentCalcMap, which also
/// covers a rocket whose component objects were replaced without a change of those ids).
///
/// Summation order: OpenRocket's InstanceMap is a ConcurrentHashMap keyed by the components, so
/// the sums over the components (in the two calculators and in the assemblies' drag here) and
/// the order of the force analysis follow the components' random ids in Java; here they follow
/// the tree order of the active instances (see InstanceMap). The sums can therefore differ from
/// Java's, and from one Java run to the next, in the last bits.
///
/// Deviations from OpenRocket:
/// - The class is final; nothing in OpenRocket extends it.
/// - A null calculator throws BugError (Java: IllegalArgumentException).
/// - BugError where Java throws a NullPointerException: in getForceAnalysis() when the
///   breakdown has no entry for the rocket, for an active assembly or for an active aerodynamic
///   component (see above for the selection of stages that leads to it).
/// - checkGeometry() resolves a null WarningSet with this calculator's own set of ignored
///   warnings (Java: the stability calculator's).
class BarrowmanCalculator final : public AbstractAerodynamicCalculator
{
public:
    /// A calculator with a BarrowmanStabilityCalculator and a BarrowmanDragCalculator.
    BarrowmanCalculator();

    /// A calculator with the given stability and drag calculators, which it owns.
    /// @throws BugError when either is null.
    BarrowmanCalculator(std::unique_ptr<StabilityCalculator> stabilityCalculator,
                        std::unique_ptr<DragCalculator>      dragCalculator);

    /// A calculator with a newInstance() of each of the two calculators.
    [[nodiscard]] std::unique_ptr<AerodynamicCalculator> newInstance() const override;

    /// The stability calculator's stall angle.
    [[nodiscard]] double getStallAngle() const override;

    /// The stability calculator's CP.
    [[nodiscard]] Coordinate getCP(const FlightConfiguration& configuration,
                                   const FlightConditions&    conditions,
                                   WarningSet*                warnings) override;

    /// The forces per component (see the class comment). The entries point at the rocket's
    /// components: read the map before the component tree changes or the rocket is destroyed.
    /// @throws BugError see the class comment: for an active stage below two inactive ones, and
    ///         when the stability calculator's breakdown lacks an entry.
    [[nodiscard]] ForceMap getForceAnalysis(const FlightConfiguration& configuration,
                                            const FlightConditions&    conditions,
                                            WarningSet*                warnings) override;

    /// The forces on the rocket (see the class comment).
    [[nodiscard]] AerodynamicForces getAerodynamicForces(const FlightConfiguration& configuration,
                                                         const FlightConditions&    conditions,
                                                         WarningSet* warnings) override;

    /// The stability calculator's geometry checks.
    void checkGeometry(const FlightConfiguration& configuration, const RocketComponent& component,
                       WarningSet* warnings) override;

    /// ModId::zero(): the calculator has no state a reader could cache (Java: ModID.ZERO).
    [[nodiscard]] ModId modId() const override { return ModId::zero(); }

    /// BarrowmanDragCalculator::calculateStagnationCD().
    [[nodiscard]] static double calculateStagnationCD(double mach) noexcept;

    /// BarrowmanDragCalculator::calculateBaseCD().
    [[nodiscard]] static double calculateBaseCD(double mach) noexcept;

protected:
    /// Voids the cached data of both calculators.
    void voidAerodynamicCache() override;

private:
    /// Replaces the drag of every assembly of @p forceMap but the rocket by that of its
    /// descendants (see the class comment).
    void aggregateAssemblyDrag(const FlightConfiguration& configuration,
                               const FlightConditions& conditions, ForceMap& forceMap) const;

    std::unique_ptr<StabilityCalculator> m_stabilityCalculator;
    std::unique_ptr<DragCalculator>      m_dragCalculator;
};

}  // namespace QtRocket
