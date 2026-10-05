#pragma once

#include <filesystem>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/models/AtmosphericModel.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/simulation/SimulationOptionsInterface.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

class Preferences;
class SimulationConditions;

/// What the user configures for one simulation (OpenRocket's simulation/SimulationOptions): the
/// launch rod, the wind (an average PinkNoiseWindModel and a MultiLevelPinkNoiseWindModel, and
/// which of the two is used), the launch site and its atmosphere, the gravity model, the geodetic
/// computation, the stepper and its limits, the recovery warning thresholds, optional drag and
/// stability lookup tables and the random seed. The simulation runs on the SimulationConditions
/// made from it.
///
/// A change source: every setter emits changed() when it changes something (each setter says
/// when), and so does any change of the two wind models, whose own changed() signals are
/// forwarded. The comparisons are Java's: MathUtil::equals for the doubles, so a value within its
/// tolerance of the stored one changes nothing, and a NaN always counts as a change.
///
/// Preferences: OpenRocket reads the global application preferences. Here SimulationOptions()
/// has the values Java gets from an empty preference store, and SimulationOptions(Preferences&)
/// reads the same keys from the given store: the launch rod length, angle and direction, the
/// launch-into-wind flag, the launch site (altitude, latitude, longitude), the ISA flag with the
/// launch temperature, pressure and humidity, the time step and maximum time, and the gravity
/// model with its constant value; the initial level of the multi-level wind model comes from
/// the store's wind keys. Everything else starts from a constant, as in Java: the average wind
/// model is calm (0 m/s from the east), the wind model type AVERAGE, the geodetic computation
/// SPHERICAL, the stepper RK4 whatever the store says, the maximum step angle 3 degrees and the
/// recovery thresholds 20, 3.048, 30.48 and 15.24 m/s (DefaultSimulationOptionFactory::
/// getDefault() applies the launch preferences on top). The options keep a non-owning pointer to
/// the store, for the one write Java makes later: setSimulationStepperMethodChoice() stores the
/// choice. The store must outlive the options and every copy of them (a copy keeps the pointer).
///
/// Copying and moving:
/// - The copy constructor is Java's clone(): every value, copies of the two wind models (with
///   their seeds), the same lookup tables (shared, as in Java) and copies of their CSV rows; the
///   copy has no connections to changed(), and its wind models report to it.
/// - There is no copy assignment: copyConditionsFrom() is what takes another simulation's
///   conditions (it keeps the connections and announces the change).
/// - Moving hands the object over, as a Java reference would be: the wind models keep their
///   addresses, and the connections to changed() and to the wind models' signals come along. A
///   moved-from SimulationOptions may only be destroyed or assigned to.
///
/// Deviations from OpenRocket:
/// - The lookup tables are std::shared_ptr<const MachAoALookup> (null for none) and the paths
///   and CSV rows std::optional (nullopt for Java's null). Java compares the tables by identity,
///   here the pointers. The paths are compared as std::filesystem::path compares them, by their
///   elements and with regard to case on every platform (Java's Path ignores case on Windows).
/// - setDragLookupCsvPath() and setStabilityLookupCsvPath() return the reader's Error where Java
///   lets its exception through; nothing changes then, as in Java.
/// - A path without a table (setDragLookup(path, nullptr)) is a BugError (Java:
///   IllegalArgumentException). The null checks of setGeodeticComputation() and
///   setSimulationStepperMethodChoice() have nothing to check.
/// - getAtmosphericModel() is public (Java: private) and returns a Result, since the launch
///   conditions come from the user and ExtendedIsaModel refuses some of them.
/// - The random seed comes from std::random_device where Java draws new Random().nextInt().
/// - getChangeListeners() is not ported (changed().size() counts the connections).
/// - getLaunchRodDirection() asks a copy of the multi-level wind model for the wind at the
///   launch altitude, where Java asks the model itself and so restarts its random sources. The
///   direction is the same (the wind at time 0 depends on the levels and their seeds only), and
///   the const members of the options write nothing: they may be called from several threads
///   at once while no thread changes the options.
/// - toString() names the atmospheric model and the two wind models by their class only, where
///   Java prints Object.toString() with an identity hash, and it never fails (Java builds the
///   atmospheric model first, which throws for launch conditions the model refuses).
class SimulationOptions final : public SimulationOptionsInterface
{
public:
    /// The largest launch rod angle, rad (MAX_LAUNCH_ROD_ANGLE).
    static constexpr double kMaxLaunchRodAngle = std::numbers::pi / 3;

