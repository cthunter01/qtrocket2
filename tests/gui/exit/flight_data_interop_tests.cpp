// The interop check's mode for flight data QtRocket ran (plan section 11.6 E7; the README of
// tools/openrocket-goldens, "Interop check"): a DISABLED_ test, which neither ctest nor CI runs,
// writes QtRocket's saves of flights its GUI ran, and InteropCheck (interop.sh) has OpenRocket read
// them: each loads without a warning, OpenRocket's re-save writes the flight as QtRocket's file
// holds it (the summary values OpenRocket computes from the branches it read, the warnings, the
// events and every value of every branch), and the reload of that re-save has every value bit for
// bit. No value of a fresh flight is pinned (CLAUDE.md, Portability): the check compares
// OpenRocket's reading of the file with the file.
//
// Each flight of kGuiFlightRuns (tests/core/goldens/GuiInteropInputs.h: "A simple model rocket"
// #1 and "Clustered motors" #1, fresh runs plan section 11.6 allows) runs in an application of its
// own (ApplicationSession, started as main() starts it, over a preferences file of the test's own:
// the defaults, but the design view's estimate off, so that no simulation runs but the one the
// test runs; the application's own executor of simulations, PollingSimulationExecutor, on its
// threads), worked on only through the GUI:
// 1. File > Open of the example's file of data/examples, the file chooser answered by the test's
//    script; the New design of the start is replaced;
// 2. a click on the Flight simulations tab, a click in the name column of the simulation's row,
//    and a click on the Run button: the application's run dialog (SimulationRunDialog) runs the
//    simulation on a thread of the executor and closes when the run is over;
// 3. File > Save as: the design information (UserPrompts::editDesignInformation()) left as it is
//    by the test's script, the file chooser answered with a file of the test's directory and the
//    storage option it offers, the document's default (the examples save their simulation data).
// The save is then copied into the directory of the interop check's saves as
// "<input>.zip-data.ork" (InteropSaves.h). The questions the steps do not answer fail the test
// (the test's ScriptedUserPrompts), the run dialog's failure messages among them. By hand, from the
// source tree, one command per line (the core's writer first: it removes the saves of an earlier
// run, these among them):
//
// clang-format off
//   build/<preset>/bin/QtRocket_core_tests --gtest_also_run_disabled_tests --gtest_filter='OrkInteropSaves.*'
//   build/<preset>/bin/QtRocket_gui_tests --gtest_also_run_disabled_tests --gtest_filter='*InteropSave*'
//   tools/openrocket-goldens/interop.sh build/<preset>/interop-saves
// clang-format on
//
// The run goes on the application's threads, so the tsan preset's build of this test is the check
// of its threads (CLAUDE.md, Threads).

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QModelIndex>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QTabBar>
#include <QTabWidget>
#include <QTest>
#include <QTimer>
#include <QWidget>
#include <Qt>
#include <QtCore/qtestsupport_core.h>

#include <gtest/gtest.h>

#include "ForwardingUserPrompts.h"
#include "GuiTestSupport.h"
#include "InteropSaves.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/app/Application.h"
#include "QtRocket/gui/app/ApplicationContext.h"
#include "QtRocket/gui/app/ApplicationSession.h"
#include "QtRocket/gui/app/DataPaths.h"
#include "QtRocket/gui/dialogs/simulation/SimulationRunDialog.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/settings/SettingsPreferences.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationPanel.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/simulation/SimulationTableView.h"
#include "QtRocket/gui/util/PollingSimulationExecutor.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/IconButton.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "ScriptedUserPrompts.h"
#include "TestTempDir.h"
#include "exit/ScenarioInput.h"
#include "goldens/GoldenExamples.h"
#include "goldens/GuiInteropInputs.h"

