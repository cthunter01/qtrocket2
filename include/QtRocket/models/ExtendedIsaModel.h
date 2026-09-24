#pragma once

#include <array>
#include <memory>
#include <vector>

#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/models/InterpolatingAtmosphericModel.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/ModId.h"

namespace QtRocket
{

/// The International Standard Atmosphere, optionally fitted to measured launch site conditions
/// (OpenRocket's models/atmosphere/ExtendedISAModel). The layers are defined over geopotential
/// altitude; getConditions() takes a geometric altitude (m above sea level) and converts it with
/// the ISA reference Earth radius.
///
/// The seven ISA layers, from each base (geopotential altitude, temperature):
///   0 m 288.15 K (-6.5 K/km), 11 km 216.65 K (isothermal), 20 km 216.65 K (+1.0 K/km),
///   32 km 228.65 K (+2.8 K/km), 47 km 270.65 K (isothermal), 51 km 270.65 K (-2.8 K/km),
///   71 km 214.65 K (-2.0 K/km), and 186.95 K from 84.852 km up (the table ends there).
/// The temperature is linear within a layer and the pressure follows the barometric formula
/// (the isothermal form when the lapse rate is below 1e-6 K/m); the relative humidity is carried
/// up unchanged (OpenRocket has a TODO to vary it). The base pressure of each layer is the
/// pressure 1 m below the layer's base, computed from the layer below.
///
/// A launch site above sea level adds a layer at the site's (geopotential) altitude with the
/// measured temperature and pressure; the temperature lapse rate from there to the 11 km layer is
/// extended down to sea level to give the sea level temperature, and the pressure there follows
/// from the barometric formula. At or below sea level the measurements are taken as the sea level
/// values. OpenRocket notes that above 32 km the values differ from standard tables by about 5%.
///
/// The conditions are interpolated from a 500 m table (InterpolatingAtmosphericModel), and the
/// model never changes after construction, so modId() is always ModId::zero().
///
/// Error policy: the launch conditions come from the user (the simulation options, a .ork file),
/// so where Java's constructor throws IllegalArgumentException the create() factories return an
/// Error with Java's message: a launch site at or above the 11 km layer ("Too high first
/// altitude: <altitude>"), a temperature or pressure that is not positive, a humidity outside
/// 0 ... 1. create() also refuses a launch site whose extrapolated sea level temperature is not
/// positive (a very cold site high up, such as 50 K at 5 km), with "Temperature must be positive
/// (Kelvin)": Java accepts the model and throws that message from its first getConditions() call.
/// A NaN passes every check, as in Java.
///
/// Deviations from OpenRocket:
/// - The constructors that can fail are the create() factories, which return the model through a
///   unique_ptr (the model is neither copyable nor movable, see InterpolatingAtmosphericModel).
/// - getExactConditions() at exactly the top of the table uses the last layer; Java indexes past
///   its tables there (unreachable through getConditions(), whose table stops below the top).
/// - main(String[]), a development harness with no output, is not ported.
class ExtendedIsaModel final : public InterpolatingAtmosphericModel
{
    /// Restricts the public constructor below to this class (make_unique needs it public).
    struct Key
    {
        explicit Key() = default;
    };

public:
    /// Standard sea level temperature in K (STANDARD_TEMPERATURE).
    static constexpr double kStandardTemperature = 288.15;
    /// Standard sea level pressure in Pa (STANDARD_PRESSURE).
    static constexpr double kStandardPressure = 101325;
    /// Standard relative humidity (STANDARD_RELATIVE_HUMIDITY).
    static constexpr double kStandardRelativeHumidity = 0;

    /// The standard ISA model (sea level 288.15 K, 101325 Pa, dry air).
    ExtendedIsaModel();

    /// For create() only (the Key cannot be named elsewhere); the arguments must have passed
    /// create()'s checks.
    ExtendedIsaModel(Key key, double altitude, double temperature, double pressure,
                     double relativeHumidity);

    ~ExtendedIsaModel() override = default;