    /// The options Java makes from an empty preference store (see the class comment); a later
    /// setSimulationStepperMethodChoice() stores nothing.
    SimulationOptions();

    /// The options Java makes when @p preferences are the application preferences. @p preferences
    /// must outlive the options and their copies.
    explicit SimulationOptions(Preferences& preferences);

    ~SimulationOptions() override = default;

    /// Java's clone() (see the class comment).
    SimulationOptions(const SimulationOptions& other);
    SimulationOptions& operator=(const SimulationOptions&) = delete;
    /// Hands the options over with their connections (see the class comment).
    SimulationOptions(SimulationOptions&&) noexcept            = default;
    SimulationOptions& operator=(SimulationOptions&&) noexcept = default;

    /// Emitted when an option or one of the wind models changes (ChangeSource).
    [[nodiscard]] Signal<>& changed() noexcept override { return *m_changed; }

    // ------------------------------------------------------------------------ launch rod

    [[nodiscard]] double getLaunchRodLength() const noexcept override { return m_launchRodLength; }
    /// Stores the length and emits changed(), unless it equals the stored one.
    void setLaunchRodLength(double launchRodLength) override;

    [[nodiscard]] bool getLaunchIntoWind() const noexcept override { return m_launchIntoWind; }
    /// Stores the flag and emits changed(), unless it is the stored one.
    void setLaunchIntoWind(bool launchIntoWind) override;

    [[nodiscard]] double getLaunchRodAngle() const noexcept override { return m_launchRodAngle; }
    /// Stores the angle clamped to +-kMaxLaunchRodAngle and emits changed(), unless the clamped
    /// angle equals the stored one.
    void setLaunchRodAngle(double launchRodAngle) override;

    /// The direction of the launch rod. When launching into the wind it is the wind direction
    /// reduced to 0 ... 2 pi: the average wind model's direction, or with the multi-level model
    /// the direction of its wind at time 0 and the launch altitude. Otherwise it is the stored
    /// direction. Like every const member it only reads (see the class comment).
    [[nodiscard]] double getLaunchRodDirection() const override;
    /// Stores the direction reduced to 0 ... 2 pi and emits changed(), unless the reduced
    /// direction equals the stored one. The stored direction only shows when not launching into
    /// the wind.
    void setLaunchRodDirection(double launchRodDirection) override;

    // ------------------------------------------------------------------------------ wind

    [[nodiscard]] WindModelType getWindModelType() const noexcept { return m_windModelType; }
    /// Stores the type and emits changed(), unless it is the stored one.
    void setWindModelType(WindModelType windModelType);

    /// The wind model of getWindModelType(): the average or the multi-level one.
    [[nodiscard]] WindModel&       getWindModel();
    [[nodiscard]] const WindModel& getWindModel() const;

    /// The average wind model. Changing it emits its own changed() and then this object's.
    [[nodiscard]] PinkNoiseWindModel& getAverageWindModel() noexcept override
    {
        return *m_averageWindModel;
    }
    [[nodiscard]] const PinkNoiseWindModel& getAverageWindModel() const noexcept override
    {
        return *m_averageWindModel;
    }

