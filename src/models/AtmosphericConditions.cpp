#include "QtRocket/models/AtmosphericConditions.h"

#include <cmath>
#include <format>
#include <string>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

// The checks of Java's setters, which the constructors go through as well. `!(x > 0)` would also
// catch NaN; Java's `x <= 0` lets it through, and so do these.
void checkPressure(double pressure)
{
    if (pressure <= 0)
    {
        bug("Pressure must be positive (Pascals)");
    }
}

void checkTemperature(double temperature)
{
    if (temperature <= 0)
    {
        bug("Temperature must be positive (Kelvin)");
    }
}

void checkRelativeHumidity(double relativeHumidity)
{
    if (relativeHumidity < 0 || relativeHumidity > 1)
    {
        bug("Humidity must be between 0 and 1");
    }
}

}  // namespace

AtmosphericConditions::AtmosphericConditions()
  : AtmosphericConditions(kStandardTemperature, kStandardPressure, kStandardHumidity)
{
}

AtmosphericConditions::AtmosphericConditions(double temperature, double pressure,
                                             double relativeHumidity)
  : m_pressure(pressure), m_temperature(temperature), m_relativeHumidity(relativeHumidity)
{
    // Java calls setTemperature, setPressure and setRelativeHumidity in this order.
    checkTemperature(temperature);
    checkPressure(pressure);
    checkRelativeHumidity(relativeHumidity);
}

void AtmosphericConditions::setPressure(double pressure)
{
    checkPressure(pressure);
    m_pressure = pressure;
    m_modId    = ModId{};
}

void AtmosphericConditions::setTemperature(double temperature)
{
    checkTemperature(temperature);
    m_temperature = temperature;
    m_modId       = ModId{};
}

void AtmosphericConditions::setRelativeHumidity(double relativeHumidity)
{
    checkRelativeHumidity(relativeHumidity);
    m_relativeHumidity = relativeHumidity;
    m_modId            = ModId{};
}

double AtmosphericConditions::vaporPressureSaturation() const noexcept
{
    return 611.3 * std::exp(19.854 - (5423 / m_temperature));
}

double AtmosphericConditions::getGasConstant() const noexcept
{
    if (m_relativeHumidity > 0)
    {
        const double numerator = kEpsilon * m_relativeHumidity * vaporPressureSaturation();
        const double denominator =
            m_pressure - (m_relativeHumidity * vaporPressureSaturation() * (1 - kEpsilon));
        const double scalingFactor = (1 / kEpsilon) - 1;

        return kR * (1 + (numerator * scalingFactor / denominator));
    }
    return kR;
}

double AtmosphericConditions::getDensity() const noexcept
{
    return m_pressure / (getGasConstant() * m_temperature);
}

double AtmosphericConditions::getMachSpeed() const noexcept
{
    return 165.77 + (0.606 * m_temperature);
}

double AtmosphericConditions::getKinematicViscosity() const noexcept
{
    const double v = 3.7291e-06 + (4.9944e-08 * m_temperature);
    return v / getDensity();
}

bool AtmosphericConditions::operator==(const AtmosphericConditions& other) const noexcept
{
    return MathUtil::equals(m_pressure, other.m_pressure) &&
           MathUtil::equals(m_temperature, other.m_temperature) &&
           MathUtil::equals(m_relativeHumidity, other.m_relativeHumidity);
}

int AtmosphericConditions::hashCode() const noexcept
{
    return MathUtil::javaIntCast(m_pressure + (m_temperature * 1000) +
                                 (m_relativeHumidity * 1000000));
}

std::string AtmosphericConditions::toString() const
{
    return std::format("AtmosphericConditions[T={},P={}]", Strings::formatFixed(m_temperature, 2),
                       Strings::formatFixed(m_pressure, 2));
}

}  // namespace QtRocket
