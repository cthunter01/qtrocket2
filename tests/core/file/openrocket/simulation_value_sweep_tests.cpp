// A sweep over every value of the simulation side of a design file: a <simulations> element
// that uses every element and attribute the handlers of this side read (the simulation, its
// conditions, its extensions, plot appearances and landing dispersion settings, its stored
// flight data with warnings, branches, events and data points) is loaded again and again, each
// time with one value replaced by a text a file should not have there: not-a-number and the
// infinities in every spelling, nothing, a blank, a word, a number outside the range of a
// double, a negative number, zero, and a very long text.
//
// Whatever stands there, a load must end as a load does: with the simulations loaded (and a
// warning where the handler has one) or with an ordinary failure, never with a BugError or
// another exception, and never with a simulation that Simulation::validateInputs() then refuses
// for a number of its options that is not finite (decision U3). The sanitizer presets run this
// test too, which is where a crash would show.
//
// The lookup elements of the document name files by relative names, and a sweep value in such a
// place is the name of a file ("0", "abc", "NaN"). The loads therefore run with an empty
// directory of the test's own as the current directory: a file of such a name in the directory
// the tests are run from would be read and change the counts.

#include <array>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/openrocket/SimulationsHandler.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "TestTempDir.h"
#include "file/openrocket/ConditionsTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "file/openrocket/SimulationTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::ErrorCode;
using QtRocket::Result;
using QtRocket::Simulation;
using QtRocket::SimulationOptions;
using QtRocket::SimulationsHandler;
using QtRocket::Test::CurrentDirectoryGuard;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::runHandler;
using QtRocket::Test::SimulationFixture;
using QtRocket::Test::TempDir;

using Texts = std::vector<std::string>;

/// Where a value of the document starts and ends in kDocument.
constexpr std::string_view kSlotOpen  = "{{";
constexpr std::string_view kSlotClose = "}}";