    /// The multi-level wind model. Changing it emits its own changed() and then this object's.
    [[nodiscard]] MultiLevelPinkNoiseWindModel& getMultiLevelWindModel() noexcept
    {
        return *m_multiLevelPinkNoiseWindModel;
    }
    [[nodiscard]] const MultiLevelPinkNoiseWindModel& getMultiLevelWindModel() const noexcept
    {
        return *m_multiLevelPinkNoiseWindModel;
    }

    /// The wind accessors of older OpenRocket versions, deprecated in Java and kept for the
    /// importers and plugins that use them. Each one, the getters included, first switches the
    /// wind model type to AVERAGE (emitting changed() when that is a change) and then reads or
    /// sets the average wind model's value (PinkNoiseWindModel::setAverage() and so on).
    [[nodiscard]] double getWindSpeedAverage();
    void                 setWindSpeedAverage(double windAverage);
    [[nodiscard]] double getWindSpeedDeviation();
    void                 setWindSpeedDeviation(double windDeviation);
    [[nodiscard]] double getWindTurbulenceIntensity();
    void                 setWindTurbulenceIntensity(double intensity);
    [[nodiscard]] double getWindDirection();
    void                 setWindDirection(double direction);

    // --------------------------------------------------------------------------- gravity

    [[nodiscard]] GravityModelType getGravityModelType() const noexcept
    {
        return m_gravityModelType;
    }
    /// Stores the type and emits changed(), unless it is the stored one.
    void setGravityModelType(GravityModelType gravityModelType);

    /// The gravity of the CONSTANT gravity model, m/s^2.
    [[nodiscard]] double getConstantGravity() const noexcept { return m_constantGravity; }
    /// Stores the value and emits changed(), unless it equals the stored one.
    void setConstantGravity(double constantGravity);

    // ----------------------------------------------------------------------- launch site

    [[nodiscard]] double getLaunchAltitude() const noexcept override { return m_launchAltitude; }
    /// Nothing happens when @p altitude equals the stored altitude. Otherwise the smaller of
    /// @p altitude and ExtendedIsaModel::getMaximumAllowedAltitude() is stored (the maximum for
    /// a NaN), with the ISA atmosphere in use the launch temperature, pressure and humidity are
    /// set to the ISA's at the new altitude (each emitting changed() when it changes), and
    /// changed() is emitted, also when the clamped altitude is the stored one.
    void setLaunchAltitude(double altitude) override;

    [[nodiscard]] double getLaunchLatitude() const noexcept override { return m_launchLatitude; }
    /// Stores the latitude clamped to +-90 degrees and emits changed(), unless the clamped
    /// latitude equals the stored one.
    void setLaunchLatitude(double launchLatitude) override;

    [[nodiscard]] double getLaunchLongitude() const noexcept override { return m_launchLongitude; }
    /// Stores the longitude clamped to +-180 degrees and emits changed(), unless the clamped
    /// longitude equals the stored one.
    void setLaunchLongitude(double launchLongitude) override;

    [[nodiscard]] GeodeticComputationStrategy getGeodeticComputation() const noexcept override
    {
        return m_geodeticComputation;
    }
    /// Stores the strategy and emits changed(), unless it is the stored one.
    void setGeodeticComputation(GeodeticComputationStrategy geodeticComputation) override;

    // --------------------------------------------------------------------------- stepper

    [[nodiscard]] SimulationStepperMethod getSimulationStepperMethodChoice() const noexcept
    {
        return m_stepperMethodChoice;
    }
    /// Stores the choice, also in the preferences these options were made from (their
    /// SimulationStepperMethod key), and emits changed() always, as in Java.
    void setSimulationStepperMethodChoice(SimulationStepperMethod choice);

    // ------------------------------------------------------------------------ atmosphere

    [[nodiscard]] bool isIsaAtmosphere() const noexcept override { return m_useIsa; }
    /// Nothing happens when @p isa is the stored flag. Otherwise it is stored, switching the ISA
    /// on sets the launch temperature, pressure and humidity to the ISA's at the launch altitude
    /// (each emitting changed() when it changes), and changed() is emitted.
    void setIsaAtmosphere(bool isa) override;

