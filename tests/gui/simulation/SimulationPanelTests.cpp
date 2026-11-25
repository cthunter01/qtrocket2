// SimulationPanel: the "Flight simulations" tab (Java: SimulationPanel's constructor, its actions
// and their enable rules, the popup menu, the help card and its switch, the mouse listener's
// rules, openDialog(), runSimulation(), plotSimulation(), activating(), takeTheSpotlight(),
// updatePreviousSelection() and the table's CSV export, exportSimulationsToCSV() with
// setUpSimExportCSVFileChooser()). OpenRocket has no JUnit test of the panel (nor one of the
// table export's file: SimulationTableCSVExportTest mocks the table, not ported by O-M4-11). The
// actions' states for no row, the first row and the first two rows of every pinned design are those
// OpenRocket's own panel has (the simulation-table pins' "actions", SimulationTableProbe); the rest
// follows the Java source. The operations themselves are SimulationEditing's (its tests); the
// dialogs are recorded (RecordingSimulationDialogs) and the run dialog scripted
// (ScriptedUserPrompts), so nothing runs.

#include "QtRocket/gui/simulation/SimulationPanel.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAbstractItemModelTester>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QMenu>
#include <QModelIndex>
#include <QObject>
#include <QPoint>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QTextDocument>
#include <QWidget>
#include <Qt>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "ForwardingUserPrompts.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/ComponentCopy.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/SimulationCopy.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/settings/CsvOptionPreferences.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationTableCsvExport.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/simulation/SimulationTableView.h"
#include "QtRocket/gui/util/CsvOptionValues.h"
#include "QtRocket/gui/util/FileNames.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationDialogs.h"
#include "QtRocket/gui/util/TableSortKeys.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/IconButton.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "RecordingSimulationDialogs.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "TestTempDir.h"
#include "simulation/SimulationTablePinChecks.h"
#include "simulation/SimulationTablePins.h"
#include "simulation/SimulationTableTestSupport.h"
#include "tree/TreeViewMouse.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using Column              = SimulationColumns::Column;
using Names               = std::vector<std::string>;
using Request             = Gui::Test::RecordingSimulationDialogs::Request;
using RequestKind         = Gui::Test::RecordingSimulationDialogs::Kind;
using SortKey             = TableSortKeys::SortKey;
using SortOrder           = TableSortKeys::SortOrder;
using Status              = Simulation::Status;
using Tab                 = SimulationDialogs::Tab;
using SimulationPanelTest = Gui::Test::GuiTest;

/// A design's "Flight simulations" tab: the test's services (recorded dialogs, scripted
/// questions, the manual executor), a clipboard of its own, the document and the panel (not
/// shown until show()), its table's model under QAbstractItemModelTester (Warning mode).
class SimulationTab
{
public:
    /// The tab of @p document (not null).
    explicit SimulationTab(std::unique_ptr<OpenRocketDocument> document)
      : m_document(std::move(document)),
        m_panel(std::make_unique<SimulationPanel>(*m_document, m_services.services(), m_clipboard)),
        m_tester(&m_panel->tableModel(), QAbstractItemModelTester::FailureReportingMode::Warning)
    {
        m_panel->resize(1600, 500);
    }

    ~SimulationTab() = default;

    SimulationTab(const SimulationTab&)            = delete;
    SimulationTab& operator=(const SimulationTab&) = delete;
    SimulationTab(SimulationTab&&)                 = delete;
    SimulationTab& operator=(SimulationTab&&)      = delete;

    [[nodiscard]] Gui::Test::TestGuiServices& services() noexcept { return m_services; }
    [[nodiscard]] OpenRocketClipboard&        clipboard() noexcept { return m_clipboard; }
    [[nodiscard]] OpenRocketDocument&         document() const noexcept { return *m_document; }
    [[nodiscard]] SimulationPanel&            panel() const noexcept { return *m_panel; }
    [[nodiscard]] SimulationTableModel& model() const noexcept { return m_panel->tableModel(); }
    [[nodiscard]] SimulationTableView&  view() const noexcept { return m_panel->table(); }
    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() noexcept
    {
        return m_services.prompts();
    }
    [[nodiscard]] const std::vector<Request>& dialogRequests() const noexcept
    {
        return m_services.simulationDialogs().requests();
    }

    /// Shows the panel (offscreen) and lets it lay itself out.
    void show() const
    {
        m_panel->show();
        QCoreApplication::processEvents();
    }

    /// Selects the simulations of the table's rows @p rows (and nothing else).
    void selectRows(const std::vector<int>& rows) const
    {
        std::vector<std::shared_ptr<Simulation>> simulations;
        simulations.reserve(rows.size());
        for (const int row : rows)
        {
            simulations.push_back(model().simulation(row));
        }
        view().selectSimulations(simulations);
    }

    /// The names of the table's rows, from the top.
    [[nodiscard]] Names rowNames() const
    {
        Names names;
        for (const std::shared_ptr<Simulation>& simulation : model().simulations())
        {
            names.push_back(simulation->getName());
        }
        return names;
    }

    /// The names of the selected simulations, from the top.
    [[nodiscard]] Names selectedNames() const
    {
        Names names;
        for (const std::shared_ptr<Simulation>& simulation : view().selectedSimulations())
        {
            names.push_back(simulation->getName());
        }
        return names;
    }

    /// The centre of the cell of @p row in @p column, in the table's viewport.
    [[nodiscard]] QPoint cellCentre(int row, Column column) const
    {
        return view().visualRect(model().index(row, SimulationColumns::indexOf(column))).center();
    }

    /// A click of the left button on the cell of @p row in @p column.
    void clickCell(int row, Column column) const
    {
        QTest::mouseClick(view().viewport(), Qt::LeftButton, {}, cellCentre(row, column));
    }

    /// A double click on the cell of @p row in @p column.
    void doubleClickCell(int row, Column column) const
    {
        Gui::Test::doubleClickAt(*view().viewport(), cellCentre(row, column));
    }

private:
    Gui::Test::TestGuiServices          m_services;
    OpenRocketClipboard                 m_clipboard;
    std::unique_ptr<OpenRocketDocument> m_document;
    std::unique_ptr<SimulationPanel>    m_panel;
    QAbstractItemModelTester            m_tester;
};

/// The tab of "A simple model rocket" as loaded: five simulations with stored data ("Simulation
/// 1", "Simulation 2", "Simulation 3 - too short delay", "Simulation 4", "Simulation 5"), LOADED.
[[nodiscard]] std::unique_ptr<SimulationTab> simpleModelRocket()
{
    std::unique_ptr<OpenRocketDocument> document =
        Gui::Test::loadSimulationExample("A simple model rocket.ork");
    return document == nullptr ? nullptr : std::make_unique<SimulationTab>(std::move(document));
}

/// The tab of a new design: no simulation, no configuration with motors.
[[nodiscard]] std::unique_ptr<SimulationTab> newDesign()
{
    return std::make_unique<SimulationTab>(OpenRocketDocumentFactory::createNewRocket());
}

/// The items of @p menu: their texts with their keys ("&Edit [Ctrl+E]") and "-" for a
/// separator, a disabled item marked " (disabled)".
[[nodiscard]] Names menuItems(const QMenu& menu)
{
    Names items;
    for (const QAction* action : menu.actions())
    {
        if (action->isSeparator())
        {
            items.emplace_back("-");
            continue;
        }
        std::string item = QtStrings::toUtf8(action->text());
        if (!action->shortcut().isEmpty())
        {
            item += " [" + QtStrings::toUtf8(action->shortcut().toString()) + "]";
        }
        items.push_back(item + (action->isEnabled() ? "" : " (disabled)"));
    }
    return items;
}

/// The panel's actions that have no icon, a menu role or keys of another context than their
/// widget's (the popup menu's).
[[nodiscard]] Names actionsOtherThanJavas(const SimulationPanel& panel)
{
    Names names;
    for (const QAction* action : Gui::Test::simulationPanelPinActions(panel))
    {
        if (action->icon().isNull() || action->menuRole() != QAction::NoRole ||
            action->shortcutContext() != Qt::WidgetShortcut)
        {
            names.push_back(Gui::Test::simulationPanelActionName(*action));
        }
    }
    return names;
}

/// The widgets of the panel's row of buttons, from the left.
[[nodiscard]] std::vector<const QWidget*> buttonRow(const SimulationPanel& panel)
{
    std::vector<const QWidget*> widgets;
    const auto* row = panel.findChild<QWidget*>(QStringLiteral("simulationButtons"));
    if (row == nullptr || row->layout() == nullptr)
    {
        ADD_FAILURE() << "no row of buttons";
        return widgets;
    }
    for (int i = 0; i < row->layout()->count(); ++i)
    {
        if (const QWidget* widget = row->layout()->itemAt(i)->widget(); widget != nullptr)
        {
            widgets.push_back(widget);
        }
    }
    return widgets;
}

