// Aerodynamic golden tests: the thirteen rockets of tests/core/rocket/TestRockets.h against what
// OpenRocket's BarrowmanCalculator computes for the Java rockets, in
// tests/data/goldens/testrocket-<name>/aero.json (tools/openrocket-goldens, AeroDumper.java; the
// format is in that tool's README.md).
//
// The calls are the dumper's, in the dumper's order, because the calculator caches across
// calls. Per configuration, selected in the rocket as the dumper selects it: one new
// BarrowmanCalculator; checkGeometry() on the rocket; the structure CG (the pitch centre of point
// 23); per point getCP(), getAerodynamicForces() and getForceAnalysis() with one warning set;
// then getWorstCP() per Mach number, each with new conditions.
//
// Compared (AeroGolden, one test per rocket):
// - per file: the atmosphere of the default flight conditions, the stall angle and the number of
//   configurations (the rocket's, the default one included);
// - per configuration: its header (index, id, name, whether it is the default), the reference
//   length and area, and the results. A configuration whose "sameResultsAs" names an earlier one
//   (it differs only in its motors) is calculated all the same and compared with the results it
//   refers to (aeroResults() of GoldenData.h);
// - the geometry warnings;
// - the 25 points. The flight conditions are set from the golden "conditions" (the reference
//   length and area, the Mach number, the angle of attack, theta, the three rates, the pitch
//   centre and, at point 24, the nozzle exit areas per assembly path), read back, and the derived
//   velocity and beta compared. The inputs are not taken on trust either: the Mach number, the
//   angle of attack, theta and the rates have to be those of the dumper's table of points
//   (AeroDumper.points()), the reference length and area those new FlightConditions take from
//   the configuration (the dumper sets neither), the pitch centre the nose tip or, at point 23,
//   the structure CG, and the nozzle exit areas none or, at point 24, those of the motor mounts,
//   each derived here as the dumper derives it. Then the CP, every field of "forces", every
//   entry of "components" by its path with every field (an entry that only one side has is
//   reported) and the warnings of the point;
// - the worst CP per Mach number (the dumper's Mach numbers): the CP, and the theta found as
//   compareWorstTheta() describes: it is one of the 360 directions getWorstCP() tries, the CP at
//   it is the worst CP (found, and so the golden one), and the golden theta is an equally bad
//   direction for QtRocket.
//   That is the one value of the files that cannot be compared for equality;
//   AeroGoldenWorstTheta holds the strict comparison, disabled, and the tests of the check.
// A warning is compared by its class, priority, description, text, sources (as paths) and
// parameter, and the warnings of a set in their order (compareWarnings() of GoldenWarnings.h,
// which the simulation goldens share). A golden field that nothing compares is reported, in
// every object of the document (noteUncomparedKeys()). The file's "schema", "schemaVersion" and
// "input" are checked by goldens_schema_tests.cpp.
//
// Nothing is skipped silently: the numbers of configurations, points, force-analysis entries,
// worst CPs and warnings compared have to be those of each golden file, and AeroGoldenCoverage
// sums them over the thirteen inputs of the manifest. Nor is the comparison vacuous:
// AeroGoldenMutation changes one golden value at a time in a copy of a document and expects the
// one line that reports it; AeroGoldenWorstTheta and AeroGoldenWarnings do the same for what no
// golden file exercises (a wrong theta from getWorstCP(), a warning with a parameter).
//
// Tolerances (plan section 6.4): coefficients relative 1e-9 (exact for a golden 0, as
// GoldenMismatches compares it), CP positions absolute 1e-9 m. No value needs the plan's wider
// relative 1e-7 for table-interpolated aerodynamics: on Linux the largest difference is below
// 1e-15, relative and absolute.
//
// The comparison itself (compareAero() and what it is made of) is in GoldenDesign.h, for the tests
// of other designs to share.
//
// Not compared yet: the sixteen example-* inputs. Their aero.json files are in
// tests/data/goldens, but the designs are .ork files, which need the .ork loader of the file
// tier. Their tests are to load the design and hand it to compareAero(). (Their goldens
// describe the design after OpenRocket's automatic dimensions have settled; see "Settled
// automatic dimensions" in the README of tools/openrocket-goldens.)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numbers>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/aero/BarrowmanCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenDesign.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenMismatches.h"
#include "goldens/GoldenWarnings.h"
#include "rocket/TestRockets.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using nlohmann::json;
using QtRocket::BarrowmanCalculator;
using QtRocket::Coordinate;
using QtRocket::FlightConditions;
using QtRocket::FlightConfiguration;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::Test::AeroComparison;
using QtRocket::Test::AeroCounts;
using QtRocket::Test::AeroSubject;
using QtRocket::Test::compareAero;
using QtRocket::Test::compareWarning;
using QtRocket::Test::compareWorstTheta;
using QtRocket::Test::goldenCounts;
using QtRocket::Test::goldenPathOf;
using QtRocket::Test::goldenValue;
using QtRocket::Test::isWorstCpDirection;
using QtRocket::Test::kThetaBeforeWorstCp;
using QtRocket::Test::parameterOf;
using QtRocket::Test::TestRocketMaker;
using QtRocket::Test::testRocketMakers;
using QtRocket::Test::toText;

