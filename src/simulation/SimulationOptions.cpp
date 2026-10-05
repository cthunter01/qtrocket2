#include "QtRocket/simulation/SimulationOptions.h"

#include <array>
#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <numbers>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "QtRocket/aero/lookup/CsvMachAoALookup.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/models/AtmosphericModel.h"
#include "QtRocket/models/ExtendedIsaModel.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/SimulationOptionsInterface.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Signal.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

// The launch preferences clamp the rod angle to the limit of the simulation options.
static_assert(SimulationOptions::kMaxLaunchRodAngle == Preferences::kMaxLaunchRodAngle);

/// AbstractRKSimulationStepper.RECOMMENDED_ANGLE_STEP, the default maximum step angle, rad.
constexpr double kRecommendedAngleStep = 3 * std::numbers::pi / 180;

/// The limits of the maximum step angle, rad: 1 and 20 degrees, written as in Java.
constexpr double kMinimumStepAngle = 1 * std::numbers::pi / 180;
constexpr double kMaximumStepAngle = 20 * std::numbers::pi / 180;

/// The ISA standard atmosphere (ISA_ATMOSPHERIC_MODEL), shared by every SimulationOptions.
[[nodiscard]] const std::shared_ptr<const ExtendedIsaModel>& isaAtmosphericModel()
{
    static const std::shared_ptr<const ExtendedIsaModel> kModel =
        std::make_shared<const ExtendedIsaModel>();
    return kModel;
}

/// DRAG_VALUE_COLUMNS: the column a drag table must have.
[[nodiscard]] const std::array<std::string, 1>& dragValueColumns()
{
    static const std::array<std::string, 1> kColumns{"cd"};
    return kColumns;
}

/// STABILITY_VALUE_COLUMNS: the columns a stability table must have.
[[nodiscard]] const std::array<std::string, 3>& stabilityValueColumns()
{
    static const std::array<std::string, 3> kColumns{"cn", "cm", "cp"};
    return kColumns;
}

/// new Random().nextInt(): a nondeterministic seed (the int conversion of the 32 random bits is
/// modular).
[[nodiscard]] int drawRandomSeed()
{
    return static_cast<int>(std::random_device{}());
}

/// ApplicationPreferences.getGravityModel(): the type whose constant name the store holds.
[[nodiscard]] GravityModelType gravityModelOf(const Preferences& preferences)
{
    const std::string name = preferences.getGravityModelName();
    for (const GravityModelType type : kAllGravityModelTypes)
    {
        if (gravityModelTypeName(type) == name)
        {
            return type;
        }
    }
    return GravityModelType::WGS;  // not reached: the store only gives constant names
}

/// Java's normalizePath(): Path.toAbsolutePath().normalize(), which resolves the path against
/// the current directory and removes "." and ".." lexically, without touching the file system.
[[nodiscard]] std::optional<std::filesystem::path> normalizePath(
    const std::optional<std::filesystem::path>& path)
{
    if (!path.has_value())
    {
        return std::nullopt;
    }
    // An empty path is the current directory itself, which std::filesystem::absolute("") rejects;
    // the path is kept as given when the current directory cannot be determined.
    std::error_code       error;
    std::filesystem::path absolute = path->empty() ? std::filesystem::current_path(error)
                                                   : std::filesystem::absolute(*path, error);
    if (error)
    {
        absolute = *path;
    }
    std::filesystem::path normal = absolute.lexically_normal();
    // A Java Path never ends in a separator; lexically_normal() keeps the one of "a/b/".
    if (!normal.has_filename() && normal.has_relative_path())
    {
        normal = normal.parent_path();
    }
    return normal;
}

/// The table read from the CSV file @p path (null without a path), as the two
/// set...LookupCsvPath() methods read it.
[[nodiscard]] Result<std::shared_ptr<const MachAoALookup>> readLookup(
    const std::optional<std::filesystem::path>& path, std::span<const std::string> valueColumns)
{
    if (!path.has_value())
    {
        return std::shared_ptr<const MachAoALookup>();
    }
    Result<MachAoALookup> table = CsvMachAoALookup::fromCsv(*path, valueColumns);
    if (!table.has_value())
    {
        return std::unexpected(std::move(table.error()));
    }
    return std::make_shared<const MachAoALookup>(std::move(*table));
}