/// A button as the tests read it: "name: text | tooltip", " (disabled)" when it is.
[[nodiscard]] std::string describeButton(const QWidget& widget)
{
    const auto* button = qobject_cast<const IconButton*>(&widget);
    if (button == nullptr)
    {
        return QtStrings::toUtf8(widget.objectName()) + ": not an IconButton";
    }
    return QtStrings::toUtf8(button->objectName()) + ": " + QtStrings::toUtf8(button->text()) +
           " | " + QtStrings::toUtf8(button->toolTip()) +
           (button->isEnabled() ? "" : " (disabled)") +
           (button->icon().isNull() ? " (no icon)" : "");
}

/// The panel's buttons, from the left (describeButton()).
[[nodiscard]] Names describeButtons(const SimulationPanel& panel)
{
    Names                             buttons;
    const std::vector<const QWidget*> row = buttonRow(panel);
    buttons.reserve(row.size());
    for (const QWidget* widget : row)
    {
        buttons.push_back(describeButton(*widget));
    }
    return buttons;
}

/// @p request as the tests compare it: "EDIT LAUNCH_CONDITIONS new [names]".
[[nodiscard]] std::string describeRequest(const Request& request)
{
    std::string text = request.kind == RequestKind::EDIT ? "EDIT" : "PLOT";
    switch (request.tab)
    {
        case Tab::LAUNCH_CONDITIONS:
            text += " LAUNCH_CONDITIONS";
            break;
        case Tab::WARNINGS:
            text += " WARNINGS";
            break;
        case Tab::PLOT_DATA:
            text += " PLOT_DATA";
            break;
        case Tab::EXPORT_DATA:
            text += " EXPORT_DATA";
            break;
    }
    text += request.newSimulation ? " new [" : " [";
    text += QtStrings::toUtf8(request.simulationNames.join(QStringLiteral(", "))) + "]";
    return text;
}

/// The recorded dialogs of @p tab (describeRequest()).
[[nodiscard]] Names describeRequests(const SimulationTab& tab)
{
    Names requests;
    for (const Request& request : tab.dialogRequests())
    {
        requests.push_back(describeRequest(request));
    }
    return requests;
}

/// The names of the simulations of each run dialog @p tab's prompts were asked for.
[[nodiscard]] std::vector<Names> runRequests(SimulationTab& tab)
{
    std::vector<Names> runs;
    for (const auto& question : tab.prompts().runSimulationsQuestions())
    {
        Names names;
        for (const QString& name : question.simulationNames)
        {
            names.push_back(QtStrings::toUtf8(name));
        }
        runs.push_back(std::move(names));
    }
    return runs;
}

/// Makes every motor mount of @p rocket stop acting as one, as an undo step: no configuration
/// has motors then.
void stopActingAsMotorMounts(OpenRocketDocument& document)
{
    std::vector<MotorMount*> mounts;
    document.getRocket().forEach([&mounts](RocketComponent& component) {
        if (auto* mount = dynamic_cast<MotorMount*>(&component);
            mount != nullptr && mount->isMotorMount())
        {
            mounts.push_back(mount);
        }
    });
    document.addUndoPosition("Stop mounts");
    for (MotorMount* mount : mounts)
    {
        mount->setMotorMount(false);
    }
}

/// The help's text as it reads (the rich text's plain text).
[[nodiscard]] std::string plainText(const QString& richText)
{
    QTextDocument text;
    text.setHtml(richText);
    return QtStrings::toUtf8(text.toPlainText());
}

/// A new simulation of @p tab's document appended to it (not simulated), as SimulationEditing's
/// New makes it, without the panel's dialog.
std::shared_ptr<Simulation> appendNewSimulation(SimulationTab& tab)
{
    OpenRocketDocument& document = tab.document();
    auto                simulation =
        std::make_shared<Simulation>(&document, document.getRocket(), tab.services().preferences());
    simulation->setName(document.getNextSimulationName());
    document.addSimulation(simulation);
    return simulation;
}

/// Makes the table's row @p row's simulation OUTDATED (its launch rod 0.5 m longer).
void outdate(const SimulationTab& tab, int row)
{
    SimulationOptions& options = tab.model().simulation(row)->getOptions();
    options.setLaunchRodLength(options.getLaunchRodLength() + 0.5);
}

// =========================================================================== the widgets

TEST_F(SimulationPanelTest, HasJavasFiveButtonsWithTheirTextsAndTooltips)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    const SimulationPanel& panel = tab->panel();
    EXPECT_EQ(panel.objectName(), QStringLiteral("simulationPanel"));
    // The row, from the left: no Monte Carlo button (Milestone 7).
    EXPECT_EQ(buttonRow(panel),
              (std::vector<const QWidget*>{panel.newButton(), panel.editButton(), panel.runButton(),
                                           panel.deleteButton(), panel.plotButton()}));
    // No row selected: only New is enabled.
    EXPECT_EQ(describeButton(*panel.newButton()),
              "newButton: New simulation | Add a new simulation");
    EXPECT_EQ(describeButton(*panel.editButton()),
              "editButton: Edit simulation | Edit the selected simulation (disabled)");
    EXPECT_EQ(describeButton(*panel.runButton()),
              "runButton: Run simulations | Re-run the selected simulations (disabled)");
    EXPECT_EQ(describeButton(*panel.deleteButton()),
              "deleteButton: Delete simulations | Delete the selected simulations (disabled)");
    EXPECT_EQ(describeButton(*panel.plotButton()),
              "plotButton: Plot / Export | Plot or Export the selected simulation(s) (disabled)");
}

TEST_F(SimulationPanelTest, TheButtonsFollowTheirActionsAndTriggerThem)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->selectRows({0});
    EXPECT_EQ(describeButtons(tab->panel()),
              (Names{"newButton: New simulation | Add a new simulation",
                     "editButton: Edit simulation | Edit the selected simulation",
                     "runButton: Run simulations | Re-run the selected simulations",
                     "deleteButton: Delete simulations | Delete the selected simulations",
                     "plotButton: Plot / Export | Plot or Export the selected simulation(s)"}));
    tab->selectRows({0, 1});
    EXPECT_FALSE(tab->panel().plotButton()->isEnabled());
    EXPECT_FALSE(tab->panel().plotSimulationAction()->isEnabled());

    const QSignalSpy edit(tab->panel().editSimulationAction(), &QAction::triggered);
    QTest::mouseClick(tab->panel().editButton(), Qt::LeftButton);
    EXPECT_EQ(edit.count(), 1);
    EXPECT_EQ(describeRequests(*tab),
              (Names{"EDIT LAUNCH_CONDITIONS [Simulation 1, Simulation 2]"}));
}

TEST_F(SimulationPanelTest, ItsActionsHaveJavasTextsTooltipsIconsAndKeys)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    const SimulationPanel& panel = tab->panel();
    EXPECT_EQ(panel.newSimulationAction()->text(), QStringLiteral("&New simulation"));
    EXPECT_EQ(panel.newSimulationAction()->toolTip(), QStringLiteral("New simulation"));
    EXPECT_EQ(panel.editSimulationAction()->toolTip(),
              QStringLiteral("Edit the selected simulation(s)"));
    EXPECT_EQ(panel.cutSimulationAction()->toolTip(),
              QStringLiteral(
                  "Copy the selected simulation(s) to the clipboard and delete from this design"));
    EXPECT_EQ(panel.copySimulationAction()->toolTip(),
              QStringLiteral("Copy the selected simulation(s) to the clipboard"));
    EXPECT_EQ(panel.pasteSimulationAction()->toolTip(),
              QStringLiteral("Paste the simulation(s) on the clipboard to the design."));
    EXPECT_EQ(panel.duplicateSimulationAction()->toolTip(),
              QStringLiteral("Duplicate the selected simulation(s)"));
    EXPECT_EQ(panel.deleteSimulationAction()->toolTip(),
              QStringLiteral("Delete the selected simulation(s)"));
    EXPECT_EQ(panel.runSimulationAction()->toolTip(),
              QStringLiteral("Run the selected simulation(s)"));
    EXPECT_EQ(panel.plotSimulationAction()->toolTip(),
              QStringLiteral("Plot or Export the selected simulation(s)"));
    EXPECT_EQ(panel.exportSimulationTableAction()->text(),
              QStringLiteral("Export simulation table as CSV file"));
    // Every one with an icon, no menu role, and its keys shown, never answered (the window's
    // Edit menu has them).
    EXPECT_EQ(actionsOtherThanJavas(panel), Names{});
    EXPECT_EQ(panel.cutSimulationAction()->shortcut(), QKeySequence(QKeySequence::Cut));
    EXPECT_EQ(panel.copySimulationAction()->shortcut(), QKeySequence(QKeySequence::Copy));
    EXPECT_EQ(panel.pasteSimulationAction()->shortcut(), QKeySequence(QKeySequence::Paste));
}

