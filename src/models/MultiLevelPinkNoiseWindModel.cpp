#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/unit/DegreeUnit.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

// The English texts of OpenRocket's MultiLevelPinkNoiseWindModel.msg.importLevelsError.* keys.
constexpr std::string_view kEmptyFile = "The CSV file is empty.";
constexpr std::string_view kInvalidColumnIndex =
    "Invalid column index. Please enter numeric indices when the file has no headers.";
constexpr std::string_view kNoValidData = "No valid wind data found in the file.";
constexpr std::string_view kWrongFormat =
    "The file is not in the correct format. Make sure the data is correctly formatted as a "
    "number.";
constexpr std::string_view kCouldNotLoadFile = "Could not load the file.";

/// The lines TextLineReader gives: BufferedReader.readLine() splits at "\n", "\r" and "\r\n";
/// each line is trimmed (String.trim), and blank lines and lines starting with '#' are skipped.
[[nodiscard]] std::vector<std::string_view> readTextLines(std::string_view text)
{
    std::vector<std::string_view> lines;
    std::size_t                   start = 0;
    while (start < text.size())
    {
        std::size_t end = text.find_first_of("\r\n", start);
        if (end == std::string_view::npos)
        {
            end = text.size();
        }
        const std::string_view line = Strings::trim(text.substr(start, end - start));
        if (!line.empty() && line.front() != '#')
        {
            lines.push_back(line);
        }
        start = end + 1;
        if (end < text.size() && text[end] == '\r' && start < text.size() && text[start] == '\n')
        {
            ++start;
        }
    }
    return lines;
}

/// line.split(separator, -1) for a literal separator: every field, empty ones included.
[[nodiscard]] std::vector<std::string_view> splitFields(std::string_view line,
                                                        std::string_view separator)
{
    std::vector<std::string_view> fields;
    std::size_t                   start = 0;
    while (true)
    {
        const std::size_t end = line.find(separator, start);
        if (end == std::string_view::npos)
        {
            fields.push_back(line.substr(start));
            return fields;
        }
        fields.push_back(line.substr(start, end - start));
        start = end + separator.size();
    }
}

/// headers.indexOf(name): the first exact match, or -1.
[[nodiscard]] int indexOf(const std::vector<std::string_view>& headers, std::string_view name)
{
    const auto it = std::ranges::find(headers, name);
    return it == headers.end() ? -1 : static_cast<int>(std::distance(headers.begin(), it));
}

/// findColumnIndex(): the index of the column named @p columnName, failing with ColumnNotFound
/// when a required one is missing.
[[nodiscard]] Result<int> findColumnIndex(const std::vector<std::string_view>& headers,
                                          std::string_view columnName, std::string_view fieldName,
                                          bool required)
{
    const int index = indexOf(headers, columnName);
    if (index == -1 && required)
    {
        return fail(
            ErrorCode::INVALID_ARGUMENT,
            std::format("{} column \"{}\" not found in header row.", fieldName, columnName));
    }
    return index;
}

/// extractDouble(): the trimmed value parsed as a double, with a decimal comma as a fallback.
[[nodiscard]] Result<double> extractDouble(const std::vector<std::string_view>& values, int index,
                                           std::string_view column)
{
    if (index < 0 || static_cast<std::size_t>(index) >= values.size())
    {
        return fail(ErrorCode::PARSE, std::format("Missing value in column {}.", column));
    }
    const std::string_view raw   = values[static_cast<std::size_t>(index)];
    const std::string_view value = Strings::trim(raw);
    if (value.empty())
    {
        return fail(ErrorCode::PARSE, std::format("Empty or null value in column {}.", column));
    }

    // Try parsing with period as decimal separator
    std::optional<double> parsed = Strings::javaParseDouble(value);
    if (!parsed.has_value())
    {
        // If that fails, try replacing the last comma with a period (European format)
        const std::size_t lastComma = value.rfind(',');
        if (lastComma != std::string_view::npos)
        {
            std::string european{value};
            european[lastComma] = '.';
            parsed              = Strings::javaParseDouble(european);
        }
    }
    if (!parsed.has_value())
    {
        return fail(ErrorCode::PARSE, std::format("{}\nValue: '{}'", kWrongFormat, raw));
    }
    return *parsed;
}

