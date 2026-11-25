// The "Flight simulations" tab in its window, as WindowManager puts one into every window
// (BasicFrame's SimulationPanel at SIMULATION_TAB): its wiring to the window (the tab, its
// design and clipboard, the File menu's "Export simulation table as CSV file", the component
// actions' simulation half), the tab switches (BasicFrame.stateChanged(): updatePreviousSelection()
// when the tab is left, takeTheSpotlight() and activating() when it is chosen), the tree's and the
// table's selections exclusive across the tabs (Java's DocumentSelectionModel), the Edit menu's
// Edit, Cut, Copy, Paste, Duplicate and Delete on the selected simulations with their states
// (RocketActions.java:478-1000), Paste without a selected simulation on the tab (OpenRocket bug 27,
// U-M4-13), undo and redo across the tabs, the tables read again when the preferences dialog
// refreshes the design views (critic G6), and the table of every pinned design through its window
// (SimulationsWindowGolden: the simulation-table pins, which OpenRocket's own SimulationPanel
// wrote). OpenRocket has no JUnit test of any of it. The dialogs are recorded
// (RecordingSimulationDialogs) and the questions scripted (ScriptedUserPrompts), so nothing runs.

#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QKeySequence>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTabBar>
#include <QTabWidget>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/SimulationCopy.h"
#include "QtRocket/gui/dialogs/DefaultUnitSelector.h"  // IWYU pragma: keep
#include "QtRocket/gui/dialogs/PreferencesDialog.h"
#include "QtRocket/gui/dialogs/UnitsPreferencesPanel.h"
#include "QtRocket/gui/main/DesignPanel.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/main/RocketActions.h"
#include "QtRocket/gui/main/WidgetUserPrompts.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/simulation/SimulationClipboard.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationPanel.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/simulation/SimulationTableView.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationDialogs.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "RecordingSimulationDialogs.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "dialogs/PreferencesPageTranscript.h"
#include "simulation/SimulationTablePinChecks.h"
#include "simulation/SimulationTablePins.h"
#include "simulation/SimulationTableTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using Column = SimulationColumns::Column;
using Status = Simulation::Status;
using Tab    = SimulationDialogs::Tab;

/// The example every test but the golden ones opens: five simulations, every one LOADED.
constexpr std::string_view kExample = "A simple model rocket.ork";

/// The tab of @p window.
[[nodiscard]] SimulationPanel& simulationsOf(const MainWindow& window)
{
    SimulationPanel* const panel = window.componentSimulationPanel();
    QTROCKET_ASSERT(panel != nullptr);
    return *panel;
}

/// The design panel of @p window.
[[nodiscard]] DesignPanel& designOf(const MainWindow& window)
{
    DesignPanel* const panel = window.componentDesignPanel();
    QTROCKET_ASSERT(panel != nullptr);
    return *panel;
}

/// The simulation of @p window's table at @p row.
[[nodiscard]] std::shared_ptr<Simulation> rowOf(const MainWindow& window, int row)
{
    return simulationsOf(window).tableModel().simulation(row);
}

/// The selected component names of @p window's design, "[A, B]".
[[nodiscard]] std::string designSelectionOf(const MainWindow& window)
{
    std::string text = "[";
    for (const RocketComponent* component : designOf(window).selection().selectedComponents())
    {
        text += (text.size() > 1 ? ", " : "") + component->getName();
    }
    return text + "]";
}

/// The names of @p window's selected simulations, from the top of the table, "[A, B]".
[[nodiscard]] std::string simulationSelectionOf(const MainWindow& window)
{
    std::string text = "[";
    for (const std::shared_ptr<Simulation>& simulation :
         simulationsOf(window).table().selectedSimulations())
    {
        text += (text.size() > 1 ? ", " : "") + simulation->getName();
    }
    return text + "]";
}

/// The names of @p window's simulations, in the document's order, "[A, B]".
[[nodiscard]] std::string simulationsListOf(const MainWindow& window)
{
    std::string text = "[";
    for (const std::shared_ptr<Simulation>& simulation : window.document().getSimulations())
    {
        text += (text.size() > 1 ? ", " : "") + simulation->getName();
    }
    return text + "]";
}

