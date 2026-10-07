// Simulation golden tests: the simulations of the thirteen rockets of tests/core/rocket/
// TestRockets.h against what OpenRocket's BasicEventSimulationEngine computes for the Java rockets,
// in tests/data/goldens/testrocket-<name>/sim_<NN>_<name>.json and the branch CSV files next to
// them (tools/openrocket-goldens: GoldenDumper.java and SimulationDumper.java; the format is in
// that tool's README.md). 50 simulations with 53 branches, 437 events and 22330 rows.
//
// There are two sets of these files. The default-step set, which this file compares, holds the
// simulations as a fresh installation runs them, with a time step of 0.05 s: 30 of its flights
// are not reproducible beyond their first tenths of a second, and are compared so far. The
// stable-step set (testrocket-<name>/stable/, the same simulations with a time step of 0.01 s
// and nothing else changed, 35323 rows) is compared over the whole flights, by
// simulation_stable_golden_tests.cpp. Both files work with GoldenSimulations.h, which holds what
// this comment describes of the set-up, the run, the sensitivity analysis and the comparison.
//
// The set-up is the harness's (GoldenDumper.dumpTestRocket()): one simulation per flight
// configuration in the rocket's order, the default configuration first, then the three variants
// of GoldenDumper.VARIANTS; the options of a fresh installation (applicationDefaultOptions():
// DefaultSimulationOptionFactory over an empty preferences store, and what the harness sets on
// top, the multi-level wind level included), then the variant's change, then
// SimulationDumper.makeReproducible() (seed 0, fixed; zero standard deviation of every wind
// model). The run is the harness's too (SimulationDumper.dump()): toSimulationConditions(),
// setSimulation(), the forces listener of the jitter removal first, the extensions (none), its
// conditions listener last, a new engine, simulate().
//
// The jitter removal is tests/core/simulation/JitterRemoval.h, the port of the harness's
// JitterRemoval.java that the stepper tests use as well: two system listeners with the harness's
// placement, the harness's extra calculator call and its counting. Java tells the hook the
// Runge-Kutta steppers fire from the one the landing and tumble steppers fire by walking the call
// stack; the listener API has no such thing, so the port tells them by the forces handed to the
// hook, which hold a normal force coefficient only when the aerodynamic calculator made them
// (the Euler steppers build theirs from a drag coefficient and leave it NaN).
//
// Compared (SimulationGolden, one test per rocket), per simulation:
// - its index, name and files in the manifest, the flight configuration (index, id, name), the
//   source of the options, the variant, that it has no extensions and was not skipped;
// - the options the run used with the golden "options", field by field and exactly (they are
//   settings; the one that is computed, the rod direction of a launch into a multi-level wind,
//   to 1e-12), and what makeReproducible() changed with the golden "harness";
// - "result": the status, the exception's type and message, the number of jitter replacements;
// - "summary": the ten values and the number of branches; "warnings", in order;
// - per branch: its index, name and source component (as a golden path), the number of rows, the
//   optimum altitude, the time to it, the optimum delay, the separation time, the name of its
//   file, the excluded columns; the columns in order (key, name, symbol, whether built in,
//   minimum and maximum); the events in order (type and source as one text, then the time and
//   the data of each by its kind: the motor, the abort, the warning as the warning set holds it,
//   the text); and the time series, every column of every row. NaN equals NaN.
// A golden field that nothing compares is reported (noteUncomparedKeys()); the file's "schema",
// "schemaVersion" and "input", and the agreement of a CSV file with its branch, are checked by
// goldens_schema_tests.cpp.
//
// WHAT CAN BE COMPARED. OpenRocket's results for 30 of the 50 simulations are not reproducible,
// by OpenRocket itself or by anything else. At the installation's time step of 0.05 s the pitch
// oscillation of these light rockets is integrated beyond the stability limit of the Runge-Kutta
// method (natural frequency times step up to 4.4), and the step size control that follows the
// oscillation amplifies a difference in the last bit by eleven orders of magnitude within a
// second of flight. Measured:
// - OpenRocket against itself, with nothing changed but the random ids of the components (the
//   order in which its hash maps are summed): the [C6-5] flight of the Estes Alpha III has 796,
//   803 or 817 rows and its apogee at 5.954 s, 5.967 s or 5.971 s; the [C6-3] flight of the one
//   with the inline pod reaches 102.63 m/s in the golden file and 102.77 m/s to 102.80 m/s in
//   eight other runs. The golden files are one such run each: in each of eight dumps of the
//   harness with another id sequence (UUID_SALT of generate.sh), 15 to 20 of the 50 golden
//   simulations have another number of rows (21 of them in at least one dump), 10 to 13
//   another maximum velocity (by more than 1e-6) and 7 another apogee time (by more than 1 ms).
// - QtRocket against the golden files: equal to the last bit while the rocket is on the launch
//   rod (at most 2e-15 of a column's scale at the first record after it), 1e-12 some 5 to 70
//   records later, 1e-9 after 30 to 180 records, then different row counts (20 simulations),
//   maximum velocities (up to 1.8e-3), apogee times (up to 0.045 s) and ground hits (0.14 s).
// - QtRocket against itself under a libm whose results are moved by one ulp (the shim of the
//   aerodynamic goldens, in six patterns): the same growth, and row counts that change with the
//   pattern in 21 simulations ([C6-5]: 848 rows without the shim and 862, 802, 813, 806, 796 or
//   787 with it; 796 is the golden number).
// So no tolerance, and none within the plan's bounds (event times 1e-3 s, apogee and maximum
// velocity relative 1e-4), holds for the later part of such a flight on every platform, and the
// comparison does not pretend otherwise. It measures what is reproducible and compares that:
// - Every simulation is run twice: as the harness runs it, and once more with a last-bit
//   perturbation (LastBitListener: after every step each component of the velocity and of the
//   rotation velocity is moved to the next representable number).
// - A row of a branch is reproducible while every value of it, in the two runs, is within
//   1/kSensitivityMargin of the tolerance of its column; the first row that is not ends the
//   reproducible part of the branch (its horizon). A branch whose every row is reproducible, in
//   a run with as many rows, branches, jitter replacements and the same status, is whole.
// - Compared with the golden files, at the tolerances below: the rows of the reproducible part;
//   the time of an event the branch recorded in its reproducible part, or that the reproducible
//   part fixes (the LAUNCH, and the BURNOUT of a motor that ignited in it: see
//   isTimedInTheReproduciblePart()), and the separation time when the stage separated in it; the
//   launch rod velocity when the rod is cleared in the reproducible part; for a whole branch its
//   number of rows, optimum altitude and delay
//   and the minimum and maximum of every column; for a whole run the other summary values, the
//   number of jitter replacements (it counts the force calculations of the Runge-Kutta steppers,
//   the nested optimum-coast runs included) and the parameter, description and text of a warning
//   that has a parameter (a speed or an angle, which the text prints).
// - Always compared, in every simulation: everything that is not a number of the trajectory:
//   the options, the result's status, the branches, their names, sources and columns, the events
//   with their types, sources and data, the warnings with their classes, priorities and sources.
// - What is neither is counted as sensitive, and the sums are checked: compared plus sensitive is
//   what the files hold (in the test of each rocket; SimulationGoldenCoverage pins what the
//   files of the thirteen rockets hold in all). On Linux 20 simulations are whole (the 19
//   that never leave the launch rod and the Falcon 9 Heavy, a flight of 148 records that ends in
//   a tumble under thrust); of the 30 others the first 22 to 77 records are compared, which is
//   the launch rod and the first 0.04 s to 0.3 s of free flight; in all, 1670 of the 22330 rows.
// The test of each rocket also holds the floor under this, so that a calculation that became
// sensitive throughout could not pass by comparing nothing: a simulation that never clears the
// launch rod is whole, and every flight is reproducible at least until it has cleared the rod
// (in rows: the reproducible rows of a branch are at least the golden rows up to the clearing,
// 794 in the 31 flights and 26 in the 19 other simulations).
// SimulationGoldenStrict holds the comparison the plan asked for (everything, the rows exactly,
// the events in the order they were recorded in), disabled: it cannot pass.
//
// One more difference is not a matter of rounding: an event that ignites the motors of several
// mounts queues their IGNITION events in the order of a hash map in OpenRocket (it changes from
// one run of OpenRocket to the next) and in the order of the component tree here. Consecutive
// IGNITION events with the same time are therefore compared as a set (comparisonOrder()); 5
// golden simulations record such a pair the other way round, and each of the 10 simulations
// with such a pair (the flights of the two Estes Alpha III with a second motor mount) records
// it in either order in some dump of OpenRocket with other ids.
//
// Tolerances. A value of the time series: 1e-9 of the scale of its column (the largest magnitude
// of the golden column), which also is the measure for the minimum and maximum; another value:
// 1e-9 of its magnitude; a time: 1e-6 s. Measured over what is compared (the values a
// perturbation of one ulp per step moves by less than 1e-12): the largest difference from the
// golden files is 3.0e-12 of the scale on Linux with glibc, and 2.9e-12 to 8.0e-12 under the
// one-ulp libm shim (always up, always down, and four random patterns), so the tolerance is 124
// times the largest measurement and five orders of magnitude inside the plan's bounds. The times
// compared differ by at most 2e-15 s; their tolerance covers what a time the motors set can
// differ by, which is the rounding of the simulation time it is counted from (6e-11 s between
// OpenRocket and QtRocket in a run of the three-stage rocket with a time step of 0.01 s).
// SimulationGoldenMeasurement, disabled, prints the table of the differences per simulation and
// column, compared and sensitive.
//
// Not vacuous: SimulationGoldenMutation changes one golden value at a time (a setting, a value
// of the time series, a time, a summary value, a row count), removes an event and swaps two, and
// expects the one line that reports it; a change within the tolerance is not reported, and a
// value beyond the horizon is, as said, not compared.
//
// The tests are cut by what a process has to simulate, since ctest starts one process per test
// and the runs are kept per process (goldenRun()): one test per rocket, one per simulation that
// the mutations change (not one per mutation), and none that runs the simulations of another.
//
// Not compared yet: the sixteen example-* inputs. Their simulation files are in
// tests/data/goldens, but the designs are .ork files, which need the .ork loader of the file
// tier, and five of their simulations use extensions. Their tests are to load the document and
// hand each simulation to compareSimulation() of GoldenSimulations.h.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenMismatches.h"
#include "goldens/GoldenSimulations.h"
#include "rocket/TestRockets.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using nlohmann::json;
using QtRocket::Rocket;
using QtRocket::Warning;
using QtRocket::Test::baseName;
using QtRocket::Test::compareSimulation;
using QtRocket::Test::compareSimulationWarning;
using QtRocket::Test::Comparison;
using QtRocket::Test::comparisonOrder;
using QtRocket::Test::EventKey;
using QtRocket::Test::floorRows;
using QtRocket::Test::goldenCounts;
using QtRocket::Test::GoldenFiles;
using QtRocket::Test::goldenFormOf;
using QtRocket::Test::GoldenInput;
using QtRocket::Test::goldenRodClearance;
using QtRocket::Test::GoldenSimulation;
using QtRocket::Test::GoldenTable;
using QtRocket::Test::goldenValue;
using QtRocket::Test::Horizon;
using QtRocket::Test::inputOf;
using QtRocket::Test::kTimeAbsolute;
using QtRocket::Test::kValueRelative;
using QtRocket::Test::LastBitListener;
using QtRocket::Test::lastBitMoved;
using QtRocket::Test::loadGoldenFiles;
using QtRocket::Test::makerTestName;
using QtRocket::Test::manifest;
using QtRocket::Test::Measurements;
using QtRocket::Test::Mutation;
using QtRocket::Test::plannedSimulations;
using QtRocket::Test::reproducible;
using QtRocket::Test::rowsUpToTheClearance;
using QtRocket::Test::runSimulation;
using QtRocket::Test::scale;
using QtRocket::Test::Sensitivity;
using QtRocket::Test::sensitivityOf;
using QtRocket::Test::shift;
using QtRocket::Test::shiftSeries;
using QtRocket::Test::SimulationComparison;
using QtRocket::Test::SimulationCounts;
using QtRocket::Test::SimulationRun;
using QtRocket::Test::slug;
using QtRocket::Test::sortedNames;
using QtRocket::Test::Subject;
using QtRocket::Test::SubjectCase;
using QtRocket::Test::subjectTestName;
using QtRocket::Test::TestRocketMaker;
using QtRocket::Test::testRocketMakers;