TEST_F(SimulationPanelTest, ThePopupMenuHasJavasItemsInJavasOrder)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    const QMenu& menu = tab->panel().popupMenu();
    EXPECT_EQ(menu.objectName(), QStringLiteral("pm"));
    EXPECT_TRUE(menu.toolTipsVisible());
    tab->selectRows({0});
    // No Monte Carlo item (Milestone 7); Paste needs simulations on the clipboard.
    EXPECT_EQ(menuItems(menu),
              (Names{"&Edit [Ctrl+E]", "Cut [Ctrl+X]", "&Copy [Ctrl+C]",
                     "Paste [Ctrl+V] (disabled)", "&Duplicate [Ctrl+D]", "Delete [Del]", "-", "Run",
                     "Plot / Export", "Export simulation(s) as CSV file"}));
    tab->selectRows({0, 1});
    EXPECT_EQ(menuItems(menu),
              (Names{"&Edit [Ctrl+E]", "Cut [Ctrl+X]", "&Copy [Ctrl+C]",
                     "Paste [Ctrl+V] (disabled)", "&Duplicate [Ctrl+D]", "Delete [Del]", "-", "Run",
                     "Plot / Export (disabled)", "Export simulation(s) as CSV file"}));
}

TEST_F(SimulationPanelTest, ARightClickOnARowShowsThePopupMenuForThatRow)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->show();
    QMenu& menu = tab->panel().popupMenu();
    {
        const Gui::Test::OffscreenPopupWarnings warnings;
        Gui::Test::rightClickAt(*tab->view().viewport(), tab->cellCentre(2, Column::APOGEE));
        EXPECT_TRUE(menu.isVisible());
        EXPECT_EQ(tab->selectedNames(), Names{"Simulation 3 - too short delay"});
        EXPECT_TRUE(tab->panel().plotSimulationAction()->isEnabled());
        menu.close();
    }
    EXPECT_FALSE(menu.isVisible());
}

// ======================================================================== the help card

TEST_F(SimulationPanelTest, ShowsTheHelpWhileNoConfigurationHasMotors)
{
    const std::unique_ptr<SimulationTab> tab   = newDesign();
    const SimulationPanel&               panel = tab->panel();
    EXPECT_FALSE(panel.hasValidConfig());
    EXPECT_TRUE(panel.isHelpShown());
    EXPECT_EQ(panel.cards()->objectName(), QStringLiteral("cardPanel"));
    EXPECT_EQ(panel.helpLabel()->objectName(), QStringLiteral("helpLabel"));
    EXPECT_EQ(panel.helpLabel()->textFormat(), Qt::RichText);
    EXPECT_EQ(panel.helpLabel()->alignment(), Qt::AlignCenter);
    EXPECT_EQ(plainText(panel.helpLabel()->text()),
              "No configuration with motors defined.\nPlease create a valid configuration in "
              "the 'Motors & Configuration' tab.");
    // OpenRocket's states for a new design (the pins' "new_design"): New disabled.
    EXPECT_EQ(Gui::Test::simulationPanelActionStates(panel),
              "newSimulation=0 editSimulation=0 cutSimulation=0 copySimulation=0 "
              "pasteSimulation=0 runSimulation=0 plotSimulation=0 duplicateSimulation=0 "
              "deleteSimulation=0 simTableExport=0 selectedSimsExport=0");
    EXPECT_FALSE(panel.newButton()->isEnabled());
}

TEST_F(SimulationPanelTest, SwitchesBetweenTheHelpAndTheTableWithTheMotors)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    const SimulationPanel& panel = tab->panel();
    EXPECT_TRUE(panel.hasValidConfig());
    EXPECT_FALSE(panel.isHelpShown());
    EXPECT_EQ(panel.cards()->currentWidget(), &panel.table());
    tab->selectRows({0});
    EXPECT_TRUE(panel.editSimulationAction()->isEnabled());

    // A MOTOR change leaves no configuration with motors.
    stopActingAsMotorMounts(tab->document());
    EXPECT_FALSE(panel.hasValidConfig());
    EXPECT_TRUE(panel.isHelpShown());
    // The rules that need a configuration with motors say no; the others still see the row.
    EXPECT_EQ(Gui::Test::simulationPanelActionStates(panel),
              "newSimulation=0 editSimulation=0 cutSimulation=1 copySimulation=1 "
              "pasteSimulation=0 runSimulation=0 plotSimulation=0 duplicateSimulation=1 "
              "deleteSimulation=0 simTableExport=1 selectedSimsExport=1");

    // The undo (a TREE change) brings them back.
    tab->document().undo();
    EXPECT_TRUE(panel.hasValidConfig());
    EXPECT_FALSE(panel.isHelpShown());
    EXPECT_TRUE(panel.newSimulationAction()->isEnabled());
}

// =========================================================================== the rules

/// The pinned designs' tabs: OpenRocket's action states for no row, the first row and the first
/// two rows.
class SimulationPanelGolden : public Gui::Test::GuiTest,
                              public ::testing::WithParamInterface<std::string>
{
protected:
    [[nodiscard]] static const Gui::Test::SimulationTablePinDesign* design()
    {
        const Gui::Test::SimulationTablePins* pins = Gui::Test::simulationTablePinsOrFail();
        return pins == nullptr ? nullptr : pins->findDesign(GetParam());
    }

    [[nodiscard]] static std::unique_ptr<SimulationTab> tabOf(
        const Gui::Test::SimulationTablePinDesign& pin)
    {
        if (pin.kind == "new")
        {
            return newDesign();
        }
        std::unique_ptr<OpenRocketDocument> document = Gui::Test::loadSimulationExample(pin.file);
        return document == nullptr ? nullptr : std::make_unique<SimulationTab>(std::move(document));
    }

    /// The selection @p selection of the pins ("none", "row0", "rows01") made in @p tab.
    static void select(const SimulationTab& tab, const std::string& selection)
    {
        if (selection == "row0")
        {
            tab.selectRows({0});
        }
        else if (selection == "rows01")
        {
            tab.selectRows({0, 1});
        }
        else
        {
            tab.selectRows({});
        }
    }
};

TEST_P(SimulationPanelGolden, EnablesItsActionsAsOpenRocket)
{
    const Gui::Test::SimulationTablePinDesign* pin = design();
    ASSERT_NE(pin, nullptr);
    const std::unique_ptr<SimulationTab> tab = tabOf(*pin);
    ASSERT_NE(tab, nullptr);
    EXPECT_EQ(tab->panel().hasValidConfig(), pin->hasValidConfig);
    EXPECT_EQ(tab->panel().isHelpShown(), !pin->hasValidConfig);
    EXPECT_FALSE(pin->actions.empty());
    for (const auto& [selection, states] : pin->actions)
    {
        select(*tab, selection);
        EXPECT_EQ(Gui::Test::simulationPanelActionStates(tab->panel()),
                  Gui::Test::pinnedSimulationPanelActionStates(tab->panel(), states))
            << selection;
    }
}

INSTANTIATE_TEST_SUITE_P(Designs, SimulationPanelGolden,
                         ::testing::ValuesIn(Gui::Test::simulationTablePinnedExampleDesigns()),
                         Gui::Test::simulationTablePinTestName);

TEST_F(SimulationPanelTest, PastesWithoutASelectedRowWhileTheTableIsShown)
{
    // OpenRocket bug 27: OpenRocket's Paste needs a selected row.
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    const SimulationPanel& panel = tab->panel();
    EXPECT_FALSE(panel.pasteSimulationAction()->isEnabled());
    tab->selectRows({1});
    panel.copySimulationAction()->trigger();
    QGuiApplication::clipboard()->clear();
    EXPECT_TRUE(panel.pasteSimulationAction()->isEnabled());
    tab->selectRows({});
    EXPECT_TRUE(panel.pasteSimulationAction()->isEnabled());

    panel.pasteSimulationAction()->trigger();
    EXPECT_EQ(tab->rowNames(),
              (Names{"Simulation 1", "Simulation 2", "Simulation 3 - too short delay",
                     "Simulation 4", "Simulation 5", "Simulation 6"}));
    EXPECT_EQ(tab->selectedNames(), Names{"Simulation 6"});

    // The clipboard's changes reach the rule: components empty its simulations.
    tab->clipboard().setComponents(ComponentCopy{});
    EXPECT_FALSE(panel.pasteSimulationAction()->isEnabled());
}

