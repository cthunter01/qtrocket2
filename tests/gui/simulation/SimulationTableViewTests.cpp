// The simulations table's view (Java: SimulationPanel's ColumnTable, its mouse listener and the
// keys of its JTable, SimulationPanel.java:257-387). OpenRocket has no JUnit test of it. What
// OpenRocket's table does with the keys and on its header comes from
// tools/openrocket-gui-pins/simulation-table/probe/SimulationTableViewProbe.java, pinned here by
// hand (the family's hand-pinned.txt keeps its transcript: the KEY and HEADER lines); the orders
// of the header clicks come from the family's table (SimulationTablePins.h). Three kinds of tests:
// - SimulationTableViewTest: the view's own behaviour (its set-up and widths, the mouse and its
//   signals, OpenRocket bug 26, the keys, the selection by identity: OpenRocket bug 28);
// - SimulationTableViewGolden: a click and a second click on each column's header, the columns
//   in the reverse order, give OpenRocket's orders, for every pinned design.

#include "QtRocket/gui/simulation/SimulationTableView.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAbstractItemDelegate>
#include <QAbstractItemView>
#include <QApplication>
#include <QContextMenuEvent>
#include <QEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QModelIndex>
#include <QObject>
#include <QPoint>
#include <QSignalSpy>
#include <QStyleOptionViewItem>
#include <QTest>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/simulation/SimulationWarningsDelegate.h"
#include "QtRocket/gui/util/TableSortKeys.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/BugError.h"
#include "simulation/SimulationTablePins.h"
#include "simulation/SimulationTableTestSupport.h"
#include "tree/TreeViewMouse.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationTableViewTest = Gui::Test::GuiTest;
using Column                  = SimulationColumns::Column;
using SortKey                 = TableSortKeys::SortKey;
using SortOrder               = TableSortKeys::SortOrder;
using Table                   = Gui::Test::SimulationTableFixture;

/// "A simple model rocket" (five simulations, the third with a warning) in a table.
[[nodiscard]] std::unique_ptr<Table> simpleModelRocket()
{
    std::unique_ptr<OpenRocketDocument> document =
        Gui::Test::loadSimulationExample("A simple model rocket.ork");
    return document == nullptr ? nullptr : std::make_unique<Table>(std::move(document));
}

/// What the view's signals told: each click as "<simulation> <column id>", each menu by its
/// position.
class Signals
{
public:
    explicit Signals(const SimulationTableView& view)
    {
        QObject::connect(
            &view, &SimulationTableView::cellClicked, &view,
            [told = m_told](const std::shared_ptr<Simulation>& simulation, Column column) {
                told->clicks.push_back(text(simulation, column));
            });
        QObject::connect(
            &view, &SimulationTableView::cellDoubleClicked, &view,
            [told = m_told](const std::shared_ptr<Simulation>& simulation, Column column) {
                told->doubleClicks.push_back(text(simulation, column));
            });
        QObject::connect(
            &view, &SimulationTableView::contextMenuRequested, &view,
            [told = m_told](const QPoint& position) { told->menus.push_back(position); });
    }

    [[nodiscard]] const std::vector<std::string>& clicks() const noexcept { return m_told->clicks; }
    [[nodiscard]] const std::vector<std::string>& doubleClicks() const noexcept
    {
        return m_told->doubleClicks;
    }
    [[nodiscard]] const std::vector<QPoint>& menus() const noexcept { return m_told->menus; }

private:
    struct Told
    {
        std::vector<std::string> clicks;
        std::vector<std::string> doubleClicks;
        std::vector<QPoint>      menus;
    };

    [[nodiscard]] static std::string text(const std::shared_ptr<Simulation>& simulation,
                                          Column                             column)
    {
        return (simulation == nullptr ? std::string("<null>") : simulation->getName()) + " " +
               std::string(SimulationColumns::id(column));
    }

    std::shared_ptr<Told> m_told = std::make_shared<Told>();
};

/// A click of @p button on the cell of @p row in @p column.
void clickCell(const Table& table, int row, Column column,
               Qt::KeyboardModifiers modifiers = Qt::NoModifier,
               Qt::MouseButton       button    = Qt::LeftButton)
{
    QTest::mouseClick(table.view().viewport(), button, modifiers, table.cellCentre(row, column));
}

/// A double click (Test::doubleClickAt()) on the cell of @p row in @p column.
void doubleClickCell(const Table& table, int row, Column column,
                     Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    Gui::Test::doubleClickAt(*table.view().viewport(), table.cellCentre(row, column), modifiers);
}

/// A click of @p button on @p column's header.
void clickHeader(const Table& table, Column column, Qt::MouseButton button = Qt::LeftButton)
{
    QTest::mouseClick(table.view().horizontalHeader()->viewport(), button, Qt::NoModifier,
                      table.headerCentre(column));
}

/// The sort keys as the probe prints them: "name ASCENDING, apogee ASCENDING", "<none>".
[[nodiscard]] std::string sortKeysText(const SimulationTableModel& model)
{
    std::string text;
    for (const SortKey& key : model.sortKeys().keys())
    {
        if (!text.empty())
        {
            text += ", ";
        }
        text += std::string(SimulationColumns::id(SimulationColumns::columnAt(key.column))) +
                (key.order == SortOrder::ASCENDING ? " ASCENDING" : " DESCENDING");
    }
    return text.empty() ? std::string("<none>") : text;
}

/// The selection as the probe's KEY lines print it, without the anchor: "rows [1 2 3] lead 3
/// column name".
[[nodiscard]] std::string selectionText(const SimulationTableView& view)
{
    std::string                rows;
    const QItemSelectionModel& selection = *view.selectionModel();
    for (int row = 0; row < view.model()->rowCount(); ++row)
    {
        if (selection.isRowSelected(row))
        {
            rows += (rows.empty() ? "" : " ") + std::to_string(row);
        }
    }
    const QModelIndex current = view.currentIndex();
    const std::string column =
        current.isValid()
            ? std::string(SimulationColumns::id(SimulationColumns::columnAt(current.column())))
            : std::string("<none>");
    return "rows [" + rows + "] lead " + std::to_string(current.isValid() ? current.row() : -1) +
           " column " + column;
}

/// Enter (or Shift+Enter) on the view.
void enter(SimulationTableView& view, bool forward = true)
{
    QTest::keyClick(&view, Qt::Key_Return, forward ? Qt::NoModifier : Qt::ShiftModifier);
}