/// copyConditionsFrom()'s step for one field: `if (this.x != src.x) { isChanged = true; this.x =
/// src.x; }`, with Java's != (so a NaN always differs).
template <typename T>
void copyIfDifferent(T& field, const T& source, bool& isChanged)
{
    if (field != source)
    {
        isChanged = true;
        field     = source;
    }
}

/// GeodeticComputationStrategy.toString(), its translated name: the English texts of the keys
/// GeodeticComputationStrategy.<strategy>.name.
[[nodiscard]] std::string_view geodeticComputationName(
    GeodeticComputationStrategy strategy) noexcept
{
    switch (strategy)
    {
        case GeodeticComputationStrategy::FLAT:
            return "Flat Earth";
        case GeodeticComputationStrategy::SPHERICAL:
            return "Spherical approximation";
        case GeodeticComputationStrategy::WGS84:
            return "WGS84 ellipsoid";
    }
    return "Spherical approximation";  // not reached: the switch covers every strategy
}

/// String.format("%f", value).
[[nodiscard]] std::string fixed(double value)
{
    return Strings::formatFixed(value, 6);
}

}  // namespace

SimulationOptions::SimulationOptions() : SimulationOptions(InMemoryPreferences(), nullptr) { }

SimulationOptions::SimulationOptions(Preferences& preferences)
  : SimulationOptions(preferences, &preferences)
{
}

SimulationOptions::SimulationOptions(const Preferences& source, Preferences* store)
  : m_preferences(store),
    // Java reads these four keys directly, not through the typed getters (whose
    // getLaunchRodDirection() would store the wind direction when launching into the wind).
    m_launchRodLength(source.getDouble(PreferenceKeys::kLaunchRodLength, 1)),
    m_launchIntoWind(source.getBoolean(PreferenceKeys::kLaunchIntoWind, true)),
    m_launchRodAngle(source.getDouble(PreferenceKeys::kLaunchRodAngle, 0)),
    m_launchRodDirection(
        source.getDouble(PreferenceKeys::kLaunchRodDirection, std::numbers::pi / 2)),
    m_launchAltitude(source.getLaunchAltitude()),
    m_launchLatitude(source.getLaunchLatitude()),
    m_launchLongitude(source.getLaunchLongitude()),
    m_useIsa(source.isIsaAtmosphere()),
    m_launchTemperature(source.getLaunchTemperature()),
    m_launchPressure(source.getLaunchPressure()),
    m_launchRelativeHumidity(source.getLaunchRelativeHumidity()),
    m_timeStep(source.getTimeStep()),
    m_maxSimulationTime(source.getMaxSimulationTime()),
    m_maximumAngle(kRecommendedAngleStep),
    m_randomSeed(drawRandomSeed()),
    m_averageWindModel(std::make_unique<PinkNoiseWindModel>(m_randomSeed)),
    m_multiLevelPinkNoiseWindModel(std::make_unique<MultiLevelPinkNoiseWindModel>(source)),
    m_gravityModelType(gravityModelOf(source)),
    m_constantGravity(source.getConstantGravityValue())
{
    connectWindModels();
}

SimulationOptions::SimulationOptions(const SimulationOptions& other)
  : SimulationOptionsInterface(other),
    // The copy owns its listeners, including relays from its cloned wind models.
    m_preferences(other.m_preferences),
    m_launchRodLength(other.m_launchRodLength),
    m_launchIntoWind(other.m_launchIntoWind),
    m_launchRodAngle(other.m_launchRodAngle),
    m_launchRodDirection(other.m_launchRodDirection),
    m_launchAltitude(other.m_launchAltitude),
    m_launchLatitude(other.m_launchLatitude),
    m_launchLongitude(other.m_launchLongitude),
    m_geodeticComputation(other.m_geodeticComputation),
    m_useIsa(other.m_useIsa),
    m_launchTemperature(other.m_launchTemperature),
    m_launchPressure(other.m_launchPressure),
    m_launchRelativeHumidity(other.m_launchRelativeHumidity),
    m_timeStep(other.m_timeStep),
    m_maxSimulationTime(other.m_maxSimulationTime),
    m_maximumAngle(other.m_maximumAngle),
    m_randomSeed(other.m_randomSeed),
    m_randomSeedFixed(other.m_randomSeedFixed),
    m_windModelType(other.m_windModelType),
    // Deep clone the wind models
    m_averageWindModel(std::make_unique<PinkNoiseWindModel>(*other.m_averageWindModel)),
    m_multiLevelPinkNoiseWindModel(
        std::make_unique<MultiLevelPinkNoiseWindModel>(*other.m_multiLevelPinkNoiseWindModel)),
    m_gravityModelType(other.m_gravityModelType),
    m_constantGravity(other.m_constantGravity),
    m_stepperMethodChoice(other.m_stepperMethodChoice),
    m_recoverySpeedWarning(other.m_recoverySpeedWarning),
    m_drogueLowSpeedWarning(other.m_drogueLowSpeedWarning),
    m_recoveryDrogueMainHighSpeedWarning(other.m_recoveryDrogueMainHighSpeedWarning),
    m_recoveryDrogueMainLowSpeedWarning(other.m_recoveryDrogueMainLowSpeedWarning),
    m_dragLookupCsvPath(other.m_dragLookupCsvPath),
    m_stabilityLookupCsvPath(other.m_stabilityLookupCsvPath),
    m_dragLookupTable(other.m_dragLookupTable),
    m_stabilityLookupTable(other.m_stabilityLookupTable),
    m_dragLookupCsvRows(other.m_dragLookupCsvRows),
    m_stabilityLookupCsvRows(other.m_stabilityLookupCsvRows)
{
    connectWindModels();
}