TEST_F(SimulationPanelTest, PastesNothingIntoADesignWhoseTableIsHidden)
{
    const std::unique_ptr<OpenRocketDocument> source =
        Gui::Test::loadSimulationExample("A simple model rocket.ork");
    ASSERT_NE(source, nullptr);
    // The new design's tab shows the help: nothing pasted there would be seen.
    const std::unique_ptr<SimulationTab> target = newDesign();
    target->clipboard().setSimulations(SimulationCopy::copyOf({source->getSimulation(0).get()}));
    EXPECT_TRUE(target->panel().isHelpShown());
    EXPECT_FALSE(target->panel().pasteSimulationAction()->isEnabled());
}

TEST_F(SimulationPanelTest, TheExportActionsAskForTheirExports)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    const SimulationPanel& panel = tab->panel();
    const QSignalSpy       exports(&panel, &SimulationPanel::csvExportRequested);
    EXPECT_TRUE(panel.exportSimulationTableAction()->isEnabled());
    EXPECT_FALSE(panel.exportSelectedSimulationsAction()->isEnabled());
    // The panel answers each request with its export, whose file question is cancelled here.
    tab->prompts().answerSaveFile(std::nullopt);
    panel.exportSimulationTableAction()->trigger();
    tab->selectRows({3});
    tab->prompts().answerSaveFile(std::nullopt);
    panel.exportSelectedSimulationsAction()->trigger();
    ASSERT_EQ(exports.count(), 2);
    EXPECT_FALSE(exports.at(0).at(0).toBool());
    EXPECT_TRUE(exports.at(1).at(0).toBool());
    EXPECT_EQ(tab->prompts().saveFileRequests().size(), 2U);
}

TEST_F(SimulationPanelTest, TheExportOfTheTableNeedsRows)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->selectRows({0, 1, 2, 3, 4});
    tab->prompts().answerDeleteSimulations({.confirmed = true, .doNotAskAgain = false});
    tab->panel().deleteSimulations();
    EXPECT_EQ(tab->document().getSimulationCount(), 0U);
    EXPECT_FALSE(tab->panel().exportSimulationTableAction()->isEnabled());
    EXPECT_FALSE(tab->panel().exportSelectedSimulationsAction()->isEnabled());
    // A new simulation is a row.
    tab->panel().newSimulation();
    EXPECT_TRUE(tab->panel().exportSimulationTableAction()->isEnabled());
}

// ============================================================================== the keys

/// The tab of "A simple model rocket" as the design tab of a shown, active window, its first row
/// selected, the component items of the window's Edit menu enabled.
class SimulationPanelKeysTest : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        m_document = Gui::Test::loadSimulationExample("A simple model rocket.ork");
        ASSERT_NE(m_document, nullptr);
        m_window = std::make_unique<MainWindow>(*m_document, m_services.services());
        m_panel  = makeChild<SimulationPanel>(m_window.get(), *m_document, m_services.services(),
                                              m_clipboard);
        m_window->setDesignPanel(m_panel);
        for (QAction* action : windowActions())
        {
            action->setEnabled(true);
        }
        m_window->show();
        m_window->activateWindow();
        ASSERT_TRUE(QTest::qWaitForWindowActive(m_window.get()));
        m_panel->table().selectSimulations({m_panel->tableModel().simulation(0)});
        m_panel->table().setFocus();
        ASSERT_EQ(QApplication::focusWidget(), &m_panel->table());
    }

    void TearDown() override
    {
        m_window.reset();
        m_document.reset();
        GuiTest::TearDown();
    }

    /// The window's Edit, Cut, Copy, Paste, Duplicate and Delete.
    [[nodiscard]] std::array<QAction*, 6> windowActions() const
    {
        const MainMenus& menus = m_window->menus();
        return {menus.editAction(),  menus.cutAction(),       menus.copyAction(),
                menus.pasteAction(), menus.duplicateAction(), menus.deleteAction()};
    }

    /// The panel's Edit, Cut, Copy, Paste, Duplicate and Delete.
    [[nodiscard]] std::array<QAction*, 6> panelEditActions() const
    {
        return {m_panel->editSimulationAction(),      m_panel->cutSimulationAction(),
                m_panel->copySimulationAction(),      m_panel->pasteSimulationAction(),
                m_panel->duplicateSimulationAction(), m_panel->deleteSimulationAction()};
    }

    SimulationPanel* m_panel{nullptr};

private:
    Gui::Test::TestGuiServices          m_services;
    OpenRocketClipboard                 m_clipboard;
    std::unique_ptr<OpenRocketDocument> m_document;
    std::unique_ptr<MainWindow>         m_window;
};

/// The number of times each of @p actions was triggered while @p keys were pressed in @p widget.
[[nodiscard]] std::vector<int> triggeredBy(const std::array<QAction*, 6>& windowActions,
                                           const std::array<QAction*, 6>& panelActions,
                                           QWidget&                       widget)
{
    std::vector<std::unique_ptr<QSignalSpy>> spies;
    spies.reserve(windowActions.size() + panelActions.size());
    for (QAction* action : windowActions)
    {
        spies.push_back(std::make_unique<QSignalSpy>(action, &QAction::triggered));
    }
    for (QAction* action : panelActions)
    {
        spies.push_back(std::make_unique<QSignalSpy>(action, &QAction::triggered));
    }
    QTest::keyClick(&widget, Qt::Key_E, Qt::ControlModifier);
    QTest::keySequence(&widget, QKeySequence(QKeySequence::Cut));
    QTest::keySequence(&widget, QKeySequence(QKeySequence::Copy));
    QTest::keySequence(&widget, QKeySequence(QKeySequence::Paste));
    QTest::keyClick(&widget, Qt::Key_D, Qt::ControlModifier);
    QTest::keyClick(&widget, Qt::Key_Delete);
    std::vector<int> counts;
    counts.reserve(spies.size());
    for (const std::unique_ptr<QSignalSpy>& spy : spies)
    {
        counts.push_back(static_cast<int>(spy->count()));
    }
    return counts;
}

TEST_F(SimulationPanelKeysTest, TheEditKeysInTheTableAreTheWindowsActions)
{
    // SimulationPanel binds them to "none" in the table, so that RocketActions' keys act; the
    // panel's actions only show them in the popup menu.
    EXPECT_TRUE(m_panel->copySimulationAction()->isEnabled());
    EXPECT_EQ(triggeredBy(windowActions(), panelEditActions(), m_panel->table()),
              (std::vector<int>{1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0}));
}

// ====================================================================== the operations

TEST_F(SimulationPanelTest, NewAddsASimulationAndOpensItsDialog)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->selectRows({2});
    QTest::mouseClick(tab->panel().newButton(), Qt::LeftButton);
    EXPECT_EQ(tab->rowNames().back(), "Simulation 6");
    EXPECT_EQ(tab->selectedNames(), Names{"Simulation 6"});
    EXPECT_EQ(tab->document().getUndoDescription().value_or("null"), "Add Simulation 6");
    // The settings dialog of the new simulation (Cancel would undo the addition).
    EXPECT_EQ(describeRequests(*tab), (Names{"EDIT LAUNCH_CONDITIONS new [Simulation 6]"}));
    ASSERT_EQ(tab->dialogRequests().size(), 1U);
    EXPECT_EQ(tab->dialogRequests().front().parent, &tab->panel());
    EXPECT_EQ(tab->dialogRequests().front().document, &tab->document());
    // The new simulation was remembered: a later spotlight without a selection selects it.
    tab->view().selectSimulations({});
    tab->panel().takeTheSpotlight();
    QCoreApplication::processEvents();
    EXPECT_EQ(tab->selectedNames(), Names{"Simulation 6"});
}

TEST_F(SimulationPanelTest, TheTableShowsTheDialogsEditsWithItsSelection)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    // While the dialog is open, the user renames the simulation.
    tab->services().simulationDialogs().whileEditing(
        [](QWidget* /*parent*/, OpenRocketDocument& /*document*/,
           const SimulationDialogs::EditRequest& request) {
            request.simulations.front()->setName("Renamed");
        });
    tab->selectRows({1});
    tab->panel().editSimulationAction()->trigger();
    EXPECT_EQ(tab->selectedNames(), Names{"Renamed"});
    EXPECT_EQ(tab->model().cellText(1, Column::NAME), QStringLiteral("Renamed"));
}

TEST_F(SimulationPanelTest, DuplicateDuplicatesTheSelection)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->selectRows({0, 2});
    tab->panel().duplicateSimulationAction()->trigger();
    EXPECT_EQ(
        tab->rowNames(),
        (Names{"Simulation 1", "Simulation 2", "Simulation 3 - too short delay", "Simulation 4",
               "Simulation 5", "Simulation 6", "Simulation 3 - too short delay"}));
    EXPECT_EQ(tab->selectedNames(), (Names{"Simulation 6", "Simulation 3 - too short delay"}));
    EXPECT_EQ(tab->document().getUndoDescription().value_or("null"), "Duplicate simulations");
    EXPECT_TRUE(tab->dialogRequests().empty());
}

