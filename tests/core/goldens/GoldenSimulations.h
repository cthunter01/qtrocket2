#pragma once

// Running the golden simulations and comparing them with their files (tests/data/goldens/<input>/
// sim_<NN>_<name>.json and the branch CSV files next to them, written by tools/openrocket-goldens:
// GoldenDumper.java and SimulationDumper.java; the format is in that tool's README.md). Test-only.
//
// What is here is what the golden tests of the simulations share: the tolerances of the
// default-step set, the harness's set-up of a simulation of a test rocket (the options of a fresh
// installation, the variants, makeReproducible()), the run (with the jitter removal and, for the
// sensitivity analysis, a perturbation), the golden files of a simulation and what they hold, the
// measurements, the rule of the out-of-plane noise columns (rodOnlyRule()), the sensitivity
// analysis, the comparison of a run with its files (compareSimulation()), the manifest and the
// floor under the reproducible part, and the scaffolding of the mutation tests.
//
// simulation_golden_tests.cpp compares the default-step set with it (time step 0.05 s: what is
// reproducible of a flight, and why no more is, is described at the top of that file) and
// simulation_stable_golden_tests.cpp the stable-step set (0.01 s, whole flights, with rules of its
// own that live in that file). The comparison of the default-step set, compareSimulation(), also
// compares everything of a stable-step simulation that is not a number of the trajectory, of a
// simulation whose golden files are its document alone as well (the stable-step set of the
// examples, example_simulation_stable_golden_tests.cpp: a branch without a time series is
// compared in everything but its rows). One rule is the same in both comparisons and lives here:
// an out-of-plane column that is noise-dominated is compared on the launch rod only (rule N of
// the stable-step set). What the two comparisons of the stable-step sets share beyond that is in
// GoldenStableSimulations.h.
//
// The set-up and the run are those of a test rocket of tests/core/rocket/TestRockets.h
// (runSimulation() builds the rocket and the harness's document from a TestRocketMaker), or
// those of a simulation of a loaded design (runDocumentSimulation(): the simulation as the file
// has it, with its own options and extensions, on the rocket of its document, which is how the
// harness runs the simulations of OpenRocket's examples; example_simulation_golden_tests.cpp
// compares them). What the two kinds of tests do with a run and its files is here as well: the
// run next to its perturbed run and its files (GoldenRun), the floor under what is compared,
// the sums of an input, and what the measurements print.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/DefaultSimulationOptionFactory.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenMismatches.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Test
{

// ================================================================================ tolerances

/// A value against its golden value: within kValueRelative of the scale of the value (the larger
/// magnitude of the two, or, for a value of a time series and for the minimum and maximum of a
/// column, the largest magnitude of the golden column).
inline constexpr double kValueRelative = 1e-9;

/// A time (an event, the time to apogee, ...) against its golden value: within kTimeAbsolute
/// seconds.
inline constexpr double kTimeAbsolute = 1e-6;

/// A value of a time series in a row on the launch rod is reproducible when the perturbed run
/// moves it by no more than its tolerance divided by this (1e-12 of the scale of its column).
/// On the rod nothing amplifies a difference: over the 47 runs of the measurement (glibc and 46
/// patterns of the one-ulp libm shim) the perturbed run moves a value there by 7.3e-14 of its
/// scale at most, and the run differs from the golden file by as little.
inline constexpr double kSensitivityMargin = 1000.0;

/// ... and a value in a row after the launch rod, of a run that is not reproducible as a whole
/// at that margin, when the perturbed run moves it by no more than its tolerance divided by
/// this (1e-13 of the scale of its column): ten times less. In such a flight a difference grows
/// from row to row, at a rate that the perturbed run shows, from a start that it does not: the
/// golden run of OpenRocket and a run under another mathematical library start elsewhere. The
/// margin is the least power of ten for which the largest difference from the golden files
/// among the values compared stays a hundredth of the tolerance in each of the 47 runs of the
/// measurement: 5.0e-12 of the scale at this margin, 1.2e-10 at a margin of 1000 or 3000 (see
/// "Tolerances" at the top of simulation_golden_tests.cpp, and
/// SimulationGoldenMeasurement.DISABLED_PrintsTheSensitivityRowByRow, which prints what any
/// margin would give). A run that the perturbed run reproduces to its last row at
/// kSensitivityMargin has nothing that grows, and is whole as it is (sensitivityOf()).
inline constexpr double kFlightSensitivityMargin = 10000.0;

/// The relative tolerance of the one option that is computed with mathematical functions: the
/// launch rod direction of a launch into a multi-level wind (see compareLaunchOptions()). With
/// glibc it is the golden value to the last bit, because the sum the model reduces to a full
/// turn (atan2() + 2 pi) absorbs a last-bit error of atan2(); a library that is three ulps off
/// gives 1.5707963267948961 for 1.5707963267948966 (3e-16).
inline constexpr double kComputedDirectionRelative = 1e-12;

// =================================================================================== harness

/// SimulationDumper.RANDOM_SEED: the random seed every golden simulation runs with.
inline constexpr int kHarnessRandomSeed = 0;

/// SimulationDumper.columnKey(): the save key of a built-in type, "custom:<name>" otherwise.
[[nodiscard]] std::string columnKey(const FlightDataType& type);

/// GoldenDumper.slug(): lower-case ASCII letters and digits, every other run of characters
/// replaced by one '-' (none at the start or the end); "unnamed" when nothing is left.
[[nodiscard]] std::string slug(std::string_view text);

/// GoldenDumper.Variant: a harness-defined extra simulation of a test rocket, the default
/// simulation of one flight configuration with one option changed.
struct Variant
{
    std::string_view input;                     ///< the golden input ("testrocket-estes-alpha-iii")
    std::string_view configuration;             ///< the name of the flight configuration ("[C6-5]")
    std::string_view label;                     ///< appended to the simulation's name
    std::string_view description;               ///< the golden "variant"
    void (*apply)(SimulationOptions& options);  ///< the change
};

/// GoldenDumper.applicationDefaultOptions(): the options a fresh OpenRocket installation gives a
/// new simulation. The harness's ApplicationDefaultsPreferences answers every query with the
/// default ApplicationPreferences passes; here that is an empty store. The options keep a
/// pointer to their store, so the two live together.
struct InstallationDefaults
{
    InMemoryPreferences preferences;
    SimulationOptions   options{DefaultSimulationOptionFactory(preferences).getDefault()};

    /// Sets what the harness sets on top of the factory's options: the time step, the maximum
    /// simulation time, the geodetic computation, the gravity model with its constant value and
    /// the one level of the multi-level wind, each from the store.
    InstallationDefaults();
};

/// SimulationDumper.makeReproducible(): fixed seed kHarnessRandomSeed and zero standard deviation
/// of every wind model. Returns what was changed, as the golden "harness" object.
[[nodiscard]] nlohmann::json makeReproducible(SimulationOptions& options);

/// One simulation of the document the harness builds for a test rocket
/// (GoldenDumper.dumpTestRocket()).
struct PlannedSimulation
{
    std::size_t    index{0};          ///< in the document, and in the manifest's list
    int            configuration{0};  ///< in the rocket's configurations, the default one first
    std::string    name;              ///< the configuration's name, and the variant's label
    const Variant* variant{nullptr};  ///< null for the simulation of a configuration
};

/// The simulations of the document: one per flight configuration in the rocket's order, the
/// default configuration first, then the variants of the input @p input.
[[nodiscard]] std::vector<PlannedSimulation> plannedSimulations(Rocket&          rocket,
                                                                std::string_view input);

/// "sim_<NN>_<slug of the name>": the base name of the files of a simulation.
[[nodiscard]] std::string baseName(const PlannedSimulation& planned);

// ======================================================================================= run

/// @p value moved to the next representable number away from zero; zero stays zero.
[[nodiscard]] double lastBitMoved(double value);

/// The listener of the perturbed run: after every step it moves each component of the velocity
/// and of the rotation velocity of the rocket to the next representable number. A system
/// listener, so that it stays in the nested optimum-coast simulation and adds no "listeners
/// affected the simulation" warning.
class LastBitListener final : public CloneableSimulationListener<LastBitListener>
{
public:
    [[nodiscard]] bool isSystemListener() const override { return true; }

    void postStep(SimulationStatus& status) override
    {
        const Coordinate& velocity = status.getRocketVelocity();
        status.setRocketVelocity(Coordinate{lastBitMoved(velocity.x), lastBitMoved(velocity.y),
                                            lastBitMoved(velocity.z), velocity.weight});
        const Coordinate& rotation = status.getRocketRotationVelocity();
        status.setRocketRotationVelocity(Coordinate{lastBitMoved(rotation.x),
                                                    lastBitMoved(rotation.y),
                                                    lastBitMoved(rotation.z), rotation.weight});
    }
};

/// The golden "optionsSource" of a simulation of a test rocket: the options are those of a fresh
/// installation (InstallationDefaults), and of a simulation of a loaded design: its own.
inline constexpr std::string_view kOptionsOfTheApplication = "applicationDefaults";
inline constexpr std::string_view kOptionsOfTheDocument    = "document";

/// One golden simulation as QtRocket runs it.
struct SimulationRun
{
    std::string       problem;  ///< why there is no run, else ""
    PlannedSimulation planned;
    /// The loaded design whose simulation was run (runDocumentSimulation()): it owns the rocket
    /// and holds the simulation, and the run keeps it alive. Null for a simulation of a test
    /// rocket, which the run owns with its rocket.
    std::shared_ptr<LoadedDocument> document;
    std::shared_ptr<Rocket>         rocket;      ///< the run's own rocket, or the document's
    std::shared_ptr<Simulation>     simulation;  ///< with the options the run used
    /// The golden "optionsSource": where the options of the simulation come from.
    std::string_view            optionsSource{kOptionsOfTheApplication};
    nlohmann::json              harness;  ///< what makeReproducible() changed
    std::string                 status;   ///< "completed" or "exception"
    std::optional<std::string>  exceptionType;
    std::optional<std::string>  exceptionMessage;
    std::int64_t                jitterReplacements{0};
    std::shared_ptr<FlightData> data;  ///< null when the engine has none
};

/// How a golden simulation is run, beyond what the harness does to every one of them.
struct RunVariation
{
    /// SimulationDumper.useStableTimeStep(): the time step of the stable-step set, which the
    /// harness gives the simulation at the last moment before the run (nullopt: the simulation
    /// keeps its own, as in the default-step set).
    std::optional<double> stableTimeStep;
    /// The listener of a perturbed run (LastBitListener, KickListener), or null: the run of the
    /// harness.
    std::shared_ptr<SimulationListener> perturbation;
};

/// SimulationDumper.useStableTimeStep(): gives the simulation the time step @p timeStep of the
/// stable-step set and nothing else, and adds what was changed to @p harness: the time step the
/// simulation had and the one it has now.
void useStableTimeStep(SimulationOptions& options, nlohmann::json& harness, double timeStep);

/// GoldenDumper.dumpTestRocket() for one simulation: the rocket of @p maker, a simulation of
/// the planned configuration with the installation's default options, the variant's change, the
/// harness's changes (for the stable-step set, the stable time step last), and the run with the
/// perturbation of @p variation, if any.
[[nodiscard]] SimulationRun runSimulation(const TestRocketMaker& maker, std::size_t index,
                                          const RunVariation& variation);

/// SimulationDumper.dump()'s "flightConfiguration.index": the place of the configuration with
/// the id @p id among the configurations of @p rocket, the default one first (index 0). An id
/// the rocket has no configuration of gets the number of its configurations, the default
/// included, as the dumper's loop leaves it.
[[nodiscard]] int configurationIndexOf(const Rocket& rocket, const FlightConfigurationId& id);

/// GoldenDumper.examplePass() for one simulation of a loaded design: simulation @p index of
/// @p document as the file has it, with its own flight configuration, options and extensions;
/// the harness's changes to its options (makeReproducible(); for the stable-step set, the
/// stable time step last); and the run, on the rocket of the document, with the extensions
/// initialised between the two listeners of the jitter removal and the perturbation of
/// @p variation, if any. The run keeps @p document alive.
///
/// The harness runs the simulations of a design one after the other on the one rocket of its
/// document, and so must a test: call this once for every simulation, in the order of the
/// document (the options of the simulation are changed in place, so a second call would record
/// the harness's values as the document's; and an extension may change the rocket while its
/// simulation flies, as RollControl turns its fins, so that a run is the harness's only on the
/// rocket that the runs before it left). The stored flight of the simulation is not touched:
/// the run is the engine's, as in the harness, not Simulation::simulate().
[[nodiscard]] SimulationRun runDocumentSimulation(const std::shared_ptr<LoadedDocument>& document,
                                                  std::size_t index, const RunVariation& variation);

// ==================================================================================== golden

/// A golden simulation: its document and the time series of its branches.
struct GoldenFiles
{
    std::string              problem;  ///< why the files could not be read, else ""
    nlohmann::json           document;
    std::vector<GoldenTable> tables;  ///< one per branch CSV of the manifest
};

/// Reads the files of the simulation @p simulation of the manifest.
[[nodiscard]] GoldenFiles loadGoldenFiles(const GoldenSimulation& simulation);

// ==================================================================================== counts

/// How much of the golden simulations there is to compare, was compared, or is sensitive.
struct SimulationCounts
{
    int simulations{0};
    int branches{0};
    int events{0};
    int columns{0};
    int warnings{0};  ///< the warnings of the simulations (those of the events not counted)
    /// The numbers outside the time series: the ten summary values, the jitter replacements,
    /// and per branch the number of rows, the four values of its header, the minimum and maximum
    /// of every column, the time of every event and the parameter of every warning that has one.
    int          numbers{0};
    std::int64_t rows{0};
    std::int64_t values{0};  ///< the values of the time series: rows times columns

    [[nodiscard]] bool operator==(const SimulationCounts&) const = default;

    SimulationCounts& operator+=(const SimulationCounts& other)
    {
        simulations += other.simulations;
        branches += other.branches;
        events += other.events;
        columns += other.columns;
        warnings += other.warnings;
        numbers += other.numbers;
        rows += other.rows;
        values += other.values;
        return *this;
    }
};

/// The sum of @p a and @p b.
[[nodiscard]] SimulationCounts operator+(SimulationCounts a, const SimulationCounts& b);

/// "50 simulations, 53 branches, ...", for the messages of the tests.
[[nodiscard]] std::string toText(const SimulationCounts& counts);

/// toText() of @p counts, for the messages of the tests.
std::ostream& operator<<(std::ostream& out, const SimulationCounts& counts);

/// What the files @p files of one simulation hold.
[[nodiscard]] SimulationCounts goldenCounts(const GoldenFiles& files);

// ============================================================================== measurements

/// What the comparison of the default-step set does with a number of the trajectory.
enum class Treatment : std::uint8_t
{
    COMPARED,   ///< compared with its golden value, at its tolerance
    SENSITIVE,  ///< not compared: beyond what is reproducible of the run (its horizon)
    EXCLUDED    ///< not compared: of a noise-dominated out-of-plane column, off the launch rod
};

/// "compared", "sensitive" or "excluded": what the table of the measurements calls @p treatment.
[[nodiscard]] std::string_view treatmentName(Treatment treatment);

/// The largest differences from the golden files, by simulation and by what was compared; see
/// SimulationGoldenMeasurement.
class Measurements
{
public:
    /// A difference from a golden value: @p difference against the tolerance @p tolerance, of a
    /// value that was compared, or one that was not (@p treatment).
    void record(const std::string& context, const std::string& what, Treatment treatment,
                double difference, double tolerance)
    {
        Entry& entry =
            m_entries[std::format("{}\t{}\t{}", context, what, treatmentName(treatment))];
        entry.count++;
        entry.absolute = std::max(entry.absolute, difference);
        if (tolerance > 0)
        {
            entry.ofTolerance = std::max(entry.ofTolerance, difference / tolerance);
        }
        else if (difference > 0)
        {
            entry.ofTolerance = std::numeric_limits<double>::infinity();
        }
    }

    /// The table: one line per simulation and thing compared, with how many values, their
    /// largest difference and that difference as a multiple of its tolerance.
    [[nodiscard]] std::string text() const
    {
        std::string text;
        for (const auto& [what, entry] : m_entries)
        {
            text += std::format("{}\t{}\t{:.3e}\t{:.3e}\n", what, entry.count, entry.absolute,
                                entry.ofTolerance);
        }
        return text;
    }

    /// The largest difference of a value of the time series of the simulation @p context from
    /// its golden value, whether compared or not, as a multiple of its tolerance (which is
    /// kValueRelative of the scale of its column).
    [[nodiscard]] double largestOfTheTimeSeries(const std::string& context) const
    {
        const std::string prefix  = context + "\tcolumn:";
        double            largest = 0;
        for (auto entry = m_entries.lower_bound(prefix);
             entry != m_entries.end() && entry->first.starts_with(prefix); ++entry)
        {
            largest = std::max(largest, entry->second.ofTolerance);
        }
        return largest;
    }

    /// The largest difference of a compared value, as a multiple of its tolerance.
    [[nodiscard]] double largestComparedOfTolerance() const
    {
        double largest = 0;
        for (const auto& [what, entry] : m_entries)
        {
            if (what.ends_with("\tcompared"))
            {
                largest = std::max(largest, entry.ofTolerance);
            }
        }
        return largest;
    }

private:
    struct Entry
    {
        std::int64_t count{0};
        double       absolute{0};     ///< the largest difference
        double       ofTolerance{0};  ///< the largest difference, as a multiple of its tolerance
    };
    std::map<std::string, Entry> m_entries;
};

// ============================================================ the out-of-plane noise columns

/// An out-of-plane column is noise-dominated when its largest magnitude is below this fraction
/// of that of its counterpart in the plane of the flight (kOutOfPlaneColumns). In the planar
/// flights of the test rockets the fractions are 2e-6 to 3e-4 in the stable-step set and 2e-6 to
/// 2e-3 in the default-step set (the roll rate, in the flights that have one: 7e-11 to 1e-4 of
/// the pitch rate), in the one flight that leaves its plane (the multi-level wind turns with the
/// altitude) 0.2 to 2 in both.
inline constexpr double kNoiseRatio = 1e-2;

/// An out-of-plane column, the column whose scale measures it and its counterpart in the plane
/// of a planar flight. When the measure is below kNoiseRatio of the counterpart (and not a
/// column of zeros, which is compared like any other), the column is noise-dominated and is
/// compared on the launch rod only (rodOnlyRule()). The flights of the test rockets are planar
/// but for the Coriolis acceleration (the wind blows along one axis), so such a column holds a
/// signal of a millionth to a thousandth of its counterpart's, at a tolerance that is as much
/// smaller: what rounding leaves of the motion in the plane is larger than that. In the stable-step
/// set it is rule N (the hunting scatters these columns; two runs of OpenRocket differ in them by
/// up to their own scale). In the default-step set a mathematical library that rounds a
/// result otherwise moves them by more than their tolerance within the first rows of free
/// flight, which the perturbed run of the sensitivity analysis cannot show (it moves every
/// component of the velocities by its own last bit, so what it does to an out-of-plane
/// component is in proportion to that component and not to the motion in the plane): see "The
/// out-of-plane noise columns" at the top of simulation_golden_tests.cpp.
struct OutOfPlaneColumn
{
    std::string_view key;
    std::string_view measure;
    std::string_view counterpart;
};
inline constexpr std::array<OutOfPlaneColumn, 6> kOutOfPlaneColumns{{
    {.key = "yaw_rate", .measure = "yaw_rate", .counterpart = "pitch_rate"},
    {.key = "roll_rate", .measure = "roll_rate", .counterpart = "pitch_rate"},
    {.key = "acceleration_y", .measure = "acceleration_y", .counterpart = "acceleration_x"},
    {.key         = "acceleration_bodyy",
     .measure     = "acceleration_bodyy",
     .counterpart = "acceleration_bodyx"},
    {.key = "position_y", .measure = "position_y", .counterpart = "position_x"},
    // The direction of the lateral position is atan2() of its two components.
    {.key = "position_direction", .measure = "position_y", .counterpart = "position_x"},
}};

/// Which columns of the golden time series @p table are noise-dominated out-of-plane columns
/// (kOutOfPlaneColumns, kNoiseRatio), by the index of the column. The golden files alone decide
/// it: nothing depends on the run that is compared with them.
[[nodiscard]] std::vector<bool> noiseColumns(const GoldenTable& table);

/// The rule of the noise-dominated out-of-plane columns for the golden time series of one
/// branch: such a column is compared in the rows on the launch rod, where it is as reproducible
/// as every other column, and in no row after them.
struct RodOnlyRule
{
    std::size_t rows{0};  ///< the rows of the golden time series
    /// Its first rodRows rows are on the launch rod (rowsUpToTheClearance(): the records on
    /// the rod and the first one after it; every row of a simulation that never clears the rod).
    std::size_t       rodRows{0};
    std::vector<bool> noise;  ///< noiseColumns() of the time series

    /// Whether column @p column is a noise-dominated one (false for a column the time series
    /// does not have).
    [[nodiscard]] bool isNoise(std::size_t column) const;

    /// Whether the rule excludes the value of column @p column in row @p row.
    [[nodiscard]] bool excludes(std::size_t column, std::size_t row) const;

    /// In how many of the first @p compared rows of a branch column @p column is compared.
    [[nodiscard]] std::size_t comparedRows(std::size_t column, std::size_t compared) const;

    /// Whether the rule excludes the minimum or maximum @p extreme of column @p column, whose
    /// values are @p values (of the golden time series, or of a run): it does when the column
    /// attains the extreme only in rows in which the rule excludes it. (An extreme that is no
    /// value of the column at all, NaN among them, is not excluded: it is compared.)
    [[nodiscard]] bool excludesExtreme(std::size_t column, std::span<const double> values,
                                       double extreme) const;

    /// The number of noise-dominated columns.
    [[nodiscard]] std::int64_t noiseColumnCount() const;

    /// The number of values the rule excludes: those of the noise-dominated columns in the rows
    /// off the launch rod.
    [[nodiscard]] std::int64_t excludedValues() const;
};

/// The rule for the golden time series @p table of a branch of a simulation whose rocket clears
/// the launch rod at @p cleared (goldenRodClearance(); nullopt: never).
[[nodiscard]] RodOnlyRule rodOnlyRule(const GoldenTable&           table,
                                      const std::optional<double>& cleared);

/// The minimum or maximum of a column of a branch on both sides of the comparison: the golden
/// number (the values of its column are those of the golden time series) and the run's, with the
/// values of the run's column.
struct ColumnExtreme
{
    std::size_t             column{0};    ///< the index of the column in the golden time series
    double                  expected{0};  ///< the golden minimum or maximum
    double                  actual{0};    ///< the run's
    std::span<const double> values;       ///< the values of the run's column
};

/// What the comparison of the default-step set does with the minimum or maximum @p extreme of a
/// column of a branch whose golden time series is @p table, with the rule @p rule. In a branch
/// that is not reproducible as a whole (@p whole) it is sensitive, whatever the column. In one
/// that is, it is compared, unless the column is a noise-dominated out-of-plane one and the
/// golden column or the run's attains the extreme only off the launch rod
/// (RodOnlyRule::excludesExtreme()): then it is one of the values the rule excludes. (The strict
/// comparison compares what is sensitive as well, and its rule names no noise column.)
[[nodiscard]] Treatment extremeTreatment(const GoldenTable& table, const RodOnlyRule& rule,
                                         const ColumnExtreme& extreme, bool whole);

// =============================================================================== sensitivity

/// Whether a value is reproducible: the perturbed run's value @p twin is within 1/@p margin of
/// @p tolerance of the run's value @p actual (NaN equals NaN, and an infinity the same
/// infinity). The margin is kSensitivityMargin or kFlightSensitivityMargin (sensitivityOf()).
[[nodiscard]] bool reproducible(double actual, double twin, double tolerance, double margin);

/// How far the time series of a branch is reproducible: up to the first row of which the
/// perturbed run moves a value by more than its tolerance over the margin of the row (see
/// sensitivityOf()). A noise-dominated out-of-plane column counts in the rows on the launch rod
/// only (RodOnlyRule): the perturbed run says nothing about such a column after them, and the
/// comparison does not compare it there.
struct Horizon
{
    std::size_t rows{0};       ///< the rows [0, rows) are reproducible
    bool        whole{false};  ///< every row is, and the perturbed run has as many rows
    /// The time of the last reproducible row: what the branch records up to it is reproducible.
    double time{-std::numeric_limits<double>::infinity()};
};

/// The data types of @p branch in OpenRocket's order, without the excluded ones
/// (SimulationDumper.csvTypes()).
[[nodiscard]] std::vector<const FlightDataType*> csvTypes(const FlightDataBranch& branch);

/// The largest finite magnitude of column @p column of @p table; 0 when it has no such column.
[[nodiscard]] double columnScale(const GoldenTable& table, std::size_t column);

/// How far a run is reproducible: the horizons of its branches, and whether the whole run is.
struct Sensitivity
{
    std::vector<Horizon> horizons;
    bool                 whole{false};
};

/// The margins of the sensitivity analysis: a value is reproducible when the perturbed run
/// moves it by no more than its tolerance divided by the margin of its row. Those of the test
/// rockets are the default; the examples have a margin of the free flight of their own, from
/// their own measurement (example_simulation_golden_tests.cpp).
struct SensitivityMargins
{
    double rod{kSensitivityMargin};           ///< of a row on the launch rod
    double flight{kFlightSensitivityMargin};  ///< of a row after it, in a run that is not whole
    /// Whether a run that the perturbed run reproduces to its last row at the margin of the rod
    /// is whole as it is, whatever the margin of the free flight (the test rockets: the Falcon 9
    /// Heavy). Without it a run is whole only when every row of it is reproducible at the margin
    /// of its own.
    bool steadyRunIsWhole{true};
};

/// The sensitivity of @p run, measured with the perturbed run @p twin against the tolerances of
/// the comparison with the golden files @p golden. A row on the launch rod is reproducible at
/// the margin of the rod (of @p margins: kSensitivityMargin unless said otherwise). When every
/// row of every branch is at that margin (in a perturbed run with as many rows, branches and
/// jitter replacements and the same status), the run is whole: nothing grows in it (unless
/// @p margins say that such a run is judged like any other). In any other run a row after the
/// launch rod is reproducible at the margin of the free flight (kFlightSensitivityMargin unless
/// said otherwise), and the first row that is not ends the reproducible part of its branch.
[[nodiscard]] Sensitivity sensitivityOf(const GoldenFiles& golden, const SimulationRun& run,
                                        const SimulationRun&      twin,
                                        const SensitivityMargins& margins = {});

// ================================================================================ comparison

/// What a comparison works with and collects.
struct Comparison
{
    std::string      context;    ///< "testrocket-beta/sim_02_b4-3-d21-0"
    SimulationCounts compared;   ///< what was compared with the golden files
    SimulationCounts sensitive;  ///< the numbers, rows and values that are not reproducible
    /// The values of the noise-dominated out-of-plane columns off the launch rod, and the
    /// minima and maxima those columns attain only there (RodOnlyRule).
    SimulationCounts excluded;
    std::string      report;  ///< the mismatches
    /// Whether everything is compared, reproducible or not, and the events in the order they
    /// were recorded in (the strict comparison, which no implementation passes).
    bool strict{false};
    /// ComparisonMode::droppedStagesTumbleFreely.
    bool          droppedStagesTumbleFreely{false};
    Measurements* measurements{nullptr};
};

/// The larger of @p scale and the magnitudes of @p expected and @p actual, as far as finite.
[[nodiscard]] double referenceOf(double scale, double expected, double actual);

// ----------------------------------------------------------------------------------- options

/// The keys of the golden "harness" object of a simulation of the default-step set: what
/// makeReproducible() changed.
inline constexpr std::array<std::string_view, 6> kHarnessKeys{
    "documentRandomSeed",
    "documentAverageWindStandardDeviation",
    "documentMultiLevelWindStandardDeviations",
    "randomSeed",
    "windStandardDeviation",
    "pitchYawJitterRemoved"};

/// ... and of a simulation of the stable-step set, which also records its time step
/// (useStableTimeStep()).
inline constexpr std::array<std::string_view, 8> kStableHarnessKeys{
    "documentRandomSeed",
    "documentAverageWindStandardDeviation",
    "documentMultiLevelWindStandardDeviations",
    "randomSeed",
    "windStandardDeviation",
    "pitchYawJitterRemoved",
    "documentTimeStep",
    "timeStep"};

// ---------------------------------------------------------------------------------- warnings

/// Compares the warning @p actual with the golden warning @p expected. A warning with a
/// parameter (a speed or an angle the simulation computed, which its text prints, and which a
/// later warning of the same kind replaces) is compared in full when the whole run is
/// reproducible (@p whole), and by its class, priority and sources otherwise.
void compareSimulationWarning(GoldenMismatches& m, Comparison& c, const std::string& field,
                              const nlohmann::json& expected, const Warning& actual, bool whole,
                              const Rocket& rocket);

// ------------------------------------------------------------------------------------ events

/// The golden path of @p component, or "null" for none.
[[nodiscard]] std::string pathOrNull(const RocketComponent* component);

/// What decides the place of an event in the comparison.
struct EventKey
{
    bool        ignition{false};
    double      time{0};
    std::string source;
};

/// The order in which events are compared: as they were recorded, except that consecutive
/// IGNITION events with the same time are sorted by the path of their source. One event
/// ignites the motors of several mounts at once; OpenRocket queues them in the order of a hash
/// map, which changes from one run of OpenRocket to the next, and QtRocket in the order of the
/// component tree. (@p recorded: the order they were recorded in, for the strict comparison.)
[[nodiscard]] std::vector<std::size_t> comparisonOrder(const std::vector<EventKey>& keys,
                                                       bool                         recorded);

/// The events of @p branch in the order of the comparison.
[[nodiscard]] std::vector<const FlightEvent*> orderedEvents(const FlightDataBranch& branch,
                                                            bool                    recorded);

/// The golden events @p events in the order of the comparison.
[[nodiscard]] std::vector<const nlohmann::json*> orderedGoldenEvents(const nlohmann::json& events,
                                                                     bool recorded);

/// The types and sources of @p events, one after the other: "LAUNCH(/) IGNITION(/0/1/2) ...".
[[nodiscard]] std::string sequenceOf(const std::vector<const FlightEvent*>& events);

/// sequenceOf() of golden events.
[[nodiscard]] std::string sequenceOf(const std::vector<const nlohmann::json*>& events);

// ----------------------------------------------------------------------------------- columns

/// Whether @p actual differs from the golden @p expected by more than @p tolerance: NaN equals
/// NaN, and an infinity the same infinity.
[[nodiscard]] bool differs(double expected, double actual, double tolerance);

// -------------------------------------------------------------------------------- simulation

/// What the comparison of a run with its golden files compared and found.
struct SimulationComparison
{
    SimulationCounts compared;
    SimulationCounts sensitive;
    SimulationCounts excluded;  ///< see Comparison::excluded
    std::string      report;    ///< the mismatches, empty when everything matched
};

/// How compareSimulation() compares.
struct ComparisonMode
{
    bool randomConfigurationId{false};  ///< the maker draws the configuration's id
    /// See Comparison::strict. The strict comparison knows no rule either: it compares the
    /// noise-dominated out-of-plane columns in every row.
    bool          strict{false};
    Measurements* measurements{nullptr};  ///< where the differences are recorded, or null
    /// A simulation of the stable-step set: its "harness" also records the time step.
    bool stableSet{false};
    /// The rule of the tumbling dropped stages, of the comparison of the examples (its
    /// measurement is at the top of example_simulation_golden_tests.cpp): in the branch of a
    /// dropped stage that flies on without a recovery device (isUnrecoveredDroppedStage()) the
    /// TUMBLE events after its separation are compared on neither side. Whether such a stage
    /// tumbles, and whether before or after its apogee, is decided by the last bits of its
    /// attitude: OpenRocket's own runs of one simulation differ in it. The golden events that
    /// are left out are counted as sensitive, each with its time; the strict comparison knows
    /// no such rule.
    bool droppedStagesTumbleFreely{false};
};

/// Compares @p run with the golden files @p golden of its simulation; @p sensitivity says what
/// of the run is reproducible, and @p context names the simulation in the report.
[[nodiscard]] SimulationComparison compareSimulation(const GoldenFiles&    golden,
                                                     const SimulationRun&  run,
                                                     const Sensitivity&    sensitivity,
                                                     const std::string&    context,
                                                     const ComparisonMode& mode);

// ============================================================================== the goldens

/// The manifest, read once per test process.
[[nodiscard]] const Result<GoldenManifest>& manifest();

/// The golden input of @p maker, or null (also when the manifest cannot be read).
[[nodiscard]] const GoldenInput* inputOf(const TestRocketMaker& maker);

/// When the rocket of the golden simulation @p document clears the launch rod (nullopt: it never
/// does).
[[nodiscard]] std::optional<double> goldenRodClearance(const nlohmann::json& document);

/// The number of rows of the golden time series @p table up to the time @p cleared at which the
/// launch rod is cleared: the records on the rod and the first one after it (the LAUNCHROD event
/// has the time of that record, a number of the same file). Every row for nullopt, a simulation
/// that never clears the rod.
[[nodiscard]] std::size_t rowsUpToTheClearance(const GoldenTable&           table,
                                               const std::optional<double>& cleared);

/// The floor under the reproducible part of the golden simulation @p files: the rows of its
/// branches up to the clearing of the launch rod (every row when it never clears it).
[[nodiscard]] std::int64_t floorRows(const GoldenFiles& files);

/// Whether the golden branch @p branch (an entry of "branches" of a simulation document) is
/// that of a dropped stage that flies on without a recovery device: a branch after the first,
/// whose events have a STAGE_SEPARATION and no RECOVERY_DEVICE_DEPLOYMENT. Such a stage is left
/// to its attitude, and tumbles or does not (ComparisonMode::droppedStagesTumbleFreely).
[[nodiscard]] bool isUnrecoveredDroppedStage(const nlohmann::json& branch);

/// The golden TUMBLE events of @p branch that the rule of the tumbling dropped stages leaves
/// out of the comparison: those after the last STAGE_SEPARATION of the branch of an
/// unrecovered dropped stage; none of any other branch.
[[nodiscard]] int tumblesOfADroppedStage(const nlohmann::json& branch);

/// The values of the rows of floorRows(): every column of every such row. A noise-dominated
/// out-of-plane column is compared in those rows as well, so a comparison whose reproducible
/// parts hold the floor has compared at least this many values.
[[nodiscard]] std::int64_t floorValues(const GoldenFiles& files);

// ============================================================ a run next to its golden files

/// A golden simulation compared with QtRocket's run of it.
struct GoldenRun
{
    std::string problem;  ///< why there is no comparison, else ""
    std::string context;  ///< "<input>/sim_<NN>_<name>"
    /// How the run was compared (compareWithItsFiles()), and of it whether the maker of the
    /// rocket draws the configuration's id.
    ComparisonMode       mode;
    bool                 randomConfigurationId{false};
    GoldenFiles          files;
    SimulationRun        run;
    SimulationRun        twin;  ///< the perturbed run
    Sensitivity          sensitivity;
    SimulationCounts     golden;
    SimulationComparison comparison;
};

/// Compares the run of @p result with its files, which are those of the simulation
/// @p simulation of the golden input named @p input: names the simulation (the context),
/// measures how far the run is reproducible with its perturbed run (sensitivityOf() with
/// @p margins), and compares it with the files under @p mode (compareSimulation()), after the
/// name and the file the manifest lists for it. The files and the two runs of @p result are
/// there already.
void compareWithItsFiles(GoldenRun& result, std::string_view input,
                         const GoldenSimulation& simulation, const ComparisonMode& mode,
                         const SensitivityMargins& margins = {});

/// What is wrong with the reproducible part of @p run, "" when nothing is: a simulation that
/// never clears the launch rod has to be reproducible as a whole, and a flight in every branch
/// at least until the rod is cleared. The floor is stated in rows, counted in the golden file
/// alone: the reproducible rows of a branch must be at least the golden rows up to the
/// clearance (comparing the time of the run's last reproducible row with the golden time of the
/// clearance would compare two numbers of different origin for equality).
[[nodiscard]] std::string floorProblem(const GoldenRun& run);

/// What the rule of the out-of-plane noise columns (RodOnlyRule) decides for golden
/// simulations, from their files alone.
struct NoiseCounts
{
    std::int64_t columns{0};  ///< the noise-dominated out-of-plane columns
    std::int64_t values{0};   ///< their values off the launch rod, which are not compared

    [[nodiscard]] bool operator==(const NoiseCounts&) const = default;

    NoiseCounts& operator+=(const NoiseCounts& other)
    {
        columns += other.columns;
        values += other.values;
        return *this;
    }
};

/// "150 noise columns with 63406 values off the rod", for the messages of the tests.
std::ostream& operator<<(std::ostream& out, const NoiseCounts& counts);

/// What the rule decides for the branches of the golden simulation @p files.
[[nodiscard]] NoiseCounts noiseCounts(const GoldenFiles& files);

/// What is wrong with what the rule of the out-of-plane noise columns excluded from the
/// comparison of @p run, "" when nothing is: nothing at all in a simulation that never clears
/// the launch rod (it has no row off the rod), and of the minima and maxima, in the branches
/// that are reproducible as a whole, at least those that the golden column attains only off
/// the launch rod (the files alone say so) and at most two per noise column (the run's column
/// can attain its own only there as well); in any other branch none, since its minima and
/// maxima are sensitive. (A comparison that excluded none of them, or every one of every
/// column, would add up to what the files hold all the same.)
[[nodiscard]] std::string exclusionProblem(const GoldenRun& run);

/// What the simulations of one golden input hold, and what their comparison compared and found.
struct InputResult
{
    std::string      problems;  ///< a simulation that could not be run, a floor that does not hold
    SimulationCounts golden;
    SimulationCounts compared;
    SimulationCounts sensitive;
    SimulationCounts excluded;  ///< what the rule of the out-of-plane noise columns excludes
    std::string      report;
    int              flights{0};      ///< the simulations whose rocket clears the launch rod
    int              whole{0};        ///< the simulations that are reproducible as a whole
    std::int64_t     floorRows{0};    ///< floorRows() of the simulations
    std::int64_t     floorValues{0};  ///< floorValues() of the simulations
    NoiseCounts      noise;           ///< noiseCounts() of the simulations
};

/// Adds the compared run @p run to the sums of its input, @p result: what its files hold, what
/// was compared, is sensitive and is excluded, its report, and among the problems why it has
/// no comparison (@p file names its document there), a floor that does not hold
/// (floorProblem()) and an exclusion that is not the rule's (exclusionProblem()).
void addComparedRun(InputResult& result, const GoldenRun& run, std::string_view file);

// -------------------------------------------------------------------------- the measurements

/// "branch 0: 37 of 796 rows (until t = 0.2507 s); the run has 848 rows, the perturbed run 794"
/// for every branch of @p run that is not reproducible as a whole.
[[nodiscard]] std::string sensitivityText(const GoldenRun& run);

/// One line of the overview of the measurement: the apogee (the maximum altitude) and the flight
/// time of @p run and of its golden file, the rows of their branches, and the largest difference
/// of a value of the time series (over the rows both have), as a fraction of its column's scale.
[[nodiscard]] std::string overviewLine(const GoldenRun& run, const Measurements& measurements);

/// The row profiles of the branches of @p run that the run, the perturbed run and the golden
/// files all have: one line per row, up to the first row that no margin makes reproducible (the
/// perturbed run moves a value of it by more than the tolerance of the comparison itself,
/// kValueRelative of the scale of its column), with the largest difference of the perturbed
/// run from the run, which decides whether the row is reproducible, and the largest difference
/// of the run from the golden file, which is what the comparison finds; both as fractions of
/// the scales of their columns, each with its column, over the columns that are compared in
/// the row (not a noise-dominated out-of-plane column after the launch rod).
[[nodiscard]] std::string runProfile(const GoldenRun& run);

// ============================================================ the scaffolding of the tests

/// The test name of @p paramInfo's maker: its golden input with '-' as '_'.
[[nodiscard]] std::string makerTestName(const ::testing::TestParamInfo<TestRocketMaker>& paramInfo);

/// The simulations the mutations change: their golden input and their index in it.
struct Subject
{
    std::string_view input;
    std::size_t      index;
};

/// Multiplies the number at @p pointer by 1 + @p relative.
void scale(GoldenFiles& files, std::string_view pointer, double relative);

/// Adds @p offset to the number at @p pointer.
void shift(GoldenFiles& files, std::string_view pointer, double offset);

/// Adds @p relative times the scale of the column to the value of column @p key in row @p row
/// of the time series of the first branch.
void shiftSeries(GoldenFiles& files, std::string_view key, std::size_t row, double relative);

/// A change to the golden files of a simulation that the comparison has to report, in one line.
struct Mutation
{
    std::string_view name;              ///< what is changed; it names the change in a failure
    Subject          subject;           ///< the simulation
    void (*apply)(GoldenFiles& files);  ///< makes the change
    std::string_view heading;           ///< what follows the context in the report's heading
    std::string_view line;              ///< what its one line starts with
};

/// A simulation the mutations change, with the changes within the tolerances that go with it
/// (null: none). The tests are per simulation, not per mutation: ctest starts a process per
/// test, and a process runs the simulation (twice) before it can compare anything.
struct SubjectCase
{
    std::string_view name;  ///< it names the tests
    Subject          subject;
    void (*withinTheTolerances)(GoldenFiles& files);
};

/// The name of @p subjectCase, for the messages of the tests.
std::ostream& operator<<(std::ostream& out, const SubjectCase& subjectCase);

/// The test name of @p paramInfo's simulation.
[[nodiscard]] std::string subjectTestName(const ::testing::TestParamInfo<SubjectCase>& paramInfo);

/// The names of @p all, sorted.
[[nodiscard]] std::vector<std::string_view> sortedNames(const std::vector<Mutation>& all);

/// What the comparison of @p run with a copy of its golden files finds after @p change changed
/// the copy (null: nothing changed), under the mode the run was compared under.
[[nodiscard]] SimulationComparison comparisonAfter(const GoldenRun& run,
                                                   void (*change)(GoldenFiles& files));

/// What is wrong with the report of @p mutation, applied to a copy of the files of @p run: ""
/// when the report is the heading and the one line that name the change.
[[nodiscard]] std::string mutationProblem(const Mutation& mutation, const GoldenRun& run);

/// A golden warning as the dumper writes it (Values.warning()), for a warning without sources.
[[nodiscard]] nlohmann::json goldenFormOf(const Warning& warning);

/// The configuration of an extension as the dumper writes it (SimulationDumper.extensions()):
/// an object of its keys with a Boolean as true or false, a number as a number (a NaN or an
/// infinity as the strings the golden files have for them), a String as a string and a List as
/// an array of such values. (The dumper cannot write a Byte or a BigDecimal; here the one is an
/// integer and the other its double value.)
[[nodiscard]] nlohmann::json goldenFormOf(const Config& config);

/// An entry of the golden "extensions" as the dumper writes it: the id of @p extension, its
/// type, its name and its configuration. The type is Java's simple name of its class, which
/// for every extension OpenRocket provides is the last component of its id (the id is the name
/// of the class), and that is what is taken here.
[[nodiscard]] nlohmann::json goldenFormOf(const SimulationExtension& extension);

}  // namespace QtRocket::Test
