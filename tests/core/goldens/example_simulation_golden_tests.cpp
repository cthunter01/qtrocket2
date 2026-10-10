// Simulation golden tests of OpenRocket's example designs, the default-step set: every simulation
// of the sixteen designs of data/examples, as the .ork loader loads it, against what OpenRocket's
// BasicEventSimulationEngine computes for it, in tests/data/goldens/example-<name>/
// sim_<NN>_<name>.json and the branch CSV files next to them (tools/openrocket-goldens:
// GoldenDumper.examplePass() and SimulationDumper.java; the format is in that tool's README.md).
// 54 simulations with 69 branches, 733 events and 46346 rows, at the time step their documents
// give them: 0.05 s in 49 and 0.04 s in the five of "Pods--airframes and winglets". (The same
// simulations at 0.01 s are the stable-step set of the examples, example-<name>/stable/: the
// documents without the time series, for the comparison of the whole flights.)
//
// The comparison is that of the default-step set of the test rockets: read the top of
// simulation_golden_tests.cpp for what of a flight at this time step is reproducible and why no
// more is, and GoldenSimulations.h for the code, which both files share. This file says what
// is different for a simulation of a loaded design, and holds the measurements of the examples
// that its two own numbers come from: the margin of the free flight and the rule of the
// tumbling dropped stages.
//
// THE SET-UP is the harness's (GoldenDumper.examplePass()). The design is loaded as
// GoldenExamples.h loads it (the bundled motors, the six presets, OpenRocket's test preferences)
// and its automatic dimensions are settled, as the harness settles them before it dumps the
// design. Then every simulation of the document is run, in the order of the document and on the
// one rocket of the document, with its own flight configuration, options and extensions
// (runDocumentSimulation()): SimulationDumper.makeReproducible() (seed 0, fixed, and zero
// standard deviation of every wind model; the documents have no fixed seed, a deviation of the
// average wind of 0.2, 0.22352 or 0.19982688 m/s and one level of the multi-level wind with
// 1.52 or 0.2 m/s), toSimulationConditions(), the forces listener of the jitter removal, the
// extensions initialised in their order, the conditions listener, a new engine. The flight the
// document stores for a simulation (OpenRocket's, in turbulence) plays no part.
// Every simulation is run twice, as in the test rockets' comparison: as the harness runs it and
// with the last-bit perturbation (LastBitListener). The example is therefore loaded twice, and
// the perturbed runs have a document of their own: the simulations of a document share its
// rocket, which an extension changes while its simulation flies (RollControl turns the fins of
// its fin set and puts them back when the run ends), so each run has to meet the rocket as the
// harness's runs before it left it.
//
// COMPARED (ExampleSimulationGolden, one test per example), per simulation: what the test
// rockets' comparison compares, with these differences:
// - the source of the options is "document", and the options are those of the file, each
//   compared exactly with the golden "options" (they are the loader's work: the launch site, the
//   atmosphere, both wind models, the time step, the geodetic computation, FLAT in 29 simulations
//   and SPHERICAL in 25);
// - the flight configuration is the one the simulation names, by its index among the rocket's
//   configurations, its id and its name, which is formatted with the preference store of the
//   load (a motor by its common name: "[G40-7]");
// - the extensions, each by its id, its type, its name and its configuration: six, in four
//   simulations (see "The extensions");
// - the simulations of an example that are one flight in OpenRocket, value for value, are one
//   flight here too, to the last bit and to the last row (sameFlights(); one pair, see "The
//   extensions").
// Everything else is as there: the result's status, the summary, the warnings (20), the
// branches with their names, sources and columns, the events with their types, sources and
// data, and of the trajectory what is reproducible: the rows up to the horizon of each branch,
// the times of the events in them or fixed by them, the launch rod velocity.
//
// WHAT OPENROCKET REPRODUCES OF THESE FLIGHTS. A design that is loaded keeps the ids of its
// components, so two runs of the harness give the same files, and the measurement of the test
// rockets (other ids, another order of summation) says nothing here. Run 9a of the port gave
// the harness a last-bit perturbation of its own for this (LAST_BIT of generate.sh: one ulp
// added to the velocity, the rotation velocity, the position or the orientation after every
// step, away from zero, towards it or at random) and made 21 dumps with it. Those and the
// committed dump are 22 runs of OpenRocket of every simulation, at the default step with their
// time series, and this is what they say (tier9d-default-step/scripts/openrocket_spread.py and
// openrocket_structure.py; the dumps are those of tier9a-golden-data and the two directories
// next to it):
// - The structure is the same in all 22 runs of all 54 simulations (status, branches, columns,
//   warnings by class, priority and source, events by type and source, the data of the events)
//   with one exception, the TUMBLE of a dropped stage: see "The tumbling of a dropped stage".
// - The rows on the launch rod agree to 9.6e-14 of the scale of a column, in every branch.
// - After the rod the runs part: counted from the launch, the rows in which all 22 runs agree
//   to 1e-11 of a column's scale (a hundredth of the tolerance) are 5930 of the 46346, 2004 of
//   them on the rod; that is 11 to 149 rows after the rod per branch. No branch agrees to 1e-11
//   to its end, 16 of the 69 agree to 1e-9, and in the 53 others two runs of OpenRocket differ
//   by more than the tolerance of this comparison somewhere. The number of rows differs in 24
//   branches and the number of jitter replacements in 10 simulations.
// So no example flight is steady the way the Falcon 9 Heavy of the test rockets is, and none
// is taken as whole for being reproducible at the margin of the launch rod
// (SensitivityMargins::steadyRunIsWhole is off): a run would be whole only if the perturbed run
// reproduced every row of it at the margin of that row, and none does (0 of 54 under each of
// the 47 patterns below). Summary values, row counts and late event times are therefore never
// compared here; they are the stable-step set's.
// QtRocket's whole flights are as close to the golden ones as OpenRocket's own: with glibc 42
// of the 54 simulations have the golden number of rows in every branch, and 48 the apogee
// within 1e-4 and the flight time within 1 ms; the largest differences are 6.3e-5 of an apogee
// and 0.14 s of a flight time, in flights whose rows differ between OpenRocket's own runs too.
// None of that is asserted.
//
// THE MARGIN OF THE FREE FLIGHT. With the margins of the test rockets (a row on the rod is
// reproducible when the perturbed run moves no value of it by more than 1e-12 of its column's
// scale, a row after the rod at 1e-13) the examples would be compared too far. Measured over
// 47 runs of the test binary (glibc, and the 46 patterns of the one-ulp libm shim that the
// test rockets' measurement uses), for a margin of the free flight of 1e3, 1e4, 3e4, 1e5 and
// 1e6 (ExampleSimulationGoldenMeasurement.DISABLED_PrintsTheSensitivityRowByRow prints the row
// profile from which the effect of any margin is read; tier9d-default-step/scripts/patterns.py
// runs it under the patterns and horizons.py reads it, next to the differences between
// OpenRocket's 22 runs in the same rows):
//
//   margin   rows compared      largest difference among the compared values, of the tolerance
//            (47 runs; glibc)   QtRocket from the golden files   between OpenRocket's own runs
//   1e3      4554..5403; 4928   0.19      (factor 5)             5.1      (factor 0.2)
//   1e4      2972..3371; 3136   0.0105    (factor 95)            5.1      (factor 0.2)
//   3e4      2616..2737; 2665   0.0099    (factor 101)           0.038    (factor 26)
//   1e5      2241..2340; 2283   0.00086   (factor 1165)          0.0032   (factor 316)
//   1e6      2008..2019; 2011   0.000053  (factor 18889)         0.000098 (factor 10207)
//
// The one perturbed run does not show everything that moves a row: OpenRocket's own runs differ
// by five times the tolerance in rows that QtRocket's perturbed run moves by less than 1e-13
// (the pitch damping moment coefficient in row 63 of "Simulation 3 - too short delay" of "A
// simple model rocket", where OpenRocket's perturbed runs have parted). So the margin of the
// free flight of the examples is kExampleFlightSensitivityMargin = 1e5, the least power of ten
// for which the tolerance is a hundred times the largest difference in both columns: 316 times
// the largest difference between OpenRocket's own runs among the values compared (3.2e-12 of a
// scale, the pitch rate in row 24 of "Simulation 3" of "Dual parachute deployment") and 1165
// times the largest difference of QtRocket from the golden files (8.6e-13, a damping moment
// coefficient in the first row after the rod of the air-start flight), in each of the 47 runs.
// The margin of the rod stays kSensitivityMargin: the perturbed run moves a value of a rod row
// by 8.0e-14 of its scale at most, and QtRocket differs from the golden files there by 3.6e-14
// at most, over the 47 runs.
// The tolerances are the test rockets' (a value 1e-9 of the scale of its column, a time
// 1e-6 s). Outside the time series 355 numbers are compared, the same in each of the 47 runs
// (tier9d-default-step/scripts/openrocket_numbers.py): the 54 launch rod velocities, which
// differ from the golden ones by 3.8e-16 of themselves at most and between OpenRocket's own
// runs by 2.6e-14, and 301 times of events (the events of the launch, and the burnout of the
// motors that ignite in the reproducible parts), in which QtRocket does not differ from the
// golden files at all and OpenRocket's own runs by 4.4e-16 s.
// What is compared of the time series is then the launch rod and a little more: 2004 rows up
// to the clearing of the rod, which are the floor and are compared on every platform with each
// of their 140336 values (the test of each example holds it, and
// ExampleSimulationGoldenCoverage pins the numbers from the files alone), and 237 to 336 rows
// after it over the 47 runs, as the perturbed run of the platform has it (none at all in ten
// of the branches, in every run). The whole flights are the stable-step set's to compare.
//
// THE TUMBLING OF A DROPPED STAGE. A stage that is dropped and puts out no recovery device is
// left to its attitude, and the engine records a TUMBLE for it when its tumble detector finds
// that it has stopped flying nose first. Whether it does, and whether before or after the
// apogee of the stage, is decided by the last bits: in OpenRocket's 22 runs the booster set of
// "Simulation 1" of "Parallel booster staging" records APOGEE and then TUMBLE in 9, TUMBLE and
// then APOGEE in 8 and no TUMBLE at all in 5 (the committed run among them, with 8462 rows
// where the others have 373 to 8519), and the first booster of the three-stage rocket records
// none in 1 run of "Simulation 1" and in 3 of "Simulation 2". QtRocket's run of the booster
// set, with glibc, records APOGEE and then TUMBLE. Seven branches are of this kind (the booster
// set, and the two boosters of each of the three simulations of the three-stage rocket; a
// dropped stage that puts out a recovery device records the same events in every run, and so
// does every sustainer, the two that tumble after their apogee included). In their branches
// (isUnrecoveredDroppedStage(): a branch after the first with a STAGE_SEPARATION and no
// RECOVERY_DEVICE_DEPLOYMENT, decided from the golden file alone) the TUMBLE events after the
// separation are compared on neither side, and the other events as everywhere, in their order
// (ComparisonMode::droppedStagesTumbleFreely). The six golden TUMBLE events this leaves out are
// counted as sensitive, each with its time, and the test of each example holds the number.
// The comparison of the test rockets does not apply the rule, and does not need it: in the
// same 22 runs each of their 50 simulations records the same events in every run
// (openrocket_structure.py with the kind "testrocket").
//
// THE EXTENSIONS. Four simulations have extensions, and each is compared like every other
// simulation; what that covers of the extension:
// - "Active roll control" of "Simulation extensions" has RollControl with the setting
//   controlFinName = CONTROL. Compared: the entry of the extension; the 71st column that the
//   extension adds (its key "custom:Control fin cant", its name, its symbol and that it is not
//   built in); the events and that there is no warning; and the rows of the launch rod, 46,
//   in which the fin cant is NaN on both sides: the controller starts at 0.5 s, 0.05 s after
//   the rod is cleared, and the reproducible part ends on the rod or one row after it. What
//   the controller does is beyond the horizon here (QtRocket's flight has the golden 864 rows,
//   and its fin cant differs from the golden one by 1.5e-9 rad at most with glibc, which is
//   nine tolerances and not asserted). The flight is the stable-step set's to compare.
// - "Roll control + air-start" has RollControl and AirStart, whose name the default units
//   format ("Air-start (100 m, 50 m/s)") and whose configuration is empty. Compared as above,
//   and: the warning "Listeners modified the flight simulation", in the warnings and as the
//   data of the SIM_WARN that is the first event; and the rows of the launch rod, which hold
//   what AirStart does, number by number: the flight starts at 100 m with 50 m/s and has left
//   the rod after 0.0225 s, in 10 rows, and the launch rod velocity (49.85 m/s) is compared.
// - "Active roll control" and "Roll control + air-start" of "Simulation scripting" have one
//   and two ScriptingExtension entries, each disabled as the file stores it. Compared: the
//   entries with their configurations (enabled false, the language, the text of the script).
//   A disabled script adds nothing to a run, so the flights are those of simulations without
//   an extension: in OpenRocket "Active roll control" is, value for value, the flight of "No
//   controlling" of the same design, and the test holds QtRocket's two runs against each other
//   over the whole flight. (QtRocket never runs a script; a simulation with an enabled one
//   does not fly, and the harness skips such a simulation. The examples have none.)
//
// Not vacuous: ExampleSimulationGoldenMutations changes, one at a time in a copy of the files
// of "Roll control + air-start", what a simulation of a design has and one of a test rocket
// lacks (the source of the options, the index, id and name of the flight configuration, the
// number of extensions, the id, type, name and configuration of one, the column an extension
// adds, the warning of the listeners, the name of the time series) and expects the one line
// that reports it; and it holds the rule of the tumbling dropped stages to what it says, on
// "Simulation 1" of the three-stage rocket: the golden files match with no TUMBLE of the first
// booster after its separation, with three, and with the one before the APOGEE, while a missing
// APOGEE, a TUMBLE before the separation and a TUMBLE of the sustainer are reported, and so
// are the three without the rule. ExampleSimulationGoldenHarness holds runDocumentSimulation()
// against runSimulation(): the document the harness builds for a test rocket gives the flight
// of the test rocket's simulation, value for value, and matches its golden files in everything
// but the source of the options.
//
// The tests are cut by what a process has to simulate, as those of the test rockets: one test
// per example (ctest starts a process per test, and a process reads the motor database and
// runs every simulation of its example twice), two for the mutations (each runs the
// simulations of one example), and the ones that run nothing of an example.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"
#include "QtRocket/simulation/extension/example/AirStart.h"
#include "QtRocket/simulation/extension/example/RollControl.h"
#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"
#include "QtRocket/util/BigDecimal.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenExamples.h"
#include "goldens/GoldenSimulations.h"
#include "rocket/TestRockets.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using nlohmann::json;
using QtRocket::AirStart;
using QtRocket::BigDecimal;
using QtRocket::Config;
using QtRocket::FlightConfigurationId;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::LoadedDocument;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::RollControl;
using QtRocket::ScriptingExtension;
using QtRocket::Simulation;
using QtRocket::SimulationExtension;
using QtRocket::SimulationExtensionRegistry;
using QtRocket::Test::addComparedRun;
using QtRocket::Test::compareSimulation;
using QtRocket::Test::compareWithItsFiles;
using QtRocket::Test::comparisonAfter;
using QtRocket::Test::ComparisonMode;
using QtRocket::Test::configurationIndexOf;
using QtRocket::Test::csvTypes;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::findGoldenExample;
using QtRocket::Test::floorProblem;
using QtRocket::Test::floorRows;
using QtRocket::Test::floorValues;
using QtRocket::Test::goldenCounts;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::goldenExamples;
using QtRocket::Test::goldenExampleTestName;
using QtRocket::Test::GoldenFiles;
using QtRocket::Test::goldenFormOf;
using QtRocket::Test::GoldenInput;
using QtRocket::Test::goldenRodClearance;
using QtRocket::Test::GoldenRun;
using QtRocket::Test::GoldenSimulation;
using QtRocket::Test::GoldenTable;
using QtRocket::Test::InputResult;
using QtRocket::Test::InstallationDefaults;
using QtRocket::Test::isUnrecoveredDroppedStage;
using QtRocket::Test::kSensitivityMargin;
using QtRocket::Test::LastBitListener;
using QtRocket::Test::loadGoldenFiles;
using QtRocket::Test::loadSettledGoldenExample;
using QtRocket::Test::manifest;
using QtRocket::Test::Measurements;
using QtRocket::Test::Mutation;
using QtRocket::Test::mutationProblem;
using QtRocket::Test::NoiseCounts;
using QtRocket::Test::noiseCounts;
using QtRocket::Test::overviewLine;
using QtRocket::Test::PlannedSimulation;
using QtRocket::Test::plannedSimulations;
using QtRocket::Test::runDocumentSimulation;
using QtRocket::Test::runProfile;
using QtRocket::Test::SensitivityMargins;
using QtRocket::Test::sensitivityText;
using QtRocket::Test::SimulationComparison;
using QtRocket::Test::SimulationCounts;
using QtRocket::Test::sortedNames;
using QtRocket::Test::Subject;
using QtRocket::Test::TestRocketMaker;
using QtRocket::Test::testRocketMakers;
using QtRocket::Test::tumblesOfADroppedStage;