TEST_F(SimulationPanelTest, DeleteRemovesTheSelectionAndThenSelectsTheFirstRow)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->selectRows({1, 3});
    tab->prompts().answerDeleteSimulations({.confirmed = true, .doNotAskAgain = false});
    QTest::mouseClick(tab->panel().deleteButton(), Qt::LeftButton);
    EXPECT_EQ(tab->rowNames(),
              (Names{"Simulation 1", "Simulation 3 - too short delay", "Simulation 5"}));
    EXPECT_EQ(tab->document().getUndoDescription().value_or("null"), "Delete simulations");
    // Java's deleteSimulations(): nothing selected, nothing remembered, the spotlight taken in
    // the next turn: the first row.
    EXPECT_TRUE(tab->selectedNames().empty());
    QCoreApplication::processEvents();
    EXPECT_EQ(tab->selectedNames(), Names{"Simulation 1"});
    EXPECT_TRUE(tab->panel().editSimulationAction()->isEnabled());
}

TEST_F(SimulationPanelTest, ACancelledDeleteKeepsEverything)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->selectRows({1});
    tab->prompts().answerDeleteSimulations({.confirmed = false, .doNotAskAgain = false});
    tab->panel().deleteSimulationAction()->trigger();
    QCoreApplication::processEvents();
    EXPECT_EQ(tab->document().getSimulationCount(), 5U);
    EXPECT_EQ(tab->selectedNames(), Names{"Simulation 2"});
}

TEST_F(SimulationPanelTest, CutCopiesThenDeletesAndSelectsTheFirstRow)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->selectRows({0});
    tab->prompts().answerDeleteSimulations({.confirmed = true, .doNotAskAgain = false});
    tab->panel().cutSimulationAction()->trigger();
    EXPECT_EQ(tab->document().getSimulationCount(), 4U);
    EXPECT_EQ(tab->clipboard().simulations()->size(), 1U);
    EXPECT_FALSE(QGuiApplication::clipboard()->text().isEmpty());
    QGuiApplication::clipboard()->clear();
    QCoreApplication::processEvents();
    EXPECT_EQ(tab->selectedNames(), Names{"Simulation 2"});
}

// ============================================================== the dialogs and the runs

TEST_F(SimulationPanelTest, EditOpensTheDialogOfTheSelectedSimulations)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->selectRows({1, 3});
    tab->panel().editSimulationAction()->trigger();
    EXPECT_EQ(describeRequests(*tab),
              (Names{"EDIT LAUNCH_CONDITIONS [Simulation 2, Simulation 4]"}));
    EXPECT_EQ(tab->selectedNames(), (Names{"Simulation 2", "Simulation 4"}));
}

TEST_F(SimulationPanelTest, RunRunsTheSelectedSimulationsThroughTheRunDialog)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->selectRows({4, 0});
    tab->prompts().answerRunSimulations({.cancelled = false, .allSuccessful = true});
    QTest::mouseClick(tab->panel().runButton(), Qt::LeftButton);
    EXPECT_EQ(runRequests(*tab), (std::vector<Names>{{"Simulation 1", "Simulation 5"}}));
    const auto& question = tab->prompts().runSimulationsQuestions().front();
    EXPECT_EQ(question.window, tab->panel().window());
    EXPECT_EQ(question.document, &tab->document());
    EXPECT_EQ(question.executor, &tab->services().executor());
    EXPECT_EQ(tab->selectedNames(), (Names{"Simulation 1", "Simulation 5"}));
}

TEST_F(SimulationPanelTest, NothingRunsWithoutAnExecutor)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    GuiServices services = tab->services().services();
    services.setSimulationExecutor(nullptr);
    services.setSimulationDialogs(nullptr);
    const SimulationPanel panel(tab->document(), services, tab->clipboard());
    panel.table().selectSimulations({panel.tableModel().simulation(0)});
    panel.runSimulationAction()->trigger();
    panel.editSimulationAction()->trigger();
    panel.plotSimulationAction()->trigger();
    EXPECT_TRUE(tab->prompts().runSimulationsQuestions().empty());
    EXPECT_TRUE(tab->dialogRequests().empty());
}

TEST_F(SimulationPanelTest, PlotOpensThePlotTabOfASimulationWithData)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->selectRows({2});
    QTest::mouseClick(tab->panel().plotButton(), Qt::LeftButton);
    // Its data is there: no run.
    EXPECT_TRUE(tab->prompts().runSimulationsQuestions().empty());
    EXPECT_EQ(describeRequests(*tab), (Names{"EDIT PLOT_DATA [Simulation 3 - too short delay]"}));
}

TEST_F(SimulationPanelTest, PlotRunsASimulationWithoutDataFirst)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    const std::shared_ptr<Simulation> fresh = appendNewSimulation(*tab);
    tab->view().selectSimulations({fresh});
    // The run is cancelled: the simulation still has no data, so its dialog shows its Launch
    // conditions (Java switches to a plot tab that dialog does not have).
    tab->prompts().answerRunSimulations({.cancelled = true, .allSuccessful = false});
    tab->panel().plotSimulationAction()->trigger();
    EXPECT_EQ(runRequests(*tab), (std::vector<Names>{{"Simulation 6"}}));
    EXPECT_EQ(describeRequests(*tab), (Names{"EDIT LAUNCH_CONDITIONS [Simulation 6]"}));
}

TEST_F(SimulationPanelTest, PlotSelectsThePlottedSimulationInASortedTable)
{
    // OpenRocket bug 24 (Plot / Export): Java selects the simulation's model index as a view
    // row.
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->model().setSortKeys({SortKey{.column = SimulationColumns::indexOf(Column::NAME),
                                      .order  = SortOrder::DESCENDING}});
    ASSERT_EQ(tab->rowNames(),
              (Names{"Simulation 5", "Simulation 4", "Simulation 3 - too short delay",
                     "Simulation 2", "Simulation 1"}));
    // Rows 0 and 3 selected: the first is plotted, and selected alone (Java would select view
    // row 4, Simulation 1, its model index).
    tab->selectRows({0, 3});
    tab->panel().plotSimulation();
    EXPECT_EQ(tab->selectedNames(), Names{"Simulation 5"});
    EXPECT_EQ(describeRequests(*tab), (Names{"EDIT PLOT_DATA [Simulation 5]"}));
}

// ====================================================================== the table's mouse

TEST_F(SimulationPanelTest, AClickOnTheStatusOfASimulationToRunRunsTheSelection)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    outdate(*tab, 3);
    ASSERT_EQ(tab->model().status(3), Status::OUTDATED);
    tab->show();
    // A LOADED simulation: nothing runs.
    tab->clickCell(1, Column::STATUS);
    EXPECT_TRUE(tab->prompts().runSimulationsQuestions().empty());
    // An OUTDATED one: the selection (that row alone) runs.
    tab->prompts().answerRunSimulations({.cancelled = false, .allSuccessful = true});
    tab->clickCell(3, Column::STATUS);
    EXPECT_EQ(runRequests(*tab), (std::vector<Names>{{"Simulation 4"}}));
    // A click on another column of it runs nothing.
    tab->clickCell(3, Column::APOGEE);
    EXPECT_EQ(runRequests(*tab).size(), 1U);
    EXPECT_TRUE(tab->dialogRequests().empty());
}

TEST_F(SimulationPanelTest, ADoubleClickOpensTheWarningsOrTheDialogOfTheSimulation)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    outdate(*tab, 4);
    tab->show();
    tab->doubleClickCell(1, Column::WARNINGS);
    // The status column: nothing.
    tab->doubleClickCell(2, Column::STATUS);
    // Data up to date: the plot tab.
    tab->doubleClickCell(0, Column::NAME);
    // OUTDATED: the settings.
    tab->doubleClickCell(4, Column::APOGEE);
    EXPECT_EQ(describeRequests(*tab),
              (Names{"EDIT WARNINGS [Simulation 2]", "EDIT PLOT_DATA [Simulation 1]",
                     "EDIT LAUNCH_CONDITIONS [Simulation 5]"}));
    EXPECT_TRUE(tab->prompts().runSimulationsQuestions().empty());
}

// ===================================================================== the window's part

TEST_F(SimulationPanelTest, ActivatingRunsTheSimulationsToRunWhileThePreferencesAskForIt)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    appendNewSimulation(*tab);
    outdate(*tab, 2);
    tab->model().setSortKeys({SortKey{.column = SimulationColumns::indexOf(Column::NAME),
                                      .order  = SortOrder::DESCENDING}});
    tab->services().preferences().setAutoRunSimulations(false);
    tab->panel().activating();
    EXPECT_TRUE(tab->prompts().runSimulationsQuestions().empty());

    // In the table's order.
    tab->services().preferences().setAutoRunSimulations(true);
    tab->prompts().answerRunSimulations({.cancelled = false, .allSuccessful = true});
    tab->panel().activating();
    EXPECT_EQ(runRequests(*tab),
              (std::vector<Names>{{"Simulation 6", "Simulation 3 - too short delay"}}));
}