/// The comparison collector of the golden tests.
using Mismatches = QtRocket::Test::GoldenMismatches;

// ============================================================================== the goldens

/// A golden simulation compared with QtRocket's run of it.
struct GoldenRun
{
    std::string          problem;  ///< why there is no comparison, else ""
    std::string          context;  ///< "<input>/sim_<NN>_<name>"
    bool                 randomConfigurationId{false};
    GoldenFiles          files;
    SimulationRun        run;
    SimulationRun        twin;  ///< the perturbed run
    Sensitivity          sensitivity;
    SimulationCounts     golden;
    SimulationComparison comparison;
};

/// Runs simulation @p index of the golden input @p input, the one of @p maker, twice (see
/// LastBitListener) and compares it with its files.
[[nodiscard]] GoldenRun runGolden(const TestRocketMaker& maker, const GoldenInput& input,
                                  std::size_t index)
{
    GoldenRun               result;
    const GoldenSimulation& simulation = input.simulations.at(index);
    result.files                       = loadGoldenFiles(simulation);
    if (!result.files.problem.empty())
    {
        result.problem = result.files.problem;
        return result;
    }
    result.golden = goldenCounts(result.files);
    result.run    = runSimulation(maker, index, {});
    result.twin   = runSimulation(
        maker, index, {.stableTimeStep = {}, .perturbation = std::make_shared<LastBitListener>()});
    if (!result.run.problem.empty() || !result.twin.problem.empty())
    {
        result.problem = result.run.problem + result.twin.problem;
        return result;
    }
    result.context               = std::format("{}/{}", input.name, baseName(result.run.planned));
    result.randomConfigurationId = maker.randomConfigurationId;
    result.sensitivity           = sensitivityOf(result.files, result.run, result.twin);
    Mismatches m(result.context);
    m.text("name in the manifest", simulation.name, result.run.planned.name);
    m.text("file in the manifest", simulation.json, result.context + ".json");
    result.comparison =
        compareSimulation(result.files, result.run, result.sensitivity, result.context,
                          {.randomConfigurationId = maker.randomConfigurationId});
    result.comparison.report = m.report() + result.comparison.report;
    return result;
}