    ExtendedIsaModel(const ExtendedIsaModel&)            = delete;
    ExtendedIsaModel& operator=(const ExtendedIsaModel&) = delete;
    ExtendedIsaModel(ExtendedIsaModel&&)                 = delete;
    ExtendedIsaModel& operator=(ExtendedIsaModel&&)      = delete;

    /// A model with @p temperature (K) and @p pressure (Pa) at sea level and dry air.
    [[nodiscard]] static Result<std::unique_ptr<ExtendedIsaModel>> create(double temperature,
                                                                          double pressure);

    /// A model with @p temperature (K), @p pressure (Pa) and @p relativeHumidity (0 ... 1) at sea
    /// level.
    [[nodiscard]] static Result<std::unique_ptr<ExtendedIsaModel>> create(double temperature,
                                                                          double pressure,
                                                                          double relativeHumidity);

    /// A model with the conditions measured at the geometric @p altitude (m) of the launch site,
    /// which must lie below the 11 km layer (getMaximumAllowedAltitude()). Below the site the
    /// model extrapolates the lapse rate between the site and 11 km (see the class comment).
    /// Fails with ErrorCode::INVALID_ARGUMENT as the class comment lists.
    [[nodiscard]] static Result<std::unique_ptr<ExtendedIsaModel>> create(double altitude,
                                                                          double temperature,
                                                                          double pressure,
                                                                          double relativeHumidity);

    /// The highest geometric launch site altitude a model can be fitted to: 1 m (geopotential)
    /// below the 11 km layer, 11018.064... m.
    [[nodiscard]] static constexpr double getMaximumAllowedAltitude() noexcept
    {
        return geopotentialToGeometric(kStandardLayers[1] - 1);
    }

    /// Always ModId::zero(): the model is immutable.
    [[nodiscard]] ModId modId() const noexcept override { return ModId::zero(); }

protected:
    /// The geometric altitude of the top of the table (84.852 km geopotential, about 86 km).
    [[nodiscard]] double getMaxAltitude() const override;

    /// The exact conditions at the geometric @p altitude (m), clamped to the layer table.
    /// @throws BugError when the values are out of range for AtmosphericConditions (only for
    ///         launch conditions far outside nature, e.g. a pressure that underflows to 0).
    [[nodiscard]] AtmosphericConditions getExactConditions(double altitude) const override;

private:
    /// ISA reference Earth radius in m for the geopotential conversion (ISA_EARTH_RADIUS).
    static constexpr double kIsaEarthRadius = 6356766.0;

    /// Where each ISA layer begins, geopotential m (STANDARD_LAYERS).
    static constexpr std::array<double, 8> kStandardLayers{0,     11000, 20000, 32000,
                                                           47000, 51000, 71000, 84852};
    /// The base temperature of each ISA layer, K (STANDARD_TEMPERATURES).
    static constexpr std::array<double, 8> kStandardTemperatures{288.15, 216.65, 216.65, 228.65,
                                                                 270.65, 270.65, 214.65, 186.95};

    /// The exact conditions without the range checks of AtmosphericConditions.
    struct Sample
    {
        double temperature;
        double pressure;
        double relativeHumidity;
    };

    [[nodiscard]] static constexpr double geometricToGeopotential(double geometricAltitude) noexcept
    {
        return kIsaEarthRadius * geometricAltitude / (kIsaEarthRadius + geometricAltitude);
    }

    [[nodiscard]] static constexpr double geopotentialToGeometric(
        double geopotentialAltitude) noexcept
    {
        return kIsaEarthRadius * geopotentialAltitude / (kIsaEarthRadius - geopotentialAltitude);
    }

    /// getExactConditions() without constructing AtmosphericConditions (the constructor needs the
    /// values before the table is complete).
    [[nodiscard]] Sample exactSample(double altitude) const noexcept;

    std::vector<double> m_layer;                 ///< geopotential altitude of each layer, m
    std::vector<double> m_baseTemperature;       ///< K
    std::vector<double> m_basePressure;          ///< Pa
    std::vector<double> m_baseRelativeHumidity;  ///< 0 ... 1
};

}  // namespace QtRocket
