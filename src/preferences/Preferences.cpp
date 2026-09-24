#include "QtRocket/preferences/Preferences.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Chars.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

namespace Keys = PreferenceKeys;

constexpr double kPi     = std::numbers::pi;
constexpr double kHalfPi = std::numbers::pi / 2;

// ------------------------------------------------------------------- ISA conditions
//
// The standard ISA atmosphere as OpenRocket's ExtendedISAModel() gives it through
// InterpolatingAtmosphericModel.getConditions(): the exact conditions are computed every 500 m
// and interpolated linearly between. setLaunchAltitude() and setISAAtmosphere() store its
// temperature, pressure and humidity. Duplicated here until models/atmosphere is ported; then
// this becomes a call into that model.

struct IsaConditions
{
    double temperature;
    double pressure;
    double relativeHumidity;
};

/// Gravitational acceleration in m/s^2 (ExtendedISAModel.G).
constexpr double kIsaG = 9.80665;
/// Specific gas constant of dry air in J/(kg K) (AtmosphericConditions.R).
constexpr double kIsaR = 287.053;
/// ISA reference Earth radius in m (ExtendedISAModel.ISA_EARTH_RADIUS).
constexpr double kIsaEarthRadius = 6356766.0;
/// Interpolation layer thickness in m (InterpolatingAtmosphericModel.DELTA).
constexpr double kIsaDelta = 500;
/// Geopotential altitude in m where each ISA layer begins, and its base temperature in K
/// (ExtendedISAModel.STANDARD_LAYERS and STANDARD_TEMPERATURES).
constexpr std::array<double, 8> kIsaLayers{0, 11000, 20000, 32000, 47000, 51000, 71000, 84852};
constexpr std::array<double, 8> kIsaTemperatures{288.15, 216.65, 216.65, 228.65,
                                                 270.65, 270.65, 214.65, 186.95};

[[nodiscard]] double geometricToGeopotential(double geometricAltitude) noexcept
{
    return kIsaEarthRadius * geometricAltitude / (kIsaEarthRadius + geometricAltitude);
}

[[nodiscard]] double geopotentialToGeometric(double geopotentialAltitude) noexcept
{
    return kIsaEarthRadius * geopotentialAltitude / (kIsaEarthRadius - geopotentialAltitude);
}

/// ExtendedISAModel.calculatePressure: the pressure at geopotential altitude @p targetAltitude
/// (temperature @p targetTemperature) from the pressure @p basePressure at @p baseAltitude
/// (temperature @p baseTemperature), by the barometric formula.
[[nodiscard]] double isaPressure(double targetAltitude, double targetTemperature,
                                 double baseAltitude, double baseTemperature,
                                 double basePressure) noexcept
{
    if (baseAltitude == targetAltitude)
    {
        // Java: the lapse rate is 0.0 / 0.0 = NaN, which takes the isothermal branch below with
        // exp(-0) = 1 and gives the base pressure back.
        return basePressure;
    }
    const double altitudeDifference = baseAltitude - targetAltitude;
    const double tempRate           = (baseTemperature - targetTemperature) / altitudeDifference;
    if (std::abs(tempRate) > 0.000001)
    {
        // Non-isothermal case
        return basePressure / std::pow(1 + (altitudeDifference * tempRate / targetTemperature),
                                       -kIsaG / (tempRate * kIsaR));
    }
    // Isothermal case
    return basePressure / std::exp(-altitudeDifference * kIsaG / (kIsaR * targetTemperature));
}

/// The layer tables of ExtendedISAModel(): the base pressure and humidity of each layer follow
/// from the sea level values through the layers below it.
class IsaModel
{
public:
    IsaModel()
    {
        m_basePressure[0] = Preferences::kStandardPressure;
        m_baseHumidity[0] = Preferences::kStandardRelativeHumidity;
        for (std::size_t i = 1; i < kIsaLayers.size(); ++i)
        {
            const double        sampleAltitude = geopotentialToGeometric(kIsaLayers.at(i) - 1);
            const IsaConditions sample         = exactConditions(sampleAltitude);
            m_basePressure.at(i)               = sample.pressure;
            m_baseHumidity.at(i)               = sample.relativeHumidity;
        }
    }

    /// ExtendedISAModel.getExactConditions at a geometric altitude.
    [[nodiscard]] IsaConditions exactConditions(double altitude) const noexcept
    {
        const std::span<const double> layers = kIsaLayers;
        const std::span<const double> temps  = kIsaTemperatures;
        const std::span<const double> pressures{m_basePressure};
        const std::span<const double> humidities{m_baseHumidity};

        // Clamp altitude to be within defined layers
        const double geopotential =
            MathUtil::clamp(geometricToGeopotential(altitude), layers.front(), layers.back());

        // Find the correct layer
        std::size_t startLayer = 0;
        while (startLayer < layers.size() - 1 && layers[startLayer + 1] <= geopotential)
        {
            ++startLayer;
        }

        const double altDiff   = geopotential - layers[startLayer];
        const double startTemp = temps[startLayer];
        // Temperature lapse rate
        const double tempRate =
            (temps[startLayer + 1] - startTemp) / (layers[startLayer + 1] - layers[startLayer]);
        const double temp = startTemp + (altDiff * tempRate);
        const double press =
            isaPressure(geopotential, temp, layers[startLayer], startTemp, pressures[startLayer]);
        return {.temperature = temp, .pressure = press, .relativeHumidity = humidities[startLayer]};
    }

private:
    std::array<double, 8> m_basePressure{};
    std::array<double, 8> m_baseHumidity{};
};

