#pragma once

#include <mutex>
#include <vector>

#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/models/AtmosphericModel.h"

namespace QtRocket
{

/// An atmospheric model that precomputes its conditions at fixed altitude steps and interpolates
/// between them (OpenRocket's models/atmosphere/InterpolatingAtmosphericModel). A subclass gives
/// the exact conditions at any altitude and the altitude up to which they are tabulated.
///
/// The table (the levels) holds getExactConditions(i * kDelta) for i = 0 ... ceil(max / kDelta) -
/// 1, computed on the first getConditions() call. getConditions() then returns the first level at
/// or below 0 m, the last level at or above its altitude, and the linear interpolation of the
/// temperature, pressure and humidity of the two surrounding levels in between.
///
/// Thread safety: the table is built once under std::call_once, so any number of threads may
/// call getConditions() on one model (Java double-checks a null array inside a synchronized block
/// without a volatile field; std::call_once is the sound version of that). The once_flag makes
/// the models neither copyable nor movable; they are held through pointers.
///
/// Deviations from OpenRocket:
/// - The conditions are returned by value. Java hands out the cached level objects themselves at
///   and beyond the ends of the table, so a caller that modified them would change the table.
/// - A table with no levels (getMaxAltitude() not positive) is a BugError on the first call;
///   Java fails with ArrayIndexOutOfBoundsException.
/// - The index of the lower level is clamped below the last one. Java would index past its table
///   if an altitude a hair below the top had a quotient by kDelta that rounded up to the last
///   index; with kDelta = 500 that cannot happen (the division is correctly rounded, and the
///   quotient of the largest double below 500 * n stays below n), so the clamp is defensive.
class InterpolatingAtmosphericModel : public AtmosphericModel
{
public:
    /// The thickness in metres of one interpolation layer (DELTA): small enough to keep the
    /// linear interpolation accurate for rocket flights, large enough to keep the table short.
    static constexpr double kDelta = 500;

    ~InterpolatingAtmosphericModel() override = default;

    InterpolatingAtmosphericModel(const InterpolatingAtmosphericModel&)            = delete;
    InterpolatingAtmosphericModel& operator=(const InterpolatingAtmosphericModel&) = delete;
    InterpolatingAtmosphericModel(InterpolatingAtmosphericModel&&)                 = delete;
    InterpolatingAtmosphericModel& operator=(InterpolatingAtmosphericModel&&)      = delete;

    /// The conditions at @p altitude (m), interpolated from the table as the class comment
    /// describes. A NaN altitude interpolates with a NaN weight between the first two levels,
    /// giving NaN values, as in Java.
    /// @throws BugError when the table is empty (see the class comment).
    [[nodiscard]] AtmosphericConditions getConditions(double altitude) const override;

protected:
    InterpolatingAtmosphericModel() = default;

    /// The altitude (m) up to which the table is computed.
    [[nodiscard]] virtual double getMaxAltitude() const = 0;

    /// The exact conditions at @p altitude (m). Called kDelta metres apart when the table is
    /// built, possibly from any thread (once per model).
    [[nodiscard]] virtual AtmosphericConditions getExactConditions(double altitude) const = 0;

    /// The altitudes (m) at which the table samples getExactConditions(): i * kDelta for i = 0
    /// ... ceil(getMaxAltitude() / kDelta) - 1, none when that is not positive. A subclass whose
    /// values come from the user can check them there before the first getConditions().
    [[nodiscard]] std::vector<double> tableAltitudes() const;

private:
    /// computeLayers(): the exact conditions at every tableAltitudes() altitude.
    [[nodiscard]] std::vector<AtmosphericConditions> computeLayers() const;

    mutable std::once_flag                     m_levelsOnce;
    mutable std::vector<AtmosphericConditions> m_levels;
};

}  // namespace QtRocket