/// The comparison collector of the golden tests.
using Mismatches = QtRocket::Test::GoldenMismatches;

// ============================================================================== the goldens

/// The aero.json of the golden input @p name, found through manifest.json.
[[nodiscard]] QtRocket::Result<json> loadGoldenAero(std::string_view name)
{
    const QtRocket::Result<QtRocket::Test::GoldenManifest> manifest =
        QtRocket::Test::loadGoldenManifest();
    if (!manifest)
    {
        return std::unexpected(manifest.error());
    }
    const QtRocket::Test::GoldenInput* input = manifest->find(name);
    if (input == nullptr)
    {
        return QtRocket::fail(QtRocket::ErrorCode::NOT_FOUND,
                              std::format("no golden input named {}", name));
    }
    return QtRocket::Test::loadGoldenJson(input->aero);
}

/// A test rocket compared with its golden aero.json: what the file holds, and what the
/// comparison compared and found.
struct GoldenRun
{
    std::string    problem;  ///< why there is no comparison (the file could not be read), else ""
    AeroCounts     golden;
    AeroComparison comparison;
};

/// Builds the rocket of @p maker and compares it with its aero.json.
[[nodiscard]] GoldenRun runGolden(const TestRocketMaker& maker)
{
    GoldenRun                    run;
    const QtRocket::Result<json> aero = loadGoldenAero(maker.input);
    if (!aero)
    {
        run.problem = aero.error().message;
        return run;
    }
    const std::unique_ptr<Rocket>       rocket = maker.make();
    const QtRocket::InMemoryPreferences preferences;  // the golden names are an empty store's
    run.golden     = goldenCounts(*aero);
    run.comparison = compareAero(*rocket, *aero, maker.randomConfigurationId, preferences);
    return run;
}

/// runGolden() of @p maker, run once per test process and kept (std::map keeps the entries where
/// they are): the per-rocket test, the coverage test and the worst-theta test read the same
/// comparison (a getWorstCP() is 360 CPs, and there are 329 of them). The result depends on the
/// maker alone, so the order of the tests does not matter.
[[nodiscard]] const GoldenRun& goldenRun(const TestRocketMaker& maker)
{
    static std::mutex                                    s_mutex;
    static std::map<std::string, GoldenRun, std::less<>> s_runs;
    const std::scoped_lock                               lock{s_mutex};
    auto                                                 cached = s_runs.find(maker.input);
    if (cached == s_runs.end())
    {
        cached = s_runs.emplace(std::string{maker.input}, runGolden(maker)).first;
    }
    return cached->second;
}

// ===================================================================================== tests

/// One test rocket of TestRockets.h against its golden aerodynamics.
class AeroGolden : public ::testing::TestWithParam<TestRocketMaker>
{ };

TEST_P(AeroGolden, EveryConfigurationPointAndWorstCP)
{
    const GoldenRun& run = goldenRun(GetParam());
    ASSERT_EQ(run.problem, "");
    EXPECT_EQ(run.comparison.report, "");
    // Everything the file holds was compared: no configuration, point, force-analysis entry,
    // worst CP or warning skipped.
    EXPECT_EQ(run.comparison.compared, run.golden) << "what was compared, and what the file holds";
    EXPECT_GT(run.golden.points, 0);
}

/// The test name of @p info's maker: its golden input with '-' as '_'.
[[nodiscard]] std::string makerTestName(const ::testing::TestParamInfo<TestRocketMaker>& info)
{
    std::string name{info.param.input};
    std::ranges::replace(name, '-', '_');
    return name;
}

INSTANTIATE_TEST_SUITE_P(Makers, AeroGolden, ::testing::ValuesIn(testRocketMakers()),
                         makerTestName);

// ================================================================================== coverage

/// What the makers of TestRockets.h cover of the golden aerodynamics of the test rockets.
struct AeroCoverage
{
    int         goldenInputs{0};  ///< the "testrocket" inputs of the manifest
    AeroCounts  golden;           ///< what their aero.json files hold
    AeroCounts  compared;         ///< what was compared
    std::string problems;         ///< an input without a maker, a file not compared in full, ...
};

/// Compares every "testrocket" input of the manifest with the rocket of its maker and sums what
/// the files hold and what was compared; an input without a maker, one whose file cannot be
/// read and one that was not compared in full are problems.
[[nodiscard]] AeroCoverage aeroCoverage()
{
    AeroCoverage                                           coverage;
    const QtRocket::Result<QtRocket::Test::GoldenManifest> manifest =
        QtRocket::Test::loadGoldenManifest();
    if (!manifest)
    {
        coverage.problems = manifest.error().message;
        return coverage;
    }
    const std::span<const TestRocketMaker> makers = testRocketMakers();
    for (const QtRocket::Test::GoldenInput& input : manifest->inputs)
    {
        if (input.kind != "testrocket")
        {
            continue;
        }
        coverage.goldenInputs++;
        const auto maker =
            std::ranges::find(makers, std::string_view{input.name}, &TestRocketMaker::input);
        if (maker == makers.end())
        {
            coverage.problems += std::format("{}: no maker\n", input.name);
            continue;
        }
        const GoldenRun& run = goldenRun(*maker);
        coverage.golden += run.golden;
        coverage.compared += run.comparison.compared;
        if (!run.problem.empty() || run.comparison.compared != run.golden)
        {
            coverage.problems +=
                std::format("{}: {} compared {}; the file holds {}\n", input.name, run.problem,
                            toText(run.comparison.compared), toText(run.golden));
        }
    }
    return coverage;
}