/// extractDoubleAndConvert(): extractDouble() taken from @p unit to SI (unchanged without one).
[[nodiscard]] Result<double> extractDoubleAndConvert(const std::vector<std::string_view>& values,
                                                     int index, std::string_view fieldName,
                                                     const Unit* unit)
{
    const Result<double> rawValue = extractDouble(values, index, fieldName);
    if (!rawValue.has_value() || unit == nullptr)
    {
        return rawValue;
    }
    return unit->fromUnit(*rawValue);
}

/// The file name for a message, UTF-8 on every platform (File.getName()).
[[nodiscard]] std::string fileName(const std::filesystem::path& file)
{
    const std::u8string name = file.filename().u8string();
    return {name.begin(), name.end()};
}

/// The column settings of an import: names, or indices when the file has no header line.
struct ColumnNames
{
    std::string_view altitude;
    std::string_view speed;
    std::string_view direction;
    std::string_view stdDeviation;
};

/// The columns of an import; the standard deviation column is -1 (or any negative index) when
/// there is none.
struct ColumnIndices
{
    int altitude;
    int speed;
    int direction;
    int stdDeviation;
};

/// The units of an import's columns, null for values taken as they are.
struct ColumnUnits
{
    const Unit* altitude;
    const Unit* speed;
    const Unit* direction;
    const Unit* stdDeviation;
};

/// The values of one data row, in SI units.
struct Row
{
    double                altitude;
    double                speed;
    double                direction;
    std::optional<double> stdDeviation;
};

/// The column indices found by name in the header line (Java's findColumnIndex() calls).
[[nodiscard]] Result<ColumnIndices> headerColumns(const std::vector<std::string_view>& headers,
                                                  const ColumnNames&                   names)
{
    const Result<int> altitude = findColumnIndex(headers, names.altitude, "altitude", true);
    if (!altitude.has_value())
    {
        return std::unexpected(altitude.error());
    }
    const Result<int> speed = findColumnIndex(headers, names.speed, "speed", true);
    if (!speed.has_value())
    {
        return std::unexpected(speed.error());
    }
    const Result<int> direction = findColumnIndex(headers, names.direction, "direction", true);
    if (!direction.has_value())
    {
        return std::unexpected(direction.error());
    }
    // Standard deviation is optional
    const int stdDeviation = names.stdDeviation.empty() ? -1 : indexOf(headers, names.stdDeviation);
    return ColumnIndices{.altitude     = *altitude,
                         .speed        = *speed,
                         .direction    = *direction,
                         .stdDeviation = stdDeviation};
}

/// The column indices given as numbers (Integer.parseInt) when the file has no header line.
[[nodiscard]] Result<ColumnIndices> indexColumns(const ColumnNames& names)
{
    const std::optional<int> altitude  = Strings::parseInt(names.altitude);
    const std::optional<int> speed     = Strings::parseInt(names.speed);
    const std::optional<int> direction = Strings::parseInt(names.direction);
    const std::optional<int> stdDeviation =
        names.stdDeviation.empty() ? std::optional<int>{-1} : Strings::parseInt(names.stdDeviation);
    // Java also takes a negative altitude, speed or direction index here and then fails with an
    // ArrayIndexOutOfBoundsException on the first row.
    if (!altitude.has_value() || !speed.has_value() || !direction.has_value() ||
        !stdDeviation.has_value() || *altitude < 0 || *speed < 0 || *direction < 0)
    {
        return fail(ErrorCode::INVALID_ARGUMENT, std::string{kInvalidColumnIndex});
    }
    return ColumnIndices{.altitude     = *altitude,
                         .speed        = *speed,
                         .direction    = *direction,
                         .stdDeviation = *stdDeviation};
}