void SimulationOptions::connectWindModels()
{
    // The slots point at the signal, not at this object, which may be moved.
    const Signal<>* const signal = m_changed.get();
    m_averageWindModel->changed().connect([signal] { signal->emit(); });
    m_multiLevelPinkNoiseWindModel->changed().connect([signal] { signal->emit(); });
}

void SimulationOptions::setLaunchRodLength(double launchRodLength)
{
    if (MathUtil::equals(m_launchRodLength, launchRodLength))
    {
        return;
    }
    m_launchRodLength = launchRodLength;
    fireChangeEvent();
}

void SimulationOptions::setLaunchIntoWind(bool launchIntoWind)
{
    if (m_launchIntoWind == launchIntoWind)
    {
        return;
    }
    m_launchIntoWind = launchIntoWind;
    fireChangeEvent();
}

void SimulationOptions::setLaunchRodAngle(double launchRodAngle)
{
    launchRodAngle = MathUtil::clamp(launchRodAngle, -kMaxLaunchRodAngle, kMaxLaunchRodAngle);
    if (MathUtil::equals(m_launchRodAngle, launchRodAngle))
    {
        return;
    }
    m_launchRodAngle = launchRodAngle;
    fireChangeEvent();
}

double SimulationOptions::getLaunchRodDirection() const
{
    if (m_launchIntoWind)
    {
        double windDirection = 0;
        if (m_windModelType == WindModelType::AVERAGE)
        {
            windDirection = m_averageWindModel->getDirection();
        }
        else
        {
            windDirection = m_multiLevelPinkNoiseWindModel->getWindDirection(0, m_launchAltitude);
        }
        return MathUtil::reduce2Pi(windDirection);
    }
    return m_launchRodDirection;
}

void SimulationOptions::setLaunchRodDirection(double launchRodDirection)
{
    launchRodDirection = MathUtil::reduce2Pi(launchRodDirection);
    if (MathUtil::equals(m_launchRodDirection, launchRodDirection))
    {
        return;
    }
    m_launchRodDirection = launchRodDirection;
    fireChangeEvent();
}

void SimulationOptions::setWindModelType(WindModelType windModelType)
{
    if (m_windModelType != windModelType)
    {
        m_windModelType = windModelType;
        fireChangeEvent();
    }
}

WindModel& SimulationOptions::getWindModel()
{
    switch (m_windModelType)
    {
        case WindModelType::AVERAGE:
            return *m_averageWindModel;
        case WindModelType::MULTI_LEVEL:
            return *m_multiLevelPinkNoiseWindModel;
    }
    bug("Unknown wind model type");
}

const WindModel& SimulationOptions::getWindModel() const
{
    switch (m_windModelType)
    {
        case WindModelType::AVERAGE:
            return *m_averageWindModel;
        case WindModelType::MULTI_LEVEL:
            return *m_multiLevelPinkNoiseWindModel;
    }
    bug("Unknown wind model type");
}

// Deprecated wind methods to keep compatibility with old plugins (e.g. the original multi-level
// wind code)

double SimulationOptions::getWindSpeedAverage()
{
    setWindModelType(WindModelType::AVERAGE);
    return m_averageWindModel->getAverage();
}