    [[nodiscard]] double getLaunchTemperature() const noexcept override
    {
        return m_launchTemperature;
    }
    /// Stores the temperature (K) and emits changed(), unless it equals the stored one.
    void setLaunchTemperature(double launchTemperature) override;

    [[nodiscard]] double getLaunchPressure() const noexcept override { return m_launchPressure; }
    /// Stores the pressure (Pa) and emits changed(), unless it equals the stored one.
    void setLaunchPressure(double launchPressure) override;

    [[nodiscard]] double getLaunchRelativeHumidity() const noexcept override
    {
        return m_launchRelativeHumidity;
    }
    /// Stores the humidity (0 ... 1) and emits changed(), unless it equals the stored one.
    void setLaunchRelativeHumidity(double launchHumidity) override;

    /// The atmospheric model of the launch conditions: the standard ISA model when the ISA is in
    /// use (one model shared by every caller, as Java's ISA_ATMOSPHERIC_MODEL), otherwise a new
    /// ExtendedIsaModel fitted to the launch altitude, temperature, pressure and humidity. Fails
    /// with ExtendedIsaModel::create()'s Error when the model refuses those conditions, such as
    /// a temperature that is not positive. Java throws IllegalArgumentException for the same
    /// conditions, but not always as early: for some (36 K at 11 km) its constructor succeeds
    /// and only the first getConditions() of the run throws. A caller takes this Error for
    /// Java's exception at either point. Public for SimulationConditions (see the class
    /// comment).
    [[nodiscard]] Result<std::shared_ptr<const AtmosphericModel>> getAtmosphericModel() const;

    // -------------------------------------------------------------------- stepper limits

    [[nodiscard]] double getTimeStep() const noexcept { return m_timeStep; }
    /// Stores the time step (s) and emits changed(), unless it equals the stored one.
    void setTimeStep(double timeStep);

    [[nodiscard]] double getMaxSimulationTime() const noexcept { return m_maxSimulationTime; }
    /// Stores the maximum simulation time (s) and emits changed(), unless it equals the stored
    /// one.
    void setMaxSimulationTime(double maxSimulationTime);

    [[nodiscard]] double getMaximumStepAngle() const noexcept { return m_maximumAngle; }
    /// Stores the angle clamped to 1 ... 20 degrees (in rad) and emits changed(), unless the
    /// clamped angle equals the stored one.
    void setMaximumStepAngle(double maximumAngle);

    // --------------------------------------------------------------- recovery thresholds

    /// A design without a drogue: warn when a recovery device deploys above this speed, m/s.
    [[nodiscard]] double getRecoverySpeedWarning() const noexcept { return m_recoverySpeedWarning; }
    void                 setRecoverySpeedWarning(double recoverySpeedWarning);

    /// A design with a drogue: warn when the drogue deploys below this speed at apogee, m/s.
    [[nodiscard]] double getDrogueLowSpeedWarning() const noexcept
    {
        return m_drogueLowSpeedWarning;
    }
    void setDrogueLowSpeedWarning(double drogueLowSpeedWarning);

    /// A design with a drogue: warn when the main deploys above this speed, m/s.
    [[nodiscard]] double getRecoveryDrogueMainHighSpeedWarning() const noexcept
    {
        return m_recoveryDrogueMainHighSpeedWarning;
    }
    void setRecoveryDrogueMainHighSpeedWarning(double recoveryDrogueMainHighSpeedWarning);

    /// A design with a drogue: warn when the main deploys below this speed, m/s.
    [[nodiscard]] double getRecoveryDrogueMainLowSpeedWarning() const noexcept
    {
        return m_recoveryDrogueMainLowSpeedWarning;
    }
    /// Each threshold setter stores the value and emits changed(), unless it equals the stored
    /// one.
    void setRecoveryDrogueMainLowSpeedWarning(double recoveryDrogueMainLowSpeedWarning);