/// runGolden() of simulation @p index of @p maker, run once per test process and kept (std::map
/// keeps the entries where they are): the tests read the same runs. The result depends on the
/// maker and the index alone, so the order of the tests does not matter.
[[nodiscard]] const GoldenRun& goldenRun(const TestRocketMaker& maker, const GoldenInput& input,
                                         std::size_t index)
{
    static std::mutex                                    s_mutex;
    static std::map<std::string, GoldenRun, std::less<>> s_runs;
    const std::scoped_lock                               lock{s_mutex};
    const std::string key    = std::format("{}#{}", maker.input, index);
    auto              cached = s_runs.find(key);
    if (cached == s_runs.end())
    {
        cached = s_runs.emplace(key, runGolden(maker, input, index)).first;
    }
    return cached->second;
}

/// What is wrong with the reproducible part of @p run, "" when nothing is: a simulation that
/// never clears the launch rod has to be reproducible as a whole, and a flight in every branch
/// at least until the rod is cleared. The floor is stated in rows, counted in the golden file
/// alone: the reproducible rows of a branch must be at least the golden rows up to the
/// clearance (comparing the time of the run's last reproducible row with the golden time of the
/// clearance would compare two numbers of different origin for equality).
[[nodiscard]] std::string floorProblem(const GoldenRun& run)
{
    const std::optional<double> cleared = goldenRodClearance(run.files.document);
    if (!cleared.has_value())
    {
        return run.sensitivity.whole
                   ? std::string{}
                   : std::format(
                         "{}: never clears the launch rod, but is not reproducible as a "
                         "whole\n",
                         run.context);
    }
    std::string problem;
    for (std::size_t i = 0; i < run.sensitivity.horizons.size() && i < run.files.tables.size(); i++)
    {
        const std::size_t floor = rowsUpToTheClearance(run.files.tables[i], cleared);
        if (run.sensitivity.horizons[i].rows < floor)
        {
            problem += std::format(
                "{} branch {}: {} reproducible rows only, the launch rod is "
                "cleared at row {} (t = {} s)\n",
                run.context, i, run.sensitivity.horizons[i].rows, floor, *cleared);
        }
    }
    return problem;
}

/// What the simulations of one golden input hold, and what their comparison compared and found.
struct InputResult
{
    std::string      problems;  ///< a simulation that could not be run, a floor that does not hold
    SimulationCounts golden;
    SimulationCounts compared;
    SimulationCounts sensitive;
    std::string      report;
    int              flights{0};    ///< the simulations whose rocket clears the launch rod
    int              whole{0};      ///< the simulations that are reproducible as a whole
    std::int64_t     floorRows{0};  ///< floorRows() of the simulations
};

/// Runs and compares every simulation of the golden input of @p maker.
[[nodiscard]] InputResult compareInput(const TestRocketMaker& maker)
{
    InputResult        result;
    const GoldenInput* input = inputOf(maker);
    if (input == nullptr)
    {
        result.problems = std::format("no golden input named {}", maker.input);
        return result;
    }
    const std::unique_ptr<Rocket> rocket  = maker.make();
    const std::size_t             planned = plannedSimulations(*rocket, maker.input).size();
    if (planned != input->simulations.size())
    {
        result.problems += std::format(
            "{}: the harness's document has {} simulations, the "
            "manifest lists {}\n",
            maker.input, planned, input->simulations.size());
    }
    for (std::size_t i = 0; i < input->simulations.size(); i++)
    {
        const GoldenRun& run = goldenRun(maker, *input, i);
        if (!run.problem.empty())
        {
            result.problems += std::format("{}: {}\n", input->simulations[i].json, run.problem);
            continue;
        }
        result.golden += run.golden;
        result.compared += run.comparison.compared;
        result.sensitive += run.comparison.sensitive;
        result.report += run.comparison.report;
        result.flights += goldenRodClearance(run.files.document).has_value() ? 1 : 0;
        result.whole += run.sensitivity.whole ? 1 : 0;
        result.floorRows += floorRows(run.files);
        result.problems += floorProblem(run);
    }
    return result;
}

// ===================================================================================== tests

/// One test rocket of TestRockets.h: its golden simulations.
class SimulationGolden : public ::testing::TestWithParam<TestRocketMaker>
{ };

// Every simulation of the rocket is compared as far as it is reproducible: what was compared
// and what is sensitive add up to what the files hold. And the reproducible part has a floor
// (floorProblem(), among the problems), so that a calculation that became sensitive throughout
// cannot pass by comparing nothing: a simulation that never clears the launch rod is
// reproducible as a whole, and a flight in every branch at least until it has cleared the rod.
TEST_P(SimulationGolden, EverySimulationOfTheRocket)
{
    const InputResult result = compareInput(GetParam());
    ASSERT_EQ(result.problems, "");
    EXPECT_EQ(result.report, "");
    // Everything the files hold was compared or is sensitive: nothing skipped.
    EXPECT_EQ(result.compared + result.sensitive, result.golden)
        << "what was compared or is sensitive, and what the files hold";
    EXPECT_GT(result.golden.simulations, 0);
    // The structure is compared in every simulation, sensitive or not.
    EXPECT_EQ(result.sensitive.simulations + result.sensitive.branches + result.sensitive.events +
                  result.sensitive.columns + result.sensitive.warnings,
              0);
    // The floor, in sums: the simulations that never clear the rod are whole, and at least the
    // rows up to the clearing of the rod were compared.
    EXPECT_GE(result.whole, result.golden.simulations - result.flights);
    EXPECT_GE(result.compared.rows, result.floorRows);
}

INSTANTIATE_TEST_SUITE_P(Makers, SimulationGolden, ::testing::ValuesIn(testRocketMakers()),
                         makerTestName);

// ================================================================================== coverage

/// What the golden simulations of the test rockets hold, read from the files alone (no
/// simulation is run), and whether each input has a maker in TestRockets.h, whose test
/// (SimulationGolden) compares its simulations.
struct GoldenCoverage
{
    int              goldenInputs{0};  ///< the "testrocket" inputs of the manifest
    SimulationCounts golden;           ///< what their simulation files hold
    int              flights{0};       ///< the simulations whose rocket clears the launch rod
    std::int64_t     floorRows{0};     ///< floorRows() of the simulations
    std::string      problems;         ///< an input without a maker, a file that cannot be read
};