namespace QtRocket::Gui
{

namespace
{

using Gui::Test::ScenarioInput::activateWindowOf;
using Gui::Test::ScenarioInput::settle;
using Gui::Test::ScenarioInput::settleAndDelete;
using Gui::Test::ScenarioInput::triggerAction;
using QtRocket::Test::GuiFlightRun;
using Column = SimulationColumns::Column;

/// How long the test lets the run dialog run before it cancels the run and fails: a safety net only
/// a defect reaches (a run of these short flights takes well under a second, under the sanitizers
/// a few).
constexpr std::chrono::milliseconds kRunSafetyNet{300000};

/// How long the test waits for the application's executor to have nothing left to poll after the
/// run dialog closed: a safety net as well.
constexpr int kIdleSafetyNetMilliseconds = 30000;

/// The questions of the running application: the test's script (ScriptedUserPrompts: the file of
/// File > Open, the design information of Save as, and a test failure for every other question),
/// but for the run dialog, which is the application's own (SimulationRunDialog::run(), on the
/// application's executor; its failure messages go through these prompts, so to the script), and
/// the file chooser of Save as, which the test answers with its file and the storage option the
/// chooser offers. Each run dialog and each file chooser is noted.
class FlightRunPrompts final : public Gui::Test::ForwardingUserPrompts
{
public:
    /// The prompts over the test's script @p delegate (which outlives them); Save as saves to
    /// @p saveAs.
    FlightRunPrompts(UserPrompts& delegate, std::filesystem::path saveAs)
      : ForwardingUserPrompts(delegate), m_saveAs(std::move(saveAs))
    {
    }

    [[nodiscard]] RunSimulationsAnswer runSimulations(QWidget*                     parent,
                                                      const RunSimulationsRequest& request) override
    {
        std::vector<std::string> names;
        names.reserve(request.simulations.size());
        for (const std::shared_ptr<Simulation>& simulation : request.simulations)
        {
            names.push_back(simulation->getName());
        }
        m_runs.push_back(std::move(names));
        const RunSimulationsAnswer answer = SimulationRunDialog::run(parent, *this, request);
        m_runAnswers.push_back(answer);
        return answer;
    }

    [[nodiscard]] std::optional<SaveFileAnswer> askSaveFile(QWidget* /*parent*/,
                                                            const SaveFileRequest& request) override
    {
        m_offeredStorage.push_back(request.saveSimulationData);
        return SaveFileAnswer{.file               = m_saveAs,
                              .directory          = m_saveAs.parent_path(),
                              .saveSimulationData = request.saveSimulationData};
    }

    /// The simulations of each run dialog, by name, in the order the dialogs opened.
    [[nodiscard]] const std::vector<std::vector<std::string>>& runs() const noexcept
    {
        return m_runs;
    }
    /// How each run dialog ended.
    [[nodiscard]] const std::vector<RunSimulationsAnswer>& runAnswers() const noexcept
    {
        return m_runAnswers;
    }
    /// The storage option each file chooser of Save as offered (nullopt: none).
    [[nodiscard]] const std::vector<std::optional<bool>>& offeredStorage() const noexcept
    {
        return m_offeredStorage;
    }

private:
    std::filesystem::path                 m_saveAs;
    std::vector<std::vector<std::string>> m_runs;
    std::vector<RunSimulationsAnswer>     m_runAnswers;
    std::vector<std::optional<bool>>      m_offeredStorage;
};

/// The run dialogs that are open.
[[nodiscard]] std::vector<SimulationRunDialog*> openRunDialogs()
{
    std::vector<SimulationRunDialog*> dialogs;
    for (QWidget* const widget : QApplication::topLevelWidgets())
    {
        if (auto* const dialog = qobject_cast<SimulationRunDialog*>(widget);
            dialog != nullptr && dialog->isVisible())
        {
            dialogs.push_back(dialog);
        }
    }
    return dialogs;
}

/// The safety net of a run: when it fires, it notes that it did and cancels the open run dialogs
/// (Cancel: the batch is cancelled, the dialog closes and run() returns).
struct CancelTheRunDialogs
{
    bool* fired;