/// The names of the simulations, "Simulation 1" ... (the third's has more).
[[nodiscard]] std::vector<std::string> names(const std::vector<std::shared_ptr<Simulation>>& list)
{
    std::vector<std::string> result;
    result.reserve(list.size());
    for (const std::shared_ptr<Simulation>& simulation : list)
    {
        result.push_back(simulation->getName());
    }
    return result;
}

// ------------------------------------------------------------------------------ the table

TEST_F(SimulationTableViewTest, SetsUpTheTableAsOpenRocket)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    const SimulationTableView& view = table->view();
    EXPECT_EQ(view.objectName(), QStringLiteral("simulationTable"));
    EXPECT_EQ(&view.tableModel(), &table->model());
    EXPECT_EQ(view.model(), &table->model());
    EXPECT_EQ(view.selectionMode(), QAbstractItemView::ExtendedSelection);
    EXPECT_EQ(view.selectionBehavior(), QAbstractItemView::SelectRows);
    EXPECT_EQ(view.editTriggers(), QAbstractItemView::NoEditTriggers);
    EXPECT_FALSE(view.isSortingEnabled());
    EXPECT_TRUE(view.verticalHeader()->isHidden());
    const QHeaderView& header = *view.horizontalHeader();
    EXPECT_TRUE(header.sectionsMovable());
    EXPECT_TRUE(header.sectionsClickable());
    EXPECT_TRUE(header.isSortIndicatorShown());
    EXPECT_FALSE(header.highlightSections());
    EXPECT_FALSE(header.stretchLastSection());
    EXPECT_EQ(header.minimumSectionSize(), SimulationTableView::kMinimumColumnWidth);
    // Every column is shown (the column chooser hides some), in the model's order.
    EXPECT_EQ(table->visibleIds(),
              "status,warnings,name,configuration,simulationStepper,launchRodVelocity,apogee,"
              "deploymentVelocity,optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,"
              "flightTime,groundHitVelocity");
    // The warnings column alone has its delegate.
    for (const Column column : SimulationColumns::kAllColumns)
    {
        const bool warnings = column == Column::WARNINGS;
        EXPECT_EQ(dynamic_cast<SimulationWarningsDelegate*>(
                      view.itemDelegateForColumn(SimulationColumns::indexOf(column))) != nullptr,
                  warnings)
            << SimulationColumns::id(column);
    }
}

TEST_F(SimulationTableViewTest, RefusesAnotherModel)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    EXPECT_THROW(table->view().setModel(nullptr), BugError);
    EXPECT_EQ(table->view().model(), &table->model());
}

/// The widest warnings cell of @p table as its delegate measures it.
[[nodiscard]] int widestWarningsCell(Table& table)
{
    const QAbstractItemDelegate* delegate =
        table.view().itemDelegateForColumn(SimulationColumns::indexOf(Column::WARNINGS));
    int                  widest = 0;
    QStyleOptionViewItem option;
    option.initFrom(&table.view());
    option.font = table.view().font();
    for (int row = 0; row < table.model().rowCount(); ++row)
    {
        const QModelIndex index =
            table.model().index(row, SimulationColumns::indexOf(Column::WARNINGS));
        widest = std::max(widest, delegate->sizeHint(option, index).width());
    }
    return widest;
}

/// Each column's width and resize mode but the warnings column's: "status 26 fixed".
[[nodiscard]] std::vector<std::string> columnWidths(const QHeaderView& header)
{
    std::vector<std::string> widths;
    for (const Column column : SimulationColumns::kAllColumns)
    {
        if (column == Column::WARNINGS)
        {
            continue;
        }
        const int section = SimulationColumns::indexOf(column);
        widths.push_back(
            std::string(SimulationColumns::id(column)) + " " +
            std::to_string(header.sectionSize(section)) +
            (header.sectionResizeMode(section) == QHeaderView::Fixed ? " fixed" : " interactive"));
    }
    return widths;
}

TEST_F(SimulationTableViewTest, GivesTheColumnsOpenRocketsWidths)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    const QHeaderView& header = *table->view().horizontalHeader();
    // ColumnTableModel.setColumnWidths(): the status column exactly 26, the name and configuration
    // columns 125, the others 100.
    EXPECT_EQ(columnWidths(header),
              (std::vector<std::string>{
                  "status 26 fixed", "name 125 interactive", "configuration 125 interactive",
                  "simulationStepper 100 interactive", "launchRodVelocity 100 interactive",
                  "apogee 100 interactive", "deploymentVelocity 100 interactive",
                  "optimumCoastTime 100 interactive", "maxVelocity 100 interactive",
                  "maxAcceleration 100 interactive", "timeToApogee 100 interactive",
                  "flightTime 100 interactive", "groundHitVelocity 100 interactive"}));
    // The warnings column: its widest cell (the third simulation's warning) and the padding, at
    // least its minimum.
    const int widest = widestWarningsCell(*table);
    EXPECT_GT(widest, SimulationWarningsDelegate::kLeftPadding);
    EXPECT_EQ(header.sectionSize(SimulationColumns::indexOf(Column::WARNINGS)),
              std::max(widest + SimulationColumns::kAutoSizePadding,
                       SimulationColumns::defaultWidth(Column::WARNINGS)));
    EXPECT_EQ(header.sectionResizeMode(SimulationColumns::indexOf(Column::WARNINGS)),
              QHeaderView::Interactive);
}

TEST_F(SimulationTableViewTest, GivesAWarningsColumnWithoutWarningsItsMinimumWidth)
{
    const Table        table(OpenRocketDocumentFactory::createNewRocket());
    const QHeaderView& header = *table.view().horizontalHeader();
    EXPECT_EQ(header.sectionSize(SimulationColumns::indexOf(Column::WARNINGS)), 40);
}

TEST_F(SimulationTableViewTest, KeepsTheWarningsColumnAtItsMinimumWidth)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    QHeaderView& header   = *table->view().horizontalHeader();
    const int    warnings = SimulationColumns::indexOf(Column::WARNINGS);
    header.resizeSection(warnings, 20);
    EXPECT_EQ(header.sectionSize(warnings), 40);
    header.resizeSection(warnings, 70);
    EXPECT_EQ(header.sectionSize(warnings), 70);
    // Hidden and shown again, at its width.
    header.setSectionHidden(warnings, true);
    header.setSectionHidden(warnings, false);
    EXPECT_EQ(header.sectionSize(warnings), 70);
    // Another column is narrower.
    header.resizeSection(SimulationColumns::indexOf(Column::APOGEE), 20);
    EXPECT_EQ(header.sectionSize(SimulationColumns::indexOf(Column::APOGEE)), 20);
}