/// InterpolatingAtmosphericModel.getConditions for the standard model.
[[nodiscard]] IsaConditions isaConditions(double altitude)
{
    if (std::isnan(altitude))
    {
        // Java: (int) floor(NaN) is 0 and the interpolation weight NaN, so every value is NaN.
        constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
        return {.temperature = kNaN, .pressure = kNaN, .relativeHumidity = kNaN};
    }
    // Java keeps the model (and its 172-level table) in the static ISA_ATMOSPHERIC_MODEL.
    static const IsaModel kModel;
    if (altitude <= 0)
    {
        return kModel.exactConditions(0);
    }
    // computeLayers(): ceil(max / DELTA) levels at multiples of DELTA from 0.
    const double maxAltitude = geopotentialToGeometric(kIsaLayers.back());
    const auto   size        = static_cast<std::size_t>(std::ceil(maxAltitude / kIsaDelta));
    const auto   maxIndex    = static_cast<double>(size - 1);
    if (altitude >= kIsaDelta * maxIndex)
    {
        return kModel.exactConditions(maxIndex * kIsaDelta);
    }
    const double        lowerIndex = std::floor(altitude / kIsaDelta);
    const double        fraction   = (altitude - (lowerIndex * kIsaDelta)) / kIsaDelta;
    const IsaConditions lower      = kModel.exactConditions(lowerIndex * kIsaDelta);
    const IsaConditions upper      = kModel.exactConditions((lowerIndex + 1) * kIsaDelta);
    return {
        .temperature = MathUtil::interpolate(lower.temperature, upper.temperature, fraction),
        .pressure    = MathUtil::interpolate(lower.pressure, upper.pressure, fraction),
        .relativeHumidity =
            MathUtil::interpolate(lower.relativeHumidity, upper.relativeHumidity, fraction),
    };
}

// ------------------------------------------------------------------------- tables

/// StaticFieldHolder.DEFAULT_LINE_STYLES.
constexpr std::array<std::pair<std::string_view, std::string_view>, 2> kDefaultLineStyles{
    {{"RocketComponent", "SOLID"}, {"MassObject", "DASHED"}}};

/// SwingPreferences.fillDefaultComponentColors() with UITheme.Themes.LIGHT's colours.
constexpr std::array<std::pair<std::string_view, std::string_view>, 10> kLightThemeColors{{
    {"BodyComponent", "0,0,240"},
    {"TubeFinSet", "0,0,200"},
    {"FinSet", "0,0,200"},
    {"LaunchLug", "0,0,180"},
    {"RailButton", "0,0,180"},
    {"InternalComponent", "170,0,100"},
    {"MassObject", "0,0,0"},
    {"RecoveryDevice", "255,0,0"},
    {"PodSet", "160,160,215"},
    {"ParallelStage", "198,163,184"},
}};

/// java.awt.Color.GRAY.
constexpr Color kAwtGray{128, 128, 128};

[[nodiscard]] bool isOneOf(std::span<const std::string_view> names, std::string_view name) noexcept
{
    return std::ranges::find(names, name) != names.end();
}

/// Whether @p id is in OpenRocket's UnitGroup.UNITS map, whose keys name the groups in the "units"
/// node: every group but SHAPE_PARAMETER and STABILITY_CALIBERS.
[[nodiscard]] constexpr bool isInUnitsMap(UnitGroupId id) noexcept
{
    return id != UnitGroupId::SHAPE_PARAMETER && id != UnitGroupId::STABILITY_CALIBERS;
}

/// The multi-level wind CSV unit getters: the unit of the process-wide group @p id named exactly
/// by @p stored, or nullptr when nothing is stored or the group has no such unit (Java throws
/// IllegalArgumentException for the latter; see the header).
[[nodiscard]] const Unit* storedUnit(const std::optional<std::string>& stored, UnitGroupId id)
{
    return stored.has_value() ? unitGroup(id).getUnit(*stored) : nullptr;
}

/// The text putDouble() stores: Java's spellings of the non-finite values, else the shortest
/// digits that read back as the same value.
[[nodiscard]] std::string doubleToStoredText(double value)
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    if (std::isinf(value))
    {
        return value < 0 ? "-Infinity" : "Infinity";
    }
    return std::format("{}", value);
}

}  // namespace

// ---------------------------------------------------------------- typed accessors

bool Preferences::getBoolean(std::string_view key, bool defaultValue) const
{
    const std::optional<std::string> value = get(key);
    if (!value.has_value())
    {
        return defaultValue;
    }
    // AbstractPreferences.getBoolean: "true" / "false" without regard to case, else the default.
    if (Strings::equalsIgnoreAsciiCase(*value, "true"))
    {
        return true;
    }
    if (Strings::equalsIgnoreAsciiCase(*value, "false"))
    {
        return false;
    }
    return defaultValue;
}

void Preferences::putBoolean(std::string_view key, bool value)
{
    put(key, value ? "true" : "false");
}

int Preferences::getInt(std::string_view key, int defaultValue) const
{
    const std::optional<std::string> value = get(key);
    if (!value.has_value())
    {
        return defaultValue;
    }
    return Strings::parseInt(*value).value_or(defaultValue);
}

void Preferences::putInt(std::string_view key, int value)
{
    put(key, std::format("{}", value));
}

double Preferences::getDouble(std::string_view key, double defaultValue) const
{
    const std::optional<std::string> value = get(key);
    if (!value.has_value())
    {
        return defaultValue;
    }
    // Double.parseDouble spells an infinity "Infinity" only; Strings::parseDouble also takes the
    // .ork spelling "Inf", which java.util.prefs would reject (NumberFormatException: the default).
    const std::string_view trimmed = Strings::trim(*value);
    if (trimmed == "Inf" || trimmed == "-Inf" || trimmed == "+Inf")
    {
        return defaultValue;
    }
    return Strings::parseDouble(*value).value_or(defaultValue);
}

void Preferences::putDouble(std::string_view key, double value)
{
    put(key, doubleToStoredText(value));
}

std::string Preferences::getString(std::string_view key, std::string_view defaultValue) const
{
    return get(key).value_or(std::string(defaultValue));
}

void Preferences::putString(std::string_view key, std::optional<std::string_view> value)
{
    if (value.has_value())
    {
        put(key, *value);
    }
    else
    {
        remove(key);
    }
}

std::optional<std::string> Preferences::getInNode(std::string_view directory,
                                                  std::string_view key) const
{
    const Preferences* const node = findNode(directory);
    if (node == nullptr)
    {
        return std::nullopt;
    }
    return node->get(key);
}

std::string Preferences::getString(std::string_view directory, std::string_view key,
                                   std::string_view defaultValue) const
{
    return getInNode(directory, key).value_or(std::string(defaultValue));
}

void Preferences::putString(std::string_view directory, std::string_view key,
                            std::optional<std::string_view> value)
{
    getNode(directory).putString(key, value);
}

// ------------------------------------------------------------------ generic helpers