/// Adds the simulations of the golden input @p input to @p coverage.
void cover(GoldenCoverage& coverage, const GoldenInput& input)
{
    for (const GoldenSimulation& simulation : input.simulations)
    {
        const GoldenFiles files = loadGoldenFiles(simulation);
        if (!files.problem.empty())
        {
            coverage.problems += std::format("{}: {}\n", simulation.json, files.problem);
            continue;
        }
        coverage.golden += goldenCounts(files);
        coverage.flights += goldenRodClearance(files.document).has_value() ? 1 : 0;
        coverage.floorRows += floorRows(files);
    }
}

/// Reads every simulation of every "testrocket" input of the manifest.
[[nodiscard]] GoldenCoverage goldenCoverage()
{
    GoldenCoverage coverage;
    if (!manifest())
    {
        coverage.problems = manifest().error().message;
        return coverage;
    }
    const std::span<const TestRocketMaker> makers = testRocketMakers();
    for (const GoldenInput& input : manifest()->inputs)
    {
        if (input.kind != "testrocket")
        {
            continue;
        }
        coverage.goldenInputs++;
        if (std::ranges::find(makers, std::string_view{input.name}, &TestRocketMaker::input) ==
            makers.end())
        {
            coverage.problems += std::format("{}: no maker\n", input.name);
        }
        cover(coverage, input);
    }
    return coverage;
}

/// Every golden simulation of the test rockets is compared: each of the thirteen inputs has a
/// maker, and the test of each maker (SimulationGolden) checks that what it compared and what
/// is sensitive add up to what the files of its input hold, and that the floor holds. This
/// test pins what the files hold in all, and the floor in all; it runs no simulation, so that
/// the simulations are not run a second time for the sums (ctest starts a process per test).
TEST(SimulationGoldenCoverage, EveryGoldenSimulationOfTheTestRocketsHasAMaker)
{
    const GoldenCoverage coverage = goldenCoverage();
    EXPECT_EQ(coverage.problems, "");
    EXPECT_EQ(coverage.goldenInputs, 13);
    EXPECT_EQ(static_cast<std::size_t>(coverage.goldenInputs), testRocketMakers().size());
    EXPECT_EQ(coverage.golden.simulations, 50) << "one per configuration, and three variants";
    EXPECT_EQ(coverage.golden.branches, 53);
    EXPECT_EQ(coverage.golden.events, 437);
    EXPECT_EQ(coverage.golden.columns, 2826);
    EXPECT_EQ(coverage.golden.warnings, 21);
    EXPECT_EQ(coverage.golden.numbers, 6920);
    EXPECT_EQ(coverage.golden.rows, 22330);
    EXPECT_EQ(coverage.golden.values, 1562004);
    // The floor (floorProblem()): 31 flights, which are reproducible until they have cleared
    // the rod, 794 rows in all, and 19 simulations that never clear it and are reproducible as
    // a whole, 26 rows.
    EXPECT_EQ(coverage.flights, 31);
    EXPECT_EQ(coverage.floorRows, 820);
}

// ==================================================================================== strict

/// The comparison the plan asked for: every value of every simulation at the tolerances, the
/// rows exactly, the events in the order they were recorded in. It cannot pass (see "WHAT CAN BE
/// COMPARED" at the top of the file): OpenRocket does not reproduce these values itself.
/// Disabled; run it with --gtest_also_run_disabled_tests to see what differs on a platform.
TEST(SimulationGoldenStrict, DISABLED_EverySimulationIsReproducedInFull)
{
    std::string report;
    for (const TestRocketMaker& maker : testRocketMakers())
    {
        const GoldenInput* input = inputOf(maker);
        ASSERT_NE(input, nullptr) << maker;
        for (std::size_t i = 0; i < input->simulations.size(); i++)
        {
            const GoldenRun& run = goldenRun(maker, *input, i);
            ASSERT_EQ(run.problem, "");
            report += compareSimulation(
                          run.files, run.run, run.sensitivity, run.context,
                          {.randomConfigurationId = run.randomConfigurationId, .strict = true})
                          .report;
        }
    }
    EXPECT_EQ(report, "");
}

// =============================================================================== measurement

/// "branch 0: 37 of 796 rows (until t = 0.2507 s); the run has 848 rows, the perturbed run 794"
/// for every branch of @p run that is not reproducible as a whole.
[[nodiscard]] std::string sensitivityText(const GoldenRun& run)
{
    std::string text;
    for (std::size_t i = 0; i < run.sensitivity.horizons.size(); i++)
    {
        const Horizon& horizon = run.sensitivity.horizons[i];
        if (horizon.whole || i >= run.files.tables.size() || i >= run.twin.data->getBranchCount())
        {
            continue;
        }
        text += std::format(
            " branch {}: {} of {} rows (until t = {:.4f} s); the run has {} rows, "
            "the perturbed run {};",
            i, horizon.rows, run.files.tables[i].rows.size(), horizon.time,
            run.run.data->getBranch(i).getLength(), run.twin.data->getBranch(i).getLength());
    }
    return text;
}

/// "848+283" for a flight of two branches with 848 and 283 rows.
[[nodiscard]] std::string rowCounts(const std::vector<std::size_t>& rows)
{
    std::string text;
    for (const std::size_t count : rows)
    {
        text += std::format("{}{}", text.empty() ? "" : "+", count);
    }
    return text;
}

/// One line of the overview of the measurement: the apogee (the maximum altitude) and the flight
/// time of @p run and of its golden file, the rows of their branches, and the largest difference
/// of a value of the time series (over the rows both have), as a fraction of its column's scale.
[[nodiscard]] std::string overviewLine(const GoldenRun& run, const Measurements& measurements)
{
    if (run.run.data == nullptr)
    {
        return std::format("{}\tno flight data\n", run.context);
    }
    const json&              summary = run.files.document.at("summary");
    std::vector<std::size_t> rows;
    std::vector<std::size_t> goldenRows;
    rows.reserve(run.run.data->getBranchCount());
    goldenRows.reserve(run.files.tables.size());
    for (std::size_t i = 0; i < run.run.data->getBranchCount(); i++)
    {
        rows.push_back(run.run.data->getBranch(i).getLength());
    }
    for (const GoldenTable& table : run.files.tables)
    {
        goldenRows.push_back(table.rows.size());
    }
    return std::format("{}\t{}\t{}\t{}\t{}\t{}\t{}\t{:.1e}\n", run.context,
                       run.run.data->getMaxAltitude(), goldenValue(summary.at("maxAltitude")),
                       run.run.data->getFlightTime(), goldenValue(summary.at("flightTime")),
                       rowCounts(rows), rowCounts(goldenRows),
                       measurements.largestOfTheTimeSeries(run.context) * kValueRelative);
}

