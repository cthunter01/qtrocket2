#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

class Preferences;
class Unit;

/// Wind that varies with altitude (OpenRocket's models/wind/MultiLevelPinkNoiseWindModel): a list
/// of levels, each an altitude with its own PinkNoiseWindModel (average speed, direction and
/// standard deviation).
///
/// The levels are kept sorted by altitude as they are added; LevelWindModel::setAltitude() does
/// not re-sort, sortLevels() does. The velocity at an altitude is that of the level at exactly
/// that altitude, that of the lowest level below all levels and of the highest above them, and
/// otherwise the linear interpolation of the velocity vectors of the two levels around it (so a
/// change of direction passes through a lower speed). Each level draws its own turbulence; see
/// PinkNoiseWindModel for the random source and its reproducibility.
///
/// The altitude reference (MSL by default) picks which altitude of
/// getWindVelocity(time, msl, agl) the levels refer to.
///
/// Changes emit changed(): adding, removing and clearing levels, the altitude reference, and
/// any change to a level (its altitude, or its wind model's values). sortLevels(), setSeed() and
/// loadFrom() emit nothing, as in Java.
///
/// The levels are owned by the model, and getLevels() hands out pointers to them (Java returns a
/// new list of the same objects): a pointer stays valid until the level is removed or the model
/// is destroyed. The model is not movable (the levels report their changes to it); the copy
/// constructor and clone() are Java's clone(): the altitude reference and a copy of every level
/// (see PinkNoiseWindModel's copy), without the connections to changed().
///
/// A slot on changed() (or on a level's changed()) may add, remove, clear, reset, load or import
/// levels, also while a level's setter is announcing a change. A level removed that way stays
/// alive until its setter returns (Java's garbage collector keeps it too), finishes that setter,
/// and meanwhile still reports its changes to this model, as in Java. A slot must not destroy
/// the model itself (see Signal).
///
/// Deviations from OpenRocket:
/// - The constructor and resetLevels() take the preferences whose average wind makes the initial
///   level at 0 m: PinkNoiseWindModel::loadFrom(const Preferences&) applies
///   Preferences::getWindAverage(), getWindTurbulenceIntensity() and getWindDirection(), and the
///   level takes that model's average, direction and standard deviation. Java reads the global
///   application preferences, through the same steps.
/// - addWindLevel() returns an Error for an altitude that already has a level, where Java throws
///   IllegalArgumentException; the CSV import returns an Error where Java throws.
/// - loadFrom() of the model itself does nothing (Java clears the levels it is about to copy).
/// - removeWindLevelIdx() takes a std::size_t, and an index out of range is a BugError (Java:
///   an int, and IndexOutOfBoundsException).
/// - The private, unused getHeaderIndex() helpers are not ported.
class MultiLevelPinkNoiseWindModel final : public WindModel
{
public:
    /// One level: an altitude and the wind there (OpenRocket's nested LevelWindModel). Neither
    /// copyable nor movable (its wind model reports to it); clone() copies it.
    ///
    /// A model holds its levels through std::shared_ptr, and every setter (and fireChangeEvent())
    /// of a level owned that way keeps the level alive until it returns, so that a slot removing
    /// the level from its model while the setter announces the change cannot destroy it mid-call
    /// (see the class comment of MultiLevelPinkNoiseWindModel). A level made on its own, not
    /// through std::make_shared, is simply not kept alive.
    class LevelWindModel : public std::enable_shared_from_this<LevelWindModel>
    {
    public:
        /// A level at @p altitude (m) with the wind @p model. Java's package-private constructor.
        LevelWindModel(double altitude, PinkNoiseWindModel model);
        ~LevelWindModel() = default;

        LevelWindModel(const LevelWindModel&)            = delete;
        LevelWindModel& operator=(const LevelWindModel&) = delete;
        LevelWindModel(LevelWindModel&&)                 = delete;
        LevelWindModel& operator=(LevelWindModel&&)      = delete;

        /// The altitude, m.
        [[nodiscard]] double getAltitude() const noexcept { return m_altitude; }
        /// Sets the altitude and emits changed() (always); the model's levels are not re-sorted.
        void setAltitude(double altitude);

        /// The average wind speed, m/s (PinkNoiseWindModel::getAverage()).
        [[nodiscard]] double getSpeed() const noexcept { return m_model.getAverage(); }
        /// PinkNoiseWindModel::setAverage().
        void setSpeed(double speed);
        /// PinkNoiseWindModel::setAveragePreservingStandardDeviation().
        void setSpeedPreservingStandardDeviation(double speed);

        /// The direction the wind blows from, radians (PinkNoiseWindModel::getDirection()).
        [[nodiscard]] double getDirection() const noexcept { return m_model.getDirection(); }
        /// PinkNoiseWindModel::setDirection().
        void setDirection(double direction);

        /// The standard deviation of the speed, m/s.
        [[nodiscard]] double getStandardDeviation() const noexcept
        {
            return m_model.getStandardDeviation();
        }
        /// PinkNoiseWindModel::setStandardDeviation().
        void setStandardDeviation(double standardDeviation);