/// Every test rocket of the golden data is compared, and in full: the numbers of configurations,
/// points, force-analysis entries, worst CPs and warnings compared are those of each aero.json
/// (the per-rocket tests above report the mismatches; the totals are those of the thirteen
/// files).
TEST(AeroGoldenCoverage, EveryGoldenTestRocketIsComparedInFull)
{
    const AeroCoverage coverage = aeroCoverage();
    EXPECT_EQ(coverage.problems, "");
    EXPECT_EQ(coverage.goldenInputs, 13);
    EXPECT_EQ(static_cast<std::size_t>(coverage.goldenInputs), testRocketMakers().size());
    EXPECT_EQ(coverage.compared, coverage.golden) << "what was compared, and what the files hold";
    EXPECT_EQ(coverage.golden.configurations, 47) << "the default configurations included";
    EXPECT_EQ(coverage.golden.points, 1175) << "25 per configuration";
    EXPECT_EQ(coverage.golden.components, 9400) << "the force-analysis entries of every point";
    EXPECT_EQ(coverage.golden.worstCPs, 329) << "7 per configuration";
    EXPECT_EQ(coverage.golden.warnings, 294) << "the geometry warnings and those of the points";
}

// ================================================================================= mutations

// The comparison is not vacuous: a golden value changed in a copy of the document by four times
// its tolerance is reported, in one line that names it, and one changed by a quarter of its
// tolerance is not. The document is that of the Iso-Haisu: one configuration, and a worst CP
// that depends on the direction (two control fins).

/// Four times the tolerances of the comparison, relative and absolute alike.
constexpr double kBeyondTolerance = 4e-9;
/// A quarter of them.
constexpr double kWithinTolerance = 2.5e-10;

/// The value at the JSON pointer @p pointer of @p aero.
[[nodiscard]] json& valueAt(json& aero, std::string_view pointer)
{
    return aero.at(json::json_pointer{std::string{pointer}});
}

/// Multiplies the number at @p pointer by 1 + @p relative.
void scale(json& aero, std::string_view pointer, double relative)
{
    json& value = valueAt(aero, pointer);
    value       = value.get<double>() * (1 + relative);
}

/// Adds @p offset to the number at @p pointer.
void shift(json& aero, std::string_view pointer, double offset)
{
    json& value = valueAt(aero, pointer);
    value       = value.get<double>() + offset;
}

/// A change to a golden document that the comparison has to report, in one line.
struct Mutation
{
    std::string_view name;      ///< what is changed; it names the test
    void (*apply)(json& aero);  ///< makes the change
    std::string_view context;   ///< what the heading of the report starts with
    std::string_view line;      ///< what its one line starts with
};

/// The name of @p mutation, for the messages of the tests.
std::ostream& operator<<(std::ostream& out, const Mutation& mutation)
{
    return out << mutation.name;
}