/// The values of the data row @p values, the @p lineNumber-th line read.
[[nodiscard]] Result<Row> parseRow(const std::vector<std::string_view>& values,
                                   const ColumnIndices& columns, const ColumnUnits& units,
                                   int lineNumber)
{
    // Check if we have enough columns
    const int maxColumnIndex =
        std::max({columns.altitude, columns.speed, columns.direction, columns.stdDeviation, 0});
    if (static_cast<std::size_t>(maxColumnIndex) >= values.size())
    {
        return fail(ErrorCode::PARSE,
                    std::format("Line {} does not have enough columns.", lineNumber));
    }

    // Extract and convert values
    const Result<double> altitude =
        extractDoubleAndConvert(values, columns.altitude, "altitude", units.altitude);
    if (!altitude.has_value())
    {
        return std::unexpected(altitude.error());
    }
    const Result<double> speed =
        extractDoubleAndConvert(values, columns.speed, "speed", units.speed);
    if (!speed.has_value())
    {
        return std::unexpected(speed.error());
    }
    const Result<double> direction =
        extractDoubleAndConvert(values, columns.direction, "direction", units.direction);
    if (!direction.has_value())
    {
        return std::unexpected(direction.error());
    }

    // Standard deviation is optional: a missing column or an empty value leaves it out
    Row row{.altitude = *altitude, .speed = *speed, .direction = *direction, .stdDeviation = {}};
    const int stdDeviationIndex = columns.stdDeviation;
    if (stdDeviationIndex >= 0 && static_cast<std::size_t>(stdDeviationIndex) < values.size() &&
        !Strings::trim(values[static_cast<std::size_t>(stdDeviationIndex)]).empty())
    {
        const Result<double> stdDeviation = extractDoubleAndConvert(
            values, stdDeviationIndex, "standard deviation", units.stdDeviation);
        if (!stdDeviation.has_value())
        {
            return std::unexpected(stdDeviation.error());
        }
        row.stdDeviation = *stdDeviation;
    }
    return row;
}

}  // namespace

// ------------------------------------------------------------------------ LevelWindModel

MultiLevelPinkNoiseWindModel::LevelWindModel::LevelWindModel(double             altitude,
                                                             PinkNoiseWindModel model)
  : m_altitude(altitude), m_model(std::move(model))
{
    // Java adds every listener of the level to its model as well; forwarding the model's changes
    // to the level's signal is the same. The level is not movable, so `this` stays valid.
    m_model.changed().connect([this] { m_changed.emit(); });
}

// Every setter holds keepAlive() for its whole call: a slot reached by its change notification
// may remove the level from the model, and the setter (PinkNoiseWindModel::setAverage() emits
// twice) and the signals' emit() loops go on using the level after that slot returns.

void MultiLevelPinkNoiseWindModel::LevelWindModel::setAltitude(double altitude)
{
    const std::shared_ptr<const LevelWindModel> self = keepAlive();
    m_altitude                                       = altitude;
    fireChangeEvent();
}

void MultiLevelPinkNoiseWindModel::LevelWindModel::setSpeed(double speed)
{
    const std::shared_ptr<const LevelWindModel> self = keepAlive();
    m_model.setAverage(speed);
}

void MultiLevelPinkNoiseWindModel::LevelWindModel::setSpeedPreservingStandardDeviation(double speed)
{
    const std::shared_ptr<const LevelWindModel> self = keepAlive();
    m_model.setAveragePreservingStandardDeviation(speed);
}

void MultiLevelPinkNoiseWindModel::LevelWindModel::setDirection(double direction)
{
    const std::shared_ptr<const LevelWindModel> self = keepAlive();
    m_model.setDirection(direction);
}

void MultiLevelPinkNoiseWindModel::LevelWindModel::setStandardDeviation(double standardDeviation)
{
    const std::shared_ptr<const LevelWindModel> self = keepAlive();
    m_model.setStandardDeviation(standardDeviation);
}

void MultiLevelPinkNoiseWindModel::LevelWindModel::setTurbulenceIntensity(
    double turbulenceIntensity)
{
    const std::shared_ptr<const LevelWindModel> self = keepAlive();
    m_model.setTurbulenceIntensity(turbulenceIntensity);
}

void MultiLevelPinkNoiseWindModel::LevelWindModel::fireChangeEvent() const
{
    const std::shared_ptr<const LevelWindModel> self = keepAlive();
    m_changed.emit();
}

std::shared_ptr<MultiLevelPinkNoiseWindModel::LevelWindModel>
MultiLevelPinkNoiseWindModel::LevelWindModel::clone() const
{
    return std::make_shared<LevelWindModel>(m_altitude, PinkNoiseWindModel{m_model});
}

bool MultiLevelPinkNoiseWindModel::LevelWindModel::operator==(
    const LevelWindModel& other) const noexcept
{
    return MathUtil::javaDoubleCompare(other.m_altitude, m_altitude) == 0 &&
           m_model == other.m_model;
}