        /// PinkNoiseWindModel::getTurbulenceIntensity().
        [[nodiscard]] double getTurbulenceIntensity() const noexcept
        {
            return m_model.getTurbulenceIntensity();
        }
        /// PinkNoiseWindModel::setTurbulenceIntensity().
        void setTurbulenceIntensity(double turbulenceIntensity);

        /// PinkNoiseWindModel::getIntensityDescriptionKey() (Java: getIntensityDescription()).
        [[nodiscard]] std::string_view getIntensityDescriptionKey() const noexcept
        {
            return m_model.getIntensityDescriptionKey();
        }

        /// Java's clone(): the altitude and a copy of the wind model (seed included, see
        /// PinkNoiseWindModel), without the connections to changed().
        [[nodiscard]] std::shared_ptr<LevelWindModel> clone() const;

        /// Java's equals(): the altitudes compare equal under Double.compare and the wind models
        /// are equal (PinkNoiseWindModel::operator==, seed included).
        [[nodiscard]] bool operator==(const LevelWindModel& other) const noexcept;

        /// Java's hashCode(): Objects.hash(altitude, model).
        [[nodiscard]] int hashCode() const noexcept;

        /// Emitted when the altitude or a value of the wind model changes (ChangeSource: Java
        /// registers each listener with the level and with its model).
        [[nodiscard]] Signal<>& changed() noexcept { return m_changed; }

        /// Emits changed().
        void fireChangeEvent() const;

    private:
        friend class MultiLevelPinkNoiseWindModel;

        /// The owner of this level while one exists (see the class comment), held by a setter
        /// for the rest of its call; null for a level not owned through a std::shared_ptr.
        [[nodiscard]] std::shared_ptr<const LevelWindModel> keepAlive() const noexcept
        {
            return weak_from_this().lock();
        }

        double             m_altitude;
        PinkNoiseWindModel m_model;
        Signal<>           m_changed;
    };

    /// A model with one level at 0 m holding the average wind of @p preferences (see the class
    /// comment), referring to mean sea level.
    explicit MultiLevelPinkNoiseWindModel(const Preferences& preferences);
    ~MultiLevelPinkNoiseWindModel() override = default;

    /// Java's clone() (see the class comment).
    MultiLevelPinkNoiseWindModel(const MultiLevelPinkNoiseWindModel& other);
    MultiLevelPinkNoiseWindModel& operator=(const MultiLevelPinkNoiseWindModel&) = delete;
    MultiLevelPinkNoiseWindModel(MultiLevelPinkNoiseWindModel&&)                 = delete;
    MultiLevelPinkNoiseWindModel& operator=(MultiLevelPinkNoiseWindModel&&)      = delete;

    /// Adds a level at @p altitude (m) with a new PinkNoiseWindModel (a nondeterministic seed
    /// until setSeed()) given @p direction, then @p speed, then @p standardDeviation when there is
    /// one (otherwise the deviation stays 0). The level goes where Java's binary search by
    /// altitude (Double.compare) puts it, so the levels stay sorted if they were. Emits changed().
    /// Fails with ErrorCode::INVALID_ARGUMENT ("Wind level already exists for altitude: <alt>")
    /// when that search finds a level at the same altitude; nothing changes then.
    [[nodiscard]] Result<void> addWindLevel(double altitude, double speed, double direction,
                                            std::optional<double> standardDeviation = std::nullopt);

    /// Removes every level whose altitude == @p altitude (so -0.0 matches 0.0, and NaN nothing),
    /// then emits changed() (also when nothing was removed).
    void removeWindLevel(double altitude);

    /// Removes the level at @p index, then emits changed().
    /// @throws BugError when @p index is out of range.
    void removeWindLevelIdx(std::size_t index);

    /// Removes every level, then emits changed().
    void clearLevels();

    /// Removes every level and adds the initial level from @p preferences (see the constructor);
    /// emits changed() twice, for the addition and the reset, as in Java.
    void resetLevels(const Preferences& preferences);

    /// The levels in their current order (a new vector of pointers to the model's own levels).
    [[nodiscard]] std::vector<LevelWindModel*>       getLevels();
    [[nodiscard]] std::vector<const LevelWindModel*> getLevels() const;

    /// Sorts the levels by altitude (a stable sort by Double.compare). Emits nothing.
    void sortLevels();

    /// The velocity at @p altitudeMsl or @p altitudeAgl, as the altitude reference says.
    [[nodiscard]] Coordinate getWindVelocity(double time, double altitudeMsl,
                                             double altitudeAgl) override;

    /// The velocity at @p time and @p altitude (see the class comment): Coordinate::kZero when
    /// there are no levels.
    /// @throws BugError when @p time is negative (see PinkNoiseWindModel::getWindVelocity()).
    [[nodiscard]] Coordinate getWindVelocity(double time, double altitude) override;

    /// The direction of getWindVelocity(time, altitude), atan2(x, y) moved into 0 ... 2 pi.
    [[nodiscard]] double getWindDirection(double time, double altitude);