    // --------------------------------------------------------------------- lookup tables

    /// The CSV file the drag table was read from, absolute and normalised, or nullopt.
    [[nodiscard]] const std::optional<std::filesystem::path>& getDragLookupCsvPath() const noexcept
    {
        return m_dragLookupCsvPath;
    }
    /// The drag table ("cd" over Mach number and angle of attack) that replaces the Barrowman
    /// drag, or null.
    [[nodiscard]] std::shared_ptr<const MachAoALookup> getDragLookupTable() const noexcept
    {
        return m_dragLookupTable;
    }
    /// Reads the drag table from the CSV file @p csvPath (made absolute and normalised; the
    /// column "cd" is required) and stores the path and the table, or clears both for nullopt;
    /// the CSV rows are left as they are. Emits changed() when the path or the table changed
    /// (a table that was read is always a new one). Fails with CsvMachAoALookup::fromCsv()'s
    /// Error when the file cannot be read or parsed; nothing changes then.
    [[nodiscard]] Result<void> setDragLookupCsvPath(
        const std::optional<std::filesystem::path>& csvPath);
    /// setDragLookup(csvPath, table, nullopt).
    void setDragLookup(const std::optional<std::filesystem::path>& csvPath,
                       std::shared_ptr<const MachAoALookup>        table);
    /// Stores @p csvRows (the lines of the CSV the table was parsed from, which a .ork file
    /// embeds; nullopt for none), the path (made absolute and normalised) and the table. Emits
    /// changed() when the path or the table changed, not for the rows alone.
    /// @throws BugError for a path without a table ("table must not be null when csvPath is
    ///         provided"); nothing changes then
    void setDragLookup(const std::optional<std::filesystem::path>& csvPath,
                       std::shared_ptr<const MachAoALookup>        table,
                       std::optional<std::vector<std::string>>     csvRows);
    /// Removes the drag path, table and rows; emits changed() when there was a path or a table.
    void clearDragLookup();
    /// Whether there is a drag table.
    [[nodiscard]] bool hasDragLookup() const noexcept { return m_dragLookupTable != nullptr; }
    /// A copy of the drag CSV rows, or nullopt.
    [[nodiscard]] std::optional<std::vector<std::string>> getDragLookupCsvRows() const
    {
        return m_dragLookupCsvRows;
    }

    /// The stability table ("cn", "cm" and "cp") that replaces the Barrowman stability
    /// calculation; everything as for the drag table.
    [[nodiscard]] const std::optional<std::filesystem::path>& getStabilityLookupCsvPath()
        const noexcept
    {
        return m_stabilityLookupCsvPath;
    }
    [[nodiscard]] std::shared_ptr<const MachAoALookup> getStabilityLookupTable() const noexcept
    {
        return m_stabilityLookupTable;
    }
    [[nodiscard]] Result<void> setStabilityLookupCsvPath(
        const std::optional<std::filesystem::path>& csvPath);
    void               setStabilityLookup(const std::optional<std::filesystem::path>& csvPath,
                                          std::shared_ptr<const MachAoALookup>        table);
    void               setStabilityLookup(const std::optional<std::filesystem::path>& csvPath,
                                          std::shared_ptr<const MachAoALookup>        table,
                                          std::optional<std::vector<std::string>>     csvRows);
    void               clearStabilityLookup();
    [[nodiscard]] bool hasStabilityLookup() const noexcept
    {
        return m_stabilityLookupTable != nullptr;
    }
    [[nodiscard]] std::optional<std::vector<std::string>> getStabilityLookupCsvRows() const
    {
        return m_stabilityLookupCsvRows;
    }

    // ----------------------------------------------------------------------- random seed