int MultiLevelPinkNoiseWindModel::LevelWindModel::hashCode() const noexcept
{
    // Objects.hash(altitude, model) = Arrays.hashCode(new Object[]{altitude, model})
    int result = 1;
    result     = MathUtil::javaHashCombine(result, MathUtil::javaDoubleHashCode(m_altitude));
    result     = MathUtil::javaHashCombine(result, m_model.hashCode());
    return result;
}

// ------------------------------------------------------------ MultiLevelPinkNoiseWindModel

MultiLevelPinkNoiseWindModel::MultiLevelPinkNoiseWindModel(const Preferences& preferences)
{
    // Add a default wind level
    addInitialLevel(preferences);
}

MultiLevelPinkNoiseWindModel::MultiLevelPinkNoiseWindModel(
    const MultiLevelPinkNoiseWindModel& other)
  : WindModel(other)
{
    loadFrom(other);
}

Result<void> MultiLevelPinkNoiseWindModel::addWindLevel(double altitude, double speed,
                                                        double                direction,
                                                        std::optional<double> standardDeviation)
{
    PinkNoiseWindModel pinkNoiseModel;
    pinkNoiseModel.setDirection(direction);
    pinkNoiseModel.setAverage(speed);
    if (standardDeviation.has_value())
    {
        pinkNoiseModel.setStandardDeviation(*standardDeviation);
    }

    const std::ptrdiff_t index = binarySearch(altitude);
    if (index >= 0)
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "Wind level already exists for altitude: " +
                                                     Strings::javaDoubleToString(altitude));
    }

    auto newLevel = std::make_shared<LevelWindModel>(altitude, std::move(pinkNoiseModel));
    connectLevel(*newLevel);
    m_levels.insert(m_levels.begin() + (-index - 1), std::move(newLevel));
    fireChangeEvent();
    return {};
}

void MultiLevelPinkNoiseWindModel::addInitialLevel(const Preferences& preferences)
{
    // Java: prefs.getAverageWindModel(), a PinkNoiseWindModel loaded from the wind preferences
    // (only its values are read, so its seed does not matter)
    PinkNoiseWindModel averageWindModel(0);
    averageWindModel.loadFrom(preferences);
    const Result<void> added =
        addWindLevel(0, averageWindModel.getAverage(), averageWindModel.getDirection(),
                     averageWindModel.getStandardDeviation());
    // Only called on an empty model, so 0 m is always free.
    QTROCKET_ASSERT(added.has_value());
}

void MultiLevelPinkNoiseWindModel::removeWindLevel(double altitude)
{
    std::erase_if(m_levels, [altitude](const std::shared_ptr<LevelWindModel>& level) {
        return level->m_altitude == altitude;
    });
    fireChangeEvent();
}

void MultiLevelPinkNoiseWindModel::removeWindLevelIdx(std::size_t index)
{
    if (index >= m_levels.size())
    {
        bug(std::format("removeWindLevelIdx: index {} out of range for {} levels", index,
                        m_levels.size()));
    }
    m_levels.erase(m_levels.begin() + static_cast<std::ptrdiff_t>(index));
    fireChangeEvent();
}

void MultiLevelPinkNoiseWindModel::clearLevels()
{
    m_levels.clear();
    fireChangeEvent();
}

void MultiLevelPinkNoiseWindModel::resetLevels(const Preferences& preferences)
{
    m_levels.clear();
    addInitialLevel(preferences);
    fireChangeEvent();
}

std::vector<MultiLevelPinkNoiseWindModel::LevelWindModel*> MultiLevelPinkNoiseWindModel::getLevels()
{
    std::vector<LevelWindModel*> levels;
    levels.reserve(m_levels.size());
    for (const std::shared_ptr<LevelWindModel>& level : m_levels)
    {
        levels.push_back(level.get());
    }
    return levels;
}

std::vector<const MultiLevelPinkNoiseWindModel::LevelWindModel*>
MultiLevelPinkNoiseWindModel::getLevels() const
{
    std::vector<const LevelWindModel*> levels;
    levels.reserve(m_levels.size());
    for (const std::shared_ptr<LevelWindModel>& level : m_levels)
    {
        levels.push_back(level.get());
    }
    return levels;
}

void MultiLevelPinkNoiseWindModel::sortLevels()
{
    // List.sort is a stable merge sort; Comparator.comparingDouble compares with Double.compare.
    std::ranges::stable_sort(m_levels, [](const std::shared_ptr<LevelWindModel>& a,
                                          const std::shared_ptr<LevelWindModel>& b) {
        return MathUtil::javaDoubleCompare(a->m_altitude, b->m_altitude) < 0;
    });
}