/// The changes, each to the results of the one configuration of the Iso-Haisu.
[[nodiscard]] std::vector<Mutation> mutations()
{
    return {
        {.name = "CpX",
         .apply =
             [](json& aero) { shift(aero, "/configurations/0/points/4/cp/0", kBeyondTolerance); },
         .context = "configuration 0 point 4 (",
         .line    = "  cp.x: expected "},
        {.name = "CNa",
         .apply =
             [](json& aero) { scale(aero, "/configurations/0/points/4/cp/3", kBeyondTolerance); },
         .context = "configuration 0 point 4 (",
         .line    = "  cp.weight: expected "},
        {.name = "TotalCD",
         .apply =
             [](json& aero) {
                 scale(aero, "/configurations/0/points/7/forces/cd", kBeyondTolerance);
             },
         .context = "configuration 0 point 7 (",
         .line    = "  forces.cd: expected "},
        {.name = "TotalCm",
         .apply =
             [](json& aero) {
                 scale(aero, "/configurations/0/points/7/forces/cm", kBeyondTolerance);
             },
         .context = "configuration 0 point 7 (",
         .line    = "  forces.cm: expected "},
        {.name = "TotalCPOfTheForces",
         .apply =
             [](json& aero) {
                 shift(aero, "/configurations/0/points/7/forces/cp/0", kBeyondTolerance);
             },
         .context = "configuration 0 point 7 (",
         .line    = "  forces.cp.x: expected "},
        {.name = "RollDamping",
         .apply =
             [](json& aero) {
                 scale(aero, "/configurations/0/points/22/forces/crollDamp", kBeyondTolerance);
             },
         .context = "configuration 0 point 22 (",
         .line    = "  forces.crollDamp: expected "},
        {.name = "PitchDampingMoment",
         .apply =
             [](json& aero) {
                 scale(aero, "/configurations/0/points/22/forces/pitchDampingMoment",
                       kBeyondTolerance);
             },
         .context = "configuration 0 point 22 (",
         .line    = "  forces.pitchDampingMoment: expected "},
        {.name = "ComponentFrictionCD",
         .apply =
             [](json& aero) {
                 scale(aero, "/configurations/0/points/13/components/5/frictionCD",
                       kBeyondTolerance);
             },
         .context = "configuration 0 point 13 (",
         .line    = "  components /0/1/5.frictionCD: expected "},
        {.name = "ComponentCpX",
         .apply =
             [](json& aero) {
                 shift(aero, "/configurations/0/points/13/components/5/cp/0", kBeyondTolerance);
             },
         .context = "configuration 0 point 13 (",
         .line    = "  components /0/1/5.cp.x: expected "},
        {.name = "AGoldenZero",
         .apply =
             [](json& aero) { valueAt(aero, "/configurations/0/points/0/forces/cside") = 1e-300; },
         .context = "configuration 0 point 0 (",
         .line    = "  forces.cside: expected "},
        {.name = "Axisymmetric",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/0/forces/axisymmetric") = false;
             },
         .context = "configuration 0 point 0 (",
         .line    = "  forces.axisymmetric: expected "},
        {.name = "AFieldNothingCompares",
         .apply =
             [](json& aero) { valueAt(aero, "/configurations/0/points/0/forces")["cnew"] = 1.0; },
         .context = "configuration 0 point 0 (",
         .line    = "  forces.cnew: not compared"},
        {.name = "Velocity",
         .apply =
             [](json& aero) {
                 scale(aero, "/configurations/0/points/5/conditions/velocity", kBeyondTolerance);
             },
         .context = "configuration 0 point 5 (",
         .line    = "  conditions.velocity: expected "},
        {.name = "Beta",
         .apply =
             [](json& aero) {
                 scale(aero, "/configurations/0/points/5/conditions/beta", kBeyondTolerance);
             },
         .context = "configuration 0 point 5 (",
         .line    = "  conditions.beta: expected "},
        {.name = "AForceAnalysisEntryLess",
         .apply =
             [](json& aero) { valueAt(aero, "/configurations/0/points/3/components").erase(2); },
         .context = "configuration 0 point 3 (",
         .line    = "  components: expected "},
        {.name = "WarningText",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/15/warnings/0/text") = "Another text";
             },
         .context = "configuration 0 point 15 (",
         .line    = "  warnings[0].text: expected "},
        {.name = "WarningPriority",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/15/warnings/0/priority") = "HIGH";
             },
         .context = "configuration 0 point 15 (",
         .line    = "  warnings[0].priority: expected "},
        {.name = "WarningSources",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/15/warnings/0/sources").push_back("/0/1");
             },
         .context = "configuration 0 point 15 (",
         .line    = "  warnings[0].sources: expected "},
        {.name = "AGeometryWarningMore",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/geometryWarnings")
                     .push_back(valueAt(aero, "/configurations/0/points/15/warnings/0"));
             },
         .context = "configuration 0 geometry warnings:",
         .line    = "  geometryWarnings: number: expected 1, got 0"},
        {.name = "WorstCpX",
         .apply =
             [](json& aero) { shift(aero, "/configurations/0/worstCP/2/cp/0", kBeyondTolerance); },
         .context = "configuration 0 worst CP at Mach 0.6:",
         .line    = "  cp.x: expected "},
        {.name = "WorstTheta",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/worstCP/2/theta") = std::numbers::pi / 2;
             },
         .context = "configuration 0 worst CP at Mach 0.6:",
         .line    = "  theta: expected "},
        {.name = "ReferenceArea",
         .apply =
             [](json& aero) { scale(aero, "/configurations/0/referenceArea", kBeyondTolerance); },
         .context = "configuration 0:",
         .line    = "  referenceArea: expected "},
        {.name    = "ConfigurationName",
         .apply   = [](json& aero) { valueAt(aero, "/configurations/0/name") = "Another name"; },
         .context = "configuration 0:",
         .line    = "  name: expected "},
        {.name    = "StallAngle",
         .apply   = [](json& aero) { scale(aero, "/stallAngle", kBeyondTolerance); },
         .context = "file:",
         .line    = "  stallAngle: expected "},
        {.name    = "AtmosphericDensity",
         .apply   = [](json& aero) { scale(aero, "/atmosphere/density", kBeyondTolerance); },
         .context = "file:",
         .line    = "  atmosphere.density: expected "},
        // The structure of the document: a configuration or a worst CP less than the rocket and
        // the dumper have.
        {.name    = "AConfigurationLess",
         .apply   = [](json& aero) { valueAt(aero, "/configurations").erase(0); },
         .context = "file:",
         .line    = "  configurations: number: expected 0, got 1"},
        {.name    = "AWorstCpLess",
         .apply   = [](json& aero) { valueAt(aero, "/configurations/0/worstCP").erase(6); },
         .context = "configuration 0 inputs of the points:",
         .line    = "  worstCP: number: expected 7, got 6"},
        // An input that is not the dumper's (the yaw rate changes no result: the yaw damping
        // moment is capped by a total Cyaw of 0).
        {.name = "AnInputThatIsNotTheDumpers",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/3/conditions/yawRate") = 5.0;
             },
         .context = "configuration 0 point 3 (",
         .line    = "  the dumper's yawRate: expected 0, got 5"},
        // A field nothing compares, in every object of the document but the forces (above).
        {.name = "AConditionNothingCompares",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/3/conditions")["altitude"] = 100.0;
             },
         .context = "configuration 0 point 3 (",
         .line    = "  conditions.altitude: not compared"},
        {.name = "APointFieldNothingCompares",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/3")["cg"] = json::array({0.5, 0.0, 0.0});
             },
         .context = "configuration 0 point 3 (",
         .line    = "  cg: not compared"},
        {.name = "AComponentFieldNothingCompares",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/13/components/5")["cnew"] = 1.0;
             },
         .context = "configuration 0 point 13 (",
         .line    = "  components /0/1/5.cnew: not compared"},
        {.name = "AWarningFieldNothingCompares",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/15/warnings/0")["id"] = "an id";
             },
         .context = "configuration 0 point 15 (",
         .line    = "  warnings[0].id: not compared"},
        {.name    = "AWorstCpFieldNothingCompares",
         .apply   = [](json& aero) { valueAt(aero, "/configurations/0/worstCP/2")["aoa"] = 0.0; },
         .context = "configuration 0 worst CP at Mach 0.6:",
         .line    = "  aoa: not compared"},
        {.name  = "AConfigurationFieldNothingCompares",
         .apply = [](json& aero) { valueAt(aero, "/configurations/0")["motors"] = json::array(); },
         .context = "configuration 0:",
         .line    = "  motors: not compared"},
        {.name    = "AnAtmosphereFieldNothingCompares",
         .apply   = [](json& aero) { valueAt(aero, "/atmosphere")["altitude"] = 0.0; },
         .context = "file:",
         .line    = "  atmosphere.altitude: not compared"},
        {.name    = "AFileFieldNothingCompares",
         .apply   = [](json& aero) { aero["generator"] = "another dumper"; },
         .context = "file:",
         .line    = "  generator: not compared"},
    };
}