    /// The seed of the simulation's random sources (the wind turbulence, the stepper's jitter),
    /// a 32-bit int as in Java.
    [[nodiscard]] int getRandomSeed() const noexcept { return m_randomSeed; }
    /// Stores the seed. Emits changed() only when the seed changed and is fixed: a generated
    /// seed does not invalidate existing results, a seed the user chose does.
    void setRandomSeed(int randomSeed);

    /// Whether the simulations reuse the seed instead of drawing a new one for every run.
    [[nodiscard]] bool isRandomSeedFixed() const noexcept { return m_randomSeedFixed; }
    /// Stores the flag and emits changed(), unless it is the stored one.
    void setRandomSeedFixed(bool randomSeedFixed);

    /// setRandomSeed() with a new nondeterministic seed.
    void randomizeSeed();
    /// randomizeSeed() unless the seed is fixed.
    void randomizeSeedIfNotFixed();

    // -------------------------------------------------------------------------- the rest

    /// Takes every condition of @p src: the wind model type, the wind models' configuration
    /// (PinkNoiseWindModel::loadFrom(), so not the average model's seed, and
    /// MultiLevelPinkNoiseWindModel::loadFrom()), the gravity, the launch site, rod and
    /// atmosphere, the stepper and its limits, the geodetic computation, the lookup tables (the
    /// same table objects, copies of the rows) and the recovery thresholds, comparing with == and
    /// != where the setters use MathUtil::equals. The connections to changed() and the
    /// preferences pointer stay as they are.
    ///
    /// When any of that differed, or the fixed-seed flag did, or @p src has a fixed seed that
    /// differs, the seed and its flag are taken as well, each wind model that was loaded emits
    /// its changed() (which this object forwards), and changed() is emitted once more.
    /// Otherwise nothing is emitted and a seed that is not fixed is not copied. The wind models
    /// compare with their seeds, so the average models of two options that are not copies of
    /// each other always differ: copying between them announces a change every time.
    void copyConditionsFrom(const SimulationOptions& src);

    /// Java's equals(): the launch altitude, latitude, longitude, pressure, humidity and
    /// temperature, the rod angle, direction and length, the maximum step angle, the time step,
    /// the maximum time, the constant gravity and the four recovery thresholds agree within
    /// MathUtil::equals (so never with a NaN), the stepper, the wind model type and the gravity
    /// model type are the same, the lookup tables are the same objects, the wind models are
    /// equal (with their seeds), and the fixed-seed flags are the same with, when fixed, the
    /// same seed. The launch-into-wind flag, the ISA flag, the geodetic computation and the
    /// lookup paths and rows do not count, as in Java.
    [[nodiscard]] bool operator==(const SimulationOptions& other) const noexcept;

    /// Java's hashCode(): always 0.
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static): Java's instance method
    [[nodiscard]] int hashCode() const noexcept { return 0; }

    /// Java's toString(), a debugging dump: "SimulationOptions [", one indented line per value
    /// (the doubles with six decimals) and "]", each ending in a newline. See the class comment
    /// for the three lines that differ from Java's.
    [[nodiscard]] std::string toString() const;

    /// The conditions a simulation runs on (Java: toSimulationConditions()): new conditions with
    /// - the launch rod length and angle, and the rod direction of getLaunchRodDirection(), so
    ///   the wind direction when launching into the wind;
    /// - the launch site (latitude, longitude, altitude) as a WorldCoordinate, which clamps the
    ///   latitude and reduces the longitude, and the geodetic computation;
    /// - the random seed;
    /// - a clone() of the wind model in use, seeded with the random seed: the seed governs the
    ///   run without becoming part of the configured model;
    /// - the atmospheric model of getAtmosphericModel(): the shared ISA model, or a new model
    ///   fitted to the launch conditions;
    /// - a new gravity model of the chosen type: a WgsGravityModel, or a ConstantGravityModel
    ///   with the constant gravity;
    /// - a new BarrowmanCalculator whose stability and drag calculators are the Barrowman ones,
    ///   each replaced by a lookup-table calculator over the options' table when there is one;
    /// - a mass calculator;
    /// - the time step, the maximum simulation time and the maximum step angle, and the four
    ///   recovery thresholds.
    /// The conditions have no simulation and no listener yet, and the launch position and
    /// velocity are zero. Nothing of the options changes.
    ///
    /// Fails with getAtmosphericModel()'s Error when the model refuses the launch conditions
    /// (Java: an IllegalArgumentException from here or from the first step of the run).
    [[nodiscard]] Result<SimulationConditions> toSimulationConditions() const;

