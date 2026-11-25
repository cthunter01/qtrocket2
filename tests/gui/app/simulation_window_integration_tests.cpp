// The simulations of the running application (ApplicationSession, as main() makes it, over a
// preferences file of the test's own, with the test's questions but for the run dialog, which is
// the application's own): what Milestone 4's run 3 connected between the Flight simulations tab,
// its window and the design view (plan 11.3, decision O-M4-16). A new simulation's settings
// dialog (SimulationConfigDialog, the application's SimulationDialogs) whose OK runs it through
// the run dialog (SimulationRunDialog), the table up to date afterwards; the design view's
// background estimate of a production window, paused while the settings dialog is open and
// cancelled for the simulations a run dialog runs; a window closed while a job of it waits or
// runs. On the tests' executor (ManualSimulationExecutor, given to the ApplicationContext: no
// thread; SimulationWindowIntegrationTest), and on the application's own
// (PollingSimulationExecutor, on its threads: SimulationWindowIntegrationThreadsTest, which
// carries ctest's TIMEOUT, tests/gui/CMakeLists.txt, and runs under the tsan preset at a merge).
// Fresh runs fly the configuration of "A simple model rocket" #1 only, and no value of a fresh
// flight is pinned (CLAUDE.md, Portability).

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>
#include <QtCore/qtestsupport_core.h>

#include <gtest/gtest.h>

#include "ForwardingUserPrompts.h"
#include "GuiTestSupport.h"
#include "ManualSimulationExecutor.h"
#include "NextModalDialog.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/app/Application.h"
#include "QtRocket/gui/app/ApplicationContext.h"
#include "QtRocket/gui/app/ApplicationSession.h"
#include "QtRocket/gui/app/DataPaths.h"
#include "QtRocket/gui/dialogs/simulation/SimulationConfigDialog.h"
#include "QtRocket/gui/dialogs/simulation/SimulationRunDialog.h"
#include "QtRocket/gui/figure/BackgroundSimulation.h"
#include "QtRocket/gui/figure/RocketPanel.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/WidgetUserPrompts.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/settings/SettingsPreferences.h"
#include "QtRocket/gui/simulation/SimulationPanel.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/simulation/SimulationTableView.h"
#include "QtRocket/gui/util/PollingSimulationExecutor.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationExecutor.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/IconButton.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "ScriptedUserPrompts.h"
#include "TestPaths.h"
#include "TestTempDir.h"
#include "simulation/SimulationRunnerSupport.h"

namespace QtRocket::Gui
{

namespace
{

using Gui::Test::ManualSimulationExecutor;
using Mode   = ManualSimulationExecutor::Mode;
using Status = Simulation::Status;

/// How long a wait for the application's executor goes on before it gives up and fails the test:
/// a safety net that only a defect reaches (a run of the short flight takes well under a second,
/// under the sanitizers a few).
constexpr int kSafetyNetMilliseconds = 30000;

/// "A simple model rocket.ork": five simulations with stored data, LOADED; the selected
/// configuration is that of its fourth.
constexpr const char* kSimpleModelRocket = "A simple model rocket.ork";

/// A run dialog the application showed: its request as the test compares it.
struct ShownRunDialog
{
    const QObject*      window = nullptr;
    QStringList         simulations;
    SimulationExecutor* executor = nullptr;
    /// Whether the request takes its simulations out of the estimate's hands.
    bool cancelsTheEstimate = false;
};

/// The questions of the running application: the test's script (ScriptedUserPrompts), but for the
/// run dialog, which is the application's own (WidgetUserPrompts::runSimulations():
/// SimulationRunDialog), each of which it notes.
class RunDialogPrompts final : public Gui::Test::ForwardingUserPrompts
{
public:
    explicit RunDialogPrompts(UserPrompts& delegate) noexcept : ForwardingUserPrompts(delegate) { }

    [[nodiscard]] RunSimulationsAnswer runSimulations(QWidget*                     parent,
                                                      const RunSimulationsRequest& request) override
    {
        ShownRunDialog shown{.window             = request.window,
                             .simulations        = {},
                             .executor           = request.executor,
                             .cancelsTheEstimate = static_cast<bool>(request.cancelBackgroundRuns)};
        for (const std::shared_ptr<Simulation>& simulation : request.simulations)
        {
            shown.simulations.append(QtStrings::fromUtf8(simulation->getName()));
        }
        m_shown.push_back(std::move(shown));
        return m_widgets.runSimulations(parent, request);
    }