TEST_F(SimulationTableViewTest, FindsTheColumnsWhereverTheyAre)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    const SimulationTableView& view = table->view();
    table->moveColumn(Column::APOGEE, 0);
    view.horizontalHeader()->setSectionHidden(SimulationColumns::indexOf(Column::WARNINGS), true);
    EXPECT_EQ(view.visibleColumns().front(), Column::APOGEE);
    EXPECT_EQ(view.viewColumn(Column::APOGEE), 0);
    EXPECT_EQ(view.viewColumn(Column::STATUS), 1);
    EXPECT_EQ(view.viewColumn(Column::NAME), 2);
    EXPECT_EQ(view.viewColumn(Column::WARNINGS), -1);
    EXPECT_FALSE(view.isColumnVisible(Column::WARNINGS));
    EXPECT_TRUE(view.isColumnVisible(Column::NAME));
    EXPECT_EQ(view.columnAtPoint(table->cellCentre(0, Column::APOGEE)), Column::APOGEE);
    EXPECT_EQ(view.columnAtPoint(table->cellCentre(2, Column::NAME)), Column::NAME);
    EXPECT_EQ(view.columnAtPoint(QPoint(1590, 10)), std::nullopt);
}

// ------------------------------------------------------------------------------ the header

TEST_F(SimulationTableViewTest, SortsByTheHeadersAsOpenRocket)
{
    // The probe's HEADER lines: a click toggles, the second click of a double click does not,
    // the right button never sorts, at most three keys, and a column is sorted by its own header
    // wherever it was moved.
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    table->show();
    const SimulationTableModel& model = table->model();
    EXPECT_EQ(sortKeysText(model), "<none>");
    clickHeader(*table, Column::NAME);
    EXPECT_EQ(sortKeysText(model), "name ASCENDING");
    clickHeader(*table, Column::NAME);
    EXPECT_EQ(sortKeysText(model), "name DESCENDING");
    clickHeader(*table, Column::NAME);
    EXPECT_EQ(sortKeysText(model), "name ASCENDING");
    // A double click: one toggle (its first click).
    Gui::Test::doubleClickAt(*table->view().horizontalHeader()->viewport(),
                             table->headerCentre(Column::NAME));
    EXPECT_EQ(sortKeysText(model), "name DESCENDING");
    clickHeader(*table, Column::NAME);
    EXPECT_EQ(sortKeysText(model), "name ASCENDING");
    clickHeader(*table, Column::APOGEE);
    EXPECT_EQ(sortKeysText(model), "apogee ASCENDING, name ASCENDING");
    clickHeader(*table, Column::FLIGHT_TIME);
    EXPECT_EQ(sortKeysText(model), "flightTime ASCENDING, apogee ASCENDING, name ASCENDING");
    clickHeader(*table, Column::STATUS);
    EXPECT_EQ(sortKeysText(model), "status ASCENDING, flightTime ASCENDING, apogee ASCENDING");
    clickHeader(*table, Column::APOGEE, Qt::RightButton);
    EXPECT_EQ(sortKeysText(model), "status ASCENDING, flightTime ASCENDING, apogee ASCENDING");
    table->moveColumn(Column::APOGEE, 0);
    table->view().horizontalHeader()->setSectionHidden(SimulationColumns::indexOf(Column::WARNINGS),
                                                       true);
    clickHeader(*table, Column::SIMULATION_STEPPER);
    EXPECT_EQ(sortKeysText(model),
              "simulationStepper ASCENDING, status ASCENDING, flightTime ASCENDING");
    clickHeader(*table, Column::APOGEE);
    EXPECT_EQ(sortKeysText(model),
              "apogee ASCENDING, simulationStepper ASCENDING, status ASCENDING");
}

// ------------------------------------------------------------------------------ the mouse

TEST_F(SimulationTableViewTest, TellsAClickWithTheFirstSelectedSimulation)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    table->show();
    const Signals told(table->view());
    clickCell(*table, 1, Column::STATUS);
    EXPECT_EQ(table->selectedNames(), (std::vector<std::string>{"Simulation 2"}));
    clickCell(*table, 3, Column::APOGEE, Qt::ControlModifier);
    EXPECT_EQ(table->selectedNames(), (std::vector<std::string>{"Simulation 2", "Simulation 4"}));
    // Java's getSelectedRow(): the topmost selected row, not the clicked one.
    EXPECT_EQ(told.clicks(),
              (std::vector<std::string>{"Simulation 2 status", "Simulation 2 apogee"}));
    EXPECT_TRUE(told.doubleClicks().empty());
    EXPECT_TRUE(told.menus().empty());
}

TEST_F(SimulationTableViewTest, ClearsTheSelectionOnAClickBesideTheRows)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    table->show();
    const Signals told(table->view());
    clickCell(*table, 1, Column::NAME);
    QTest::mouseClick(table->view().viewport(), Qt::LeftButton, Qt::NoModifier,
                      table->belowTheRows());
    EXPECT_TRUE(table->selectedNames().empty());
    // With Ctrl held too (Qt's view keeps the selection; OpenRocket's listener clears it).
    clickCell(*table, 1, Column::NAME);
    QTest::mouseClick(table->view().viewport(), Qt::LeftButton, Qt::ControlModifier,
                      table->belowTheRows());
    EXPECT_TRUE(table->selectedNames().empty());
    // A Ctrl+click that leaves no row selected tells nothing.
    clickCell(*table, 2, Column::NAME);
    clickCell(*table, 2, Column::NAME, Qt::ControlModifier);
    EXPECT_TRUE(table->selectedNames().empty());
    EXPECT_EQ(told.clicks(), (std::vector<std::string>{"Simulation 2 name", "Simulation 2 name",
                                                       "Simulation 3 - too short delay name"}));
}

TEST_F(SimulationTableViewTest, SelectsNothingWithTheMiddleButton)
{
    // A JTable selects with the left button only; a click beside the rows clears the selection
    // whatever the button (OpenRocket's listener).
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    table->show();
    const Signals told(table->view());
    clickCell(*table, 1, Column::NAME);
    clickCell(*table, 3, Column::NAME, Qt::NoModifier, Qt::MiddleButton);
    EXPECT_EQ(table->selectedNames(), (std::vector<std::string>{"Simulation 2"}));
    QTest::mouseClick(table->view().viewport(), Qt::MiddleButton, Qt::NoModifier,
                      table->belowTheRows());
    EXPECT_TRUE(table->selectedNames().empty());
    EXPECT_EQ(told.clicks(), (std::vector<std::string>{"Simulation 2 name"}));
    EXPECT_TRUE(told.menus().empty());
}

