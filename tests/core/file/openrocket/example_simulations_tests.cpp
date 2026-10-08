// The simulations of OpenRocket's sixteen example designs (data/examples), loaded through the
// handlers of the simulation side of the .ork loader alone: the <simulations> element of each
// file goes through SimulationsHandler under a stand-in parent, into a document whose rocket
// has the flight configurations the file defines and none of its components (the handlers of
// the rocket side are not needed for this, and the loader that puts the two sides together
// comes after them). What is expected is what OpenRocket's own loader makes of the same files
// (ExampleSimulations.h): no warning, 54 simulations that are all LOADED, their names,
// configurations and extensions, and of their stored flight data the summary values (which are
// computed from the stored columns), the warnings and of every branch its rows, columns and
// events.

#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/ZipInputStream.h"
#include "QtRocket/file/openrocket/SimulationsHandler.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"
#include "TestPaths.h"
#include "file/openrocket/ConditionsTestSupport.h"
#include "file/openrocket/ExampleSimulations.h"
#include "file/openrocket/FlightDataTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "file/openrocket/SimulationTestSupport.h"
#include "rocket/TestRockets.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::FlightConfigurationId;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightEvent;
using QtRocket::Result;
using QtRocket::Simulation;
using QtRocket::SimulationExtension;
using QtRocket::SimulationsHandler;
using QtRocket::Uuid;
using QtRocket::Warning;
using QtRocket::ZipInputStream;
using QtRocket::Test::ascii;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::ExampleSimulations;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::kExampleSimulations;
using QtRocket::Test::runHandler;
using QtRocket::Test::SimulationFixture;

using Texts = std::vector<std::string>;

/// The design document of the example @p file: the contents of the entry "rocket.ork" of the
/// archive, read in memory; empty (with a failure of the test) when there is none.
[[nodiscard]] std::string readExampleDocument(std::string_view file)
{
    const Result<std::vector<std::byte>> bytes =
        QtRocket::readFile(QtRocket::Test::dataDir() / "examples" / std::string(file));
    if (!bytes)
    {
        ADD_FAILURE() << file << ": " << bytes.error().message;
        return {};
    }
    ZipInputStream stream{std::span<const std::byte>(*bytes)};
    for (;;)
    {
        const Result<std::optional<ZipInputStream::Entry>> entry = stream.nextEntry();
        if (!entry || !entry->has_value())
        {
            ADD_FAILURE() << file << ": no entry rocket.ork";
            return {};
        }
        if (entry->value_or(ZipInputStream::Entry{}).name != "rocket.ork")
        {
            continue;
        }
        const Result<std::vector<std::byte>> contents = stream.readEntry();
        if (!contents)
        {
            ADD_FAILURE() << file << ": " << contents.error().message;
            return {};
        }
        return QtRocket::bytesToString(*contents);
    }
}

/// The <simulations> element of @p document, cut out as text; empty when there is none.
[[nodiscard]] std::string_view simulationsElement(std::string_view document)
{
    constexpr std::string_view kOpen  = "<simulations>";
    constexpr std::string_view kClose = "</simulations>";
    const std::size_t          start  = document.find(kOpen);
    const std::size_t          end    = document.rfind(kClose);
    if (start == std::string_view::npos || end == std::string_view::npos || end < start)
    {
        return {};
    }
    return document.substr(start, end + kClose.size() - start);
}

/// The ids of the flight configurations @p document defines: the configid attributes of its
/// <motorconfiguration> elements, in the order of the file.
[[nodiscard]] std::vector<std::string> motorConfigurationIds(std::string_view document)
{
    constexpr std::string_view kMark = "<motorconfiguration configid=\"";
    std::vector<std::string>   ids;
    for (std::size_t at = document.find(kMark); at != std::string_view::npos;
         at             = document.find(kMark, at + 1))
    {
        const std::size_t start = at + kMark.size();
        const std::size_t end   = document.find('"', start);
        if (end == std::string_view::npos)
        {
            break;
        }
        ids.emplace_back(document.substr(start, end - start));
    }
    return ids;
}

