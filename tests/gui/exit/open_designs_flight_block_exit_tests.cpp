// The exit suite of the design view's flight block (plan section 11.6's E5, the tests-exit
// scout's E5): every design of OpenDesignsExitGolden (exit/OpenDesigns.h: OpenRocket's sixteen
// examples, the 49 re-saves of its golden harness and the GUI reference) opened through File > Open
// of a New design's window, as that suite opens it (exit/OpenDesignWindows.h), but with the design
// view's background estimate on (the preference backgroundFlight, OpenRocket's default, which the
// other suites turn off: O-M4-12) and AutoRunSimulations off, the Design tab the current one,
// shows the flight block of OpenRocket's RocketPanel for the same file (RocketInfoFlightPins.h:
// the rocket-info family's RocketInfoFlightProbe, RocketPanel.updateExtras() replayed for the
// design just opened, with RocketInfo painted), compared through RocketInfoFlightPinChecks.h:
// - as opened, which simulations the estimate runs (updateSims(false): the first simulation of
//   the selected configuration that is not up to date), whether it is calculating, and the
//   overlay's state and flight block: its texts with their fonts, colours, rows and columns, and
//   the same texts painted in the window's figure;
// - 62 designs show their stored data at once, as OpenRocket shows a simulation whose status is
//   UPTODATE, LOADED or EXTERNAL without a run (RocketPanel.java:1645, :1669), or "N/A" (no motors,
//   no simulation of the configuration), and run nothing: no job once the debounce is over, the
//   design still saved;
// - the re-saves of four test rockets (beta, Falcon 9 Heavy, the multi-stage event rocket, the
//   cluster pods), whose simulation of the selected configuration was never simulated, show
//   "Calculating..." and the block's labels with "N/A", dimmed, while the estimate's job waits in
//   the tests' executor (ManualSimulationExecutor, STEPPED); OpenDesignsFlightBlockRunExitGolden
//   then runs the job and compares the window with OpenRocket's state after its worker's run (the
//   pins' run, the probe's runEstimate()): the design unsaved, as Java's adoption of the run makes
//   it, with nothing to undo or redo; the simulation's status (ABORTED for all but beta, with the
//   status line above the block); the overlay not calculating and showing the simulation's new
//   data in the active colour; the Simulations tab's status of the simulation;
// - beta's estimate runs on the application's executor too (PollingSimulationExecutor, on a
//   thread of its own: OpenDesignsFlightBlockThreadsTest, one of the threaded suites of
//   tests/gui/CMakeLists.txt, which carry ctest's TIMEOUT; run under the tsan preset at a merge),
//   with the same result.
// No value of a fresh flight is compared with OpenRocket's (CLAUDE.md, Portability: a flight at the
// default time step is chaotic): the pins leave the apogee, the maximum velocity and the maximum
// acceleration of the run out, and the block must show the maxima of the data the simulation
// adopted, whatever they are. The fresh runs are those the plan's E5 names, of the four designs
// whose estimate OpenRocket runs when they are opened (plan 11.6 has the other suites fly only the
// three short examples); each takes a fraction of a second (below).
//
// The windows' environment is OpenDesignsExitGolden's (DesignFileEnvironment with the six
// component presets of the example designs, over the bundled motors or the seven synthetic ones of
// the test rockets' re-saves; OpenRocket's built-in materials; the scripted questions, of which
// only File > Open's file chooser is answered: no other question may come).
//
// Each test of a design opens it anew, so that ctest runs them as separate processes in parallel:
// 66 + 4 tests and the threaded one. In a debug build's run of every test a design took 0.25 to
// 2.4 s (38 s of processes for the 66), a run 0.27 to 0.6 s and the threaded test 0.6 s; in the
// asan preset's run of every test 0.9 to 3.5 s (121 s), 1 to 2.5 s and 2.5 s (all 66 in one debug
// process: 11 s, the four runs 0.8 s).

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include <QColor>
#include <QLatin1Char>
#include <QObject>
#include <QPoint>
#include <QRegion>
#include <QString>
#include <QWidget>
#include <QtCore/qtestsupport_core.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "ManualSimulationExecutor.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/figure/BackgroundSimulation.h"
#include "QtRocket/gui/figure/DesignPhysics.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RocketInfo.h"
#include "QtRocket/gui/figure/RocketPanel.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/simulation/SimulationPanel.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/PollingSimulationExecutor.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationExecutor.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/FileIo.h"
#include "ScopedApplicationPalette.h"
#include "ScriptedUserPrompts.h"
#include "TestPaths.h"
#include "exit/OpenDesignWindows.h"
#include "exit/OpenDesigns.h"
#include "figure/RecordingPaintDevice.h"
#include "figure/RocketInfoFlightPinChecks.h"
#include "figure/RocketInfoFlightPins.h"
#include "file/DesignFileEnvironment.h"