TEST_F(SimulationTableViewTest, TellsADoubleClickAfterItsFirstClick)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    table->show();
    const Signals told(table->view());
    doubleClickCell(*table, 3, Column::STATUS);
    doubleClickCell(*table, 3, Column::WARNINGS);
    doubleClickCell(*table, 3, Column::FLIGHT_TIME);
    EXPECT_EQ(told.clicks(),
              (std::vector<std::string>{"Simulation 4 status", "Simulation 4 warnings",
                                        "Simulation 4 flightTime"}));
    EXPECT_EQ(told.doubleClicks(),
              (std::vector<std::string>{"Simulation 4 status", "Simulation 4 warnings",
                                        "Simulation 4 flightTime"}));
}

TEST_F(SimulationTableViewTest, SelectsTheFirstSelectedRowAloneForADoubleClickThatOpensIt)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    table->show();
    const Signals told(table->view());
    // Rows 1 to 3, then a double click with Shift on row 3: the warnings column keeps them.
    clickCell(*table, 1, Column::NAME);
    doubleClickCell(*table, 3, Column::WARNINGS, Qt::ShiftModifier);
    EXPECT_EQ(table->selectedNames(),
              (std::vector<std::string>{"Simulation 2", "Simulation 3 - too short delay",
                                        "Simulation 4"}));
    // The status column too.
    doubleClickCell(*table, 3, Column::STATUS, Qt::ShiftModifier);
    EXPECT_EQ(table->selectedNames().size(), 3U);
    // Any other column: the first selected row alone.
    doubleClickCell(*table, 3, Column::NAME, Qt::ShiftModifier);
    EXPECT_EQ(table->selectedNames(), (std::vector<std::string>{"Simulation 2"}));
    EXPECT_EQ(table->view().currentIndex().row(), 1);
    EXPECT_EQ(told.doubleClicks(),
              (std::vector<std::string>{"Simulation 2 warnings", "Simulation 2 status",
                                        "Simulation 2 name"}));
}

TEST_F(SimulationTableViewTest, TellsTheClickedColumnWhereverItIs)
{
    // OpenRocket bug 26, fixed: OpenRocket tests the view columns 0 (run) and 1 (warnings).
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    QHeaderView& header = *table->view().horizontalHeader();
    header.setSectionHidden(SimulationColumns::indexOf(Column::STATUS), true);
    table->moveColumn(Column::APOGEE, 0);
    table->moveColumn(Column::WARNINGS, header.count() - 1);
    table->show();
    ASSERT_EQ(table->view().visibleColumns().at(0), Column::APOGEE);
    ASSERT_EQ(table->view().visibleColumns().at(1), Column::NAME);
    const Signals told(table->view());
    clickCell(*table, 2, Column::APOGEE);
    doubleClickCell(*table, 2, Column::NAME);
    doubleClickCell(*table, 2, Column::WARNINGS);
    header.setSectionHidden(SimulationColumns::indexOf(Column::STATUS), false);
    table->moveColumn(Column::STATUS, header.count() - 2);
    clickCell(*table, 4, Column::STATUS);
    EXPECT_EQ(told.clicks(),
              (std::vector<std::string>{
                  "Simulation 3 - too short delay apogee", "Simulation 3 - too short delay name",
                  "Simulation 3 - too short delay warnings", "Simulation 5 status"}));
    EXPECT_EQ(told.doubleClicks(),
              (std::vector<std::string>{"Simulation 3 - too short delay name",
                                        "Simulation 3 - too short delay warnings"}));
}

TEST_F(SimulationTableViewTest, AsksForTheContextMenuOfTheSelection)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    table->show();
    const SimulationTableView& view = table->view();
    const Signals              told(view);
    // A row that is not selected: it alone.
    clickCell(*table, 0, Column::NAME);
    Gui::Test::rightClickAt(*view.viewport(), table->cellCentre(2, Column::APOGEE));
    EXPECT_EQ(table->selectedNames(), (std::vector<std::string>{"Simulation 3 - too short delay"}));
    // A selected row of several: the selection stays.
    clickCell(*table, 3, Column::NAME, Qt::ControlModifier);
    Gui::Test::rightClickAt(*view.viewport(), table->cellCentre(3, Column::FLIGHT_TIME));
    EXPECT_EQ(table->selectedNames(),
              (std::vector<std::string>{"Simulation 3 - too short delay", "Simulation 4"}));
    ASSERT_EQ(told.menus().size(), 2U);
    EXPECT_EQ(told.menus().at(1),
              view.viewport()->mapToGlobal(table->cellCentre(3, Column::FLIGHT_TIME)));
    // Beside the rows: no menu, no selection.
    Gui::Test::rightClickAt(*view.viewport(), table->belowTheRows());
    EXPECT_TRUE(table->selectedNames().empty());
    EXPECT_EQ(told.menus().size(), 2U);
    // The keyboard's menu key: at the current row, when a row is selected.
    QContextMenuEvent keyboard(QContextMenuEvent::Keyboard, QPoint(0, 0),
                               view.viewport()->mapToGlobal(QPoint(0, 0)));
    QApplication::sendEvent(view.viewport(), &keyboard);
    EXPECT_EQ(told.menus().size(), 2U);
    clickCell(*table, 4, Column::NAME);
    QContextMenuEvent again(QContextMenuEvent::Keyboard, QPoint(0, 0),
                            view.viewport()->mapToGlobal(QPoint(0, 0)));
    QApplication::sendEvent(view.viewport(), &again);
    ASSERT_EQ(told.menus().size(), 3U);
    EXPECT_EQ(told.menus().at(2), view.viewport()->mapToGlobal(table->cellCentre(4, Column::NAME)));
    EXPECT_TRUE(told.doubleClicks().empty());
}

// ------------------------------------------------------------------------------ the keys

/// Whether @p view takes the key @p key with @p modifiers (a QKeyEvent it accepts).
[[nodiscard]] bool takesKey(SimulationTableView& view, int key, Qt::KeyboardModifiers modifiers)
{
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(&view, &event);
    return event.isAccepted();
}