/// The document: two simulations that between them use every element and attribute of the
/// simulation side. A value that the sweep replaces stands between "{{" and "}}"; what stands
/// there is the value of the document as it is loaded otherwise, which loads without a warning.
constexpr std::string_view kDocument = R"(
<simulations>
  <simulation status="{{uptodate}}">
    <name>{{Sim one}}</name>
    <simulator>{{RK4Simulator}}</simulator>
    <calculator>{{BarrowmanCalculator}}</calculator>
    <listener>{{some.Listener}}</listener>
    <conditions>
      <configid>{{11111111-1111-1111-1111-111111111111}}</configid>
      <launchrodlength>{{1.5}}</launchrodlength>
      <launchintowind>{{false}}</launchintowind>
      <launchrodangle>{{5.0}}</launchrodangle>
      <launchroddirection>{{90.0}}</launchroddirection>
      <windaverage>{{2.0}}</windaverage>
      <windturbulence>{{0.1}}</windturbulence>
      <winddirection>{{1.5707963267948966}}</winddirection>
      <wind model="{{average}}">
        <speed>{{2.0}}</speed>
        <direction>{{1.5707963267948966}}</direction>
        <standarddeviation>{{0.2}}</standarddeviation>
      </wind>
      <wind model="{{multilevel}}" altituderef="{{agl}}">
        <windlevel altitude="{{0.0}}" speed="{{3.8}}" direction="{{3.0}}" standarddeviation="{{1.52}}"/>
        <windlevel altitude="{{100.0}}" speed="{{5.0}}" direction="{{1.0}}" standarddeviation="{{0.5}}"/>
      </wind>
      <windmodeltype>{{MultiLevel}}</windmodeltype>
      <launchaltitude>{{100.0}}</launchaltitude>
      <launchlatitude>{{32.0}}</launchlatitude>
      <launchlongitude>{{-106.0}}</launchlongitude>
      <geodeticmethod>{{spherical}}</geodeticmethod>
      <simulationsteppermethod>{{rk4}}</simulationsteppermethod>
      <randomseed>{{42}}</randomseed>
      <atmosphere model="{{extendedisa}}">
        <basetemperature>{{290.0}}</basetemperature>
        <basepressure>{{100000.0}}</basepressure>
        <baserelativehumidity>{{0.5}}</baserelativehumidity>
      </atmosphere>
      <gravity model="{{constant}}">
        <value>{{9.5}}</value>
      </gravity>
      <timestep>{{0.05}}</timestep>
      <maxtime>{{1200.0}}</maxtime>
      <recoveryspeedwarning>{{20.0}}</recoveryspeedwarning>
      <drogueLowspeedwarning>{{3.0}}</drogueLowspeedwarning>
      <recoverydroguemainhighspeedwarning>{{30.0}}</recoverydroguemainhighspeedwarning>
      <recoverydroguemainlowspeedwarning>{{15.0}}</recoverydroguemainlowspeedwarning>
      <draglookup file="{{drag.csv}}">
        <row>{{Mach,Cd}}</row>
        <row>{{0}},{{0.3}}</row>
        <row>1,0.5</row>
      </draglookup>
      <stabilitylookup file="{{stability.csv}}">
        <row>Mach,Cn,Cm,Cp</row>
        <row>{{0}},{{1}},{{2}},{{3}}</row>
        <row>{{2,3,4,5}}</row>
      </stabilitylookup>
    </conditions>
    <landingdispersion runs="{{500}}" seed="{{12345}}">
      <uncertainty parameter="{{windspeed}}" distribution="{{normal}}" spread="{{0.5}}"/>
    </landingdispersion>
    <plotappearance>
      <series symbol="{{h}}" linestyle="{{dashed}}" red="{{255}}" green="{{0}}" blue="{{0}}" alpha="{{255}}"/>
    </plotappearance>
    <extension extensionid="{{info.openrocket.core.simulation.extension.example.AirStart}}">
      <entry key="{{launchAltitude}}" type="{{number}}">{{250}}</entry>
      <entry key="launchVelocity" type="number">{{12.5}}</entry>
      <entry key="flag" type="boolean">{{true}}</entry>
      <entry key="text" type="string">{{text}}</entry>
      <entry key="list" type="{{list}}"><entry type="{{number}}">{{1}}</entry></entry>
    </extension>
    <extension extensionid="info.openrocket.core.simulation.extension.example.RollControl">
      <entry key="controlFinName" type="string">{{CONTROL}}</entry>
      <entry key="startTime" type="number">{{0.5}}</entry>
      <entry key="setPoint" type="number">{{0.0}}</entry>
      <entry key="finRate" type="number">{{10.0}}</entry>
      <entry key="maxFinAngle" type="number">{{15.0}}</entry>
      <entry key="KP" type="number">{{0.007}}</entry>
      <entry key="KI" type="number">{{0.2}}</entry>
    </extension>
    <extension extensionid="info.openrocket.core.simulation.extension.impl.ScriptingExtension">
      <entry key="script" type="string">{{var a = 1;}}</entry>
      <entry key="language" type="string">{{JavaScript}}</entry>
      <entry key="enabled" type="boolean">{{false}}</entry>
    </extension>
    <flightdata maxaltitude="{{10.5}}" maxvelocity="{{20.5}}" flighttime="{{3.0}}">
      <warning type="{{HighSpeedDeployment}}">
        <id>{{cccccccc-0000-0000-0000-000000000001}}</id>
        <description>{{Recovery device deployment at high speed (26.9 m/s)}}</description>
        <priority>{{NORMAL}}</priority>
        <source>{{aaaaaaaa-0000-0000-0000-000000000004}}</source>
        <parameter>{{26.9}}</parameter>
      </warning>
      <warning type="LargeAOA">
        <id>cccccccc-0000-0000-0000-000000000002</id>
        <parameter>{{0.35}}</parameter>
      </warning>
      <warning type="EventAfterLanding">
        <id>cccccccc-0000-0000-0000-000000000003</id>
      </warning>
      <warning>{{A warning that is only text}}</warning>
      <databranch name="{{Sustainer}}" optimumAltitude="{{12.5}}" timeToOptimumAltitude="{{1.5}}" types="{{time,altitude,velocity_total}}">
        <event time="{{0}}" type="{{launch}}" id="{{bbbbbbbb-0000-0000-0000-000000000001}}" source="{{aaaaaaaa-0000-0000-0000-000000000001}}"/>
        <event time="1" type="simwarn" id="bbbbbbbb-0000-0000-0000-000000000002" warnid="{{cccccccc-0000-0000-0000-000000000001}}"/>
        <event time="1.5" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000003"/>
        <event time="2" type="simwarn" id="bbbbbbbb-0000-0000-0000-000000000004" warnid="cccccccc-0000-0000-0000-000000000003" eventid="{{bbbbbbbb-0000-0000-0000-000000000003}}"/>
        <event time="3" type="simabort" id="bbbbbbbb-0000-0000-0000-000000000005" cause="{{nomotorsdefined}}"/>
        <datapoint>{{0}},{{0}},{{0}}</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>{{2,5,NaN}}</datapoint>
      </databranch>
    </flightdata>
  </simulation>
  <simulation status="{{loaded}}">
    <name>Sim two</name>
    <conditions>
      <configid>{{22222222-2222-2222-2222-222222222222}}</configid>
      <atmosphere model="{{isa}}"/>
      <gravity model="{{wgs}}"/>
      <draglookupcsv>{{}}</draglookupcsv>
      <stabilitylookupcsv>{{}}</stabilitylookupcsv>
    </conditions>
    <flightdata maxaltitude="{{10.5}}" maxvelocity="{{20.5}}" maxacceleration="{{30.5}}" maxmach="{{0.5}}" timetoapogee="{{1.5}}" flighttime="{{3.0}}" groundhitvelocity="{{4.5}}" launchrodvelocity="{{12.5}}" deploymentvelocity="{{5.5}}" optimumdelay="{{2.5}}"/>
  </simulation>