/// The events of @p branch as ExamplesProbe prints them: how many there are of each type, in
/// the order the types first occur ("{LAUNCH=1, IGNITION=2}"), how many have a source and how
/// many have data.
[[nodiscard]] std::string describeEvents(const FlightDataBranch& branch)
{
    std::vector<std::pair<std::string_view, int>> counts;
    int                                           withSource = 0;
    int                                           withData   = 0;
    for (const FlightEvent& event : branch.getEvents())
    {
        const std::string_view type  = name(event.getType());
        bool                   known = false;
        for (auto& [countedType, count] : counts)
        {
            if (countedType == type)
            {
                count++;
                known = true;
            }
        }
        if (!known)
        {
            counts.emplace_back(type, 1);
        }
        withSource += event.getSourceId().has_value() ? 1 : 0;
        withData += event.hasData() ? 1 : 0;
    }
    std::string text;
    for (const auto& [type, count] : counts)
    {
        text += std::format("{}{}={}", text.empty() ? "" : ", ", type, count);
    }
    return std::format("events={{{}}} withSource={} withData={}", text, withSource, withData);
}

/// One branch of flight data as ExampleSimulations.h has it.
[[nodiscard]] std::string describeBranch(std::size_t index, const FlightDataBranch& branch)
{
    const auto number = [](double value) { return QtRocket::Strings::javaDoubleToString(value); };
    const std::size_t columns = branch.getTypes().size();
    std::size_t       builtin = 0;
    for (const FlightDataType* type : branch.getTypes())
    {
        builtin += FlightDataType::getTypeBySaveKey(type->getSaveKey()) == type ? 1U : 0U;
    }
    return ascii(std::format(
        "  branch[{}] '{}' rows={} types={} (builtin {}, other {}) srcId={} optAlt={} tOptAlt={} "
        "optDelay={} sepTime={} {} mutable={}",
        index, branch.getName(), branch.getLength(), columns, builtin, columns - builtin,
        branch.getSourceComponentId().has_value()
            ? branch.getSourceComponentId().value_or(Uuid::nil()).toString()
            : "null",
        number(branch.getOptimumAltitude()), number(branch.getTimeToOptimumAltitude()),
        number(branch.getOptimumDelay()), number(branch.getSeparationTime()),
        describeEvents(branch), branch.isMutable()));
}

/// The stored flight data @p data of a simulation as ExampleSimulations.h has them: the
/// summary, the warnings and the branches.
void describeData(const FlightData& data, Texts& lines)
{
    const auto number = [](double value) { return QtRocket::Strings::javaDoubleToString(value); };
    lines.push_back(std::format(
        "  summary maxAlt={} maxVel={} maxAcc={} maxMach={} tApogee={} tFlight={} vGround={} "
        "vRod={} vDeploy={} optDelay={}",
        number(data.getMaxAltitude()), number(data.getMaxVelocity()),
        number(data.getMaxAcceleration()), number(data.getMaxMachNumber()),
        number(data.getTimeToApogee()), number(data.getFlightTime()),
        number(data.getGroundHitVelocity()), number(data.getLaunchRodVelocity()),
        number(data.getDeploymentVelocity()), number(data.getOptimumDelay())));
    for (const Warning& warning : data.getWarningSet())
    {
        lines.push_back(ascii(std::format("  warning {} prio={} id={} desc='{}'",
                                          warning.typeName(), exportLabel(warning.priority()),
                                          warning.id().toString(), warning.messageDescription())));
    }
    for (std::size_t index = 0; index < data.getBranchCount(); index++)
    {
        lines.push_back(describeBranch(index, data.getBranch(index)));
    }
}

/// What a loaded example is counted by.
struct Counts
{
    int simulations{0};
    int branches{0};
    int rows{0};
    int events{0};
    int warnings{0};

    [[nodiscard]] bool operator==(const Counts&) const = default;
};

/// The simulations of the document of @p fixture as ExampleSimulations.h has them, after the
/// loader's next step for each (Simulation::syncModId()); @p stored receives the status the
/// handler gave each simulation and @p counts what there is of everything.
[[nodiscard]] Texts describeSimulations(SimulationFixture& fixture, Texts& stored, Counts& counts)
{
    Texts lines;
    for (std::size_t index = 0; index < fixture.document().getSimulationCount(); index++)
    {
        Simulation& simulation = *fixture.document().getSimulation(index);
        stored.emplace_back(name(simulation.getStoredStatus()));
        simulation.syncModId();
        lines.push_back(ascii(
            std::format("sim[{}] '{}' status={} fcid={} hasSimData={} hasSummary={} seedFixed={}",
                        index, simulation.getName(), name(simulation.getStatus()),
                        simulation.getId().toShortKey(), simulation.hasSimulationData(),
                        simulation.hasSummaryData(), simulation.getOptions().isRandomSeedFixed())));
        for (const std::shared_ptr<SimulationExtension>& extension :
             simulation.getSimulationExtensions())
        {
            lines.push_back(ascii(
                std::format("  ext id={} name={}", extension->getId(), extension->getName())));
        }
        counts.simulations++;
        const std::shared_ptr<FlightData>& data = simulation.getSimulatedData();
        if (data == nullptr)
        {
            continue;
        }
        describeData(*data, lines);
        counts.warnings += static_cast<int>(data->getWarningSet().size());
        counts.branches += static_cast<int>(data->getBranchCount());
        for (std::size_t branch = 0; branch < data->getBranchCount(); branch++)
        {
            counts.rows += static_cast<int>(data->getBranch(branch).getLength());
            counts.events += static_cast<int>(data->getBranch(branch).getEvents().size());
        }
    }
    return lines;
}