/// Changes of a quarter of the tolerance to the numbers mutations() changes.
void changeWithinTheTolerances(json& aero)
{
    shift(aero, "/configurations/0/points/4/cp/0", kWithinTolerance);
    scale(aero, "/configurations/0/points/4/cp/3", kWithinTolerance);
    scale(aero, "/configurations/0/points/7/forces/cd", kWithinTolerance);
    scale(aero, "/configurations/0/points/7/forces/cm", kWithinTolerance);
    shift(aero, "/configurations/0/points/7/forces/cp/0", kWithinTolerance);
    scale(aero, "/configurations/0/points/22/forces/crollDamp", kWithinTolerance);
    scale(aero, "/configurations/0/points/22/forces/pitchDampingMoment", kWithinTolerance);
    scale(aero, "/configurations/0/points/13/components/5/frictionCD", kWithinTolerance);
    shift(aero, "/configurations/0/points/13/components/5/cp/0", kWithinTolerance);
    scale(aero, "/configurations/0/points/5/conditions/velocity", kWithinTolerance);
    scale(aero, "/configurations/0/points/5/conditions/beta", kWithinTolerance);
    shift(aero, "/configurations/0/worstCP/2/cp/0", kWithinTolerance);
    scale(aero, "/configurations/0/referenceArea", kWithinTolerance);
    scale(aero, "/stallAngle", kWithinTolerance);
    scale(aero, "/atmosphere/density", kWithinTolerance);
}

/// The golden input the mutations change.
constexpr std::string_view kMutatedInput = "testrocket-iso-haisu";

/// The report of the comparison of the Iso-Haisu with its aero.json after @p change (nullptr: the
/// document as it is); "no golden data" when the document or the maker is missing.
[[nodiscard]] std::string reportAfter(void (*change)(json& aero))
{
    const std::span<const TestRocketMaker> makers = testRocketMakers();
    const auto maker            = std::ranges::find(makers, kMutatedInput, &TestRocketMaker::input);
    QtRocket::Result<json> aero = loadGoldenAero(kMutatedInput);
    if (maker == makers.end() || !aero)
    {
        return "no golden data";
    }
    if (change != nullptr)
    {
        change(*aero);
    }
    const std::unique_ptr<Rocket>       rocket = maker->make();
    const QtRocket::InMemoryPreferences preferences;  // the golden names are an empty store's
    return compareAero(*rocket, *aero, maker->randomConfigurationId, preferences).report;
}

/// One changed golden value.
class AeroGoldenMutation : public ::testing::TestWithParam<Mutation>
{ };