    [[nodiscard]] const std::vector<ShownRunDialog>& shown() const noexcept { return m_shown; }

private:
    WidgetUserPrompts           m_widgets;
    std::vector<ShownRunDialog> m_shown;
};

/// Which executor the application runs its simulations on.
enum class Executor
{
    /// The tests' (ManualSimulationExecutor, given to the ApplicationContext): no thread.
    TESTS,
    /// The application's own (PollingSimulationExecutor): its threads.
    APPLICATIONS,
};

/// The tab of @p window.
[[nodiscard]] SimulationPanel& simulationsOf(const MainWindow& window)
{
    SimulationPanel* const panel = window.componentSimulationPanel();
    QTROCKET_ASSERT(panel != nullptr);
    return *panel;
}

/// The design view's estimate of the flight of @p window.
[[nodiscard]] BackgroundSimulation& estimateOf(const MainWindow& window)
{
    const RocketPanel* const panel = window.componentRocketPanel();
    QTROCKET_ASSERT(panel != nullptr);
    return panel->backgroundSimulation();
}

/// The status @p simulation has in the table of @p window ("none" when it is no row: never).
[[nodiscard]] std::string_view tableStatusOf(const MainWindow& window, const Simulation& simulation)
{
    const SimulationTableModel& model = simulationsOf(window).tableModel();
    const int                   row   = model.rowOf(simulation);
    if (row < 0)
    {
        return "none";
    }
    return model.status(row) == Status::UPTODATE ? "UPTODATE" : "not UPTODATE";
}

/// What the test saw of the estimate of @p window, in one line: "pauses=1 paused calculating
/// job" (the window's pauses, then each flag that is set).
[[nodiscard]] std::string estimateState(const MainWindow& window)
{
    const BackgroundSimulation& estimate = estimateOf(window);
    std::string                 text     = "pauses=" + std::to_string(window.backgroundRunPauses());
    text += estimate.isPaused() ? " paused" : "";
    text += estimate.isCalculating() ? " calculating" : "";
    text += estimate.job() != SimulationExecutor::kNoJob ? " job" : "";
    return text;
}

/// Waits in the event loop until the estimate @p estimate has submitted a job, at most the safety
/// net; whether it has.
[[nodiscard]] bool waitForAJob(const BackgroundSimulation& estimate)
{
    return QTest::qWaitFor([&estimate] { return estimate.job() != SimulationExecutor::kNoJob; },
                           kSafetyNetMilliseconds);
}

/// Waits in the event loop until the estimate @p estimate calculates no more, at most the safety
/// net; whether it does not.
[[nodiscard]] bool waitUntilEstimated(const BackgroundSimulation& estimate)
{
    return QTest::qWaitFor([&estimate] { return !estimate.isCalculating(); },
                           kSafetyNetMilliseconds);
}

/// Waits in the event loop until @p executor polls no more (no job is left), at most the safety
/// net; whether it does not.
[[nodiscard]] bool waitUntilIdle(const PollingSimulationExecutor& executor)
{
    return QTest::qWaitFor([&executor] { return !executor.isPolling(); }, kSafetyNetMilliseconds);
}

/// Waits until a run got to @p gate, at most the safety net; whether one did.
[[nodiscard]] bool waitForTheGate(const QtRocket::Test::StepGate& gate)
{
    return QTest::qWaitFor([&gate] { return gate.reached(); }, kSafetyNetMilliseconds);
}

/// The settings dialog of a new simulation, answered with OK (which runs it: the run dialog): its
/// title and whether it was the dialog of a new simulation are noted.
struct OkOfANewSimulation
{
    QString* title;
    bool*    newSimulation;

    void operator()(QWidget& modal) const
    {
        auto* const dialog = qobject_cast<SimulationConfigDialog*>(&modal);
        ASSERT_NE(dialog, nullptr);
        *title         = dialog->windowTitle();
        *newSimulation = dialog->isNewSimulation();
        dialog->okButton().click();
    }
};

/// The settings dialog, closed with Cancel after the test noted what the estimate of @p window
/// was while it was open, and how many jobs the tests' executor ran meanwhile (none may: the
/// estimate is paused).
struct CancelAfterLookingAtTheEstimate
{
    const MainWindow*         window;
    ManualSimulationExecutor* executor;
    std::string*              seen;