Coordinate MultiLevelPinkNoiseWindModel::getWindVelocity(double time, double altitudeMsl,
                                                         double altitudeAgl)
{
    if (m_altitudeReference == AltitudeReference::MSL)
    {
        return getWindVelocity(time, altitudeMsl);
    }
    return getWindVelocity(time, altitudeAgl);
}

Coordinate MultiLevelPinkNoiseWindModel::getWindVelocity(double time, double altitude)
{
    if (m_levels.empty())
    {
        return Coordinate::kZero;
    }

    const std::ptrdiff_t index = binarySearch(altitude);

    // Retrieve the wind level if it exists
    if (index >= 0)
    {
        return m_levels[static_cast<std::size_t>(index)]->m_model.getWindVelocity(time, altitude);
    }

    // Extrapolation (take the value of the outer bounds)
    const auto insertionPoint = static_cast<std::size_t>(-index - 1);
    if (insertionPoint == 0)
    {
        return m_levels.front()->m_model.getWindVelocity(time, altitude);
    }
    if (insertionPoint == m_levels.size())
    {
        return m_levels.back()->m_model.getWindVelocity(time, altitude);
    }

    // Interpolation (take the value between the closest two bounds)
    LevelWindModel& lowerLevel = *m_levels[insertionPoint - 1];
    LevelWindModel& upperLevel = *m_levels[insertionPoint];
    const double    fraction =
        (altitude - lowerLevel.m_altitude) / (upperLevel.m_altitude - lowerLevel.m_altitude);

    const Coordinate lowerVelocity = lowerLevel.m_model.getWindVelocity(time, altitude);
    const Coordinate upperVelocity = upperLevel.m_model.getWindVelocity(time, altitude);

    return lowerVelocity.interpolate(upperVelocity, fraction);
}

double MultiLevelPinkNoiseWindModel::getWindDirection(double time, double altitude)
{
    const Coordinate velocity  = getWindVelocity(time, altitude);
    const double     direction = std::atan2(velocity.x, velocity.y);

    // Normalize the result to be between 0 and 2*PI (Java's % on doubles is fmod)
    return std::fmod(direction + (2 * std::numbers::pi), 2 * std::numbers::pi);
}

void MultiLevelPinkNoiseWindModel::setAltitudeReference(AltitudeReference altitudeReference)
{
    m_altitudeReference = altitudeReference;
    fireChangeEvent();
}

void MultiLevelPinkNoiseWindModel::setSeed(int seed)
{
    for (std::size_t i = 0; i < m_levels.size(); ++i)
    {
        // seed * 31 + i on Java ints; i is an int index there, so it wraps the same way.
        m_levels[i]->m_model.setSeed(MathUtil::javaHashCombine(seed, static_cast<int>(i)));
    }
}

void MultiLevelPinkNoiseWindModel::loadFrom(const MultiLevelPinkNoiseWindModel& source)
{
    if (&source == this)
    {
        return;  // Java would clear the very list it is about to copy
    }
    m_levels.clear();
    for (const std::shared_ptr<LevelWindModel>& level : source.m_levels)
    {
        std::shared_ptr<LevelWindModel> copy = level->clone();
        connectLevel(*copy);
        m_levels.push_back(std::move(copy));
    }
    m_altitudeReference = source.m_altitudeReference;
}