/// The measurement behind the tolerances: prints, per simulation, how far it is reproducible,
/// and a table of the largest differences from the golden files, one line per simulation and
/// thing compared (a summary value, the times of the events, a column, ...), separately for what
/// is compared and for what is sensitive: the number of values, the largest difference, and that
/// difference as a multiple of its tolerance; then an overview, one line per simulation: the
/// apogee and the flight time here and in the golden file, the rows, and the largest difference
/// of the time series. Disabled; run it with --gtest_also_run_disabled_tests (under a libm of
/// another platform, or the one-ulp shim, to see what the tolerances have to cover there).
TEST(SimulationGoldenMeasurement, DISABLED_PrintsTheDifferencesFromTheGoldenFiles)
{
    Measurements measurements;
    std::string  sensitivity;
    std::string  overview;
    for (const TestRocketMaker& maker : testRocketMakers())
    {
        const GoldenInput* input = inputOf(maker);
        ASSERT_NE(input, nullptr) << maker;
        for (std::size_t i = 0; i < input->simulations.size(); i++)
        {
            const GoldenRun& run = goldenRun(maker, *input, i);
            ASSERT_EQ(run.problem, "");
            (void)compareSimulation(run.files, run.run, run.sensitivity, run.context,
                                    {.randomConfigurationId = run.randomConfigurationId,
                                     .measurements          = &measurements});
            sensitivity += run.sensitivity.whole
                               ? std::string{}
                               : std::format("{}:{}\n", run.context, sensitivityText(run));
            overview += overviewLine(run, measurements);
        }
    }
    std::cout << "Not reproducible as a whole:\n"
              << sensitivity
              << "simulation\twhat\tcompared or sensitive\tvalues\tlargest difference\t"
                 "... as a multiple of its tolerance\n"
              << measurements.text()
              << "simulation\tapogee (m)\t... in the golden file\tflight time (s)\t"
                 "... in the golden file\trows per branch\t... in the golden file\t"
                 "largest difference of the time series, of the column's scale\n"
              << overview
              << std::format(
                     "The largest difference of a compared value is {:.3e} of its "
                     "tolerance.\n",
                     measurements.largestComparedOfTolerance());
    EXPECT_LE(measurements.largestComparedOfTolerance(), 1.0);
}

// ================================================================================= mutations

// The comparison is not vacuous: a golden value changed in a copy of the files of a simulation
// is reported, in one line that names it. The simulations: the [A8-0; None] one of the Beta,
// which ends on the launch pad after one step and is reproducible as a whole on every platform;
// the [C6-5] flight of the Estes Alpha III, of which the launch rod is reproducible on every
// platform; and the simulation with motors of the cluster pods, which has a warning.

/// Four times the tolerance of a value.
constexpr double kBeyondTolerance = 4 * kValueRelative;
/// A quarter of it.
constexpr double kWithinTolerance = kValueRelative / 4;
/// Four times the tolerance of a time, in s.
constexpr double kBeyondTimeTolerance = 4 * kTimeAbsolute;
/// A quarter of it.
constexpr double kWithinTimeTolerance = kTimeAbsolute / 4;

/// The simulations the mutations change (Subject: the golden input and the index in it).
constexpr Subject kOnThePad{.input = "testrocket-beta", .index = 1};
constexpr Subject kFlight{.input = "testrocket-estes-alpha-iii", .index = 4};
constexpr Subject kWithWarning{.input = "testrocket-cluster-pods", .index = 1};

/// The changes to what identifies a simulation, to its options and to how it ended.
[[nodiscard]] std::vector<Mutation> settingMutations()
{
    return {
        {.name    = "SimulationName",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["name"] = "[C6-6]"; },
         .heading = "",
         .line    = R"(  name: expected "[C6-6]", got "[C6-5]")"},
        {.name    = "FlightConfigurationName",
         .subject = kFlight,
         .apply = [](GoldenFiles& files) { files.document["flightConfiguration"]["name"] = "[X]"; },
         .heading = "",
         .line    = R"(  flightConfiguration.name: expected "[X]")"},
        {.name    = "Variant",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["variant"] = "something"; },
         .heading = "",
         .line    = R"(  variant: expected "something", got "null")"},
        {.name    = "TimeStep",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { scale(files, "/options/timeStep", 1e-15); },
         .heading = " options",
         .line    = "  timeStep: expected "},
        {.name    = "WindDirection",
         .subject = kFlight,
         .apply = [](GoldenFiles& files) { scale(files, "/options/averageWind/direction", 1e-15); },
         .heading = " options",
         .line    = "  averageWind.direction: expected "},
        {.name    = "StepperMethod",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["options"]["stepperMethod"] = "RK6"; },
         .heading = " options",
         .line    = R"(  stepperMethod: expected "RK6", got "RK4")"},
        {.name    = "HarnessSeed",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["harness"]["randomSeed"] = 1; },
         .heading = " harness",
         .line    = R"(  randomSeed: expected "1", got "0")"},
        {.name    = "Status",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["result"]["status"] = "exception"; },
         .heading = " result",
         .line    = R"(  status: expected "exception", got "completed")"},
        {.name    = "ExceptionMessage",
         .subject = kFlight,
         .apply = [](GoldenFiles& files) { files.document["result"]["exceptionMessage"] = "bad"; },
         .heading = " result",
         .line    = R"(  exceptionMessage: expected "bad", got "null")"},
        {.name    = "JitterReplacements",
         .subject = kOnThePad,
         .apply   = [](GoldenFiles& files) { files.document["result"]["jitterReplacements"] = 5; },
         .heading = " result",
         .line    = "  jitterReplacements: expected 5, got 4"},
        {.name    = "UnknownField",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["result"]["steps"] = 1; },
         .heading = " result",
         .line    = "  steps: not compared"},
    };
}

/// The changes to the summary, the warnings and the header and columns of a branch.
[[nodiscard]] std::vector<Mutation> branchMutations()
{
    return {
        {.name    = "MaximumMachNumber",
         .subject = kOnThePad,
         .apply =
             [](GoldenFiles& files) { scale(files, "/summary/maxMachNumber", kBeyondTolerance); },
         .heading = " summary",
         .line    = "  maxMachNumber: expected "},
        {.name    = "FlightTime",
         .subject = kOnThePad,
         .apply =
             [](GoldenFiles& files) { shift(files, "/summary/flightTime", kBeyondTimeTolerance); },
         .heading = " summary",
         .line    = "  flightTime: expected "},
        {.name    = "LaunchRodVelocity",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/summary/launchRodVelocity", kBeyondTolerance);
             },
         .heading = " summary",
         .line    = "  launchRodVelocity: expected "},
        {.name    = "BranchCount",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["summary"]["branchCount"] = 2; },
         .heading = " summary",
         .line    = "  branchCount: expected 2, got 1"},
        {.name    = "WarningText",
         .subject = kWithWarning,
         .apply = [](GoldenFiles& files) { files.document["warnings"][0]["text"] = "No device."; },
         .heading = "",
         .line    = R"(  warnings[0].text: expected "No device.")"},
        {.name    = "RowCount",
         .subject = kOnThePad,
         .apply   = [](GoldenFiles& files) { files.document["branches"][0]["rows"] = 3; },
         .heading = " branch 0",
         .line    = "  rows: expected 3, got 2"},
        {.name    = "BranchName",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["branches"][0]["name"] = "Booster"; },
         .heading = " branch 0",
         .line    = R"(  name: expected "Booster", got "Stage")"},
        {.name    = "SourceComponent",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) { files.document["branches"][0]["sourceComponent"] = "/1"; },
         .heading = " branch 0",
         .line    = R"(  sourceComponent: expected "/1", got "/0")"},
        {.name    = "ExcludedColumns",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["excludedColumns"] = json::array();
             },
         .heading = " branch 0",
         .line    = R"(  excludedColumns: expected "", got "computation_time")"},
        {.name    = "ColumnSymbol",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["columns"][1]["symbol"] = "H";
             },
         .heading = " branch 0",
         .line    = R"(  columns[1].symbol: expected "H", got "h")"},
        {.name    = "ColumnMaximum",
         .subject = kOnThePad,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/branches/0/columns/0/max", kBeyondTolerance);
             },
         .heading = " branch 0",
         .line    = "  columns[0].max (time): expected "},
    };
}