// ================================================================================== the runs

/// The margin of the sensitivity analysis after the launch rod, for the examples: a row of the
/// free flight is reproducible when the perturbed run moves no value of it that is compared by
/// more than its tolerance divided by this (1e-14 of the scale of its column), ten times less
/// than in the flights of the test rockets (kFlightSensitivityMargin). It is the least power of
/// ten for which the tolerance of the comparison is a hundred times the largest difference among
/// the values compared, both between OpenRocket's own runs and between QtRocket and the golden
/// files; see "The margin of the free flight" at the top of the file for the measurement.
constexpr double kExampleFlightSensitivityMargin = 1e5;

/// The margins of the sensitivity analysis of the examples: the test rockets' on the launch
/// rod, their own after it, and no run taken as whole for being steady at the margin of the rod.
constexpr SensitivityMargins kExampleMargins{.rod              = kSensitivityMargin,
                                             .flight           = kExampleFlightSensitivityMargin,
                                             .steadyRunIsWhole = false};

/// How the simulations of the examples are compared: under the rules of the default-step set of
/// the test rockets, and under the rule of the tumbling dropped stages (see the top of the file).
constexpr ComparisonMode kExampleMode{.randomConfigurationId     = false,
                                      .strict                    = false,
                                      .measurements              = nullptr,
                                      .stableSet                 = false,
                                      .droppedStagesTumbleFreely = true};

/// The simulations of one example, each run twice (as the harness runs it, and perturbed) and
/// compared with its files.
struct ExampleRuns
{
    std::string problems;         ///< a document that cannot be loaded, or is not the manifest's
    std::vector<GoldenRun> runs;  ///< one per simulation, in the order of the document
};

/// The document of @p example as the harness has it when it runs the simulations: loaded from
/// the file OpenRocket ships and settled (the harness settles the automatic dimensions before
/// it dumps the design, and runs the simulations after that). Null, with the reason added to
/// @p problems, when the example cannot be loaded.
[[nodiscard]] std::shared_ptr<LoadedDocument> settledDocument(const GoldenExample& example,
                                                              std::string&         problems)
{
    Result<LoadedDocument> loaded = loadSettledGoldenExample(example);
    if (!loaded)
    {
        problems += std::format("{}: {}\n", example.file, loaded.error().message);
        return nullptr;
    }
    return std::make_shared<LoadedDocument>(std::move(*loaded));
}