Result<void> MultiLevelPinkNoiseWindModel::importLevelsFromCsv(
    const std::filesystem::path& file, std::string_view fieldSeparator,
    std::string_view altitudeColumn, std::string_view speedColumn, std::string_view directionColumn,
    std::string_view stdDeviationColumn, const Unit* altitudeUnit, const Unit* speedUnit,
    const Unit* directionUnit, const Unit* stdDeviationUnit, bool hasHeaders)
{
    // Not in Java (whose regex split takes an empty separator): refused before anything changes
    if (fieldSeparator.empty())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "The field separator is empty.");
    }

    // Clear the current levels
    clearLevels();

    const Result<std::string> text = readTextFile(file);
    if (!text.has_value())
    {
        return fail(ErrorCode::IO, std::format("{} '{}'", kCouldNotLoadFile, fileName(file)));
    }

    const std::vector<std::string_view> lines = readTextLines(*text);
    const ColumnNames                   names{.altitude     = altitudeColumn,
                                              .speed        = speedColumn,
                                              .direction    = directionColumn,
                                              .stdDeviation = stdDeviationColumn};

    // Map column indices: from the header line, or given directly
    if (hasHeaders && lines.empty())
    {
        return fail(ErrorCode::PARSE, std::string{kEmptyFile});
    }
    const Result<ColumnIndices> columns =
        hasHeaders ? headerColumns(splitFields(lines.front(), fieldSeparator), names)
                   : indexColumns(names);
    if (!columns.has_value())
    {
        return std::unexpected(columns.error());
    }

    // Read data rows
    const ColumnUnits units{.altitude     = altitudeUnit,
                            .speed        = speedUnit,
                            .direction    = directionUnit,
                            .stdDeviation = stdDeviationUnit};
    int               lineNumber = hasHeaders ? 1 : 0;
    for (std::size_t next = hasHeaders ? 1 : 0; next < lines.size(); ++next)
    {
        ++lineNumber;
        const Result<Row> row =
            parseRow(splitFields(lines[next], fieldSeparator), *columns, units, lineNumber);
        if (!row.has_value())
        {
            return std::unexpected(row.error());
        }

        // Add the wind level
        const Result<void> added =
            addWindLevel(row->altitude, row->speed, row->direction, row->stdDeviation);
        if (!added.has_value())
        {
            return added;
        }
    }

    // Sort levels by altitude
    sortLevels();

    // Check if we have at least one level
    if (m_levels.empty())
    {
        return fail(ErrorCode::PARSE, std::string{kNoValidData});
    }
    return {};
}

Result<void> MultiLevelPinkNoiseWindModel::importLevelsFromCsv(const std::filesystem::path& file,
                                                               std::string_view fieldSeparator)
{
    const DegreeUnit degrees;  // This is more common in wind data
    return importLevelsFromCsv(file, fieldSeparator, "altitude", "speed", "direction", "stddev",
                               &unitGroup(UnitGroupId::DISTANCE).getSIUnit(),
                               &unitGroup(UnitGroupId::WINDSPEED).getSIUnit(), &degrees,
                               &unitGroup(UnitGroupId::WINDSPEED).getSIUnit(), true);
}

std::unique_ptr<WindModel> MultiLevelPinkNoiseWindModel::clone() const
{
    return std::make_unique<MultiLevelPinkNoiseWindModel>(*this);
}

bool MultiLevelPinkNoiseWindModel::operator==(
    const MultiLevelPinkNoiseWindModel& other) const noexcept
{
    if (m_altitudeReference != other.m_altitudeReference)
    {
        return false;
    }
    // Compare the levels list
    return std::ranges::equal(m_levels, other.m_levels,
                              [](const std::shared_ptr<LevelWindModel>& a,
                                 const std::shared_ptr<LevelWindModel>& b) { return *a == *b; });
}

int MultiLevelPinkNoiseWindModel::hashCode() const noexcept
{
    // Objects.hash(levels) = 31 * 1 + levels.hashCode(), the list hash starting from 1
    int listHash = 1;
    for (const std::shared_ptr<LevelWindModel>& level : m_levels)
    {
        listHash = MathUtil::javaHashCombine(listHash, level->hashCode());
    }
    return MathUtil::javaHashCombine(1, listHash);
}

void MultiLevelPinkNoiseWindModel::connectLevel(LevelWindModel& level)
{
    // The model owns its levels and is not movable, so `this` outlives the connection (a level
    // removed during its own setter lives on only until that setter returns).
    level.changed().connect([this] { fireChangeEvent(); });
}

std::ptrdiff_t MultiLevelPinkNoiseWindModel::binarySearch(double altitude) const noexcept
{
    // Collections.indexedBinarySearch with Comparator.comparingDouble(l -> l.altitude)
    std::ptrdiff_t low  = 0;
    std::ptrdiff_t high = std::ssize(m_levels) - 1;
    while (low <= high)
    {
        const std::ptrdiff_t mid = low + ((high - low) / 2);
        const int            cmp = MathUtil::javaDoubleCompare(
            m_levels[static_cast<std::size_t>(mid)]->m_altitude, altitude);
        if (cmp < 0)
        {
            low = mid + 1;
        }
        else if (cmp > 0)
        {
            high = mid - 1;
        }
        else
        {
            return mid;  // key found
        }
    }
    return -(low + 1);  // key not found
}

}  // namespace QtRocket
