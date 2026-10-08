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
//   the text); and the time series, every column of every row (but a noise-dominated
//   out-of-plane column in the rows on the launch rod only: see "The out-of-plane noise
//   columns" below). NaN equals NaN.
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
//   rod (at most 6e-16 of a column's scale, and 4e-14, in a moment coefficient, at the first
//   record after it), 1e-12 some 5 to 70 records later, 1e-9 after 30 to 180 records, then
//   different row counts (20 simulations), maximum velocities (up to 1.8e-3), apogee times (up
//   to 0.045 s) and ground hits (0.14 s).
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
// - A row on the launch rod is reproducible when every value of it, in the two runs, is within
//   1/kSensitivityMargin of the tolerance of its column (1e-12 of the scale of the column). A
//   run of which every row of every branch is, in a perturbed run with as many rows, branches
//   and jitter replacements and the same status, is whole: nothing grows in it.
// - In any other run a difference grows somewhere, and a row after the launch rod is
//   reproducible when every value of it that is compared is within 1/kFlightSensitivityMargin
//   of the tolerance of its column (1e-13 of the scale: see "Tolerances" for why ten times
//   less). The first row of a branch that is not reproducible ends the reproducible part of
//   the branch (its horizon).
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
// - What is neither is counted as sensitive, what the rule of the out-of-plane noise columns
//   leaves out as excluded, and the sums are checked: compared plus sensitive plus excluded is
//   what the files hold (in the test of each rocket; SimulationGoldenCoverage pins what the
//   files of the thirteen rockets hold in all, and what the rule excludes of it). A value of
//   the time series is counted as compared where it is compared, one by one, and the two other
//   counts follow from the rows and the columns: the sum holds only when the loop over the rows
//   compared every value it had to. On Linux 20
//   simulations are whole (the 19 that never leave the launch rod and the Falcon 9 Heavy, a
//   flight of 148 records that ends in a tumble under thrust, which the perturbed run
//   reproduces to 6.4e-13 of a column's scale throughout); of the 30 others the first 19 to 51
//   records are compared, which is the launch rod and the first 0.007 s to 0.29 s of free
//   flight; in all, 1289 of the 22330 rows (1230 to 1350 under the patterns of the libm shim).
// The test of each rocket also holds the floor under this, so that a calculation that became
// sensitive throughout could not pass by comparing nothing: a simulation that never clears the
// launch rod is whole, and every flight is reproducible at least until it has cleared the rod
// (in rows: the reproducible rows of a branch are at least the golden rows up to the clearing,
// 794 in the 31 flights and 26 in the 19 other simulations).
// SimulationGoldenStrict holds the comparison the plan asked for (everything, the rows exactly,
// the events in the order they were recorded in), disabled: it cannot pass.
//
// The out-of-plane noise columns. This is rule N of the stable-step set, taken over by the
// user's decision and through the same code (kOutOfPlaneColumns, kNoiseRatio and rodOnlyRule()
// of GoldenSimulations.h). The flights are planar but for the Coriolis acceleration (the wind
// blows along one axis), so the yaw rate, the lateral acceleration across the wind (in world and
// in body coordinates), the lateral position across it and the direction of the lateral
// position hold a signal of 2e-6 to 2e-3 of their counterparts in the plane, and the roll rate,
// where a flight has one, 2e-8 to 1e-4 of the pitch rate. Their tolerances, 1e-9 of their own
// scales, are as much smaller, and what another rounding of the motion in the plane leaves in
// such a column is larger than that as soon as the rocket is free. The perturbed run does not
// show it: it moves every component of the velocities by its own last bit, so what it does to
// an out-of-plane component is in proportion to that component. Measured before the rule: under
// one of the 47 patterns of the libm shim (ULP_MODE=1 ULP_RATE=37) the lateral acceleration
// across the wind of the [2 A10-0; B4-3] flight of the Estes Alpha III with motor pods differed
// from the golden file by 1.3e-12 m/s^2 in row 27 of the 31 rows that the sensitivity analysis
// took as reproducible, 2.4 times its tolerance (the column is 6e-5 of the acceleration in the
// plane there), and the test of that rocket failed.
// So an out-of-plane column whose largest magnitude is below kNoiseRatio = 1e-2 of its
// counterpart's (and that is not a column of zeros) is compared in the rows on the launch rod,
// where it is as reproducible as every other column, and in no row after them: there it neither
// ends the reproducible part of its branch nor is compared, and its values are counted as
// excluded. That is 189 columns (five in each of the 33 planar branches that leave the rod, and
// the roll rate in 24 branches) with 120481 values after the rod, which SimulationGoldenCoverage
// pins from the files alone and the test of each rocket holds against what its comparison
// excluded. In the flight into the multi-level wind, which turns with the altitude, the five
// columns hold a signal (0.2 to 2 times their counterparts) and are compared like every other.
// A minimum or maximum of a noise column is compared in a branch that is reproducible as a
// whole, unless the golden column or the run's attains it only after the rod: then it is one of
// the excluded values (extremeTreatment() of GoldenSimulations.h decides it; the test of each
// rocket holds the number of them between what the golden columns alone exclude and two per
// noise column of a whole branch). The strict comparison knows no rule.
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
// 1e-9 of its magnitude; a time: 1e-6 s. Measured over what is compared, in 47 runs (Linux with
// glibc, and 46 patterns of the one-ulp libm shim of the aerodynamic goldens: every result
// moved up, every one down, and up or down at random on one call in 1, 2, ... 41, 101, 1009 and
// 10007): the largest difference of a value from the golden files is 3.7e-13 of the scale with
// glibc and 2.1e-13 to 5.0e-12 under the shim, so the tolerance is 199 times the largest
// measurement and five orders of magnitude inside the plan's bounds. On the launch rod the
// largest is 7.3e-14, which is also the most the perturbed run moves a value there. The 241
// times that are compared differ by at most 2e-15 s; their tolerance covers what a time the
// motors set can differ by, which is the rounding of the simulation time it is counted from
// (6e-11 s between OpenRocket and QtRocket in a run of the three-stage rocket with a time step
// of 0.01 s).
// The factor of 199 is what kFlightSensitivityMargin is for. That margin is not part of the
// user's decision on the noise columns, which is the rule above and no more: it was added with
// the rule, by the work that took the rule over, because the rule alone does not leave the
// tolerance a hundred times the largest measurement, and whether it stays is the user's to
// decide (the alternatives: the margin of the launch rod in every row, with a factor of 8; or a
// wider tolerance after the rod). With the margin of the launch rod
// in every row (the comparison had no other one before) the largest difference in the same 47
// runs is 1.2e-10 of the scale, an eighth of the tolerance, and more than a hundredth of it in
// 10 runs, for two reasons:
// - A difference grows in free flight at the rate that the perturbed run shows, but from where
//   it started, and the golden run of OpenRocket and a run under another library start
//   elsewhere than the perturbed run: in rows that the perturbed run moves by less than 1e-12,
//   values of ten flights (damping ratios, stability derivatives, pitch rates, lateral
//   accelerations) differ from the golden files by 1.0e-11 to 2.8e-11 of their scales, under
//   five of the patterns.
// - One row is decided by a last bit, and no perturbed run can be relied on to show that. In
//   row 27 of the [2 A10-0; B4-3] flight of the Estes Alpha III with motor pods the angle of
//   attack passes through zero (4e-5 rad, between 8e-3 rad a row before and 9e-3 rad a row
//   after), and the lateral acceleration of that row has one of two values, 1.2e-10 of its
//   scale apart. The golden file has one of them; 19 of the 47 runs have it too and 28 the
//   other, and the perturbed run has the value of its run in 24 and the other one in 23. In
//   the 14 runs that differ from the golden file while their perturbed run does not, only the
//   rows before can end the reproducible part in time: at a margin of 1000 they did not in 7.
// At a margin of 3000 after the rod that one row is left above a hundredth of the tolerance, in
// one run; at 10000 the reproducible part of that flight ends within its first 26 rows in every
// run, and the largest difference is the 5.0e-12 above (the damping ratio of a flight of the
// Estes Alpha III with pods). The margin is not applied to the rows on the rod (the perturbed
// run moves a value of the first record after the rod by up to 7.3e-14 of its scale, so a floor
// at 1e-13 would hold by a factor of 1.4 only), nor to a run that is reproducible as a whole at
// the margin of the rod: the Falcon 9 Heavy differs from the golden files by 6.3e-13 of a scale
// at most in its 148 rows, in every run. The price is the reproducible part of the 30 other
// flights: 1115 rows on Linux, where a margin of 1000 gave 1496 (the first 22 to 77 records of
// each).
// SimulationGoldenMeasurement, disabled, prints the table of the differences per simulation and
// column (compared, sensitive and excluded) and, row by row, what the perturbed run moves and
// what differs from the golden files, from which the effect of any margin can be read
// (tier9a-g2-rules/scripts/margin.py).
//
// Not vacuous: SimulationGoldenMutation changes one golden value at a time (a setting, a value
// of the time series, a value of a noise column in the last row on the launch rod, a time, a
// summary value, a row count), removes an event and swaps two, and expects the one line that
// reports it; a change within the tolerance is not reported. Two kinds of changes are, as said,
// not noticed: a value beyond the horizon, and, by the rule above, a value of a noise-dominated
// out-of-plane column in a row after the launch rod (each has a test in
// SimulationGoldenMutations that changes such values and expects the same counts and no line).
// A column in the plane is compared after the rod as far as the flight is reproducible, and
// that has a test of its own, since the changes above are all in rows on the rod: a value of
// the altitude in the first row after it is reported. How far the perturbed run of a platform
// takes a flight as reproducible beyond the rod is not something a test can rely on (the floor
// covers the rows on the rod), so the two tests that look at the first row after it compare
// with a reproducible part that is fixed by hand to end with that row.
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
#include <initializer_list>
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
#include "QtRocket/simulation/FlightDataType.h"
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
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::Rocket;
using QtRocket::Warning;
using QtRocket::Test::baseName;
using QtRocket::Test::columnKey;
using QtRocket::Test::columnScale;
using QtRocket::Test::compareSimulation;
using QtRocket::Test::compareSimulationWarning;
using QtRocket::Test::Comparison;
using QtRocket::Test::comparisonOrder;
using QtRocket::Test::csvTypes;
using QtRocket::Test::EventKey;
using QtRocket::Test::extremeTreatment;
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
using QtRocket::Test::kFlightSensitivityMargin;
using QtRocket::Test::kSensitivityMargin;
using QtRocket::Test::kTimeAbsolute;
using QtRocket::Test::kValueRelative;
using QtRocket::Test::LastBitListener;
using QtRocket::Test::lastBitMoved;
using QtRocket::Test::loadGoldenFiles;
using QtRocket::Test::makerTestName;
using QtRocket::Test::manifest;
using QtRocket::Test::Measurements;
using QtRocket::Test::Mutation;
using QtRocket::Test::noiseColumns;
using QtRocket::Test::plannedSimulations;
using QtRocket::Test::reproducible;
using QtRocket::Test::RodOnlyRule;
using QtRocket::Test::rodOnlyRule;
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
using QtRocket::Test::Treatment;
using QtRocket::Test::treatmentName;

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
std::ostream& operator<<(std::ostream& out, const NoiseCounts& counts)
{
    return out << std::format("{} noise columns with {} values off the rod", counts.columns,
                              counts.values);
}