/// The changes to the events and to the time series of a branch.
[[nodiscard]] std::vector<Mutation> trajectoryMutations()
{
    return {
        {.name    = "ValueOfTheTimeSeries",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { shiftSeries(files, "altitude", 5, kBeyondTolerance); },
         .heading = " branch 0",
         .line    = "  column altitude: 1 of "},
        {.name    = "TimeOfARecord",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { shiftSeries(files, "time", 3, kBeyondTolerance); },
         .heading = " branch 0",
         .line    = "  column time: 1 of "},
        {.name    = "TimeOfTheLiftoff",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/2/time", kBeyondTimeTolerance);
             },
         .heading = " branch 0",
         .line    = "  events[2].time: expected "},
        {.name    = "TimeOfTheBurnout",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/4/time", kBeyondTimeTolerance);
             },
         .heading = " branch 0",
         .line    = "  events[4].time: expected "},
        {.name    = "RemovedEvent",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["branches"][0]["events"].erase(3); },
         .heading = " branch 0",
         .line    = R"(  events: expected "LAUNCH(/) IGNITION(/0/1/2) LIFTOFF(null) )"
                    "BURNOUT(/0/1/2) "},
        {.name    = "SwappedEvents",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 json& events = files.document["branches"][0]["events"];
                 std::swap(events[5], events[6]);
             },
         .heading = " branch 0",
         .line    = R"(  events: expected "LAUNCH(/) IGNITION(/0/1/2) LIFTOFF(null) )"
                    "LAUNCHROD(null) BURNOUT(/0/1/2) EJECTION_CHARGE(/0) APOGEE(/) "},
        {.name    = "SourceOfAnEvent",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) { files.document["branches"][0]["events"][6]["source"] = "/"; },
         .heading = " branch 0",
         .line    = R"(  events: expected ")"},
        {.name    = "MotorOfAnEvent",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["events"][1]["data"]["designation"] = "C7";
             },
         .heading = " branch 0",
         .line    = R"(  events[1].data.designation: expected "C7", got "C6")"},
        {.name    = "CauseOfAnAbort",
         .subject = kOnThePad,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["events"][1]["data"]["cause"] = "NO_CP";
             },
         .heading = " branch 0",
         .line    = R"(  events[1].data.cause: expected "NO_CP", got "NO_MOTORS_FIRED")"},
        {.name    = "WarningOfAnEvent",
         .subject = kWithWarning,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["events"][0]["data"]["priority"] = "LOW";
             },
         .heading = " branch 0",
         .line    = R"(  events[0].data.priority: expected "LOW", got "HIGH")"},
        {.name    = "KindOfTheDataOfAnEvent",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["events"][2]["data"] = "text";
             },
         .heading = " branch 0",
         .line    = R"(  events[2].data: kind: expected "text", got "none")"},
    };
}

/// Every change.
[[nodiscard]] std::vector<Mutation> mutations()
{
    std::vector<Mutation> all = settingMutations();
    for (const Mutation& mutation : branchMutations())
    {
        all.push_back(mutation);
    }
    for (const Mutation& mutation : trajectoryMutations())
    {
        all.push_back(mutation);
    }
    return all;
}

/// The run of the simulation @p subject, or null (with a test failure) when there is none.
[[nodiscard]] const GoldenRun* subjectRun(const Subject& subject)
{
    const std::span<const TestRocketMaker> makers = testRocketMakers();
    const auto maker = std::ranges::find(makers, subject.input, &TestRocketMaker::input);
    if (maker == makers.end() || inputOf(*maker) == nullptr)
    {
        ADD_FAILURE() << "no golden input or maker named " << subject.input;
        return nullptr;
    }
    const GoldenRun& run = goldenRun(*maker, *inputOf(*maker), subject.index);
    if (!run.problem.empty())
    {
        ADD_FAILURE() << run.problem;
        return nullptr;
    }
    return &run;
}

/// What the comparison of the simulation @p subject with a copy of its golden files finds after
/// @p change changed the copy (null: nothing changed).
[[nodiscard]] SimulationComparison comparisonAfter(const Subject& subject,
                                                   void (*change)(GoldenFiles& files))
{
    const GoldenRun* run = subjectRun(subject);
    if (run == nullptr)
    {
        SimulationComparison none;
        none.report = "no run";
        return none;
    }
    GoldenFiles files = run->files;
    if (change != nullptr)
    {
        change(files);
    }
    return compareSimulation(files, run->run, run->sensitivity, run->context,
                             {.randomConfigurationId = run->randomConfigurationId});
}

/// Changes within the tolerances to the [C6-5] flight: a value and a time of the time series,
/// the times of two events, a summary value.
void changeTheFlightWithinTheTolerances(GoldenFiles& files)
{
    shiftSeries(files, "altitude", 5, kWithinTolerance);
    shiftSeries(files, "time", 3, kWithinTolerance);
    shift(files, "/branches/0/events/2/time", kWithinTimeTolerance);
    shift(files, "/branches/0/events/4/time", kWithinTimeTolerance);
    scale(files, "/summary/launchRodVelocity", kWithinTolerance);
}

/// Changes within the tolerances to the run that ends on the pad: two summary values and the
/// maximum of a column.
void changeTheRunOnThePadWithinTheTolerances(GoldenFiles& files)
{
    scale(files, "/summary/maxMachNumber", kWithinTolerance);
    shift(files, "/summary/flightTime", kWithinTimeTolerance);
    scale(files, "/branches/0/columns/0/max", kWithinTolerance);
}

/// The simulations of the tests, with the changes within the tolerances that go with them.
constexpr std::array<SubjectCase, 3> kSubjectCases{{
    {.name                = "TheRunOnThePad",
     .subject             = kOnThePad,
     .withinTheTolerances = changeTheRunOnThePadWithinTheTolerances},
    {.name                = "TheFlight",
     .subject             = kFlight,
     .withinTheTolerances = changeTheFlightWithinTheTolerances},
    {.name = "TheRunWithAWarning", .subject = kWithWarning, .withinTheTolerances = nullptr},
}};

/// The mutations of the simulation @p subject.
[[nodiscard]] std::vector<Mutation> mutationsOf(const Subject& subject)
{
    std::vector<Mutation> own;
    for (const Mutation& mutation : mutations())
    {
        if (mutation.subject.input == subject.input && mutation.subject.index == subject.index)
        {
            own.push_back(mutation);
        }
    }
    return own;
}

