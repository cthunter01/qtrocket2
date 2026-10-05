#pragma once

#include <memory>

#include "QtRocket/aero/DragCalculator.h"
#include "QtRocket/aero/barrowman/ComponentCalcMap.h"

namespace QtRocket
{

class AerodynamicForces;
class FlightConditions;
class FlightConfiguration;
class ForceMap;
class RocketComponent;
class RocketComponentCalc;
class WarningSet;

/// The drag half of the extended Barrowman method (OpenRocket's
/// aerodynamics/BarrowmanDragCalculator): the friction, pressure, base and override drag of a
/// rocket, summed over the calculations of its active components (aero/barrowman/), and the
/// conversion of a drag coefficient into an axial one.
///
/// Friction drag: the rocket's Reynolds number (velocity * aerodynamic length / kinematic
/// viscosity) gives the skin friction coefficient calculateFrictionCoefficient(); a component's
/// own coefficient is that or the roughness-limited one of its Finish,
/// 0.032 * (roughness / aerodynamic length)^0.2 * calculateRoughnessCorrection(): the larger of
/// the two, but with a perfect finish the roughness-limited one only above a Reynolds number of
/// 1e6 and when it is the larger. Each component's calculation turns it into a friction CD. The
/// CDs of the symmetric components are summed apart and multiplied by
/// calculateBodyFrictionCorrection() of the body they form: from the foremost front to the
/// rearmost end of those components (their getAxialOffset(AxialMethod::ABSOLUTE) and lengths)
/// plus 0.1 mm, with their largest radius.
///
/// Pressure drag: each calculation's pressure CD, plus for a symmetric component the stagnation
/// drag of the ring by which its fore radius exceeds the aft radius of the active symmetric
/// component ahead of it (0 when there is none); a component of length 0 counts with the larger
/// of its radii.
///
/// Base drag: for a symmetric component the base CD on the ring by which its aft radius exceeds
/// the fore radius of the active symmetric component behind it; when none follows (a terminal
/// base), the area is reduced by the nozzle exit area of the motors thrusting into the wake of
/// the component's assembly (FlightConditions::getThrustingNozzleExitArea(assembly)), at most to
/// nothing, over all the instances of the component together. For the other aerodynamic
/// components, the base CD of their calculation when it is positive (fins).
///
/// Override drag: the override CD of every active aerodynamic component and assembly whose CD is
/// overridden, unless an ancestor overrides it. Components that are overridden, by themselves or
/// by an ancestor, have no friction, pressure or base drag.
///
/// Every part counts each instance of a component. The parts go into the total forces, with
/// their sum as CD and toAxialDrag() of it as CDaxial. In a force analysis each component's
/// friction, pressure and base drag of one instance go into its entry of the component map as
/// well, and its override CD times its instance count; an overridden assembly's goes into its
/// entry of the assembly map. Nothing else is written to the assembly map.
///
/// The cache is a ComponentCalcMap of its own, built at the first calculateDrag(). As in Java
/// this calculator does not look at the rocket's modification ids itself: the BarrowmanCalculator
/// that owns it calls voidAerodynamicCache() when they change, and a caller that uses the
/// calculator on its own must do the same after changing the rocket.
///
/// Summation order: OpenRocket's InstanceMap is a ConcurrentHashMap keyed by the components, so
/// Java sums the components in an order that follows their random ids; here the active
/// instances are in tree order (see InstanceMap). Sums over the components can therefore differ
/// from Java's, and from one Java run to the next, in the last bits.
///
/// Deviations from OpenRocket:
/// - Java's null WarningSet is resolved by the caller (see DragCalculator), so the calculator
///   has no ignoreWarningSet of its own.
/// - BugError where Java throws a NullPointerException: for an active aerodynamic component
///   without a calculation (one that joined the rocket after the calculations were made, the
///   cache not having been voided since).
/// - Java's static axial drag polynomials are function-local statics, built at first use from
///   the same expressions.
/// - Public here, for the tests: calculateBodyFrictionCorrection() (Java: package-private, called
///   by BarrowmanDragCalculatorTest), calculateFrictionCoefficient() and
///   calculateRoughnessCorrection() (Java: private; the first takes the configuration there, of
///   which it reads the rocket's perfect finish only).
///
/// Java's BarrowmanCalculator has static calculateStagnationCD() and calculateBaseCD() that only
/// forward to the ones here, and so has QtRocket's; the component calculations of
/// aero/barrowman/ call these directly.
class BarrowmanDragCalculator final : public DragCalculator
{
public:
    BarrowmanDragCalculator() = default;

    /// A new calculator with an empty cache.
    [[nodiscard]] std::unique_ptr<DragCalculator> newInstance() const override;