TEST_F(SimulationTableViewTest, LeavesTheWindowsKeysToTheWindow)
{
    // The probe's BIND lines: Ctrl+E, X, C, V, D and Ctrl+DELETE are "none" in the table's map,
    // DELETE is not in it.
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    SimulationTableView& view = table->view();
    view.selectSimulations({table->model().simulation(1)});
    EXPECT_FALSE(takesKey(view, Qt::Key_C, Qt::ControlModifier));
    EXPECT_FALSE(takesKey(view, Qt::Key_X, Qt::ControlModifier));
    EXPECT_FALSE(takesKey(view, Qt::Key_V, Qt::ControlModifier));
    EXPECT_FALSE(takesKey(view, Qt::Key_E, Qt::ControlModifier));
    EXPECT_FALSE(takesKey(view, Qt::Key_D, Qt::ControlModifier));
    EXPECT_FALSE(takesKey(view, Qt::Key_Delete, Qt::ControlModifier));
    EXPECT_FALSE(takesKey(view, Qt::Key_Delete, Qt::NoModifier));
    // Nothing copied (GuiTest checks the clipboard), the selection as it was.
    EXPECT_EQ(table->selectedNames(), (std::vector<std::string>{"Simulation 2"}));
    // Its own keys it takes.
    EXPECT_TRUE(takesKey(view, Qt::Key_Return, Qt::NoModifier));
    EXPECT_TRUE(takesKey(view, Qt::Key_Down, Qt::NoModifier));
}

TEST_F(SimulationTableViewTest, IgnoresTypedText)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    SimulationTableView& view = table->view();
    view.selectSimulations({table->model().simulation(0)});
    QTest::keyClicks(&view, QStringLiteral("Simulation 5"));
    EXPECT_EQ(table->selectedNames(), (std::vector<std::string>{"Simulation 1"}));
}

TEST_F(SimulationTableViewTest, MovesFromRowToRowWithTabAndTheArrows)
{
    // TableRowTraversal: OpenRocket's TableRowTraversalPolicy on the table.
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    SimulationTableView& view = table->view();
    view.selectSimulations({table->model().simulation(1)});
    QTest::keyClick(&view, Qt::Key_Right);
    EXPECT_EQ(table->selectedNames(), (std::vector<std::string>{"Simulation 3 - too short delay"}));
    QTest::keyClick(&view, Qt::Key_Tab);
    EXPECT_EQ(table->selectedNames(), (std::vector<std::string>{"Simulation 4"}));
    QTest::keyClick(&view, Qt::Key_Left);
    EXPECT_EQ(table->selectedNames(), (std::vector<std::string>{"Simulation 3 - too short delay"}));
}

TEST_F(SimulationTableViewTest, AddsTheCurrentRowWithSpaceAndDoesNothingWithEscapeOrF2)
{
    // The probe's BIND lines: SPACE is JTable's addToSelection (the lead row added to the
    // selection, which keeps its other rows), ESCAPE its cancel and F2 its startEditing, which do
    // nothing in a table without an editable cell.
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    SimulationTableView& view = table->view();
    view.selectSimulations({table->model().simulation(1)});
    // Ctrl+Down moves the current row alone (JTable's selectNextRowChangeLead).
    QTest::keyClick(&view, Qt::Key_Down, Qt::ControlModifier);
    ASSERT_EQ(view.currentIndex().row(), 2);
    ASSERT_EQ(table->selectedNames(), (std::vector<std::string>{"Simulation 2"}));
    const std::vector<std::string> both{"Simulation 2", "Simulation 3 - too short delay"};
    QTest::keyClick(&view, Qt::Key_Space);
    EXPECT_EQ(table->selectedNames(), both);
    // A row that is selected already stays selected.
    QTest::keyClick(&view, Qt::Key_Space);
    EXPECT_EQ(table->selectedNames(), both);
    QTest::keyClick(&view, Qt::Key_Escape);
    QTest::keyClick(&view, Qt::Key_F2);
    EXPECT_EQ(table->selectedNames(), both);
    EXPECT_EQ(view.currentIndex().row(), 2);
}

/// A step of the probe's KEY lines: what is done, then the selection it leaves (selectionText();
/// not compared without one).
struct KeyStep
{
    enum class Action
    {
        /// A click on the cell of row and column, with Ctrl or Shift held.
        CLICK,
        CTRL_CLICK,
        SHIFT_CLICK,
        ENTER,
        SHIFT_ENTER,
        /// Ctrl+A.
        SELECT_ALL,
        /// No selection and no current cell (the probe sets the leads to -1).
        CLEAR,
        /// No selection, the current cell kept (JTable.clearSelection()).
        CLEAR_SELECTION,
    };

    Action      action   = Action::CLICK;
    int         row      = -1;
    Column      column   = Column::STATUS;
    const char* expected = nullptr;
};

using KeyAction = KeyStep::Action;

/// Does @p step on @p table.
void perform(const Table& table, const KeyStep& step)
{
    SimulationTableView& view = table.view();
    switch (step.action)
    {
        case KeyAction::CLICK:
            clickCell(table, step.row, step.column);
            break;
        case KeyAction::CTRL_CLICK:
            clickCell(table, step.row, step.column, Qt::ControlModifier);
            break;
        case KeyAction::SHIFT_CLICK:
            clickCell(table, step.row, step.column, Qt::ShiftModifier);
            break;
        case KeyAction::ENTER:
            enter(view);
            break;
        case KeyAction::SHIFT_ENTER:
            enter(view, false);
            break;
        case KeyAction::SELECT_ALL:
            QTest::keyClick(&view, Qt::Key_A, Qt::ControlModifier);
            break;
        case KeyAction::CLEAR:
            view.selectionModel()->clear();
            break;
        case KeyAction::CLEAR_SELECTION:
            view.clearSelection();
            break;
    }
}

/// Does @p steps on @p table, each followed by its selection.
void expectKeySteps(const Table& table, std::span<const KeyStep> steps)
{
    for (std::size_t i = 0; i < steps.size(); ++i)
    {
        perform(table, steps[i]);
        if (steps[i].expected != nullptr)
        {
            EXPECT_EQ(selectionText(table.view()), std::string_view(steps[i].expected))
                << "step " << i;
        }
    }
}