    /// The altitude the levels refer to.
    [[nodiscard]] AltitudeReference getAltitudeReference() const noexcept
    {
        return m_altitudeReference;
    }
    /// Sets the altitude reference and emits changed() (always).
    void setAltitudeReference(AltitudeReference altitudeReference);

    /// Seeds level i (in the current order) with seed * 31 + i in Java's wrapping int arithmetic:
    /// levels seeded alike would draw identical turbulence, making the gusts at every altitude
    /// perfectly correlated. The levels are sorted by altitude, so the seeds are a function of
    /// the configuration.
    void setSeed(int seed) override;

    /// Always ModId::zero(), as in Java (changes are announced through changed()).
    [[nodiscard]] ModId modId() const noexcept override { return ModId::zero(); }

    /// Replaces the levels with copies of @p source's (LevelWindModel::clone()) and takes its
    /// altitude reference. Emits nothing.
    void loadFrom(const MultiLevelPinkNoiseWindModel& source);

    /// Replaces the levels with those read from the CSV @p file (UTF-8). First every level is
    /// removed (emitting changed()), then every row adds its level in turn. If the import fails,
    /// the levels of the rows before the failing one remain, as in Java (a caller such as the GUI
    /// resets the model then); an empty @p fieldSeparator fails before anything changes. Lines
    /// are read as OpenRocket's TextLineReader reads them: trimmed, and skipped when blank or
    /// starting with '#'. Each is split at every @p fieldSeparator, keeping empty fields.
    ///
    /// With @p hasHeaders the first line names the columns, and @p altitudeColumn,
    /// @p speedColumn, @p directionColumn and @p stdDeviationColumn are column names, matched
    /// exactly; the standard deviation column is optional (an empty name, or a name the header
    /// lacks, means none). Without headers they are zero-based column indices (Integer.parseInt;
    /// an empty standard deviation index or -1 means none).
    ///
    /// Each value is trimmed and parsed as Double.parseDouble does (Strings::javaParseDouble),
    /// then, failing that, again with its last comma turned into a point ("1,5"); a unit converts
    /// it to SI (Unit::fromUnit), a null unit keeps it as it is. An empty standard deviation
    /// leaves the level's deviation at 0. Every row adds a level (addWindLevel()), and the levels
    /// are sorted at the end.
    ///
    /// Fails (with OpenRocket's English messages) when the file cannot be read (ErrorCode::IO),
    /// holds no line ("The CSV file is empty."), lacks a required column, has a column index that
    /// is not an integer, a row with too few fields, an empty or unreadable value, two rows at
    /// one altitude, or no rows at all (ErrorCode::PARSE, ErrorCode::INVALID_ARGUMENT for the
    /// column settings). Deviations: Java splits at a regular expression, here @p fieldSeparator
    /// is taken literally (the separators OpenRocket offers, "," ";" " " and a tab, mean the same
    /// either way), and an empty separator fails; a negative altitude, speed or direction index
    /// fails where Java reads past its array.
    [[nodiscard]] Result<void> importLevelsFromCsv(
        const std::filesystem::path& file, std::string_view fieldSeparator,
        std::string_view altitudeColumn, std::string_view speedColumn,
        std::string_view directionColumn, std::string_view stdDeviationColumn,
        const Unit* altitudeUnit, const Unit* speedUnit, const Unit* directionUnit,
        const Unit* stdDeviationUnit, bool hasHeaders);

    /// importLevelsFromCsv() with the header names "altitude", "speed", "direction" and "stddev",
    /// altitudes in m, speeds and deviations in m/s, and directions in degrees (DegreeUnit).
    [[nodiscard]] Result<void> importLevelsFromCsv(const std::filesystem::path& file,
                                                   std::string_view             fieldSeparator);

    /// A copy (see the class comment).
    [[nodiscard]] std::unique_ptr<WindModel> clone() const override;

    /// Java's equals(): the same altitude reference and equal levels in the same order.
    [[nodiscard]] bool operator==(const MultiLevelPinkNoiseWindModel& other) const noexcept;

    /// Java's hashCode(): Objects.hash(levels), the list hash of the levels' hashCode()s (the
    /// altitude reference does not count).
    [[nodiscard]] int hashCode() const noexcept;

private:
    /// addInitialLevel(): the level at 0 m from the average wind of @p preferences.
    void addInitialLevel(const Preferences& preferences);

    /// Makes changes of @p level emit this model's changed().
    void connectLevel(LevelWindModel& level);

    /// Collections.binarySearch by altitude with Double.compare: the index of a level at
    /// @p altitude, or -(insertion point) - 1.
    [[nodiscard]] std::ptrdiff_t binarySearch(double altitude) const noexcept;

    /// Shared ownership only so that a level's setter can keep it alive (see LevelWindModel);
    /// the model is the only other owner.
    std::vector<std::shared_ptr<LevelWindModel>> m_levels;
    AltitudeReference                            m_altitudeReference{AltitudeReference::MSL};
};

}  // namespace QtRocket
