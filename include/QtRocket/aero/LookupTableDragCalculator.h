#pragma once

#include <filesystem>
#include <memory>

#include "QtRocket/aero/DragCalculator.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class AerodynamicForces;
class FlightConditions;
class FlightConfiguration;
class ForceMap;
class WarningSet;

/// A DragCalculator that reads the drag coefficient from a table over the Mach number and the
/// angle of attack (OpenRocket's aerodynamics/LookupTableDragCalculator): the table's "cd" at the
/// current Mach number and angle of attack (in degrees, Java's Math.toDegrees) is the rocket's
/// total and friction drag, and every component and assembly gets zero drag.
///
/// The axial drag follows the Barrowman calculator's conversion: CD times a polynomial in the
/// angle of attack that rises from 1 at 0 degrees to 1.3 at 17 degrees and falls to 0 at 90,
/// mirrored (and negated) beyond 90 degrees.
///
/// The (immutable) table is shared: by newInstance(), and with the caller through the
/// shared_ptr constructor, as Java's SimulationOptions hands its one MachAoALookup object to the
/// calculator of every simulation.
class LookupTableDragCalculator final : public DragCalculator
{
public:
    /// A calculator for @p table, which must have a "cd" column (a convenience for the shared_ptr
    /// constructor, with a table of its own).
    explicit LookupTableDragCalculator(MachAoALookup table);

    /// A calculator that shares @p table, which must have a "cd" column (Java: new
    /// LookupTableDragCalculator(table) with the caller's object).
    /// @throws BugError when @p table is null.
    explicit LookupTableDragCalculator(std::shared_ptr<const MachAoALookup> table);

    /// A calculator for the CSV table in @p csvPath (a "mach" and a "cd" column, optionally
    /// "aoa"); see CsvMachAoALookup::fromCsv() for the failures (Java's constructor throws).
    [[nodiscard]] static Result<LookupTableDragCalculator> fromCsv(
        const std::filesystem::path& csvPath);

    [[nodiscard]] std::unique_ptr<DragCalculator> newInstance() const override;

    /// Sets @p totalForces' friction CD and CD to the table's "cd", its pressure, base and
    /// override CDs to 0 and its CDaxial to toAxialDrag(); every entry of @p componentForces and
    /// @p assemblyForces (when given) gets zero friction, pressure, base, override, total and
    /// axial drag. @p configuration and @p warnings are not used.
    void calculateDrag(const FlightConfiguration& configuration, const FlightConditions& conditions,
                       ForceMap* componentForces, ForceMap* assemblyForces,
                       AerodynamicForces& totalForces, WarningSet& warnings) override;

    /// @p cd times the axial polynomial at the angle of attack clamped to 0 ... pi, mirrored
    /// about pi/2; negative beyond pi/2.
    [[nodiscard]] double toAxialDrag(const FlightConditions& conditions, double cd) const override;

    /// Nothing to clear.
    void voidAerodynamicCache() override;

    /// The table.
    [[nodiscard]] const MachAoALookup& getTable() const noexcept { return *m_table; }

    /// The table, shared (never null).
    [[nodiscard]] std::shared_ptr<const MachAoALookup> getTableShared() const noexcept
    {
        return m_table;
    }

private:
    std::shared_ptr<const MachAoALookup> m_table;
};

}  // namespace QtRocket