/// The enabled ones of the Edit menu's Edit, Cut, Copy, Paste, Duplicate and Delete of
/// @p menus, by their names, "edit cut copy".
[[nodiscard]] std::string enabledEditItems(const MainMenus& menus)
{
    const std::array<std::pair<std::string_view, const QAction*>, 6> items{{
        {"edit", menus.editAction()},
        {"cut", menus.cutAction()},
        {"copy", menus.copyAction()},
        {"paste", menus.pasteAction()},
        {"duplicate", menus.duplicateAction()},
        {"delete", menus.deleteAction()},
    }};
    std::string                                                      text;
    for (const auto& [name, action] : items)
    {
        if (action->isEnabled())
        {
            text += (text.empty() ? "" : " ") + std::string(name);
        }
    }
    return text;
}

/// The status of each row of @p model, from the top.
[[nodiscard]] std::vector<Status> statusesOf(const SimulationTableModel& model)
{
    std::vector<Status> statuses;
    statuses.reserve(static_cast<std::size_t>(model.rowCount()));
    for (int row = 0; row < model.rowCount(); ++row)
    {
        statuses.push_back(model.status(row));
    }
    return statuses;
}

/// Makes @p tab @p window's current one and delivers the posted events (the simulations tab's
/// spotlight comes in the next turn of the event loop: Java's invokeLater()).
void showTab(MainWindow& window, int tab)
{
    window.selectTab(tab);
    QCoreApplication::processEvents();
}

/// The windows of a window manager over test services and a clipboard of the test's own.
class SimulationsWindowTest : public Gui::Test::GuiTest
{
protected:
    void TearDown() override
    {
        // The windows, and the documents and panels they leave to deleteLater(), go first; Copy
        // put the table's text on the system clipboard.
        m_manager = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QGuiApplication::clipboard()->clear();
        GuiTest::TearDown();
    }

    [[nodiscard]] WindowManager& manager()
    {
        if (m_manager == nullptr)
        {
            m_manager = std::make_unique<WindowManager>(m_services.services(), m_clipboard,
                                                        WindowManager::LastWindowBehaviour::EXIT);
        }
        return *m_manager;
    }

    /// A window on the example @p file (kExample by default); null (and a failure) when it cannot
    /// be loaded.
    [[nodiscard]] MainWindow* openExample(std::string_view file = kExample)
    {
        std::unique_ptr<OpenRocketDocument> document = Gui::Test::loadSimulationExample(file);
        if (document == nullptr)
        {
            return nullptr;
        }
        MainWindow& window = manager().addDocument(std::move(document));
        QCoreApplication::processEvents();
        return &window;
    }

    /// Selects every simulation of @p window's table and deletes them (the question answered).
    void deleteEverySimulation(const MainWindow& window)
    {
        SimulationPanel&                               tab = simulationsOf(window);
        const std::vector<std::shared_ptr<Simulation>> all = tab.tableModel().simulations();
        tab.table().selectSimulations(all);
        prompts().answerDeleteSimulations({.confirmed = true, .doNotAskAgain = false});
        tab.deleteSimulations();
        QCoreApplication::processEvents();
    }

    [[nodiscard]] Gui::Test::TestGuiServices&     services() { return m_services; }
    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() { return m_services.prompts(); }
    [[nodiscard]] Preferences&                    preferences() { return m_services.preferences(); }
    [[nodiscard]] const OpenRocketClipboard&      clipboard() const { return m_clipboard; }
    [[nodiscard]] const std::vector<Gui::Test::RecordingSimulationDialogs::Request>&
    dialogRequests() const
    {
        return m_services.simulationDialogs().requests();
    }

    /// Makes @p window the active one; whether it is.
    [[nodiscard]] static bool activate(MainWindow& window)
    {
        window.activateWindow();
        return QTest::qWaitForWindowActive(&window);
    }

private:
    Gui::Test::TestGuiServices     m_services;
    OpenRocketClipboard            m_clipboard;
    std::unique_ptr<WindowManager> m_manager;
};

// ================================================================================ the wiring

/// How @p window's simulations tab is wired, in one line: its index and text, and whether it is
/// the window's simulation panel, on the window's design, with @p clipboard, reached by the
/// component actions and in the window (its questions' and its runs' window).
[[nodiscard]] std::string wiringOf(const MainWindow& window, const OpenRocketClipboard& clipboard)
{
    const SimulationPanel& tab   = simulationsOf(window);
    const int              index = window.tabs()->indexOf(window.componentSimulationPanel());
    std::string            text  = "tab=" + std::to_string(index) + " text='" +
                                   QtStrings::toUtf8(window.tabs()->tabText(index)) + "'";
    text += window.simulationPanel() == &tab ? " panel" : " other-panel";
    text += &tab.document() == &window.document() ? " document" : " other-document";
    text += &tab.clipboard() == static_cast<const SimulationClipboard*>(&clipboard)
                ? " clipboard"
                : " other-clipboard";
    text += designOf(window).rocketActions().simulationPanel() == &tab ? " actions" : " no-actions";
    return text + (tab.window() == &window ? " window" : " other-window");
}

