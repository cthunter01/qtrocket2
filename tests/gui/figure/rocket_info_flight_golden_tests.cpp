// The flight block of the design view's overlay against OpenRocket's (RocketInfoFlightPins.h:
// tests/data/goldens/gui/rocket-info/flight-blocks.json, the rocket-info family's
// RocketInfoFlightProbe): for each design of OpenDesignsExitGolden and the probe's own states,
// the design as loaded (the configuration's name, every simulation's status and maxima), what
// BackgroundSimulation makes of it when it is opened with the estimate on (which simulations it
// would run, with AutoRunSimulations off and on with the Simulations tab selected; what the
// overlay shows; calculating or not), and what RocketInfo then draws of the block: its texts, in
// which font and colour, on which row and in which column (RocketInfoFlightPinChecks.h, which
// the exit suite of the flight block shares). Nothing runs: the runs wait in the tests' executor
// (no value of a fresh flight is pinned; plan 11.6). The runs of the four designs whose estimate
// runs, which the pins hold too, are compared in their windows
// (exit/open_designs_flight_block_exit_tests.cpp).

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "ManualSimulationExecutor.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/figure/BackgroundSimulation.h"
#include "QtRocket/gui/figure/DesignPhysics.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RocketInfo.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationStatusText.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "RocketInfoFlightPinChecks.h"
#include "RocketInfoFlightPins.h"
#include "ScopedApplicationPalette.h"
#include "TestPaths.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenResaves.h"
#include "motor/TestMotorDatabase.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using Gui::Test::FlightPinRun;
using Gui::Test::FlightPinShown;
using Gui::Test::FlightPinSimulation;
using Gui::Test::ManualSimulationExecutor;
using Gui::Test::RocketInfoFlightPin;
using QtRocket::Test::DesignFileEnvironment;

/// A scenario's design, loaded as the probe loaded it (the six presets of the example designs;
/// the bundled motors, or the seven of TestRockets for the test rockets' re-saves; motors named by
/// their common name), in the probe's units, with the configuration it selected, shown in a
/// RocketFigure with its DesignPhysics.
class FlightScenario
{
public:
    explicit FlightScenario(const RocketInfoFlightPin& pin)
      : m_environment(DesignFileEnvironment::Presets::EXAMPLES,
                      pin.kind == "testrocket" ? QtRocket::Test::testRocketMotors()
                                               : QtRocket::Test::bundledMotorDatabase())
    {
        if (pin.imperial)
        {
            m_units.emplace();
            UnitGroup::setDefaultImperialUnits();
        }
        Result<LoadedDocument> loaded =
            GeneralRocketLoader(m_environment.context())
                .load(QtRocket::Test::dataDir().parent_path() / pathFromUtf8(pin.file));
        if (!loaded.has_value())
        {
            ADD_FAILURE() << "cannot load " << pin.file << ": " << loaded.error().message;
            m_document = OpenRocketDocumentFactory::createNewRocket();
        }
        else
        {
            m_document = std::move(loaded->document);
        }
        if (pin.selectConfigurationOf.has_value() &&
            *pin.selectConfigurationOf < m_document->getSimulationCount())
        {
            m_document->getRocket().setSelectedConfiguration(
                m_document->getSimulation(*pin.selectConfigurationOf)->getFlightConfigurationId());
        }
        m_figure =
            std::make_unique<RocketFigure>(m_document->getRocket(), preferences(), 96.0, nullptr);
        m_physics = std::make_unique<DesignPhysics>(*m_figure, preferences());
    }

    ~FlightScenario()
    {
        m_physics.reset();
        m_figure.reset();
    }
    FlightScenario(const FlightScenario&)            = delete;
    FlightScenario& operator=(const FlightScenario&) = delete;
    FlightScenario(FlightScenario&&)                 = delete;
    FlightScenario& operator=(FlightScenario&&)      = delete;

    [[nodiscard]] OpenRocketDocument&  document() const noexcept { return *m_document; }
    [[nodiscard]] DesignPhysics&       physics() const noexcept { return *m_physics; }
    [[nodiscard]] RocketInfo&          info() const noexcept { return m_physics->info(); }
    [[nodiscard]] InMemoryPreferences& preferences() noexcept
    {
        return m_environment.preferences();
    }

private:
    std::optional<QtRocket::Test::DefaultUnitsGuard> m_units;
    DesignFileEnvironment                            m_environment;
    std::unique_ptr<OpenRocketDocument>              m_document;
    std::unique_ptr<RocketFigure>                    m_figure;
    std::unique_ptr<DesignPhysics>                   m_physics;
};

