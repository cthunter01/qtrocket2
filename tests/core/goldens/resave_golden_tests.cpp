// Golden tests of the re-saved test rockets: OpenRocket's save of each of the thirteen rockets
// of TestRockets.java (tests/data/goldens/testrocket-<name>/resave/rocket.ork, written by
// tools/openrocket-goldens: plain XML of file version 1.11), loaded by the .ork loader
// (GeneralRocketLoader) and compared with the goldens of the rocket that was saved.
//
// The rocket built in code is compared with those goldens by test_rockets_golden_tests.cpp
// (geometry.json), tests/core/mass/MassCalculatorTests.cpp (mass.json) and aero_golden_tests.cpp
// (aero.json). Here the same numbers are reached by another road: OpenRocket's saver wrote the
// Java rocket, and QtRocket's loader reads it. A setter of the loader that puts a value of one
// of these designs in the wrong place shows as a difference from the rocket of the makers.
//
// The load. The motors of the makers are synthetic (their digests are "digest A8 test" and so
// on): against the bundled motor database the loader would give the mounts real motors of the
// same designations, with other digests, masses and flights. So the motor database of these
// loads holds exactly the seven motors of TestRockets, added in the order A8, A10, B4, C6, D21,
// M1350, G77 (testRocketMotors()), and the loader finds each motor by its digest. Everything
// else is the environment of the other loaded designs (DesignFileEnvironment: OpenRocket's test
// preferences, its materials, the bundled extensions), without component presets: no test
// rocket names one. Every file is loaded in an environment of its own.
//
// Compared, for each of the thirteen files (ResaveGolden):
// - the load gives no warning;
// - the design, settled as the harness settles a design before it dumps it (see
//   AutomaticDimensions.h; no test rocket needs a pass), against geometry.json, mass.json and
//   aero.json with compareLoadedDesign() of GoldenDesign.h, as the example designs are compared:
//   every component in the order of the tree with its id (the file has the ids of the Java
//   rocket, which no rebuilt rocket has), every flight configuration with its id (those of
//   makeMultiStageEventTestRocket() and makeClusterPods() included, which the makers draw at
//   random and the comparison of the rebuilt rockets has to leave out), the masses, and the
//   aerodynamics;
// - the deployment, separation and ignition settings against the elements of the file itself
//   (compareSavedSettings() of GoldenSettings.h);
// - the simulations (ResaveGoldenSimulations): the fifty the harness gave the documents, one per
//   flight configuration and the three variants, each with its name, its flight configuration,
//   no extension, no flight data, and the options the harness flew it with, which are the
//   "options" of its golden simulation document, value for value (the re-save was written after
//   the harness had made the options reproducible). One option of the file is not among them:
//   every one of the fifty launches into the wind, and the launch rod direction of such options
//   is the direction of the wind (SimulationOptions::getLaunchRodDirection(), and so the
//   "launchRodDirection" of the golden options), not the direction stored for the rod. The
//   stored one is compared with the <launchroddirection> of the file itself, 90 degrees in
//   each. Their statuses are those of OpenRocket's own reload: NOT_SIMULATED for the 36 whose
//   configuration has a motor, CANT_RUN for the 14 without one. And the options are those of
//   the file whatever the preference store of the load holds: a second load of the thirteen,
//   with other preferences for everything the options of a new simulation take from the store,
//   gives the same fifty.
//
// THE FOURTEEN EXCEPTIONS. OpenRocket's own reload of these files does not give back the rocket
// of the makers in fourteen values of geometry.json, in three of the designs (kReloadChanges;
// measured with OpenRocket's loader against the same seven motors and the harness's dumpers:
// the probe "resaves" of tier 9d, ResaveProbe.java and out/reload-diff.txt). They are
// properties of the pair of OpenRocket's saver and loader, not of this port, and each is pinned
// with both numbers: the test changes a copy of the golden document to what OpenRocket's reload
// gives, after checking that the committed document holds the maker's number, and the
// comparison with the changed document has to report nothing. There are three causes:
// - A fin tab without a height is not saved (FinSetSaver writes a tab only when it has a height
//   and a length). The fin sets of Big Blue and of the booster of the multi-stage event rocket
//   were given their length when the tab already stood at the front of the fin, and neither
//   FreeformFinSet.setPoints() nor the constructor of TrapezoidFinSet moves a tab; read as an
//   offset from the middle of the fin, that position is -0.05 m and +0.005 m. The loader
//   recomputes the position from the offset it has, the default 0 (FinSetPointHandler calls
//   updateTabPosition() and the <rootchord> setter is TrapezoidFinSet.setRootChord(), which
//   does too), so the reloaded tab sits in the middle: five values each (the offset and the x
//   of the four tab points).
// - The saver writes the points of a freeform fin set as getFinPoints() gives them, whose last
//   point is the last root point, computed in the frame of the body: 0.15000000000000002 for
//   the 0.15 the maker gave Big Blue's fins. The reloaded fin set is that long, and its
//   rotational inertia follows by one unit in the last place: three values, each within the
//   tolerance of the comparison either way (as are six values of mass.json that follow the
//   inertia; they are no exceptions: mass.json is compared as it is).
// - The pod body of makeEstesAlphaIIIWithPods() is a BodyTube(length, 0): the constructor
//   leaves the default wall of 2 mm on a tube of radius 0. The file has both, and every setter
//   keeps a wall within its radius, so the loaded tube has no wall: one value.
// ResaveGoldenExceptions shows that the exceptions are needed and are all there are: against
// the committed documents the comparison reports the eleven values beyond its tolerance, one
// line each, and nothing else.
//
// Tolerances (plan section 6.4), as for the rockets of the makers: geometry, mass and
// aerodynamic coefficients relative 1e-9; positions, CGs and CPs absolute 1e-9 m; ids, names,
// the numbers of the file and the options of the simulations exactly, but for the one option
// that is computed with mathematical functions: the launch rod direction of the launch into a
// multi-level wind, which is the direction of that wind at the launch site (relative 1e-12).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <pugixml.hpp>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/AutomaticDimensions.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenDesign.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenMismatches.h"
#include "goldens/GoldenResaves.h"
#include "goldens/GoldenSettings.h"
#include "rocket/TestRockets.h"