    /// Calculates the drag (see the class comment) into @p totalForces and, when given, into the
    /// entries of @p componentForces and @p assemblyForces; no entry is added to either.
    /// @throws BugError see the class comment.
    void calculateDrag(const FlightConfiguration& configuration, const FlightConditions& conditions,
                       ForceMap* componentForces, ForceMap* assemblyForces,
                       AerodynamicForces& totalForces, WarningSet& warnings) override;

    /// @p cd times a multiplier of the angle of attack, clamped to 0 ... pi and mirrored about
    /// pi/2: a polynomial that rises from 1 at 0 to 1.3 at 17 degrees with zero slope at both,
    /// then one that falls to 0 at 90 degrees, with zero slope at both ends and zero curvature at
    /// 90 degrees. The result is negated from an angle of attack of pi/2 on.
    [[nodiscard]] double toAxialDrag(const FlightConditions& conditions, double cd) const override;

    /// Drops the calculations (with their pointers to the rocket's components, see
    /// ComponentCalcMap).
    void voidAerodynamicCache() override;

    /// The correction of the skin friction of a cylindrical body for its fineness (OpenRocket
    /// technical documentation, equation 3.85): 1 + 1 / (2 * fineness ratio), the fineness ratio
    /// being @p bodyLength over the largest diameter, 2 * @p maxRadius. A radius of 0 gives 1
    /// (and NaN with a length of 0).
    [[nodiscard]] static double calculateBodyFrictionCorrection(double bodyLength,
                                                                double maxRadius) noexcept;

    /// The skin friction coefficient of the rocket at the Reynolds number @p reynolds, with the
    /// compressibility correction for Mach @p mach.
    ///
    /// With a perfect finish: 1.33e-2 below a Reynolds number of 1e4, the laminar
    /// 1.328 / sqrt(Re) below 5.39e5, and from there 1 / (1.5 ln Re - 5.6)^2 - 1700 / Re. Above
    /// a Reynolds number of 1e6 it is corrected, in full from 3e6 and in proportion to
    /// (Re - 1e6) / 2e6 below: by c1 = 1 - 0.1 M^2 subsonically and by
    /// c2 = 1 / (1 + 0.045 M^2)^0.25 supersonically.
    ///
    /// Otherwise: 1.48e-2 below a Reynolds number of 1e4 and 1 / (1.5 ln Re - 5.6)^2 from there,
    /// corrected by c1 = 1 - 0.1 M^2 and c2 = 1 / (1 + 0.15 M^2)^0.58.
    ///
    /// In both cases the correction is c1 below Mach 0.9, c2 from Mach 1.1, and between them
    /// c2 * (M - 0.9) / 0.2 + c1 * (1.1 - M) / 0.2.
    [[nodiscard]] static double calculateFrictionCoefficient(bool perfectFinish, double mach,
                                                             double reynolds) noexcept;

    /// The compressibility correction of the roughness-limited skin friction coefficient:
    /// 1 - 0.1 M^2 below Mach 0.9, 1 / (1 + 0.18 M^2) above Mach 1.1, and between them the
    /// interpolation of the values at those two Mach numbers.
    [[nodiscard]] static double calculateRoughnessCorrection(double mach) noexcept;

    /// The stagnation pressure drag coefficient at Mach @p mach: 0.85 times the pressure ratio
    /// 1 + M^2 / 4 + M^4 / 40 up to Mach 1 (inclusive), and
    /// 1.84 - 0.76 / M^2 + 0.166 / M^4 + 0.035 / M^6 above. A NaN gives NaN.
    [[nodiscard]] static double calculateStagnationCD(double mach) noexcept;

    /// The base pressure drag coefficient at Mach @p mach: 0.12 + 0.13 M^2 up to Mach 1
    /// (inclusive), 0.25 / M above. A NaN gives NaN.
    [[nodiscard]] static double calculateBaseCD(double mach) noexcept;

private:
    /// The calculation of the active aerodynamic component @p component.
    /// @throws BugError when there is none (see the class comment).
    [[nodiscard]] RocketComponentCalc& calcOf(const RocketComponent& component);

    /// The friction drag of the rocket; each component's into @p forceMap when given.
    [[nodiscard]] double calculateFrictionCD(const FlightConfiguration& configuration,
                                             const FlightConditions& conditions, ForceMap* forceMap,
                                             WarningSet& warnings);

    /// The pressure drag of the rocket; each component's into @p forceMap when given.
    [[nodiscard]] double calculatePressureCD(const FlightConfiguration& configuration,
                                             const FlightConditions& conditions, ForceMap* forceMap,
                                             WarningSet& warnings);

    /// The base drag of the rocket; each component's into @p forceMap when given.
    [[nodiscard]] double calculateBaseCD(const FlightConfiguration& configuration,
                                         const FlightConditions& conditions, ForceMap* forceMap,
                                         WarningSet& warnings);

    ComponentCalcMap m_calcMap;
};

}  // namespace QtRocket