/// Compares the simulation @p simulation of @p document with @p pin.
void expectSimulationAsJava(OpenRocketDocument& document, Simulation& simulation,
                            const FlightPinSimulation& pin)
{
    SCOPED_TRACE(pin.name);
    EXPECT_EQ(simulation.getName(), pin.name);
    EXPECT_EQ(std::string(name(simulation.getStatus())), pin.status);
    EXPECT_EQ(simulation.hasSummaryData(), pin.hasData);
    const Rocket& rocket = document.getRocket();
    EXPECT_EQ(simulation.getFlightConfigurationId() == rocket.getSelectedConfiguration().getId(),
              pin.selectedConfiguration);
    EXPECT_EQ(rocket.getFlightConfiguration(simulation.getFlightConfigurationId()).hasMotors(),
              pin.configurationHasMotors);
    if (pin.maxima.has_value() && simulation.getSimulatedData() != nullptr)
    {
        Gui::Test::expectFlightPinMaxima(
            RocketInfo::FlightSummary::of(*simulation.getSimulatedData()), *pin.maxima);
    }
}

/// Compares the state of @p info with @p pin's overlay (its stored maxima for the shown
/// simulation).
void expectOverlayAsJava(const RocketInfo& info, const RocketInfoFlightPin& pin)
{
    Gui::Test::expectFlightOverlayAsJava(info, pin.configurationName, pin.overlay,
                                         Gui::Test::shownFlightPinMaxima(pin));
}

/// Gives @p info @p pin's overlay state for @p document (as the probe set it on RocketInfo).
void setPinnedState(RocketInfo& info, const OpenRocketDocument& document,
                    const RocketInfoFlightPin& pin)
{
    std::shared_ptr<Simulation>       shown;
    const std::optional<std::size_t>& shownIndex = pin.overlay.shownSimulation;
    if (shownIndex.has_value() && *shownIndex < document.getSimulationCount())
    {
        shown = document.getSimulation(*shownIndex);
    }
    switch (pin.overlay.shown)
    {
        case FlightPinShown::NONE:
            info.setFlightData(std::nullopt);
            break;
        case FlightPinShown::NAN_DATA:
            info.setFlightData(RocketInfo::FlightSummary::notAvailable());
            break;
        case FlightPinShown::DATA:
            if (shown != nullptr && shown->getSimulatedData() != nullptr)
            {
                info.setFlightData(RocketInfo::FlightSummary::of(*shown->getSimulatedData()));
            }
            break;
    }
    info.setCalculatingData(pin.overlay.calculating);
    info.setAbortedStatus(shown != nullptr && shown->getStatus() == Simulation::Status::ABORTED
                              ? std::optional(SimulationStatusText::plainDescription(*shown))
                              : std::nullopt);
}

/// The scenarios whose overlay is the estimate's (BackgroundSimulationGolden).
[[nodiscard]] std::vector<RocketInfoFlightPin> estimatedScenarios()
{
    std::vector<RocketInfoFlightPin> scenarios;
    std::ranges::copy_if(Gui::Test::rocketInfoFlightPins().scenarios, std::back_inserter(scenarios),
                         [](const RocketInfoFlightPin& pin) { return pin.estimated; });
    return scenarios;
}

/// Compares every simulation of @p document with @p pin's.
void expectSimulationsAsJava(OpenRocketDocument& document, const RocketInfoFlightPin& pin)
{
    ASSERT_EQ(document.getSimulationCount(), pin.simulations.size());
    for (std::size_t index = 0; index < pin.simulations.size(); ++index)
    {
        expectSimulationAsJava(document, *document.getSimulation(index), pin.simulations.at(index));
    }
}

/// The designs of the suite (not the probe's own states) whose estimate runs a simulation.
[[nodiscard]] std::vector<std::string> designsWhoseEstimateRuns()
{
    std::vector<std::string> running;
    for (const RocketInfoFlightPin& pin : Gui::Test::rocketInfoFlightPins().scenarios)
    {
        if (!pin.name.starts_with("special:") && !pin.runsCurrent.empty())
        {
            running.push_back(pin.name);
        }
    }
    return running;
}

/// The scenarios that hold a run of the estimate, by name.
[[nodiscard]] std::vector<std::string> scenariosWithARun()
{
    std::vector<std::string> named;
    for (const RocketInfoFlightPin& pin : Gui::Test::rocketInfoFlightPins().scenarios)
    {
        if (pin.run.has_value())
        {
            named.push_back(pin.name);
        }
    }
    return named;
}