namespace
{

using nlohmann::json;
using QtRocket::Preferences;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::Test::addResaveRun;
using QtRocket::Test::changeToTheReload;
using QtRocket::Test::ComparedResaves;
using QtRocket::Test::ComparedSimulations;
using QtRocket::Test::compareLoadedGeometry;
using QtRocket::Test::compareLoadedResave;
using QtRocket::Test::compareOptions;
using QtRocket::Test::compareSimulations;
using QtRocket::Test::DesignComparison;
using QtRocket::Test::designCountsOfTheTestRockets;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::DesignGoldens;
using QtRocket::Test::goldenCounts;
using QtRocket::Test::GoldenInput;
using QtRocket::Test::GoldenManifest;
using QtRocket::Test::kBigBlue;
using QtRocket::Test::kMultiStage;
using QtRocket::Test::kPods;
using QtRocket::Test::kReloadChanges;
using QtRocket::Test::loadDesignGoldens;
using QtRocket::Test::LoadedResave;
using QtRocket::Test::loadGoldenJson;
using QtRocket::Test::loadGoldenManifest;
using QtRocket::Test::loadGoldenResave;
using QtRocket::Test::loadResave;
using QtRocket::Test::PreparePreferences;
using QtRocket::Test::ReloadChange;
using QtRocket::Test::ReloadChanges;
using QtRocket::Test::ResaveRun;
using QtRocket::Test::settingsCountsOfTheTestRockets;
using QtRocket::Test::settleAutomaticDimensions;
using QtRocket::Test::testRocketInputs;

using Presets = DesignFileEnvironment::Presets;

// ============================================================================ the comparison

/// Loads the re-save of @p input, settles the design and compares it with the design documents
/// @p goldens and with the settings of the file (compareLoadedResave()).
[[nodiscard]] ResaveRun compareResave(const GoldenInput& input, const DesignGoldens& goldens)
{
    const Result<LoadedResave> resave = loadResave(input);
    const auto                 file   = loadGoldenResave(input.name);
    if (!resave || !file)
    {
        ResaveRun run;
        run.golden  = goldenCounts(goldens);
        run.problem = !resave ? resave.error().toString() : file.error().toString();
        return run;
    }
    return compareLoadedResave(*resave, goldens, **file);
}

/// Loads the re-save of every test rocket of the manifest and compares it with the design
/// documents of its input, changed to what OpenRocket's own reload gives (kReloadChanges).
[[nodiscard]] ComparedResaves compareResaves()
{
    ComparedResaves compared;
    for (const GoldenInput& input : testRocketInputs(compared.problems))
    {
        compared.inputs++;
        Result<DesignGoldens> goldens = loadDesignGoldens(input.name);
        if (!goldens)
        {
            compared.problems += std::format("{}: {}\n", input.name, goldens.error().toString());
            continue;
        }
        changeToTheReload(input.name, goldens->geometry, compared.changes);
        addResaveRun(compared, input.name, compareResave(input, *goldens));
    }
    return compared;
}

// The thirteen re-saves load without a warning, and each reloaded design is the rocket of its
// goldens but for the fourteen values that OpenRocket's own reload changes: 151 components with
// the ids of the files, 47 configurations with theirs, 47 mass calculations, 1175 points of the
// aerodynamics, and the settings elements of the files. Everything the documents hold is
// compared for every input (its line says so when not).
TEST(ResaveGolden, TheReloadedTestRocketsAreTheRocketsOfTheGoldens)
{
    const ComparedResaves compared = compareResaves();
    EXPECT_EQ(compared.problems, "");
    EXPECT_EQ(compared.inputs, 13);
    EXPECT_EQ(compared.loadWarnings, 0);
    EXPECT_EQ(compared.settlingPasses, 0) << "no test rocket has a stale automatic dimension";
    EXPECT_EQ(compared.changes.problems, "");
    EXPECT_EQ(compared.changes.values, 14);
    EXPECT_EQ(compared.compared, compared.golden) << "what was compared, and what the files hold";
    EXPECT_EQ(compared.golden, designCountsOfTheTestRockets());
    EXPECT_EQ(compared.settings, compared.saved) << "the settings compared, and those of the files";
    EXPECT_EQ(compared.saved, settingsCountsOfTheTestRockets());
}

// =============================================================================== simulations

/// Loads the re-save of every test rocket (with the preference store that @p prepare made of
/// OpenRocket's test preferences, when given) and compares its simulations, as loaded, with the
/// golden simulation documents of its input and with the file.
[[nodiscard]] ComparedSimulations compareSimulationsOfTheResaves(
    PreparePreferences prepare = nullptr)
{
    ComparedSimulations compared;
    for (const GoldenInput& input : testRocketInputs(compared.problems))
    {
        compared.inputs++;
        const Result<LoadedResave> resave   = loadResave(input, prepare);
        const Result<const json*>  geometry = QtRocket::Test::goldenGeometry(input.name);
        const auto                 file     = loadGoldenResave(input.name);
        if (!resave || !geometry || !file)
        {
            compared.problems +=
                std::format("{}: no load, no geometry.json or no re-save to read\n", input.name);
            continue;
        }
        compareSimulations(compared, input, *resave, **geometry, **file);
    }
    return compared;
}

// The documents of the thirteen re-saves hold the fifty simulations the harness flew: each
// with the name, the flight configuration and the options of its golden simulation document
// (1808 values: 32 of every simulation and 4 of each of its wind levels, one level but for the
// three of the flight into a multi-level wind; among the fifty are the RK6 stepper, the WGS84
// geodetic computation and that wind). OpenRocket's own reload leaves 36 of them NOT_SIMULATED
// and the 14 of a configuration without motors CANT_RUN, and so does this one. All fifty launch
// into the wind, so the launch rod direction among those values is the wind's: the direction
// stored for the rod is compared with the fifty <launchroddirection> of the files.
TEST(ResaveGoldenSimulations, TheSimulationsAreThoseTheHarnessSaved)
{
    const ComparedSimulations compared = compareSimulationsOfTheResaves();
    EXPECT_EQ(compared.problems, "");
    EXPECT_EQ(compared.inputs, 13);
    EXPECT_EQ(compared.simulations, 50);
    EXPECT_EQ(compared.notSimulated, 36);
    EXPECT_EQ(compared.cannotRun, 14);
    EXPECT_EQ(compared.withoutMotors, 14);
    EXPECT_EQ(compared.optionValues, 1808);
    EXPECT_EQ(compared.intoTheWind, 50);
    EXPECT_EQ(compared.rodDirections, 50);
}

/// Gives @p preferences another value for every option that a new simulation takes from the
/// preference store (SimulationOptions(Preferences&)) and that a re-save stores, each unlike
/// the value of any of the fifty simulations: the launch rod (1 m, upright, stored direction 90
/// degrees in the files), the launch site (28.61 N, 80.6 W, sea level), the time step (0.05 s)
/// and the maximum time (1200 s), the gravity model (WGS) and the wind keys, from which the
/// multi-level wind model gets its first level (2 m/s from the east, calm). OpenRocket's test
/// preferences, which the other loads have, equal the files in the rod angle, the altitude, the
/// maximum time and the gravity model: there a value of the file that is not applied does not
/// show. Left as they are: the flags (the files launch into the wind and use the ISA model, the
/// test preferences do neither) and what a file with the ISA model and the WGS gravity does not
/// store (the temperature, the pressure, the humidity, the constant gravity).
void storeOtherPreferences(Preferences& preferences)
{
    namespace Keys = QtRocket::PreferenceKeys;
    preferences.putDouble(Keys::kLaunchRodLength, 3.0);
    preferences.putDouble(Keys::kLaunchRodAngle, 0.2);
    preferences.putDouble(Keys::kLaunchRodDirection, 1.0);
    preferences.putDouble(Keys::kWindAverage, 5.0);
    preferences.putDouble(Keys::kWindTurbulence, 0.3);
    preferences.putDouble(Keys::kWindDirection, 1.0);
    preferences.putDouble(Keys::kLaunchAltitude, 250.0);
    preferences.putDouble(Keys::kLaunchLatitude, 10.0);
    preferences.putDouble(Keys::kLaunchLongitude, 20.0);
    preferences.putDouble(Keys::kSimulationTimeStep, 0.02);
    preferences.putDouble(Keys::kSimulationMaxTime, 600.0);
    preferences.setGravityModelName("CONSTANT");
}

// The options of a loaded simulation are those of its file, not those of the installation: the
// same fifty simulations come out of a load whose preference store has other values for
// everything the options of a new simulation start from. (What starts from a constant and has
// that constant in every file does not show here either: the four speeds of the recovery
// warnings and the direction and the turbulence of the average wind. Each element of
// <conditions> is held by tests/core/file/openrocket/SimulationConditionsHandlerTests.cpp.)
TEST(ResaveGoldenSimulations, TheOptionsAreThoseOfTheFilesWhateverThePreferencesHold)
{
    const ComparedSimulations compared = compareSimulationsOfTheResaves(storeOtherPreferences);
    EXPECT_EQ(compared.problems, "");
    EXPECT_EQ(compared.inputs, 13);
    EXPECT_EQ(compared.simulations, 50);
    EXPECT_EQ(compared.notSimulated, 36);
    EXPECT_EQ(compared.cannotRun, 14);
    EXPECT_EQ(compared.optionValues, 1808);
    EXPECT_EQ(compared.rodDirections, 50);
}

// The comparison of the options is not vacuous: a value of the golden options changed in a copy
// is reported, in the line that names it, and so is an option that only one side has.

/// The golden input whose simulations the changes are made to: the Alpha III has the three
/// variants among its nine simulations (6: the RK6 stepper, 7: the WGS84 geodetic computation,
/// 8: the multi-level wind).
constexpr std::string_view kAlphaIII = "testrocket-estes-alpha-iii";

/// A change to the golden options of a simulation of the Alpha III that the comparison of the
/// options has to report (or, with no line, has to let pass).
struct OptionsMutation
{
    std::string_view name;        ///< what is changed
    std::size_t      simulation;  ///< the index of the simulation
    json             change;      ///< the operation of a JSON patch (RFC 6902) on its "options"
    std::string_view line;        ///< what the first line of the report starts with
    int              lines;       ///< the number of lines of the report
};

/// What GoogleTest prints for the mutation of a test that failed: its name.
// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const OptionsMutation& mutation, std::ostream* out)
{
    *out << mutation.name;
}

[[nodiscard]] json replaced(std::string_view path, json value)
{
    return json{{"op", "replace"}, {"path", path}, {"value", std::move(value)}};
}

[[nodiscard]] json added(std::string_view path, json value)
{
    return json{{"op", "add"}, {"path", path}, {"value", std::move(value)}};
}

[[nodiscard]] json removed(std::string_view path)
{
    return json{{"op", "remove"}, {"path", path}};
}

/// A wind level of the golden options.
[[nodiscard]] json aWindLevel()
{
    return json{
        {"altitude", 100.0}, {"speed", 4.0}, {"direction", 2.2}, {"standardDeviation", 0.0}};
}

/// The changes. The launch rod direction of the options is pi / 2 in every simulation of the
/// Alpha III, the direction of its wind; the number next to it is one unit in the last place
/// away, 1.5707963268 three parts in 10^11.
[[nodiscard]] std::vector<OptionsMutation> optionsMutations()
{
    return {
        {.name       = "TheTimeStep",
         .simulation = 4,
         .change     = replaced("/timeStep", 0.06),
         .line       = "  options/timeStep: expected 0.06, got 0.05",
         .lines      = 1},
        {.name       = "TheRandomSeed",
         .simulation = 4,
         .change     = replaced("/randomSeed", 1),
         .line       = "  options/randomSeed: expected 1, got 0",
         .lines      = 1},
        {.name       = "AFlag",
         .simulation = 4,
         .change     = replaced("/launchIntoWind", false),
         .line       = "  options/launchIntoWind: expected false, got true",
         .lines      = 1},
        {.name       = "TheStepperMethod",
         .simulation = 6,
         .change     = replaced("/stepperMethod", "RK4"),
         .line       = R"(  options/stepperMethod: expected "RK4", got "RK6")",
         .lines      = 1},
        {.name       = "TheGeodeticComputation",
         .simulation = 7,
         .change     = replaced("/geodeticComputation", "SPHERICAL"),
         .line       = R"(  options/geodeticComputation: expected "SPHERICAL", got "WGS84")",
         .lines      = 1},
        {.name       = "TheWindModelType",
         .simulation = 8,
         .change     = replaced("/windModelType", "AVERAGE"),
         .line       = R"(  options/windModelType: expected "AVERAGE", got "MULTI_LEVEL")",
         .lines      = 1},
        {.name       = "TheSpeedOfAWindLevel",
         .simulation = 8,
         .change     = replaced("/multiLevelWind/levels/1/speed", 5.0),
         .line       = "  options/multiLevelWind/levels/1/speed: expected 5.0, got 4.0",
         .lines      = 1},
        {.name       = "TheAltitudeReference",
         .simulation = 8,
         .change     = replaced("/multiLevelWind/altitudeReference", "AGL"),
         .line       = R"(  options/multiLevelWind/altitudeReference: expected "AGL", got "MSL")",
         .lines      = 1},
        {.name       = "AWindLevelMore",
         .simulation = 4,
         .change     = added("/multiLevelWind/levels/-", aWindLevel()),
         .line       = "  options/multiLevelWind/levels/1/altitude: not compared",
         .lines      = 4},
        {.name       = "AWindLevelLess",
         .simulation = 8,
         .change     = removed("/multiLevelWind/levels/2"),
         .line       = "  options/multiLevelWind/levels/2/altitude: not in the golden options",
         .lines      = 4},
        {.name       = "AnOptionNothingCompares",
         .simulation = 0,
         .change     = added("/launchRailLength", 1.0),
         .line       = "  options/launchRailLength: not compared",
         .lines      = 1},
        {.name       = "AnOptionLess",
         .simulation = 0,
         .change     = removed("/randomSeedFixed"),
         .line       = "  options/randomSeedFixed: not in the golden options",
         .lines      = 1},
        // The direction into the average wind is a number of the file, compared exactly ...
        {.name       = "TheDirectionIntoTheAverageWindByOneUnitInTheLastPlace",
         .simulation = 4,
         .change     = replaced("/launchRodDirection", 1.5707963267948968),
         .line =
             "  options/launchRodDirection: expected 1.5707963267948968, got 1.5707963267948966",
         .lines = 1},
        // ... and the computed one, the direction into a multi-level wind, within its tolerance.
        {.name       = "TheComputedDirectionByOneUnitInTheLastPlace",
         .simulation = 8,
         .change     = replaced("/launchRodDirection", 1.5707963267948968),
         .line       = "",
         .lines      = 0},
        {.name       = "TheComputedDirectionBeyondItsTolerance",
         .simulation = 8,
         .change     = replaced("/launchRodDirection", 1.5707963268),
         .line       = "  options/launchRodDirection: expected 1.5707963268, got 1.57079632679",
         .lines      = 1},
    };
}

/// The report of comparing the options of the simulation of index @p simulation of the reloaded
/// Alpha III with its golden options after @p change; a line that starts with "no " when there
/// is nothing to compare.
[[nodiscard]] std::string optionsReportAfter(std::size_t simulation, const json& change)
{
    const Result<GoldenManifest> manifest = loadGoldenManifest();
    const GoldenInput* const     input    = manifest ? manifest->find(kAlphaIII) : nullptr;
    if (input == nullptr || simulation >= input->simulations.size())
    {
        return "no such golden simulation";
    }
    const Result<json>         golden = loadGoldenJson(input->simulations.at(simulation).json);
    const Result<LoadedResave> resave = loadResave(*input);
    if (!golden || !resave || simulation >= resave->loaded.document->getSimulationCount())
    {
        return "no golden document or no such simulation in the re-save";
    }
    const json options = golden->at("options").patch(json::array({change}));
    return compareOptions(options, resave->loaded.document->getSimulation(simulation)->getOptions())
        .report;
}

/// One changed golden option.
class ResaveGoldenOptionsMutation : public ::testing::TestWithParam<OptionsMutation>
{ };

TEST_P(ResaveGoldenOptionsMutation, IsReportedInItsLine)
{
    const OptionsMutation& mutation = GetParam();
    const std::string      report   = optionsReportAfter(mutation.simulation, mutation.change);
    EXPECT_TRUE(report.starts_with(mutation.line)) << report;
    EXPECT_EQ(std::ranges::count(report, '\n'), mutation.lines) << report;
}

/// The test name of @p info's mutation: what it changes.
[[nodiscard]] std::string optionsMutationTestName(
    const ::testing::TestParamInfo<OptionsMutation>& info)
{
    return std::string{info.param.name};
}

INSTANTIATE_TEST_SUITE_P(Changes, ResaveGoldenOptionsMutation,
                         ::testing::ValuesIn(optionsMutations()), optionsMutationTestName);

// ===================================================================== the exceptions, needed

/// The start of the line with which the comparison of geometry.json reports @p change when the
/// document is the committed one: the name of the value and the number of the maker's rocket,
/// up to the number found.
[[nodiscard]] std::string reportedLine(const ReloadChange& change)
{
    return std::format("\n  {}: expected {}, got ", change.field, change.built);
}

/// What is wrong with @p report, the comparison of the reloaded design of @p input with its
/// committed geometry.json: it has to hold one line for each value of kReloadChanges of the
/// input that is beyond the tolerance, under the one line that names their component (the
/// values of a design are all of one component), and nothing else. Adds the number of those
/// values to @p reported.
[[nodiscard]] std::string reportProblems(std::string_view input, const std::string& report,
                                         int& reported)
{
    std::string problems;
    int         expected = 0;
    for (const ReloadChange& change : kReloadChanges)
    {
        if (change.input != input || !change.beyondTolerance)
        {
            continue;
        }
        expected++;
        if (!report.contains(reportedLine(change)))
        {
            problems += std::format("{} {} {}: not reported\n", input, change.path, change.field);
        }
    }
    reported += expected;
    if (std::ranges::count(report, '\n') != (expected == 0 ? 0 : expected + 1))
    {
        problems += std::format("{}: the report is not that of {} values of one component:\n{}",
                                input, expected, report);
    }
    return problems;
}

/// What comparing the reloaded designs with the committed geometry.json documents, as they
/// are, gave.
struct WithoutTheChanges
{
    int         inputs{0};
    int         reported{0};  ///< the values of kReloadChanges the comparison has to report
    std::string problems;     ///< a value that is not reported, a line beyond the values
};

/// Compares the reloaded and settled design of every test rocket with its committed
/// geometry.json and checks the report against kReloadChanges (reportProblems()).
[[nodiscard]] WithoutTheChanges compareWithTheCommittedGeometry()
{
    WithoutTheChanges result;
    for (const GoldenInput& input : testRocketInputs(result.problems))
    {
        result.inputs++;
        const Result<const json*>  geometry = QtRocket::Test::goldenGeometry(input.name);
        const Result<LoadedResave> resave   = loadResave(input);
        if (!geometry || !resave)
        {
            result.problems += std::format("{}: no geometry.json or no re-save\n", input.name);
            continue;
        }
        Rocket& rocket = resave->loaded.document->getRocket();
        if (!settleAutomaticDimensions(rocket).has_value())
        {
            result.problems += std::format("{}: does not settle\n", input.name);
            continue;
        }
        const DesignComparison compared = compareLoadedGeometry(
            rocket, resave->loaded.warnings, **geometry, resave->environment->preferences());
        result.problems += reportProblems(input.name, compared.report, result.reported);
    }
    return result;
}

// The exceptions are needed, and they are all there are: against the committed geometry.json
// documents the reloaded designs differ in the eleven values of the table that are beyond the
// tolerance of the comparison, each in its line with the number of the maker's rocket, and in
// nothing else (ten of the thirteen designs in nothing at all).
TEST(ResaveGoldenExceptions, TheCommittedGeometryDiffersInTheElevenValuesBeyondTheTolerance)
{
    const WithoutTheChanges result = compareWithTheCommittedGeometry();
    EXPECT_EQ(result.problems, "");
    EXPECT_EQ(result.inputs, 13);
    EXPECT_EQ(result.reported, 11);
}

/// What is wrong with kReloadChanges as a table: a value whose two numbers are the same, or
/// whose beyondTolerance is not what the relative tolerance of the comparison says of them.
/// Counts the values by design in @p bigBlue, @p pods and @p multiStage.
[[nodiscard]] std::string tableProblems(int& bigBlue, int& pods, int& multiStage)
{
    std::string problems;
    for (const ReloadChange& change : kReloadChanges)
    {
        bigBlue += change.input == kBigBlue ? 1 : 0;
        pods += change.input == kPods ? 1 : 0;
        multiStage += change.input == kMultiStage ? 1 : 0;
        const double scale = std::max(std::abs(change.built), std::abs(change.reloaded));
        const bool   beyond =
            std::abs(change.built - change.reloaded) > QtRocket::Test::kGoldenRelative * scale;
        if (change.built == change.reloaded || beyond != change.beyondTolerance)
        {
            problems += std::format("{} {} {}\n", change.input, change.path, change.value);
        }
    }
    return problems;
}

// The table: eight values of Big Blue's fins, one of the pod body, five of the booster's fins;
// the three that are within the tolerance are the length of Big Blue's fins and the two
// inertias that follow it, one unit in the last place each.
TEST(ResaveGoldenExceptions, TheTableHoldsFourteenValuesOfThreeDesigns)
{
    int bigBlue    = 0;
    int pods       = 0;
    int multiStage = 0;
    EXPECT_EQ(tableProblems(bigBlue, pods, multiStage), "");
    EXPECT_EQ(bigBlue, 8);
    EXPECT_EQ(pods, 1);
    EXPECT_EQ(multiStage, 5);
    EXPECT_EQ(kReloadChanges.size(), 14U);
}

/// The length of the component at @p path of the reloaded design of the input @p name; NaN
/// when there is no such design or component.
[[nodiscard]] double reloadedLength(std::string_view name, std::string_view path)
{
    const Result<GoldenManifest> manifest = loadGoldenManifest();
    const GoldenInput* const     input    = manifest ? manifest->find(name) : nullptr;
    if (input == nullptr)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const Result<LoadedResave> resave = loadResave(*input);
    if (!resave)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const QtRocket::RocketComponent* const component =
        QtRocket::Test::componentAtGoldenPath(resave->loaded.document->getRocket(), path);
    return component != nullptr ? component->getLength() : std::numeric_limits<double>::quiet_NaN();
}

// One of the three values within the tolerance can be told apart all the same: the length of
// Big Blue's fins is the x of their last point as the file has it, a number that was read and
// not computed, and it is the saver's 0.15000000000000002, not the maker's 0.15.
TEST(ResaveGoldenExceptions, TheFinsOfBigBlueAreAsLongAsTheFileSays)
{
    EXPECT_EQ(reloadedLength(kBigBlue, "/0/1/0"), 0.15000000000000002);
    EXPECT_NE(0.15000000000000002, 0.15);
}

// ============================================================== the ids of the configurations

/// An id that no flight configuration has.
constexpr const char* kAnotherId = "00000000-0000-4000-8000-000000000000";

/// What comparing the configuration ids of the designs whose makers draw one at random gave.
struct RandomIds
{
    int makers{0};  ///< the makers that draw the id of a configuration at random
    /// A design that is not that of its geometry.json, and a changed id that the comparison
    /// does not report.
    std::string problems;
};

/// The report of the comparison of the reloaded and settled design of @p input with @p geometry.
[[nodiscard]] std::string geometryReport(const GoldenInput& input, const json& geometry)
{
    const Result<LoadedResave> resave = loadResave(input);
    if (!resave)
    {
        return resave.error().toString();
    }
    Rocket& rocket = resave->loaded.document->getRocket();
    if (!settleAutomaticDimensions(rocket).has_value())
    {
        return "the design does not settle";
    }
    return compareLoadedGeometry(rocket, resave->loaded.warnings, geometry,
                                 resave->environment->preferences())
        .report;
}

/// For every maker that draws the id of its flight configuration at random: the reloaded design
/// has the ids of geometry.json (the report is empty), and it is the ids that are compared (the
/// id of the last configuration replaced by another one is reported).
[[nodiscard]] RandomIds compareTheRandomIds()
{
    RandomIds                    result;
    const Result<GoldenManifest> manifest = loadGoldenManifest();
    for (const QtRocket::Test::TestRocketMaker& maker : QtRocket::Test::testRocketMakers())
    {
        if (!maker.randomConfigurationId)
        {
            continue;
        }
        result.makers++;
        const GoldenInput* const input   = manifest ? manifest->find(maker.input) : nullptr;
        Result<DesignGoldens>    goldens = loadDesignGoldens(maker.input);
        if (input == nullptr || !goldens)
        {
            result.problems += std::format("{}: no goldens\n", maker.input);
            continue;
        }
        ReloadChanges changes;
        changeToTheReload(maker.input, goldens->geometry, changes);
        result.problems += geometryReport(*input, goldens->geometry);

        // (By index: GCC's -Wnull-dereference sees a null in json::back() here.)
        json& configurations = goldens->geometry.at("configurations");
        configurations.at(configurations.size() - 1).at("id") = kAnotherId;
        const std::string report = geometryReport(*input, goldens->geometry);
        if (!report.contains(std::format(R"(id: expected "{}", got ")", kAnotherId)))
        {
            result.problems += std::format("{}: another configuration id is not reported:\n{}",
                                           maker.input, report);
        }
    }
    return result;
}

// makeMultiStageEventTestRocket() and makeClusterPods() give their flight configuration a new
// random id, which the comparison of the rebuilt rockets leaves out. The file has the id
// OpenRocket drew, the golden documents have it too, and here it is compared.
TEST(ResaveGoldenConfigurationIds, TheIdsTheMakersDrawAtRandomAreThoseOfTheFiles)
{
    const RandomIds result = compareTheRandomIds();
    EXPECT_EQ(result.problems, "");
    EXPECT_EQ(result.makers, 2);
}

}  // namespace