int Preferences::getChoice(std::string_view key, int max, int defaultValue) const
{
    const int value = getInt(key, defaultValue);
    if (value < 0 || value > max)
    {
        return defaultValue;
    }
    return value;
}

double Preferences::getChoice(std::string_view key, double max, double defaultValue) const
{
    const double value = getDouble(key, defaultValue);
    if (value < 0 || value > max)
    {
        return defaultValue;
    }
    return value;
}

void Preferences::putChoice(std::string_view key, int value)
{
    putInt(key, value);
}

std::string Preferences::getEnumName(std::string_view key, std::span<const std::string_view> names,
                                     std::string_view defaultName) const
{
    const std::optional<std::string> value = get(key);
    if (!value.has_value() || !isOneOf(names, *value))
    {
        return std::string(defaultName);
    }
    return *value;
}

void Preferences::putEnumName(std::string_view key, std::optional<std::string_view> name)
{
    putString(key, name);
}

Color Preferences::getColor(std::string_view key, const Color& defaultValue) const
{
    const std::optional<std::string> text = get(key);
    if (!text.has_value())
    {
        return defaultValue;
    }
    return parseColor(*text).value_or(defaultValue);
}

void Preferences::putColor(std::string_view key, const Color& value)
{
    put(key, stringifyColor(value));
}

std::optional<Color> Preferences::parseColor(std::string_view text)
{
    const std::vector<std::string> rgb = Strings::splitJava(text, ',');
    if (rgb.size() != 3)
    {
        return std::nullopt;
    }
    const std::optional<int> red   = Strings::parseInt(rgb[0]);
    const std::optional<int> green = Strings::parseInt(rgb[1]);
    const std::optional<int> blue  = Strings::parseInt(rgb[2]);
    if (!red.has_value() || !green.has_value() || !blue.has_value())
    {
        return std::nullopt;
    }
    return Color(MathUtil::clamp(*red, 0, 255), MathUtil::clamp(*green, 0, 255),
                 MathUtil::clamp(*blue, 0, 255));
}

std::string Preferences::stringifyColor(const Color& color)
{
    return std::format("{},{},{}", color.red(), color.green(), color.blue());
}

std::optional<std::string> Preferences::lookup(std::string_view    directory,
                                               ComponentClassChain classChain,
                                               ComponentDefaults   defaults) const
{
    // Search preferences
    for (const std::string_view className : classChain)
    {
        std::optional<std::string> value = getInNode(directory, className);
        if (value.has_value())
        {
            return value;
        }
    }
    // Search defaults
    for (const std::string_view className : classChain)
    {
        const auto entry = std::ranges::find(defaults, className,
                                             &std::pair<std::string_view, std::string_view>::first);
        if (entry != defaults.end())
        {
            return std::string(entry->second);
        }
    }
    return std::nullopt;
}

void Preferences::setDoubleIfChanged(std::string_view key, double defaultValue, double value)
{
    if (MathUtil::equals(getDouble(key, defaultValue), value))
    {
        return;
    }
    putDouble(key, value);
    fireChanged();
}

void Preferences::setBoolIfChanged(std::string_view key, bool defaultValue, bool value)
{
    if (getBoolean(key, defaultValue) == value)
    {
        return;
    }
    putBoolean(key, value);
    fireChanged();
}

void Preferences::setIntIfChanged(std::string_view key, int defaultValue, int value)
{
    if (getInt(key, defaultValue) == value)
    {
        return;
    }
    putInt(key, value);
    fireChanged();
}

// ------------------------------------------------------------- welcome dialog, updates

void Preferences::setIgnoreWelcome(std::string_view version, bool ignore)
{
    putBoolean(std::format("{}_{}", Keys::kIgnoreWelcome, version), ignore);
}

bool Preferences::getIgnoreWelcome(std::string_view version) const
{
    return getBoolean(std::format("{}_{}", Keys::kIgnoreWelcome, version), false);
}

bool Preferences::getCheckUpdates() const
{
    return getBoolean(Keys::kCheckUpdates, true);
}

void Preferences::setCheckUpdates(bool check)
{
    putBoolean(Keys::kCheckUpdates, check);
}

bool Preferences::isUpdateCheckPermissionSet() const
{
    return get(Keys::kUpdateCheckPermission).has_value();
}

void Preferences::setUpdateCheckPermission(bool allowed)
{
    setCheckUpdates(allowed);
    setCheckMotorDatabaseUpdates(allowed);
    putBoolean(Keys::kUpdateCheckPermission, allowed);
}

std::vector<std::string> Preferences::getIgnoreUpdateVersions() const
{
    return Strings::splitJava(getString(Keys::kIgnoreUpdateVersions, ""), '\n');
}

void Preferences::setIgnoreUpdateVersions(std::span<const std::string> versions)
{
    putString(Keys::kIgnoreUpdateVersions, Strings::join("\n", versions));
}

bool Preferences::getCheckBetaUpdates() const
{
    return getBoolean(Keys::kCheckBetaUpdates, true);
}

void Preferences::setCheckBetaUpdates(bool check)
{
    putBoolean(Keys::kCheckBetaUpdates, check);
}

bool Preferences::getCheckMotorDatabaseUpdates() const
{
    return getBoolean(Keys::kCheckMotorDatabaseUpdates, true);
}

void Preferences::setCheckMotorDatabaseUpdates(bool check)
{
    putBoolean(Keys::kCheckMotorDatabaseUpdates, check);
}

bool Preferences::getAutoInstallMotorDatabaseUpdates() const
{
    return getBoolean(Keys::kAutoInstallMotorDatabaseUpdates, false);
}

void Preferences::setAutoInstallMotorDatabaseUpdates(bool autoInstall)
{
    putBoolean(Keys::kAutoInstallMotorDatabaseUpdates, autoInstall);
}

std::vector<std::string> Preferences::getIgnoreMotorDatabaseUpdateVersions() const
{
    return Strings::splitJava(getString(Keys::kIgnoreMotorDatabaseUpdateVersions, ""), '\n');
}

void Preferences::setIgnoreMotorDatabaseUpdateVersions(std::span<const std::string> versions)
{
    putString(Keys::kIgnoreMotorDatabaseUpdateVersions, Strings::join("\n", versions));
}

// -------------------------------------------------------------------- 3D interaction