/// What the rule decides for the branches of the golden simulation @p files.
[[nodiscard]] NoiseCounts noiseCounts(const GoldenFiles& files)
{
    NoiseCounts                 counts;
    const std::optional<double> cleared = goldenRodClearance(files.document);
    for (const GoldenTable& table : files.tables)
    {
        const RodOnlyRule rule = rodOnlyRule(table, cleared);
        counts += NoiseCounts{.columns = rule.noiseColumnCount(), .values = rule.excludedValues()};
    }
    return counts;
}

/// The golden minima and maxima of column @p column of branch @p branch of @p files that the
/// golden column attains only off the launch rod (0, 1 or 2): what the rule @p rule of the
/// branch excludes of them whatever the run has.
[[nodiscard]] std::int64_t goldenExtremesOffTheRod(const GoldenFiles& files, std::size_t branch,
                                                   const RodOnlyRule& rule, std::size_t column)
{
    const GoldenTable&        table  = files.tables.at(branch);
    const json&               golden = files.document.at("branches").at(branch).at("columns");
    const std::vector<double> values =
        table.column(table.columns.at(column)).value_or(std::vector<double>{});
    std::int64_t excluded = 0;
    for (const char* extreme : {"min", "max"})
    {
        excluded +=
            column < golden.size() &&
                    rule.excludesExtreme(column, values, goldenValue(golden.at(column).at(extreme)))
                ? 1
                : 0;
    }
    return excluded;
}

/// How many minima and maxima the rule of the out-of-plane noise columns can exclude from the
/// comparison of a run: in a branch that is reproducible as a whole at least those that the
/// golden column attains only off the launch rod (the files alone say so) and at most two per
/// noise column (the run's column can attain its own only there as well); in any other branch
/// none, since its minima and maxima are sensitive.
struct ExtremeBounds
{
    std::int64_t atLeast{0};
    std::int64_t atMost{0};
};
[[nodiscard]] ExtremeBounds extremeBounds(const GoldenRun& run)
{
    ExtremeBounds               bounds;
    const std::optional<double> cleared = goldenRodClearance(run.files.document);
    for (std::size_t i = 0; i < run.sensitivity.horizons.size() && i < run.files.tables.size(); i++)
    {
        if (!run.sensitivity.horizons[i].whole)
        {
            continue;
        }
        const RodOnlyRule rule = rodOnlyRule(run.files.tables[i], cleared);
        bounds.atMost += 2 * rule.noiseColumnCount();
        for (std::size_t column = 0; column < rule.noise.size(); column++)
        {
            bounds.atLeast += goldenExtremesOffTheRod(run.files, i, rule, column);
        }
    }
    return bounds;
}

/// What is wrong with what the rule of the out-of-plane noise columns excluded from the
/// comparison of @p run, "" when nothing is: nothing at all in a simulation that never clears
/// the launch rod (it has no row off the rod), and of the minima and maxima no more and no fewer
/// than extremeBounds() allows (a comparison that excluded none of them, or every one of every
/// column, would add up to what the files hold all the same).
[[nodiscard]] std::string exclusionProblem(const GoldenRun& run)
{
    const SimulationCounts& excluded = run.comparison.excluded;
    if (!goldenRodClearance(run.files.document).has_value() && excluded != SimulationCounts{})
    {
        return std::format("{}: never clears the launch rod, but the rule excluded {}\n",
                           run.context, toText(excluded));
    }
    const ExtremeBounds bounds = extremeBounds(run);
    if (excluded.numbers < bounds.atLeast || excluded.numbers > bounds.atMost)
    {
        return std::format(
            "{}: {} minima and maxima excluded, the rule allows {} to {} in the branches that "
            "are reproducible as a whole\n",
            run.context, excluded.numbers, bounds.atLeast, bounds.atMost);
    }
    return {};
}