/// Runs simulation @p index of @p document as the harness runs it and that of @p twin with the
/// last-bit perturbation (LastBitListener), and compares the run with the files of simulation
/// @p index of the golden input @p input. The two documents are two loads of the same example.
[[nodiscard]] GoldenRun runGolden(const std::shared_ptr<LoadedDocument>& document,
                                  const std::shared_ptr<LoadedDocument>& twin,
                                  const GoldenInput& input, std::size_t index)
{
    GoldenRun               result;
    const GoldenSimulation& simulation = input.simulations.at(index);
    // The runs first, whatever the files are: the simulations of a document are run in its
    // order, each on the rocket the one before it left.
    result.run  = runDocumentSimulation(document, index, {});
    result.twin = runDocumentSimulation(
        twin, index, {.stableTimeStep = {}, .perturbation = std::make_shared<LastBitListener>()});
    result.files = loadGoldenFiles(simulation);
    if (!result.files.problem.empty())
    {
        result.problem = result.files.problem;
        return result;
    }
    result.golden = goldenCounts(result.files);
    if (!result.run.problem.empty() || !result.twin.problem.empty())
    {
        result.problem = result.run.problem + result.twin.problem;
        return result;
    }
    compareWithItsFiles(result, input.name, simulation, kExampleMode, kExampleMargins);
    return result;
}

/// Loads @p example twice and runs every simulation of it, in the order of the document: once
/// as the harness runs it and once perturbed, each in a document of its own, so that every run
/// meets the rocket in the state the runs before it left it in (see "The set-up" at the top).
[[nodiscard]] ExampleRuns runExample(const GoldenExample& example)
{
    const DefaultUnitsGuard units;  // the names of the configurations and of an extension
    ExampleRuns             result;
    const GoldenInput* input = manifest().has_value() ? manifest()->find(example.name) : nullptr;
    if (input == nullptr)
    {
        result.problems = std::format("no golden input named {}\n", example.name);
        return result;
    }
    const std::shared_ptr<LoadedDocument> document = settledDocument(example, result.problems);
    const std::shared_ptr<LoadedDocument> twin     = settledDocument(example, result.problems);
    if (document == nullptr || twin == nullptr)
    {
        return result;
    }
    const std::size_t simulations = document->document->getSimulationCount();
    if (simulations != input->simulations.size())
    {
        result.problems +=
            std::format("{}: the document has {} simulations, the manifest lists {}\n",
                        example.file, simulations, input->simulations.size());
    }
    const std::size_t common = std::min(simulations, input->simulations.size());
    result.runs.reserve(common);
    for (std::size_t i = 0; i < common; i++)
    {
        result.runs.push_back(runGolden(document, twin, *input, i));
    }
    return result;
}

/// runExample() of @p example, run once per test process and kept (std::map keeps the entries
/// where they are): the tests of a process read the same runs.
[[nodiscard]] const ExampleRuns& exampleRuns(const GoldenExample& example)
{
    static std::mutex                                      s_mutex;
    static std::map<std::string, ExampleRuns, std::less<>> s_runs;
    const std::scoped_lock                                 lock{s_mutex};
    auto                                                   cached = s_runs.find(example.name);
    if (cached == s_runs.end())
    {
        cached = s_runs.emplace(example.name, runExample(example)).first;
    }
    return cached->second;
}

// ================================================================== flights that are the same

/// Whether the golden time series @p a and @p b hold the same columns and the same values, NaN
/// in the same places.
[[nodiscard]] bool sameTable(const GoldenTable& a, const GoldenTable& b)
{
    if (a.columns != b.columns || a.rows.size() != b.rows.size())
    {
        return false;
    }
    for (std::size_t row = 0; row < a.rows.size(); row++)
    {
        for (std::size_t column = 0; column < a.columns.size(); column++)
        {
            if (!QtRocket::Test::reproducible(a.rows[row][column], b.rows[row][column], 0.0, 1.0))
            {
                return false;
            }
        }
    }
    return true;
}

/// Whether OpenRocket's flights of the golden simulations @p a and @p b are the same flight:
/// as many branches, with the same time series.
[[nodiscard]] bool sameGoldenFlight(const GoldenFiles& a, const GoldenFiles& b)
{
    return !a.tables.empty() && a.tables.size() == b.tables.size() &&
           std::ranges::equal(a.tables, b.tables, sameTable);
}

/// Where the branches @p a and @p b of two runs differ: "" when they have the same columns
/// with the same values (NaN in the same places), else the first difference.
[[nodiscard]] std::string branchDifference(const FlightDataBranch& a, const FlightDataBranch& b)
{
    const std::vector<const FlightDataType*> types = csvTypes(a);
    if (a.getLength() != b.getLength() || types != csvTypes(b))
    {
        return std::format("{} rows of {} columns and {} rows of {}", a.getLength(), types.size(),
                           b.getLength(), csvTypes(b).size());
    }
    for (const FlightDataType* type : types)
    {
        const std::vector<double>* ours   = a.getView(*type);
        const std::vector<double>* theirs = b.getView(*type);
        for (std::size_t row = 0; ours != nullptr && theirs != nullptr && row < ours->size(); row++)
        {
            if (!QtRocket::Test::reproducible((*ours)[row], (*theirs)[row], 0.0, 1.0))
            {
                return std::format("{} in row {}: {} and {}", type->getName(), row, (*ours)[row],
                                   (*theirs)[row]);
            }
        }
    }
    return {};
}

/// Where the flights of the runs @p a and @p b differ: "" when they are the same flight.
[[nodiscard]] std::string flightDifference(const GoldenRun& a, const GoldenRun& b)
{
    const FlightData* ours   = a.run.data.get();
    const FlightData* theirs = b.run.data.get();
    if (ours == nullptr || theirs == nullptr || ours->getBranchCount() != theirs->getBranchCount())
    {
        return "another number of branches";
    }
    for (std::size_t i = 0; i < ours->getBranchCount(); i++)
    {
        const std::string difference = branchDifference(ours->getBranch(i), theirs->getBranch(i));
        if (!difference.empty())
        {
            return std::format("branch {}: {}", i, difference);
        }
    }
    return {};
}

/// The pairs of simulations of an example that are one flight in OpenRocket (their golden time
/// series are the same, value for value: two simulations of one configuration with the same
/// options, of which one has a script that is disabled), and what is wrong with them here.
struct SameFlights
{
    int         pairs{0};
    std::string problems;  ///< a pair that is not one flight here; "" when every pair is
};
[[nodiscard]] SameFlights sameFlights(const ExampleRuns& runs)
{
    SameFlights same;
    for (std::size_t i = 0; i < runs.runs.size(); i++)
    {
        for (std::size_t j = i + 1; j < runs.runs.size(); j++)
        {
            const GoldenRun& first  = runs.runs[i];
            const GoldenRun& second = runs.runs[j];
            if (!first.problem.empty() || !second.problem.empty() ||
                !sameGoldenFlight(first.files, second.files))
            {
                continue;
            }
            same.pairs++;
            const std::string difference = flightDifference(first, second);
            if (!difference.empty())
            {
                same.problems +=
                    std::format("{} and {} are one flight in OpenRocket, here not: {}\n",
                                first.context, second.context, difference);
            }
        }
    }
    return same;
}

// =============================================================================== the examples

/// The golden TUMBLE events of the simulation @p files that the rule of the tumbling dropped
/// stages leaves out of the comparison (tumblesOfADroppedStage() of its branches).
[[nodiscard]] int tumblesLeftOut(const GoldenFiles& files)
{
    int tumbles = 0;
    for (const json& branch : files.document.at("branches"))
    {
        tumbles += tumblesOfADroppedStage(branch);
    }
    return tumbles;
}

/// The simulations of an example, compared: the sums of GoldenSimulations.h (InputResult, with
/// the floor and the exclusions among the problems), and from the files alone the golden TUMBLE
/// events that the rule of the tumbling dropped stages leaves out.
struct ExampleResult
{
    InputResult sums;
    int         tumbles{0};
};

/// Compares every simulation of @p example.
[[nodiscard]] ExampleResult compareExample(const GoldenExample& example)
{
    ExampleResult      result;
    const ExampleRuns& runs  = exampleRuns(example);
    result.sums.problems     = runs.problems;
    const GoldenInput* input = manifest().has_value() ? manifest()->find(example.name) : nullptr;
    for (std::size_t i = 0; input != nullptr && i < runs.runs.size(); i++)
    {
        addComparedRun(result.sums, runs.runs[i], input->simulations.at(i).json);
        result.tumbles += runs.runs[i].problem.empty() ? tumblesLeftOut(runs.runs[i].files) : 0;
    }
    result.sums.problems += sameFlights(runs).problems;
    return result;
}