TEST_P(AeroGoldenMutation, IsReportedInOneLine)
{
    const Mutation&   mutation = GetParam();
    const std::string report   = reportAfter(mutation.apply);
    EXPECT_TRUE(report.starts_with(mutation.context)) << report;
    EXPECT_NE(report.find(std::format("\n{}", mutation.line)), std::string::npos) << report;
    EXPECT_EQ(std::ranges::count(report, '\n'), 2) << report;
}

/// The test name of @p info's mutation: what it changes.
[[nodiscard]] std::string mutationTestName(const ::testing::TestParamInfo<Mutation>& info)
{
    return std::string{info.param.name};
}

INSTANTIATE_TEST_SUITE_P(Changes, AeroGoldenMutation, ::testing::ValuesIn(mutations()),
                         mutationTestName);

TEST(AeroGoldenMutations, TheDocumentAsItIsMatches)
{
    EXPECT_EQ(reportAfter(nullptr), "");
}

TEST(AeroGoldenMutations, ChangesWithinTheTolerancesAreNotReported)
{
    EXPECT_EQ(reportAfter(changeWithinTheTolerances), "");
}

// =============================================================================== worst theta

/// The worst CPs of every test rocket whose theta is not the golden one, one line each.
[[nodiscard]] std::string otherWorstThetas()
{
    std::string lines;
    for (const TestRocketMaker& maker : testRocketMakers())
    {
        const GoldenRun& run = goldenRun(maker);
        if (!run.comparison.otherThetas.empty())
        {
            lines += std::format("{}:\n{}", maker.input, run.comparison.otherThetas);
        }
    }
    return lines;
}

// BLOCKED, and disabled because it cannot pass: the theta getWorstCP() leaves in the conditions,
// compared strictly with the golden one. It differs for most of the worst CPs of the rockets
// whose CP does not depend on the direction (251 of the 329 on Linux with glibc; every rocket but
// the Iso-Haisu, the simple two-stage rocket and the cluster pods): there the 360 CPs differ only
// in the rounding of the sum over the fins, and the direction that happens to give the smallest
// x follows the order of that sum (Java: the components' random ids, so OpenRocket's own answer
// changes with them) and the last bit of sin() (which differs between Java, glibc, Apple's libm
// and the UCRT). The CPs themselves match (AeroGolden), and AeroGolden accepts another theta
// only when it is a direction getWorstCP() tries whose CP is the worst CP, and the CP at the
// golden theta is the worst CP too (compareWorstTheta()): any equally bad direction passes,
// as it must, so which one is found stays uncompared and this test is its record. To see the
// list, run with --gtest_also_run_disabled_tests --gtest_filter='AeroGoldenWorstTheta.*'.
TEST(AeroGoldenWorstTheta, DISABLED_TheDirectionFoundIsJavasInEveryRocket)
{
    EXPECT_EQ(otherWorstThetas(), "");
}

// The check of the theta found is not vacuous either. The mutation WorstTheta above changes the
// golden side; these put a theta in the place of the one getWorstCP() left, for the worst CP of
// the Iso-Haisu at Mach 0.6 (golden theta 0; the CP is at x = 1.8197 m in the directions 0 and
// pi, the plane of its two control fins, and at 2.1349 m across it, in OpenRocket and here).

/// What compareWorstTheta() answers and reports.
struct ThetaVerdict
{
    bool        sameDirection{false};
    std::string report;
};

/// compareWorstTheta() for the worst CP of the Iso-Haisu at Mach 0.6, with @p theta as the
/// theta found; @p change, when given, first changes the golden worst CP.
[[nodiscard]] ThetaVerdict isoHaisuThetaVerdict(double theta, void (*change)(json& worst) = nullptr)
{
    const std::span<const TestRocketMaker> makers = testRocketMakers();
    const auto maker            = std::ranges::find(makers, kMutatedInput, &TestRocketMaker::input);
    QtRocket::Result<json> aero = loadGoldenAero(kMutatedInput);
    if (maker == makers.end() || !aero)
    {
        return {.sameDirection = false, .report = "no golden data"};
    }
    json& expected = valueAt(*aero, "/configurations/0/worstCP/2");
    if (change != nullptr)
    {
        change(expected);
    }
    const std::unique_ptr<Rocket> rocket = maker->make();
    const FlightConfiguration&    config = rocket->getSelectedConfiguration();
    BarrowmanCalculator           calculator;
    const AeroSubject subject{.rocket = rocket.get(), .config = &config, .calculator = &calculator};
    FlightConditions  conditions(config);
    conditions.setMach(goldenValue(expected.at("mach")));
    conditions.setAOA(0.0);
    const Coordinate worst = calculator.getWorstCP(config, conditions, nullptr);

    Mismatches m("worst theta");
    const bool same = compareWorstTheta(m, subject, expected, theta, worst);
    return {.sameDirection = same, .report = m.report()};
}

/// Direction @p i of the 360 getWorstCP() tries, as the library writes it.
[[nodiscard]] double worstCpDirection(int i)
{
    return 2 * std::numbers::pi * i / 360;
}