namespace QtRocket::Gui
{

namespace
{

using Gui::Test::FlightPinDocumentState;
using Gui::Test::FlightPinOverlay;
using Gui::Test::FlightPinRanSimulation;
using Gui::Test::FlightPinRun;
using Gui::Test::ManualSimulationExecutor;
using Gui::Test::OpenDesign;
using Gui::Test::RocketInfoFlightPin;
using QtRocket::Test::DesignFileEnvironment;

/// How long a wait for the application's executor goes on before it gives up and fails the test:
/// a safety net that only a defect reaches (beta's flight takes well under a second, a few under
/// the sanitizers).
constexpr int kSafetyNetMilliseconds = 60000;

/// Which executor the windows run their simulations on.
enum class Executor
{
    /// The tests' (ManualSimulationExecutor, STEPPED): a job runs when the test says.
    TESTS,
    /// The application's (PollingSimulationExecutor, with one thread).
    APPLICATIONS,
};

/// The executor of the windows for @p executor.
[[nodiscard]] std::unique_ptr<SimulationExecutor> makeExecutor(Executor executor)
{
    if (executor == Executor::TESTS)
    {
        return std::make_unique<ManualSimulationExecutor>(ManualSimulationExecutor::Mode::STEPPED);
    }
    return std::make_unique<PollingSimulationExecutor>(1U);
}

/// The services of the windows (see the top of the file): a DesignFileEnvironment with the six
/// component presets of the example designs over @p motors, OpenRocket's built-in materials, the
/// environment's presets, the scripted questions, the simulations run on @p executor, and the
/// design view's estimate on.
class FlightBlockServices
{
public:
    FlightBlockServices(const ThrustCurveMotorSetDatabase& motors, Executor executor)
      : m_environment(DesignFileEnvironment::Presets::EXAMPLES, motors),
        m_executor(makeExecutor(executor)),
        m_services(m_environment.preferences(), m_materials, motors, m_environment.presets(),
                   m_environment.context(), m_prompts)
    {
        addBuiltinMaterials(m_materials);
        // OpenRocket's default, which the services of the other suites turn off (O-M4-12).
        m_environment.preferences().putBoolean(GuiPreferenceKeys::kBackgroundFlight, true);
        m_services.setSimulationExecutor(m_executor.get());
    }