/// The probe's KEY lines (their anchors aside), on "A simple model rocket" without the stepper
/// column: no selection, one row, the last and the first cells; several rows; the current cell in
/// a row that is not selected; nothing selected, the current cell kept.
constexpr std::array kProbeKeySteps{
    KeyStep{.action = KeyAction::CLEAR, .expected = "rows [] lead -1 column <none>"},
    KeyStep{.action = KeyAction::ENTER, .expected = "rows [0] lead 0 column status"},
    KeyStep{.action = KeyAction::CLEAR, .expected = "rows [] lead -1 column <none>"},
    KeyStep{.action   = KeyAction::SHIFT_ENTER,
            .expected = "rows [4] lead 4 column groundHitVelocity"},
    KeyStep{.action   = KeyAction::CLICK,
            .row      = 2,
            .column   = Column::APOGEE,
            .expected = "rows [2] lead 2 column apogee"},
    KeyStep{.action = KeyAction::ENTER, .expected = "rows [3] lead 3 column apogee"},
    KeyStep{.action = KeyAction::ENTER, .expected = "rows [4] lead 4 column apogee"},
    KeyStep{.action = KeyAction::ENTER, .expected = "rows [0] lead 0 column deploymentVelocity"},
    KeyStep{.action = KeyAction::SHIFT_ENTER, .expected = "rows [4] lead 4 column apogee"},
    KeyStep{.action = KeyAction::SHIFT_ENTER, .expected = "rows [3] lead 3 column apogee"},
    KeyStep{.action = KeyAction::CLICK, .row = 4, .column = Column::GROUND_HIT_VELOCITY},
    KeyStep{.action = KeyAction::ENTER, .expected = "rows [0] lead 0 column status"},
    KeyStep{.action = KeyAction::CLICK, .row = 0, .column = Column::STATUS},
    KeyStep{.action   = KeyAction::SHIFT_ENTER,
            .expected = "rows [4] lead 4 column groundHitVelocity"},
    KeyStep{.action = KeyAction::CLICK, .row = 1, .column = Column::NAME},
    KeyStep{.action   = KeyAction::SHIFT_CLICK,
            .row      = 3,
            .column   = Column::NAME,
            .expected = "rows [1 2 3] lead 3 column name"},
    KeyStep{.action = KeyAction::ENTER, .expected = "rows [1 2 3] lead 1 column configuration"},
    KeyStep{.action = KeyAction::ENTER, .expected = "rows [1 2 3] lead 2 column configuration"},
    KeyStep{.action = KeyAction::ENTER, .expected = "rows [1 2 3] lead 3 column configuration"},
    KeyStep{.action = KeyAction::ENTER, .expected = "rows [1 2 3] lead 1 column launchRodVelocity"},
    KeyStep{.action   = KeyAction::SHIFT_ENTER,
            .expected = "rows [1 2 3] lead 3 column configuration"},
    KeyStep{.action   = KeyAction::SHIFT_ENTER,
            .expected = "rows [1 2 3] lead 2 column configuration"},
    KeyStep{.action = KeyAction::CLICK, .row = 0, .column = Column::APOGEE},
    KeyStep{.action   = KeyAction::CTRL_CLICK,
            .row      = 3,
            .column   = Column::APOGEE,
            .expected = "rows [0 3] lead 3 column apogee"},
    KeyStep{.action = KeyAction::ENTER, .expected = "rows [0 3] lead 0 column deploymentVelocity"},
    KeyStep{.action = KeyAction::ENTER, .expected = "rows [0 3] lead 3 column deploymentVelocity"},
    KeyStep{.action = KeyAction::ENTER, .expected = "rows [0 3] lead 0 column optimumCoastTime"},
    KeyStep{.action   = KeyAction::SELECT_ALL,
            .expected = "rows [0 1 2 3 4] lead 0 column optimumCoastTime"},
    KeyStep{.action   = KeyAction::ENTER,
            .expected = "rows [0 1 2 3 4] lead 1 column optimumCoastTime"},
    KeyStep{.action = KeyAction::CLICK, .row = 1, .column = Column::NAME},
    KeyStep{.action = KeyAction::CTRL_CLICK, .row = 2, .column = Column::NAME},
    KeyStep{.action = KeyAction::CTRL_CLICK, .row = 3, .column = Column::NAME},
    KeyStep{.action   = KeyAction::CTRL_CLICK,
            .row      = 3,
            .column   = Column::NAME,
            .expected = "rows [1 2] lead 3 column name"},
    KeyStep{.action = KeyAction::ENTER, .expected = "rows [1 2] lead 1 column configuration"},
    KeyStep{.action = KeyAction::ENTER, .expected = "rows [1 2] lead 2 column configuration"},
    KeyStep{.action = KeyAction::CLICK, .row = 1, .column = Column::NAME},
    KeyStep{.action = KeyAction::CTRL_CLICK, .row = 3, .column = Column::NAME},
    KeyStep{.action   = KeyAction::CTRL_CLICK,
            .row      = 3,
            .column   = Column::NAME,
            .expected = "rows [1] lead 3 column name"},
    KeyStep{.action = KeyAction::ENTER, .expected = "rows [1] lead 1 column configuration"},
    KeyStep{.action = KeyAction::CLICK, .row = 1, .column = Column::NAME},
    KeyStep{.action = KeyAction::CTRL_CLICK, .row = 3, .column = Column::NAME},
    KeyStep{.action = KeyAction::CTRL_CLICK, .row = 3, .column = Column::NAME},
    KeyStep{.action = KeyAction::SHIFT_ENTER, .expected = "rows [1] lead 1 column name"},
    KeyStep{.action = KeyAction::CLICK, .row = 2, .column = Column::APOGEE},
    KeyStep{.action = KeyAction::CLEAR_SELECTION, .expected = "rows [] lead 2 column apogee"},
    KeyStep{.action = KeyAction::ENTER, .expected = "rows [3] lead 3 column apogee"},
    KeyStep{.action = KeyAction::CLICK, .row = 2, .column = Column::APOGEE},
    KeyStep{.action = KeyAction::CLEAR_SELECTION},
    KeyStep{.action = KeyAction::SHIFT_ENTER, .expected = "rows [1] lead 1 column apogee"},
};