TEST_F(SimulationsWindowTest, EveryWindowHasTheTabWiredToItsDesign)
{
    const MainWindow* const first = openExample();
    ASSERT_NE(first, nullptr);
    const MainWindow& second = manager().newDesign();
    // BasicFrame's tab "Flight simulations" at index 2.
    EXPECT_EQ(wiringOf(*first, clipboard()),
              "tab=2 text='Flight simulations' panel document clipboard actions window");
    EXPECT_EQ(wiringOf(second, clipboard()),
              "tab=2 text='Flight simulations' panel document clipboard actions window");
    // The File menu's "Export simulation table as CSV file" is the tab's action: enabled while the
    // table has rows, and triggered it asks the tab for the export of every row.
    EXPECT_TRUE(first->menus().exportSimulationTableAction()->isEnabled());
    EXPECT_FALSE(second.menus().exportSimulationTableAction()->isEnabled());
    const QSignalSpy exports(&simulationsOf(*first), &SimulationPanel::csvExportRequested);
    first->menus().exportSimulationTableAction()->trigger();
    ASSERT_EQ(exports.count(), 1);
    EXPECT_FALSE(exports.at(0).at(0).toBool());
    deleteEverySimulation(*first);
    EXPECT_FALSE(first->menus().exportSimulationTableAction()->isEnabled());
    first->menus().undoAction()->trigger();
    EXPECT_TRUE(first->menus().exportSimulationTableAction()->isEnabled());
}

// ============================================================================ the tab switches

TEST_F(SimulationsWindowTest, ChoosingTheTabGivesItsTableTheFocusAndItsRememberedSelection)
{
    MainWindow* const shown = openExample();
    ASSERT_NE(shown, nullptr);
    MainWindow& window = *shown;
    ASSERT_TRUE(activate(window));
    SimulationTableView& table = simulationsOf(window).table();
    EXPECT_EQ(designSelectionOf(window), "[Sustainer]");

    // BasicFrame.stateChanged(): takeTheSpotlight() gives the table the focus and, as none of its
    // rows is selected, selects the first one, which deselects the components.
    showTab(window, MainWindow::kSimulationTab);
    EXPECT_EQ(QApplication::focusWidget(), &table);
    EXPECT_EQ(simulationSelectionOf(window), "[Simulation 1]");
    EXPECT_EQ(designSelectionOf(window), "[]");

    // The design tab gives the tree the focus; the simulations stay selected (so the Edit menu
    // acts on them there, as in Java).
    table.selectSimulations({rowOf(window, 2), rowOf(window, 3)});
    showTab(window, MainWindow::kDesignTab);
    EXPECT_EQ(QApplication::focusWidget(), &designOf(window).tree());
    EXPECT_EQ(simulationSelectionOf(window), "[Simulation 3 - too short delay, Simulation 4]");
    EXPECT_EQ(enabledEditItems(window.menus()), "edit cut copy duplicate delete");

    // A component selected there deselects them; the tab selects them again when the user's
    // click chooses it (updatePreviousSelection() when it was left).
    designOf(window).selection().setSelectedComponent(window.document().getRocket().getChild(0));
    EXPECT_EQ(simulationSelectionOf(window), "[]");
    QTabBar* const bar = window.tabs()->tabBar();
    QTest::mouseClick(bar, Qt::LeftButton, {}, bar->tabRect(MainWindow::kSimulationTab).center());
    QCoreApplication::processEvents();
    EXPECT_EQ(window.selectedTab(), MainWindow::kSimulationTab);
    EXPECT_EQ(QApplication::focusWidget(), &table);
    EXPECT_EQ(simulationSelectionOf(window), "[Simulation 3 - too short delay, Simulation 4]");
    EXPECT_EQ(designSelectionOf(window), "[]");
}