    [[nodiscard]] const GuiServices&              services() const noexcept { return m_services; }
    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() noexcept { return m_prompts; }
    [[nodiscard]] const Preferences&              preferences() const noexcept
    {
        return m_environment.preferences();
    }
    /// The tests' executor (Executor::TESTS).
    [[nodiscard]] ManualSimulationExecutor& manual() const
    {
        auto* const manual = dynamic_cast<ManualSimulationExecutor*>(m_executor.get());
        QTROCKET_ASSERT(manual != nullptr);
        return *manual;
    }

private:
    DesignFileEnvironment               m_environment;
    MaterialStorage                     m_materials;
    Gui::Test::ScriptedUserPrompts      m_prompts;
    std::unique_ptr<SimulationExecutor> m_executor;
    GuiServices                         m_services;
};

/// The design view of @p window (every design window has one).
[[nodiscard]] RocketPanel& rocketPanelOf(const MainWindow& window)
{
    RocketPanel* const panel = window.componentRocketPanel();
    QTROCKET_ASSERT(panel != nullptr);
    return *panel;
}

/// The design view's estimate of the flight of @p window.
[[nodiscard]] BackgroundSimulation& estimateOf(const MainWindow& window)
{
    return rocketPanelOf(window).backgroundSimulation();
}

/// The overlay of the design view of @p window.
[[nodiscard]] RocketInfo& overlayOf(const MainWindow& window)
{
    return rocketPanelOf(window).physics().info();
}

/// Waits in the event loop until the debounce interval of @p estimate is over (no start waits for
/// it), at most the safety net; whether it is.
[[nodiscard]] bool waitForTheDebounce(const BackgroundSimulation& estimate)
{
    return QTest::qWaitFor([&estimate] { return !estimate.isStartPending(); },
                           kSafetyNetMilliseconds);
}

/// Waits in the event loop until @p estimate calculates no more, at most the safety net; whether
/// it does not.
[[nodiscard]] bool waitUntilEstimated(const BackgroundSimulation& estimate)
{
    return QTest::qWaitFor([&estimate] { return !estimate.isCalculating(); },
                           kSafetyNetMilliseconds);
}

/// The texts @p figure paints, each with its pen's colour, as "<text> <#aarrggbb>".
[[nodiscard]] std::vector<std::string> paintedTexts(RocketFigure& figure)
{
    Gui::Test::RecordingPaintDevice device(figure.size());
    figure.render(&device, QPoint(), QRegion(figure.rect()), QWidget::RenderFlags());
    std::vector<std::string> texts;
    for (const Gui::Test::RecordedPaint& record : Gui::Test::joinTextRuns(device.records()))
    {
        if (record.kind == Gui::Test::RecordedPaint::Kind::TEXT)
        {
            texts.push_back(QtStrings::toUtf8(record.text + QLatin1Char(' ') +
                                              record.pen.color().name(QColor::HexArgb)));
        }
    }
    return texts;
}

/// Expects the window's figure of @p panel to paint every text of the flight block of its
/// overlay, each in its colour.
void expectFlightBlockPainted(const RocketPanel& panel)
{
    const std::vector<std::string> painted = paintedTexts(panel.figure());
    for (const RocketInfo::Item& item : Gui::Test::flightBlockItemsOf(panel.physics().info()))
    {
        const std::string text =
            QtStrings::toUtf8(item.text + QLatin1Char(' ') + item.colour.name(QColor::HexArgb));
        EXPECT_TRUE(std::ranges::find(painted, text) != painted.end()) << "not painted: " << text;
    }
}

/// Expects the document of @p window, and the window's modified mark, as @p state says.
void expectDocumentAsJava(const MainWindow& window, const FlightPinDocumentState& state)
{
    const OpenRocketDocument& document = window.document();
    EXPECT_EQ(document.isSaved(), state.saved);
    EXPECT_EQ(window.isWindowModified(), !state.saved);
    EXPECT_EQ(document.isUndoAvailable(), state.undoAvailable);
    EXPECT_EQ(document.isRedoAvailable(), state.redoAvailable);
}

/// Expects the overlay of @p window to show @p overlay (OpenRocket's, of @p pin's design) with the
/// shown simulation's maxima @p shownMaxima, its flight block drawn as OpenRocket draws it in the
/// fonts of @p preferences, and painted in the window's figure.
void expectOverlayAsJava(const MainWindow& window, const RocketInfoFlightPin& pin,
                         const FlightPinOverlay&                     overlay,
                         const std::optional<std::array<double, 4>>& shownMaxima,
                         const Preferences&                          preferences)
{
    RocketInfo& info = overlayOf(window);
    Gui::Test::expectFlightOverlayAsJava(info, pin.configurationName, overlay, shownMaxima);
    Gui::Test::expectFlightBlockAsJava(info, preferences, overlay.texts);
    expectFlightBlockPainted(rocketPanelOf(window));
}

/// Expects the estimate of @p window and its overlay to be OpenRocket's for @p pin's design just
/// opened: the simulations it runs, calculating or not, the overlay with the stored maxima of the
/// shown simulation (preferences: @p preferences).
void expectEstimateAsOpened(const MainWindow& window, const RocketInfoFlightPin& pin,
                            const Preferences& preferences)
{
    const BackgroundSimulation& estimate = estimateOf(window);
    EXPECT_EQ(Gui::Test::scheduledFlightPinSimulations(estimate, window.document()),
              pin.runsCurrent);
    EXPECT_EQ(estimate.isCalculating(), pin.overlay.calculating);
    expectOverlayAsJava(window, pin, pin.overlay, Gui::Test::shownFlightPinMaxima(pin),
                        preferences);
}

/// Expects the jobs @p executor was given to be those of @p window's estimate for @p pin's design
/// once its debounce is over: one, of the window, running the first simulation updateSims(false)
/// found, when the estimate runs one, else none.
void expectTheEstimatesJob(const ManualSimulationExecutor& executor, const MainWindow& window,
                           const RocketInfoFlightPin& pin)
{
    const BackgroundSimulation&                                estimate = estimateOf(window);
    const std::vector<ManualSimulationExecutor::SubmittedJob>& jobs     = executor.submittedJobs();
    EXPECT_EQ(jobs.size(), pin.runsCurrent.empty() ? 0U : 1U);
    EXPECT_TRUE(
        std::ranges::all_of(jobs, [&window](const ManualSimulationExecutor::SubmittedJob& job) {
            return job.window == static_cast<const QObject*>(&window);
        }));
    EXPECT_EQ(estimate.job() != SimulationExecutor::kNoJob, !pin.runsCurrent.empty());
    EXPECT_EQ(executor.waitingJobs().size(), jobs.size());
}

/// The maxima of the data of the simulation @p overlay shows of @p document (the data it adopted),
/// or none.
[[nodiscard]] std::optional<std::array<double, 4>> adoptedMaxima(const OpenRocketDocument& document,
                                                                 const FlightPinOverlay&   overlay)
{
    if (!overlay.shownSimulation.has_value() ||
        *overlay.shownSimulation >= document.getSimulationCount())
    {
        return std::nullopt;
    }
    const std::shared_ptr<FlightData> data =
        document.getSimulation(*overlay.shownSimulation)->getSimulatedData();
    if (data == nullptr)
    {
        return std::nullopt;
    }
    return Gui::Test::flightPinMaximaOf(RocketInfo::FlightSummary::of(*data));
}

/// Expects the simulation @p ran of @p window's design to have OpenRocket's status and data after
/// its run, and the Simulations tab to show its status.
void expectRanAsJava(const MainWindow& window, const FlightPinRanSimulation& ran)
{
    SCOPED_TRACE(ran.index);
    const OpenRocketDocument& document = window.document();
    ASSERT_LT(ran.index, document.getSimulationCount());
    const std::shared_ptr<Simulation> simulation = document.getSimulation(ran.index);
    EXPECT_EQ(std::string(name(simulation->getStatus())), ran.status);
    EXPECT_EQ(simulation->hasSummaryData(), ran.hasData);
    const SimulationPanel* const tab = window.componentSimulationPanel();
    ASSERT_NE(tab, nullptr);
    const SimulationTableModel& model = tab->tableModel();
    const int                   row   = model.rowOf(*simulation);
    ASSERT_GE(row, 0);
    EXPECT_EQ(model.status(row), simulation->getStatus());
}

/// OpenRocket's run of the estimate of @p pin's design (a test failure, and an empty run, without
/// one).
[[nodiscard]] FlightPinRun runOf(const RocketInfoFlightPin& pin)
{
    if (!pin.run.has_value())
    {
        ADD_FAILURE() << "no run of " << pin.name;
        return {};
    }
    return *pin.run;
}

/// Expects @p window, whose estimate has run, to be OpenRocket's after its worker's run of @p pin's
/// design: the document as Java's adoption leaves it, each simulation's status and data, the
/// estimate done, and the overlay with the adopted data's maxima (preferences: @p preferences).
void expectAdoptedAsJava(const MainWindow& window, const RocketInfoFlightPin& pin,
                         const Preferences& preferences)
{
    const FlightPinRun run = runOf(pin);
    EXPECT_TRUE(run.succeeded) << run.failure;
    expectDocumentAsJava(window, run.after);
    for (const FlightPinRanSimulation& ran : run.ran)
    {
        expectRanAsJava(window, ran);
    }
    const BackgroundSimulation& estimate = estimateOf(window);
    EXPECT_FALSE(estimate.isCalculating());
    EXPECT_EQ(estimate.job(), SimulationExecutor::kNoJob);
    EXPECT_TRUE(estimate.chain().empty());
    EXPECT_FALSE(estimate.isStartPending());
    expectOverlayAsJava(window, pin, run.overlay, adoptedMaxima(window.document(), run.overlay),
                        preferences);
}

/// The designs of the suite whose estimate runs when they are opened (their pins hold a run), in
/// the suite's order.
[[nodiscard]] std::vector<OpenDesign> designsWhoseEstimateRuns()
{
    std::vector<OpenDesign> designs;
    std::ranges::copy_if(
        Gui::Test::openDesigns(), std::back_inserter(designs), [](const OpenDesign& design) {
            const RocketInfoFlightPin* const pin = Gui::Test::findRocketInfoFlightPin(design.name);
            return pin != nullptr && pin->run.has_value();
        });
    return designs;
}

/// The names of @p designs, in their order.
[[nodiscard]] std::vector<std::string> namesOf(const std::vector<OpenDesign>& designs)
{
    std::vector<std::string> names;
    std::ranges::transform(designs, std::back_inserter(names), &OpenDesign::name);
    return names;
}

/// Whether @p pin is OpenRocket's flight block of @p design as updateExtras() leaves it for the
/// design just opened: an estimated scenario of the design's kind and file (relative to the
/// repository), in metric units, with the file's selected configuration.
[[nodiscard]] bool isOpenedDesignPin(const RocketInfoFlightPin& pin, const OpenDesign& design)
{
    std::error_code error;
    return pin.estimated && !pin.imperial && !pin.selectConfigurationOf.has_value() &&
           pin.kind == Gui::Test::openDesignKind(design.source) &&
           std::filesystem::equivalent(
               QtRocket::Test::dataDir().parent_path() / pathFromUtf8(pin.file), design.file,
               error);
}

/// The designs of the suite without a pinned flight block as opened (isOpenedDesignPin()), by
/// name.
[[nodiscard]] std::vector<std::string> designsWithoutTheirPins()
{
    std::vector<std::string> without;
    for (const OpenDesign& design : Gui::Test::openDesigns())
    {
        const RocketInfoFlightPin* const pin = Gui::Test::findRocketInfoFlightPin(design.name);
        if (pin == nullptr || !isOpenedDesignPin(*pin, design))
        {
            without.push_back(design.name);
        }
    }
    return without;
}

/// The windows of the application with the estimate on, a design of the suite opened in them
/// (open()).
class FlightBlockWindowTest : public Gui::Test::GuiTest
{
protected:
    void TearDown() override
    {
        // The windows (with their documents and design views, which abandon their jobs) go before
        // the services and their executor.
        m_windows  = nullptr;
        m_services = nullptr;
        GuiTest::TearDown();
    }