/// What the default-step simulation files of an example hold, counted from the files alone
/// (tier9d-default-step/scripts/coverage.py, which counts as goldenCounts(), floorRows() and
/// noiseCounts() do and gives the numbers of the test rockets that SimulationGoldenCoverage
/// pins): the simulations, their branches and the rows of their time series; of the rows those
/// up to the clearing of the launch rod (the floor under what is compared); the noise-dominated
/// out-of-plane columns and their values off the rod, which the rule excludes.
struct ExampleCounts
{
    std::string_view input;
    int              simulations;
    int              branches;
    std::int64_t     rows;
    std::int64_t     floorRows;
    std::int64_t     noiseColumns;
    std::int64_t     noiseValues;
    /// The golden TUMBLE events the rule of the tumbling dropped stages leaves out
    /// (tumblesOfADroppedStage()).
    int tumbles;
};
constexpr std::array<ExampleCounts, 16> kExampleCounts{{
    {.input        = "example-3d-printable-nose-cone-and-fins",
     .simulations  = 5,
     .branches     = 5,
     .rows         = 2175,
     .floorRows    = 132,
     .noiseColumns = 20,
     .noiseValues  = 8172,
     .tumbles      = 0},
    {.input        = "example-a-simple-model-rocket",
     .simulations  = 5,
     .branches     = 5,
     .rows         = 2656,
     .floorRows    = 128,
     .noiseColumns = 20,
     .noiseValues  = 10112,
     .tumbles      = 0},
    {.input        = "example-arc-payload-rocket",
     .simulations  = 1,
     .branches     = 2,
     .rows         = 1407,
     .floorRows    = 32,
     .noiseColumns = 10,
     .noiseValues  = 6875,
     .tumbles      = 0},
    {.input        = "example-airstart-timing",
     .simulations  = 5,
     .branches     = 5,
     .rows         = 3260,
     .floorRows    = 141,
     .noiseColumns = 25,
     .noiseValues  = 15595,
     .tumbles      = 0},
    {.input        = "example-chute-release",
     .simulations  = 2,
     .branches     = 2,
     .rows         = 1048,
     .floorRows    = 70,
     .noiseColumns = 10,
     .noiseValues  = 4890,
     .tumbles      = 0},
    {.input        = "example-clustered-motors",
     .simulations  = 5,
     .branches     = 5,
     .rows         = 1584,
     .floorRows    = 124,
     .noiseColumns = 20,
     .noiseValues  = 5840,
     .tumbles      = 0},
    {.input        = "example-deployable-payload",
     .simulations  = 5,
     .branches     = 10,
     .rows         = 4423,
     .floorRows    = 272,
     .noiseColumns = 40,
     .noiseValues  = 16604,
     .tumbles      = 0},
    {.input        = "example-dual-parachute-deployment",
     .simulations  = 6,
     .branches     = 6,
     .rows         = 3338,
     .floorRows    = 162,
     .noiseColumns = 30,
     .noiseValues  = 15880,
     .tumbles      = 0},
    {.input        = "example-parallel-booster-staging",
     .simulations  = 2,
     .branches     = 3,
     .rows         = 10482,
     .floorRows    = 89,
     .noiseColumns = 16,
     .noiseValues  = 60397,
     .tumbles      = 0},
    {.input        = "example-pods-airframes-and-winglets",
     .simulations  = 5,
     .branches     = 5,
     .rows         = 2418,
     .floorRows    = 169,
     .noiseColumns = 27,
     .noiseValues  = 12401,
     .tumbles      = 0},
    {.input        = "example-pods-powered-with-recovery-deployment",
     .simulations  = 1,
     .branches     = 1,
     .rows         = 332,
     .floorRows    = 35,
     .noiseColumns = 5,
     .noiseValues  = 1485,
     .tumbles      = 0},
    {.input        = "example-simulation-extensions",
     .simulations  = 3,
     .branches     = 3,
     .rows         = 2805,
     .floorRows    = 102,
     .noiseColumns = 0,
     .noiseValues  = 0,
     .tumbles      = 0},
    {.input        = "example-simulation-scripting",
     .simulations  = 3,
     .branches     = 3,
     .rows         = 2938,
     .floorRows    = 125,
     .noiseColumns = 0,
     .noiseValues  = 0,
     .tumbles      = 0},
    {.input        = "example-three-stage-low-power-rocket",
     .simulations  = 3,
     .branches     = 9,
     .rows         = 5311,
     .floorRows    = 288,
     .noiseColumns = 36,
     .noiseValues  = 20092,
     .tumbles      = 6},
    {.input        = "example-tube-fin-rocket",
     .simulations  = 1,
     .branches     = 1,
     .rows         = 610,
     .floorRows    = 23,
     .noiseColumns = 6,
     .noiseValues  = 3522,
     .tumbles      = 0},
    {.input        = "example-two-stage-high-power-rocket",
     .simulations  = 2,
     .branches     = 4,
     .rows         = 1559,
     .floorRows    = 112,
     .noiseColumns = 21,
     .noiseValues  = 7639,
     .tumbles      = 0},
}};

/// The pinned counts of the example whose golden input is @p input; those of an example
/// without simulations when the table has none (the test then fails on its counts).
[[nodiscard]] ExampleCounts pinnedCounts(std::string_view input)
{
    const std::span<const ExampleCounts> all{kExampleCounts};
    const auto found = std::ranges::find(all, input, &ExampleCounts::input);
    return found == all.end() ? ExampleCounts{.input        = input,
                                              .simulations  = 0,
                                              .branches     = 0,
                                              .rows         = 0,
                                              .floorRows    = 0,
                                              .noiseColumns = 0,
                                              .noiseValues  = 0,
                                              .tumbles      = 0}
                              : *found;
}

// ===================================================================================== tests

/// One of OpenRocket's example designs: its golden simulations of the default-step set.
class ExampleSimulationGolden : public ::testing::TestWithParam<GoldenExample>
{ };

// Every simulation of the loaded, settled example is run and compared as far as it is
// reproducible, under the rules of the default-step comparison of the test rockets
// (SimulationGolden of simulation_golden_tests.cpp) and the rule of the tumbling dropped stages:
// what was compared, what is sensitive and what the rule of the out-of-plane noise columns
// excludes add up to what the files hold, and the files hold what is pinned here for the
// example. The reproducible part has its floor (floorProblem(), among the problems): every
// simulation of the examples is a flight, and a flight is reproducible in every branch at least
// until it has cleared the launch rod; so at least the rows up to the clearing, and every value
// of them, were compared.
TEST_P(ExampleSimulationGolden, EverySimulationOfTheExample)
{
    const ExampleResult result = compareExample(GetParam());
    const InputResult&  sums   = result.sums;
    const ExampleCounts pinned = pinnedCounts(GetParam().name);
    ASSERT_EQ(sums.problems, "");
    EXPECT_EQ(sums.report, "");
    // Everything the files hold was compared, is sensitive or is excluded by the rule: nothing
    // skipped. And the files hold what is pinned.
    EXPECT_EQ(sums.compared + sums.sensitive + sums.excluded, sums.golden)
        << "what was compared, is sensitive or is excluded, and what the files hold";
    EXPECT_EQ(sums.golden.simulations, pinned.simulations);
    EXPECT_EQ(sums.golden.branches, pinned.branches);
    EXPECT_EQ(sums.golden.rows, pinned.rows);
    // The structure is compared in every simulation, sensitive or not, but for the TUMBLE
    // events of the dropped stages that tumble or do not; and the rule of the noise columns
    // excludes values of the time series and the extremes of its columns, no row.
    EXPECT_EQ(sums.compared.simulations, pinned.simulations);
    EXPECT_EQ(sums.compared.branches, pinned.branches);
    EXPECT_EQ(result.tumbles, pinned.tumbles);
    EXPECT_EQ(sums.sensitive.events, pinned.tumbles);
    EXPECT_EQ(sums.sensitive.simulations + sums.sensitive.branches + sums.sensitive.columns +
                  sums.sensitive.warnings,
              0);
    EXPECT_EQ(sums.excluded.simulations + sums.excluded.branches + sums.excluded.events +
                  sums.excluded.columns + sums.excluded.warnings + sums.excluded.rows,
              0);
    EXPECT_EQ(sums.noise,
              (NoiseCounts{.columns = pinned.noiseColumns, .values = pinned.noiseValues}))
        << sums.noise;
    EXPECT_EQ(sums.excluded.values, sums.noise.values) << sums.noise;
    EXPECT_LE(sums.excluded.numbers, 2 * sums.noise.columns) << sums.noise;
    // The floor, in sums: every simulation is a flight, and at least the rows up to the
    // clearing of the rod were compared, with every value of them.
    EXPECT_EQ(sums.flights, pinned.simulations);
    EXPECT_EQ(sums.floorRows, pinned.floorRows);
    EXPECT_GE(sums.compared.rows, sums.floorRows);
    EXPECT_GE(sums.compared.values, sums.floorValues);
    EXPECT_GT(sums.floorValues, sums.floorRows);
}

INSTANTIATE_TEST_SUITE_P(Examples, ExampleSimulationGolden, ::testing::ValuesIn(goldenExamples()),
                         goldenExampleTestName);

// ================================================================================== coverage

/// What the default-step simulation files of the examples hold, read from the files alone (no
/// simulation is run).
struct ExampleCoverage
{
    int              inputs{0};  ///< the "example" inputs of the manifest
    SimulationCounts golden;     ///< what their simulation files hold
    int              flights{0};
    std::int64_t     floorRows{0};
    std::int64_t     floorValues{0};
    NoiseCounts      noise;
    int              unrecoveredStages{0};  ///< the branches of isUnrecoveredDroppedStage()
    int              tumbles{0};            ///< tumblesLeftOut() of the simulations
    int              extensions{0};
    int              simulationsWithExtensions{0};
    /// The pairs of simulations of one input whose golden flights are the same
    /// (sameGoldenFlight()).
    int         sameFlights{0};
    std::string problems;  ///< a file that cannot be read, counts that are not the pinned ones
};

/// What the files of one golden input hold of what kExampleCounts pins.
struct CountsOfAnInput
{
    SimulationCounts golden;
    std::int64_t     floorRows{0};
    NoiseCounts      noise;
    int              tumbles{0};
};

/// What is wrong with the pinned counts of the golden input @p input, whose files hold
/// @p counts: "" when kExampleCounts has them.
[[nodiscard]] std::string pinProblem(const GoldenInput& input, const CountsOfAnInput& counts)
{
    const ExampleCounts pinned = pinnedCounts(input.name);
    const bool same = pinned.simulations == counts.golden.simulations &&
                      pinned.branches == counts.golden.branches &&
                      pinned.rows == counts.golden.rows && pinned.floorRows == counts.floorRows &&
                      pinned.noiseColumns == counts.noise.columns &&
                      pinned.noiseValues == counts.noise.values && pinned.tumbles == counts.tumbles;
    return same ? std::string{}
                : std::format(
                      "{}: the files hold {} simulations, {} branches, {} rows, {} of them up "
                      "to the clearing of the rod, {} noise columns with {} values off it, {} "
                      "TUMBLE events of dropped stages\n",
                      input.name, counts.golden.simulations, counts.golden.branches,
                      counts.golden.rows, counts.floorRows, counts.noise.columns,
                      counts.noise.values, counts.tumbles);
}

/// The branches of the golden simulation @p files that are those of dropped stages without a
/// recovery device (isUnrecoveredDroppedStage()).
[[nodiscard]] int unrecoveredStages(const GoldenFiles& files)
{
    int stages = 0;
    for (const json& branch : files.document.at("branches"))
    {
        stages += isUnrecoveredDroppedStage(branch) ? 1 : 0;
    }
    return stages;
}