TEST_F(SimulationsWindowTest, ChoosingTheTabRunsTheOutdatedSimulationsWhenThePreferencesAskForIt)
{
    MainWindow* const shown = openExample();
    ASSERT_NE(shown, nullptr);
    MainWindow& window = *shown;
    // A change of the design leaves every simulation out of date.
    RocketComponent& stage = window.document().getRocket().getChild(0);
    stage.setOverrideMass(1.0);
    stage.setMassOverridden(true);
    EXPECT_EQ(simulationsOf(window).tableModel().status(0), Status::OUTDATED);

    // "Automatically run all simulations when the simulations tab is opened" is off by default.
    showTab(window, MainWindow::kSimulationTab);
    EXPECT_TRUE(prompts().runSimulationsQuestions().empty());

    // On, the tab's activating() runs them, in the table's order, as jobs of the window.
    showTab(window, MainWindow::kDesignTab);
    preferences().setAutoRunSimulations(true);
    prompts().answerRunSimulations({.cancelled = true, .allSuccessful = false});
    showTab(window, MainWindow::kSimulationTab);
    ASSERT_EQ(prompts().runSimulationsQuestions().size(), 1U);
    const Gui::Test::ScriptedUserPrompts::RunSimulationsQuestion& run =
        prompts().runSimulationsQuestions().front();
    EXPECT_EQ(run.simulationNames,
              (QStringList{QStringLiteral("Simulation 1"), QStringLiteral("Simulation 2"),
                           QStringLiteral("Simulation 3 - too short delay"),
                           QStringLiteral("Simulation 4"), QStringLiteral("Simulation 5")}));
    EXPECT_EQ(run.window, &window);
    EXPECT_EQ(run.document, &window.document());
    EXPECT_EQ(run.executor, &services().executor());
}

// ============================================================================ the selection

TEST_F(SimulationsWindowTest, TheTreesAndTheTablesSelectionsAreExclusive)
{
    MainWindow* const shown = openExample();
    ASSERT_NE(shown, nullptr);
    MainWindow&             window = *shown;
    SimulationTableView&    table  = simulationsOf(window).table();
    DocumentSelectionModel& tree   = designOf(window).selection();
    const RocketComponent&  stage  = window.document().getRocket().getChild(0);

    // Selecting simulations deselects the components, on any tab.
    table.selectSimulations({rowOf(window, 1)});
    EXPECT_EQ(simulationSelectionOf(window), "[Simulation 2]");
    EXPECT_EQ(designSelectionOf(window), "[]");
    // Selecting components deselects the simulations.
    tree.setSelectedComponent(stage);
    EXPECT_EQ(simulationSelectionOf(window), "[]");
    EXPECT_EQ(designSelectionOf(window), "[Sustainer]");
    // An empty selection on either side leaves the other alone.
    table.selectSimulations({});
    EXPECT_EQ(designSelectionOf(window), "[Sustainer]");
    table.selectSimulations({rowOf(window, 0), rowOf(window, 4)});
    tree.clearComponentSelection();
    EXPECT_EQ(simulationSelectionOf(window), "[Simulation 1, Simulation 5]");

    // The Motors & Configuration tab's spotlight selects its column's components: the
    // simulations go.
    showTab(window, MainWindow::kFlightConfigurationTab);
    EXPECT_NE(designSelectionOf(window), "[]");
    EXPECT_EQ(simulationSelectionOf(window), "[]");
    // The simulations tab's selects its first row: the components go.
    showTab(window, MainWindow::kSimulationTab);
    EXPECT_EQ(simulationSelectionOf(window), "[Simulation 1]");
    EXPECT_EQ(designSelectionOf(window), "[]");
}

// =================================================================== the Edit menu's actions