void SimulationOptions::setWindSpeedAverage(double windAverage)
{
    setWindModelType(WindModelType::AVERAGE);
    m_averageWindModel->setAverage(windAverage);
}

double SimulationOptions::getWindSpeedDeviation()
{
    setWindModelType(WindModelType::AVERAGE);
    return m_averageWindModel->getStandardDeviation();
}

void SimulationOptions::setWindSpeedDeviation(double windDeviation)
{
    setWindModelType(WindModelType::AVERAGE);
    m_averageWindModel->setStandardDeviation(windDeviation);
}

double SimulationOptions::getWindTurbulenceIntensity()
{
    setWindModelType(WindModelType::AVERAGE);
    return m_averageWindModel->getTurbulenceIntensity();
}

void SimulationOptions::setWindTurbulenceIntensity(double intensity)
{
    setWindModelType(WindModelType::AVERAGE);
    m_averageWindModel->setTurbulenceIntensity(intensity);
}

double SimulationOptions::getWindDirection()
{
    setWindModelType(WindModelType::AVERAGE);
    return m_averageWindModel->getDirection();
}

void SimulationOptions::setWindDirection(double direction)
{
    setWindModelType(WindModelType::AVERAGE);
    m_averageWindModel->setDirection(direction);
}

void SimulationOptions::setGravityModelType(GravityModelType gravityModelType)
{
    if (m_gravityModelType != gravityModelType)
    {
        m_gravityModelType = gravityModelType;
        fireChangeEvent();
    }
}

void SimulationOptions::setConstantGravity(double constantGravity)
{
    if (MathUtil::equals(m_constantGravity, constantGravity))
    {
        return;
    }
    m_constantGravity = constantGravity;
    fireChangeEvent();
}

void SimulationOptions::setLaunchAltitude(double altitude)
{
    if (MathUtil::equals(m_launchAltitude, altitude))
    {
        return;
    }
    m_launchAltitude = MathUtil::min(altitude, ExtendedIsaModel::getMaximumAllowedAltitude());

    // Update the launch temperature, pressure and humidity if using ISA
    if (m_useIsa)
    {
        applyIsaConditions();
    }

    fireChangeEvent();
}

void SimulationOptions::applyIsaConditions()
{
    // One lookup per value, as in Java: each setter may run a listener that moves the launch
    // site, and the next value is then the ISA's at the new altitude.
    const ExtendedIsaModel& isa = *isaAtmosphericModel();
    setLaunchTemperature(isa.getConditions(getLaunchAltitude()).getTemperature());
    setLaunchPressure(isa.getConditions(getLaunchAltitude()).getPressure());
    setLaunchRelativeHumidity(isa.getConditions(getLaunchAltitude()).getRelativeHumidity());
}

void SimulationOptions::setLaunchLatitude(double launchLatitude)
{
    launchLatitude = MathUtil::clamp(launchLatitude, -90, 90);
    if (MathUtil::equals(m_launchLatitude, launchLatitude))
    {
        return;
    }
    m_launchLatitude = launchLatitude;
    fireChangeEvent();
}

void SimulationOptions::setLaunchLongitude(double launchLongitude)
{
    launchLongitude = MathUtil::clamp(launchLongitude, -180, 180);
    if (MathUtil::equals(m_launchLongitude, launchLongitude))
    {
        return;
    }
    m_launchLongitude = launchLongitude;
    fireChangeEvent();
}

void SimulationOptions::setGeodeticComputation(GeodeticComputationStrategy geodeticComputation)
{
    if (m_geodeticComputation == geodeticComputation)
    {
        return;
    }
    m_geodeticComputation = geodeticComputation;
    fireChangeEvent();
}

void SimulationOptions::setSimulationStepperMethodChoice(SimulationStepperMethod choice)
{
    if (m_preferences != nullptr)
    {
        m_preferences->setSimulationStepperMethodName(simulationStepperMethodName(choice));
    }
    m_stepperMethodChoice = choice;
    fireChangeEvent();
}

void SimulationOptions::setIsaAtmosphere(bool isa)
{
    if (isa == m_useIsa)
    {
        return;
    }
    m_useIsa = isa;

    // Update the launch temperature, pressure and humidity
    if (isa)
    {
        applyIsaConditions();
    }

    fireChangeEvent();
}

void SimulationOptions::setLaunchTemperature(double launchTemperature)
{
    if (MathUtil::equals(m_launchTemperature, launchTemperature))
    {
        return;
    }
    m_launchTemperature = launchTemperature;
    fireChangeEvent();
}

