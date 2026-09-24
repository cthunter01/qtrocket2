#include "QtRocket/models/ExtendedIsaModel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>

#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// Gravitational acceleration in m/s^2 (G).
constexpr double kG = 9.80665;

/// calculatePressure(): the pressure at geopotential altitude @p alt1 (temperature @p temp1)
/// from the pressure @p press2 at @p alt2 (temperature @p temp2), by the barometric formula.
/// When the two altitudes coincide the lapse rate is 0 / 0 = NaN, which fails the comparison and
/// takes the isothermal branch with exp(-0) = 1, giving @p press2 back, as in Java.
[[nodiscard]] double calculatePressure(double alt1, double temp1, double alt2, double temp2,
                                       double press2) noexcept
{
    const double tempRate = (temp2 - temp1) / (alt2 - alt1);
    if (std::abs(tempRate) > 0.000001)
    {
        // Non-isothermal case
        return press2 / std::pow(1 + ((alt2 - alt1) * tempRate / temp1),
                                 -kG / (tempRate * AtmosphericConditions::kR));
    }
    // Isothermal case
    return press2 / std::exp(-(alt2 - alt1) * kG / (AtmosphericConditions::kR * temp1));
}

/// The first layer whose base pressure and humidity the constructor computes: the layers below
/// it (sea level, and the launch site when it is above sea level) are given.
[[nodiscard]] constexpr std::size_t firstComputedLayer(double altitude) noexcept
{
    return altitude > 0 ? 2 : 1;
}

/// calculateRelativeHumidity(): the humidity at another geopotential altitude. OpenRocket carries
/// the source humidity over unchanged (a TODO there), so the altitudes are unused.
[[nodiscard]] double calculateRelativeHumidity(double /*altSource*/, double /*altTarget*/,
                                               double relativeHumiditySource) noexcept
{
    return relativeHumiditySource;
}

}  // namespace

ExtendedIsaModel::ExtendedIsaModel()
  : ExtendedIsaModel(Key{}, 0, kStandardTemperature, kStandardPressure, kStandardRelativeHumidity)
{
}

ExtendedIsaModel::ExtendedIsaModel(Key /*key*/, double altitude, double temperature,
                                   double pressure, double relativeHumidity)
{
    const double geopotentialAltitude = geometricToGeopotential(altitude);

    // If altitude is not 0, we need to create a new layer structure
    if (altitude > 0)
    {
        // One layer more than the standard ones: the launch site
        const std::size_t newSize = kStandardLayers.size() + 1;
        m_layer.assign(newSize, 0.0);
        m_baseTemperature.assign(newSize, 0.0);
        m_basePressure.assign(newSize, 0.0);
        m_baseRelativeHumidity.assign(newSize, 0.0);

        // Standard second layer values (11km)
        const double layer1Alt  = kStandardLayers[1];
        const double layer1Temp = kStandardTemperatures[1];

        // Temperature lapse rate between the launch site and 11 km, extended down to sea level
        const double tempRate     = (layer1Temp - temperature) / (layer1Alt - geopotentialAltitude);
        const double seaLevelTemp = temperature - (tempRate * geopotentialAltitude);

        m_layer[0]           = 0;                     // Sea level
        m_layer[1]           = geopotentialAltitude;  // Launch site
        m_baseTemperature[0] = seaLevelTemp;
        m_baseTemperature[1] = temperature;
        m_basePressure[0] =
            calculatePressure(0, seaLevelTemp, geopotentialAltitude, temperature, pressure);
        m_basePressure[1] = pressure;
        m_baseRelativeHumidity[0] =
            calculateRelativeHumidity(geopotentialAltitude, 0, relativeHumidity);
        m_baseRelativeHumidity[1] = relativeHumidity;

        // The standard layers from 11 km up
        for (std::size_t i = 2; i < newSize; ++i)
        {
            m_layer[i]           = kStandardLayers.at(i - 1);
            m_baseTemperature[i] = kStandardTemperatures.at(i - 1);
        }
    }
    else
    {
        m_layer.assign(kStandardLayers.begin(), kStandardLayers.end());
        m_baseTemperature.assign(kStandardTemperatures.begin(), kStandardTemperatures.end());
        m_basePressure.assign(m_layer.size(), 0.0);
        m_baseRelativeHumidity.assign(m_layer.size(), 0.0);
        m_layer[0]                = 0;
        m_baseTemperature[0]      = temperature;
        m_basePressure[0]         = pressure;
        m_baseRelativeHumidity[0] = relativeHumidity;
    }

    // The pressure and humidity of every remaining layer, 1 m below its base, from the layer
    // below (Java calls getExactConditions() twice per layer with the same result; once is enough)
    for (std::size_t i = firstComputedLayer(altitude); i < m_basePressure.size(); ++i)
    {
        const Sample sample       = layerSample(i);
        m_basePressure[i]         = sample.pressure;
        m_baseRelativeHumidity[i] = sample.relativeHumidity;
    }
}