/// What is wrong with the report of @p mutation, applied to a copy of the files of @p run: ""
/// when the report is the heading and the one line that name the change.
[[nodiscard]] std::string mutationProblem(const Mutation& mutation, const GoldenRun& run)
{
    const std::string report = comparisonAfter(mutation.subject, mutation.apply).report;
    const std::string start =
        std::format("{}{}:\n{}", run.context, mutation.heading, mutation.line);
    if (!report.starts_with(start))
    {
        return std::format("{}: the report\n{}\ndoes not start with\n{}\n", mutation.name, report,
                           start);
    }
    if (std::ranges::count(report, '\n') != 2)
    {
        return std::format("{}: the report is not one line:\n{}\n", mutation.name, report);
    }
    return {};
}

/// What is wrong with the reports of the mutations of the simulation @p subject, each named;
/// "" when every one is reported in its one line.
[[nodiscard]] std::string mutationProblems(const Subject& subject)
{
    const GoldenRun* run = subjectRun(subject);
    if (run == nullptr)
    {
        return "no run";
    }
    std::string problems;
    for (const Mutation& mutation : mutationsOf(subject))
    {
        problems += mutationProblem(mutation, *run);
    }
    return problems;
}

/// The report of the changes within the tolerances of @p subjectCase; "" when it has none.
[[nodiscard]] std::string reportWithinTheTolerances(const SubjectCase& subjectCase)
{
    return subjectCase.withinTheTolerances == nullptr
               ? std::string{}
               : comparisonAfter(subjectCase.subject, subjectCase.withinTheTolerances).report;
}

/// The golden values of one simulation, changed one at a time in a copy of its files.
class SimulationGoldenMutation : public ::testing::TestWithParam<SubjectCase>
{ };

TEST_P(SimulationGoldenMutation, EveryChangeIsReportedInOneLine)
{
    EXPECT_FALSE(mutationsOf(GetParam().subject).empty());
    EXPECT_EQ(mutationProblems(GetParam().subject), "");
}

TEST_P(SimulationGoldenMutation, TheFilesAsTheyAreAndChangesWithinTheTolerancesMatch)
{
    EXPECT_EQ(comparisonAfter(GetParam().subject, nullptr).report, "");
    EXPECT_EQ(reportWithinTheTolerances(GetParam()), "");
}

INSTANTIATE_TEST_SUITE_P(Changes, SimulationGoldenMutation, ::testing::ValuesIn(kSubjectCases),
                         subjectTestName);

// The tests above run every mutation: each belongs to one of the three simulations, and each
// has a name of its own for the report. No simulation is run here.
TEST(SimulationGoldenMutations, EveryMutationBelongsToASimulationOfTheTests)
{
    const std::vector<Mutation> all = mutations();
    EXPECT_EQ(all.size(), 33U);
    std::size_t covered = 0;
    for (const SubjectCase& subjectCase : kSubjectCases)
    {
        covered += mutationsOf(subjectCase.subject).size();
    }
    EXPECT_EQ(covered, all.size());
    const std::vector<std::string_view> names = sortedNames(all);
    EXPECT_EQ(std::ranges::adjacent_find(names), names.end()) << "two mutations of one name";
}

/// Changes to what is not reproducible in the [C6-5] flight: the last record, the time of the
/// apogee, the maximum altitude, the number of rows and the number of jitter replacements.
void changeTheFlightBeyondItsHorizon(GoldenFiles& files)
{
    files.tables.at(0).rows.back().at(1) += 1.0;
    shift(files, "/branches/0/events/5/time", 1.0);
    scale(files, "/summary/maxAltitude", 0.5);
    files.document["branches"][0]["rows"]          = 1;
    files.document["result"]["jitterReplacements"] = 1;
}

/// The other side of comparing what is reproducible: a value beyond the horizon of a flight is
/// not compared, whatever it is. It is counted as sensitive, with the same sums.
TEST(SimulationGoldenMutations, AValueBeyondTheHorizonIsCountedAsSensitiveNotCompared)
{
    const SimulationComparison unchanged = comparisonAfter(kFlight, nullptr);
    const SimulationComparison changed = comparisonAfter(kFlight, changeTheFlightBeyondItsHorizon);
    EXPECT_EQ(changed.report, "");
    EXPECT_EQ(changed.compared, unchanged.compared);
    EXPECT_EQ(changed.sensitive, unchanged.sensitive);
    EXPECT_GT(unchanged.sensitive.rows, 0);
    EXPECT_GT(unchanged.sensitive.numbers, 0);
    EXPECT_GT(unchanged.compared.rows, 0);
}

// ============================================================================ the comparison

// What no golden simulation of the test rockets exercises on every platform.

/// What compareSimulationWarning() reports and counts for @p actual against @p expected in a run
/// that is reproducible as a whole (@p whole) or not.
[[nodiscard]] Comparison warningComparison(const json& expected, const Warning& actual, bool whole)
{
    const Rocket rocket;
    Comparison   c;
    c.context = "warning";
    Mismatches m("warning");
    compareSimulationWarning(m, c, "w", expected, actual, whole, rocket);
    c.report = m.report();
    return c;
}

TEST(SimulationGoldenWarnings, AWarningWithAParameterIsComparedInFullInAReproducibleRun)
{
    const QtRocket::Test::DefaultUnitsGuard    units;  // the text prints the speed
    const Warning::RecoveryHighSpeedDeployment warning{80.5};
    json                                       expected = goldenFormOf(warning);
    ASSERT_TRUE(expected.contains("parameter"));
    EXPECT_EQ(expected.at("type"), "RecoveryHighSpeedDeployment");

    const Comparison same = warningComparison(expected, warning, true);
    EXPECT_EQ(same.report, "");
    EXPECT_EQ(same.compared.numbers, 1);
    EXPECT_EQ(same.sensitive.numbers, 0);

    expected["parameter"] = 80.5 * (1 + kWithinTolerance);
    EXPECT_EQ(warningComparison(expected, warning, true).report, "");

    expected["parameter"]    = 80.5 * (1 + kBeyondTolerance);
    const std::string report = warningComparison(expected, warning, true).report;
    EXPECT_TRUE(report.starts_with("warning:\n  w.parameter: expected 80.5000003")) << report;
    EXPECT_EQ(std::ranges::count(report, '\n'), 2) << report;
}

TEST(SimulationGoldenWarnings, AWarningWithAParameterIsComparedByItsIdentityInASensitiveRun)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    const Warning::LargeAOA                 warning{0.4};
    json                                    expected = goldenFormOf(warning);
    expected["parameter"]                            = 0.5;
    expected["text"]                                 = "Another angle";
    expected["description"]                          = "Another angle";

    // The angle and the texts that print it are not compared, and counted as sensitive.
    const Comparison sensitive = warningComparison(expected, warning, false);
    EXPECT_EQ(sensitive.report, "");
    EXPECT_EQ(sensitive.compared.numbers, 0);
    EXPECT_EQ(sensitive.sensitive.numbers, 1);

    // The class, the priority and the sources are.
    expected["priority"]     = "HIGH";
    const std::string report = warningComparison(expected, warning, false).report;
    EXPECT_TRUE(report.starts_with("warning:\n  w.priority: expected \"HIGH\", got \"LOW\""))
        << report;
    EXPECT_EQ(std::ranges::count(report, '\n'), 2) << report;
}