/// What the simulations of one golden input hold, and what their comparison compared and found.
struct InputResult
{
    std::string      problems;  ///< a simulation that could not be run, a floor that does not hold
    SimulationCounts golden;
    SimulationCounts compared;
    SimulationCounts sensitive;
    SimulationCounts excluded;  ///< what the rule of the out-of-plane noise columns excludes
    std::string      report;
    int              flights{0};    ///< the simulations whose rocket clears the launch rod
    int              whole{0};      ///< the simulations that are reproducible as a whole
    std::int64_t     floorRows{0};  ///< floorRows() of the simulations
    NoiseCounts      noise;         ///< noiseCounts() of the simulations
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
        result.excluded += run.comparison.excluded;
        result.report += run.comparison.report;
        result.flights += goldenRodClearance(run.files.document).has_value() ? 1 : 0;
        result.whole += run.sensitivity.whole ? 1 : 0;
        result.floorRows += floorRows(run.files);
        result.noise += noiseCounts(run.files);
        result.problems += floorProblem(run);
        result.problems += exclusionProblem(run);
    }
    return result;
}

// ===================================================================================== tests

/// One test rocket of TestRockets.h: its golden simulations.
class SimulationGolden : public ::testing::TestWithParam<TestRocketMaker>
{ };

// Every simulation of the rocket is compared as far as it is reproducible: what was compared,
// what is sensitive and what the rule of the out-of-plane noise columns excludes add up to what
// the files hold. And the reproducible part has a floor (floorProblem(), among the problems), so
// that a calculation that became sensitive throughout cannot pass by comparing nothing: a
// simulation that never clears the launch rod is reproducible as a whole, and a flight in every
// branch at least until it has cleared the rod. What the rule excludes of the time series is
// what the files alone say it does (noiseCounts()): nothing of it depends on the run. Of the
// minima and maxima it excludes at most two per noise column, and per simulation what
// exclusionProblem() allows (among the problems as well).
TEST_P(SimulationGolden, EverySimulationOfTheRocket)
{
    const InputResult result = compareInput(GetParam());
    ASSERT_EQ(result.problems, "");
    EXPECT_EQ(result.report, "");
    // Everything the files hold was compared, is sensitive or is excluded by the rule: nothing
    // skipped.
    EXPECT_EQ(result.compared + result.sensitive + result.excluded, result.golden)
        << "what was compared, is sensitive or is excluded, and what the files hold";
    EXPECT_GT(result.golden.simulations, 0);
    // The structure is compared in every simulation, sensitive or not, and the rule excludes
    // values of the time series and the extremes of its columns, no row.
    EXPECT_EQ(result.sensitive.simulations + result.sensitive.branches + result.sensitive.events +
                  result.sensitive.columns + result.sensitive.warnings,
              0);
    EXPECT_EQ(result.excluded.simulations + result.excluded.branches + result.excluded.events +
                  result.excluded.columns + result.excluded.warnings + result.excluded.rows,
              0);
    EXPECT_EQ(result.excluded.values, result.noise.values) << result.noise;
    EXPECT_LE(result.excluded.numbers, 2 * result.noise.columns) << result.noise;
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
    NoiseCounts      noise;            ///< noiseCounts() of the simulations
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
        coverage.noise += noiseCounts(files);
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
    // The rule of the out-of-plane noise columns (RodOnlyRule), from the files alone: 189
    // columns are noise-dominated, the yaw rate, the two lateral accelerations across the wind,
    // the lateral position across it and its direction in each of the 33 planar branches that
    // leave the rod (30 flights and 3 dropped stages; not in the flight into the multi-level
    // wind, where they hold a signal), and the roll rate in the 24 branches that have one (23
    // of these, and that flight). Their 120481 values off the rod are not compared, in any run.
    EXPECT_EQ(coverage.noise, (NoiseCounts{.columns = 189, .values = 120481})) << coverage.noise;
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
/// is compared, for what is sensitive and for what the rule of the out-of-plane noise columns
/// excludes: the number of values, the largest difference, and that
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
              << "simulation\twhat\tcompared, sensitive or excluded\tvalues\tlargest difference\t"
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

/// The difference of @p value from @p other as a fraction of @p scale, the scale of their
/// column: 0 for the same number (two NaN, the same infinity), and infinite for two that differ
/// in a column without a scale, or of which one is no number.
[[nodiscard]] double fractionOfTheScale(double value, double other, double scale)
{
    if (std::isnan(value) || std::isnan(other))
    {
        return std::isnan(value) == std::isnan(other) ? 0.0
                                                      : std::numeric_limits<double>::infinity();
    }
    if (value == other)
    {
        return 0.0;
    }
    const double difference = std::abs(value - other);
    return scale > 0 && std::isfinite(difference) ? difference / scale
                                                  : std::numeric_limits<double>::infinity();
}

/// The largest of the differences of the values of one row, and the column it is in.
struct RowDifference
{
    double      largest{0};
    std::size_t column{0};

    void add(double difference, std::size_t index)
    {
        if (difference > largest)
        {
            largest = difference;
            column  = index;
        }
    }
};

/// A branch of a run and of its perturbed run next to its golden time series, column by column.
struct ProfiledBranch
{
    const GoldenTable*                      table{nullptr};
    RodOnlyRule                             rule;
    std::vector<std::string>                keys;
    std::vector<const std::vector<double>*> values;
    std::vector<const std::vector<double>*> twin;
    std::vector<double>                     scales;
    std::size_t                             rows{0};  ///< the rows all three have
};

/// Branch @p index of @p run for the row profile; a column the perturbed run lacks is left out.
[[nodiscard]] ProfiledBranch profiledBranch(const GoldenRun& run, std::size_t index)
{
    const FlightDataBranch& branch = run.run.data->getBranch(index);
    const FlightDataBranch& twin   = run.twin.data->getBranch(index);
    ProfiledBranch          profiled;
    profiled.table = &run.files.tables[index];
    profiled.rule  = rodOnlyRule(*profiled.table, goldenRodClearance(run.files.document));
    profiled.rows  = std::min({branch.getLength(), twin.getLength(), profiled.table->rows.size()});
    const std::vector<const FlightDataType*> types = csvTypes(branch);
    for (std::size_t i = 0; i < std::min(types.size(), profiled.table->columns.size()); i++)
    {
        profiled.keys.push_back(columnKey(*types[i]));
        profiled.values.push_back(branch.getView(*types[i]));
        profiled.twin.push_back(twin.getView(*types[i]));
        profiled.scales.push_back(columnScale(*profiled.table, i));
    }
    return profiled;
}

/// The largest differences of row @p row of @p branch: of the perturbed run from the run, which
/// decides whether the row is reproducible, and of the run from the golden file, which is what
/// the comparison finds; over the columns that are compared in the row (not a noise-dominated
/// out-of-plane column after the launch rod).
struct RowProfile
{
    RowDifference perturbed;
    RowDifference golden;
};
[[nodiscard]] RowProfile rowProfile(const ProfiledBranch& branch, std::size_t row)
{
    RowProfile profile;
    for (std::size_t i = 0; i < branch.values.size(); i++)
    {
        if (branch.rule.excludes(i, row) || branch.values[i] == nullptr ||
            branch.twin[i] == nullptr)
        {
            continue;
        }
        const double value = (*branch.values[i])[row];
        profile.perturbed.add(fractionOfTheScale(value, (*branch.twin[i])[row], branch.scales[i]),
                              i);
        profile.golden.add(fractionOfTheScale(value, branch.table->rows[row][i], branch.scales[i]),
                           i);
    }
    return profile;
}

/// The row profile of branch @p index of @p run: one line per row, up to the first row that no
/// margin makes reproducible (the perturbed run moves a value of it by more than the tolerance
/// of the comparison itself, kValueRelative of the scale of its column).
[[nodiscard]] std::string branchProfile(const GoldenRun& run, std::size_t index)
{
    const ProfiledBranch branch = profiledBranch(run, index);
    std::string          text;
    for (std::size_t row = 0; row < branch.rows; row++)
    {
        const RowProfile profile = rowProfile(branch, row);
        text += std::format("{}\t{}\t{}\t{}\t{:.3e}\t{}\t{:.3e}\t{}\n", run.context, index, row,
                            row < branch.rule.rodRows ? "rod" : "free", profile.perturbed.largest,
                            branch.keys.at(profile.perturbed.column), profile.golden.largest,
                            branch.keys.at(profile.golden.column));
        if (profile.perturbed.largest > kValueRelative)
        {
            break;
        }
    }
    return text;
}

/// The row profiles of the branches of @p run that the run, the perturbed run and the golden
/// files all have.
[[nodiscard]] std::string runProfile(const GoldenRun& run)
{
    std::string text;
    if (run.run.data == nullptr || run.twin.data == nullptr)
    {
        return text;
    }
    const std::size_t branches = std::min(
        {run.run.data->getBranchCount(), run.twin.data->getBranchCount(), run.files.tables.size()});
    for (std::size_t i = 0; i < branches; i++)
    {
        text += branchProfile(run, i);
    }
    return text;
}

/// The measurement behind kSensitivityMargin and kFlightSensitivityMargin: prints, for every row
/// of every branch up to the first one that is not reproducible at any margin, the largest
/// difference of the perturbed run from the run and the largest difference of the run from the
/// golden file, both as fractions of the scales of their columns and each with its column, over
/// the columns that are compared in the row. A row is reproducible while the first number is at
/// most kValueRelative over the margin of the row, in it and in every row before it; the second
/// number of such a row is what the comparison then holds against the tolerance kValueRelative.
/// So the table says, for any margin, how far the flights would be compared and how close to
/// the tolerance that would come (tier9a-g2-rules/scripts/margin.py reads it). Disabled; run it
/// with --gtest_also_run_disabled_tests, under the one-ulp shim or the libm of another platform.
TEST(SimulationGoldenMeasurement, DISABLED_PrintsTheSensitivityRowByRow)
{
    std::string profile;
    for (const TestRocketMaker& maker : testRocketMakers())
    {
        const GoldenInput* input = inputOf(maker);
        ASSERT_NE(input, nullptr) << maker;
        for (std::size_t i = 0; i < input->simulations.size(); i++)
        {
            const GoldenRun& run = goldenRun(maker, *input, i);
            ASSERT_EQ(run.problem, "");
            profile += runProfile(run);
        }
    }
    std::cout << "simulation\tbranch\trow\ton the rod or in free flight\t"
                 "largest difference of the perturbed run, of the column's scale\t... its column\t"
                 "largest difference from the golden file, of the column's scale\t... its column\n"
              << profile;
}

// ================================================================================= mutations

// The comparison is not vacuous: a golden value changed in a copy of the files of a simulation
// is reported, in one line that names it. The simulations: the [A8-0; None] one of the Beta,
// which ends on the launch pad after one step and is reproducible as a whole on every platform;
// the [C6-5] flight of the Estes Alpha III, of which the launch rod is reproducible on every
// platform; and the simulation with motors of the cluster pods, which has a warning.
//
// Two kinds of changes are not noticed, by decision, and each has a test that says so: a value
// beyond the horizon of a flight (AValueBeyondTheHorizonIsCountedAsSensitiveNotCompared), and a
// value of a noise-dominated out-of-plane column in a row after the launch rod, within the
// horizon or not (AValueOfANoiseColumnOffTheRodIsCountedAsExcludedNotCompared). The same column
// is compared on the rod: the mutation ValueOfANoiseColumnOnTheRod changes its last row there.
// And a column in the plane is compared after the rod as far as the flight is reproducible:
// AValueOfAColumnInThePlaneIsComparedInTheFirstRowOffTheRod (the mutations of the list below
// are all in rows on the rod, so without it nothing would notice a comparison that stopped at
// the rod for every column).

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

/// The [C6-5] flight clears the launch rod at its row 19: the rows up to it are the rod rows of
/// the rule of the out-of-plane noise columns, and the row after it is the first one in which
/// those columns are not compared.
constexpr std::size_t kLastRodRow        = 19;
constexpr std::size_t kFirstRowOffTheRod = 20;

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
        // A noise-dominated out-of-plane column is compared in the rows on the launch rod, the
        // last of them included, and in those 20 rows only. (The same change one row later is
        // not noticed: AValueOfANoiseColumnOffTheRodIsCountedAsExcludedNotCompared.)
        {.name    = "ValueOfANoiseColumnOnTheRod",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 shiftSeries(files, "acceleration_y", kLastRodRow, kBeyondTolerance);
             },
         .heading = " branch 0",
         .line    = "  column acceleration_y: 1 of 20 rows differ, the first at row 19: "},
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