ExtendedIsaModel::Sample ExtendedIsaModel::layerSample(std::size_t layer) const noexcept
{
    // Only the layers below @p layer are read, so the result is the same during construction
    // (while the higher base pressures are still unset) and afterwards.
    return exactSample(geopotentialToGeometric(m_layer[layer] - 1));
}

Result<void> ExtendedIsaModel::validateSamples(double altitude) const
{
    const auto check = [](const Sample& sample) {
        return AtmosphericConditions::validate(sample.temperature, sample.pressure,
                                               sample.relativeHumidity);
    };

    // Java's constructor builds AtmosphericConditions from the sample under each layer it
    // computes, and throws IllegalArgumentException there (a pressure that underflows to 0).
    for (std::size_t i = firstComputedLayer(altitude); i < m_layer.size(); ++i)
    {
        if (const Result<void> checked = check(layerSample(i)); !checked.has_value())
        {
            return checked;
        }
    }

    // Java throws from the first getConditions(), which builds the table; checked here instead,
    // so that the model create() returns never throws from getConditions() (the extrapolated sea
    // level temperature of a very cold launch site is the table's first sample).
    for (const double tableAltitude : tableAltitudes())
    {
        if (const Result<void> checked = check(exactSample(tableAltitude)); !checked.has_value())
        {
            return checked;
        }
    }
    return {};
}

Result<std::unique_ptr<ExtendedIsaModel>> ExtendedIsaModel::create(double temperature,
                                                                   double pressure)
{
    return create(0, temperature, pressure, 0);
}

Result<std::unique_ptr<ExtendedIsaModel>> ExtendedIsaModel::create(double temperature,
                                                                   double pressure,
                                                                   double relativeHumidity)
{
    return create(0, temperature, pressure, relativeHumidity);
}

Result<std::unique_ptr<ExtendedIsaModel>> ExtendedIsaModel::create(double altitude,
                                                                   double temperature,
                                                                   double pressure,
                                                                   double relativeHumidity)
{
    // Java's constructor checks, in its order and with its messages.
    const double geopotentialAltitude = geometricToGeopotential(altitude);
    if (geopotentialAltitude >= kStandardLayers[1])
    {
        return fail(ErrorCode::INVALID_ARGUMENT,
                    "Too high first altitude: " + Strings::javaDoubleToString(altitude));
    }
    if (temperature <= 0)
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "Temperature must be positive (Kelvin)");
    }
    if (pressure <= 0)
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "Pressure must be positive (Pascals)");
    }
    if (relativeHumidity < 0 || relativeHumidity > 1)
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "Relative humidity must be between 0 and 1");
    }

    auto model = std::make_unique<ExtendedIsaModel>(Key{}, altitude, temperature, pressure,
                                                    relativeHumidity);
    if (const Result<void> checked = model->validateSamples(altitude); !checked.has_value())
    {
        return std::unexpected(checked.error());
    }
    return model;
}

double ExtendedIsaModel::getMaxAltitude() const
{
    return geopotentialToGeometric(m_layer.back());
}

AtmosphericConditions ExtendedIsaModel::getExactConditions(double altitude) const
{
    const Sample sample = exactSample(altitude);
    return AtmosphericConditions{sample.temperature, sample.pressure, sample.relativeHumidity};
}

ExtendedIsaModel::Sample ExtendedIsaModel::exactSample(double altitude) const noexcept
{
    // Clamp altitude to be within defined layers
    const double geopotentialAltitude =
        MathUtil::clamp(geometricToGeopotential(altitude), m_layer.front(), m_layer.back());

    // Find the correct layer. Java stops at the last index when the altitude is the top of the
    // table (or NaN) and then reads past its arrays; the last layer is used instead.
    std::size_t startLayer = 0;
    for (; startLayer < m_layer.size() - 1; ++startLayer)
    {
        if (m_layer[startLayer + 1] > geopotentialAltitude)
        {
            break;
        }
    }
    startLayer = std::min(startLayer, m_layer.size() - 2);

    const double altDiff = geopotentialAltitude - m_layer[startLayer];

    const double startTemp = m_baseTemperature[startLayer];
    // Temperature lapse rate
    const double tempRate = (m_baseTemperature[startLayer + 1] - startTemp) /
                            (m_layer[startLayer + 1] - m_layer[startLayer]);

    const double temp       = startTemp + (altDiff * tempRate);
    const double startPress = m_basePressure[startLayer];
    const double press =
        calculatePressure(geopotentialAltitude, temp, m_layer[startLayer], startTemp, startPress);
    const double startHumid = m_baseRelativeHumidity[startLayer];
    const double humid =
        calculateRelativeHumidity(geopotentialAltitude, m_layer[startLayer], startHumid);

    return Sample{.temperature = temp, .pressure = press, .relativeHumidity = humid};
}

}  // namespace QtRocket