</simulations>
)";

/// The length of the long texts of the sweep.
constexpr std::size_t kLongLength = 100000;

/// The text the sweep value @p value stands for: itself, but kLongLength letters for "<long
/// text>" and as many digits for "<long number>".
[[nodiscard]] std::string sweepText(std::string_view value)
{
    std::string text(value);
    if (value == "<long text>")
    {
        text.assign(kLongLength, 'x');
    }
    else if (value == "<long number>")
    {
        text.assign(kLongLength, '9');
    }
    return text;
}

/// kDocument cut at its slots: the texts between the values, and the values.
struct Template
{
    /// The texts around the values: one more than there are values.
    std::vector<std::string_view> texts;
    /// The values of the document as it is.
    std::vector<std::string_view> values;

    /// The document with the value at @p slot replaced by @p replacement; with a @p slot the
    /// document does not have, the document as it is.
    [[nodiscard]] std::string with(std::size_t slot, std::string_view replacement) const
    {
        std::string document;
        for (std::size_t i = 0; i < values.size(); i++)
        {
            document += texts.at(i);
            document += i == slot ? replacement : values.at(i);
        }
        document += texts.at(values.size());
        return document;
    }

    /// What a failure of the test calls the value at @p slot: its number and the text before
    /// it, which names its element or attribute.
    [[nodiscard]] std::string describe(std::size_t slot) const
    {
        const std::string_view before = texts.at(slot);
        const std::size_t      start  = before.rfind('<');
        return std::format("value {} after '{}'", slot,
                           start == std::string_view::npos ? before : before.substr(start));
    }
};

/// kDocument as a Template.
[[nodiscard]] Template parseDocument()
{
    Template    result;
    std::size_t from = 0;
    for (std::size_t open = kDocument.find(kSlotOpen); open != std::string_view::npos;
         open             = kDocument.find(kSlotOpen, from))
    {
        const std::size_t close = kDocument.find(kSlotClose, open);
        result.texts.push_back(kDocument.substr(from, open - from));
        result.values.push_back(
            kDocument.substr(open + kSlotOpen.size(), close - open - kSlotOpen.size()));
        from = close + kSlotClose.size();
    }
    result.texts.push_back(kDocument.substr(from));
    return result;
}

