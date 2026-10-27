// The "Motors & Configuration" tab in its window, as WindowManager puts one into every window
// (BasicFrame's FlightConfigurationPanel at FLIGHT_CONFIGURATION_TAB): its wiring to the window and
// its design (the design's selection, the configuration dialog's close requests, the open
// documents of "Save as default"), the spotlight when the tab is chosen, the design view's
// configuration combo box and the tables following each other, the component actions that show
// the design tab, the tables' own Cut, Copy and Paste keys, and the tab's operations end to end
// with a second window open (New with the motor chooser, ignition, rename with {motors},
// duplicate, delete, undo and redo; the questions scripted).

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QModelIndex>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/figure/RocketPanel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurablePanel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationEditing.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/MotorConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/MotorMountConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/MotorMountTableModel.h"
#include "QtRocket/gui/flightconfig/RecoveryConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/RowColumnSelectionModel.h"
#include "QtRocket/gui/main/DesignPanel.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/main/RocketActions.h"
#include "QtRocket/gui/main/UndoRedoActions.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/ConfigurationComboBox.h"
#include "QtRocket/gui/widgets/IconButton.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "flightconfig/FlightConfigurationTabSupport.h"
#include "motor/TestMotorDatabase.h"
#include "tree/ProbeDesign.h"