    void operator()(QWidget& modal) const
    {
        auto* const dialog = qobject_cast<SimulationConfigDialog*>(&modal);
        ASSERT_NE(dialog, nullptr);
        const std::size_t ran = executor->runAll();
        *seen                 = estimateState(*window) + " ran=" + std::to_string(ran);
        dialog->cancelButton().click();
    }
};

/// The run dialog, whose jobs the tests' executor runs (its STEPPED mode) after the test noted
/// what the estimate of @p window was when the batch had started; the dialog then closes by
/// itself.
struct RunAfterLookingAtTheEstimate
{
    const MainWindow*         window;
    ManualSimulationExecutor* executor;
    std::string*              seen;

    void operator()(QWidget& modal) const
    {
        ASSERT_NE(qobject_cast<SimulationRunDialog*>(&modal), nullptr);
        *seen = estimateState(*window);
        static_cast<void>(executor->runAll());
    }
};

/// The extra listeners of the estimate's runs: the gate of the test, which holds a run after its
/// first step until the test lets it go or its job's stop is requested.
struct HoldAtTheGate
{
    const QtRocket::Test::StepGate* gate;

    [[nodiscard]] std::vector<std::shared_ptr<SimulationListener>> operator()(
        const std::stop_token& stop) const
    {
        return {gate->listener(stop)};
    }
};

/// The running application, started as main() starts it, on the executor the test asks for
/// (start()), with the questions of the test (its script, and the application's run dialog), and
/// the design view's estimate on or off as the test asks; a design opened in a window of it
/// (open()).
class SimulationWindowIntegrationTest : public Gui::Test::GuiTest
{
protected:
    void TearDown() override
    {
        // The windows, the documents and the services go first.
        m_session = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        GuiTest::TearDown();
    }

    /// Starts the application on @p executor (the tests' in its @p mode), the design view's
    /// estimate on when @p estimate says.
    void start(Executor executor, bool estimate, Mode mode = Mode::IMMEDIATE)
    {
        auto prompts     = std::make_unique<RunDialogPrompts>(m_scripted);
        m_prompts        = prompts.get();
        auto preferences = std::make_unique<SettingsPreferences>(
            QtStrings::fromPath(m_directory.resolve("QtRocket.ini")));
        preferences->putBoolean(GuiPreferenceKeys::kBackgroundFlight, estimate);
        std::unique_ptr<ApplicationContext> context;
        if (executor == Executor::TESTS)
        {
            auto manual = std::make_unique<ManualSimulationExecutor>(mode);
            m_manual    = manual.get();
            context = std::make_unique<ApplicationContext>(std::move(preferences),
                                                           DataPaths::forApplication(),
                                                           std::move(prompts), std::move(manual));
        }
        else
        {
            context = std::make_unique<ApplicationContext>(
                std::move(preferences), DataPaths::forApplication(), std::move(prompts));
        }
        ApplicationSession::Options options;
        options.lastWindowBehaviour    = WindowManager::LastWindowBehaviour::EXIT;
        Application* const application = Application::current();
        QTROCKET_ASSERT(application != nullptr);
        m_session = std::make_unique<ApplicationSession>(*application, std::move(context), options);
    }

    /// A window of the application on the example @p fileName, loaded with the application's
    /// loading context (its motors, its preferences), beside the window the application started
    /// with; its estimate (when on) estimates without a debounce. Null, with a test failure, when
    /// the design cannot be loaded.
    [[nodiscard]] MainWindow* open(const std::string& fileName = kSimpleModelRocket)
    {
        Result<LoadedDocument> loaded =
            GeneralRocketLoader(m_session->context().loadingContext())
                .load(QtRocket::Test::dataDir() / "examples" / fileName);
        if (!loaded.has_value())
        {
            ADD_FAILURE() << "cannot load " << fileName << ": " << loaded.error().message;
            return nullptr;
        }
        MainWindow& window = m_session->windows().addDocument(std::move(loaded->document));
        estimateOf(window).setDebounceInterval(std::chrono::milliseconds(0));
        QCoreApplication::processEvents();
        return &window;
    }

    /// The simulations tab of @p window made the current one, its spotlight taken.
    static void showTheSimulationsTab(MainWindow& window)
    {
        window.selectTab(MainWindow::kSimulationTab);
        QCoreApplication::processEvents();
    }

    /// Selects the configuration of @p window's first simulation, which flies the short flight
    /// the tests run, and returns that simulation.
    [[nodiscard]] static std::shared_ptr<Simulation> selectTheFirstConfiguration(
        const MainWindow& window)
    {
        std::shared_ptr<Simulation> first = window.document().getSimulation(0);
        window.document().getRocket().setSelectedConfiguration(first->getFlightConfigurationId());
        QCoreApplication::processEvents();
        return first;
    }