bool Preferences::shouldReduceEffectsDuring3DInteraction() const
{
    return getBoolean(Keys::kOpenglReduceEffectsDuringInteraction, false);
}

void Preferences::setReduceEffectsDuring3DInteraction(bool enabled)
{
    setBoolIfChanged(Keys::kOpenglReduceEffectsDuringInteraction, false, enabled);
}

bool Preferences::isMsaaEnabled() const
{
    return getBoolean(Keys::kOpenglEnableMsaa, true);
}

void Preferences::setMsaaEnabled(bool enabled)
{
    setBoolIfChanged(Keys::kOpenglEnableMsaa, true, enabled);
}

// ---------------------------------------------------------------------- units, files

bool Preferences::isDisplaySecondaryStability() const
{
    return getBoolean(Keys::kDisplaySecondaryStability, true);
}

void Preferences::setDisplaySecondaryStability(bool check)
{
    putBoolean(Keys::kDisplaySecondaryStability, check);
}

void Preferences::loadDefaultUnits() const
{
    const Preferences* const node = findNode(Keys::kUnitsNode);
    if (node == nullptr)
    {
        return;
    }
    for (const std::string& key : node->keys())
    {
        const std::optional<UnitGroupId> id = unitGroupFromName(key);
        if (!id.has_value() || !isInUnitsMap(*id))
        {
            continue;
        }
        const std::optional<std::string> unitName = node->get(key);
        if (unitName.has_value())
        {
            // False for a name the group lacks, which Java ignores as well.
            unitGroup(*id).setDefaultUnit(std::string_view{*unitName});
        }
    }
}

void Preferences::storeDefaultUnits()
{
    Preferences& node = getNode(Keys::kUnitsNode);
    for (const UnitGroupId id : kAllUnitGroupIds)
    {
        const UnitGroup& group = unitGroup(id);
        if (!isInUnitsMap(id) || group.getUnitCount() < 2)
        {
            continue;
        }
        node.put(unitGroupName(id), group.getDefaultUnit().getUnit());
    }
}

std::optional<std::filesystem::path> Preferences::getDefaultDirectory() const
{
    const std::optional<std::string> file = get(Keys::kDefaultDirectory);
    if (!file.has_value())
    {
        return std::nullopt;
    }
    // Stored in UTF-8 (see setDefaultDirectory()), so it is read back through char8_t, which
    // path converts from UTF-8 on every platform.
    return std::filesystem::path(std::u8string(file->begin(), file->end()));
}

void Preferences::setDefaultDirectory(const std::optional<std::filesystem::path>& directory)
{
    if (!directory.has_value())
    {
        remove(Keys::kDefaultDirectory);
        return;
    }
    // File.getAbsolutePath(): resolved against the current directory (an empty path is the
    // current directory itself, which std::filesystem::absolute("") rejects), never failing; the
    // path is stored as given when the current directory cannot be determined.
    std::error_code       error;
    std::filesystem::path absolute = directory->empty()
                                         ? std::filesystem::current_path(error)
                                         : std::filesystem::absolute(*directory, error);
    if (error)
    {
        absolute = *directory;
    }
    // The UTF-8 form: path::string() would go through the Windows ANSI code page and turn any
    // character outside it into '?'.
    const std::u8string utf8 = absolute.u8string();
    put(Keys::kDefaultDirectory, std::string(utf8.begin(), utf8.end()));
}

// ------------------------------------------------------------- flight configurations

std::string Preferences::getDefaultFlightConfigName() const
{
    return getString(Keys::kDefaultFlightConfigName, kDefaultConfigName);
}

void Preferences::setDefaultFlightConfigName(std::string_view name)
{
    putString(Keys::kDefaultFlightConfigName, name);
}

// ------------------------------------------------------------------ simulation flags

bool Preferences::getConfirmSimDeletion() const
{
    return getBoolean(Keys::kConfirmDeleteSimulation, true);
}

void Preferences::setConfirmSimDeletion(bool check)
{
    putBoolean(Keys::kConfirmDeleteSimulation, check);
}

bool Preferences::getAutoRunSimulations() const
{
    return getBoolean(Keys::kAutoRunSimulations, false);
}

void Preferences::setAutoRunSimulations(bool check)
{
    putBoolean(Keys::kAutoRunSimulations, check);
}

bool Preferences::getLaunchIntoWind() const
{
    return getBoolean(Keys::kLaunchIntoWind, true);
}

void Preferences::setLaunchIntoWind(bool check)
{
    putBoolean(Keys::kLaunchIntoWind, check);
}

bool Preferences::getShowRasaeroFormatWarning() const
{
    return getBoolean(Keys::kShowRasaeroFormatWarning, true);
}

void Preferences::setShowRasaeroFormatWarning(bool check)
{
    putBoolean(Keys::kShowRasaeroFormatWarning, check);
}

bool Preferences::getShowRocksimFormatWarning() const
{
    return getBoolean(Keys::kShowRocksimFormatWarning, true);
}

void Preferences::setShowRocksimFormatWarning(bool check)
{
    putBoolean(Keys::kShowRocksimFormatWarning, check);
}

bool Preferences::getExportUserDirectories() const
{
    return getBoolean(Keys::kExportUserDirectories, false);
}

void Preferences::setExportUserDirectories(bool check)
{
    putBoolean(Keys::kExportUserDirectories, check);
}

bool Preferences::getExportWindowInformation() const
{
    return getBoolean(Keys::kExportWindowInformation, false);
}

void Preferences::setExportWindowInformation(bool check)
{
    putBoolean(Keys::kExportWindowInformation, check);
}

// --------------------------------------------------------- simulation defaults (SI)

double Preferences::getDefaultMach() const
{
    return getChoice(Keys::kDefaultMachNumber, 0.9, 0.3);
}

void Preferences::setDefaultMach(double mach)
{
    const double oldMach = getChoice(Keys::kDefaultMachNumber, 0.9, 0.3);
    if (MathUtil::equals(oldMach, mach))
    {
        return;
    }
    putDouble(Keys::kDefaultMachNumber, mach);
    fireChanged();
}

double Preferences::getLaunchRodLength() const
{
    return getDouble(Keys::kLaunchRodLength, 1);
}