/// The pairs of the golden simulations @p simulations whose flights are the same
/// (sameGoldenFlight()).
[[nodiscard]] int sameFlightPairs(const std::vector<GoldenFiles>& simulations)
{
    int pairs = 0;
    for (std::size_t i = 0; i < simulations.size(); i++)
    {
        for (std::size_t j = i + 1; j < simulations.size(); j++)
        {
            pairs += sameGoldenFlight(simulations[i], simulations[j]) ? 1 : 0;
        }
    }
    return pairs;
}

/// Adds the simulations of the golden input @p input to @p coverage.
void cover(ExampleCoverage& coverage, const GoldenInput& input)
{
    CountsOfAnInput          counts;
    std::vector<GoldenFiles> read;
    read.reserve(input.simulations.size());
    for (const GoldenSimulation& simulation : input.simulations)
    {
        const GoldenFiles& files = read.emplace_back(loadGoldenFiles(simulation));
        if (!files.problem.empty())
        {
            coverage.problems += std::format("{}: {}\n", simulation.json, files.problem);
            continue;
        }
        counts.golden += goldenCounts(files);
        counts.floorRows += floorRows(files);
        counts.noise += noiseCounts(files);
        counts.tumbles += tumblesLeftOut(files);
        const auto extensions = static_cast<int>(files.document.at("extensions").size());
        coverage.flights += goldenRodClearance(files.document).has_value() ? 1 : 0;
        coverage.floorValues += floorValues(files);
        coverage.unrecoveredStages += unrecoveredStages(files);
        coverage.extensions += extensions;
        coverage.simulationsWithExtensions += extensions > 0 ? 1 : 0;
    }
    coverage.golden += counts.golden;
    coverage.floorRows += counts.floorRows;
    coverage.noise += counts.noise;
    coverage.tumbles += counts.tumbles;
    coverage.sameFlights += sameFlightPairs(read);
    coverage.problems += pinProblem(input, counts);
}

/// Reads every simulation of every "example" input of the manifest.
[[nodiscard]] ExampleCoverage exampleCoverage()
{
    ExampleCoverage coverage;
    if (!manifest())
    {
        coverage.problems = manifest().error().message;
        return coverage;
    }
    for (const GoldenInput& input : manifest()->inputs)
    {
        if (input.kind != "example")
        {
            continue;
        }
        coverage.inputs++;
        if (findGoldenExample(input.name) == nullptr)
        {
            coverage.problems += std::format("{}: not an example of the fixture\n", input.name);
        }
        cover(coverage, input);
    }
    return coverage;
}

/// Every golden simulation of the examples is compared: each of the sixteen inputs is an
/// example of the fixture (goldenExamples()), and the test of each example
/// (ExampleSimulationGolden) checks that what it compared, what is sensitive and what is
/// excluded add up to what the files of its input hold, and that the floor holds. This test
/// pins what the files hold, per example (kExampleCounts) and in all, and the floor in all; it
/// runs no simulation.
TEST(ExampleSimulationGoldenCoverage, EveryGoldenSimulationOfTheExamplesIsOfAnExampleOfTheFixture)
{
    const ExampleCoverage coverage = exampleCoverage();
    EXPECT_EQ(coverage.problems, "");
    EXPECT_EQ(coverage.inputs, 16);
    EXPECT_EQ(goldenExamples().size(), 16U);
    EXPECT_EQ(kExampleCounts.size(), 16U);
    EXPECT_EQ(coverage.golden.simulations, 54) << "every simulation of the sixteen designs";
    EXPECT_EQ(coverage.golden.branches, 69);
    EXPECT_EQ(coverage.golden.events, 733);
    EXPECT_EQ(coverage.golden.columns, 4832);
    EXPECT_EQ(coverage.golden.warnings, 20);
    EXPECT_EQ(coverage.golden.numbers, 11368);
    EXPECT_EQ(coverage.golden.rows, 46346);
    EXPECT_EQ(coverage.golden.values, 3246066);
    // The floor (floorProblem()): every one of the 54 simulations is a flight, which is
    // reproducible in each of its 69 branches until it has cleared the rod: 2004 rows, which
    // are compared on every platform with each of their 140336 values.
    EXPECT_EQ(coverage.flights, 54);
    EXPECT_EQ(coverage.floorRows, 2004);
    EXPECT_EQ(coverage.floorValues, 140336);
    // The rule of the out-of-plane noise columns (RodOnlyRule), from the files alone: 286
    // columns are noise-dominated (the two lateral accelerations across the wind, the lateral
    // position across it and its direction in 63 of the 69 branches, the yaw rate in 29 of
    // them and the roll rate in 5; none in the six flights of the two designs with the
    // extensions, which do not fly in a plane). Their 189504 values off the rod are not
    // compared, in any run.
    EXPECT_EQ(coverage.noise, (NoiseCounts{.columns = 286, .values = 189504})) << coverage.noise;
    // The rule of the tumbling dropped stages, from the files alone: seven branches are those
    // of dropped stages without a recovery device (the two boosters of each simulation of the
    // three-stage rocket and the booster set of the parallel staging), and six of them record
    // a TUMBLE after their separation in the golden run, which is not compared.
    EXPECT_EQ(coverage.unrecoveredStages, 7);
    EXPECT_EQ(coverage.tumbles, 6);
    // The extensions: six, in four simulations (RollControl in two of "Simulation extensions",
    // one of them with AirStart; a disabled script in two of "Simulation scripting", one of
    // them with a second one).
    EXPECT_EQ(coverage.extensions, 6);
    EXPECT_EQ(coverage.simulationsWithExtensions, 4);
    // One pair of simulations is one flight in OpenRocket, value for value: "No controlling" and
    // "Active roll control" of "Simulation scripting", which differ in a disabled script. The
    // test of that example holds QtRocket's two runs against each other (sameFlights()).
    EXPECT_EQ(coverage.sameFlights, 1);
}

// =============================================================================== measurement

/// What the measurements print of the examples.
struct MeasuredExamples
{
    std::string  problems;
    Measurements measurements;
    std::string  sensitivity;  ///< sensitivityText() of the runs that are not whole
    std::string  overview;     ///< overviewLine() of every run
    std::string  profile;      ///< runProfile() of every run
    int          whole{0};     ///< the runs that are reproducible as a whole
};

/// Adds the runs of @p example to @p measured.
void measure(MeasuredExamples& measured, const GoldenExample& example)
{
    const ExampleRuns& runs = exampleRuns(example);
    measured.problems += runs.problems;
    for (const GoldenRun& run : runs.runs)
    {
        if (!run.problem.empty())
        {
            measured.problems += std::format("{}: {}\n", example.name, run.problem);
            continue;
        }
        ComparisonMode mode = kExampleMode;
        mode.measurements   = &measured.measurements;
        const SimulationComparison comparison =
            compareSimulation(run.files, run.run, run.sensitivity, run.context, mode);
        measured.problems += comparison.report;
        measured.whole += run.sensitivity.whole ? 1 : 0;
        measured.sensitivity += run.sensitivity.whole
                                    ? std::string{}
                                    : std::format("{}:{}\n", run.context, sensitivityText(run));
        measured.overview += overviewLine(run, measured.measurements);
        measured.profile += runProfile(run);
    }
}

/// The runs of every example, measured.
[[nodiscard]] MeasuredExamples measuredExamples()
{
    MeasuredExamples measured;
    for (const GoldenExample& example : goldenExamples())
    {
        measure(measured, example);
    }
    return measured;
}

/// The measurement behind the tolerances, as SimulationGoldenMeasurement prints it for the
/// test rockets: per simulation, how far it is reproducible, and a table of the largest
/// differences from the golden files, one line per simulation and thing compared, separately
/// for what is compared, for what is sensitive and for what the rule of the out-of-plane noise
/// columns excludes; then an overview, one line per simulation. Disabled; run it with
/// --gtest_also_run_disabled_tests (under a libm of another platform, or the one-ulp shim, to
/// see what the tolerances have to cover there: tier9d-default-step/scripts/patterns.py).
TEST(ExampleSimulationGoldenMeasurement, DISABLED_PrintsTheDifferencesFromTheGoldenFiles)
{
    const MeasuredExamples measured = measuredExamples();
    std::cout << "Not reproducible as a whole:\n"
              << measured.sensitivity
              << "simulation\twhat\tcompared, sensitive or excluded\tvalues\tlargest difference\t"
                 "... as a multiple of its tolerance\n"
              << measured.measurements.text()
              << "simulation\tapogee (m)\t... in the golden file\tflight time (s)\t"
                 "... in the golden file\trows per branch\t... in the golden file\t"
                 "largest difference of the time series, of the column's scale\n"
              << measured.overview << std::format("Whole runs: {}\n", measured.whole)
              << std::format(
                     "The largest difference of a compared value is {:.3e} of its "
                     "tolerance.\n",
                     measured.measurements.largestComparedOfTolerance());
    EXPECT_EQ(measured.problems, "");
    EXPECT_LE(measured.measurements.largestComparedOfTolerance(), 1.0);
}

/// The measurement behind the margins of the sensitivity analysis, as SimulationGoldenMeasurement
/// prints it for the test rockets: for every row of every branch up to the first one that is
/// not reproducible at any margin, the largest difference of the perturbed run from the run and
/// the largest difference of the run from the golden file (runProfile()). From it the effect of
/// any margin is read (tier9d-default-step/scripts/horizons.py, which also holds what is
/// compared against the differences between OpenRocket's own runs). Disabled; run it with
/// --gtest_also_run_disabled_tests.
TEST(ExampleSimulationGoldenMeasurement, DISABLED_PrintsTheSensitivityRowByRow)
{
    const MeasuredExamples measured = measuredExamples();
    std::cout << "simulation\tbranch\trow\ton the rod or in free flight\t"
                 "largest difference of the perturbed run, of the column's scale\t... its column\t"
                 "largest difference from the golden file, of the column's scale\t... its column\n"
              << measured.profile;
    EXPECT_EQ(measured.problems, "");
}