/// How the loads of a sweep ended.
struct Tally
{
    /// Loads that failed with an ordinary error.
    int failed{0};
    /// Loads that succeeded with at least one warning.
    int warned{0};
    /// Loads that succeeded without a warning: the value is one the element takes.
    int silent{0};
    /// Simulations of loads that succeeded which Simulation::validateInputs() refuses for a
    /// number of an extension's configuration.
    int refusedForAnExtension{0};

    [[nodiscard]] bool operator==(const Tally&) const = default;
};

/// A Tally in the making, with one letter per load in the order of the values (F for failed,
/// W for warned, S for silent), which tells where the counts changed when they do.
struct Outcomes
{
    Tally       tally;
    std::string letters;
};

/// What is wrong with the loaded simulation @p simulation, or nothing: its options hold what
/// their copy holds (a NaN among them would make them differ), its status can be worked out,
/// and a run would not be refused for a number that is not finite, but for one of an
/// extension's configuration, which a file can make infinite only as an integer too large for
/// a double (see Simulation::validateInputs()); such a refusal is counted in @p tally.
[[nodiscard]] std::string problemOf(Simulation& simulation, Tally& tally)
{
    const SimulationOptions* simulated = simulation.getSimulatedConditions();
    if (simulated == nullptr || !(*simulated == simulation.getOptions()))
    {
        return "the options differ from the simulated conditions";
    }
    simulation.syncModId();
    static_cast<void>(simulation.getStatus());
    const Result<void> valid = simulation.validateInputs();
    if (valid.has_value())
    {
        return {};
    }
    if (!valid.error().message.contains("of the simulation extension"))
    {
        return valid.error().message;
    }
    tally.refusedForAnExtension++;
    return {};
}

/// Loads @p document in a new fixture and adds how it ended to @p outcomes; returns what is
/// wrong with the outcome, or nothing.
[[nodiscard]] std::string loadAndCheck(const std::string& document, Outcomes& outcomes)
{
    SimulationFixture  fixture;
    SimulationsHandler handler(fixture.context());
    const HandlerRun   run = runHandler(handler, document);
    if (!run.result.has_value())
    {
        outcomes.tally.failed++;
        outcomes.letters += 'F';
        // The one failure a handler of this side has: what is an IllegalArgumentException in
        // OpenRocket, with its message. (Only an id with nothing between two of its four
        // dashes fails without a message, as in Java, and no value of the sweep is one.)
        if (run.result.error().code != ErrorCode::INVALID_ARGUMENT ||
            run.result.error().message.empty())
        {
            return "failed with " + run.result.error().toString();
        }
        return {};
    }
    (run.warnings.empty() ? outcomes.tally.silent : outcomes.tally.warned)++;
    outcomes.letters += run.warnings.empty() ? 'S' : 'W';
    for (std::size_t i = 0; i < fixture.document().getSimulationCount(); i++)
    {
        std::string problem = problemOf(*fixture.document().getSimulation(i), outcomes.tally);
        if (!problem.empty())
        {
            return std::format("simulation {}: {}", i, problem);
        }
    }
    return {};
}

/// Loads the document once for every value of it, with @p value in the place of that value,
/// and returns what went wrong, one text per load that did not end as a load may; @p outcomes
/// receives how the loads ended. The loads run with an empty directory of the test's own as the
/// current directory (see the head of this file).
[[nodiscard]] Texts sweep(std::string_view value, Outcomes& outcomes)
{
    const TempDir               directory;
    const CurrentDirectoryGuard workingDirectory(directory.path());
    const Template              document    = parseDocument();
    const std::string           replacement = sweepText(value);
    Texts                       problems;
    for (std::size_t slot = 0; slot < document.values.size(); slot++)
    {
        std::string problem;
        try
        {
            problem = loadAndCheck(document.with(slot, replacement), outcomes);
        }
        catch (const std::exception& e)
        {
            problem = std::format("threw: {}", e.what());
        }
        if (!problem.empty())
        {
            problems.push_back(
                std::format("'{}' as {}: {}", value, document.describe(slot), problem));
        }
    }
    return problems;
}