private:
    /// The values Java reads from @p source; @p store is the store to write to later, or null.
    SimulationOptions(const Preferences& source, Preferences* store);

    /// Makes changes of the wind models emit changed().
    void connectWindModels();

    void fireChangeEvent() const { m_changed->emit(); }

    /// Java's updateDragLookup() and updateStabilityLookup(): stores the path and the table and
    /// emits changed() when either differs from the stored one.
    void updateDragLookup(std::optional<std::filesystem::path> path,
                          std::shared_ptr<const MachAoALookup> table);
    void updateStabilityLookup(std::optional<std::filesystem::path> path,
                               std::shared_ptr<const MachAoALookup> table);

    /// Sets the launch temperature, pressure and humidity to the ISA's at the launch altitude.
    void applyIsaConditions();

    /// The change signal, on the heap so that the wind models' forwarding slots, which point at
    /// it, stay valid when the options are moved. Declared before the wind models, which are
    /// destroyed first.
    std::unique_ptr<Signal<>> m_changed{std::make_unique<Signal<>>()};

    /// The store setSimulationStepperMethodChoice() writes to, or null.
    Preferences* m_preferences{nullptr};

    /*
     * NOTE:  When adding/modifying parameters, they must also be added to the
     * equals and copyFrom methods!!
     */

    double m_launchRodLength;
    bool   m_launchIntoWind;
    double m_launchRodAngle;
    double m_launchRodDirection;

    double                      m_launchAltitude;
    double                      m_launchLatitude;
    double                      m_launchLongitude;
    GeodeticComputationStrategy m_geodeticComputation{GeodeticComputationStrategy::SPHERICAL};

    bool   m_useIsa;
    double m_launchTemperature;       // K
    double m_launchPressure;          // Pa
    double m_launchRelativeHumidity;  // 0 ... 1

    double m_timeStep;
    double m_maxSimulationTime;
    double m_maximumAngle;

    int  m_randomSeed;
    bool m_randomSeedFixed{false};

    WindModelType                                 m_windModelType{WindModelType::AVERAGE};
    std::unique_ptr<PinkNoiseWindModel>           m_averageWindModel;
    std::unique_ptr<MultiLevelPinkNoiseWindModel> m_multiLevelPinkNoiseWindModel;

    GravityModelType m_gravityModelType;
    double           m_constantGravity;

    SimulationStepperMethod m_stepperMethodChoice{SimulationStepperMethod::RK4};

    double m_recoverySpeedWarning{20.0};
    double m_drogueLowSpeedWarning{3.048};
    double m_recoveryDrogueMainHighSpeedWarning{30.48};
    double m_recoveryDrogueMainLowSpeedWarning{15.24};

    std::optional<std::filesystem::path>    m_dragLookupCsvPath;
    std::optional<std::filesystem::path>    m_stabilityLookupCsvPath;
    std::shared_ptr<const MachAoALookup>    m_dragLookupTable;
    std::shared_ptr<const MachAoALookup>    m_stabilityLookupTable;
    std::optional<std::vector<std::string>> m_dragLookupCsvRows;
    std::optional<std::vector<std::string>> m_stabilityLookupCsvRows;
};

}  // namespace QtRocket