    /// Opens @p design through File > Open of a New design's window, in windows whose simulations
    /// run on @p executor, with the estimate on, and returns the window it opened in; null (with a
    /// test failure) when none opened.
    MainWindow* open(const OpenDesign& design, Executor executor)
    {
        m_services =
            std::make_unique<FlightBlockServices>(Gui::Test::openDesignMotors(design), executor);
        m_windows = std::make_unique<Gui::Test::OpenDesignWindows>(m_services->services());
        MainWindow* const window =
            m_windows->openThroughFileOpen(design.file, m_services->prompts());
        m_askedOnOpening = m_services->prompts().prompts().size();
        return window;
    }

    [[nodiscard]] const Preferences& preferences() const { return m_services->preferences(); }
    [[nodiscard]] ManualSimulationExecutor& manual() const { return m_services->manual(); }
    /// The questions asked since the design was opened (none may be).
    [[nodiscard]] std::size_t askedSinceOpening() const
    {
        return m_services->prompts().prompts().size() - m_askedOnOpening;
    }

private:
    std::unique_ptr<FlightBlockServices>          m_services;
    std::unique_ptr<Gui::Test::OpenDesignWindows> m_windows;
    std::size_t                                   m_askedOnOpening{0};
};

/// The suite: the flight block of each design of OpenDesignsExitGolden.
class OpenDesignsFlightBlockExitGolden : public FlightBlockWindowTest,
                                         public ::testing::WithParamInterface<OpenDesign>
{ };

/// The runs of the four designs whose estimate runs.
class OpenDesignsFlightBlockRunExitGolden : public FlightBlockWindowTest,
                                            public ::testing::WithParamInterface<OpenDesign>
{ };

/// Beta's estimate on the application's executor.
using OpenDesignsFlightBlockThreadsTest = FlightBlockWindowTest;

// The pins hold the flight block of every design of the suite as OpenRocket shows it when the
// design is opened, and the run of the estimate of the four designs that run it (plan 11.6's E5).
TEST(OpenDesignsFlightBlockExitGoldenDesigns, HaveOpenRocketsFlightBlockEach)
{
    EXPECT_EQ(Gui::Test::rocketInfoFlightPins().problem, "");
    EXPECT_EQ(Gui::Test::openDesigns().size(), 66U);
    EXPECT_EQ(designsWithoutTheirPins(), std::vector<std::string>{});
    EXPECT_EQ(namesOf(designsWhoseEstimateRuns()),
              (std::vector<std::string>{"testrocket_beta", "testrocket_falcon_9_heavy",
                                        "testrocket_multi_stage_event_test_rocket",
                                        "testrocket_cluster_pods"}));
}

TEST_P(OpenDesignsFlightBlockExitGolden, ShowsOpenRocketsFlightBlockWithTheEstimateOn)
{
    const OpenDesign&                         design = GetParam();
    const RocketInfoFlightPin* const          pin = Gui::Test::findRocketInfoFlightPin(design.name);
    const Gui::Test::ScopedApplicationPalette light(Gui::Test::ScopedApplicationPalette::light());
    ASSERT_NE(pin, nullptr);
    const MainWindow* const window = open(design, Executor::TESTS);
    ASSERT_NE(window, nullptr);
    BackgroundSimulation& estimate = estimateOf(*window);
    estimate.setDebounceInterval(std::chrono::milliseconds(0));

    // As opened (updateExtras() of the window's design view, the Design tab current,
    // AutoRunSimulations off): the design saved, the estimate's runs and the overlay OpenRocket's.
    EXPECT_FALSE(preferences().getAutoRunSimulations());
    EXPECT_FALSE(estimate.isSimulationsTabSelected());
    EXPECT_FALSE(estimate.isPaused());
    expectDocumentAsJava(*window, {.saved = true, .undoAvailable = false, .redoAvailable = false});
    expectEstimateAsOpened(*window, *pin, preferences());

    // Once the debounce is over, the estimate's job, if any, waits in the executor; the overlay
    // and the design are as they were.
    EXPECT_TRUE(waitForTheDebounce(estimate));
    expectTheEstimatesJob(manual(), *window, *pin);
    expectEstimateAsOpened(*window, *pin, preferences());
    expectDocumentAsJava(*window, {.saved = true, .undoAvailable = false, .redoAvailable = false});
    EXPECT_EQ(askedSinceOpening(), 0U);
}

TEST_P(OpenDesignsFlightBlockRunExitGolden, AdoptsTheEstimatesRunAsOpenRocketDoes)
{
    const OpenDesign&                         design = GetParam();
    const RocketInfoFlightPin* const          pin = Gui::Test::findRocketInfoFlightPin(design.name);
    const Gui::Test::ScopedApplicationPalette light(Gui::Test::ScopedApplicationPalette::light());
    ASSERT_NE(pin, nullptr);
    ASSERT_TRUE(pin->run.has_value());
    const MainWindow* const window = open(design, Executor::TESTS);
    ASSERT_NE(window, nullptr);
    BackgroundSimulation& estimate = estimateOf(*window);
    estimate.setDebounceInterval(std::chrono::milliseconds(0));
    EXPECT_TRUE(waitForTheDebounce(estimate));
    // The design just opened (OpenRocket's before its run), the estimate's job waiting.
    expectDocumentAsJava(*window, runOf(*pin).before);
    ASSERT_EQ(manual().waitingJobs().size(), 1U);

    // The job runs (on the GUI thread, as a thread of the application's executor would run it) and
    // its run is adopted: the window is OpenRocket's after its worker's run, and nothing more runs.
    EXPECT_TRUE(manual().runNext());
    expectAdoptedAsJava(*window, *pin, preferences());
    EXPECT_EQ(manual().runAll(), 0U);
    EXPECT_EQ(manual().submittedJobs().size(), 1U);
    EXPECT_EQ(askedSinceOpening(), 0U);
}

TEST_F(OpenDesignsFlightBlockThreadsTest, BetasEstimateRunsOnTheApplicationsExecutorAsOpenRockets)
{
    // The design view's estimate of a window opened through File > Open, on the application's
    // executor: its run goes on a thread of the executor, and is adopted on the GUI thread as the
    // tests' executor's is.
    const OpenDesign* const          design = Gui::Test::findOpenDesign("testrocket_beta");
    const RocketInfoFlightPin* const pin    = Gui::Test::findRocketInfoFlightPin("testrocket_beta");
    const Gui::Test::ScopedApplicationPalette light(Gui::Test::ScopedApplicationPalette::light());
    ASSERT_NE(design, nullptr);
    ASSERT_NE(pin, nullptr);
    const MainWindow* const window = open(*design, Executor::APPLICATIONS);
    ASSERT_NE(window, nullptr);
    BackgroundSimulation& estimate = estimateOf(*window);
    estimate.setDebounceInterval(std::chrono::milliseconds(0));
    EXPECT_TRUE(waitUntilEstimated(estimate));
    expectAdoptedAsJava(*window, *pin, preferences());
    EXPECT_EQ(askedSinceOpening(), 0U);
}

INSTANTIATE_TEST_SUITE_P(Designs, OpenDesignsFlightBlockExitGolden,
                         ::testing::ValuesIn(Gui::Test::openDesigns()),
                         Gui::Test::openDesignTestName);
INSTANTIATE_TEST_SUITE_P(EstimateRuns, OpenDesignsFlightBlockRunExitGolden,
                         ::testing::ValuesIn(designsWhoseEstimateRuns()),
                         Gui::Test::openDesignTestName);

}  // namespace

}  // namespace QtRocket::Gui