namespace QtRocket::Gui
{

namespace
{

using Tab       = Gui::Test::FlightConfigurationTab;
using Booster   = Gui::Test::ProbeDesign::Booster;
using Documents = std::vector<OpenRocketDocument*>;

/// The probes' design of one stage (ProbeDesign: Sustainer[NC, BT1[IT[EB], CR1, Fins],
/// BT2[Chute]]), with IT acting as a motor mount and @p configurations configurations without a
/// motor, the first of them selected when there is one, no undo history, saved (as a design
/// that was just loaded).
[[nodiscard]] std::unique_ptr<OpenRocketDocument> mountedDesign(int configurations)
{
    std::unique_ptr<OpenRocketDocument> document =
        Gui::Test::ProbeDesign::makeDocument(Booster::NONE);
    Rocket& rocket = document->getRocket();
    dynamic_cast<MotorMount&>(Gui::Test::ProbeDesign::find(rocket, "IT")).setMotorMount(true);
    for (int i = 0; i < configurations; ++i)
    {
        const FlightConfigurationId id = rocket.createFlightConfiguration().getId();
        if (i == 0)
        {
            rocket.setSelectedConfiguration(id);
        }
    }
    document->clearUndo();
    document->setSaved(true);
    return document;
}

/// The tab of @p window.
[[nodiscard]] FlightConfigurationPanel& tabOf(const MainWindow& window)
{
    FlightConfigurationPanel* const panel = window.componentFlightConfigurationPanel();
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

/// The design view's configuration combo box of @p window.
[[nodiscard]] ConfigurationComboBox& comboOf(const MainWindow& window)
{
    const RocketPanel* const panel = window.componentRocketPanel();
    QTROCKET_ASSERT(panel != nullptr);
    return panel->configurationSelector();
}

/// How @p window's tab is wired, in one line: its index and text, whether its document, its
/// design selection and its questions' parent are the window's, the open documents of "Save as
/// default" (by their windows' order in @p documents), and the current tab.
[[nodiscard]] std::string wiringOf(const MainWindow& window, const Documents& documents)
{
    const FlightConfigurationPanel& tab = tabOf(window);
    const int   index = window.tabs()->indexOf(window.componentFlightConfigurationPanel());
    std::string text  = "tab=" + std::to_string(index) + " text='" +
                        QtStrings::toUtf8(window.tabs()->tabText(index)) + "'";
    text += &tab.document() == &window.document() ? " document" : " other-document";
    text += tab.designSelection() == &designOf(window).selection() ? " selection" : " no-selection";
    text += tab.editing().dialogParent() == &tab ? " parent" : " other-parent";
    text += " documents=[";
    const Documents open = tab.editing().openDocuments();
    for (std::size_t i = 0; i < open.size(); ++i)
    {
        const auto found = std::ranges::find(documents, open[i]);
        text += (i > 0 ? ", " : "") + (found == documents.end()
                                           ? std::string("?")
                                           : std::to_string(found - documents.begin()));
    }
    return text + "] current=" + std::to_string(window.selectedTab());
}

/// The rows of @p panel's table, "name: cell, cell; name: ..." (a motor cell with its ignition
/// after " / ", a shaded one with a "~" before it).
[[nodiscard]] std::string tableOf(const FlightConfigurablePanel& panel)
{
    const FlightConfigurableTableModel& model = panel.tableModel();
    std::string                         text;
    for (int row = 0; row < model.rowCount(); ++row)
    {
        text +=
            (row > 0 ? "; " : "") + QtStrings::toUtf8(model.index(row, 0).data().toString()) + ":";
        for (int column = 1; column < model.columnCount(); ++column)
        {
            const QModelIndex index = model.index(row, column);
            text += column > 1 ? ", " : " ";
            text += QtStrings::toUtf8(index.data().toString());
            if (const QString ignition =
                    index.data(FlightConfigurableTableModel::kIgnitionTextRole).toString();
                !ignition.isEmpty())
            {
                text +=
                    std::string(" / ") +
                    (index.data(FlightConfigurableTableModel::kDefaultRole).toBool() ? "~" : "") +
                    QtStrings::toUtf8(ignition);
            }
        }
    }
    return text;
}

/// What the window shows of its design's configurations: the motor table, the design view's
/// combo box, the undo step and the number of simulations.
[[nodiscard]] std::string stateOf(const MainWindow& window)
{
    const OpenRocketDocument& document = window.document();
    return "motors={" + tableOf(tabOf(window).motorConfigurationPanel()) + "} combo='" +
           QtStrings::toUtf8(comboOf(window).currentText()) + "' undo='" +
           document.getUndoDescription().value_or("-") +
           "' simulations=" + std::to_string(document.getSimulationCount());
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

/// Shows the Motors & Configuration tab of @p window, triggers @p action when it is enabled, and
/// tells what happened: "disabled", or "tab=<the current tab then>".
[[nodiscard]] std::string tabAfter(MainWindow& window, QAction* action)
{
    window.selectTab(MainWindow::kFlightConfigurationTab);
    if (!action->isEnabled())
    {
        return "disabled";
    }
    action->trigger();
    return "tab=" + std::to_string(window.selectedTab());
}

/// The clipboard's plain text and HTML, "<text>|<html>"; "-" for an empty clipboard.
[[nodiscard]] std::string clipboardText()
{
    const QMimeData* data = QGuiApplication::clipboard()->mimeData();
    if (data == nullptr || data->formats().isEmpty())
    {
        return "-";
    }
    return QtStrings::toUtf8(data->text()) + "|" + QtStrings::toUtf8(data->html());
}

/// The windows of a window manager over test services and a component clipboard of the test's own,
/// each window's requests to close the configuration dialog counted.
class FlightConfigurationWindowTest : public Gui::Test::GuiTest
{
protected:
    void TearDown() override
    {
        // The windows, and the documents and panels they leave to deleteLater(), go first.
        m_manager = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QGuiApplication::clipboard()->clear();
        GuiTest::TearDown();
    }

    /// Opens a window on @p document and counts its requests to close the configuration dialog.
    MainWindow& open(std::unique_ptr<OpenRocketDocument> document)
    {
        MainWindow& window = manager().addDocument(std::move(document));
        QObject::connect(&window, &MainWindow::closeConfigDialogRequested, &window,
                         [this, &window] { m_closeRequests.push_back(&window); });
        return window;
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

    /// The number of requests of @p window to close the configuration dialog so far.
    [[nodiscard]] std::ptrdiff_t closeRequests(const MainWindow& window) const
    {
        return std::ranges::count(m_closeRequests, &window);
    }

    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() { return m_services.prompts(); }
    [[nodiscard]] const OpenRocketClipboard&      components() const { return m_clipboard; }
    [[nodiscard]] const Preferences&              preferences() { return m_services.preferences(); }

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
    std::vector<const MainWindow*> m_closeRequests;
};

// ================================================================================ the wiring

TEST_F(FlightConfigurationWindowTest, EveryWindowHasTheTabWiredToItsDesign)
{
    const MainWindow& first     = open(mountedDesign(0));
    MainWindow&       second    = open(OpenRocketDocumentFactory::createNewRocket());
    const Documents   documents = {&first.document(), &second.document()};
    // BasicFrame's tab "Motors & Configuration" at index 1, with the window's design and design
    // selection; "Save as default" pins the names of every open window's design.
    EXPECT_EQ(wiringOf(first, documents),
              "tab=1 text='Motors && Configuration' document selection parent documents=[0, 1] "
              "current=0");
    EXPECT_EQ(wiringOf(second, documents),
              "tab=1 text='Motors && Configuration' document selection parent documents=[0, 1] "
              "current=0");
    EXPECT_EQ(manager().documents(), documents);

    // A closed window's design is no longer among them.
    ASSERT_TRUE(manager().close(second));
    EXPECT_EQ(wiringOf(first, documents),
              "tab=1 text='Motors && Configuration' document selection parent documents=[0] "
              "current=0");
    EXPECT_EQ(manager().documents(), Documents{&first.document()});
}

TEST_F(FlightConfigurationWindowTest, TheTabAsksTheWindowToCloseTheDialogBeforeItsSteps)
{
    const MainWindow&               window = open(mountedDesign(1));
    const FlightConfigurationPanel& tab    = tabOf(window);
    // Duplicate adds its undo step (bug 5) after the close request; Rename before its question.
    tab.duplicateConfigurationAction()->trigger();
    EXPECT_EQ(closeRequests(window), 1);
    prompts().answerRenameConfiguration({.action = UserPrompts::RenameAction::CANCEL, .name = {}});
    tab.renameConfigurationAction()->trigger();
    EXPECT_EQ(closeRequests(window), 2);
    // The cancelled rename changed nothing since its undo position (Java's too): the step is
    // Duplicate's still.
    EXPECT_EQ(window.document().getUndoDescription().value_or("-"), "Duplicate Configuration");
}

// ============================================================================ the spotlight

TEST_F(FlightConfigurationWindowTest, ChoosingTheTabGivesItsTableTheFocusAndItsColumnsTheDesign)
{
    MainWindow& window = open(mountedDesign(1));
    ASSERT_TRUE(activate(window));
    const FlightConfigurationPanel& tab = tabOf(window);
    // The tables start on the selected configuration in their last column, which reaches neither
    // the rocket nor the design (Java's doTableInitialization()).
    EXPECT_EQ(Tab::lines(tab.motorConfigurationPanel()), "rows=[0] cols=[1]");
    EXPECT_EQ(designSelectionOf(window), "[Sustainer]");

    // BasicFrame.stateChanged(): the current sub-tab's table takes the focus and its selected
    // columns' components become the design's selection.
    window.selectTab(MainWindow::kFlightConfigurationTab);
    EXPECT_EQ(QApplication::focusWidget(), &tab.motorConfigurationPanel().table());
    EXPECT_EQ(designSelectionOf(window), "[IT]");

    // The design tab gives the tree the focus; the selection stays.
    window.selectTab(MainWindow::kDesignTab);
    EXPECT_EQ(QApplication::focusWidget(), &designOf(window).tree());
    EXPECT_EQ(designSelectionOf(window), "[IT]");

    // The user's click on the tab does the same; the Recovery sub-tab's table when it is the
    // current one (the Chute's column).
    tab.tabs()->setCurrentIndex(FlightConfigurationPanel::kRecoveryTabIndex);
    window.selectTab(MainWindow::kDesignTab);
    designOf(window).selection().clearComponentSelection();
    QTabBar* const bar = window.tabs()->tabBar();
    QTest::mouseClick(bar, Qt::LeftButton, {},
                      bar->tabRect(MainWindow::kFlightConfigurationTab).center());
    EXPECT_EQ(window.selectedTab(), MainWindow::kFlightConfigurationTab);
    EXPECT_EQ(QApplication::focusWidget(), &tab.recoveryConfigurationPanel().table());
    EXPECT_EQ(designSelectionOf(window), "[Chute]");
}

// =================================================================== the design view's combo

TEST_F(FlightConfigurationWindowTest, TheDesignViewsConfigurationAndTheTablesFollowEachOther)
{
    MainWindow&                    window = open(mountedDesign(2));
    const MotorConfigurationPanel& motors = tabOf(window).motorConfigurationPanel();
    ConfigurationComboBox&         combo  = comboOf(window);
    const Rocket&                  rocket = window.document().getRocket();
    window.selectTab(MainWindow::kFlightConfigurationTab);

    // A row of the table selects the configuration in the rocket, which the combo box shows.
    Tab::click(motors, 1, 1);
    EXPECT_EQ(rocket.getSelectedConfiguration().getId(), rocket.getFlightConfigurationId(1));
    EXPECT_EQ(combo.selectedConfigurationId(), rocket.getFlightConfigurationId(1));

    // The combo box's choice (the user's key: the configuration before) selects the row of every
    // table.
    QTest::keyClick(&combo, Qt::Key_Up);
    EXPECT_EQ(rocket.getSelectedConfiguration().getId(), rocket.getFlightConfigurationId(0));
    EXPECT_EQ(Tab::lines(motors), "rows=[0] cols=[1]");
    EXPECT_EQ(Tab::lines(tabOf(window).recoveryConfigurationPanel()), "rows=[0] cols=[1]");

    // The default configuration (the combo box's first item) leaves the tables without a
    // selection.
    QTest::keyClick(&combo, Qt::Key_Up);
    EXPECT_TRUE(rocket.getSelectedConfiguration().getId().isDefaultId());
    EXPECT_EQ(Tab::lines(motors), "rows=[] cols=[]");
    EXPECT_FALSE(tabOf(window).renameConfigurationAction()->isEnabled());
}

// =================================================================== the component actions

TEST_F(FlightConfigurationWindowTest, CutCopyPasteDuplicateAndDeleteShowTheDesignTab)
{
    MainWindow&      window = open(mountedDesign(1));
    const MainMenus& menus  = window.menus();
    // RocketActions.java:691, 747, 829, 926 and 981: parentFrame.selectTab(DESIGN_TAB). The
    // tab's spotlight makes its selected column's component (IT) the design's selection each
    // time.
    EXPECT_EQ(tabAfter(window, menus.copyAction()), "tab=0");
    EXPECT_EQ(designSelectionOf(window), "[IT]");
    EXPECT_EQ(tabAfter(window, menus.duplicateAction()), "tab=0");
    EXPECT_EQ(designSelectionOf(window), "[IT]");
    EXPECT_EQ(tabAfter(window, menus.pasteAction()), "tab=0");
    // The others leave the tab as it is.
    EXPECT_EQ(tabAfter(window, designOf(window).rocketActions().moveUpAction()), "tab=1");
    EXPECT_EQ(tabAfter(window, menus.toggleVisibilityAction()), "tab=1");
    EXPECT_EQ(tabAfter(window, menus.editAction()), "tab=1");
    EXPECT_EQ(tabAfter(window, menus.deleteAction()), "tab=0");
    EXPECT_EQ(tabAfter(window, menus.cutAction()), "tab=0");
    // An Edit action triggered from the design tab leaves it there.
    window.selectTab(MainWindow::kDesignTab);
    menus.undoAction()->trigger();
    EXPECT_EQ(window.selectedTab(), MainWindow::kDesignTab);
}

TEST_F(FlightConfigurationWindowTest, TheTablesTakeCutCopyAndPasteForThemselves)
{
    MainWindow& window = open(mountedDesign(1));
    ASSERT_TRUE(activate(window));
    const MotorConfigurationPanel& motors = tabOf(window).motorConfigurationPanel();
    QTableView&                    table  = motors.table();
    const Rocket&                  rocket = window.document().getRocket();
    const std::string              id     = rocket.getFlightConfigurationId(0).toString();
    window.selectTab(MainWindow::kFlightConfigurationTab);
    ASSERT_EQ(QApplication::focusWidget(), &table);

    // Copy: the selected cell's value as Java's table exports it (a Pair of the configuration id
    // and the mount); the components stay where they are, the tab too.
    QTest::keyClick(&table, Qt::Key_C, Qt::ControlModifier);
    EXPECT_EQ(clipboardText(), "[" + id + ";IT]|<html>\n<body>\n<table>\n<tr>\n  <td>[" + id +
                                   ";IT]</td>\n</tr>\n</table>\n</body>\n</html>");
    EXPECT_TRUE(components().isEmpty());
    EXPECT_EQ(window.selectedTab(), MainWindow::kFlightConfigurationTab);

    // Two columns: the configuration's id, then the cell, separated by a tab.
    motors.tableSelection().setLines({.rows = {0}, .columns = {0, 1}},
                                     RowColumnSelectionModel::Order::COLUMNS_FIRST);
    QTest::keyClick(&table, Qt::Key_C, Qt::ControlModifier);
    EXPECT_EQ(QtStrings::toUtf8(QGuiApplication::clipboard()->text()), id + "\t[" + id + ";IT]");

    // Cut and Paste do nothing: the mount stays, nothing is pasted, the clipboard keeps its text.
    QTest::keyClick(&table, Qt::Key_X, Qt::ControlModifier);
    QTest::keyClick(&table, Qt::Key_V, Qt::ControlModifier);
    EXPECT_EQ(Gui::Test::ProbeDesign::plainTree(rocket),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]]]");
    EXPECT_EQ(QtStrings::toUtf8(QGuiApplication::clipboard()->text()), id + "\t[" + id + ";IT]");
    EXPECT_EQ(window.document().getUndoDescription().value_or("-"), "-");

    // The motor mount table takes them too and copies nothing.
    QGuiApplication::clipboard()->clear();
    QTableView& mounts = motors.motorMountConfigurationPanel().table();
    mounts.setFocus();
    ASSERT_EQ(QApplication::focusWidget(), &mounts);
    QTest::keyClick(&mounts, Qt::Key_C, Qt::ControlModifier);
    QTest::keyClick(&mounts, Qt::Key_X, Qt::ControlModifier);
    EXPECT_EQ(clipboardText(), "-");
    EXPECT_TRUE(components().isEmpty());

    // Duplicate (Ctrl+D) is the window's, as in Java: the design's selection (IT) is duplicated,
    // and the design tab shown.
    table.setFocus();
    QTest::keyClick(&table, Qt::Key_D, Qt::ControlModifier);
    EXPECT_EQ(window.selectedTab(), MainWindow::kDesignTab);
    EXPECT_EQ(Gui::Test::ProbeDesign::plainTree(rocket),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins, IT[EB]], BT2[Chute]]]");
    EXPECT_FALSE(components().isEmpty());
}

// ======================================================================== the save as default

TEST_F(FlightConfigurationWindowTest, SaveAsDefaultPinsTheNamesOfEveryOpenWindowsDesign)
{
    const MainWindow& window = open(mountedDesign(1));
    const MainWindow& other  = open(mountedDesign(1));
    prompts().answerRenameConfiguration({.action = UserPrompts::RenameAction::CANCEL, .name = {}},
                                        {QStringLiteral("Pinned {motors}")});
    tabOf(window).renameConfigurationAction()->trigger();

    // Every configuration keeps the name it had, in both designs, as Java's store theirs; the new
    // default is the preference's, which a new configuration takes.
    const Rocket& rocket      = window.document().getRocket();
    const Rocket& otherRocket = other.document().getRocket();
    EXPECT_EQ(rocket.getFlightConfiguration(rocket.getFlightConfigurationId(0)).getStoredName(),
              "[{motors}]");
    EXPECT_EQ(
        otherRocket.getFlightConfiguration(otherRocket.getFlightConfigurationId(0)).getStoredName(),
        "[{motors}]");
    EXPECT_EQ(preferences().getDefaultFlightConfigName(), "Pinned {motors}");
    EXPECT_EQ(stateOf(other),
              "motors={[No motors]: None / ~Default (Automatic)} combo='[No "
              "motors]' undo='-' simulations=0");
    ASSERT_FALSE(prompts().prompts().empty());
    EXPECT_EQ(prompts().prompts().back().title, QStringLiteral("Save as default"));

    // A new configuration of the other design takes the new default (the motor chooser
    // cancelled).
    prompts().answerMotor(std::nullopt);
    tabOf(other).newConfigurationAction()->trigger();
    EXPECT_EQ(
        QtStrings::toUtf8(
            tabOf(other).motorConfigurationPanel().tableModel().index(1, 0).data().toString()),
        "Pinned No motors");
}

// ======================================================================== the tab end to end

TEST_F(FlightConfigurationWindowTest, TheTabsOperationsEndToEndWithASecondWindowOpen)
{
    MainWindow&       window = open(Gui::Test::ProbeDesign::makeDocument(Booster::NONE));
    const MainWindow& other  = open(mountedDesign(1));
    window.document().clearUndo();
    const std::string                   otherBefore = stateOf(other);
    const FlightConfigurationPanel&     tab         = tabOf(window);
    const MotorConfigurationPanel&      motors      = tab.motorConfigurationPanel();
    const MotorMountConfigurationPanel& mounts      = motors.motorMountConfigurationPanel();
    window.selectTab(MainWindow::kFlightConfigurationTab);
    EXPECT_FALSE(tab.newConfigurationAction()->isEnabled());

    // The motor mount's check box: IT acts as a mount (its own undo step, bug 5).
    const QModelIndex check = mounts.model().index(
        mounts.model().rowOf(
            Gui::Test::ProbeDesign::find(window.document().getRocket(), "IT").getId()),
        MotorMountTableModel::kMotorMountColumn);
    QTest::mouseClick(mounts.table().viewport(), Qt::LeftButton, {},
                      mounts.table().visualRect(check).center());
    EXPECT_TRUE(tab.newConfigurationAction()->isEnabled());
    EXPECT_EQ(stateOf(window),
              "motors={} combo='[No motors]' undo='Modify motor mount' simulations=0");

    // New: a configuration, its simulation, and the motor chooser over its cell (Estes A8, delay
    // 3).
    const std::shared_ptr<const ThrustCurveMotor> a8 = QtRocket::Test::estesA8Motor();
    ASSERT_NE(a8, nullptr);
    prompts().answerMotor(
        UserPrompts::MotorChoice{.motor = a8, .delay = 3, .nozzleExitDiameter = 0});
    QTest::mouseClick(tab.newConfigurationButton(), Qt::LeftButton);
    ASSERT_EQ(prompts().motorQuestions().size(), 1U);
    EXPECT_EQ(prompts().motorQuestions().front().mountName, QStringLiteral("IT"));
    EXPECT_EQ(stateOf(window),
              "motors={[A8-3]: A8-3 / ~Default (Automatic)} combo='[A8-3]' "
              "undo='Select motor' simulations=1");

    // Select ignition for this configuration only: burnout plus 1 s.
    prompts().answerIgnition(
        UserPrompts::IgnitionChoice{.scope = UserPrompts::ConfigurationScope::THIS_CONFIGURATION,
                                    .event = IgnitionEvent::BURNOUT,
                                    .delay = 1});
    QTest::mouseClick(motors.selectIgnitionButton(), Qt::LeftButton);
    EXPECT_EQ(stateOf(window),
              "motors={[A8-3]: A8-3 / Burnout + 1 s} combo='[A8-3]' "
              "undo='Select ignition' simulations=1");

    // Rename with {motors}: the table and the design view's combo box show the motors.
    prompts().answerRenameConfiguration(
        {.action = UserPrompts::RenameAction::RENAME, .name = QStringLiteral("Flight {motors}")});
    QTest::mouseClick(tab.renameConfigurationButton(), Qt::LeftButton);
    EXPECT_EQ(stateOf(window),
              "motors={Flight A8-3: A8-3 / Burnout + 1 s} combo='Flight A8-3' "
              "undo='Rename configuration(s)' simulations=1");

    // Duplicate: a copy with the name, the motor and the ignition, selected, with a simulation.
    QTest::mouseClick(tab.duplicateConfigurationButton(), Qt::LeftButton);
    const std::string duplicated =
        "motors={Flight A8-3: A8-3 / Burnout + 1 s; Flight A8-3: A8-3 / Burnout + 1 s} "
        "combo='Flight A8-3' undo='Duplicate Configuration' simulations=2";
    EXPECT_EQ(stateOf(window), duplicated);
    EXPECT_EQ(Tab::lines(motors), "rows=[1] cols=[1]");

    // Delete: the copy and its simulation go; the rocket selects the default configuration.
    QTest::mouseClick(tab.deleteConfigurationButton(), Qt::LeftButton);
    const std::string deleted =
        "motors={Flight A8-3: A8-3 / Burnout + 1 s} combo='[No motors]' "
        "undo='Remove configuration(s)' simulations=1";
    EXPECT_EQ(stateOf(window), deleted);
    // Each step asked for the configuration dialog to close first: the mount, New, Select motor,
    // Select ignition, Rename, Duplicate and Delete.
    EXPECT_EQ(closeRequests(window), 7);

    // Undo and redo through the window's Edit menu.
    const UndoRedoActions& undoRedo = window.undoRedoActions();
    undoRedo.undoAction()->trigger();
    EXPECT_EQ(stateOf(window), duplicated);
    undoRedo.redoAction()->trigger();
    EXPECT_EQ(stateOf(window), deleted);
    for (int step = 0; step < 7; ++step)
    {
        undoRedo.undoAction()->trigger();
    }
    EXPECT_EQ(stateOf(window), "motors={} combo='[No motors]' undo='-' simulations=0");
    EXPECT_FALSE(tab.newConfigurationAction()->isEnabled());
    for (int step = 0; step < 7; ++step)
    {
        undoRedo.redoAction()->trigger();
    }
    EXPECT_EQ(stateOf(window), deleted);

    // The other window's design saw nothing of it.
    EXPECT_EQ(stateOf(other), otherBefore);
    EXPECT_TRUE(other.document().isSaved());
    EXPECT_EQ(closeRequests(other), 0);
}

}  // namespace

}  // namespace QtRocket::Gui