TEST_F(SimulationsWindowTest, TheEditMenuActsOnTheSelectedSimulationsAndShowsTheirTab)
{
    MainWindow* const shown = openExample();
    ASSERT_NE(shown, nullptr);
    MainWindow&      window = *shown;
    const MainMenus& menus  = window.menus();
    // On the design tab, with a simulation selected: its tab's actions decide (the tree has no
    // selection); Paste needs simulations on the clipboard.
    simulationsOf(window).table().selectSimulations({rowOf(window, 0)});
    EXPECT_EQ(enabledEditItems(menus), "edit cut copy duplicate delete");

    // Copy: the simulation on the clipboard (no components there), the tab shown.
    menus.copyAction()->trigger();
    EXPECT_EQ(window.selectedTab(), MainWindow::kSimulationTab);
    EXPECT_EQ(clipboard().simulations()->size(), 1U);
    EXPECT_TRUE(clipboard().components().empty());
    EXPECT_EQ(enabledEditItems(menus), "edit cut copy paste duplicate delete");

    // Duplicate from the design tab: the duplicate after the others, selected; the tab shown.
    showTab(window, MainWindow::kDesignTab);
    menus.duplicateAction()->trigger();
    EXPECT_EQ(window.selectedTab(), MainWindow::kSimulationTab);
    EXPECT_EQ(simulationsListOf(window),
              "[Simulation 1, Simulation 2, Simulation 3 - too short delay, Simulation 4, "
              "Simulation 5, Simulation 6]");
    EXPECT_EQ(simulationSelectionOf(window), "[Simulation 6]");
    EXPECT_EQ(window.document().getUndoDescription().value_or("-"), "Duplicate Simulation 1");

    // Paste: the copy right after the selected simulation, selected.
    menus.pasteAction()->trigger();
    EXPECT_EQ(simulationsListOf(window),
              "[Simulation 1, Simulation 2, Simulation 3 - too short delay, Simulation 4, "
              "Simulation 5, Simulation 6, Simulation 7]");
    EXPECT_EQ(simulationSelectionOf(window), "[Simulation 7]");

    // Edit: the settings dialog of the selection, on its Launch conditions tab.
    simulationsOf(window).table().selectSimulations({rowOf(window, 1), rowOf(window, 3)});
    menus.editAction()->trigger();
    ASSERT_EQ(dialogRequests().size(), 1U);
    EXPECT_EQ(dialogRequests().front().simulationNames,
              (QStringList{QStringLiteral("Simulation 2"), QStringLiteral("Simulation 4")}));
    EXPECT_EQ(dialogRequests().front().tab, Tab::LAUNCH_CONDITIONS);
    EXPECT_FALSE(dialogRequests().front().newSimulation);
    EXPECT_EQ(window.selectedTab(), MainWindow::kSimulationTab);

    // Delete from the design tab, asked first; the tab shown, its first row selected by its
    // spotlight.
    showTab(window, MainWindow::kDesignTab);
    prompts().answerDeleteSimulations({.confirmed = true, .doNotAskAgain = false});
    menus.deleteAction()->trigger();
    QCoreApplication::processEvents();
    EXPECT_EQ(window.selectedTab(), MainWindow::kSimulationTab);
    EXPECT_EQ(simulationsListOf(window),
              "[Simulation 1, Simulation 3 - too short delay, Simulation 5, Simulation 6, "
              "Simulation 7]");
    EXPECT_EQ(window.document().getUndoDescription().value_or("-"), "Delete simulations");

    // Cut: copied, asked, deleted.
    simulationsOf(window).table().selectSimulations({rowOf(window, 4)});
    prompts().answerDeleteSimulations({.confirmed = true, .doNotAskAgain = false});
    menus.cutAction()->trigger();
    EXPECT_EQ(simulationsListOf(window),
              "[Simulation 1, Simulation 3 - too short delay, Simulation 5, Simulation 6]");
    EXPECT_EQ(clipboard().simulations()->size(), 1U);

    // With a component selected instead, they are the components' again: Copy copies it and
    // shows the design tab.
    designOf(window).selection().setSelectedComponent(window.document().getRocket().getChild(0));
    menus.copyAction()->trigger();
    EXPECT_EQ(window.selectedTab(), MainWindow::kDesignTab);
    EXPECT_FALSE(clipboard().components().empty());
    EXPECT_TRUE(clipboard().simulations()->empty());
    EXPECT_EQ(window.document().getSimulationCount(), 4U);
}

TEST_F(SimulationsWindowTest, TheKeysInTheTableReachTheSimulations)
{
    MainWindow* const shown = openExample();
    ASSERT_NE(shown, nullptr);
    MainWindow& window = *shown;
    ASSERT_TRUE(activate(window));
    showTab(window, MainWindow::kSimulationTab);
    SimulationTableView& table = simulationsOf(window).table();
    ASSERT_EQ(QApplication::focusWidget(), &table);
    // The table leaves Ctrl+D, Copy, Paste and Delete to the window's actions (SimulationPanel
    // binds them to "none" in its table), which act on its selection: the first row.
    QTest::keyClick(&table, Qt::Key_D, Qt::ControlModifier);
    EXPECT_EQ(window.document().getSimulationCount(), 6U);
    EXPECT_EQ(simulationSelectionOf(window), "[Simulation 6]");
    QTest::keySequence(&table, QKeySequence(QKeySequence::Copy));
    QTest::keySequence(&table, QKeySequence(QKeySequence::Paste));
    EXPECT_EQ(window.document().getSimulationCount(), 7U);
    prompts().answerDeleteSimulations({.confirmed = true, .doNotAskAgain = false});
    QTest::keyClick(&table, Qt::Key_Delete);
    EXPECT_EQ(window.document().getSimulationCount(), 6U);
    EXPECT_EQ(window.selectedTab(), MainWindow::kSimulationTab);
}

