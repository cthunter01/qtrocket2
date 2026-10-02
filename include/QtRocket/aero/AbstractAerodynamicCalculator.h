#pragma once

#include "QtRocket/aero/AerodynamicCalculator.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"

namespace QtRocket
{

class FlightConditions;
class FlightConfiguration;

/// The shared part of the aerodynamic calculators (OpenRocket's
/// aerodynamics/AbstractAerodynamicCalculator): getWorstCP() on top of getCP(), the cache check
/// against the rocket's modification ids, and a WarningSet that stands in for a null one.
///
/// A concrete calculator calls checkCache() first in every calculation that may use cached data;
/// when the rocket's aerodynamic or tree modification id differs from the one seen last,
/// voidAerodynamicCache() runs (an override calls the base version). The first call always voids
/// the cache, since the ids start as fresh draws that no rocket has.
class AbstractAerodynamicCalculator : public AerodynamicCalculator
{
public:
    /// The number of lateral wind directions getWorstCP() tries (DIVISIONS).
    static constexpr int kDivisions = 360;

    /// Tries theta = 2 pi i / kDivisions for i = 0 ... kDivisions - 1 on a copy of @p conditions
    /// and returns the CP with the smallest x among those with a weight (CNa) above
    /// MathUtil::kEpsilon, or (Double.MAX_VALUE, 0, 0, 0) when there is none. The theta that gave
    /// it (as the copy stored it, see FlightConditions::setTheta()) is set in @p conditions, 0 when
    /// there was none; a caller may use the call for that alone and drop the CP.
    Coordinate getWorstCP(const FlightConfiguration& configuration, FlightConditions& conditions,
                          WarningSet* warnings) override;

protected:
    AbstractAerodynamicCalculator() = default;

    /// Voids the cached data when the rocket of @p configuration has a different aerodynamic or
    /// tree modification id than at the previous call (Java's final checkCache()).
    void checkCache(const FlightConfiguration& configuration);

    /// Clears the cached data. An override must call this base version (which does nothing).
    virtual void voidAerodynamicCache();

    /// @p warnings, or the calculator's own set of ignored warnings when it is null (Java's
    /// `(warnings != null) ? warnings : ignoreWarningSet`).
    [[nodiscard]] WarningSet& actualWarnings(WarningSet* warnings) noexcept
    {
        return warnings != nullptr ? *warnings : m_ignoreWarningSet;
    }

    /// The set that collects the warnings nobody asked for (Java: ignoreWarningSet).
    [[nodiscard]] WarningSet& ignoreWarningSet() noexcept { return m_ignoreWarningSet; }

private:
    WarningSet m_ignoreWarningSet;
    /// The aerodynamic and tree modification ids of the rocket seen last.
    ModId m_rocketAeroModId;
    ModId m_rocketTreeModId;
};

}  // namespace QtRocket