TEST_F(SimulationTableViewTest, MovesThroughTheCellsWithEnterAsOpenRocket)
{
    // The probe's KEY lines, on OpenRocket's table as the panel makes it: the stepper column
    // hidden.
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    QHeaderView& header = *table->view().horizontalHeader();
    header.setSectionHidden(SimulationColumns::indexOf(Column::SIMULATION_STEPPER), true);
    table->show();
    expectKeySteps(*table, kProbeKeySteps);
    // A current cell in a column hidden afterwards: from the shown column before it.
    clickCell(*table, 2, Column::APOGEE);
    header.setSectionHidden(SimulationColumns::indexOf(Column::APOGEE), true);
    enter(table->view());
    EXPECT_EQ(selectionText(table->view()), "rows [3] lead 3 column launchRodVelocity");
    header.setSectionHidden(SimulationColumns::indexOf(Column::APOGEE), false);
    // Sorted: by the view's rows.
    table->model().setSortKeys({SortKey{.column = SimulationColumns::indexOf(Column::APOGEE),
                                        .order  = SortOrder::DESCENDING}});
    clickCell(*table, 0, Column::NAME);
    enter(table->view());
    EXPECT_EQ(selectionText(table->view()), "rows [1] lead 1 column name");
    EXPECT_EQ(table->selectedNames(), (std::vector<std::string>{"Simulation 4"}));
    // The keypad's Enter is Enter.
    QTest::keyClick(&table->view(), Qt::Key_Enter, Qt::KeypadModifier);
    EXPECT_EQ(selectionText(table->view()), "rows [2] lead 2 column name");
}

TEST_F(SimulationTableViewTest, DoesNothingWithEnterInATableWithoutRows)
{
    const Table table(OpenRocketDocumentFactory::createNewRocket());
    enter(table.view());
    enter(table.view(), false);
    EXPECT_EQ(selectionText(table.view()), "rows [] lead -1 column <none>");
}

// ------------------------------------------------------------------------------ the selection

TEST_F(SimulationTableViewTest, SelectsSimulationsByIdentity)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    SimulationTableView&              view   = table->view();
    const std::shared_ptr<Simulation> second = table->model().simulation(1);
    const std::shared_ptr<Simulation> fifth  = table->model().simulation(4);
    EXPECT_EQ(view.firstSelectedSimulation(), nullptr);
    EXPECT_TRUE(view.selectedSimulations().empty());
    // The last one given is the current row; the selection in the table's order.
    view.selectSimulations({fifth, second});
    EXPECT_EQ(view.selectedSimulations(),
              (std::vector<std::shared_ptr<Simulation>>{second, fifth}));
    EXPECT_EQ(view.firstSelectedSimulation(), second);
    EXPECT_EQ(view.currentIndex().row(), 1);
    EXPECT_EQ(view.currentIndex().column(), SimulationColumns::indexOf(Column::STATUS));
    // A simulation of no row is left out; the current cell's column stays.
    view.setCurrentIndex(table->model().index(1, SimulationColumns::indexOf(Column::APOGEE)));
    const std::unique_ptr<OpenRocketDocument> other =
        Gui::Test::loadSimulationExample("A simple model rocket.ork");
    ASSERT_NE(other, nullptr);
    view.selectSimulations({other->getSimulation(0), fifth, nullptr});
    EXPECT_EQ(view.selectedSimulations(), (std::vector<std::shared_ptr<Simulation>>{fifth}));
    EXPECT_EQ(view.currentIndex(),
              table->model().index(4, SimulationColumns::indexOf(Column::APOGEE)));
    // None: nothing selected.
    view.selectSimulations({});
    EXPECT_TRUE(view.selectedSimulations().empty());
}

TEST_F(SimulationTableViewTest, KeepsTheSelectionOnItsSimulationsAcrossASortAndAnUndo)
{
    // A header click keeps the selection on its simulations, as JTable does. OpenRocket bug 28,
    // fixed: after an edit that reorders the rows (below) OpenRocket selects the same view rows
    // again.
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    table->show();
    clickCell(*table, 0, Column::NAME);
    clickCell(*table, 3, Column::NAME, Qt::ControlModifier);
    const std::vector<std::shared_ptr<Simulation>> selected = table->view().selectedSimulations();
    ASSERT_EQ(names(selected), (std::vector<std::string>{"Simulation 1", "Simulation 4"}));
    // Apogee descending: the rows 4, 3, 2, 1, 0.
    clickHeader(*table, Column::APOGEE);
    clickHeader(*table, Column::APOGEE);
    EXPECT_EQ(table->rowNames().front(), "Simulation 5");
    EXPECT_EQ(names(table->view().selectedSimulations()),
              (std::vector<std::string>{"Simulation 4", "Simulation 1"}));
    EXPECT_EQ(table->view().currentIndex().row(), table->model().rowOf(*selected.at(1)));

    // Sorted by status: an edit makes the fourth OUTDATED and moves it last; its undo brings it
    // back (the same simulations, loaded from the undo history).
    table->model().setSortKeys({SortKey{.column = SimulationColumns::indexOf(Column::STATUS),
                                        .order  = SortOrder::ASCENDING}});
    OpenRocketDocument& document = table->document();
    document.addUndoPosition("Launch rod length");
    selected.at(1)->getOptions().setLaunchRodLength(
        selected.at(1)->getOptions().getLaunchRodLength() + 0.5);
    EXPECT_EQ(table->rowNames().back(), "Simulation 4");
    EXPECT_EQ(table->model().status(4), Simulation::Status::OUTDATED);
    EXPECT_EQ(table->view().selectedSimulations(), selected);
    document.undo();
    EXPECT_EQ(table->model().status(table->model().rowOf(*selected.at(1))),
              Simulation::Status::LOADED);
    EXPECT_EQ(table->rowNames().at(3), "Simulation 4");
    EXPECT_EQ(table->view().selectedSimulations(), selected);
    EXPECT_EQ(table->view().currentIndex().row(), 3);
}

TEST_F(SimulationTableViewTest, SelectsTheSameRowsAfterAnUndoThatReplacesTheSimulations)
{
    // An undo or a redo that changes the number of simulations loads copies of all of them: the
    // rows selected before are selected again, as OpenRocket's fireMaintainSelection() does.
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    table->show();
    OpenRocketDocument& document = table->document();
    clickCell(*table, 1, Column::NAME);
    clickCell(*table, 2, Column::NAME, Qt::ShiftModifier);
    document.addUndoPosition("Delete Simulation 4");
    document.removeSimulation(*document.getSimulation(3));
    // The selected simulations are still there.
    EXPECT_EQ(table->selectedNames(),
              (std::vector<std::string>{"Simulation 2", "Simulation 3 - too short delay"}));
    const std::vector<std::shared_ptr<Simulation>> before = table->view().selectedSimulations();
    document.undo();
    ASSERT_EQ(document.getSimulationCount(), 5U);
    EXPECT_EQ(table->selectedNames(),
              (std::vector<std::string>{"Simulation 2", "Simulation 3 - too short delay"}));
    EXPECT_NE(table->view().selectedSimulations(), before);
    EXPECT_EQ(table->view().currentIndex().row(), 2);
    EXPECT_EQ(table->view().currentIndex().column(), SimulationColumns::indexOf(Column::NAME));
    document.redo();
    ASSERT_EQ(document.getSimulationCount(), 4U);
    EXPECT_EQ(table->selectedNames(),
              (std::vector<std::string>{"Simulation 2", "Simulation 3 - too short delay"}));
    // A deletion of a selected simulation takes its selection with it: others stay.
    clickCell(*table, 3, Column::NAME);
    document.addUndoPosition("Delete Simulation 5");
    document.removeSimulation(*document.getSimulation(3));
    EXPECT_TRUE(table->selectedNames().empty());
}

