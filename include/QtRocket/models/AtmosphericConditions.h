#pragma once

#include <string>

#include "QtRocket/util/ModId.h"

namespace QtRocket
{

/// The state of the air at one point (OpenRocket's models/atmosphere/AtmosphericConditions): the
/// temperature, pressure and relative humidity, and the properties derived from them (density,
/// speed of sound, kinematic viscosity). The atmospheric models return one per altitude, and
/// simulation listeners may adjust it through the setters.
///
/// A value type: a copy is Java's clone() and keeps the modification id; every setter draws a
/// fresh one (modId(), the Monitorable concept).
///
/// Error policy: Java throws IllegalArgumentException from the setters (and so the constructors)
/// for a temperature or pressure that is not positive and a humidity outside 0 ... 1. Here that
/// is a BugError: the values come from an atmospheric model or a listener, and the user's launch
/// conditions are checked before they get here (ExtendedIsaModel::create() returns a Result). A
/// NaN passes every check, as in Java.
class AtmosphericConditions
{
public:
    /// Specific gas constant of dry air, J/(kg K) (R).
    static constexpr double kR = 287.053;
    /// Specific heat ratio of air (GAMMA).
    static constexpr double kGamma = 1.4;
    /// Ratio of the molar masses of water vapour and dry air (EPSILON).
    static constexpr double kEpsilon = 0.622;
    /// The standard air pressure, Pa (STANDARD_PRESSURE).
    static constexpr double kStandardPressure = 101325.0;
    /// The standard air temperature, K (STANDARD_TEMPERATURE): 20 degC, not the ISA's 15 degC.
    static constexpr double kStandardTemperature = 293.15;
    /// The standard relative humidity (STANDARD_HUMIDITY).
    static constexpr double kStandardHumidity = 0;

    /// The standard conditions: kStandardTemperature, kStandardPressure, kStandardHumidity.
    AtmosphericConditions();

    /// @p temperature in K, @p pressure in Pa, @p relativeHumidity 0 ... 1 (Java's two-argument
    /// constructor is the default humidity).
    /// @throws BugError when a value is out of range (see the class comment).
    AtmosphericConditions(double temperature, double pressure,
                          double relativeHumidity = kStandardHumidity);

    /// The pressure in Pa.
    [[nodiscard]] double getPressure() const noexcept { return m_pressure; }
    /// @throws BugError when @p pressure is not positive ("Pressure must be positive (Pascals)").
    void setPressure(double pressure);

    /// The temperature in K.
    [[nodiscard]] double getTemperature() const noexcept { return m_temperature; }
    /// @throws BugError when @p temperature is not positive ("Temperature must be positive
    ///         (Kelvin)").
    void setTemperature(double temperature);

    /// The relative humidity, 0 ... 1.
    [[nodiscard]] double getRelativeHumidity() const noexcept { return m_relativeHumidity; }
    /// @throws BugError when @p relativeHumidity is outside 0 ... 1 ("Humidity must be between 0
    ///         and 1").
    void setRelativeHumidity(double relativeHumidity);

    /// The saturation vapour pressure of water in Pa (Clausius-Clapeyron):
    /// 611.3 * exp(19.854 - 5423 / T).
    [[nodiscard]] double vaporPressureSaturation() const noexcept;

    /// The specific gas constant of the humid air in J/(kg K): exactly kR when the relative
    /// humidity is not positive, otherwise
    /// kR * (1 + (eps * RH * es) * (1/eps - 1) / (p - RH * es * (1 - eps))) with es the saturation
    /// vapour pressure.
    [[nodiscard]] double getGasConstant() const noexcept;

    /// The density in kg/m^3 by the ideal gas law, p / (R T) with R = getGasConstant().
    [[nodiscard]] double getDensity() const noexcept;

    /// The speed of sound in m/s, OpenRocket's linear fit 165.77 + 0.606 * T (T in K; accurate to
    /// about 0.5 m/s between -30 and 30 degC).
    [[nodiscard]] double getMachSpeed() const noexcept;

    /// The kinematic viscosity in m^2/s: the linear approximation 3.7291e-06 + 4.9944e-08 * T of
    /// the dynamic viscosity (Sutherland's formula is nearly linear from -40 to 40 degC) divided
    /// by getDensity().
    [[nodiscard]] double getKinematicViscosity() const noexcept;

    /// The id of the current state (Monitorable).
    [[nodiscard]] ModId modId() const noexcept { return m_modId; }

    /// Java's equals(): the pressure, temperature and humidity each agree within
    /// MathUtil::kEpsilon (MathUtil::equals, so never for a NaN). The modification id does not
    /// count.
    [[nodiscard]] bool operator==(const AtmosphericConditions& other) const noexcept;

    /// Java's hashCode(): (int)(pressure + temperature * 1000 + humidity * 1000000), narrowed as
    /// Java narrows (MathUtil::javaIntCast).
    [[nodiscard]] int hashCode() const noexcept;

    /// "AtmosphericConditions[T=<K>,P=<Pa>]" with two decimals each, digit for digit as Java's
    /// String.format("%.2f") in an English locale (Strings::formatFixed).
    [[nodiscard]] std::string toString() const;

private:
    double m_pressure;
    double m_temperature;
    double m_relativeHumidity;
    ModId  m_modId;
};

}  // namespace QtRocket