void Preferences::setLaunchRodLength(double launchRodLength)
{
    setDoubleIfChanged(Keys::kLaunchRodLength, 1, launchRodLength);
}

double Preferences::getLaunchRodAngle() const
{
    return getDouble(Keys::kLaunchRodAngle, 0);
}

void Preferences::setLaunchRodAngle(double launchRodAngle)
{
    // Use the same limit as SimulationOptions, since the launch preferences and a simulation's
    // launch conditions are edited with the same panel.
    const double clamped = MathUtil::clamp(launchRodAngle, -kMaxLaunchRodAngle, kMaxLaunchRodAngle);
    setDoubleIfChanged(Keys::kLaunchRodAngle, 0, clamped);
}

double Preferences::getLaunchRodDirection()
{
    if (getLaunchIntoWind())
    {
        // When launching into wind, sync the launch rod direction with wind direction
        const double windDirection = getDouble(Keys::kWindDirection, kHalfPi);
        setLaunchRodDirection(windDirection);
        return windDirection;
    }
    return getDouble(Keys::kLaunchRodDirection, kHalfPi);
}

void Preferences::setLaunchRodDirection(double launchRodDirection)
{
    setDoubleIfChanged(Keys::kLaunchRodDirection, kHalfPi, MathUtil::reduce2Pi(launchRodDirection));
}

double Preferences::getWindAverage() const
{
    return getDouble(Keys::kWindAverage, 2.0);
}

void Preferences::setWindAverage(double average)
{
    // PinkNoiseWindModel.setAverage(): a negative speed is its magnitude the other way round, and
    // the turbulence intensity is kept, which is what the stored intensity does by itself (also
    // across a zero average, where the model's deviation would lose it: see Preferences.h).
    if (average < 0)
    {
        average = -average;
        setWindDirection(kPi + getWindDirection());
    }
    if (average == getWindAverage())
    {
        return;
    }
    putDouble(Keys::kWindAverage, average);
    fireChanged();
}

double Preferences::getWindTurbulenceIntensity() const
{
    return getDouble(Keys::kWindTurbulence, 0.1);
}

void Preferences::setWindTurbulenceIntensity(double intensity)
{
    // PinkNoiseWindModel.setStandardDeviation() keeps max(deviation, 0), so the intensity it
    // derives is never negative either (a NaN stays NaN, as with Java's Math.max).
    intensity = std::max(intensity, 0.0);
    if (intensity == getWindTurbulenceIntensity())
    {
        return;
    }
    putDouble(Keys::kWindTurbulence, intensity);
    fireChanged();
}

double Preferences::getWindStandardDeviation() const
{
    // PinkNoiseWindModel.setTurbulenceIntensity(): deviation = intensity * average.
    return getWindTurbulenceIntensity() * getWindAverage();
}

void Preferences::setWindStandardDeviation(double standardDeviation)
{
    standardDeviation = std::max(standardDeviation, 0.0);
    // PinkNoiseWindModel.getTurbulenceIntensity()
    const double average   = getWindAverage();
    double       intensity = 0;
    if (MathUtil::equals(average, 0))
    {
        intensity = MathUtil::equals(standardDeviation, 0) ? 0 : 1;
    }
    else
    {
        intensity = standardDeviation / average;
    }
    setWindTurbulenceIntensity(intensity);
}

double Preferences::getWindDirection() const
{
    return getDouble(Keys::kWindDirection, kHalfPi);
}

void Preferences::setWindDirection(double direction)
{
    const double reduced = MathUtil::reduce2Pi(direction);
    if (reduced == getWindDirection())
    {
        return;
    }
    putDouble(Keys::kWindDirection, reduced);
    fireChanged();
}

double Preferences::getLaunchAltitude() const
{
    return getDouble(Keys::kLaunchAltitude, 0);
}

void Preferences::setLaunchAltitude(double altitude)
{
    if (MathUtil::equals(getDouble(Keys::kLaunchAltitude, 0), altitude))
    {
        return;
    }
    putDouble(Keys::kLaunchAltitude, altitude);

    // Update the launch temperature and pressure if using ISA
    if (isIsaAtmosphere())
    {
        storeIsaConditions();
    }

    fireChanged();
}

void Preferences::storeIsaConditions()
{
    const IsaConditions conditions = isaConditions(getLaunchAltitude());
    setLaunchTemperature(conditions.temperature);
    setLaunchPressure(conditions.pressure);
    setLaunchRelativeHumidity(conditions.relativeHumidity);
}

double Preferences::getLaunchLatitude() const
{
    return getDouble(Keys::kLaunchLatitude, 28.61);
}

void Preferences::setLaunchLatitude(double launchLatitude)
{
    setDoubleIfChanged(Keys::kLaunchLatitude, 28.61, MathUtil::clamp(launchLatitude, -90, 90));
}

double Preferences::getLaunchLongitude() const
{
    return getDouble(Keys::kLaunchLongitude, -80.60);
}

void Preferences::setLaunchLongitude(double launchLongitude)
{
    setDoubleIfChanged(Keys::kLaunchLongitude, -80.60, MathUtil::clamp(launchLongitude, -180, 180));
}

double Preferences::getLaunchTemperature() const
{
    return getDouble(Keys::kLaunchTemperature, kStandardTemperature);
}

void Preferences::setLaunchTemperature(double launchTemperature)
{
    setDoubleIfChanged(Keys::kLaunchTemperature, kStandardTemperature, launchTemperature);
}

double Preferences::getLaunchPressure() const
{
    return getDouble(Keys::kLaunchPressure, kStandardPressure);
}

void Preferences::setLaunchPressure(double launchPressure)
{
    setDoubleIfChanged(Keys::kLaunchPressure, kStandardPressure, launchPressure);
}

double Preferences::getLaunchRelativeHumidity() const
{
    return getDouble(Keys::kLaunchRelativeHumidity, kStandardRelativeHumidity);
}

void Preferences::setLaunchRelativeHumidity(double launchHumidity)
{
    setDoubleIfChanged(Keys::kLaunchRelativeHumidity, kStandardRelativeHumidity, launchHumidity);
}

bool Preferences::isIsaAtmosphere() const
{
    return getBoolean(Keys::kLaunchUseIsa, true);
}

