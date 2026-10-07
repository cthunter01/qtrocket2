#pragma once

// Running the golden simulations and comparing them with their files (tests/data/goldens/<input>/
// sim_<NN>_<name>.json and the branch CSV files next to them, written by tools/openrocket-goldens:
// GoldenDumper.java and SimulationDumper.java; the format is in that tool's README.md). Test-only.
//
// What is here is what the golden tests of the simulations share: the tolerances of the
// default-step set, the harness's set-up of a simulation of a test rocket (the options of a fresh
// installation, the variants, makeReproducible()), the run (with the jitter removal and, for the
// sensitivity analysis, a perturbation), the golden files of a simulation and what they hold, the
// measurements, the sensitivity analysis, the comparison of a run with its files
// (compareSimulation()), the manifest and the floor under the reproducible part, and the
// scaffolding of the mutation tests.
//
// simulation_golden_tests.cpp compares the default-step set with it (time step 0.05 s: what is
// reproducible of a flight, and why no more is, is described at the top of that file) and
// simulation_stable_golden_tests.cpp the stable-step set (0.01 s, whole flights, with rules of its
// own that live in that file). The comparison of the default-step set, compareSimulation(), also
// compares everything of a stable-step simulation that is not a number of the trajectory.
//
// The set-up and the run are those of a test rocket of tests/core/rocket/TestRockets.h
// (runSimulation() builds the rocket and the harness's document from a TestRocketMaker).

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
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
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
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
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

/// A value of a time series is reproducible when the perturbed run moves it by no more than its
/// tolerance divided by this (1e-12 of the scale of its column).
inline constexpr double kSensitivityMargin = 1000.0;

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

/// One golden simulation as QtRocket runs it.
struct SimulationRun
{
    std::string                 problem;  ///< why there is no run, else ""
    PlannedSimulation           planned;
    std::unique_ptr<Rocket>     rocket;      ///< the caller's rocket
    std::unique_ptr<Simulation> simulation;  ///< with the options the run used
    nlohmann::json              harness;     ///< what makeReproducible() changed
    std::string                 status;      ///< "completed" or "exception"
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

/// The largest differences from the golden files, by simulation and by what was compared; see
/// SimulationGoldenMeasurement.
class Measurements
{
public:
    /// A difference from a golden value: @p difference against the tolerance @p tolerance, of a
    /// value that was compared, or (@p sensitive) one that was not.
    void record(const std::string& context, const std::string& what, bool sensitive,
                double difference, double tolerance)
    {
        Entry& entry = m_entries[std::format("{}\t{}\t{}", context, what,
                                             sensitive ? "sensitive" : "compared")];
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
    /// its golden value, whether compared or sensitive, as a multiple of its tolerance (which is
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

// =============================================================================== sensitivity

/// Whether a value is reproducible: the perturbed run's value @p twin is within
/// 1/kSensitivityMargin of @p tolerance of the run's value @p actual (NaN equals NaN, and an
/// infinity the same infinity).
[[nodiscard]] bool reproducible(double actual, double twin, double tolerance);

/// How far the time series of a branch is reproducible.
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

/// The sensitivity of @p run, measured with the perturbed run @p twin against the tolerances of
/// the comparison with the golden files @p golden.
[[nodiscard]] Sensitivity sensitivityOf(const GoldenFiles& golden, const SimulationRun& run,
                                        const SimulationRun& twin);

// ================================================================================ comparison

/// What a comparison works with and collects.
struct Comparison
{
    std::string      context;    ///< "testrocket-beta/sim_02_b4-3-d21-0"
    SimulationCounts compared;   ///< what was compared with the golden files
    SimulationCounts sensitive;  ///< the numbers, rows and values that are not reproducible
    std::string      report;     ///< the mismatches
    /// Whether everything is compared, reproducible or not, and the events in the order they
    /// were recorded in (the strict comparison, which no implementation passes).
    bool          strict{false};
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
    std::string      report;  ///< the mismatches, empty when everything matched
};

/// How compareSimulation() compares.
struct ComparisonMode
{
    bool          randomConfigurationId{false};  ///< the maker draws the configuration's id
    bool          strict{false};                 ///< see Comparison::strict
    Measurements* measurements{nullptr};         ///< where the differences are recorded, or null
    /// A simulation of the stable-step set: its "harness" also records the time step.
    bool stableSet{false};
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

/// A golden warning as the dumper writes it (Values.warning()), for a warning without sources.
[[nodiscard]] nlohmann::json goldenFormOf(const Warning& warning);

}  // namespace QtRocket::Test