// ================================================================================= mutations

// The comparison of what a simulation of a design has and a simulation of a test rocket lacks
// is not vacuous: a golden value changed in a copy of the files is reported, in one line that
// names it. The simulation is "Roll control + air-start" of "Simulation extensions": its
// options are the document's, it has two extensions, one of them with a setting and the other
// with a name that the units format, a column that an extension adds, and a warning that a
// listener caused. (The changes to everything else of a simulation are those of
// SimulationGoldenMutation, on simulations of the test rockets: the comparison is the same
// code.)

/// The simulation with the two extensions.
constexpr Subject kWithExtensions{.input = "example-simulation-extensions", .index = 2};

/// The changes to what identifies a simulation of a design and to its extensions.
[[nodiscard]] std::vector<Mutation> documentMutations()
{
    return {
        {.name    = "OptionsSource",
         .subject = kWithExtensions,
         .apply =
             [](GoldenFiles& files) { files.document["optionsSource"] = "applicationDefaults"; },
         .heading = "",
         .line    = R"(  optionsSource: expected "applicationDefaults", got "document")"},
        {.name    = "IndexOfTheSimulation",
         .subject = kWithExtensions,
         .apply   = [](GoldenFiles& files) { files.document["index"] = 1; },
         .heading = "",
         .line    = "  index: expected 1, got 2"},
        {.name    = "IndexOfTheFlightConfiguration",
         .subject = kWithExtensions,
         .apply   = [](GoldenFiles& files) { files.document["flightConfiguration"]["index"] = 0; },
         .heading = "",
         .line    = "  flightConfiguration.index: expected 0, got 1"},
        {.name    = "IdOfTheFlightConfiguration",
         .subject = kWithExtensions,
         .apply   = [](GoldenFiles& files) { files.document["flightConfiguration"]["id"] = "x"; },
         .heading = "",
         .line    = R"(  flightConfiguration.id: expected "x", got "487170d2-)"},
        {.name    = "NameOfTheFlightConfiguration",
         .subject = kWithExtensions,
         .apply =
             [](GoldenFiles& files) {
                 files.document["flightConfiguration"]["name"] = "[2800CC172L-L540-P]";
             },
         .heading = "",
         .line = R"(  flightConfiguration.name: expected "[2800CC172L-L540-P]", got "[L540-P]")"},
        {.name    = "NumberOfExtensions",
         .subject = kWithExtensions,
         .apply   = [](GoldenFiles& files) { files.document["extensions"].erase(1); },
         .heading = "",
         .line    = "  extensions: number: expected 1, got 2"},
        {.name    = "IdOfAnExtension",
         .subject = kWithExtensions,
         .apply   = [](GoldenFiles& files) { files.document["extensions"][0]["id"] = "x"; },
         .heading = "",
         .line    = R"(  extensions[0].id: expected "x", got )"
                    R"("info.openrocket.core.simulation.extension.example.RollControl")"},
        {.name    = "TypeOfAnExtension",
         .subject = kWithExtensions,
         .apply =
             [](GoldenFiles& files) { files.document["extensions"][1]["type"] = "RollControl"; },
         .heading = "",
         .line    = R"(  extensions[1].type: expected "RollControl", got "AirStart")"},
        {.name    = "NameOfAnExtension",
         .subject = kWithExtensions,
         .apply =
             [](GoldenFiles& files) {
                 files.document["extensions"][1]["name"] = "Air-start (200 m, 50 m/s)";
             },
         .heading = "",
         .line    = R"x(  extensions[1].name: expected "Air-start (200 m, 50 m/s)", got )x"
                    R"x("Air-start (100 m, 50 m/s)")x"},
        {.name    = "SettingOfAnExtension",
         .subject = kWithExtensions,
         .apply =
             [](GoldenFiles& files) {
                 files.document["extensions"][0]["config"]["controlFinName"] = "FINS";
             },
         .heading = "",
         .line    = R"(  extensions[0].config: expected "{"controlFinName":"FINS"}", got )"
                    R"("{"controlFinName":"CONTROL"}")"},
        {.name    = "SettingAnExtensionDoesNotHave",
         .subject = kWithExtensions,
         .apply =
             [](GoldenFiles& files) {
                 files.document["extensions"][1]["config"]["launchAltitude"] = 100.0;
             },
         .heading = "",
         .line    = R"(  extensions[1].config: expected "{"launchAltitude":100.0}", got "{}")"},
        {.name    = "UnknownFieldOfAnExtension",
         .subject = kWithExtensions,
         .apply   = [](GoldenFiles& files) { files.document["extensions"][0]["enabled"] = true; },
         .heading = "",
         .line    = "  extensions[0].enabled: not compared"},
    };
}

/// The changes to what the extensions leave in the flight data, and to the name of a file.
[[nodiscard]] std::vector<Mutation> flightMutations()
{
    return {
        {.name    = "TextOfTheWarningOfTheListeners",
         .subject = kWithExtensions,
         .apply   = [](GoldenFiles& files) { files.document["warnings"][0]["text"] = "Listeners"; },
         .heading = "",
         .line    = R"(  warnings[0].text: expected "Listeners")"},
        {.name    = "KeyOfTheColumnOfAnExtension",
         .subject = kWithExtensions,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["columns"][70]["key"] = "custom:Fin cant";
             },
         .heading = " branch 0",
         .line    = R"(  columns[70].key: expected "custom:Fin cant", got )"
                    R"("custom:Control fin cant")"},
        {.name    = "SymbolOfTheColumnOfAnExtension",
         .subject = kWithExtensions,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["columns"][70]["symbol"] = "fc";
             },
         .heading = " branch 0",
         .line    = R"(  columns[70].symbol: expected "fc", got ")"},
        {.name    = "ColumnOfAnExtensionIsBuiltIn",
         .subject = kWithExtensions,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["columns"][70]["builtin"] = true;
             },
         .heading = " branch 0",
         .line    = "  columns[70].builtin: expected true, got false"},
        {.name    = "WarningOfTheFirstEvent",
         .subject = kWithExtensions,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["events"][0]["data"]["priority"] = "HIGH";
             },
         .heading = " branch 0",
         .line    = R"(  events[0].data.priority: expected "HIGH", got "LOW")"},
        {.name    = "NameOfTheTimeSeries",
         .subject = kWithExtensions,
         .apply = [](GoldenFiles& files) { files.document["branches"][0]["csv"] = "other.csv.gz"; },
         .heading = " branch 0",
         .line    = R"(  csv: expected "other.csv.gz", got )"
                    R"("sim_02_roll-control-air-start_branch0.csv.gz")"},
        {.name    = "NoTimeSeries",
         .subject = kWithExtensions,
         .apply   = [](GoldenFiles& files) { files.document["branches"][0]["csv"] = nullptr; },
         .heading = " branch 0",
         .line = R"(  csv: expected "null", got "sim_02_roll-control-air-start_branch0.csv.gz")"},
        {.name    = "NameOfTheTimeSeriesIsNoText",
         .subject = kWithExtensions,
         .apply   = [](GoldenFiles& files) { files.document["branches"][0]["csv"] = 7; },
         .heading = " branch 0",
         .line    = "  csv: "},
    };
}

/// Every change.
[[nodiscard]] std::vector<Mutation> mutations()
{
    std::vector<Mutation> all = documentMutations();
    for (const Mutation& mutation : flightMutations())
    {
        all.push_back(mutation);
    }
    return all;
}

/// The run of the simulation @p subject of an example, or null (with a test failure) when
/// there is none.
[[nodiscard]] const GoldenRun* subjectRun(const Subject& subject)
{
    const GoldenExample* example = findGoldenExample(subject.input);
    if (example == nullptr)
    {
        ADD_FAILURE() << "no example named " << subject.input;
        return nullptr;
    }
    const ExampleRuns& runs = exampleRuns(*example);
    if (!runs.problems.empty() || subject.index >= runs.runs.size() ||
        !runs.runs[subject.index].problem.empty())
    {
        ADD_FAILURE() << "no run of simulation " << subject.index << " of " << subject.input << ": "
                      << runs.problems;
        return nullptr;
    }
    return &runs.runs[subject.index];
}

/// What is wrong with the reports of the mutations @p all, each applied to a copy of the files
/// of its simulation; "" when every one is reported in its one line.
[[nodiscard]] std::string mutationProblems(const std::vector<Mutation>& all)
{
    std::string problems;
    for (const Mutation& mutation : all)
    {
        const GoldenRun* run = subjectRun(mutation.subject);
        problems += run == nullptr ? "no run\n" : mutationProblem(mutation, *run);
    }
    return problems;
}

TEST(ExampleSimulationGoldenMutations, EveryChangeToASimulationWithExtensionsIsReportedInOneLine)
{
    const std::vector<Mutation>         all   = mutations();
    const std::vector<std::string_view> names = sortedNames(all);
    EXPECT_EQ(all.size(), 20U);
    EXPECT_EQ(std::ranges::adjacent_find(names), names.end()) << "two mutations of one name";
    EXPECT_EQ(mutationProblems(all), "");
    // The files as they are match (the test of the example says so as well; here it is the
    // comparison the mutations go through).
    const GoldenRun* run = subjectRun(kWithExtensions);
    ASSERT_NE(run, nullptr);
    EXPECT_EQ(comparisonAfter(*run, nullptr).report, "");
}

// ------------------------------------------------------------- the tumbling of a dropped stage

/// "Simulation 1" of the three-stage rocket. Its third branch is that of the booster that is
/// dropped first, a stage without a recovery device, whose golden events are IGNITION, BURNOUT,
/// EJECTION_CHARGE, STAGE_SEPARATION (3), APOGEE (4), TUMBLE (5), GROUND_HIT and SIMULATION_END.
/// Its second branch, the other booster, records its TUMBLE before its APOGEE; its first, the
/// sustainer, has a parachute and its last STAGE_SEPARATION as event 11.
constexpr Subject kWithDroppedStages{.input = "example-three-stage-low-power-rocket", .index = 0};
constexpr std::size_t kFirstBooster               = 2;
constexpr std::size_t kSeparationOfTheBooster     = 3;
constexpr std::size_t kApogeeOfTheBooster         = 4;
constexpr std::size_t kTumbleOfTheBooster         = 5;
constexpr std::size_t kAfterTheLastSeparation     = 12;
constexpr int         kGoldenTumblesOfTheBoosters = 2;