/// The name of the test of an example: the letters and digits of its file name.
[[nodiscard]] std::string exampleTestName(const ::testing::TestParamInfo<ExampleSimulations>& info)
{
    return QtRocket::Test::conditionsTestName(
        info.param.file.substr(0, info.param.file.rfind('.')));
}

class ExampleSimulationElements : public ::testing::TestWithParam<ExampleSimulations>
{
private:
    /// The name of an AirStart extension holds an altitude and a velocity in the default units.
    DefaultUnitsGuard m_units;
};

// The <simulations> element of an example loads without a warning, and its simulations are
// what OpenRocket's loader makes of the file.
TEST_P(ExampleSimulationElements, LoadAsInOpenRocket)
{
    const ExampleSimulations& example  = GetParam();
    const std::string         document = readExampleDocument(example.file);
    const std::string_view    element  = simulationsElement(document);
    ASSERT_FALSE(element.empty());

    // A rocket with the flight configurations of the file, each with a motor in the fixture's
    // body tube, so that a simulation of it can run (else its status would be CANT_RUN).
    SimulationFixture              fixture;
    const std::vector<std::string> configurations = motorConfigurationIds(document);
    ASSERT_FALSE(configurations.empty());
    for (const std::string& id : configurations)
    {
        const FlightConfigurationId fcid = FlightConfigurationId::fromString(id);
        fixture.rocket().createFlightConfiguration(fcid);
        QtRocket::Test::addMotor(fixture.tube(), fcid,
                                 QtRocket::Test::makeEmbeddedTestMotor("ZZ99", 10.0, "digest"));
    }
    const std::size_t configurationsBefore = fixture.rocket().getIds().size();

    SimulationsHandler handler(fixture.context());
    const HandlerRun   run = runHandler(handler, element);
    ASSERT_TRUE(run.result.has_value()) << run.result.error().message;
    EXPECT_EQ(run.texts(), Texts{});
    EXPECT_EQ(run.element, "simulations");
    EXPECT_TRUE(run.attributes.empty());
    // Every simulation names a configuration the file defines: none was made for one.
    EXPECT_EQ(fixture.rocket().getIds().size(), configurationsBefore);

    Texts       stored;
    Counts      counts;
    const Texts lines = describeSimulations(fixture, stored, counts);
    EXPECT_EQ(lines, Texts(example.lines.begin(), example.lines.end()));
    EXPECT_EQ(stored, Texts(static_cast<std::size_t>(example.simulations), "LOADED"));
    EXPECT_TRUE(counts == (Counts{.simulations = example.simulations,
                                  .branches    = example.branches,
                                  .rows        = example.rows,
                                  .events      = example.events,
                                  .warnings    = example.warnings}))
        << counts.simulations << " simulations, " << counts.branches << " branches, " << counts.rows
        << " rows, " << counts.events << " events, " << counts.warnings << " warnings";
}

INSTANTIATE_TEST_SUITE_P(Examples, ExampleSimulationElements,
                         ::testing::ValuesIn(kExampleSimulations), exampleTestName);

/// The sum of what the examples of ExampleSimulations.h hold.
[[nodiscard]] Counts totalCounts()
{
    Counts total;
    for (const ExampleSimulations& example : kExampleSimulations)
    {
        total.simulations += example.simulations;
        total.branches += example.branches;
        total.rows += example.rows;
        total.events += example.events;
        total.warnings += example.warnings;
    }
    return total;
}

// What OpenRocket loads from the sixteen examples altogether (the tier 9 scout's count): 54
// simulations with 69 branches of 36,962 rows, 735 events and 20 warnings. Every example above
// is held to its share of it.
TEST(ExampleSimulationTotals, TheExamplesHoldWhatOpenRocketCounts)
{
    EXPECT_EQ(kExampleSimulations.size(), 16U);
    EXPECT_TRUE(
        totalCounts() ==
        (Counts{.simulations = 54, .branches = 69, .rows = 36962, .events = 735, .warnings = 20}));
}

}  // namespace