TEST_F(SimulationPanelTest, ActivatingRunsNothingWhenEverySimulationIsUpToDate)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->services().preferences().setAutoRunSimulations(true);
    tab->panel().activating();
    EXPECT_TRUE(tab->prompts().runSimulationsQuestions().empty());
}

TEST_F(SimulationPanelTest, TheSpotlightSelectsTheRememberedSimulations)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    // Nothing remembered: the first row.
    tab->panel().takeTheSpotlight();
    EXPECT_TRUE(tab->selectedNames().empty());
    QCoreApplication::processEvents();
    EXPECT_EQ(tab->selectedNames(), Names{"Simulation 1"});

    // Remembered as the tab is left, then the selection went (the component tree took it).
    tab->selectRows({1, 3});
    tab->panel().updatePreviousSelection();
    tab->view().selectSimulations({});
    // A new order (OpenRocket bug 28: Java would select its rows 1 and 3).
    tab->model().setSortKeys({SortKey{.column = SimulationColumns::indexOf(Column::NAME),
                                      .order  = SortOrder::DESCENDING}});
    tab->panel().takeTheSpotlight();
    QCoreApplication::processEvents();
    EXPECT_EQ(tab->selectedNames(), (Names{"Simulation 4", "Simulation 2"}));

    // A selection is kept as it is.
    tab->selectRows({0});
    tab->panel().takeTheSpotlight();
    QCoreApplication::processEvents();
    EXPECT_EQ(tab->selectedNames(), Names{"Simulation 5"});
}

TEST_F(SimulationPanelTest, TheSpotlightSelectsTheRememberedRowsOfReplacedSimulations)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    // An undo that changes the number of simulations replaces them all with copies.
    tab->document().addUndoPosition("Add");
    appendNewSimulation(*tab);
    tab->selectRows({2, 5});
    tab->panel().updatePreviousSelection();
    tab->view().selectSimulations({});
    tab->document().undo();
    ASSERT_EQ(tab->document().getSimulationCount(), 5U);
    tab->panel().takeTheSpotlight();
    QCoreApplication::processEvents();
    // Java's rows, those the table still has.
    EXPECT_EQ(tab->selectedNames(), Names{"Simulation 3 - too short delay"});
}

TEST_F(SimulationPanelTest, TheSpotlightGivesTheTableTheFocus)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->show();
    tab->panel().newButton()->setFocus();
    tab->panel().takeTheSpotlight();
    QCoreApplication::processEvents();
    EXPECT_EQ(tab->panel().focusWidget(), &tab->view());
}

// =================================================================== the window's estimate

/// What a tab's window does with its background estimate (SimulationPanel::BackgroundRuns), noted
/// in order with what the test notes: "pause", "resume", "cancel [names]".
class EstimateNotes
{
public:
    /// Hooks that note into this object, which must outlive the panel they are given to.
    [[nodiscard]] SimulationPanel::BackgroundRuns hooks()
    {
        return {.cancel =
                    [this](std::span<const std::shared_ptr<Simulation>> simulations) {
                        note("cancel " + namesOf(simulations));
                    },
                .pause = [this](bool paused) { note(paused ? "pause" : "resume"); }};
    }

    void note(std::string text) { m_notes.push_back(std::move(text)); }

    [[nodiscard]] const Names& notes() const noexcept { return m_notes; }

    /// "[A, B]" of @p simulations.
    [[nodiscard]] static std::string namesOf(
        std::span<const std::shared_ptr<Simulation>> simulations)
    {
        std::string text = "[";
        for (const std::shared_ptr<Simulation>& simulation : simulations)
        {
            text += (text.size() > 1 ? ", " : "") + simulation->getName();
        }
        return text + "]";
    }

private:
    Names m_notes;
};

/// The run dialog of the scripted questions, as far as the window's estimate sees it: notes "run
/// [names]", takes the simulations out of the estimate's hands as the batch does
/// (RunSimulationsRequest::cancelBackgroundRuns), and answers that every run succeeded (none
/// runs).
struct RunAsTheBatchDoes
{
    EstimateNotes* notes;

    UserPrompts::RunSimulationsAnswer operator()(
        QWidget* /*parent*/, const UserPrompts::RunSimulationsRequest& request) const
    {
        notes->note("run " + EstimateNotes::namesOf(request.simulations));
        if (request.cancelBackgroundRuns)
        {
            request.cancelBackgroundRuns(request.simulations);
        }
        return {.cancelled = false, .allSuccessful = true};
    }
};

/// The settings dialog as far as the window's estimate sees it (RecordingSimulationDialogs'
/// edit): notes "edit [names]", or "edit [names] without the estimate" when its request lacks a
/// hook, then pauses the estimate as it opens, takes its simulations out of the estimate's hands
/// as its OK's run does, and resumes the estimate as it closes.
struct EditAsTheDialogDoes
{
    EstimateNotes* notes;

    void operator()(QWidget* /*parent*/, OpenRocketDocument& /*document*/,
                    const SimulationDialogs::EditRequest& request) const
    {
        const std::string names = EstimateNotes::namesOf(request.simulations);
        if (!request.pauseBackgroundRuns || !request.cancelBackgroundRuns)
        {
            notes->note("edit " + names + " without the estimate");
            return;
        }
        notes->note("edit " + names);
        request.pauseBackgroundRuns(true);
        request.cancelBackgroundRuns(request.simulations);
        request.pauseBackgroundRuns(false);
    }
};

TEST_F(SimulationPanelTest, EveryRunPausesTheWindowsEstimateAndTakesItsSimulations)
{
    // O-M4-16: the estimate is paused while the run dialog is open, and the batch takes the
    // simulations it runs out of its hands: the Run action, the status click, the auto-run and
    // Plot / Export of a simulation without data.
    EstimateNotes                        notes;
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->panel().setBackgroundRuns(notes.hooks());
    outdate(*tab, 3);
    tab->show();
    tab->selectRows({4, 0});
    tab->prompts().answerRunSimulationsWith(RunAsTheBatchDoes{&notes});
    tab->panel().runSimulationAction()->trigger();
    tab->prompts().answerRunSimulationsWith(RunAsTheBatchDoes{&notes});
    tab->clickCell(3, Column::STATUS);
    tab->services().preferences().setAutoRunSimulations(true);
    tab->prompts().answerRunSimulationsWith(RunAsTheBatchDoes{&notes});
    tab->panel().activating();
    const std::shared_ptr<Simulation> fresh = appendNewSimulation(*tab);
    tab->view().selectSimulations({fresh});
    tab->prompts().answerRunSimulationsWith(RunAsTheBatchDoes{&notes});
    tab->panel().plotSimulationAction()->trigger();
    EXPECT_EQ(
        notes.notes(),
        (Names{"pause", "run [Simulation 1, Simulation 5]", "cancel [Simulation 1, Simulation 5]",
               "resume", "pause", "run [Simulation 4]", "cancel [Simulation 4]", "resume", "pause",
               "run [Simulation 4]", "cancel [Simulation 4]", "resume", "pause",
               "run [Simulation 6]", "cancel [Simulation 6]", "resume"}));
}

TEST_F(SimulationPanelTest, EverySettingsDialogGetsTheWindowsEstimate)
{
    // The settings dialog pauses the estimate while it is open and gives its OK's run the cancel
    // (SimulationDialogs::EditRequest): New, Edit, Plot / Export and both double clicks.
    EstimateNotes                        notes;
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->panel().setBackgroundRuns(notes.hooks());
    tab->services().simulationDialogs().whileEditing(EditAsTheDialogDoes{&notes});
    tab->show();
    tab->selectRows({1, 3});
    tab->panel().editSimulationAction()->trigger();
    tab->selectRows({2});
    tab->panel().plotSimulationAction()->trigger();
    tab->doubleClickCell(0, Column::WARNINGS);
    tab->doubleClickCell(4, Column::NAME);
    tab->panel().newSimulationAction()->trigger();
    EXPECT_EQ(notes.notes(), (Names{"edit [Simulation 2, Simulation 4]",
                                    "pause",
                                    "cancel [Simulation 2, Simulation 4]",
                                    "resume",
                                    "edit [Simulation 3 - too short delay]",
                                    "pause",
                                    "cancel [Simulation 3 - too short delay]",
                                    "resume",
                                    "edit [Simulation 1]",
                                    "pause",
                                    "cancel [Simulation 1]",
                                    "resume",
                                    "edit [Simulation 5]",
                                    "pause",
                                    "cancel [Simulation 5]",
                                    "resume",
                                    "edit [Simulation 6]",
                                    "pause",
                                    "cancel [Simulation 6]",
                                    "resume"}));
}