void SimulationOptions::setLaunchPressure(double launchPressure)
{
    if (MathUtil::equals(m_launchPressure, launchPressure))
    {
        return;
    }
    m_launchPressure = launchPressure;
    fireChangeEvent();
}

void SimulationOptions::setLaunchRelativeHumidity(double launchHumidity)
{
    if (MathUtil::equals(m_launchRelativeHumidity, launchHumidity))
    {
        return;
    }
    m_launchRelativeHumidity = launchHumidity;
    fireChangeEvent();
}

Result<std::shared_ptr<const AtmosphericModel>> SimulationOptions::getAtmosphericModel() const
{
    if (m_useIsa)
    {
        return std::shared_ptr<const AtmosphericModel>(isaAtmosphericModel());
    }
    Result<std::unique_ptr<ExtendedIsaModel>> model = ExtendedIsaModel::create(
        getLaunchAltitude(), m_launchTemperature, m_launchPressure, m_launchRelativeHumidity);
    if (!model.has_value())
    {
        return std::unexpected(std::move(model.error()));
    }
    return std::shared_ptr<const AtmosphericModel>(std::move(*model));
}

void SimulationOptions::setTimeStep(double timeStep)
{
    if (MathUtil::equals(m_timeStep, timeStep))
    {
        return;
    }
    m_timeStep = timeStep;
    fireChangeEvent();
}

void SimulationOptions::setMaxSimulationTime(double maxSimulationTime)
{
    if (MathUtil::equals(m_maxSimulationTime, maxSimulationTime))
    {
        return;
    }
    m_maxSimulationTime = maxSimulationTime;
    fireChangeEvent();
}

void SimulationOptions::setMaximumStepAngle(double maximumAngle)
{
    maximumAngle = MathUtil::clamp(maximumAngle, kMinimumStepAngle, kMaximumStepAngle);
    if (MathUtil::equals(m_maximumAngle, maximumAngle))
    {
        return;
    }
    m_maximumAngle = maximumAngle;
    fireChangeEvent();
}

void SimulationOptions::setRecoverySpeedWarning(double recoverySpeedWarning)
{
    if (MathUtil::equals(m_recoverySpeedWarning, recoverySpeedWarning))
    {
        return;
    }
    m_recoverySpeedWarning = recoverySpeedWarning;
    fireChangeEvent();
}

void SimulationOptions::setDrogueLowSpeedWarning(double drogueLowSpeedWarning)
{
    if (MathUtil::equals(m_drogueLowSpeedWarning, drogueLowSpeedWarning))
    {
        return;
    }
    m_drogueLowSpeedWarning = drogueLowSpeedWarning;
    fireChangeEvent();
}

void SimulationOptions::setRecoveryDrogueMainHighSpeedWarning(
    double recoveryDrogueMainHighSpeedWarning)
{
    if (MathUtil::equals(m_recoveryDrogueMainHighSpeedWarning, recoveryDrogueMainHighSpeedWarning))
    {
        return;
    }
    m_recoveryDrogueMainHighSpeedWarning = recoveryDrogueMainHighSpeedWarning;
    fireChangeEvent();
}

void SimulationOptions::setRecoveryDrogueMainLowSpeedWarning(
    double recoveryDrogueMainLowSpeedWarning)
{
    if (MathUtil::equals(m_recoveryDrogueMainLowSpeedWarning, recoveryDrogueMainLowSpeedWarning))
    {
        return;
    }
    m_recoveryDrogueMainLowSpeedWarning = recoveryDrogueMainLowSpeedWarning;
    fireChangeEvent();
}

Result<void> SimulationOptions::setDragLookupCsvPath(
    const std::optional<std::filesystem::path>& csvPath)
{
    std::optional<std::filesystem::path>         normalized = normalizePath(csvPath);
    Result<std::shared_ptr<const MachAoALookup>> table = readLookup(normalized, dragValueColumns());
    if (!table.has_value())
    {
        return std::unexpected(std::move(table.error()));
    }
    updateDragLookup(std::move(normalized), std::move(*table));
    return {};
}

void SimulationOptions::setDragLookup(const std::optional<std::filesystem::path>& csvPath,
                                      std::shared_ptr<const MachAoALookup>        table)
{
    setDragLookup(csvPath, std::move(table), std::nullopt);
}