/// The golden events of the first booster.
[[nodiscard]] json& eventsOfTheFirstBooster(GoldenFiles& files)
{
    return files.document["branches"][kFirstBooster]["events"];
}

/// Changes to the TUMBLE of the first booster, after its separation: none at all, three of
/// them, and the one before the APOGEE.
void removeTheTumble(GoldenFiles& files)
{
    eventsOfTheFirstBooster(files).erase(kTumbleOfTheBooster);
}
void addTwoTumbles(GoldenFiles& files)
{
    json&      events = eventsOfTheFirstBooster(files);
    const json tumble = events.at(kTumbleOfTheBooster);
    events.insert(events.begin() + kTumbleOfTheBooster, 2, tumble);
}
void tumbleBeforeTheApogee(GoldenFiles& files)
{
    json& events = eventsOfTheFirstBooster(files);
    std::swap(events[kApogeeOfTheBooster], events[kTumbleOfTheBooster]);
}

/// Changes that the rule does not cover: no APOGEE of the first booster; a TUMBLE of it before
/// its separation; and a TUMBLE of the sustainer, after its last separation.
void removeTheApogee(GoldenFiles& files)
{
    eventsOfTheFirstBooster(files).erase(kApogeeOfTheBooster);
}
void tumbleBeforeTheSeparation(GoldenFiles& files)
{
    json&      events = eventsOfTheFirstBooster(files);
    const json tumble = events.at(kTumbleOfTheBooster);
    events.insert(events.begin() + kSeparationOfTheBooster, tumble);
}
void tumbleOfTheSustainer(GoldenFiles& files)
{
    const json tumble = eventsOfTheFirstBooster(files).at(kTumbleOfTheBooster);
    json&      events = files.document["branches"][0]["events"];
    events.insert(events.begin() + kAfterTheLastSeparation, tumble);
}

/// What is wrong with the report @p report of a comparison of @p run in which the events of
/// branch @p branch were changed: "" when it is the heading of the branch and the one line of
/// its events (@p alone), or holds them among others.
[[nodiscard]] std::string eventsLineProblem(const std::string& report, const GoldenRun& run,
                                            std::size_t branch, bool alone)
{
    const std::string start =
        std::format("{} branch {}:\n  events: expected \"", run.context, branch);
    const bool found = alone ? report.starts_with(start) && std::ranges::count(report, '\n') == 2
                             : report.contains(start);
    return found ? std::string{}
                 : std::format("the report\n{}\ndoes not hold the line\n{}...\n", report, start);
}

/// The comparison of @p run with a copy of its files that @p change changed, and what the copy
/// holds.
struct ChangedComparison
{
    SimulationComparison comparison;
    SimulationCounts     golden;
};
[[nodiscard]] ChangedComparison changedComparison(const GoldenRun& run,
                                                  void (*change)(GoldenFiles& files),
                                                  const ComparisonMode& mode)
{
    GoldenFiles files = run.files;
    change(files);
    return {.comparison = compareSimulation(files, run.run, run.sensitivity, run.context, mode),
            .golden     = goldenCounts(files)};
}

// The rule of the tumbling dropped stages: a TUMBLE that a dropped stage without a recovery
// device records after its separation is compared on neither side, so the golden files match
// QtRocket's run whether they record none, one or three, and before or after the APOGEE; what
// is left out is counted as sensitive. Every other event of such a stage is compared, a TUMBLE
// before its separation as well, and so is the TUMBLE of any other branch. The comparison of
// the test rockets, without the rule, reports the difference.
TEST(ExampleSimulationGoldenMutations, TheTumbleOfADroppedStageIsNotComparedAndItsOtherEventsAre)
{
    const GoldenRun* run = subjectRun(kWithDroppedStages);
    ASSERT_NE(run, nullptr);
    ASSERT_TRUE(run->mode.droppedStagesTumbleFreely);
    const SimulationComparison unchanged = comparisonAfter(*run, nullptr);
    EXPECT_EQ(unchanged.report, "");
    EXPECT_EQ(unchanged.sensitive.events, kGoldenTumblesOfTheBoosters);
    EXPECT_EQ(unchanged.compared + unchanged.sensitive + unchanged.excluded, run->golden);

    const ChangedComparison without = changedComparison(*run, removeTheTumble, run->mode);
    EXPECT_EQ(without.comparison.report, "");
    EXPECT_EQ(without.comparison.sensitive.events, kGoldenTumblesOfTheBoosters - 1);
    EXPECT_EQ(without.comparison.compared.events, unchanged.compared.events);
    EXPECT_EQ(
        without.comparison.compared + without.comparison.sensitive + without.comparison.excluded,
        without.golden);
    const ChangedComparison three = changedComparison(*run, addTwoTumbles, run->mode);
    EXPECT_EQ(three.comparison.report, "");
    EXPECT_EQ(three.comparison.sensitive.events, kGoldenTumblesOfTheBoosters + 2);
    EXPECT_EQ(three.comparison.compared.events, unchanged.compared.events);
    EXPECT_EQ(three.comparison.compared + three.comparison.sensitive + three.comparison.excluded,
              three.golden);
    EXPECT_EQ(comparisonAfter(*run, tumbleBeforeTheApogee).report, "");

    // What the rule does not cover is reported, in the one line of the events of its branch.
    EXPECT_EQ(
        eventsLineProblem(comparisonAfter(*run, removeTheApogee).report, *run, kFirstBooster, true),
        "");
    EXPECT_EQ(eventsLineProblem(comparisonAfter(*run, tumbleBeforeTheSeparation).report, *run,
                                kFirstBooster, true),
              "");
    EXPECT_EQ(eventsLineProblem(comparisonAfter(*run, tumbleOfTheSustainer).report, *run, 0, true),
              "");

    // Without the rule three TUMBLE events are not QtRocket's, whatever its run records (it
    // records one or none; the other booster may then be reported as well).
    ComparisonMode withoutTheRule            = run->mode;
    withoutTheRule.droppedStagesTumbleFreely = false;
    EXPECT_EQ(
        eventsLineProblem(changedComparison(*run, addTwoTumbles, withoutTheRule).comparison.report,
                          *run, kFirstBooster, false),
        "");
}

/// A golden branch with the index @p index and events of the types @p types, in their order.
[[nodiscard]] json branchWith(int index, std::initializer_list<std::string_view> types)
{
    json events = json::array();
    for (const std::string_view type : types)
    {
        events.push_back({{"type", std::string{type}}});
    }
    return {{"index", index}, {"events", std::move(events)}};
}

// The branches the rule of the tumbling dropped stages applies to are told from their golden
// events alone: a branch after the first, of a stage that separated and put out no recovery
// device. Of such a branch the TUMBLE events after its last separation are left out, and no
// other.
TEST(ExampleSimulationGoldenMutations, ADroppedStageWithoutARecoveryDeviceIsToldFromItsEvents)
{
    const json booster = branchWith(
        1, {"IGNITION", "BURNOUT", "STAGE_SEPARATION", "APOGEE", "TUMBLE", "GROUND_HIT"});
    EXPECT_TRUE(isUnrecoveredDroppedStage(booster));
    EXPECT_EQ(tumblesOfADroppedStage(booster), 1);

    // The first branch is the flight of the rocket, whatever it drops and however it comes down.
    const json sustainer =
        branchWith(0, {"LAUNCH", "IGNITION", "STAGE_SEPARATION", "APOGEE", "TUMBLE", "GROUND_HIT"});
    EXPECT_FALSE(isUnrecoveredDroppedStage(sustainer));
    EXPECT_EQ(tumblesOfADroppedStage(sustainer), 0);

    // A dropped stage under a recovery device, wherever in its events the device comes out.
    const json recovered = branchWith(
        2, {"IGNITION", "STAGE_SEPARATION", "TUMBLE", "APOGEE", "RECOVERY_DEVICE_DEPLOYMENT"});
    EXPECT_FALSE(isUnrecoveredDroppedStage(recovered));
    EXPECT_EQ(tumblesOfADroppedStage(recovered), 0);

    // A later branch without a separation is no dropped stage.
    const json unseparated = branchWith(1, {"IGNITION", "APOGEE", "TUMBLE", "GROUND_HIT"});
    EXPECT_FALSE(isUnrecoveredDroppedStage(unseparated));
    EXPECT_EQ(tumblesOfADroppedStage(unseparated), 0);

    // Only what follows the last separation is left out: of three TUMBLE events, the two after
    // it; and none when the stage does not tumble.
    const json twice = branchWith(1, {"IGNITION", "STAGE_SEPARATION", "TUMBLE", "STAGE_SEPARATION",
                                      "TUMBLE", "APOGEE", "TUMBLE", "GROUND_HIT"});
    EXPECT_TRUE(isUnrecoveredDroppedStage(twice));
    EXPECT_EQ(tumblesOfADroppedStage(twice), 2);
    const json steady = branchWith(3, {"IGNITION", "STAGE_SEPARATION", "APOGEE", "GROUND_HIT"});
    EXPECT_TRUE(isUnrecoveredDroppedStage(steady));
    EXPECT_EQ(tumblesOfADroppedStage(steady), 0);
}

// ==================================================================================== harness

/// The names of the extensions @p ids of the bundled registry, as their golden forms have them.
[[nodiscard]] json bundledForm(std::string_view id)
{
    const SimulationExtensionRegistry          registry  = SimulationExtensionRegistry::bundled();
    const std::unique_ptr<SimulationExtension> extension = registry.create(id);
    return extension == nullptr ? json{} : goldenFormOf(*extension);
}