/// A sensitivity made by hand for the [C6-5] flight @p run: the rows up to and including the
/// first one after the launch rod are the reproducible part, whatever the perturbed run of the
/// platform says of that row. The floor under the reproducible part (floorProblem()) covers the
/// rows on the rod and not this one: the perturbed run moves a value of it by 2.1e-14 to
/// 4.8e-14 of the scale of its column under 122 patterns of the libm shim, against the limit of
/// 1e-13 after the rod, and by more than the limit in the same row of four other flights. The
/// run itself differs from the golden file in that row by 6.8e-15 of a scale at most, a hundred
/// thousand times less than the tolerance of the comparison, so the comparison of the row is
/// safe where its place within the horizon is not.
[[nodiscard]] Sensitivity sensitivityUpToTheFirstRowOffTheRod(const GoldenRun& run)
{
    Horizon horizon;
    horizon.rows  = kFirstRowOffTheRod + 1;
    horizon.whole = false;
    // The time of the last reproducible row, as horizonOf() takes it: the run's own.
    const std::vector<double>* times =
        run.run.data == nullptr || run.run.data->getBranchCount() == 0
            ? nullptr
            : run.run.data->getBranch(0).getView(
                  FlightDataType::builtin(FlightDataTypeId::TYPE_TIME));
    if (times != nullptr && times->size() > kFirstRowOffTheRod)
    {
        horizon.time = (*times)[kFirstRowOffTheRod];
    }
    Sensitivity sensitivity;
    sensitivity.whole    = false;
    sensitivity.horizons = {horizon};
    return sensitivity;
}

/// comparisonAfter() of the [C6-5] flight with sensitivityUpToTheFirstRowOffTheRod() in the
/// place of the sensitivity that the two runs of the platform gave: what the comparison does in
/// the first row after the launch rod then depends on no platform.
[[nodiscard]] SimulationComparison comparisonUpToTheFirstRowOffTheRodAfter(
    void (*change)(GoldenFiles& files))
{
    const GoldenRun* run = subjectRun(kFlight);
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
    return compareSimulation(files, run->run, sensitivityUpToTheFirstRowOffTheRod(*run),
                             run->context, {.randomConfigurationId = run->randomConfigurationId});
}