TEST_F(SimulationPanelTest, APanelOfNoWindowGivesItsDialogsNoEstimate)
{
    // Without the window's hooks (setBackgroundRuns()), the requests carry none: nothing is
    // paused or cancelled.
    EstimateNotes                        notes;
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    EXPECT_FALSE(tab->panel().backgroundRuns().cancel);
    EXPECT_FALSE(tab->panel().backgroundRuns().pause);
    tab->services().simulationDialogs().whileEditing(EditAsTheDialogDoes{&notes});
    tab->prompts().answerRunSimulationsWith(RunAsTheBatchDoes{&notes});
    tab->selectRows({0});
    tab->panel().editSimulationAction()->trigger();
    tab->panel().runSimulationAction()->trigger();
    EXPECT_EQ(notes.notes(),
              (Names{"edit [Simulation 1] without the estimate", "run [Simulation 1]"}));
}

// ==================================================================== the table's CSV export

using PromptKind = Gui::Test::ScriptedUserPrompts::Kind;

/// CSV options other than the defaults: ";", 2 decimal places, no exponential notation, "%".
[[nodiscard]] CsvOptionValues tableExportOptions()
{
    CsvOptionValues options;
    options.fieldSeparator      = QStringLiteral(";");
    options.decimalPlaces       = 2;
    options.exponentialNotation = false;
    options.commentCharacter    = QStringLiteral("%");
    return options;
}

/// The answer of the table export's file question: @p name in @p directory, with the CSV options
/// @p options.
[[nodiscard]] UserPrompts::SaveFileAnswer tableExportAnswer(
    const QtRocket::Test::TempDir& directory, std::string_view name,
    const std::optional<CsvOptionValues>& options)
{
    return {.file               = directory.resolve(std::string(name)),
            .directory          = directory.path(),
            .saveSimulationData = std::nullopt,
            .selectedNameFilter = FileNames::csvFilter(),
            .csvOptions         = options};
}

/// The text of the file @p file; empty, with a test failure, when it cannot be read.
[[nodiscard]] std::string exportedText(const std::filesystem::path& file)
{
    const Result<std::vector<std::byte>> bytes = readFile(file);
    if (!bytes.has_value())
    {
        ADD_FAILURE() << bytes.error().toString();
        return {};
    }
    return bytesToString(*bytes);
}

/// The table text of @p simulations of @p tab's table with @p options
/// (SimulationTableCsvExport::tableText()).
[[nodiscard]] std::string tableTextOf(const SimulationTab&                            tab,
                                      const std::vector<std::shared_ptr<Simulation>>& simulations,
                                      const CsvOptionValues&                          options)
{
    return SimulationTableCsvExport::tableText(tab.model(), simulations,
                                               QtStrings::toUtf8(options.fieldSeparator),
                                               options.decimalPlaces, options.exponentialNotation);
}

/// The kinds of the questions and messages of @p prompts, in order.
[[nodiscard]] std::vector<PromptKind> promptKindsOf(const Gui::Test::ScriptedUserPrompts& prompts)
{
    std::vector<PromptKind> kinds;
    for (const Gui::Test::ScriptedUserPrompts::Prompt& prompt : prompts.prompts())
    {
        kinds.push_back(prompt.kind);
    }
    return kinds;
}

/// Whether @p preferences hold the table export's options (one of the keys CsvOptionPanel.
/// storePreferences() writes).
[[nodiscard]] bool storesTableExportOptions(const Preferences& preferences)
{
    return preferences.get("ExportFieldSeparator").has_value();
}

TEST_F(SimulationPanelTest, TheTableExportAsksForItsFileAsOpenRocketsChooser)
{
    // setUpSimExportCSVFileChooser(): "Save as CSV file", the CSV filter alone, the default
    // directory, "<rocket name>.csv", a SaveFileChooser, and its accessory CsvOptionPanel(
    // SimulationTableCSVExport.class), whose options the question asks after the file (U-M4-6).
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    Preferences&                       preferences = tab->services().preferences();
    const UserPrompts::SaveFileRequest request =
        SimulationPanel::csvExportRequest(tab->document(), preferences);
    EXPECT_EQ(request.title, QStringLiteral("Save as CSV file"));
    EXPECT_EQ(request.nameFilters, QStringList{QStringLiteral("Comma Separated Files (*.csv)")});
    EXPECT_TRUE(request.directory.empty());
    EXPECT_EQ(request.fileName, QStringLiteral("A simple model rocket.csv"));
    EXPECT_EQ(request.saveSimulationData, std::nullopt);
    EXPECT_TRUE(request.refuseIllegalNames);
    // Nothing stored: the panel's defaults, no comment options.
    EXPECT_EQ(request.csvOptions, std::optional(CsvOptionValues{}));

    // Stored: the directory, and the options (OpenRocket bug 50: a separator of several
    // characters whole).
    const QtRocket::Test::TempDir directory;
    preferences.setDefaultDirectory(directory.path());
    CsvOptionValues stored = tableExportOptions();
    stored.fieldSeparator  = QStringLiteral("<->");
    CsvOptionPreferences::store(preferences, CsvOptionPreferences::kSimulationTableCsvExport,
                                stored);
    tab->prompts().answerSaveFile(std::nullopt);
    EXPECT_FALSE(tab->panel().exportSimulationsToCsv(false));
    ASSERT_EQ(tab->prompts().saveFileRequests().size(), 1U);
    const UserPrompts::SaveFileRequest& asked = tab->prompts().saveFileRequests().front();
    EXPECT_EQ(asked.title, request.title);
    EXPECT_EQ(asked.nameFilters, request.nameFilters);
    EXPECT_EQ(asked.directory, directory.path());
    EXPECT_EQ(asked.fileName, request.fileName);
    EXPECT_TRUE(asked.refuseIllegalNames);
    EXPECT_EQ(asked.csvOptions, std::optional(stored));
}

TEST_F(SimulationPanelTest, TheTableExportWritesEveryRowAsOpenRocketDoes)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    const Gui::Test::SimulationTablePins* pins = Gui::Test::simulationTablePinsOrFail();
    ASSERT_NE(pins, nullptr);
    const Gui::Test::SimulationTablePinDesign* pin =
        pins->findDesign("example_a_simple_model_rocket");
    ASSERT_NE(pin, nullptr);
    const QtRocket::Test::TempDir directory;
    // The options of the pins' text (",", 3 decimal places, plain); the name without the
    // extension, which the export adds (FileHelper.forceExtension()).
    CsvOptionValues options = tableExportOptions();
    options.fieldSeparator  = QStringLiteral(",");
    options.decimalPlaces   = 3;
    tab->prompts().answerSaveFile(tableExportAnswer(directory, "table", options));
    // The File menu's item.
    tab->panel().exportSimulationTableAction()->trigger();
    const std::string written = exportedText(directory.resolve("table.csv"));
    // OpenRocket's own text of the table (SimulationTableProbe), byte for byte.
    EXPECT_EQ(written, pin->csv.value_or(std::string()));
    EXPECT_EQ(written, tableTextOf(*tab, tab->model().simulations(), options));
    EXPECT_EQ(promptKindsOf(tab->prompts()), std::vector<PromptKind>{PromptKind::SAVE_FILE});

    // The options answered are stored as the table export's (CsvOptionPanel.storePreferences()),
    // and the default directory is not (Java's export does not store it).
    const Preferences& preferences = tab->services().preferences();
    EXPECT_EQ(preferences.get("ExportFieldSeparator"), std::optional<std::string>(","));
    EXPECT_EQ(preferences.get("ExportDecimalPlaces"), std::optional<std::string>("3"));
    EXPECT_EQ(preferences.get("ExportExponentialNotation"), std::optional<std::string>("false"));
    EXPECT_EQ(preferences.get("ExportCommentCharacter"), std::optional<std::string>("%"));
    EXPECT_EQ(preferences.getDefaultDirectory(), std::nullopt);
}

TEST_F(SimulationPanelTest, TheTableExportOfTheSelectionWritesItInTheTablesOrder)
{
    // The popup's "Export simulation(s) as CSV file": the selected rows (getSelectedRows()), in
    // the view's order of a sorted table, with a tab, no decimal places and exponential notation.
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->model().setSortKeys({SortKey{.column = SimulationColumns::indexOf(Column::NAME),
                                      .order  = SortOrder::DESCENDING}});
    tab->selectRows({3, 0});
    ASSERT_EQ(tab->selectedNames(), (Names{"Simulation 5", "Simulation 2"}));
    const QtRocket::Test::TempDir directory;
    CsvOptionValues               options = tableExportOptions();
    options.fieldSeparator                = QStringLiteral("\t");
    options.decimalPlaces                 = 0;
    options.exponentialNotation           = true;
    tab->prompts().answerSaveFile(tableExportAnswer(directory, "selection.csv", options));
    tab->panel().exportSelectedSimulationsAction()->trigger();
    const std::string written = exportedText(directory.resolve("selection.csv"));
    EXPECT_EQ(written, tableTextOf(*tab, tab->view().selectedSimulations(), options));
    // The header and the two rows, Simulation 5 first.
    EXPECT_EQ(std::ranges::count(written, '\n'), 2);
    EXPECT_LT(written.find("\tSimulation 5\t"), written.find("\tSimulation 2\t"));
    EXPECT_EQ(tab->services().preferences().get("ExportFieldSeparator"),
              std::optional<std::string>("\t"));
}