TEST_F(SimulationTableViewTest, TellsOfEveryChangeOfTheSelectedSimulations)
{
    // Java's addSimulationTableListSelectionListener() hears of every new selection, also of the
    // one fireMaintainSelection() makes after a change of the document; the selection model tells
    // of no row a layout change of the model takes away, the view does.
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    table->show();
    OpenRocketDocument&        document = table->document();
    const SimulationTableView& view     = table->view();
    QSignalSpy                 told(&view, &SimulationTableView::selectedSimulationsChanged);
    QSignalSpy selections(view.selectionModel(), &QItemSelectionModel::selectionChanged);
    clickCell(*table, 1, Column::NAME);
    clickCell(*table, 2, Column::NAME, Qt::ControlModifier);
    EXPECT_EQ(told.count(), 2);
    EXPECT_EQ(told.count(), selections.count());

    // A new order of the same simulations: nothing more than the selection model tells.
    told.clear();
    selections.clear();
    clickHeader(*table, Column::APOGEE);
    clickHeader(*table, Column::APOGEE);
    table->model().setSortKeys({});
    EXPECT_EQ(told.count(), selections.count());
    EXPECT_EQ(table->selectedNames(),
              (std::vector<std::string>{"Simulation 2", "Simulation 3 - too short delay"}));

    // A selected simulation leaves the document: told once.
    told.clear();
    selections.clear();
    document.removeSimulation(*document.getSimulation(2));
    EXPECT_EQ(told.count(), 1);
    EXPECT_EQ(table->selectedNames(), (std::vector<std::string>{"Simulation 2"}));
    // Another one leaves: the selection stays as it is.
    told.clear();
    document.removeSimulation(*document.getSimulation(3));
    EXPECT_EQ(told.count(), 0);
    EXPECT_EQ(table->selectedNames(), (std::vector<std::string>{"Simulation 2"}));

    // An undo that replaces every simulation with copies, whose selected row is no more: the
    // view selects nothing again, and tells.
    const auto added = std::make_shared<Simulation>(&document, document.getRocket());
    added->setName(document.getNextSimulationName());
    document.addUndoPosition("Add " + added->getName());
    document.addSimulation(added);
    table->view().selectSimulations({added});
    ASSERT_EQ(table->selectedNames(), (std::vector<std::string>{added->getName()}));
    told.clear();
    document.undo();
    ASSERT_EQ(document.getSimulationCount(), 3U);
    EXPECT_TRUE(table->selectedNames().empty());
    EXPECT_EQ(told.count(), 1);
}

// ------------------------------------------------------------------------------ the pins

class SimulationTableViewGolden : public Gui::Test::GuiTest,
                                  public ::testing::WithParamInterface<std::string>
{ };

/// The design of @p pin, loaded as the probe loaded it, in a table.
[[nodiscard]] std::unique_ptr<Table> tableOfPin(const Gui::Test::SimulationTablePinDesign& pin)
{
    if (pin.kind == "new")
    {
        return std::make_unique<Table>(OpenRocketDocumentFactory::createNewRocket());
    }
    std::unique_ptr<OpenRocketDocument> document = Gui::Test::loadSimulationExample(pin.file);
    return document == nullptr ? nullptr : std::make_unique<Table>(std::move(document));
}

/// The rows of @p table as the simulations' indices in its document.
[[nodiscard]] std::vector<int> documentOrder(const Table& table)
{
    std::vector<int> order;
    for (const std::shared_ptr<Simulation>& simulation : table.view().tableModel().simulations())
    {
        const std::optional<std::size_t> index = table.document().getSimulationIndex(*simulation);
        order.push_back(index.has_value() ? static_cast<int>(*index) : -1);
    }
    return order;
}

/// A click and a second click on the header of each column of @p pin give its orders.
void expectHeaderOrders(Table& table, const Gui::Test::SimulationTablePinDesign& pin)
{
    for (const Column column : SimulationColumns::kAllColumns)
    {
        const std::string id(SimulationColumns::id(column));
        SCOPED_TRACE(testing::Message() << "column " << id);
        const Gui::Test::SimulationTablePinOrder& order = pin.order(id);
        table.model().setSortKeys({});
        clickHeader(table, column);
        EXPECT_EQ(documentOrder(table), order.ascending);
        clickHeader(table, column);
        EXPECT_EQ(documentOrder(table), order.descending);
    }
}

TEST_P(SimulationTableViewGolden, SortsEachColumnByItsHeaderAsOpenRocket)
{
    const Gui::Test::SimulationTablePins* pins = Gui::Test::simulationTablePinsOrFail();
    ASSERT_NE(pins, nullptr);
    const Gui::Test::SimulationTablePinDesign* pin = pins->findDesign(GetParam());
    ASSERT_NE(pin, nullptr);
    const std::unique_ptr<Table> table = tableOfPin(*pin);
    ASSERT_NE(table, nullptr);
    // The columns in the reverse order: each header sorts by its own column.
    for (int visual = 1; visual < SimulationColumns::kColumnCount; ++visual)
    {
        table->view().horizontalHeader()->moveSection(visual, 0);
    }
    ASSERT_EQ(table->view().visibleColumns().front(), Column::GROUND_HIT_VELOCITY);
    table->show();
    expectHeaderOrders(*table, *pin);
}

INSTANTIATE_TEST_SUITE_P(Designs, SimulationTableViewGolden,
                         ::testing::ValuesIn(Gui::Test::simulationTablePinnedDesigns()),
                         Gui::Test::simulationTablePinTestName);

}  // namespace

}  // namespace QtRocket::Gui