/// The number of values of kDocument.
constexpr std::size_t kValues = 129;

// The document the sweep changes is a good one: it has the values the sweep counts on, and as
// it stands it loads without a warning into two simulations that pass every check.
TEST(SimulationValueSweepDocument, LoadsWithoutAWarningAsItIs)
{
    const DefaultUnitsGuard     units;
    const TempDir               directory;
    const CurrentDirectoryGuard workingDirectory(directory.path());
    const Template              document = parseDocument();
    EXPECT_EQ(document.values.size(), kValues);
    EXPECT_EQ(document.texts.size(), kValues + 1);

    Outcomes          outcomes;
    const std::string problem = loadAndCheck(document.with(kValues, ""), outcomes);
    EXPECT_EQ(problem, "");
    EXPECT_EQ(outcomes.letters, "S");
    EXPECT_EQ(outcomes.tally.refusedForAnExtension, 0);

    SimulationFixture  fixture;
    SimulationsHandler handler(fixture.context());
    const HandlerRun   run = runHandler(handler, document.with(kValues, ""));
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});
    ASSERT_EQ(fixture.document().getSimulationCount(), 2U);
    const std::shared_ptr<Simulation> one = fixture.document().getSimulation(0);
    EXPECT_EQ(one->getSimulationExtensions().size(), 4U);
    EXPECT_TRUE(one->getLandingDispersionSettings().has_value());
    EXPECT_EQ(one->getPlotAppearances().size(), 1U);
    ASSERT_NE(one->getSimulatedData(), nullptr);
    EXPECT_EQ(one->getSimulatedData()->getWarningSet().size(), 4U);
    ASSERT_EQ(one->getSimulatedData()->getBranchCount(), 1U);
    EXPECT_EQ(one->getSimulatedData()->getBranch(0).getEvents().size(), 5U);
    EXPECT_EQ(one->getSimulatedData()->getBranch(0).getLength(), 3U);
    EXPECT_TRUE(one->getOptions().getDragLookupTable() != nullptr);
    EXPECT_TRUE(one->getOptions().getStabilityLookupTable() != nullptr);
}

/// One value of the sweep and how the loads with it end.
struct SweepCase
{
    std::string_view value;
    Tally            tally;
};

/// What GoogleTest prints for the case of a test that failed.
// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const SweepCase& sweepCase, std::ostream* out)
{
    *out << "'" << sweepCase.value << "'";
}

class SimulationValueSweep : public ::testing::TestWithParam<SweepCase>
{
private:
    DefaultUnitsGuard m_units;
};

// Every value of the document replaced by the value of the case, one at a time: no load ends
// in a BugError or another exception, a failure is an ordinary one with a message, and a
// simulation that loaded passes the checks of problemOf(). How many of the loads fail, warn
// and pass silently is what the handlers do today with that value, counted so that a change
// shows.
TEST_P(SimulationValueSweep, NoValueBreaksALoad)
{
    Outcomes    outcomes;
    const Texts problems = sweep(GetParam().value, outcomes);
    const Tally tally    = outcomes.tally;
    EXPECT_EQ(problems, Texts{});
    EXPECT_EQ(tally.failed + tally.warned + tally.silent, static_cast<int>(kValues));
    EXPECT_TRUE(tally == GetParam().tally)
        << tally.failed << " failed, " << tally.warned << " warned, " << tally.silent << " silent, "
        << tally.refusedForAnExtension << " refused for an extension: " << outcomes.letters;
}