void SimulationOptions::setDragLookup(const std::optional<std::filesystem::path>& csvPath,
                                      std::shared_ptr<const MachAoALookup>        table,
                                      std::optional<std::vector<std::string>>     csvRows)
{
    std::optional<std::filesystem::path> normalized = normalizePath(csvPath);
    if (normalized.has_value() && table == nullptr)
    {
        bug("table must not be null when csvPath is provided");
    }
    m_dragLookupCsvRows = std::move(csvRows);
    updateDragLookup(std::move(normalized), std::move(table));
}

void SimulationOptions::clearDragLookup()
{
    m_dragLookupCsvRows = std::nullopt;
    updateDragLookup(std::nullopt, nullptr);
}

Result<void> SimulationOptions::setStabilityLookupCsvPath(
    const std::optional<std::filesystem::path>& csvPath)
{
    std::optional<std::filesystem::path>         normalized = normalizePath(csvPath);
    Result<std::shared_ptr<const MachAoALookup>> table =
        readLookup(normalized, stabilityValueColumns());
    if (!table.has_value())
    {
        return std::unexpected(std::move(table.error()));
    }
    updateStabilityLookup(std::move(normalized), std::move(*table));
    return {};
}

void SimulationOptions::setStabilityLookup(const std::optional<std::filesystem::path>& csvPath,
                                           std::shared_ptr<const MachAoALookup>        table)
{
    setStabilityLookup(csvPath, std::move(table), std::nullopt);
}

void SimulationOptions::setStabilityLookup(const std::optional<std::filesystem::path>& csvPath,
                                           std::shared_ptr<const MachAoALookup>        table,
                                           std::optional<std::vector<std::string>>     csvRows)
{
    std::optional<std::filesystem::path> normalized = normalizePath(csvPath);
    if (normalized.has_value() && table == nullptr)
    {
        bug("table must not be null when csvPath is provided");
    }
    m_stabilityLookupCsvRows = std::move(csvRows);
    updateStabilityLookup(std::move(normalized), std::move(table));
}

void SimulationOptions::clearStabilityLookup()
{
    m_stabilityLookupCsvRows = std::nullopt;
    updateStabilityLookup(std::nullopt, nullptr);
}

void SimulationOptions::updateDragLookup(std::optional<std::filesystem::path> path,
                                         std::shared_ptr<const MachAoALookup> table)
{
    const bool changed  = m_dragLookupCsvPath != path || m_dragLookupTable != table;
    m_dragLookupCsvPath = std::move(path);
    m_dragLookupTable   = std::move(table);
    if (changed)
    {
        fireChangeEvent();
    }
}

void SimulationOptions::updateStabilityLookup(std::optional<std::filesystem::path> path,
                                              std::shared_ptr<const MachAoALookup> table)
{
    const bool changed       = m_stabilityLookupCsvPath != path || m_stabilityLookupTable != table;
    m_stabilityLookupCsvPath = std::move(path);
    m_stabilityLookupTable   = std::move(table);
    if (changed)
    {
        fireChangeEvent();
    }
}

void SimulationOptions::setRandomSeed(int randomSeed)
{
    if (m_randomSeed == randomSeed)
    {
        return;
    }
    m_randomSeed = randomSeed;
    // Automatically generated seeds do not invalidate existing results, while a user-edited fixed
    // seed does.
    if (m_randomSeedFixed)
    {
        fireChangeEvent();
    }
}

void SimulationOptions::setRandomSeedFixed(bool randomSeedFixed)
{
    if (m_randomSeedFixed == randomSeedFixed)
    {
        return;
    }
    m_randomSeedFixed = randomSeedFixed;
    fireChangeEvent();
}

void SimulationOptions::randomizeSeed()
{
    setRandomSeed(drawRandomSeed());
}

void SimulationOptions::randomizeSeedIfNotFixed()
{
    if (!m_randomSeedFixed)
    {
        randomizeSeed();
    }
}