TEST_F(SimulationsWindowTest, PasteGoesToTheSimulationsTabWithoutASelectedSimulation)
{
    MainWindow* const shown = openExample();
    ASSERT_NE(shown, nullptr);
    MainWindow&          window = *shown;
    const MainMenus&     menus  = window.menus();
    SimulationTableView& table  = simulationsOf(window).table();
    table.selectSimulations({rowOf(window, 1)});
    menus.copyAction()->trigger();
    // The tab's spotlight, which its choice queued.
    QCoreApplication::processEvents();

    // OpenRocket bug 27 (U-M4-13): on the simulations tab, with no simulation selected, Paste
    // pastes them at the end of the list.
    table.selectSimulations({});
    EXPECT_EQ(designSelectionOf(window), "[]");
    EXPECT_EQ(enabledEditItems(menus), "paste");
    menus.pasteAction()->trigger();
    EXPECT_EQ(window.document().getSimulationCount(), 6U);
    EXPECT_EQ(window.document().getSimulation(5)->getName(), "Simulation 6");
    EXPECT_EQ(simulationSelectionOf(window), "[Simulation 6]");

    // On another tab Paste needs a selected simulation: none, or a component, and it is disabled
    // (the clipboard holds no component).
    table.selectSimulations({});
    showTab(window, MainWindow::kDesignTab);
    EXPECT_EQ(enabledEditItems(menus), "");
    designOf(window).selection().setSelectedComponent(window.document().getRocket().getChild(0));
    EXPECT_FALSE(menus.pasteAction()->isEnabled());

    // A design without simulations takes them on its tab too, where the tree keeps its selection
    // (the table has no row to select), which the pasted simulation then takes over.
    MainWindow* const emptied = openExample();
    ASSERT_NE(emptied, nullptr);
    deleteEverySimulation(*emptied);
    designOf(*emptied).selection().setSelectedComponent(
        emptied->document().getRocket().getChild(0));
    showTab(*emptied, MainWindow::kSimulationTab);
    EXPECT_EQ(designSelectionOf(*emptied), "[Sustainer]");
    EXPECT_TRUE(emptied->menus().pasteAction()->isEnabled());
    emptied->menus().pasteAction()->trigger();
    EXPECT_EQ(simulationsListOf(*emptied), "[Simulation 1]");
    EXPECT_EQ(simulationSelectionOf(*emptied), "[Simulation 1]");
    EXPECT_EQ(designSelectionOf(*emptied), "[]");
    EXPECT_EQ(emptied->selectedTab(), MainWindow::kSimulationTab);
}

// ======================================================================= undo across the tabs

TEST_F(SimulationsWindowTest, UndoAndRedoReachTheSimulationsFromEveryTab)
{
    MainWindow* const shown = openExample();
    ASSERT_NE(shown, nullptr);
    MainWindow&                 window = *shown;
    const MainMenus&            menus  = window.menus();
    const SimulationTableModel& model  = simulationsOf(window).tableModel();
    simulationsOf(window).table().selectSimulations({rowOf(window, 1)});
    prompts().answerDeleteSimulations({.confirmed = true, .doNotAskAgain = false});
    menus.deleteAction()->trigger();
    EXPECT_EQ(model.rowCount(), 4);

    // The window's Undo and Redo, on the design tab, reach the table.
    showTab(window, MainWindow::kDesignTab);
    menus.undoAction()->trigger();
    EXPECT_EQ(model.rowCount(), 5);
    EXPECT_EQ(model.simulation(1)->getName(), "Simulation 2");
    EXPECT_EQ(window.selectedTab(), MainWindow::kDesignTab);
    menus.redoAction()->trigger();
    EXPECT_EQ(model.rowCount(), 4);

    // A change of the design made on the design tab (the Edit menu's Delete of a component)
    // leaves the simulations out of date in the table; its undo goes back to the design.
    const RocketComponent& noseCone = window.document().getRocket().getChild(0).getChild(0);
    const std::string      name     = noseCone.getName();
    designOf(window).selection().setSelectedComponent(noseCone);
    menus.deleteAction()->trigger();
    EXPECT_EQ(window.selectedTab(), MainWindow::kDesignTab);
    showTab(window, MainWindow::kSimulationTab);
    EXPECT_EQ(statusesOf(model), std::vector<Status>(4, Status::OUTDATED));
    menus.undoAction()->trigger();
    EXPECT_EQ(window.document().getRocket().getChild(0).getChild(0).getName(), name);
    EXPECT_EQ(model.rowCount(), 4);
}