TEST_F(SimulationPanelTest, WithoutRowsTheTableExportSaysSoAndAsksNothing)
{
    // simpanel.dlg.no.simulation.table.rows, in JOptionPane's plain message.
    const std::unique_ptr<SimulationTab> tab = newDesign();
    EXPECT_FALSE(tab->panel().exportSimulationsToCsv(false));
    EXPECT_FALSE(tab->panel().exportSimulationsToCsv(true));
    const std::vector<Gui::Test::ScriptedUserPrompts::Prompt>& shown = tab->prompts().prompts();
    ASSERT_EQ(shown.size(), 2U);
    EXPECT_EQ(shown[0].kind, PromptKind::MESSAGE);
    EXPECT_EQ(shown[0].title, QStringLiteral("Message"));
    EXPECT_EQ(
        shown[0].texts,
        (QStringList{QStringLiteral("INFORMATION"),
                     QStringLiteral("Simulation table has no entries\u2026 Please run a simulation "
                                    "first.")}));
    EXPECT_EQ(shown[1].texts, shown[0].texts);
    EXPECT_FALSE(storesTableExportOptions(tab->services().preferences()));
}

TEST_F(SimulationPanelTest, TheTableExportReplacesAFileOnlyAfterYes)
{
    // FileHelper.confirmWrite(): No ends the export before anything is stored or written.
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    const QtRocket::Test::TempDir directory;
    const std::filesystem::path   file = directory.write("table.csv", "old");
    tab->prompts().answerSaveFile(tableExportAnswer(directory, "table.csv", tableExportOptions()));
    tab->prompts().answerOverwrite(false);
    EXPECT_FALSE(tab->panel().exportSimulationsToCsv(false));
    EXPECT_EQ(exportedText(file), "old");
    EXPECT_FALSE(storesTableExportOptions(tab->services().preferences()));
    ASSERT_EQ(tab->prompts().prompts().size(), 2U);
    EXPECT_EQ(tab->prompts().prompts()[1].kind, PromptKind::OVERWRITE);
    EXPECT_EQ(tab->prompts().prompts()[1].texts, QStringList{QStringLiteral("table.csv")});

    tab->prompts().answerSaveFile(tableExportAnswer(directory, "table", tableExportOptions()));
    tab->prompts().answerOverwrite(true);
    EXPECT_TRUE(tab->panel().exportSimulationsToCsv(false));
    EXPECT_EQ(exportedText(file),
              tableTextOf(*tab, tab->model().simulations(), tableExportOptions()));
}

TEST_F(SimulationPanelTest, ACancelledTableExportStoresAndWritesNothing)
{
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    tab->prompts().answerSaveFile(std::nullopt);
    EXPECT_FALSE(tab->panel().exportSimulationsToCsv(false));
    EXPECT_EQ(promptKindsOf(tab->prompts()), std::vector<PromptKind>{PromptKind::SAVE_FILE});
    EXPECT_FALSE(storesTableExportOptions(tab->services().preferences()));
}

TEST_F(SimulationPanelTest, ATableExportThatCannotBeWrittenShowsErrorWritingFile)
{
    // Java's dumpDataToFile() shows the IOException's message alone; here "Error writing file"
    // with the reason. The options are stored before the file is written, as Java stores them.
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    const QtRocket::Test::TempDir directory;
    UserPrompts::SaveFileAnswer   answer =
        tableExportAnswer(directory, "table", tableExportOptions());
    answer.file = directory.resolve("missing") / "table";
    tab->prompts().answerSaveFile(answer);
    EXPECT_FALSE(tab->panel().exportSimulationsToCsv(false));
    EXPECT_FALSE(std::filesystem::exists(directory.resolve("missing")));
    const std::vector<Gui::Test::ScriptedUserPrompts::Prompt>& shown = tab->prompts().prompts();
    ASSERT_EQ(shown.size(), 2U);
    EXPECT_EQ(shown[1].kind, PromptKind::MESSAGE);
    EXPECT_EQ(shown[1].title, QStringLiteral("Error writing file"));
    ASSERT_EQ(shown[1].texts.size(), 3);
    EXPECT_EQ(shown[1].texts[0], QStringLiteral("ERROR"));
    EXPECT_EQ(shown[1].texts[1], QStringLiteral("An error occurred while writing to the file:"));
    EXPECT_TRUE(shown[1].texts[2].contains(QStringLiteral("table.csv")))
        << shown[1].texts[2].toStdString();
    EXPECT_TRUE(storesTableExportOptions(tab->services().preferences()));
}

TEST_F(SimulationPanelTest, AFileAnswerWithoutTheCsvOptionsAskedIsABug)
{
    // UserPrompts' contract: a save with CSV options answers them.
    const std::unique_ptr<SimulationTab> tab = simpleModelRocket();
    ASSERT_NE(tab, nullptr);
    const QtRocket::Test::TempDir directory;
    tab->prompts().answerSaveFile(tableExportAnswer(directory, "table", std::nullopt));
    EXPECT_THROW(tab->panel().exportSimulationsToCsv(false), BugError);
    EXPECT_FALSE(std::filesystem::exists(directory.resolve("table.csv")));
}

/// The question in which TableClosingPrompts closes the panel's window.
enum class ClosingQuestion
{
    FILE,
    OVERWRITE,
};

/// The test's prompts, which close the window (destroy the panel's holder) when @p question is
/// asked, before the answer of the script.
class TableClosingPrompts final : public Gui::Test::ForwardingUserPrompts
{
public:
    TableClosingPrompts(UserPrompts& delegate, ClosingQuestion question,
                        std::unique_ptr<QWidget>& window)
      : ForwardingUserPrompts(delegate), m_question(question), m_window(&window)
    {
    }

    [[nodiscard]] std::optional<SaveFileAnswer> askSaveFile(QWidget*               parent,
                                                            const SaveFileRequest& request) override
    {
        if (m_question == ClosingQuestion::FILE)
        {
            m_window->reset();
        }
        return delegate().askSaveFile(parent, request);
    }

    [[nodiscard]] bool askOverwrite(QWidget* parent, const QString& fileName) override
    {
        if (m_question == ClosingQuestion::OVERWRITE)
        {
            m_window->reset();
        }
        return delegate().askOverwrite(parent, fileName);
    }

private:
    ClosingQuestion           m_question;
    std::unique_ptr<QWidget>* m_window;
};

/// A panel whose window goes during @p question: the export ends there, and nothing is stored
/// or written.
void expectNoTableExportAfterThePanelWent(ClosingQuestion question)
{
    SCOPED_TRACE(question == ClosingQuestion::FILE ? "the file question"
                                                   : "the overwrite question");
    Gui::Test::TestGuiServices                services;
    OpenRocketClipboard                       clipboard;
    const std::unique_ptr<OpenRocketDocument> document =
        Gui::Test::loadSimulationExample("A simple model rocket.ork");
    ASSERT_NE(document, nullptr);
    const QtRocket::Test::TempDir directory;
    const std::filesystem::path   existing = directory.write("table.csv", "old");
    auto                          window   = std::make_unique<QWidget>();
    TableClosingPrompts           prompts(services.prompts(), question, window);
    const GuiServices             closing = services.services().withPrompts(prompts);
    auto* const panel = makeChild<SimulationPanel>(window.get(), *document, closing, clipboard);
    services.prompts().answerSaveFile(tableExportAnswer(directory, "table", tableExportOptions()));
    if (question == ClosingQuestion::OVERWRITE)
    {
        services.prompts().answerOverwrite(true);
    }
    EXPECT_FALSE(panel->exportSimulationsToCsv(false));
    EXPECT_EQ(window, nullptr);
    EXPECT_EQ(exportedText(existing), "old");
    EXPECT_FALSE(storesTableExportOptions(services.preferences()));
}

TEST_F(SimulationPanelTest, ThePanelGoingDuringAQuestionEndsTheTableExport)
{
    // A question runs an event loop, in which the panel may go with its window.
    expectNoTableExportAfterThePanelWent(ClosingQuestion::FILE);
    expectNoTableExportAfterThePanelWent(ClosingQuestion::OVERWRITE);
}

}  // namespace

}  // namespace QtRocket::Gui