TEST(SimulationGoldenWarnings, AWarningWithoutAParameterIsAlwaysComparedInFull)
{
    const Warning& warning  = Warning::kSupersonic;
    json           expected = goldenFormOf(warning);
    EXPECT_FALSE(expected.contains("parameter"));
    const Comparison same = warningComparison(expected, warning, false);
    EXPECT_EQ(same.report, "");
    EXPECT_EQ(same.compared.numbers + same.sensitive.numbers, 0) << "it has no number";

    expected["text"]         = "Something else";
    const std::string report = warningComparison(expected, warning, false).report;
    EXPECT_TRUE(report.starts_with("warning:\n  w.text: expected \"Something else\"")) << report;
    EXPECT_EQ(std::ranges::count(report, '\n'), 2) << report;
}

/// Whether GoldenMismatches::within() takes @p actual for the golden @p expected at the
/// tolerances @p relative and @p absolute.
[[nodiscard]] bool isWithin(double expected, double actual, double relative, double absolute)
{
    Mismatches m("number");
    m.within("x", expected, actual, relative, absolute);
    return m.report().empty();
}

TEST(SimulationGoldenNumbers, ANumberMatchesWithinTheRelativeOrTheAbsoluteTolerance)
{
    const double nan      = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    EXPECT_TRUE(isWithin(100.0, 100.0 + 5e-8, 1e-9, 0.0)) << "relative to the larger magnitude";
    EXPECT_FALSE(isWithin(100.0, 100.0 + 2e-7, 1e-9, 0.0));
    EXPECT_TRUE(isWithin(100.0, 100.0 + 2e-7, 0.0, 3e-7)) << "or within the absolute tolerance";
    EXPECT_FALSE(isWithin(100.0, 100.0 + 2e-7, 0.0, 1e-7));
    EXPECT_TRUE(isWithin(0.0, 0.0, 0.0, 0.0));
    EXPECT_FALSE(isWithin(0.0, 1e-300, 0.0, 0.0)) << "without a tolerance only the same number";
    // NaN matches NaN only, and an infinity the same infinity only, whatever the tolerances.
    EXPECT_TRUE(isWithin(nan, nan, 0.0, 0.0));
    EXPECT_FALSE(isWithin(nan, 1.0, 1.0, 1.0));
    EXPECT_FALSE(isWithin(1.0, nan, 1.0, 1.0));
    EXPECT_TRUE(isWithin(infinity, infinity, 0.0, 0.0));
    EXPECT_FALSE(isWithin(infinity, -infinity, 1.0, 1.0));
    EXPECT_FALSE(isWithin(infinity, 1.0, 1.0, 1.0));
    EXPECT_FALSE(isWithin(1.0, infinity, 1.0, infinity));
}

TEST(SimulationGoldenNumbers, AValueIsReproducibleWhenThePerturbedRunMovesItByLittle)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    // Within 1/kSensitivityMargin of the tolerance.
    EXPECT_TRUE(reproducible(1.0, 1.0, 0.0));
    EXPECT_TRUE(reproducible(1.0, 1.0 + 5e-13, 1e-9));
    EXPECT_FALSE(reproducible(1.0, 1.0 + 2e-12, 1e-9));
    EXPECT_FALSE(reproducible(0.0, 1e-300, 0.0)) << "a column of zeros has no tolerance";
    EXPECT_TRUE(reproducible(nan, nan, 0.0));
    EXPECT_FALSE(reproducible(nan, 1.0, 1.0));
    EXPECT_FALSE(reproducible(1.0, nan, 1.0));
}

/// The order comparisonOrder() gives the events @p keys, as their sources one after the other.
[[nodiscard]] std::string orderedSources(const std::vector<EventKey>& keys, bool recorded)
{
    std::string sources;
    for (const std::size_t i : comparisonOrder(keys, recorded))
    {
        sources += sources.empty() ? "" : " ";
        sources += keys[i].source;
    }
    return sources;
}

TEST(SimulationGoldenEvents, SimultaneousIgnitionsAreComparedAsASet)
{
    // LAUNCH, two ignitions at 0 s, one at 0.01 s, LIFTOFF, two burnouts at the same time, two
    // ignitions at 2 s: only the consecutive ignitions with the same time are sorted.
    const std::vector<EventKey> keys{
        {.ignition = false, .time = 0.0, .source = "/"},
        {.ignition = true, .time = 0.0, .source = "/0/1/5"},
        {.ignition = true, .time = 0.0, .source = "/0/1/2"},
        {.ignition = true, .time = 0.01, .source = "/0/0"},
        {.ignition = false, .time = 0.06, .source = "null"},
        {.ignition = false, .time = 1.0, .source = "/9"},
        {.ignition = false, .time = 1.0, .source = "/8"},
        {.ignition = true, .time = 2.0, .source = "/1/1"},
        {.ignition = true, .time = 2.0, .source = "/1/0"},
    };
    EXPECT_EQ(orderedSources(keys, false), "/ /0/1/2 /0/1/5 /0/0 null /9 /8 /1/0 /1/1");
    EXPECT_EQ(orderedSources(keys, true), "/ /0/1/5 /0/1/2 /0/0 null /9 /8 /1/1 /1/0");
    EXPECT_EQ(orderedSources({}, false), "");
}

TEST(SimulationGoldenHarness, TheFileNamesAreTheDumpers)
{
    // GoldenDumper.slug(), and "sim_<NN>_<slug>".
    EXPECT_EQ(slug("[C6-5] RK6 stepper"), "c6-5-rk6-stepper");
    EXPECT_EQ(slug("[No motors]"), "no-motors");
    EXPECT_EQ(slug("[; 2 A10-0; A8-0]"), "2-a10-0-a8-0");
    EXPECT_EQ(slug("---"), "unnamed");
    EXPECT_EQ(slug(""), "unnamed");
    EXPECT_EQ(baseName({.index = 7, .configuration = 4, .name = "[C6-5] WGS84 geodetics"}),
              "sim_07_c6-5-wgs84-geodetics");
}

TEST(SimulationGoldenHarness, ThePerturbationMovesTheLastBitAwayFromZero)
{
    EXPECT_EQ(lastBitMoved(0.0), 0.0);
    EXPECT_EQ(lastBitMoved(1.0), 1.0 + std::numeric_limits<double>::epsilon());
    EXPECT_EQ(lastBitMoved(-1.0), -1.0 - std::numeric_limits<double>::epsilon());
    EXPECT_TRUE(std::isnan(lastBitMoved(std::numeric_limits<double>::quiet_NaN())));
    EXPECT_EQ(lastBitMoved(std::numeric_limits<double>::infinity()),
              std::numeric_limits<double>::infinity());
}

}  // namespace