// SimulationDumper.extensions(): the id, Java's simple class name, the name and the settings
// of an extension. A new extension has no setting (its getters answer the defaults), which is
// what the golden files have for the AirStart of the examples.
TEST(ExampleSimulationGoldenHarness, AnExtensionIsDescribedAsTheDumperDescribesIt)
{
    const DefaultUnitsGuard units;  // AirStart's name prints a length and a speed
    EXPECT_EQ(
        bundledForm(RollControl::kId),
        json::parse(R"json({"id": "info.openrocket.core.simulation.extension.example.RollControl",
                            "type": "RollControl", "name": "Roll Control", "config": {}})json"));
    EXPECT_EQ(
        bundledForm(AirStart::kId),
        json::parse(R"json({"id": "info.openrocket.core.simulation.extension.example.AirStart",
                            "type": "AirStart", "name": "Air-start (100 m, 50 m/s)",
                            "config": {}})json"));

    // A script as "Simulation scripting.ork" stores it: disabled, with its language and text.
    ScriptingExtension script;
    Config             config;
    config.put("enabled", false);
    config.put("language", "JavaScript");
    config.put("script", "// nothing\n");
    script.setConfig(config);
    EXPECT_EQ(goldenFormOf(script),
              json::parse(
                  R"json({"id": "info.openrocket.core.simulation.extension.impl.ScriptingExtension",
                    "type": "ScriptingExtension", "name": "JavaScript script",
                    "config": {"enabled": false, "language": "JavaScript",
                               "script": "// nothing\n"}})json"));
}

TEST(ExampleSimulationGoldenHarness, ASettingIsDescribedByItsType)
{
    Config config;
    config.put("flag", true);
    config.put("integer", 5);
    config.put("long", std::int64_t{1} << 40);
    config.put("double", 0.5);
    config.put("whole", 5.0);
    config.put("text", "CONTROL");
    config.put("nan", std::nan(""));
    config.put("infinity", -HUGE_VAL);
    config.put("decimal", BigDecimal::valueOf(7));
    config.put("list", Config::List{Config::Value{1}, Config::Value{"two"},
                                    Config::Value{Config::List{Config::Value{false}}}});
    const json form = goldenFormOf(config);
    EXPECT_EQ(form,
              json::parse(R"({"flag": true, "integer": 5, "long": 1099511627776, "double": 0.5,
                              "whole": 5.0, "text": "CONTROL", "nan": "NaN",
                              "infinity": "-Infinity", "decimal": 7.0,
                              "list": [1, "two", [false]]})"));
    // The types are kept: an Integer is an integer and a Double a floating-point number, also
    // when it is a whole number (the dumper prints "5" and "5.0").
    EXPECT_TRUE(form.at("integer").is_number_integer());
    EXPECT_TRUE(form.at("whole").is_number_float());
    EXPECT_EQ(goldenFormOf(Config{}), json::object());
}

/// The golden input and the maker of the Estes Alpha III, whose [C6-5] simulation is the
/// fifth of the harness's document.
constexpr std::string_view kAlphaInput      = "testrocket-estes-alpha-iii";
constexpr std::size_t      kAlphaSimulation = 4;

/// The document the harness builds for the test rocket of @p maker
/// (GoldenDumper.dumpTestRocket()): its rocket and one simulation per planned simulation, each
/// with the options of a fresh installation and its variant's change.
[[nodiscard]] std::shared_ptr<LoadedDocument> harnessDocument(const TestRocketMaker& maker)
{
    auto loaded      = std::make_shared<LoadedDocument>();
    loaded->document = OpenRocketDocumentFactory::createDocumentFromRocket(maker.make());
    Rocket&                    rocket = loaded->document->getRocket();
    const InstallationDefaults defaults;
    for (const PlannedSimulation& planned : plannedSimulations(rocket, maker.input))
    {
        const auto simulation = std::make_shared<Simulation>(loaded->document.get(), rocket);
        simulation->setFlightConfigurationId(
            rocket.getFlightConfigurationByIndex(planned.configuration, true).getId());
        simulation->setName(planned.name);
        simulation->getOptions().copyConditionsFrom(defaults.options);
        if (planned.variant != nullptr)
        {
            planned.variant->apply(simulation->getOptions());
        }
        loaded->document->addSimulation(simulation);
    }
    return loaded;
}

/// A compared run whose run is the perturbed run of @p run (for flightDifference()).
[[nodiscard]] GoldenRun perturbedRunOf(const GoldenRun& run)
{
    GoldenRun perturbed;
    perturbed.run = run.twin;
    return perturbed;
}

/// The maker of the Estes Alpha III, or null with a test failure.
[[nodiscard]] const TestRocketMaker* alphaMaker()
{
    const std::span<const TestRocketMaker> makers = testRocketMakers();
    const auto found = std::ranges::find(makers, kAlphaInput, &TestRocketMaker::input);
    if (found == makers.end())
    {
        ADD_FAILURE() << "no maker of " << kAlphaInput;
        return nullptr;
    }
    return &*found;
}

// The run of a simulation of a document is the harness's run of that simulation: the document
// the harness builds for a test rocket, run through runDocumentSimulation(), gives the flight
// that runSimulation() gives for the same simulation of the same rocket, value for value, and
// matches the golden files of the test rocket in everything but where its options are said to
// come from.
TEST(ExampleSimulationGoldenHarness, ASimulationOfADocumentIsRunAsTheHarnessRunsIt)
{
    const TestRocketMaker* maker = alphaMaker();
    ASSERT_NE(maker, nullptr);
    const GoldenInput* input = QtRocket::Test::inputOf(*maker);
    ASSERT_NE(input, nullptr);
    const GoldenSimulation&               simulation = input->simulations.at(kAlphaSimulation);
    const std::shared_ptr<LoadedDocument> document   = harnessDocument(*maker);
    const std::shared_ptr<LoadedDocument> twin       = harnessDocument(*maker);

    GoldenRun result;
    result.files = loadGoldenFiles(simulation);
    ASSERT_EQ(result.files.problem, "");
    result.golden = goldenCounts(result.files);
    result.run    = runDocumentSimulation(document, kAlphaSimulation, {});
    result.twin   = runDocumentSimulation(
        twin, kAlphaSimulation,
        {.stableTimeStep = {}, .perturbation = std::make_shared<LastBitListener>()});
    ASSERT_EQ(result.run.problem + result.twin.problem, "");

    // The run is of the document's own objects, and keeps the document.
    EXPECT_EQ(result.run.document, document);
    EXPECT_EQ(result.run.rocket.get(), &document->document->getRocket());
    EXPECT_EQ(result.run.simulation, document->document->getSimulation(kAlphaSimulation));
    EXPECT_EQ(result.run.optionsSource, "document");
    EXPECT_EQ(result.run.planned.index, kAlphaSimulation);
    EXPECT_EQ(result.run.planned.configuration, 4);
    EXPECT_EQ(result.run.planned.name, "[C6-5]");
    EXPECT_EQ(result.run.planned.variant, nullptr);
    EXPECT_EQ(result.run.status, "completed");

    // The same flight as the run of the test rocket's simulation.
    GoldenRun ofTheMaker;
    ofTheMaker.run = QtRocket::Test::runSimulation(*maker, kAlphaSimulation, {});
    ASSERT_EQ(ofTheMaker.run.problem, "");
    EXPECT_EQ(flightDifference(result, ofTheMaker), "");
    EXPECT_NE(flightDifference(result, perturbedRunOf(result)), "")
        << "the comparison of two flights finds no difference between a run and its perturbed run";
    EXPECT_EQ(result.run.harness, ofTheMaker.run.harness);
    EXPECT_EQ(result.run.jitterReplacements, ofTheMaker.run.jitterReplacements);

    // And the golden files of the test rocket, but for the source of the options.
    compareWithItsFiles(result, input->name, simulation, {});
    EXPECT_EQ(
        result.comparison.report,
        std::format("{}:\n  optionsSource: expected \"applicationDefaults\", got \"document\"\n",
                    result.context));
    EXPECT_EQ(result.comparison.compared + result.comparison.sensitive + result.comparison.excluded,
              result.golden);
    EXPECT_EQ(floorProblem(result), "");
}

TEST(ExampleSimulationGoldenHarness, ASimulationThatIsNotThereIsAProblem)
{
    const TestRocketMaker* maker = alphaMaker();
    ASSERT_NE(maker, nullptr);
    const std::shared_ptr<LoadedDocument> document = harnessDocument(*maker);
    ASSERT_EQ(document->document->getSimulationCount(), 9U);
    EXPECT_EQ(runDocumentSimulation(document, 9, {}).problem, "the document has 9 simulations");
    EXPECT_EQ(runDocumentSimulation(nullptr, 0, {}).problem, "no document");
    EXPECT_EQ(runDocumentSimulation(std::make_shared<LoadedDocument>(), 0, {}).problem,
              "no document");
}

// SimulationDumper.dump()'s index of a flight configuration: its place among the rocket's
// configurations, the default one first.
TEST(ExampleSimulationGoldenHarness, TheIndexOfAFlightConfigurationCountsTheDefaultFirst)
{
    const TestRocketMaker* maker = alphaMaker();
    ASSERT_NE(maker, nullptr);
    const std::unique_ptr<Rocket>            rocket = maker->make();
    const std::vector<FlightConfigurationId> ids    = rocket->getIds();
    ASSERT_EQ(ids.size(), 5U);
    EXPECT_EQ(configurationIndexOf(*rocket, FlightConfigurationId::defaultValueId()), 0);
    EXPECT_EQ(configurationIndexOf(*rocket, rocket->getEmptyConfiguration().getId()), 0);
    EXPECT_EQ(configurationIndexOf(*rocket, ids.front()), 1);
    EXPECT_EQ(configurationIndexOf(*rocket, ids.back()), 5);
    // An id the rocket has no configuration of: the dumper's loop runs to its end.
    EXPECT_EQ(configurationIndexOf(*rocket, FlightConfigurationId{}), 6);
}

}  // namespace