/// What the sweep puts in the place of a value: the texts a file should not have there, and
/// three more: the spellings of the infinities that OpenRocket's own flight data use ("Inf"
/// and "-Inf") and a number of kLongLength digits.
constexpr auto kSweepCases = std::to_array<SweepCase>({
    {.value = "NaN",
     .tally = {.failed = 7, .warned = 54, .silent = 68, .refusedForAnExtension = 0}},
    {.value = "Infinity",
     .tally = {.failed = 7, .warned = 60, .silent = 62, .refusedForAnExtension = 0}},
    {.value = "-Infinity",
     .tally = {.failed = 7, .warned = 56, .silent = 66, .refusedForAnExtension = 0}},
    {.value = "", .tally = {.failed = 17, .warned = 46, .silent = 66, .refusedForAnExtension = 0}},
    {.value = " ", .tally = {.failed = 17, .warned = 46, .silent = 66, .refusedForAnExtension = 0}},
    {.value = "abc",
     .tally = {.failed = 17, .warned = 49, .silent = 63, .refusedForAnExtension = 0}},
    {.value = "1e999",
     .tally = {.failed = 7, .warned = 60, .silent = 62, .refusedForAnExtension = 0}},
    {.value = "-1", .tally = {.failed = 7, .warned = 26, .silent = 96, .refusedForAnExtension = 0}},
    {.value = "0", .tally = {.failed = 8, .warned = 25, .silent = 96, .refusedForAnExtension = 0}},
    {.value = "<long text>",
     .tally = {.failed = 17, .warned = 49, .silent = 63, .refusedForAnExtension = 0}},
    {.value = "Inf",
     .tally = {.failed = 17, .warned = 45, .silent = 67, .refusedForAnExtension = 0}},
    {.value = "-Inf",
     .tally = {.failed = 17, .warned = 45, .silent = 67, .refusedForAnExtension = 0}},
    {.value = "<long number>",
     .tally = {.failed = 7, .warned = 60, .silent = 62, .refusedForAnExtension = 8}},
});

/// The name of the test of a sweep value.
[[nodiscard]] std::string sweepTestName(const ::testing::TestParamInfo<SweepCase>& info)
{
    const std::string_view value = info.param.value;
    if (value.empty())
    {
        return "Empty";
    }
    if (value == " ")
    {
        return "Blank";
    }
    return QtRocket::Test::conditionsTestName(value);
}

INSTANTIATE_TEST_SUITE_P(Values, SimulationValueSweep, ::testing::ValuesIn(kSweepCases),
                         sweepTestName);

/// The counts kSweepCases has for @p value.
[[nodiscard]] Tally pinnedTally(std::string_view value)
{
    for (const SweepCase& sweepCase : kSweepCases)
    {
        if (sweepCase.value == value)
        {
            return sweepCase.tally;
        }
    }
    return {};
}

// What lies in the directory the tests are run from does not reach the sweep. With a drag table
// under the name "0" there, the sweep of the value "0" read it where the value stands for the
// name of a file, and one load that warns of a missing file passed silently (run 9b, review).
TEST(SimulationValueSweepDocument, TheSweepDoesNotReadTheDirectoryTheTestsAreRunFrom)
{
    const DefaultUnitsGuard units;
    const TempDir           directory;
    ASSERT_TRUE(std::filesystem::exists(directory.write("0", "Mach,Cd\n0,0.3\n1,0.5\n")));
    const CurrentDirectoryGuard runFrom(directory.path());

    // The table is there for a load that looks for it.
    {
        SimulationFixture  fixture;
        SimulationsHandler handler(fixture.context());
        const HandlerRun   run =
            runHandler(handler,
                       "<simulations><simulation status='uptodate'><conditions><configid>"
                       "11111111-1111-1111-1111-111111111111</configid>"
                       "<draglookup file='0'/></conditions></simulation></simulations>");
        EXPECT_EQ(run.texts(), Texts{});
        ASSERT_EQ(fixture.document().getSimulationCount(), 1U);
        EXPECT_TRUE(fixture.document().getSimulation(0)->getOptions().hasDragLookup());
    }

    Outcomes    outcomes;
    const Texts problems = sweep("0", outcomes);
    EXPECT_EQ(problems, Texts{});
    const Tally tally = outcomes.tally;
    EXPECT_TRUE(tally == pinnedTally("0"))
        << tally.failed << " failed, " << tally.warned << " warned, " << tally.silent << " silent, "
        << tally.refusedForAnExtension << " refused for an extension: " << outcomes.letters;
}

}  // namespace