/// The indices of @p indices as one text ("1 2").
[[nodiscard]] std::string indicesText(const std::vector<std::size_t>& indices)
{
    std::string text;
    for (const std::size_t index : indices)
    {
        text += (text.empty() ? "" : " ") + std::to_string(index);
    }
    return text;
}

/// The saved state and the undo history @p state has, as one text ("saved", "unsaved undo", ...).
[[nodiscard]] std::string documentStateText(const Gui::Test::FlightPinDocumentState& state)
{
    return std::string(state.saved ? "saved" : "unsaved") + (state.undoAvailable ? " undo" : "") +
           (state.redoAvailable ? " redo" : "");
}

/// What the run of @p pin is, in one line: the simulations that ran to their end with data, whether
/// none failed, the document before and after, the overlay afterwards (what it shows, of which
/// simulation, calculating or not) and how many of its texts are the fresh flight's values.
[[nodiscard]] std::string runSummary(const RocketInfoFlightPin& pin)
{
    const FlightPinRun       run = pin.run.value_or(FlightPinRun{});
    std::vector<std::size_t> ran;
    for (const Gui::Test::FlightPinRanSimulation& simulation : run.ran)
    {
        if (simulation.hasData)
        {
            ran.push_back(simulation.index);
        }
    }
    const std::optional<std::size_t>& shown = run.overlay.shownSimulation;
    return "ran " + indicesText(ran) + (run.succeeded ? "" : " failed") + ", " +
           documentStateText(run.before) + " then " + documentStateText(run.after) + ", shows " +
           (run.overlay.shown == FlightPinShown::DATA ? "the data of " : "no data of ") +
           (shown.has_value() ? std::to_string(*shown) : std::string("none")) +
           (run.overlay.calculating ? " calculating" : "") + ", " +
           std::to_string(
               std::ranges::count_if(run.overlay.texts, &Gui::Test::FlightPinText::fresh)) +
           " fresh values";
}

/// What plan 11.6's E5 says of OpenRocket's run of @p pin's design: the simulation
/// updateSims(false) asked for ran to its end with data, the design just opened (saved, nothing to
/// undo) became unsaved without a step to undo or redo, and the overlay shows the data of the
/// simulation it showed, not calculating, in the texts of the block, of which three (the flight's
/// values) are not pinned.
[[nodiscard]] std::string plannedRunSummary(const RocketInfoFlightPin& pin)
{
    const std::optional<std::size_t>& shown = pin.overlay.shownSimulation;
    return "ran " + indicesText(pin.runsCurrent) + ", saved then unsaved, shows the data of " +
           (shown.has_value() ? std::to_string(*shown) : std::string("none")) + ", 3 fresh values";
}

/// Expects the run of @p pin to be what plan 11.6's E5 says of OpenRocket (plannedRunSummary()).
void expectRunAsPlanned(const RocketInfoFlightPin& pin)
{
    EXPECT_EQ(runSummary(pin), plannedRunSummary(pin)) << pin.name;
}

/// expectRunAsPlanned() for every scenario that holds a run.
void expectEveryRunAsPlanned()
{
    for (const RocketInfoFlightPin& pin : Gui::Test::rocketInfoFlightPins().scenarios)
    {
        if (pin.run.has_value())
        {
            expectRunAsPlanned(pin);
        }
    }
}

class RocketInfoFlightGolden : public Gui::Test::GuiTest,
                               public ::testing::WithParamInterface<RocketInfoFlightPin>
{ };

class BackgroundSimulationGolden : public Gui::Test::GuiTest,
                                   public ::testing::WithParamInterface<RocketInfoFlightPin>
{ };

TEST(RocketInfoFlightPinsGolden, AreReadAndComeFromTheGoldensOpenRocket)
{
    const Gui::Test::RocketInfoFlightPins& pins = Gui::Test::rocketInfoFlightPins();
    EXPECT_EQ(pins.problem, "");
    EXPECT_EQ(pins.schema, Gui::Test::kRocketInfoFlightSchemaVersion);
    EXPECT_EQ(pins.openRocketCommit, "5f164fd0e05d6e188857b0185601293b7a6e9eaf");
    // The 66 designs of OpenDesignsExitGolden and the probe's 7 states.
    EXPECT_EQ(pins.scenarios.size(), 73U);
    EXPECT_EQ(estimatedScenarios().size(), 68U);
}