void SimulationOptions::copyConditionsFrom(const SimulationOptions& src)
{
    // Be a little smart about triggering the change event.
    // only do it if one of the "important" (user specified) parameters has really changed.
    bool isChanged             = false;
    bool averageWindChanged    = false;
    bool multiLevelWindChanged = false;

    copyIfDifferent(m_windModelType, src.m_windModelType, isChanged);
    if (!(*m_averageWindModel == *src.m_averageWindModel))
    {
        isChanged          = true;
        averageWindChanged = true;
        m_averageWindModel->loadFrom(*src.m_averageWindModel);
    }
    if (!(*m_multiLevelPinkNoiseWindModel == *src.m_multiLevelPinkNoiseWindModel))
    {
        isChanged             = true;
        multiLevelWindChanged = true;
        m_multiLevelPinkNoiseWindModel->loadFrom(*src.m_multiLevelPinkNoiseWindModel);
    }

    copyIfDifferent(m_gravityModelType, src.m_gravityModelType, isChanged);
    copyIfDifferent(m_constantGravity, src.m_constantGravity, isChanged);

    copyIfDifferent(m_launchAltitude, src.m_launchAltitude, isChanged);
    copyIfDifferent(m_launchLatitude, src.m_launchLatitude, isChanged);
    copyIfDifferent(m_launchLongitude, src.m_launchLongitude, isChanged);
    copyIfDifferent(m_launchRodAngle, src.m_launchRodAngle, isChanged);
    copyIfDifferent(m_launchRodDirection, src.m_launchRodDirection, isChanged);
    copyIfDifferent(m_launchRodLength, src.m_launchRodLength, isChanged);
    copyIfDifferent(m_launchIntoWind, src.m_launchIntoWind, isChanged);
    copyIfDifferent(m_useIsa, src.m_useIsa, isChanged);
    copyIfDifferent(m_launchTemperature, src.m_launchTemperature, isChanged);
    copyIfDifferent(m_launchPressure, src.m_launchPressure, isChanged);
    copyIfDifferent(m_launchRelativeHumidity, src.m_launchRelativeHumidity, isChanged);
    copyIfDifferent(m_maximumAngle, src.m_maximumAngle, isChanged);

    copyIfDifferent(m_timeStep, src.m_timeStep, isChanged);
    copyIfDifferent(m_maxSimulationTime, src.m_maxSimulationTime, isChanged);
    copyIfDifferent(m_geodeticComputation, src.m_geodeticComputation, isChanged);
    copyIfDifferent(m_stepperMethodChoice, src.m_stepperMethodChoice, isChanged);
    if (m_randomSeedFixed != src.m_randomSeedFixed ||
        (src.m_randomSeedFixed && m_randomSeed != src.m_randomSeed))
    {
        isChanged = true;
    }

    if (m_dragLookupCsvPath != src.m_dragLookupCsvPath ||
        m_dragLookupTable != src.m_dragLookupTable ||
        m_dragLookupCsvRows != src.m_dragLookupCsvRows)
    {
        isChanged           = true;
        m_dragLookupCsvPath = src.m_dragLookupCsvPath;
        m_dragLookupTable   = src.m_dragLookupTable;
        m_dragLookupCsvRows = src.m_dragLookupCsvRows;
    }
    if (m_stabilityLookupCsvPath != src.m_stabilityLookupCsvPath ||
        m_stabilityLookupTable != src.m_stabilityLookupTable ||
        m_stabilityLookupCsvRows != src.m_stabilityLookupCsvRows)
    {
        isChanged                = true;
        m_stabilityLookupCsvPath = src.m_stabilityLookupCsvPath;
        m_stabilityLookupTable   = src.m_stabilityLookupTable;
        m_stabilityLookupCsvRows = src.m_stabilityLookupCsvRows;
    }

    copyIfDifferent(m_recoverySpeedWarning, src.m_recoverySpeedWarning, isChanged);
    copyIfDifferent(m_drogueLowSpeedWarning, src.m_drogueLowSpeedWarning, isChanged);
    copyIfDifferent(m_recoveryDrogueMainHighSpeedWarning, src.m_recoveryDrogueMainHighSpeedWarning,
                    isChanged);
    copyIfDifferent(m_recoveryDrogueMainLowSpeedWarning, src.m_recoveryDrogueMainLowSpeedWarning,
                    isChanged);

    if (isChanged)
    {
        m_randomSeedFixed = src.m_randomSeedFixed;
        m_randomSeed      = src.m_randomSeed;

        // The nested wind models are updated in bulk above, bypassing their setters. Notify their
        // listeners so bound controls refresh as well.
        if (averageWindChanged)
        {
            m_averageWindModel->fireChangeEvent();
        }
        if (multiLevelWindChanged)
        {
            m_multiLevelPinkNoiseWindModel->fireChangeEvent();
        }
        fireChangeEvent();
    }
}