TEST(AeroGoldenWorstTheta, TheGoldenThetaIsTheSameDirection)
{
    const ThetaVerdict verdict = isoHaisuThetaVerdict(0.0);
    EXPECT_TRUE(verdict.sameDirection);
    EXPECT_EQ(verdict.report, "");
}

TEST(AeroGoldenWorstTheta, AnEquallyBadDirectionIsAnotherThetaButNoMismatch)
{
    // pi: the other direction in the plane of the control fins, with the same CP.
    const ThetaVerdict verdict = isoHaisuThetaVerdict(worstCpDirection(180));
    EXPECT_FALSE(verdict.sameDirection);
    EXPECT_EQ(verdict.report, "");
}

TEST(AeroGoldenWorstTheta, ADirectionWithAnotherCpIsReported)
{
    // pi / 2, across the control fins: a wrong answer, not an equally bad direction.
    const ThetaVerdict verdict = isoHaisuThetaVerdict(worstCpDirection(90));
    EXPECT_FALSE(verdict.sameDirection);
    EXPECT_TRUE(verdict.report.starts_with("worst theta:\n  theta: expected 0, got 1.57079"))
        << verdict.report;
    EXPECT_NE(verdict.report.find("; the CP at the theta found, x = 2.13491"), std::string::npos)
        << verdict.report;
    EXPECT_NE(verdict.report.find(", is not the worst CP found, x = 1.81970"), std::string::npos)
        << verdict.report;
    EXPECT_EQ(std::ranges::count(verdict.report, '\n'), 2) << verdict.report;

    // One direction further than the golden one is wrong already (by 0.1 mm).
    const ThetaVerdict next = isoHaisuThetaVerdict(worstCpDirection(1));
    EXPECT_NE(next.report.find("; the CP at the theta found, x = "), std::string::npos)
        << next.report;
}

/// Whether @p verdict reports a theta that is none of the 360 directions, in one line.
[[nodiscard]] bool reportsNoDirection(const ThetaVerdict& verdict)
{
    return !verdict.sameDirection &&
           verdict.report.contains("; it is none of the 360 directions getWorstCP() tries") &&
           std::ranges::count(verdict.report, '\n') == 2;
}

TEST(AeroGoldenWorstTheta, AThetaThatIsNoneOfTheDirectionsIsReported)
{
    // The theta the conditions had before the call (a getWorstCP() that left it), a direction
    // off the grid by one rounding, values outside 0 ... 2 pi, and NaN.
    EXPECT_TRUE(reportsNoDirection(isoHaisuThetaVerdict(kThetaBeforeWorstCp)));
    EXPECT_TRUE(
        reportsNoDirection(isoHaisuThetaVerdict(std::nextafter(worstCpDirection(180), 4.0))));
    EXPECT_TRUE(reportsNoDirection(isoHaisuThetaVerdict(worstCpDirection(360))));
    EXPECT_TRUE(reportsNoDirection(isoHaisuThetaVerdict(worstCpDirection(-1))));
    EXPECT_TRUE(reportsNoDirection(isoHaisuThetaVerdict(1e9)));
    EXPECT_TRUE(reportsNoDirection(isoHaisuThetaVerdict(std::numeric_limits<double>::quiet_NaN())));
    // Every direction of the 360 is one.
    EXPECT_TRUE(isWorstCpDirection(worstCpDirection(0)));
    EXPECT_TRUE(isWorstCpDirection(worstCpDirection(1)));
    EXPECT_TRUE(isWorstCpDirection(worstCpDirection(359)));
    EXPECT_FALSE(isWorstCpDirection(worstCpDirection(360)));
}

TEST(AeroGoldenWorstTheta, AThetaForARocketWithoutLiftIsReported)
{
    // A golden worst CP without weight (no direction has a CP: getWorstCP() answers
    // Double.MAX_VALUE and leaves a theta of 0) against a theta that is a direction.
    const auto withoutLift = [](json& worst) {
        worst["cp"]    = json::array({std::numeric_limits<double>::max(), 0.0, 0.0, 0.0});
        worst["theta"] = 0.0;
    };
    const ThetaVerdict verdict = isoHaisuThetaVerdict(worstCpDirection(5), withoutLift);
    EXPECT_FALSE(verdict.sameDirection);
    EXPECT_NE(verdict.report.find("; no direction has a CP, which leaves a theta of 0"),
              std::string::npos)
        << verdict.report;
    EXPECT_EQ(std::ranges::count(verdict.report, '\n'), 2) << verdict.report;
    EXPECT_TRUE(isoHaisuThetaVerdict(0.0, withoutLift).sameDirection);
}

TEST(AeroGoldenWorstTheta, TheThetaBeforeTheCallIsNoneOfTheDirections)
{
    // ... so that a getWorstCP() that left the theta it was given is seen (compareWorstCP()).
    EXPECT_FALSE(isWorstCpDirection(kThetaBeforeWorstCp));
}

// ================================================================== warnings with a parameter