void Preferences::setIsaAtmosphere(bool isa)
{
    if (getBoolean(Keys::kLaunchUseIsa, true) == isa)
    {
        return;
    }
    putBoolean(Keys::kLaunchUseIsa, isa);

    // Update the launch temperature and pressure
    if (isa)
    {
        storeIsaConditions();
    }

    fireChanged();
}

std::string Preferences::getGeodeticComputationName() const
{
    return getEnumName(Keys::kGeodeticComputation, kGeodeticComputationNames, "SPHERICAL");
}

void Preferences::setGeodeticComputationName(std::string_view name)
{
    QTROCKET_ASSERT(isOneOf(kGeodeticComputationNames, name));
    putEnumName(Keys::kGeodeticComputation, name);
}

std::string Preferences::getGravityModelName() const
{
    return getEnumName(Keys::kGravityModel, kGravityModelNames, "WGS");
}

void Preferences::setGravityModelName(std::string_view name)
{
    QTROCKET_ASSERT(isOneOf(kGravityModelNames, name));
    putEnumName(Keys::kGravityModel, name);
}

double Preferences::getConstantGravityValue() const
{
    return getDouble(Keys::kConstantGravityValue, 9.807);
}

void Preferences::setConstantGravityValue(double value)
{
    setDoubleIfChanged(Keys::kConstantGravityValue, 9.807, value);
}

std::string Preferences::getSimulationStepperMethodName() const
{
    return getEnumName(Keys::kSimulationStepperMethod, kSimulationStepperMethodNames, "RK4");
}

void Preferences::setSimulationStepperMethodName(std::string_view name)
{
    QTROCKET_ASSERT(isOneOf(kSimulationStepperMethodNames, name));
    putEnumName(Keys::kSimulationStepperMethod, name);
}

int Preferences::getRandomSeed() const
{
    return getInt(Keys::kSimulationRandomSeed, 0);
}

void Preferences::setRandomSeed(int randomSeed)
{
    setIntIfChanged(Keys::kSimulationRandomSeed, 0, randomSeed);
}

bool Preferences::isRandomSeedFixed() const
{
    return getBoolean(Keys::kSimulationRandomSeedFixed, false);
}

void Preferences::setRandomSeedFixed(bool randomSeedFixed)
{
    setBoolIfChanged(Keys::kSimulationRandomSeedFixed, false, randomSeedFixed);
}

double Preferences::getTimeStep() const
{
    return getDouble(Keys::kSimulationTimeStep, kRecommendedTimeStep);
}

void Preferences::setTimeStep(double timeStep)
{
    setDoubleIfChanged(Keys::kSimulationTimeStep, kRecommendedTimeStep, timeStep);
}

double Preferences::getMaxSimulationTime() const
{
    const double maxTime = getDouble(Keys::kSimulationMaxTime, kRecommendedMaxTime);
    return maxTime == 0 ? kRecommendedMaxTime : maxTime;
}

void Preferences::setMaxSimulationTime(double maxTime)
{
    setDoubleIfChanged(Keys::kSimulationMaxTime, kRecommendedMaxTime, maxTime);
}

double Preferences::getRecoverySpeedWarning() const
{
    return getDouble(Keys::kRecoverySpeedWarning, 20.0);
}

void Preferences::setRecoverySpeedWarning(double value)
{
    setDoubleIfChanged(Keys::kRecoverySpeedWarning, 20.0, value);
}

double Preferences::getDrogueLowSpeedWarning() const
{
    return getDouble(Keys::kDrogueLowSpeedWarning, 3.048);
}

void Preferences::setDrogueLowSpeedWarning(double value)
{
    setDoubleIfChanged(Keys::kDrogueLowSpeedWarning, 3.048, value);
}

double Preferences::getRecoveryDrogueMainHighSpeedWarning() const
{
    return getDouble(Keys::kRecoveryDrogueMainHighSpeedWarning, 30.48);
}

void Preferences::setRecoveryDrogueMainHighSpeedWarning(double value)
{
    setDoubleIfChanged(Keys::kRecoveryDrogueMainHighSpeedWarning, 30.48, value);
}

double Preferences::getRecoveryDrogueMainLowSpeedWarning() const
{
    return getDouble(Keys::kRecoveryDrogueMainLowSpeedWarning, 15.24);
}

void Preferences::setRecoveryDrogueMainLowSpeedWarning(double value)
{
    setDoubleIfChanged(Keys::kRecoveryDrogueMainLowSpeedWarning, 15.24, value);
}

// ------------------------------------------------------------------- GUI behaviour

void Preferences::setAutoOpenLastDesignOnStartup(bool enabled)
{
    putBoolean(Keys::kAutoOpenLastDesign, enabled);
}

bool Preferences::isAutoOpenLastDesignOnStartupEnabled() const
{
    return getBoolean(Keys::kAutoOpenLastDesign, false);
}

void Preferences::setAlwaysOpenLeftmostTab(bool enabled)
{
    putBoolean(Keys::kOpenLeftmostDesignTab, enabled);
}

bool Preferences::isAlwaysOpenLeftmostTab() const
{
    return getBoolean(Keys::kOpenLeftmostDesignTab, false);
}

bool Preferences::isShowDiscardConfirmation() const
{
    return getBoolean(Keys::kShowDiscardConfirmation, true);
}

void Preferences::setShowDiscardConfirmation(bool enabled)
{
    putBoolean(Keys::kShowDiscardConfirmation, enabled);
}

bool Preferences::isShowSaveRocketInfo() const
{
    return getBoolean(Keys::kShowSaveRocketInfo, true);
}

void Preferences::setShowSaveRocketInfo(bool enabled)
{
    putBoolean(Keys::kShowSaveRocketInfo, enabled);
}

bool Preferences::isShowDiscardSimulationConfirmation() const
{
    return getBoolean(Keys::kShowDiscardSimulationConfirmation, true);
}

void Preferences::setShowDiscardSimulationConfirmation(bool enabled)
{
    putBoolean(Keys::kShowDiscardSimulationConfirmation, enabled);
}

bool Preferences::isShowDiscardPreferencesConfirmation() const
{
    return getBoolean(Keys::kShowDiscardPreferencesConfirmation, true);
}

void Preferences::setShowDiscardPreferencesConfirmation(bool enabled)
{
    putBoolean(Keys::kShowDiscardPreferencesConfirmation, enabled);
}