    /// Makes @p window's first simulation out of date (its launch rod 0.5 m longer) and selects its
    /// configuration: the design view's estimate (on) asks for its run. Returns it, once the
    /// estimate submitted its job (with a test failure when it did not).
    [[nodiscard]] static std::shared_ptr<Simulation> estimateTheFirstSimulation(
        const MainWindow& window)
    {
        std::shared_ptr<Simulation> first   = window.document().getSimulation(0);
        SimulationOptions&          options = first->getOptions();
        options.setLaunchRodLength(options.getLaunchRodLength() + 0.5);
        static_cast<void>(selectTheFirstConfiguration(window));
        const BackgroundSimulation& estimate = estimateOf(window);
        EXPECT_TRUE(waitForAJob(estimate));
        EXPECT_EQ(estimate.runningSimulation(), first);
        return first;
    }

    /// Closes @p window as File > Close does, its changes discarded, and lets the event loop
    /// delete it.
    void closeDiscarding(MainWindow& window)
    {
        if (!window.document().isSaved())
        {
            m_scripted.answerSaveChanges(UserPrompts::SaveChangesAnswer::DISCARD);
        }
        EXPECT_TRUE(m_session->windows().close(window));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    [[nodiscard]] ApplicationSession&             session() const { return *m_session; }
    [[nodiscard]] Gui::Test::ScriptedUserPrompts& scripted() noexcept { return m_scripted; }
    [[nodiscard]] const RunDialogPrompts&         prompts() const noexcept { return *m_prompts; }
    /// The tests' executor (Executor::TESTS).
    [[nodiscard]] ManualSimulationExecutor& manual() const
    {
        QTROCKET_ASSERT(m_manual != nullptr);
        return *m_manual;
    }
    /// The application's executor (Executor::APPLICATIONS).
    [[nodiscard]] const PollingSimulationExecutor& polling() const
    {
        const auto* const executor = dynamic_cast<const PollingSimulationExecutor*>(
            &m_session->context().simulationExecutor());
        QTROCKET_ASSERT(executor != nullptr);
        return *executor;
    }

private:
    QtRocket::Test::TempDir             m_directory;
    Gui::Test::ScriptedUserPrompts      m_scripted;
    RunDialogPrompts*                   m_prompts{nullptr};
    ManualSimulationExecutor*           m_manual{nullptr};
    std::unique_ptr<ApplicationSession> m_session;
};

using SimulationWindowIntegrationThreadsTest = SimulationWindowIntegrationTest;

/// New simulation on the tab of @p window through its button, the settings dialog answered with
/// OK; whether the dialog came, with its title and whether it was a new simulation's.
[[nodiscard]] std::string newSimulationThroughItsDialog(const MainWindow& window)
{
    QString                          title;
    bool                             newSimulation = false;
    const Gui::Test::NextModalDialog answer(
        OkOfANewSimulation{.title = &title, .newSimulation = &newSimulation});
    QTest::mouseClick(simulationsOf(window).newButton(), Qt::LeftButton);
    return std::string(answer.acted() ? "dialog " : "no dialog ") + QtStrings::toUtf8(title) +
           (newSimulation ? " (new)" : "");
}

// ======================================================================== a new simulation

TEST_F(SimulationWindowIntegrationTest, ANewSimulationRunsFromItsDialogsOk)
{
    // SimulationPanel.newSimulation(): the simulation is added and its settings dialog opens
    // (SimulationConfigDialog, the application's); OK runs it through the run dialog
    // (SimulationRunDialog, on the tests' executor), closes, and the table shows it up to date.
    start(Executor::TESTS, false);
    MainWindow* const shown = open();
    ASSERT_NE(shown, nullptr);
    MainWindow& window = *shown;
    static_cast<void>(selectTheFirstConfiguration(window));
    showTheSimulationsTab(window);

    EXPECT_EQ(newSimulationThroughItsDialog(window), "dialog Edit simulation (new)");
    ASSERT_EQ(window.document().getSimulationCount(), 6U);
    const std::shared_ptr<Simulation> added = window.document().getSimulation(5);
    EXPECT_EQ(added->getName(), "Simulation 6");
    EXPECT_EQ(added->getStatus(), Status::UPTODATE);
    EXPECT_TRUE(added->hasSimulationData());
    EXPECT_EQ(tableStatusOf(window, *added), "UPTODATE");
    EXPECT_EQ(simulationsOf(window).table().selectedSimulations(),
              std::vector<std::shared_ptr<Simulation>>{added});
    // One run dialog, of the window, on the application's executor, which took the simulation out
    // of the estimate's hands; one job of the window.
    ASSERT_EQ(prompts().shown().size(), 1U);
    const ShownRunDialog& run = prompts().shown().front();
    EXPECT_EQ(run.window, &window);
    EXPECT_EQ(run.simulations, QStringList{QStringLiteral("Simulation 6")});
    EXPECT_EQ(run.executor, &manual());
    EXPECT_TRUE(run.cancelsTheEstimate);
    ASSERT_EQ(manual().submittedJobs().size(), 1U);
    EXPECT_EQ(manual().submittedJobs().front().window, &window);
    // The dialogs are gone, and with them the window's pauses.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_EQ(window.findChild<SimulationConfigDialog*>(), nullptr);
    EXPECT_EQ(window.findChild<SimulationRunDialog*>(), nullptr);
    EXPECT_EQ(estimateState(window), "pauses=0");
}

TEST_F(SimulationWindowIntegrationThreadsTest, ANewSimulationRunsOnTheApplicationsExecutor)
{
    // As above, on the application's executor: the run goes on a thread of the core's runner,
    // the run dialog polls it, and its end is adopted on the GUI thread.
    start(Executor::APPLICATIONS, false);
    MainWindow* const shown = open();
    ASSERT_NE(shown, nullptr);
    MainWindow& window = *shown;
    static_cast<void>(selectTheFirstConfiguration(window));
    showTheSimulationsTab(window);

    EXPECT_EQ(newSimulationThroughItsDialog(window), "dialog Edit simulation (new)");
    ASSERT_EQ(window.document().getSimulationCount(), 6U);
    const std::shared_ptr<Simulation> added = window.document().getSimulation(5);
    EXPECT_EQ(added->getStatus(), Status::UPTODATE);
    EXPECT_TRUE(added->hasSimulationData());
    EXPECT_EQ(tableStatusOf(window, *added), "UPTODATE");
    ASSERT_EQ(prompts().shown().size(), 1U);
    EXPECT_EQ(prompts().shown().front().executor, &polling());
    // Nothing is left to poll.
    EXPECT_TRUE(waitUntilIdle(polling()));
}

// ============================================================== the design view's estimate

TEST_F(SimulationWindowIntegrationTest, TheEstimatePausesWhileTheSettingsDialogIsOpen)
{
    // A production window's design view estimates on the application's executor (here the tests',
    // STEPPED: a job waits until the test runs it). While the settings dialog of the window is
    // open, its job is abandoned and none starts or is adopted (O-M4-16); once the dialog closed,
    // the estimate starts its run again and adopts it.
    start(Executor::TESTS, true, Mode::STEPPED);
    const MainWindow* const shown = open();
    ASSERT_NE(shown, nullptr);
    const MainWindow&                 window   = *shown;
    const std::shared_ptr<Simulation> first    = estimateTheFirstSimulation(window);
    const SimulationExecutor::JobId   waiting  = estimateOf(window).job();
    const BackgroundSimulation&       estimate = estimateOf(window);
    EXPECT_EQ(estimateState(window), "pauses=0 calculating job");

    // The settings dialog of the second simulation (Edit), closed unchanged with Cancel.
    std::string                      seen;
    const Gui::Test::NextModalDialog answer(
        CancelAfterLookingAtTheEstimate{.window = &window, .executor = &manual(), .seen = &seen});
    SimulationPanel& tab = simulationsOf(window);
    tab.table().selectSimulations({window.document().getSimulation(1)});
    tab.editSimulation();
    EXPECT_TRUE(answer.acted());
    EXPECT_EQ(seen, "pauses=1 paused calculating ran=0");
    EXPECT_EQ(manual().abandonedJobs(), std::vector<SimulationExecutor::JobId>{waiting});

    // Resumed: the run of the first simulation is asked for again, and adopted.
    EXPECT_TRUE(waitForAJob(estimate));
    EXPECT_EQ(estimate.runningSimulation(), first);
    EXPECT_EQ(manual().runAll(), 1U);
    EXPECT_EQ(first->getStatus(), Status::UPTODATE);
    EXPECT_EQ(tableStatusOf(window, *first), "UPTODATE");
    EXPECT_EQ(estimateState(window), "pauses=0");
}

TEST_F(SimulationWindowIntegrationTest, ARunOfTheTabTakesItsSimulationsFromTheEstimate)
{
    // The tab's Run of the simulation the estimate is about to run: the estimate is paused while
    // the run dialog is open and the batch takes the simulation out of its hands, so the run
    // dialog's run is the one adopted, and the estimate does not run it again afterwards (the
    // critic's G4).
    start(Executor::TESTS, true, Mode::STEPPED);
    MainWindow* const shown = open();
    ASSERT_NE(shown, nullptr);
    MainWindow&                       window  = *shown;
    const std::shared_ptr<Simulation> first   = estimateTheFirstSimulation(window);
    const SimulationExecutor::JobId   waiting = estimateOf(window).job();
    showTheSimulationsTab(window);

    std::string                      seen;
    const Gui::Test::NextModalDialog answer(
        RunAfterLookingAtTheEstimate{.window = &window, .executor = &manual(), .seen = &seen});
    const SimulationPanel& tab = simulationsOf(window);
    tab.table().selectSimulations({first});
    QTest::mouseClick(tab.runButton(), Qt::LeftButton);
    EXPECT_TRUE(answer.acted());
    // While the run dialog was open: paused, nothing left to estimate.
    EXPECT_EQ(seen, "pauses=1 paused");
    EXPECT_EQ(manual().abandonedJobs(), std::vector<SimulationExecutor::JobId>{waiting});
    EXPECT_EQ(first->getStatus(), Status::UPTODATE);
    EXPECT_EQ(tableStatusOf(window, *first), "UPTODATE");
    // Afterwards nothing more runs: the estimate's job and the run dialog's.
    QCoreApplication::processEvents();
    EXPECT_EQ(estimateState(window), "pauses=0");
    EXPECT_EQ(manual().submittedJobs().size(), 2U);
    EXPECT_TRUE(manual().waitingJobs().empty());
}

TEST_F(SimulationWindowIntegrationThreadsTest, TheEstimateRunsOnTheApplicationsExecutor)
{
    // The estimate of a production window on the application's threads: the run of the first
    // simulation is adopted on the GUI thread, and the table shows it up to date.
    start(Executor::APPLICATIONS, true);
    const MainWindow* const shown = open();
    ASSERT_NE(shown, nullptr);
    const MainWindow&                 window   = *shown;
    const std::shared_ptr<Simulation> first    = estimateTheFirstSimulation(window);
    const BackgroundSimulation&       estimate = estimateOf(window);
    EXPECT_TRUE(waitUntilEstimated(estimate));
    EXPECT_EQ(first->getStatus(), Status::UPTODATE);
    EXPECT_EQ(tableStatusOf(window, *first), "UPTODATE");
    EXPECT_EQ(estimateState(window), "pauses=0");
}

// ======================================================================= a window that closes

TEST_F(SimulationWindowIntegrationTest, AClosedWindowAbandonsItsWaitingJob)
{
    // MainWindow's destructor abandons the jobs of the window (the estimate's here): none of
    // their callbacks reaches what the window owned.
    start(Executor::TESTS, true, Mode::STEPPED);
    MainWindow* const shown = open();
    ASSERT_NE(shown, nullptr);
    static_cast<void>(estimateTheFirstSimulation(*shown));
    const SimulationExecutor::JobId waiting = estimateOf(*shown).job();
    closeDiscarding(*shown);
    EXPECT_EQ(session().windows().windowCount(), 1U);
    EXPECT_EQ(manual().abandonedJobs(), std::vector<SimulationExecutor::JobId>{waiting});
    EXPECT_FALSE(manual().isPending(waiting));
    EXPECT_EQ(manual().runAll(), 0U);
}

TEST_F(SimulationWindowIntegrationThreadsTest, AClosedWindowAbandonsItsRunningJob)
{
    // The estimate's run is held on a thread of the application's executor when its window
    // closes: the job is abandoned (its stop requested, which lets the gate go), and it ends
    // without a callback, while the window, its design view and its document are gone.
    start(Executor::APPLICATIONS, true);
    MainWindow* const shown = open();
    ASSERT_NE(shown, nullptr);
    const QtRocket::Test::StepGate gate(1);
    estimateOf(*shown).setExtraListeners(HoldAtTheGate{&gate});
    static_cast<void>(estimateTheFirstSimulation(*shown));
    EXPECT_TRUE(waitForTheGate(gate));
    closeDiscarding(*shown);
    EXPECT_EQ(session().windows().windowCount(), 1U);
    EXPECT_TRUE(waitUntilIdle(polling()));
}

}  // namespace

}  // namespace QtRocket::Gui
