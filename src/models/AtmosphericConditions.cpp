#include "QtRocket/models/AtmosphericConditions.h"

#include <cmath>
#include <format>
#include <string>
#include <string_view>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

// The messages of Java's IllegalArgumentExceptions.
constexpr std::string_view kPressureMessage    = "Pressure must be positive (Pascals)";
constexpr std::string_view kTemperatureMessage = "Temperature must be positive (Kelvin)";
constexpr std::string_view kHumidityMessage    = "Humidity must be between 0 and 1";

// The checks of Java's setters, which the constructors go through as well: true when the value
// is rejected. `!(x > 0)` would also catch NaN; Java's `x <= 0` lets it through, and so do these.
[[nodiscard]] bool rejectsPressure(double pressure) noexcept
{
    return pressure <= 0;
}

[[nodiscard]] bool rejectsTemperature(double temperature) noexcept
{
    return temperature <= 0;
}

[[nodiscard]] bool rejectsRelativeHumidity(double relativeHumidity) noexcept
{
    return relativeHumidity < 0 || relativeHumidity > 1;
}

void checkPressure(double pressure)
{
    if (rejectsPressure(pressure))
    {
        bug(kPressureMessage);
    }
}

void checkTemperature(double temperature)
{
    if (rejectsTemperature(temperature))
    {
        bug(kTemperatureMessage);
    }
}

void checkRelativeHumidity(double relativeHumidity)
{
    if (rejectsRelativeHumidity(relativeHumidity))
    {
        bug(kHumidityMessage);
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

Result<void> AtmosphericConditions::validate(double temperature, double pressure,
                                             double relativeHumidity)
{
    if (rejectsTemperature(temperature))
    {
        return fail(ErrorCode::INVALID_ARGUMENT, std::string{kTemperatureMessage});
    }
    if (rejectsPressure(pressure))
    {
        return fail(ErrorCode::INVALID_ARGUMENT, std::string{kPressureMessage});
    }
    if (rejectsRelativeHumidity(relativeHumidity))
    {
        return fail(ErrorCode::INVALID_ARGUMENT, std::string{kHumidityMessage});
    }
    return {};
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
    if (this == &other)
    {
        return true;  // as Java, so an object equals itself even with a NaN field
    }
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