/// Changes within the tolerances to the [C6-5] flight: a value and a time of the time series,
/// a value of a noise-dominated out-of-plane column on the launch rod, the times of two events,
/// a summary value.
void changeTheFlightWithinTheTolerances(GoldenFiles& files)
{
    shiftSeries(files, "altitude", 5, kWithinTolerance);
    shiftSeries(files, "time", 3, kWithinTolerance);
    shiftSeries(files, "acceleration_y", kLastRodRow, kWithinTolerance);
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
    EXPECT_EQ(all.size(), 34U);
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

/// The noise-dominated out-of-plane columns of the [C6-5] flight, a planar one.
constexpr std::array<std::string_view, 6> kNoiseColumnsOfTheFlight{
    "yaw_rate",   "roll_rate",          "acceleration_y",
    "position_y", "position_direction", "acceleration_bodyy"};

/// Changes to the altitude of the [C6-5] flight in the first row after the launch rod: beyond
/// its tolerance and within it; and beyond it one row later.
void changeTheAltitudeOffTheRodBeyondItsTolerance(GoldenFiles& files)
{
    shiftSeries(files, "altitude", kFirstRowOffTheRod, kBeyondTolerance);
}
void changeTheAltitudeOffTheRodWithinItsTolerance(GoldenFiles& files)
{
    shiftSeries(files, "altitude", kFirstRowOffTheRod, kWithinTolerance);
}
void changeTheAltitudeOneRowLater(GoldenFiles& files)
{
    shiftSeries(files, "altitude", kFirstRowOffTheRod + 1, kBeyondTolerance);
}

/// What is wrong with the report @p report of a comparison of the [C6-5] flight in which the
/// altitude of the first row after the launch rod was changed beyond its tolerance: "" when it
/// is the heading of the branch and the one line that names the row, of the 21 rows compared.
[[nodiscard]] std::string altitudeOffTheRodProblem(const std::string& report)
{
    const GoldenRun* run = subjectRun(kFlight);
    if (run == nullptr)
    {
        return "no run";
    }
    const std::string start =
        std::format("{} branch 0:\n  column altitude: 1 of 21 rows differ, the first at row 20: ",
                    run->context);
    if (!report.starts_with(start) || std::ranges::count(report, '\n') != 2)
    {
        return std::format("the report\n{}\nis not the one line\n{}...\n", report, start);
    }
    return {};
}

// A column in the plane is compared after the launch rod as well, as far as the flight is
// reproducible: a value of the altitude in the first row after the rod, changed beyond its
// tolerance, is reported. (Every mutation of SimulationGoldenMutation changes a row on the rod.
// Without this test a comparison that compared no column after the rod would pass every test:
// the rule makes the number of compared rows depend on the column, RodOnlyRule::comparedRows().)
// The reproducible part is fixed by hand, see sensitivityUpToTheFirstRowOffTheRod(). The values
// are counted where they are compared, so the count says what the loop over the rows did: every
// column in the 20 rows on the rod, and every column but the six noise columns in the row after.
TEST(SimulationGoldenMutations, AValueOfAColumnInThePlaneIsComparedInTheFirstRowOffTheRod)
{
    const GoldenRun* run = subjectRun(kFlight);
    ASSERT_NE(run, nullptr);
    const auto         columns = static_cast<std::int64_t>(run->files.tables.at(0).columns.size());
    const auto         rows    = static_cast<std::int64_t>(run->files.tables.at(0).rows.size());
    const std::int64_t rodRows = kFirstRowOffTheRod;
    ASSERT_GT(columns, 6);
    ASSERT_GT(rows, rodRows + 1);

    const SimulationComparison unchanged = comparisonUpToTheFirstRowOffTheRodAfter(nullptr);
    EXPECT_EQ(unchanged.report, "");
    EXPECT_EQ(unchanged.compared.rows, rodRows + 1);
    EXPECT_EQ(unchanged.compared.values, (rodRows * columns) + (columns - 6));
    EXPECT_EQ(unchanged.sensitive.rows, rows - (rodRows + 1));
    EXPECT_EQ(unchanged.sensitive.values, (rows - (rodRows + 1)) * (columns - 6));
    EXPECT_EQ(unchanged.excluded.values, 6 * (rows - rodRows));
    EXPECT_EQ(unchanged.compared + unchanged.sensitive + unchanged.excluded, run->golden);

    // Beyond the tolerance: the one line. Within it: nothing.
    const SimulationComparison changed =
        comparisonUpToTheFirstRowOffTheRodAfter(changeTheAltitudeOffTheRodBeyondItsTolerance);
    EXPECT_EQ(altitudeOffTheRodProblem(changed.report), "");
    EXPECT_EQ(changed.compared, unchanged.compared);
    EXPECT_EQ(comparisonUpToTheFirstRowOffTheRodAfter(changeTheAltitudeOffTheRodWithinItsTolerance)
                  .report,
              "");
    // One row later the same change is beyond this reproducible part: sensitive, not compared.
    EXPECT_EQ(comparisonUpToTheFirstRowOffTheRodAfter(changeTheAltitudeOneRowLater).report, "");
}

/// Changes to what the rule of the out-of-plane noise columns excludes in the [C6-5] flight:
/// the value of each of its six noise columns in the first row after the launch rod (the test
/// compares with a reproducible part that holds that row: the floor covers the rows on the rod
/// only, and how much further the perturbed run of a platform goes is not to be relied on), the
/// lateral acceleration across the wind in every later row, and the last value of the lateral
/// position across it.
void changeTheNoiseColumnsOffTheRod(GoldenFiles& files)
{
    for (const std::string_view key : kNoiseColumnsOfTheFlight)
    {
        shiftSeries(files, key, kFirstRowOffTheRod, 0.5);
    }
    for (std::size_t row = kFirstRowOffTheRod + 1; row < files.tables.at(0).rows.size(); row++)
    {
        shiftSeries(files, "acceleration_y", row, 0.25);
    }
    shiftSeries(files, "position_y", files.tables.at(0).rows.size() - 1, 0.5);
}

/// The rule of the out-of-plane noise columns for the first branch of @p files.
[[nodiscard]] RodOnlyRule firstRule(const GoldenFiles& files)
{
    return rodOnlyRule(files.tables.at(0), goldenRodClearance(files.document));
}

/// The number of the columns @p keys of the first branch of @p files that are noise-dominated.
[[nodiscard]] int noiseColumnsAmong(const GoldenFiles&                files,
                                    std::span<const std::string_view> keys)
{
    const RodOnlyRule rule  = firstRule(files);
    int               noise = 0;
    for (const std::string_view key : keys)
    {
        const std::optional<std::size_t> column = files.tables.at(0).columnIndex(key);
        noise += column.has_value() && rule.isNoise(*column) ? 1 : 0;
    }
    return noise;
}

// The rule of the out-of-plane noise columns (the user's decision for the default-step set, as
// rule N of the stable-step set): a value of such a column in a row after the launch rod is not
// compared, whatever it is, within the horizon of the flight or beyond it. It is counted as
// excluded, with the same sums. (The same change one row earlier, in the last row on the rod, is
// reported: the mutation ValueOfANoiseColumnOnTheRod.)
TEST(SimulationGoldenMutations, AValueOfANoiseColumnOffTheRodIsCountedAsExcludedNotCompared)
{
    // The rows after the rod that the comparison takes as reproducible hold these columns too:
    // what is not compared there is excluded by the rule, not by the horizon. That the first
    // row after the rod is such a row is given here by hand
    // (sensitivityUpToTheFirstRowOffTheRod()), not taken from the perturbed run of the platform.
    const SimulationComparison unchanged = comparisonUpToTheFirstRowOffTheRodAfter(nullptr);
    const SimulationComparison changed =
        comparisonUpToTheFirstRowOffTheRodAfter(changeTheNoiseColumnsOffTheRod);
    EXPECT_EQ(unchanged.report, "");
    EXPECT_EQ(unchanged.compared.rows, static_cast<std::int64_t>(kFirstRowOffTheRod) + 1);
    EXPECT_EQ(changed.report, "");
    EXPECT_EQ(changed.compared, unchanged.compared);
    EXPECT_EQ(changed.sensitive, unchanged.sensitive);
    EXPECT_EQ(changed.excluded, unchanged.excluded);
    // Six noise columns in the 776 rows after the 20 on the launch rod, from the files alone.
    EXPECT_EQ(unchanged.excluded.values, 6 * 776);
    EXPECT_EQ(unchanged.excluded.rows, 0) << "the rule excludes values, not rows";
    EXPECT_EQ(unchanged.excluded.numbers, 0)
        << "the minima and maxima of a flight that is not reproducible as a whole are sensitive";

    // The rows and the columns the changes rely on are what their names say, and the changes
    // leave the columns what they are (a change moves the scale of its column).
    const GoldenRun* run = subjectRun(kFlight);
    ASSERT_NE(run, nullptr);
    const RodOnlyRule rule = firstRule(run->files);
    EXPECT_EQ(rule.rows, 796U);
    EXPECT_EQ(rule.rodRows, kLastRodRow + 1);
    EXPECT_EQ(rule.rodRows, kFirstRowOffTheRod);
    EXPECT_EQ(rule.noiseColumnCount(), 6);
    EXPECT_EQ(noiseColumnsAmong(run->files, kNoiseColumnsOfTheFlight), 6);
    GoldenFiles files = run->files;
    changeTheNoiseColumnsOffTheRod(files);
    EXPECT_EQ(firstRule(files).noise, rule.noise);
    // With the sensitivity of the platform's own two runs the changes are not noticed either,
    // wherever its horizon is: beyond it they are sensitive or excluded, within it excluded.
    EXPECT_EQ(comparisonAfter(kFlight, changeTheNoiseColumnsOffTheRod).report, "");
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

// ------------------------------------------------------- the out-of-plane noise columns

TEST(SimulationGoldenNoiseColumns, ANoiseDominatedOutOfPlaneColumnIsComparedOnTheRodOnly)
{
    // A planar flight that clears the launch rod at 0.1 s: the yaw rate is 1e-4 of the pitch
    // rate and the position across the wind 1e-5 of the one along it; no roll, and no lateral
    // acceleration along the wind to measure the one across it by.
    const GoldenTable table{.columns = {"time", "pitch_rate", "yaw_rate", "roll_rate", "position_x",
                                        "position_y", "position_direction", "acceleration_y"},
                            .rows    = {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
                                        {0.1, 0.5, 0.0, 0.0, 1.0, 0.0, 0.0, 1e-9},
                                        {0.2, 1.0, 1e-4, 0.0, 10.0, 1e-4, 1e-5, 2e-9},
                                        {0.3, -2.0, -5e-5, 0.0, 20.0, -2e-4, -1e-5, 1e-9},
                                        {0.4, 1.5, 1e-4, 0.0, 30.0, 1e-4, 3e-6, 3e-9}}};
    const RodOnlyRule rule = rodOnlyRule(table, 0.1);
    EXPECT_EQ(rule.rows, 5U);
    EXPECT_EQ(rule.rodRows, 2U) << "the rows up to the clearing of the rod";
    EXPECT_EQ(rule.noise, (std::vector<bool>{false, false, true, false, false, true, true, false}))
        << "not a column of zeros, and not a column without its counterpart";
    EXPECT_EQ(rule.noise, noiseColumns(table));
    EXPECT_EQ(rule.noiseColumnCount(), 3);
    EXPECT_EQ(rule.excludedValues(), 3 * 3) << "three columns in the three rows off the rod";

    // On the rod such a column is compared like every other, after it in no row.
    EXPECT_FALSE(rule.excludes(2, 0));
    EXPECT_FALSE(rule.excludes(2, 1));
    EXPECT_TRUE(rule.excludes(2, 2));
    EXPECT_TRUE(rule.excludes(6, 4));
    EXPECT_FALSE(rule.excludes(1, 4)) << "the pitch rate is in the plane";
    EXPECT_FALSE(rule.excludes(3, 4)) << "a column of zeros";
    EXPECT_FALSE(rule.excludes(8, 4)) << "no such column";
    EXPECT_FALSE(rule.isNoise(8));
    EXPECT_EQ(rule.comparedRows(2, 5), 2U);
    EXPECT_EQ(rule.comparedRows(2, 1), 1U) << "no more rows than are compared at all";
    EXPECT_EQ(rule.comparedRows(1, 5), 5U);
    EXPECT_EQ(rule.comparedRows(1, 3), 3U);

    // A simulation that never clears the rod: every row is a rod row, nothing is excluded.
    const RodOnlyRule onThePad = rodOnlyRule(table, std::nullopt);
    EXPECT_EQ(onThePad.rodRows, 5U);
    EXPECT_EQ(onThePad.noiseColumnCount(), 3);
    EXPECT_EQ(onThePad.excludedValues(), 0);
    EXPECT_FALSE(onThePad.excludes(2, 4));
    EXPECT_EQ(onThePad.comparedRows(2, 5), 5U);

    // A flight that leaves its plane: the same columns hold a signal and are compared.
    GoldenTable turning = table;
    turning.rows[2][2]  = 0.5;
    turning.rows[3][5]  = 5.0;
    EXPECT_EQ(rodOnlyRule(turning, 0.1).noiseColumnCount(), 0);
    EXPECT_EQ(rodOnlyRule(turning, 0.1).excludedValues(), 0);
}

TEST(SimulationGoldenNoiseColumns, AnExtremeAttainedOnlyOffTheRodIsExcludedWithItsColumn)
{
    const double              nan = std::numeric_limits<double>::quiet_NaN();
    const GoldenTable         table{.columns = {"time", "pitch_rate", "yaw_rate"},
                                    .rows    = {{0.0, 0.0, 0.0},
                                                {0.1, 0.5, 0.0},
                                                {0.2, 1.0, 1e-4},
                                                {0.3, -2.0, -5e-5},
                                                {0.4, 1.5, 1e-4}}};
    const RodOnlyRule         rule = rodOnlyRule(table, 0.1);
    const std::vector<double> yaw  = table.column("yaw_rate").value_or(std::vector<double>{});
    ASSERT_EQ(yaw.size(), 5U);
    ASSERT_TRUE(rule.isNoise(2));
    // The maximum and the minimum of the yaw rate are values of rows off the rod: excluded.
    EXPECT_TRUE(rule.excludesExtreme(2, yaw, 1e-4));
    EXPECT_TRUE(rule.excludesExtreme(2, yaw, -5e-5));
    // A value of a rod row is compared, also when a later row has it again.
    EXPECT_FALSE(rule.excludesExtreme(2, yaw, 0.0));
    const std::vector<double> again{0.0, 1e-4, 1e-4, -5e-5, 1e-4};
    EXPECT_FALSE(rule.excludesExtreme(2, again, 1e-4));
    EXPECT_TRUE(rule.excludesExtreme(2, again, -5e-5));
    // An extreme that is no value of its column is compared (and then differs), NaN as well.
    EXPECT_FALSE(rule.excludesExtreme(2, yaw, 7.0));
    EXPECT_FALSE(rule.excludesExtreme(2, yaw, nan));
    EXPECT_FALSE(rule.excludesExtreme(2, std::vector<double>{}, 1e-4));
    // A column in the plane is never excluded, nor one the time series does not have.
    const std::vector<double> pitch = table.column("pitch_rate").value_or(std::vector<double>{});
    EXPECT_FALSE(rule.excludesExtreme(1, pitch, -2.0));
    EXPECT_FALSE(rule.excludesExtreme(3, yaw, 1e-4));
    // A run that never clears the rod: nothing is off it.
    EXPECT_FALSE(rodOnlyRule(table, std::nullopt).excludesExtreme(2, yaw, 1e-4));
}

/// What extremeTreatment() does with the extreme @p expected of column @p column of @p table
/// (the golden one) against the extreme @p actual of the run's column @p values, in a branch
/// that is reproducible as a whole (@p whole) or not.
struct ExtremeCase
{
    std::size_t         column{0};
    double              expected{0};
    double              actual{0};
    std::vector<double> values;
};
[[nodiscard]] Treatment treatmentOf(const GoldenTable& table, const RodOnlyRule& rule,
                                    const ExtremeCase& extreme, bool whole)
{
    return extremeTreatment(table, rule,
                            {.column   = extreme.column,
                             .expected = extreme.expected,
                             .actual   = extreme.actual,
                             .values   = extreme.values},
                            whole);
}

/// The names of the treatments of @p extremes (treatmentOf()), one after the other.
[[nodiscard]] std::string treatmentsOf(const GoldenTable& table, const RodOnlyRule& rule,
                                       std::span<const ExtremeCase> extremes, bool whole)
{
    std::string names;
    for (const ExtremeCase& extreme : extremes)
    {
        names += names.empty() ? "" : " ";
        names += treatmentName(treatmentOf(table, rule, extreme, whole));
    }
    return names;
}

// The decision of the comparison for the minimum or maximum of a column: compared, sensitive or
// excluded. (compareSimulation() asks extremeTreatment() for every minimum and maximum; the
// test of each rocket holds the number it excluded between what the golden files alone exclude
// and two per noise column of a whole branch, exclusionProblem().)
TEST(SimulationGoldenNoiseColumns, AnExtremeIsComparedSensitiveOrExcluded)
{
    // A planar flight that clears the launch rod at 0.1 s (two rows on the rod). The yaw rate
    // is noise (1e-4 of the pitch rate): the golden column attains its minimum on the rod, in
    // row 1, and its maximum off the rod only, in row 2.
    const GoldenTable         table{.columns = {"time", "pitch_rate", "yaw_rate"},
                                    .rows    = {{0.0, 0.0, 0.0},
                                                {0.1, 0.5, -6e-5},
                                                {0.2, 1.0, 1e-4},
                                                {0.3, -2.0, -5e-5},
                                                {0.4, 1.5, 1e-5}}};
    const RodOnlyRule         rule  = rodOnlyRule(table, 0.1);
    const std::vector<double> pitch = table.column("pitch_rate").value_or(std::vector<double>{});
    const std::vector<double> yaw   = table.column("yaw_rate").value_or(std::vector<double>{});
    ASSERT_EQ(rule.rodRows, 2U);
    ASSERT_EQ(rule.noise, (std::vector<bool>{false, false, true}));

    // A column in the plane: compared, wherever it attains its extreme (here off the rod).
    const ExtremeCase inThePlane{.column = 1, .expected = -2.0, .actual = -2.0, .values = pitch};
    EXPECT_EQ(treatmentOf(table, rule, inThePlane, true), Treatment::COMPARED);
    // A noise column whose extreme is a value of a row on the rod, in both columns: compared.
    const ExtremeCase onTheRod{.column = 2, .expected = -6e-5, .actual = -6e-5, .values = yaw};
    EXPECT_EQ(treatmentOf(table, rule, onTheRod, true), Treatment::COMPARED);
    // One that both columns attain off the rod only: excluded.
    const ExtremeCase offTheRod{.column = 2, .expected = 1e-4, .actual = 1e-4, .values = yaw};
    EXPECT_EQ(treatmentOf(table, rule, offTheRod, true), Treatment::EXCLUDED);
    // One that the golden column attains on the rod and the run's off it only: excluded.
    const ExtremeCase runOffTheRod{
        .column = 2, .expected = -6e-5, .actual = -7e-5, .values = {0.0, -6e-5, 1e-4, -7e-5, 1e-5}};
    EXPECT_EQ(treatmentOf(table, rule, runOffTheRod, true), Treatment::EXCLUDED);
    // One that the golden column attains off the rod only and the run's on it: excluded.
    const ExtremeCase goldenOffTheRod{
        .column = 2, .expected = 1e-4, .actual = 2e-4, .values = {0.0, 2e-4, 1e-4, -5e-5, 1e-5}};
    EXPECT_EQ(treatmentOf(table, rule, goldenOffTheRod, true), Treatment::EXCLUDED);
    // A golden extreme that is no value of the golden column (a file that contradicts itself)
    // is compared when the run attains its own on the rod, so that it is reported.
    const ExtremeCase noValue{.column = 2, .expected = -9e-5, .actual = -6e-5, .values = yaw};
    EXPECT_EQ(treatmentOf(table, rule, noValue, true), Treatment::COMPARED);

    // In a branch that is not reproducible as a whole every extreme is sensitive: none is
    // compared, and none is excluded.
    const std::array<ExtremeCase, 6> cases{inThePlane,   onTheRod,        offTheRod,
                                           runOffTheRod, goldenOffTheRod, noValue};
    EXPECT_EQ(treatmentsOf(table, rule, cases, true),
              "compared compared excluded excluded excluded compared");
    EXPECT_EQ(treatmentsOf(table, rule, cases, false),
              "sensitive sensitive sensitive sensitive sensitive sensitive");

    // A simulation that never clears the rod has no row off it, and the strict comparison a
    // rule without a noise column: nothing is excluded. Nor of a column the time series lacks.
    EXPECT_EQ(treatmentOf(table, rodOnlyRule(table, std::nullopt), offTheRod, true),
              Treatment::COMPARED);
    const RodOnlyRule noRule{.rows = 5, .rodRows = 5, .noise = std::vector<bool>(3, false)};
    EXPECT_EQ(treatmentOf(table, noRule, offTheRod, true), Treatment::COMPARED);
    const ExtremeCase noColumn{.column = 7, .expected = 1e-4, .actual = 1e-4, .values = yaw};
    EXPECT_EQ(treatmentOf(table, rule, noColumn, true), Treatment::COMPARED);
}

/// A row of the synthetic flights: the time, the pitch rate and the yaw rate.
using SyntheticRow = std::array<double, 3>;

/// A run whose flight data is one branch with the rows @p rows.
[[nodiscard]] SimulationRun flightOf(const std::vector<SyntheticRow>& rows)
{
    const std::array<const FlightDataType*, 3> types{
        &FlightDataType::builtin(FlightDataTypeId::TYPE_TIME),
        &FlightDataType::builtin(FlightDataTypeId::TYPE_PITCH_RATE),
        &FlightDataType::builtin(FlightDataTypeId::TYPE_YAW_RATE)};
    const auto branch = std::make_shared<FlightDataBranch>(
        "Sustainer", std::span<const FlightDataType* const>{types});
    for (const SyntheticRow& row : rows)
    {
        branch->addPoint();
        for (std::size_t i = 0; i < types.size(); i++)
        {
            branch->setValue(*types.at(i), row.at(i));
        }
    }
    SimulationRun run;
    run.status = "completed";
    run.data   = std::make_shared<FlightData>(
        std::initializer_list<std::shared_ptr<FlightDataBranch>>{branch});
    return run;
}

/// Golden files that hold the time series of the one branch of @p run, of a simulation whose
/// rocket clears the launch rod at @p cleared.
[[nodiscard]] GoldenFiles goldenFilesOf(const SimulationRun& run, double cleared)
{
    GoldenFiles files;
    files.document = json::parse(R"({"branches": [{"events": [{"type": "LAUNCHROD"}]}]})");
    files.document["branches"][0]["events"][0]["time"] = cleared;
    const FlightDataBranch& branch                     = run.data->getBranch(0);
    // (The rows are made with the table: GCC takes assign() on the rows of a new table for a
    // potential null pointer dereference.)
    GoldenTable table{.columns = {}, .rows = std::vector<std::vector<double>>(branch.getLength())};
    for (const FlightDataType* type : csvTypes(branch))
    {
        table.columns.push_back(columnKey(*type));
        const std::vector<double>* values = branch.getView(*type);
        for (std::size_t row = 0; values != nullptr && row < values->size(); row++)
        {
            table.rows[row].push_back((*values)[row]);
        }
    }
    files.tables.push_back(std::move(table));
    return files;
}

/// @p rows with @p offset added to the value @p column of row @p row.
[[nodiscard]] std::vector<SyntheticRow> moved(std::vector<SyntheticRow> rows, std::size_t row,
                                              std::size_t column, double offset)
{
    rows.at(row).at(column) += offset;
    return rows;
}

TEST(SimulationGoldenNoiseColumns, TheReproduciblePartDoesNotEndOnANoiseColumnOffTheRod)
{
    // A planar flight that clears the launch rod at its second row; the yaw rate (column 2) is
    // 1e-4 of the pitch rate (column 1).
    const std::vector<SyntheticRow> rows{{0.0, 0.0, 0.0},    {0.1, 0.5, 1e-5}, {0.2, 1.0, 1e-4},
                                         {0.3, -2.0, -5e-5}, {0.4, 1.5, 2e-5}, {0.5, 1.0, 1e-5}};
    const SimulationRun             run    = flightOf(rows);
    const GoldenFiles               golden = goldenFilesOf(run, 0.1);
    ASSERT_EQ(golden.tables.at(0).columns,
              (std::vector<std::string>{"time", "pitch_rate", "yaw_rate"}));
    ASSERT_EQ(firstRule(golden).noise, (std::vector<bool>{false, false, true}));
    ASSERT_EQ(firstRule(golden).rodRows, 2U);

    // The run against itself: reproducible as a whole.
    const Sensitivity same = sensitivityOf(golden, run, flightOf(rows));
    EXPECT_TRUE(same.whole);
    ASSERT_EQ(same.horizons.size(), 1U);
    EXPECT_EQ(same.horizons.front().rows, 6U);

    // A perturbed run whose yaw rate differs off the rod, by a tenth of the scale of the
    // column: the column is not compared there, and does not end the reproducible part.
    const Sensitivity offTheRod = sensitivityOf(golden, run, flightOf(moved(rows, 3, 2, 1e-5)));
    EXPECT_TRUE(offTheRod.whole);
    EXPECT_TRUE(offTheRod.horizons.at(0).whole);
    EXPECT_EQ(offTheRod.horizons.at(0).rows, 6U);

    // The same difference in a row on the rod does: the column is compared there.
    const Sensitivity onTheRod = sensitivityOf(golden, run, flightOf(moved(rows, 1, 2, 1e-5)));
    EXPECT_FALSE(onTheRod.whole);
    EXPECT_EQ(onTheRod.horizons.at(0).rows, 1U);
    EXPECT_EQ(onTheRod.horizons.at(0).time, 0.0);

    // And a column in the plane ends it off the rod as before, whatever the yaw rate does.
    const Sensitivity inThePlane =
        sensitivityOf(golden, run, flightOf(moved(moved(rows, 3, 2, 1e-5), 4, 1, 1e-6)));
    EXPECT_FALSE(inThePlane.whole);
    EXPECT_EQ(inThePlane.horizons.at(0).rows, 4U);
    EXPECT_EQ(inThePlane.horizons.at(0).time, 0.3);

    // The margins. A pitch rate moved by 5e-13 of the scale of its column (2 rad/s; the
    // tolerance is 1e-9 of it) is within the margin of the launch rod (1e-12) and beyond that of
    // the free flight (1e-13). A run that is reproducible to its last row at the margin of the
    // rod is whole, wherever the perturbed run moves it so:
    EXPECT_TRUE(sensitivityOf(golden, run, flightOf(moved(rows, 1, 1, 1e-12))).whole);
    EXPECT_TRUE(sensitivityOf(golden, run, flightOf(moved(rows, 3, 1, 1e-12))).whole);
    // but in a run that diverges later (here in its last row) the reproducible part ends at the
    // first row after the rod that is beyond the margin of the free flight,
    const std::vector<SyntheticRow> diverging = moved(rows, 5, 1, 1e-6);
    const Sensitivity               inFlight =
        sensitivityOf(golden, run, flightOf(moved(diverging, 3, 1, 1e-12)));
    EXPECT_FALSE(inFlight.whole);
    EXPECT_EQ(inFlight.horizons.at(0).rows, 3U);
    // not at a row on the rod that is within the margin of the rod,
    const Sensitivity onTheRodStill =
        sensitivityOf(golden, run, flightOf(moved(diverging, 1, 1, 1e-12)));
    EXPECT_FALSE(onTheRodStill.whole);
    EXPECT_EQ(onTheRodStill.horizons.at(0).rows, 5U);
    // and not at a row after the rod that is within the margin of the free flight.
    EXPECT_EQ(
        sensitivityOf(golden, run, flightOf(moved(diverging, 3, 1, 1e-13))).horizons.at(0).rows,
        5U);

    // A flight that leaves its plane (the yaw rate is a quarter of the pitch rate): its yaw rate
    // is no noise column, and ends the reproducible part where the perturbed run moves it.
    const std::vector<SyntheticRow> turningRows = moved(rows, 2, 2, 0.5);
    const SimulationRun             turning     = flightOf(turningRows);
    const GoldenFiles               turningGold = goldenFilesOf(turning, 0.1);
    ASSERT_EQ(firstRule(turningGold).noiseColumnCount(), 0);
    const Sensitivity signal =
        sensitivityOf(turningGold, turning, flightOf(moved(turningRows, 3, 2, 1e-5)));
    EXPECT_FALSE(signal.whole);
    EXPECT_EQ(signal.horizons.at(0).rows, 3U);
}

TEST(SimulationGoldenNumbers, AValueIsReproducibleWhenThePerturbedRunMovesItByLittle)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    // On the launch rod: within 1/kSensitivityMargin of the tolerance.
    EXPECT_EQ(kSensitivityMargin, 1e3);
    EXPECT_TRUE(reproducible(1.0, 1.0, 0.0, kSensitivityMargin));
    EXPECT_TRUE(reproducible(1.0, 1.0 + 5e-13, 1e-9, kSensitivityMargin));
    EXPECT_FALSE(reproducible(1.0, 1.0 + 2e-12, 1e-9, kSensitivityMargin));
    EXPECT_FALSE(reproducible(0.0, 1e-300, 0.0, kSensitivityMargin))
        << "a column of zeros has no tolerance";
    EXPECT_TRUE(reproducible(nan, nan, 0.0, kSensitivityMargin));
    EXPECT_FALSE(reproducible(nan, 1.0, 1.0, kSensitivityMargin));
    EXPECT_FALSE(reproducible(1.0, nan, 1.0, kSensitivityMargin));
    // After the rod: within 1/kFlightSensitivityMargin of it, ten times less.
    EXPECT_EQ(kFlightSensitivityMargin, 1e4);
    EXPECT_TRUE(reproducible(1.0, 1.0, 0.0, kFlightSensitivityMargin));
    EXPECT_TRUE(reproducible(1.0, 1.0 + 5e-14, 1e-9, kFlightSensitivityMargin));
    EXPECT_FALSE(reproducible(1.0, 1.0 + 5e-13, 1e-9, kFlightSensitivityMargin));
    EXPECT_FALSE(reproducible(0.0, 1e-300, 0.0, kFlightSensitivityMargin));
    EXPECT_TRUE(reproducible(nan, nan, 0.0, kFlightSensitivityMargin));
    EXPECT_FALSE(reproducible(1.0, nan, 1.0, kFlightSensitivityMargin));
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
