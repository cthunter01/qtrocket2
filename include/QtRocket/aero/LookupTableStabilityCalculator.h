#pragma once

#include <filesystem>
#include <limits>
#include <memory>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/StabilityCalculator.h"
#include "QtRocket/aero/StabilityForceBreakdown.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class FlightConditions;
class FlightConfiguration;
class RocketComponent;
class WarningSet;

/// A StabilityCalculator that reads CN, Cm and the CP from a table over the Mach number and the
/// angle of attack (OpenRocket's aerodynamics/LookupTableStabilityCalculator): the table's "cn",
/// "cm" and "cp" at the current Mach number and angle of attack (in degrees, Java's
/// Math.toDegrees) are the rocket's normal force and pitching moment coefficients and the CP's x
/// (with CNa 1); every side, yaw and roll coefficient and the damping moments are 0, and no
/// geometry is checked.
///
/// The stall angle is the table's largest angle of attack (Java's Math.toRadians), or +inf for a
/// table without angles of attack. newInstance() shares the (immutable) table.
class LookupTableStabilityCalculator final : public StabilityCalculator
{
public:
    /// A calculator for @p table, which must have "cn", "cm" and "cp" columns.
    explicit LookupTableStabilityCalculator(MachAoALookup table);

    /// A calculator for the CSV table in @p csvPath ("mach", "cn", "cm" and "cp" columns,
    /// optionally "aoa"); see CsvMachAoALookup::fromCsv() for the failures (Java's constructor
    /// throws).
    [[nodiscard]] static Result<LookupTableStabilityCalculator> fromCsv(
        const std::filesystem::path& csvPath);

    [[nodiscard]] std::unique_ptr<StabilityCalculator> newInstance() const override;

    [[nodiscard]] double getStallAngle() const override { return m_stallAngle; }

    /// calculateNonAxialForces(...).getCP().
    [[nodiscard]] Coordinate getCP(const FlightConfiguration& configuration,
                                   const FlightConditions&    conditions,
                                   WarningSet&                warnings) override;

    /// Zeroed forces with the table's CN and Cm and the CP (cp, 0, 0) with CNa 1.
    /// @p configuration and @p warnings are not used.
    [[nodiscard]] AerodynamicForces calculateNonAxialForces(
        const FlightConfiguration& configuration, const FlightConditions& conditions,
        WarningSet& warnings) override;

    /// The total of calculateNonAxialForces(), associated with the rocket, as the rocket's
    /// assembly entry; every other active assembly gets zeroed forces in the assembly map and
    /// every other active aerodynamic component in the component map, in the order of the active
    /// instances.
    [[nodiscard]] StabilityForceBreakdown getForceAnalysis(const FlightConfiguration& configuration,
                                                           const FlightConditions&    conditions,
                                                           WarningSet& warnings) override;

    /// Sets @p total's pitch and yaw damping moments to 0.
    void calculateDampingMoments(const FlightConfiguration& configuration,
                                 const FlightConditions&    conditions,
                                 AerodynamicForces&         total) override;

    /// Nothing to check for table data.
    void checkGeometry(const FlightConfiguration& configuration, const RocketComponent& component,
                       WarningSet& warnings) override;

    /// Nothing to clear.
    void voidAerodynamicCache() override;

    /// The table.
    [[nodiscard]] const MachAoALookup& getTable() const noexcept { return *m_table; }

private:
    std::shared_ptr<const MachAoALookup> m_table;
    double                               m_stallAngle{std::numeric_limits<double>::infinity()};
};

}  // namespace QtRocket
