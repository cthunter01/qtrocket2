#pragma once

#include <memory>

namespace QtRocket
{

class AerodynamicForces;
class FlightConditions;
class FlightConfiguration;
class ForceMap;
class WarningSet;

/// The axial (drag) half of an aerodynamic calculator (OpenRocket's aerodynamics/DragCalculator):
/// BarrowmanDragCalculator implements the extended Barrowman drag, LookupTableDragCalculator reads
/// the drag from a table. BarrowmanCalculator combines one with a StabilityCalculator.
///
/// Calculations are not const, since an implementation may cache per-component data between
/// calls (voidAerodynamicCache() clears it). Java's null WarningSet is resolved by the caller
/// (AbstractAerodynamicCalculator::actualWarnings()), so the warnings are a reference here.
class DragCalculator
{
public:
    virtual ~DragCalculator() = default;

    DragCalculator& operator=(const DragCalculator&) = delete;
    DragCalculator& operator=(DragCalculator&&)      = delete;

    /// A new, independent calculator of the same kind (Java: newInstance()).
    [[nodiscard]] virtual std::unique_ptr<DragCalculator> newInstance() const = 0;

    /// Calculates the drag of @p configuration in @p conditions into @p totalForces (its
    /// friction, pressure, base and override CDs, CD and CDaxial). When @p componentForces or
    /// @p assemblyForces is given (a force analysis; Java: not null), the per-component and
    /// per-assembly drag goes into their entries as well. @p totalForces may be an entry of
    /// @p assemblyForces (the rocket's), as BarrowmanCalculator::getForceAnalysis() passes it, so
    /// an implementation must not add keys to the maps.
    virtual void calculateDrag(const FlightConfiguration& configuration,
                               const FlightConditions& conditions, ForceMap* componentForces,
                               ForceMap* assemblyForces, AerodynamicForces& totalForces,
                               WarningSet& warnings) = 0;

    /// The axial drag coefficient for the drag coefficient @p cd at the angle of attack of
    /// @p conditions.
    [[nodiscard]] virtual double toAxialDrag(const FlightConditions& conditions,
                                             double                  cd) const = 0;

    /// Clears any cached data (called when the rocket's aerodynamics or tree changes).
    virtual void voidAerodynamicCache() = 0;

protected:
    DragCalculator()                      = default;
    DragCalculator(const DragCalculator&) = default;
    DragCalculator(DragCalculator&&)      = default;
};

}  // namespace QtRocket