// ===================================================================== the preferences dialog

/// The unit of the apogee of the first row of @p window's table: the text after its last space.
[[nodiscard]] std::string apogeeUnitOf(const MainWindow& window)
{
    const std::string text =
        QtStrings::toUtf8(simulationsOf(window).tableModel().cellText(0, Column::APOGEE));
    const std::size_t space = text.rfind(' ');
    return space == std::string::npos ? text : text.substr(space + 1);
}

/// Chooses the unit @p unit of the distances on the Units page of @p dialog.
void chooseDistanceUnit(const PreferencesDialog& dialog, std::string_view unit)
{
    const UnitGroup& distance = unitGroup(UnitGroupId::DISTANCE);
    Gui::Test::chooseItem(*dialog.unitsPage()->unitSelector(UnitGroupId::DISTANCE),
                          distance.getUnitIndex(*distance.getUnit(unit)));
}

TEST_F(SimulationsWindowTest, ThePreferencesDialogsRefreshReadsEveryTableAgain)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    const MainWindow* const                 first = openExample();
    ASSERT_NE(first, nullptr);
    const MainWindow* const second = openExample();
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(apogeeUnitOf(*first), "m");

    // A unit chosen on the Units page shows in the tables once the dialog applies it (no
    // document event tells of a unit).
    first->menus().preferencesAction()->trigger();
    const PreferencesDialog* const dialog = manager().preferencesDialog();
    ASSERT_NE(dialog, nullptr);
    chooseDistanceUnit(*dialog, "ft");
    EXPECT_EQ(apogeeUnitOf(*first), "m");
    dialog->applyButton()->click();
    EXPECT_EQ(apogeeUnitOf(*first), "ft");
    EXPECT_EQ(apogeeUnitOf(*second), "ft");

    // OK too.
    chooseDistanceUnit(*dialog, "m");
    dialog->okButton()->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_EQ(apogeeUnitOf(*first), "m");
    EXPECT_EQ(apogeeUnitOf(*second), "m");
}

// ======================================================================== the motor chooser

/// The first motor mount of @p rocket that is one and has a motor (null when there is none).
[[nodiscard]] const MotorMount* firstMountWithMotorOf(const Rocket& rocket)
{
    for (const RocketComponent& component : rocket.subtree())
    {
        if (const auto* mount = dynamic_cast<const MotorMount*>(&component);
            mount != nullptr && mount->isMotorMount() && mount->hasMotor())
        {
            return mount;
        }
    }
    return nullptr;
}

/// The motor chooser's answer: "Use manufacturer's designation", then Cancel.
void chooseDesignationsAndCancel(const QWidget& dialog)
{
    auto* designation = dialog.findChild<QRadioButton*>(QStringLiteral("designation"));
    auto* cancel      = dialog.findChild<QPushButton*>(QStringLiteral("cancelButton"));
    if (designation == nullptr || cancel == nullptr)
    {
        bug("the modal dialog is no motor chooser");
    }
    designation->click();
    cancel->click();
}

TEST_F(SimulationsWindowTest, TheMotorChoosersNameColumnShowsInEveryTable)
{
    // The chooser's name column (a motor by its common name or its designation) names the motors
    // of the tables' configurations too, which OpenRocket's tables paint anew: the tables are read
    // again when the chooser's question ends, even cancelled (no document event tells of it).
    preferences().setMotorNameColumn(false);
    // "[G40-7]" by the common name, "[G40W-7]" by the designation.
    const MainWindow* const first = openExample("Chute release.ork");
    ASSERT_NE(first, nullptr);
    const MainWindow* const second = openExample("Chute release.ork");
    ASSERT_NE(second, nullptr);
    const SimulationTableModel&       model      = simulationsOf(*first).tableModel();
    const std::shared_ptr<Simulation> simulation = model.simulation(0);
    const QString                     commonName = model.cellText(0, Column::CONFIGURATION);
    const MotorMount* const           mount = firstMountWithMotorOf(first->document().getRocket());
    ASSERT_NE(mount, nullptr);

    WidgetUserPrompts widgets;
    {
        const Gui::Test::NextModalDialog answer(chooseDesignationsAndCancel);
        EXPECT_EQ(
            widgets.chooseMotor(first->tabs(), services().services(), *mount, simulation->getId()),
            std::nullopt);
    }
    EXPECT_TRUE(preferences().getMotorNameColumn());
    const QString designation = QtStrings::fromUtf8(first->document()
                                                        .getRocket()
                                                        .getFlightConfiguration(simulation->getId())
                                                        .getName(preferences()));
    EXPECT_NE(designation, commonName);
    EXPECT_EQ(model.cellText(0, Column::CONFIGURATION), designation);
    EXPECT_EQ(simulationsOf(*second).tableModel().cellText(0, Column::CONFIGURATION), designation);
}