    void operator()() const
    {
        *fired = true;
        for (SimulationRunDialog* const dialog : openRunDialogs())
        {
            dialog->reject();
        }
    }
};

/// The test's name of a flight: its example's input without "example-", '_' for '-'
/// ("a_simple_model_rocket").
[[nodiscard]] std::string flightRunTestName(const ::testing::TestParamInfo<GuiFlightRun>& info)
{
    std::string name(info.param.example);
    if (constexpr std::string_view kPrefix = "example-"; name.starts_with(kPrefix))
    {
        name.erase(0, kPrefix.size());
    }
    for (char& character : name)
    {
        if (character == '-')
        {
            character = '_';
        }
    }
    return name;
}

/// The running application of one flight (see the head of the file); its windows, documents and
/// services go when the test ends.
class FlightDataInteropSave : public Gui::Test::GuiTest,
                              public ::testing::WithParamInterface<GuiFlightRun>
{
protected:
    void TearDown() override
    {
        // The windows, the documents and the services go first, then what they gave to
        // deleteLater().
        m_session = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        GuiTest::TearDown();
    }

    /// Starts the application (see the head of the file); Save as saves to @p saveAs.
    void start(const std::filesystem::path& saveAs)
    {
        auto prompts     = std::make_unique<FlightRunPrompts>(m_scripted, saveAs);
        m_prompts        = prompts.get();
        auto preferences = std::make_unique<SettingsPreferences>(
            QtStrings::fromPath(m_directory.resolve("QtRocket.ini")));
        // No window runs a simulation on its own: the save holds the one flight the test runs.
        preferences->putBoolean(GuiPreferenceKeys::kBackgroundFlight, false);
        auto context = std::make_unique<ApplicationContext>(
            std::move(preferences), DataPaths::forApplication(), std::move(prompts));
        ApplicationSession::Options options;
        options.lastWindowBehaviour    = WindowManager::LastWindowBehaviour::KEEP_RUNNING;
        Application* const application = Application::current();
        QTROCKET_ASSERT(application != nullptr);
        m_session = std::make_unique<ApplicationSession>(*application, std::move(context), options);
        settle();
    }

    /// File > Open of @p file from the window the application started with, the file chooser
    /// answered with it: the window of the design (the start's New design replaced), or null with
    /// what went wrong in @p problem.
    [[nodiscard]] MainWindow* openThroughTheMenu(const std::filesystem::path& file,
                                                 std::string&                 problem)
    {
        const WindowManager& windows = m_session->windows();
        if (windows.windowCount() != 1)
        {
            problem = std::format("{} windows at the start, not one", windows.windowCount());
            return nullptr;
        }
        MainWindow* const first = windows.windows().front();
        QTROCKET_ASSERT(first != nullptr);
        const QPointer<MainWindow> start = first;
        m_scripted.answerOpenFiles(
            UserPrompts::OpenFileAnswer{.files = {file}, .directory = file.parent_path()});
        if (problem = triggerAction(first->menus().openAction(), "File > Open"); !problem.empty())
        {
            return nullptr;
        }
        settleAndDelete();
        if (windows.windowCount() != 1 || !start.isNull())
        {
            problem = std::format("{} windows after File > Open, the start's closed: {}",
                                  windows.windowCount(), start.isNull());
            return nullptr;
        }
        return windows.windows().front();
    }

    /// The Flight simulations tab of @p window shown with a click on its tab, the row of
    /// @p simulation selected with a click in its name column, and a click on the Run button,
    /// which returns when the run dialog has closed (or the safety net cancelled it); what went
    /// wrong, or "".
    [[nodiscard]] static std::string runThroughTheTab(const MainWindow&                  window,
                                                      const std::shared_ptr<Simulation>& simulation)
    {
        QTabBar* const tabs = window.tabs()->tabBar();
        if (!activateWindowOf(*tabs))
        {
            return "the window does not become active";
        }
        QTest::mouseClick(tabs, Qt::LeftButton, Qt::NoModifier,
                          tabs->tabRect(MainWindow::kSimulationTab).center());
        settle();
        const SimulationPanel* const panel = window.componentSimulationPanel();
        if (panel == nullptr || window.selectedTab() != MainWindow::kSimulationTab)
        {
            return "the Flight simulations tab is not shown";
        }
        SimulationTableView&        table = panel->table();
        const SimulationTableModel& model = panel->tableModel();
        const int                   row   = model.rowOf(*simulation);
        if (row < 0 || !table.isColumnVisible(Column::NAME))
        {
            return "the table shows no name of the simulation";
        }
        const QModelIndex cell = model.index(row, SimulationColumns::indexOf(Column::NAME));
        table.scrollTo(cell);
        const QRect rect = table.visualRect(cell);
        if (!table.viewport()->rect().contains(rect.center()))
        {
            return std::format("the row {} is not in view", row);
        }
        QTest::mouseClick(table.viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
        settle();
        if (table.selectedSimulations() != std::vector<std::shared_ptr<Simulation>>{simulation})
        {
            return "the click did not select the simulation alone";
        }
        IconButton* const run = panel->runButton();
        if (run == nullptr || !run->isEnabled())
        {
            return "the Run button is not enabled";
        }
        QTimer     safetyNet;
        bool       fired  = false;
        const auto cancel = CancelTheRunDialogs{.fired = &fired};
        QObject::connect(&safetyNet, &QTimer::timeout, &safetyNet, cancel);
        safetyNet.setSingleShot(true);
        safetyNet.start(kRunSafetyNet);
        QTest::mouseClick(run, Qt::LeftButton);
        safetyNet.stop();
        settleAndDelete();
        if (fired)
        {
            return "the run did not end within the safety net: the test cancelled it";
        }
        if (!openRunDialogs().empty())
        {
            return "a run dialog is still open";
        }
        return {};
    }

    /// File > Save as of @p window, the design information left as it is by the script and the
    /// file chooser answered by the prompts (FlightRunPrompts); what went wrong, or "".
    [[nodiscard]] std::string saveAs(const MainWindow& window)
    {
        m_scripted.answerDesignInformation();
        if (std::string problem = triggerAction(window.menus().saveAsAction(), "File > Save as");
            !problem.empty())
        {
            return problem;
        }
        settleAndDelete();
        return {};
    }

    /// Waits in the event loop until the application's executor polls no more (no job is left),
    /// at most the safety net; whether it does not.
    [[nodiscard]] bool waitUntilTheExecutorIsIdle() const
    {
        const auto* const executor = dynamic_cast<const PollingSimulationExecutor*>(
            &m_session->context().simulationExecutor());
        QTROCKET_ASSERT(executor != nullptr);
        return QTest::qWaitFor([executor] { return !executor->isPolling(); },
                               kIdleSafetyNetMilliseconds);
    }

    [[nodiscard]] ApplicationSession&     session() const { return *m_session; }
    [[nodiscard]] const FlightRunPrompts& prompts() const
    {
        QTROCKET_ASSERT(m_prompts != nullptr);
        return *m_prompts;
    }
    [[nodiscard]] const QtRocket::Test::TempDir& directory() const noexcept { return m_directory; }

private:
    QtRocket::Test::TempDir             m_directory;
    Gui::Test::ScriptedUserPrompts      m_scripted;
    FlightRunPrompts*                   m_prompts{nullptr};
    std::unique_ptr<ApplicationSession> m_session;
};

/// The number of branches of @p simulation's flight and the rows of its first branch, in one
/// text ("2 branches, 347 rows"), or "no flight".
[[nodiscard]] std::string flightShape(const Simulation& simulation)
{
    const std::shared_ptr<FlightData>& data = simulation.getSimulatedData();
    if (data == nullptr || data->getBranchCount() == 0)
    {
        return "no flight";
    }
    return std::format("{} branches, {} rows", data->getBranchCount(),
                       data->getBranch(0).getLength());
}

/// flightShape() of the simulation @p index of the design file @p file, loaded in @p context; or
/// why there is none.
[[nodiscard]] std::string savedFlightShape(const DocumentLoadingContext& context,
                                           const std::filesystem::path& file, std::size_t index)
{
    const Result<LoadedDocument> saved = GeneralRocketLoader(context).load(file);
    if (!saved.has_value())
    {
        return "the save does not load: " + saved.error().toString();
    }
    const OpenRocketDocument& document = *saved->document;
    if (index >= document.getSimulationCount())
    {
        return std::format("the save has {} simulations", document.getSimulationCount());
    }
    return flightShape(*document.getSimulation(index));
}

INSTANTIATE_TEST_SUITE_P(Flights, FlightDataInteropSave,
                         ::testing::ValuesIn(QtRocket::Test::kGuiFlightRuns), flightRunTestName);

// The save of a flight QtRocket's GUI ran, for the interop check's mode for flight data QtRocket
// ran (see the head of the file): a DISABLED_ test, which neither ctest nor CI runs. It writes
// build/<preset>/interop-saves/gui-run-<example>.zip-data.ork (or into the directory of
// --interop-saves=<directory>), replacing the one an earlier run wrote, and touches no other file.
// InteropCheck (tools/openrocket-goldens) checks it as its input "gui-run-<example>"
// (InteropCheck.GUI_RUNS), against OpenRocket's load of the example, in the harness's environment
// as the example's own saves.
TEST_P(FlightDataInteropSave, DISABLED_WritesTheSaveForTheInteropCheck)
{
    const GuiFlightRun&                        run = GetParam();
    const QtRocket::Test::GoldenExample* const example =
        QtRocket::Test::findGoldenExample(run.example);
    ASSERT_NE(example, nullptr) << run.example;
    const std::filesystem::path file = directory().resolve(std::format("{}.ork", run.input));
    start(file);
    std::string             problem;
    const MainWindow* const window = openThroughTheMenu(example->original, problem);
    ASSERT_NE(window, nullptr) << problem;
    ASSERT_LT(run.simulation, window->document().getSimulationCount());
    const std::shared_ptr<Simulation> simulation = window->document().getSimulation(run.simulation);

    // The run: one run dialog, of the simulation, which ended with every run successful; the
    // simulation up to date with the flight it got.
    ASSERT_EQ(runThroughTheTab(*window, simulation), "");
    EXPECT_EQ(prompts().runs(), std::vector<std::vector<std::string>>{{simulation->getName()}});
    const UserPrompts::RunSimulationsAnswer successful{.cancelled = false, .allSuccessful = true};
    EXPECT_EQ(prompts().runAnswers(), std::vector<UserPrompts::RunSimulationsAnswer>{successful});
    EXPECT_EQ(simulation->getStatus(), Simulation::Status::UPTODATE);
    EXPECT_TRUE(simulation->hasSimulationData());
    EXPECT_TRUE(waitUntilTheExecutorIsIdle());
    const std::string ran = flightShape(*simulation);
    EXPECT_NE(ran, "no flight");

    // Save as, with the storage option the chooser offers: the simulation data, the document's
    // default for the examples. The design is saved.
    ASSERT_EQ(saveAs(*window), "");
    EXPECT_EQ(prompts().offeredStorage(), std::vector<std::optional<bool>>{true});
    EXPECT_TRUE(window->document().isSaved());

    // The save holds the flight as the run left it: QtRocket's load of it has the branches and
    // the rows of the run (the values are compared by InteropCheck, against OpenRocket's reading).
    EXPECT_EQ(savedFlightShape(session().context().loadingContext(), file, run.simulation), ran);

    const Result<std::filesystem::path> written =
        Gui::Test::writeInteropSave(file, std::format("{}.zip-data.ork", run.input));
    ASSERT_TRUE(written.has_value()) << written.error().message;
    std::println("{}: {}", pathToUtf8(written.value_or(std::filesystem::path())), ran);
}

}  // namespace

}  // namespace QtRocket::Gui
