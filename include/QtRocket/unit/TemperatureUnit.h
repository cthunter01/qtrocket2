#pragma once

#include <string>

#include "QtRocket/unit/FixedPrecisionUnit.h"
#include "QtRocket/unit/Unit.h"

namespace QtRocket
{

/// A temperature scale with an offset (OpenRocket's TemperatureUnit): degrees Celsius are
/// TemperatureUnit(1, 273.15, 0.01, "°C"), Fahrenheit TemperatureUnit(5/9, 459.67, 0.01,
/// "°F"). No space is put between the value and the unit name.
class TemperatureUnit final : public FixedPrecisionUnit
{
public:
    /// @param multiplier kelvins per degree of this scale
    /// @param addition   the scale's zero, in its own degrees below 0 K
    /// @throws BugError when @p multiplier is 0
    TemperatureUnit(double multiplier, double addition, double precision, std::string unit);

    [[nodiscard]] double getAddition() const noexcept { return m_addition; }

    [[nodiscard]] bool hasSpace() const override;
    /// value / multiplier - addition.
    [[nodiscard]] double toUnit(double value) const override;
    /// (value + addition) * multiplier.
    [[nodiscard]] double fromUnit(double value) const override;

private:
    double m_addition;
};

}  // namespace QtRocket