bool SimulationOptions::operator==(const SimulationOptions& other) const noexcept
{
    return MathUtil::equals(m_launchAltitude, other.m_launchAltitude) &&
           MathUtil::equals(m_launchLatitude, other.m_launchLatitude) &&
           MathUtil::equals(m_launchLongitude, other.m_launchLongitude) &&
           MathUtil::equals(m_launchPressure, other.m_launchPressure) &&
           MathUtil::equals(m_launchRelativeHumidity, other.m_launchRelativeHumidity) &&
           MathUtil::equals(m_launchRodAngle, other.m_launchRodAngle) &&
           MathUtil::equals(m_launchRodDirection, other.m_launchRodDirection) &&
           MathUtil::equals(m_launchRodLength, other.m_launchRodLength) &&
           MathUtil::equals(m_launchTemperature, other.m_launchTemperature) &&
           MathUtil::equals(m_maximumAngle, other.m_maximumAngle) &&
           MathUtil::equals(m_timeStep, other.m_timeStep) &&
           MathUtil::equals(m_maxSimulationTime, other.m_maxSimulationTime) &&
           m_stepperMethodChoice == other.m_stepperMethodChoice &&
           m_dragLookupTable == other.m_dragLookupTable &&
           m_stabilityLookupTable == other.m_stabilityLookupTable &&
           m_windModelType == other.m_windModelType &&
           *m_averageWindModel == *other.m_averageWindModel &&
           *m_multiLevelPinkNoiseWindModel == *other.m_multiLevelPinkNoiseWindModel &&
           m_gravityModelType == other.m_gravityModelType &&
           MathUtil::equals(m_constantGravity, other.m_constantGravity) &&
           MathUtil::equals(m_recoverySpeedWarning, other.m_recoverySpeedWarning) &&
           MathUtil::equals(m_drogueLowSpeedWarning, other.m_drogueLowSpeedWarning) &&
           MathUtil::equals(m_recoveryDrogueMainHighSpeedWarning,
                            other.m_recoveryDrogueMainHighSpeedWarning) &&
           MathUtil::equals(m_recoveryDrogueMainLowSpeedWarning,
                            other.m_recoveryDrogueMainLowSpeedWarning) &&
           m_randomSeedFixed == other.m_randomSeedFixed &&
           (!m_randomSeedFixed || m_randomSeed == other.m_randomSeed);
}

std::string SimulationOptions::toString() const
{
    std::string text = "SimulationOptions [\n";
    text += "    AtmosphericModel: ExtendedIsaModel\n";
    text += std::format("    launchRodLength:  {}\n", fixed(m_launchRodLength));
    text += std::format("    launchIntoWind: {}\n", m_launchIntoWind);
    text += std::format("    launchRodAngle:  {}\n", fixed(m_launchRodAngle));
    text += std::format("    launchRodDirection:  {}\n", fixed(m_launchRodDirection));
    text += std::format("    windModelType: {}\n", windModelTypeName(m_windModelType));
    text += "    pinkNoiseWindModel: PinkNoiseWindModel\n";
    text += "    multiLevelPinkNoiseWindModel: MultiLevelPinkNoiseWindModel\n";
    text += std::format("    launchAltitude:  {}\n", fixed(m_launchAltitude));
    text += std::format("    launchLatitude:  {}\n", fixed(m_launchLatitude));
    text += std::format("    launchLongitude:  {}\n", fixed(m_launchLongitude));
    text += std::format("    geodeticComputation:  {}\n",
                        geodeticComputationName(m_geodeticComputation));
    text += std::format("    useISA:  {}\n", m_useIsa);
    text += std::format("    launchTemperature:  {}\n", fixed(m_launchTemperature));
    text += std::format("    launchPressure:  {}\n", fixed(m_launchPressure));
    text += std::format("    launchHumidity:  {}\n", fixed(m_launchRelativeHumidity));
    text += std::format("    timeStep:  {}\n", fixed(m_timeStep));
    text += std::format("    maxTime:  {}\n", fixed(m_maxSimulationTime));
    text += std::format("    maximumAngle:  {}\n", fixed(m_maximumAngle));
    text += std::format("    stepperMethodChoice: {}\n", getName(m_stepperMethodChoice));
    text += std::format("    randomSeedFixed: {}\n", m_randomSeedFixed);
    text += std::format("    randomSeed: {}\n", m_randomSeed);
    text += "]\n";
    return text;
}

}  // namespace QtRocket