TEST(RocketInfoFlightPinsGolden, FourOpenDesignsRunTheirEstimate)
{
    // Plan 11.6 E5: 62 of the 66 designs show their stored estimate at once; four test rockets'
    // re-saves (beta, Falcon 9 Heavy, the multi-stage event rocket, the cluster pods) have a
    // simulation of the selected configuration that is not up to date, which the estimate runs.
    const std::vector<std::string> running = designsWhoseEstimateRuns();
    EXPECT_EQ(running, (std::vector<std::string>{"testrocket_beta", "testrocket_falcon_9_heavy",
                                                 "testrocket_multi_stage_event_test_rocket",
                                                 "testrocket_cluster_pods"}));
}

TEST(RocketInfoFlightPinsGolden, HoldTheRunsOfTheFourEstimates)
{
    // The four designs whose estimate runs hold OpenRocket's run of it (the probe's
    // runEstimate(): the exit suite of the flight block compares their windows with it); the
    // probe's own states run nothing.
    EXPECT_EQ(scenariosWithARun(), designsWhoseEstimateRuns());
    expectEveryRunAsPlanned();
}

TEST_P(RocketInfoFlightGolden, LoadsTheDesignAsOpenRocket)
{
    const RocketInfoFlightPin& pin = GetParam();
    const FlightScenario       scenario(pin);
    OpenRocketDocument&        document = scenario.document();
    EXPECT_EQ(QtStrings::toUtf8(scenario.info().configurationName()), pin.configurationName);
    EXPECT_EQ(document.getRocket().getSelectedConfiguration().hasMotors(), pin.hasMotors);
    expectSimulationsAsJava(document, pin);
}

TEST_P(RocketInfoFlightGolden, PaintsTheFlightBlockAsOpenRocket)
{
    // The overlay in the state the probe gave OpenRocket's RocketInfo.
    const RocketInfoFlightPin&                pin = GetParam();
    const Gui::Test::ScopedApplicationPalette light(Gui::Test::ScopedApplicationPalette::light());
    FlightScenario                            scenario(pin);
    setPinnedState(scenario.info(), scenario.document(), pin);
    expectOverlayAsJava(scenario.info(), pin);
    Gui::Test::expectFlightBlockAsJava(scenario.info(), scenario.preferences(), pin.overlay.texts);
}

TEST_P(BackgroundSimulationGolden, EstimatesAsOpenRocket)
{
    // RocketPanel.updateExtras() for the design just opened, with the estimate on and
    // AutoRunSimulations off: the runs it asks for (they wait), the overlay, and what it draws.
    const RocketInfoFlightPin&                pin = GetParam();
    const Gui::Test::ScopedApplicationPalette light(Gui::Test::ScopedApplicationPalette::light());
    FlightScenario                            scenario(pin);
    ManualSimulationExecutor                  executor(ManualSimulationExecutor::Mode::NEVER);
    BackgroundSimulation                      background(scenario.physics(), scenario.document(),
                                                         scenario.preferences());
    background.setExecutor(&executor, nullptr);
    EXPECT_EQ(Gui::Test::scheduledFlightPinSimulations(background, scenario.document()),
              pin.runsCurrent);
    EXPECT_EQ(background.isCalculating(), pin.overlay.calculating);
    expectOverlayAsJava(scenario.info(), pin);
    Gui::Test::expectFlightBlockAsJava(scenario.info(), scenario.preferences(), pin.overlay.texts);
}

TEST_P(BackgroundSimulationGolden, RunsEverySimulationWithTheAutoRunOnTheSimulationsTab)
{
    // updateSims(true): with AutoRunSimulations on and the Simulations tab selected, every
    // simulation that is not up to date and has motors.
    const RocketInfoFlightPin& pin = GetParam();
    FlightScenario             scenario(pin);
    scenario.preferences().setAutoRunSimulations(true);
    ManualSimulationExecutor executor(ManualSimulationExecutor::Mode::NEVER);
    BackgroundSimulation     background(scenario.physics(), scenario.document(),
                                        scenario.preferences());
    background.setSimulationsTabSelected(true);
    background.setExecutor(&executor, nullptr);
    EXPECT_EQ(Gui::Test::scheduledFlightPinSimulations(background, scenario.document()),
              pin.runsAll);
}

/// The name of a scenario's tests.
[[nodiscard]] std::string scenarioTestName(
    const ::testing::TestParamInfo<RocketInfoFlightPin>& param)
{
    return param.param.testName;
}

INSTANTIATE_TEST_SUITE_P(Scenarios, RocketInfoFlightGolden,
                         ::testing::ValuesIn(Gui::Test::rocketInfoFlightPins().scenarios),
                         scenarioTestName);
INSTANTIATE_TEST_SUITE_P(Scenarios, BackgroundSimulationGolden,
                         ::testing::ValuesIn(estimatedScenarios()), scenarioTestName);

}  // namespace

}  // namespace QtRocket::Gui