// ======================================================================== every pinned design

/// The pinned design of a golden test's parameter, through its window.
class SimulationsWindowGolden : public SimulationsWindowTest,
                                public ::testing::WithParamInterface<std::string>
{
protected:
    /// The window of @p pin's design, loaded as the pins' probe loaded it, its simulations tab
    /// chosen; null (and a failure) when it cannot be opened.
    [[nodiscard]] MainWindow* openPinned(const Gui::Test::SimulationTablePinDesign& pin)
    {
        // The probe's configuration column names no motor (ServicesForTesting).
        preferences().setMotorNameColumn(false);
        MainWindow* const window =
            pin.kind == "new" ? &manager().newDesign() : openExample(pin.file);
        if (window != nullptr)
        {
            showTab(*window, MainWindow::kSimulationTab);
        }
        return window;
    }
};

/// Where the states of @p menus' items differ from @p pinned, the states of OpenRocket's tab's
/// actions: the File menu's export always, and the Edit menu's six when @p editItems.
[[nodiscard]] std::vector<std::string> menuStateDifferences(
    const MainMenus& menus, const std::map<std::string, bool>& pinned, bool editItems)
{
    std::vector<std::pair<std::string, const QAction*>> items{
        {"simTableExport", menus.exportSimulationTableAction()}};
    if (editItems)
    {
        items.insert(items.end(), {{"editSimulation", menus.editAction()},
                                   {"cutSimulation", menus.cutAction()},
                                   {"copySimulation", menus.copyAction()},
                                   {"pasteSimulation", menus.pasteAction()},
                                   {"duplicateSimulation", menus.duplicateAction()},
                                   {"deleteSimulation", menus.deleteAction()}});
    }
    std::vector<std::string> differences;
    for (const auto& [name, action] : items)
    {
        const auto found = pinned.find(name);
        if (found == pinned.end() || found->second != action->isEnabled())
        {
            differences.push_back(name);
        }
    }
    return differences;
}

TEST_P(SimulationsWindowGolden, ShowsTheTableAsOpenRocket)
{
    const Gui::Test::SimulationTablePins* const pins = Gui::Test::simulationTablePinsOrFail();
    ASSERT_NE(pins, nullptr);
    const Gui::Test::SimulationTablePinDesign* const pin = pins->findDesign(GetParam());
    ASSERT_NE(pin, nullptr);
    const MainWindow* const window = openPinned(*pin);
    ASSERT_NE(window, nullptr);
    const SimulationPanel& tab = simulationsOf(*window);
    EXPECT_EQ(tab.isHelpShown(), !pin->hasValidConfig);
    EXPECT_EQ(window->document().getSimulationCount(), pin->simulationCount);
    Gui::Test::expectSimulationTablePinRows(tab.tableModel(), pin->simulations);
}

TEST_P(SimulationsWindowGolden, EnablesTheMenusItemsAsOpenRocketsTab)
{
    const Gui::Test::SimulationTablePins* const pins = Gui::Test::simulationTablePinsOrFail();
    ASSERT_NE(pins, nullptr);
    const Gui::Test::SimulationTablePinDesign* const pin = pins->findDesign(GetParam());
    ASSERT_NE(pin, nullptr);
    const MainWindow* const window = openPinned(*pin);
    ASSERT_NE(window, nullptr);
    // The tab's spotlight selected its first row when it has rows, which deselected the
    // components: the Edit menu's items are then the tab's actions (RocketActions'
    // clipboardChanged(): the component rule or the tab's), whose states OpenRocket's own tab had
    // for its first row ("row0"). Without rows the tree keeps its selection, and only the File
    // menu's export is the tab's.
    const bool rows = pin->simulationCount > 0;
    EXPECT_EQ(simulationSelectionOf(*window) != "[]", rows);
    EXPECT_EQ(menuStateDifferences(window->menus(), pin->actions.at(rows ? "row0" : "none"), rows),
              std::vector<std::string>{});
}

INSTANTIATE_TEST_SUITE_P(Designs, SimulationsWindowGolden,
                         ::testing::ValuesIn(Gui::Test::simulationTablePinnedDesigns()),
                         Gui::Test::simulationTablePinTestName);

}  // namespace

}  // namespace QtRocket::Gui