// No aerodynamic calculator raises a warning with a parameter (the angle of a LargeAOA comes
// from the simulation), so no aero.json holds one and the comparison of the parameter never runs
// against golden data. It is tested here: a LargeAOA against its own golden form and against
// changed ones.

/// @p warning as the dumper writes it (Values.warning()): its class, priority, description,
/// text, the paths of its sources in @p rocket and, for a warning with one, its parameter.
[[nodiscard]] json goldenFormOf(const Warning& warning, const Rocket& rocket)
{
    json form           = json::object();
    form["type"]        = std::string{warning.typeName()};
    form["priority"]    = std::string{QtRocket::exportLabel(warning.priority())};
    form["description"] = warning.messageDescription();
    form["text"]        = warning.toString();
    form["sources"]     = json::array();
    for (const QtRocket::MessageSource& source : warning.sources())
    {
        const RocketComponent* component = rocket.findComponent(source.id);
        form["sources"].push_back(component != nullptr ? goldenPathOf(*component)
                                                       : std::string{"?"});
    }
    if (const std::optional<double> parameter = parameterOf(warning))
    {
        form["parameter"] = *parameter;
    }
    return form;
}

/// The report of compareWarning() for @p actual against the golden form @p expected.
[[nodiscard]] std::string warningReport(const json& expected, const Warning& actual,
                                        const Rocket& rocket)
{
    Mismatches m("warning");
    compareWarning(m, "w", expected, actual, rocket);
    return m.report();
}

/// The angle of the LargeAOA of these tests, in radians.
constexpr double kLargeAoa = 0.4;

TEST(AeroGoldenWarnings, TheParameterOfAWarningIsCompared)
{
    const QtRocket::Test::DefaultUnitsGuard units;  // the text of a LargeAOA prints the angle
    const Rocket                            rocket;
    const Warning::LargeAOA                 large{kLargeAoa};
    EXPECT_EQ(parameterOf(large), kLargeAoa);

    json expected = goldenFormOf(large, rocket);
    ASSERT_TRUE(expected.contains("parameter"));
    EXPECT_EQ(expected.at("type"), "LargeAOA");
    EXPECT_EQ(warningReport(expected, large, rocket), "");

    expected["parameter"] = kLargeAoa * (1 + kWithinTolerance);
    EXPECT_EQ(warningReport(expected, large, rocket), "");

    expected["parameter"]    = kLargeAoa * (1 + kBeyondTolerance);
    const std::string report = warningReport(expected, large, rocket);
    EXPECT_TRUE(report.starts_with("warning:\n  w.parameter: expected 0.40000000")) << report;
    EXPECT_NE(report.find(", got 0.4 (difference "), std::string::npos) << report;
    EXPECT_EQ(std::ranges::count(report, '\n'), 2) << report;
}

TEST(AeroGoldenWarnings, AParameterOnOneSideOnlyIsReported)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    const Rocket                            rocket;
    const Warning::LargeAOA                 large{kLargeAoa};

    // The golden warning without its parameter.
    json expected = goldenFormOf(large, rocket);
    expected.erase("parameter");
    const std::string missing = warningReport(expected, large, rocket);
    EXPECT_TRUE(missing.starts_with("warning:\n  w has a parameter: expected false, got true"))
        << missing;
    EXPECT_EQ(std::ranges::count(missing, '\n'), 2) << missing;

    // A golden parameter for a warning that has none.
    const Warning& plain    = Warning::kSupersonic;
    json           withOne  = goldenFormOf(plain, rocket);
    withOne["parameter"]    = 1.0;
    const std::string extra = warningReport(withOne, plain, rocket);
    EXPECT_TRUE(extra.starts_with("warning:\n  w has a parameter: expected true, got false"))
        << extra;
    EXPECT_EQ(std::ranges::count(extra, '\n'), 2) << extra;
    EXPECT_EQ(warningReport(goldenFormOf(plain, rocket), plain, rocket), "");
}

TEST(AeroGoldenWarnings, AWarningWithSourcesIsComparedByTheirPaths)
{
    // The sources of a warning are compared as golden paths, in their order.
    const QtRocket::Test::DefaultUnitsGuard units;
    const QtRocket::Test::TestEstesAlphaIII alpha;
    WarningSet                              warnings;
    warnings.add(Warning::kDiameterDiscontinuity,
                 QtRocket::MessageSources{QtRocket::MessageSource::of(*alpha.nose),
                                          QtRocket::MessageSource::of(*alpha.body)});
    const Warning& warning  = *warnings.begin();
    json           expected = goldenFormOf(warning, *alpha.rocket);
    EXPECT_EQ(expected.at("sources"), json::array({"/0/0", "/0/1"}));
    EXPECT_EQ(warningReport(expected, warning, *alpha.rocket), "");

    expected["sources"]      = json::array({"/0/1", "/0/0"});
    const std::string report = warningReport(expected, warning, *alpha.rocket);
    EXPECT_TRUE(report.starts_with("warning:\n  w.sources: expected ")) << report;
    EXPECT_EQ(std::ranges::count(report, '\n'), 2) << report;
}

}  // namespace