void Preferences::setShowMarkers(bool enabled)
{
    putBoolean(Keys::kShowMarkers, enabled);
}

bool Preferences::isShowMarkers() const
{
    return getBoolean(Keys::kShowMarkers, false);
}

void Preferences::setMatchForeDiameter(bool enabled)
{
    putBoolean(Keys::kMatchForeDiameter, enabled);
}

bool Preferences::isMatchForeDiameter() const
{
    return getBoolean(Keys::kMatchForeDiameter, true);
}

void Preferences::setMatchAftDiameter(bool enabled)
{
    putBoolean(Keys::kMatchAftDiameter, enabled);
}

bool Preferences::isMatchAftDiameter() const
{
    return getBoolean(Keys::kMatchAftDiameter, true);
}

bool Preferences::getMotorNameColumn() const
{
    return getBoolean(Keys::kMotorNameColumn, true);
}

void Preferences::setMotorNameColumn(bool value)
{
    putBoolean(Keys::kMotorNameColumn, value);
}

// ------------------------------------------------ per component class defaults

LineStyle Preferences::getDefaultLineStyle(ComponentClassChain classChain) const
{
    const std::optional<std::string> value =
        lookup(Keys::kComponentStyleNode, classChain, defaultLineStyles());
    if (!value.has_value())
    {
        // Java: LineStyle.valueOf(null) throws, and the catch gives SOLID.
        return LineStyle::SOLID;
    }
    return lineStyleFromString(*value).value_or(LineStyle::SOLID);
}

void Preferences::setDefaultLineStyle(std::string_view className, LineStyle style)
{
    putString(Keys::kComponentStyleNode, className, lineStyleName(style));
}

ComponentDefaults Preferences::defaultLineStyles() noexcept
{
    return kDefaultLineStyles;
}

std::optional<std::string> Preferences::getDefaultComponentMaterialString(
    ComponentClassChain classChain) const
{
    return lookup(Keys::kComponentMaterialsNode, classChain, {});
}

void Preferences::setDefaultComponentMaterialString(std::string_view                className,
                                                    std::optional<std::string_view> storableString)
{
    putString(Keys::kComponentMaterialsNode, className, storableString);
}

Color Preferences::getDefaultColor(ComponentClassChain classChain) const
{
    const std::optional<std::string> color =
        lookup(Keys::kComponentColorsNode, classChain, defaultComponentColors());
    if (!color.has_value())
    {
        return Color::black();
    }
    return parseColor(*color).value_or(Color::black());
}

void Preferences::setDefaultColor(std::string_view className, const Color& color)
{
    putString(Keys::kComponentColorsNode, className, stringifyColor(color));
}

ComponentDefaults Preferences::defaultComponentColors() const noexcept
{
    return kLightThemeColors;
}

// --------------------------------------------------------------------- SVG export

Color Preferences::getSvgStrokeColor() const
{
    return getColor(Keys::kSvgStrokeColor, Color::black());
}

void Preferences::setSvgStrokeColor(const Color& color)
{
    putColor(Keys::kSvgStrokeColor, color);
}

double Preferences::getSvgStrokeWidth() const
{
    return getDouble(Keys::kSvgStrokeWidth, 0.1);
}

void Preferences::setSvgStrokeWidth(double width)
{
    putDouble(Keys::kSvgStrokeWidth, width);
}

bool Preferences::isSvgDrawCrosshair() const
{
    return getBoolean(Keys::kSvgDrawCrosshair, false);
}

void Preferences::setSvgDrawCrosshair(bool drawCrosshair)
{
    putBoolean(Keys::kSvgDrawCrosshair, drawCrosshair);
}

Color Preferences::getSvgCrosshairColor() const
{
    return getColor(Keys::kSvgCrosshairColor, kAwtGray);
}

void Preferences::setSvgCrosshairColor(const Color& color)
{
    putColor(Keys::kSvgCrosshairColor, color);
}

double Preferences::getSvgCrosshairSize() const
{
    return getDouble(Keys::kSvgCrosshairSize, 2.0);  // Default 2mm
}

void Preferences::setSvgCrosshairSize(double size)
{
    putDouble(Keys::kSvgCrosshairSize, size);
}

bool Preferences::isSvgShowLabels() const
{
    return getBoolean(Keys::kSvgShowLabels, true);
}

void Preferences::setSvgShowLabels(bool showLabels)
{
    putBoolean(Keys::kSvgShowLabels, showLabels);
}

Color Preferences::getSvgLabelColor() const
{
    return getColor(Keys::kSvgLabelColor, Color::black());
}

void Preferences::setSvgLabelColor(const Color& color)
{
    putColor(Keys::kSvgLabelColor, color);
}

double Preferences::getSvgPartSpacing() const
{
    return getDouble(Keys::kSvgPartSpacing, 0.01);  // Default 10mm
}

void Preferences::setSvgPartSpacing(double spacing)
{
    putDouble(Keys::kSvgPartSpacing, spacing);
}

// -------------------------------------------------------------- texture generation

double Preferences::getTextureGenerationDpi() const
{
    return getDouble(Keys::kTextureGenerationDpi, 300);
}

void Preferences::setTextureGenerationDpi(double dpi)
{
    putDouble(Keys::kTextureGenerationDpi, dpi);
}

bool Preferences::isTextureGenerationDrawOutline() const
{
    return getBoolean(Keys::kTextureGenerationDrawOutline, true);
}

void Preferences::setTextureGenerationDrawOutline(bool drawOutline)
{
    putBoolean(Keys::kTextureGenerationDrawOutline, drawOutline);
}

int Preferences::getTextureGenerationOutlinePx() const
{
    return getInt(Keys::kTextureGenerationOutlinePx, 1);
}

void Preferences::setTextureGenerationOutlinePx(int outlinePx)
{
    putInt(Keys::kTextureGenerationOutlinePx, std::max(0, outlinePx));
}

bool Preferences::isTextureGenerationResetTransforms() const
{
    return getBoolean(Keys::kTextureGenerationResetTransforms, true);
}

void Preferences::setTextureGenerationResetTransforms(bool reset)
{
    putBoolean(Keys::kTextureGenerationResetTransforms, reset);
}

