#pragma once

#include <memory>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/StabilityForceBreakdown.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

class FlightConditions;
class FlightConfiguration;
class RocketComponent;
class WarningSet;

/// The non-axial (stability) half of an aerodynamic calculator (OpenRocket's
/// aerodynamics/StabilityCalculator): normal and side forces, pitch, yaw and roll moments, the CP,
/// the damping moments and the geometry checks. BarrowmanStabilityCalculator implements the
/// extended Barrowman method, LookupTableStabilityCalculator reads the coefficients from a table.
///
/// Calculations are not const, since an implementation may cache per-component data between
/// calls (voidAerodynamicCache() clears it). Java's null WarningSet is resolved by the caller
/// (AbstractAerodynamicCalculator::actualWarnings()), so the warnings are a reference here.
class StabilityCalculator
{
public:
    virtual ~StabilityCalculator() = default;

    StabilityCalculator& operator=(const StabilityCalculator&) = delete;
    StabilityCalculator& operator=(StabilityCalculator&&)      = delete;

    /// A new, independent calculator of the same kind (Java: newInstance()).
    [[nodiscard]] virtual std::unique_ptr<StabilityCalculator> newInstance() const = 0;

    /// The stall angle in radians.
    [[nodiscard]] virtual double getStallAngle() const = 0;

    /// The CP of @p configuration in @p conditions, with CNa as its weight.
    [[nodiscard]] virtual Coordinate getCP(const FlightConfiguration& configuration,
                                           const FlightConditions&    conditions,
                                           WarningSet&                warnings) = 0;

    /// The non-axial force coefficients of the whole rocket (the drag is left to the
    /// DragCalculator).
    [[nodiscard]] virtual AerodynamicForces calculateNonAxialForces(
        const FlightConfiguration& configuration, const FlightConditions& conditions,
        WarningSet& warnings) = 0;

    /// The non-axial forces per component and per assembly; the rocket's assembly entry is the
    /// total.
    [[nodiscard]] virtual StabilityForceBreakdown getForceAnalysis(
        const FlightConfiguration& configuration, const FlightConditions& conditions,
        WarningSet& warnings) = 0;

    /// Sets the pitch and yaw damping moments of @p total.
    virtual void calculateDampingMoments(const FlightConfiguration& configuration,
                                         const FlightConditions&    conditions,
                                         AerodynamicForces&         total) = 0;

    /// Checks the component tree under @p component for geometric problems and adds the
    /// warnings to @p warnings.
    virtual void checkGeometry(const FlightConfiguration& configuration,
                               const RocketComponent& component, WarningSet& warnings) = 0;

    /// Clears any cached data (called when the rocket's aerodynamics or tree changes).
    virtual void voidAerodynamicCache() = 0;

protected:
    StabilityCalculator()                           = default;
    StabilityCalculator(const StabilityCalculator&) = default;
    StabilityCalculator(StabilityCalculator&&)      = default;
};

}  // namespace QtRocket