Color Preferences::getTextureGenerationOutlineColor() const
{
    return getColor(Keys::kTextureGenerationOutlineColor, Color(0, 0, 0));
}

void Preferences::setTextureGenerationOutlineColor(const Color& color)
{
    putColor(Keys::kTextureGenerationOutlineColor, color);
}

// ------------------------------------------------------ multi-level wind CSV import

bool Preferences::isMultiLevelWindCsvImportHeader() const
{
    return getBoolean(Keys::kMultiLevelWindCsvImportHasHeader, true);
}

void Preferences::setMultiLevelWindCsvImportHeader(bool hasHeader)
{
    putBoolean(Keys::kMultiLevelWindCsvImportHasHeader, hasHeader);
}

std::string Preferences::getMultiLevelWindCsvImportAltitudeColumn() const
{
    return getString(Keys::kMultiLevelWindCsvImportAltitudeColumn, "altitude");
}

void Preferences::setMultiLevelWindCsvImportAltitudeColumn(std::string_view columnName)
{
    putString(Keys::kMultiLevelWindCsvImportAltitudeColumn, columnName);
}

int Preferences::getMultiLevelWindCsvImportAltitudeColumnIndex() const
{
    return getInt(Keys::kMultiLevelWindCsvImportAltitudeColumnIndex, 0);
}

void Preferences::setMultiLevelWindCsvImportAltitudeColumnIndex(int columnIndex)
{
    putInt(Keys::kMultiLevelWindCsvImportAltitudeColumnIndex, columnIndex);
}

const Unit& Preferences::getMultiLevelWindCsvImportAltitudeUnit() const
{
    const Unit* const unit =
        storedUnit(get(Keys::kMultiLevelWindCsvImportAltitudeUnit), UnitGroupId::DISTANCE);
    return unit != nullptr ? *unit : unitGroup(UnitGroupId::DISTANCE).getSIUnit();
}

void Preferences::setMultiLevelWindCsvImportAltitudeUnit(const Unit& unit)
{
    putString(Keys::kMultiLevelWindCsvImportAltitudeUnit, unit.getUnit());
}

std::string Preferences::getMultiLevelWindCsvImportSpeedColumn() const
{
    return getString(Keys::kMultiLevelWindCsvImportSpeedColumn, "speed");
}

void Preferences::setMultiLevelWindCsvImportSpeedColumn(std::string_view columnName)
{
    putString(Keys::kMultiLevelWindCsvImportSpeedColumn, columnName);
}

int Preferences::getMultiLevelWindCsvImportSpeedColumnIndex() const
{
    return getInt(Keys::kMultiLevelWindCsvImportSpeedColumnIndex, 1);
}

void Preferences::setMultiLevelWindCsvImportSpeedColumnIndex(int columnIndex)
{
    putInt(Keys::kMultiLevelWindCsvImportSpeedColumnIndex, columnIndex);
}

const Unit& Preferences::getMultiLevelWindCsvImportSpeedUnit() const
{
    const Unit* const unit =
        storedUnit(get(Keys::kMultiLevelWindCsvImportSpeedUnit), UnitGroupId::WINDSPEED);
    return unit != nullptr ? *unit : unitGroup(UnitGroupId::WINDSPEED).getSIUnit();
}

void Preferences::setMultiLevelWindCsvImportSpeedUnit(const Unit& unit)
{
    putString(Keys::kMultiLevelWindCsvImportSpeedUnit, unit.getUnit());
}

std::string Preferences::getMultiLevelWindCsvImportDirectionColumn() const
{
    return getString(Keys::kMultiLevelWindCsvImportDirectionColumn, "direction");
}

void Preferences::setMultiLevelWindCsvImportDirectionColumn(std::string_view columnName)
{
    putString(Keys::kMultiLevelWindCsvImportDirectionColumn, columnName);
}

int Preferences::getMultiLevelWindCsvImportDirectionColumnIndex() const
{
    return getInt(Keys::kMultiLevelWindCsvImportDirectionColumnIndex, 2);
}

void Preferences::setMultiLevelWindCsvImportDirectionColumnIndex(int columnIndex)
{
    putInt(Keys::kMultiLevelWindCsvImportDirectionColumnIndex, columnIndex);
}

const Unit& Preferences::getMultiLevelWindCsvImportDirectionUnit() const
{
    const Unit* const unit =
        storedUnit(get(Keys::kMultiLevelWindCsvImportDirectionUnit), UnitGroupId::ANGLE);
    if (unit != nullptr)
    {
        return *unit;
    }
    // Java: new DegreeUnit(), a unit equal to the ANGLE group's degrees.
    const Unit* const degrees = unitGroup(UnitGroupId::ANGLE).getUnit(Chars::kDegree);
    QTROCKET_ASSERT(degrees != nullptr);
    return *degrees;
}

void Preferences::setMultiLevelWindCsvImportDirectionUnit(const Unit& unit)
{
    putString(Keys::kMultiLevelWindCsvImportDirectionUnit, unit.getUnit());
}

std::string Preferences::getMultiLevelWindCsvImportStddevColumn() const
{
    return getString(Keys::kMultiLevelWindCsvImportStddevColumn, "stddev");
}

void Preferences::setMultiLevelWindCsvImportStddevColumn(std::string_view columnName)
{
    putString(Keys::kMultiLevelWindCsvImportStddevColumn, columnName);
}

int Preferences::getMultiLevelWindCsvImportStddevColumnIndex() const
{
    return getInt(Keys::kMultiLevelWindCsvImportStddevColumnIndex, 3);
}

void Preferences::setMultiLevelWindCsvImportStddevColumnIndex(int columnIndex)
{
    putInt(Keys::kMultiLevelWindCsvImportStddevColumnIndex, columnIndex);
}

const Unit& Preferences::getMultiLevelWindCsvImportStddevUnit() const
{
    const Unit* const unit =
        storedUnit(get(Keys::kMultiLevelWindCsvImportStddevUnit), UnitGroupId::WINDSPEED);
    return unit != nullptr ? *unit : unitGroup(UnitGroupId::WINDSPEED).getSIUnit();
}

void Preferences::setMultiLevelWindCsvImportStddevUnit(const Unit& unit)
{
    putString(Keys::kMultiLevelWindCsvImportStddevUnit, unit.getUnit());
}

}  // namespace QtRocket
