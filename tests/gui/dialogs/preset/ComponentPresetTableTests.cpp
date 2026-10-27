// The parts library's table (Java: ComponentPresetTable with XTableColumnModel, and what
// ComponentPresetChooserDialog sets up on it, swing gui/dialogs/preset/). OpenRocket has no JUnit
// test of either.
//
// Expectations marked "probe" are OpenRocket's own results, printed by
// tools/openrocket-gui-pins/preset-chooser/probe/ComponentPresetTableProbe.java (run on OpenRocket
// 5f164fd0e with JDK 17; tools/openrocket-gui-pins/preset-chooser/hand-pinned.txt keeps its
// transcript): ComponentPresetTable over the presets of smallPresetDatabase(), its header menu made
// for a column (ColumnPopupMenu) and its items clicked. The probe's table is OpenRocket's bare
// table, whose Legacy column is shown; where a test compares with it, it shows Legacy first.

#include "QtRocket/gui/dialogs/preset/ComponentPresetTable.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

#include <QAbstractItemView>
#include <QApplication>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMenu>
#include <QModelIndex>
#include <QObject>
#include <QPoint>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QTimer>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "PresetTableTestSupport.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetFilterModel.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetTableModel.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/TableSortKeys.h"
#include "QtRocket/gui/widgets/TableRowTraversal.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetFavorites.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/util/BugError.h"
#include "rocket/preset/TestPresetDatabase.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using ComponentPresetTableTest = Test::GuiTest;
using Model                    = ComponentPresetTableModel;
using Type                     = ComponentPresetType;
using Test::OffscreenPopupWarnings;
using Test::presetMenuItem;
using Test::presetMenuItems;
using Test::presetSortKeys;
using Test::presetViewColumns;
using Test::presetViewRows;
using Test::showActiveWith;

/// A table of the small database's presets of @p type (never shown).
struct SmallPresetTable
{
    InMemoryPreferences  preferences;
    ComponentPresetTable table;

    explicit SmallPresetTable(Type type)
      : table(type, QtRocket::Test::smallPresetDatabase().listForType(type), preferences, nullptr)
    {
    }

    [[nodiscard]] std::string rows() const { return presetViewRows(table.filterModel()); }
    [[nodiscard]] std::string keys() const { return presetSortKeys(table.filterModel()); }

    /// The header menu of @p column as the probe writes it.
    [[nodiscard]] std::string menu(int column)
    {
        QMenu menu;
        table.populateHeaderMenu(menu, column);
        return presetMenuItems(menu);
    }

    /// Clicks the item @p text of @p column's header menu.
    void click(int column, const QString& text)
    {
        QMenu menu;
        table.populateHeaderMenu(menu, column);
        QAction* action = presetMenuItem(menu, text);
        ASSERT_NE(action, nullptr) << QtStrings::toUtf8(text);
        action->trigger();
    }

    /// Clicks @p unit in the Units menu of @p column's header menu.
    void chooseUnit(int column, const QString& unit)
    {
        QMenu menu;
        table.populateHeaderMenu(menu, column);
        const QAction* units = presetMenuItem(menu, QStringLiteral("Units"));
        ASSERT_NE(units, nullptr);
        ASSERT_NE(units->menu(), nullptr);
        QAction* action = presetMenuItem(*units->menu(), unit);
        ASSERT_NE(action, nullptr) << QtStrings::toUtf8(unit);
        action->trigger();
    }

    /// Selects the view row @p row.
    void select(int row)
    {
        table.selectionModel()->setCurrentIndex(
            table.filterModel().index(row, 0),
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
};

/// What the Sort items of the header menu of the model column @p column give, as the probe's
/// "fixed:" lines write it: "<keys> <rows>; <keys> <rows>" after Sort Ascending and after Sort
/// Descending.
[[nodiscard]] std::string sortItemsOf(int column)
{
    SmallPresetTable tubes(Type::BODY_TUBE);
    tubes.click(column, QStringLiteral("Sort Ascending"));
    const std::string ascending = tubes.keys() + " " + tubes.rows();
    tubes.click(column, QStringLiteral("Sort Descending"));
    return ascending + "; " + tubes.keys() + " " + tubes.rows();
}

TEST_F(ComponentPresetTableTest, ATableShowsItsTypesColumnsWithoutLegacy)
{
    SmallPresetTable tubes(Type::BODY_TUBE);
    // probe: "view \u2026 Favorite [0] | Manufacturer [2] | Part Number [3] | Description [4] |
    // Outer Diameter [5] | Inner Diameter [8] | Length [9]" (Legacy hidden as the dialog hides it).
    EXPECT_EQ(presetViewColumns(tubes.table),
              "\u2026 Favorite [0] | Manufacturer [2] | Part Number [3] | Description [4] | Outer "
              "Diameter [5] | Inner Diameter [8] | Length [9]");
    EXPECT_FALSE(tubes.table.isLegacyShown());
    EXPECT_TRUE(tubes.table.filterModel().hidesLegacy());
    EXPECT_TRUE(tubes.table.isTableColumn(1));
    EXPECT_FALSE(tubes.table.isColumnVisible(1));
    EXPECT_FALSE(tubes.table.isTableColumn(6));
    EXPECT_FALSE(tubes.table.isColumnVisible(6));
    EXPECT_EQ(tubes.table.viewColumn(5), 4);
    EXPECT_EQ(tubes.table.viewColumn(1), -1);
    EXPECT_THROW(tubes.table.setColumnVisible(6, true), BugError);
    EXPECT_THROW(static_cast<void>(tubes.table.isTableColumn(31)), BugError);
    // The identifiers TableUiPreferences stores the columns under: the English headers of the
    // table's columns.
    const std::span<const std::optional<std::string>> identifiers = tubes.table.columnIdentifiers();
    ASSERT_EQ(identifiers.size(), 31U);
    EXPECT_EQ(identifiers[0], std::optional<std::string>("\u2026 Favorite"));
    EXPECT_EQ(identifiers[1], std::optional<std::string>("Legacy"));
    EXPECT_EQ(identifiers[8], std::optional<std::string>("Inner Diameter"));
    EXPECT_EQ(identifiers[6], std::nullopt);
    EXPECT_EQ(identifiers[30], std::nullopt);
    // One whole row selected at most; no row header; columns that move; the favourite fixed at
    // the row height and 4 pixels; the sort indicator on Manufacturer.
    EXPECT_EQ(tubes.table.selectionMode(), QAbstractItemView::SingleSelection);
    EXPECT_EQ(tubes.table.selectionBehavior(), QAbstractItemView::SelectRows);
    EXPECT_TRUE(tubes.table.verticalHeader()->isHidden());
    const QHeaderView& header = *tubes.table.horizontalHeader();
    EXPECT_TRUE(header.sectionsMovable());
    EXPECT_EQ(header.sectionResizeMode(0), QHeaderView::Fixed);
    EXPECT_EQ(header.sectionSize(0), tubes.table.verticalHeader()->defaultSectionSize() + 4);
    EXPECT_TRUE(header.isSortIndicatorShown());
    EXPECT_EQ(header.sortIndicatorSection(), 2);
    EXPECT_EQ(header.sortIndicatorOrder(), Qt::AscendingOrder);
    EXPECT_FALSE(tubes.table.isSortingEnabled());
}

TEST_F(ComponentPresetTableTest, AHeaderClickTogglesItsColumnsSortKey)
{
    const SmallPresetTable tubes(Type::BODY_TUBE);
    QHeaderView&           header = *tubes.table.horizontalHeader();
    // probe ("== sorting"): "toggle 9: [9 asc]: 2 3 1 5 0 8 4 7 11 10 6 9", then "[9 desc]".
    Q_EMIT header.sectionClicked(9);
    EXPECT_EQ(tubes.keys(), "[9 asc, 2 asc]");
    EXPECT_EQ(tubes.rows(), "2 3 1 5 0 8 4 7 11 10 6 9");
    EXPECT_EQ(header.sortIndicatorSection(), 9);
    EXPECT_EQ(header.sortIndicatorOrder(), Qt::AscendingOrder);
    Q_EMIT header.sectionClicked(9);
    EXPECT_EQ(tubes.keys(), "[9 desc, 2 asc]");
    EXPECT_EQ(tubes.rows(), "9 6 10 11 4 7 0 8 5 1 3 2");
    EXPECT_EQ(header.sortIndicatorOrder(), Qt::DescendingOrder);
}

/// A table of the small database's body tubes in a window shown offscreen.
struct ShownPresetTable
{
    InMemoryPreferences   preferences;
    QWidget               window;
    ComponentPresetTable* table = makeChild<ComponentPresetTable>(
        &window, Type::BODY_TUBE,
        QtRocket::Test::smallPresetDatabase().listForType(Type::BODY_TUBE), preferences);

    /// A point inside the header section of the model column @p column.
    [[nodiscard]] QPoint inSection(int column) const
    {
        const QHeaderView& header = *table->horizontalHeader();
        return {header.sectionViewportPosition(column) + (header.sectionSize(column) / 2),
                header.viewport()->height() / 2};
    }
};

/// Closes the popup menu that is open (the header menu exec() shows) and gives its items.
struct PopupMenuReader
{
    std::string* items;

    void operator()() const
    {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (menu == nullptr)
        {
            for (QWidget* widget : QApplication::topLevelWidgets())
            {
                if (auto* candidate = qobject_cast<QMenu*>(widget);
                    candidate != nullptr && candidate->isVisible())
                {
                    menu = candidate;
                }
            }
        }
        if (menu != nullptr)
        {
            *items = presetMenuItems(*menu);
            menu->close();
        }
    }
};

TEST_F(ComponentPresetTableTest, AClickOnAHeaderTogglesItsSortKey)
{
    ShownPresetTable shown;
    ASSERT_TRUE(showActiveWith(shown.window, {shown.table}));
    const QHeaderView& header = *shown.table->horizontalHeader();
    QTest::mouseClick(header.viewport(), Qt::LeftButton, Qt::NoModifier, shown.inSection(9));
    EXPECT_EQ(presetSortKeys(shown.table->filterModel()), "[9 asc, 2 asc]");
    EXPECT_EQ(header.sortIndicatorSection(), 9);
    EXPECT_EQ(header.sortIndicatorOrder(), Qt::AscendingOrder);
    QTest::mouseClick(header.viewport(), Qt::LeftButton, Qt::NoModifier, shown.inSection(9));
    EXPECT_EQ(presetSortKeys(shown.table->filterModel()), "[9 desc, 2 asc]");
    EXPECT_EQ(header.sortIndicatorOrder(), Qt::DescendingOrder);
    // A double click (press, release, double click, release) toggles once, as JTable's, which
    // sorts on odd click counts.
    QTest::mousePress(header.viewport(), Qt::LeftButton, Qt::NoModifier, shown.inSection(5));
    QTest::mouseRelease(header.viewport(), Qt::LeftButton, Qt::NoModifier, shown.inSection(5));
    QTest::mouseDClick(header.viewport(), Qt::LeftButton, Qt::NoModifier, shown.inSection(5));
    QTest::mouseRelease(header.viewport(), Qt::LeftButton, Qt::NoModifier, shown.inSection(5));
    EXPECT_EQ(presetSortKeys(shown.table->filterModel()), "[5 asc, 9 desc, 2 asc]");
}

TEST_F(ComponentPresetTableTest, ARightClickOnTheHeaderShowsItsMenu)
{
    ShownPresetTable shown;
    ASSERT_TRUE(showActiveWith(shown.window, {shown.table}));
    QHeaderView& header = *shown.table->horizontalHeader();
    ASSERT_EQ(header.contextMenuPolicy(), Qt::CustomContextMenu);
    const OffscreenPopupWarnings warnings;
    std::string                  items;
    QTimer::singleShot(0, shown.table, PopupMenuReader{&items});
    Q_EMIT header.customContextMenuRequested(shown.inSection(5));
    EXPECT_EQ(items,
              "[ ]Sort Ascending [ ]Sort Descending | [Units: mm *cm m in in/64 ft] | "
              "[x]\u2026 Favorite [ ]Legacy [x]Manufacturer [x]Part Number [x]Description "
              "[x]Outer Diameter [x]Inner Diameter [x]Length");
    // Beside the columns: the column items only.
    std::string beside;
    QTimer::singleShot(0, shown.table, PopupMenuReader{&beside});
    Q_EMIT header.customContextMenuRequested(
        QPoint(header.length() + 10, header.viewport()->height() / 2));
    EXPECT_EQ(beside,
              "[x]\u2026 Favorite [ ]Legacy [x]Manufacturer [x]Part Number "
              "[x]Description [x]Outer Diameter [x]Inner Diameter [x]Length");
}

TEST_F(ComponentPresetTableTest, TheHeaderMenuSortsByTheClickedColumn)
{
    // probe ("== header menu", Legacy hidden as the dialog hides it): the items of each column's
    // menu. Its Sort items sort by the column's model index ("fixed:"); OpenRocket's take the
    // view index as one (bug 13): with Legacy hidden, "Sort Ascending" on Manufacturer (view 1)
    // sorts by Legacy ("[1 asc]") and on Outer Diameter (view 4) by Description ("[4 asc]").
    SmallPresetTable  tubes(Type::BODY_TUBE);
    const std::string columns =
        "[x]\u2026 Favorite [ ]Legacy [x]Manufacturer [x]Part Number "
        "[x]Description [x]Outer Diameter [x]Inner Diameter [x]Length";
    EXPECT_EQ(tubes.menu(-1), columns);
    EXPECT_EQ(tubes.menu(0), "[ ]Sort Ascending [ ]Sort Descending | " + columns);
    EXPECT_EQ(tubes.menu(2), "[ ]Sort Ascending [ ]Sort Descending | " + columns);
    EXPECT_EQ(tubes.menu(5),
              "[ ]Sort Ascending [ ]Sort Descending | [Units: mm *cm m in in/64 ft] | " + columns);
    EXPECT_EQ(tubes.menu(8),
              "[ ]Sort Ascending [ ]Sort Descending | [Units: mm *cm m in in/64 ft] | " + columns);
    // probe: the "fixed:" lines.
    EXPECT_EQ(sortItemsOf(0),
              "[0 asc] 0 1 2 3 4 5 6 7 8 9 10 11; [0 desc] 0 1 2 3 4 5 6 7 8 9 10 11");
    EXPECT_EQ(sortItemsOf(2),
              "[2 asc] 0 1 2 3 4 5 6 7 8 9 10 11; [2 desc] 0 1 2 3 4 5 6 7 8 9 10 11");
    EXPECT_EQ(sortItemsOf(3),
              "[3 asc] 0 1 2 3 4 5 6 7 8 9 10 11; [3 desc] 11 10 9 8 7 6 5 4 3 2 1 0");
    EXPECT_EQ(sortItemsOf(4),
              "[4 asc] 8 0 7 4 3 11 2 1 5 6 9 10; [4 desc] 10 9 6 5 1 2 11 3 4 7 0 8");
    EXPECT_EQ(sortItemsOf(5),
              "[5 asc] 11 1 2 0 8 3 4 7 5 6 9 10; [5 desc] 10 5 6 9 3 4 7 0 8 2 1 11");
    EXPECT_EQ(sortItemsOf(8),
              "[8 asc] 11 1 2 0 8 3 4 7 5 6 9 10; [8 desc] 10 5 6 9 3 4 7 0 8 2 1 11");
    EXPECT_EQ(sortItemsOf(9),
              "[9 asc] 2 3 1 5 0 8 4 7 11 10 6 9; [9 desc] 9 6 10 11 4 7 0 8 5 1 3 2");
    EXPECT_EQ(tubes.table.horizontalHeader()->sortIndicatorSection(), 2);
}

TEST_F(ComponentPresetTableTest, TheUnitsMenuListsTheGroupsUnits)
{
    // probe: "rail button, menu of Mass (view 10): ... [Units: *g kg oz lb] ...", "parachute,
    // menu of Sides (view 6)": no Units (an Integer column); OpenRocket's bare tables, Legacy
    // shown.
    SmallPresetTable rail(Type::RAIL_BUTTON);
    rail.table.setLegacyShown(true);
    EXPECT_EQ(rail.menu(26),
              "[ ]Sort Ascending [ ]Sort Descending | [Units: *g kg oz lb] | [x]\u2026 Favorite "
              "[x]Legacy [x]Manufacturer [x]Part Number [x]Description [x]Outer Diameter [x]Inner "
              "Diameter [x]Base Height [x]Flange Height [x]Screw Height [x]Mass [x]Screw Mass "
              "[x]Nut Mass");
    SmallPresetTable chutes(Type::PARACHUTE);
    chutes.table.setLegacyShown(true);
    EXPECT_EQ(chutes.menu(22),
              "[ ]Sort Ascending [ ]Sort Descending | [x]\u2026 Favorite [x]Legacy "
              "[x]Manufacturer [x]Part Number [x]Description [x]Diameter [x]Sides [x]Line Count "
              "[x]Line Length [x]Line Material [x]Material");
}

TEST_F(ComponentPresetTableTest, ChoosingAUnitShowsTheColumnInIt)
{
    // probe ("== header menu: units"): "mm chosen: 50.8 mm | 30.7 mm | ...", "menu again: ...
    // [Units: *mm cm m in in/64 ft] ...", "a new table: 5.08 cm". OpenRocket's selection is gone
    // after it ("selected row after: -1"); here the selected row stays.
    SmallPresetTable tubes(Type::BODY_TUBE);
    tubes.table.setLegacyShown(true);
    tubes.select(3);
    tubes.chooseUnit(5, QStringLiteral("mm"));
    const Model& model = tubes.table.tableModel();
    EXPECT_EQ(model.cellText(0, 5), QStringLiteral("50.8 mm"));
    EXPECT_EQ(model.cellText(10, 5), QStringLiteral("102 mm"));
    EXPECT_EQ(model.cellText(11, 5), QStringLiteral("18.7 mm"));
    EXPECT_EQ(tubes.menu(5),
              "[ ]Sort Ascending [ ]Sort Descending | [Units: *mm cm m in in/64 ft] "
              "| [x]\u2026 Favorite [x]Legacy [x]Manufacturer [x]Part Number "
              "[x]Description [x]Outer Diameter [x]Inner Diameter [x]Length");
    EXPECT_EQ(tubes.table.selectedPreset(), model.preset(3));
    const SmallPresetTable fresh(Type::BODY_TUBE);
    EXPECT_EQ(fresh.table.tableModel().cellText(0, 5), QStringLiteral("5.08 cm"));
}

TEST_F(ComponentPresetTableTest, TheColumnItemsHideAndShowColumnsInTheirPlace)
{
    // probe ("== header menu: columns", OpenRocket's bare table: Legacy shown).
    SmallPresetTable tubes(Type::BODY_TUBE);
    tubes.table.setLegacyShown(true);
    tubes.click(3, QStringLiteral("Manufacturer"));
    EXPECT_EQ(presetViewColumns(tubes.table),
              "\u2026 Favorite [0] | Legacy [1] | Part Number [3] | Description [4] | Outer "
              "Diameter [5] | Inner Diameter [8] | Length [9]");
    QHeaderView& header = *tubes.table.horizontalHeader();
    header.moveSection(header.visualIndex(9), header.visualIndex(0));
    EXPECT_EQ(presetViewColumns(tubes.table),
              "Length [9] | \u2026 Favorite [0] | Legacy [1] | Part Number [3] | Description [4] | "
              "Outer Diameter [5] | Inner Diameter [8]");
    tubes.click(9, QStringLiteral("Manufacturer"));
    EXPECT_EQ(
        presetViewColumns(tubes.table),
        "Length [9] | \u2026 Favorite [0] | Legacy [1] | Manufacturer [2] | Part Number [3] | "
        "Description [4] | Outer Diameter [5] | Inner Diameter [8]");
    tubes.table.setColumnVisible(1, false);
    EXPECT_EQ(presetViewColumns(tubes.table),
              "Length [9] | \u2026 Favorite [0] | Manufacturer [2] | Part Number [3] | Description "
              "[4] | Outer Diameter [5] | Inner Diameter [8]");
    EXPECT_EQ(tubes.menu(2),
              "[ ]Sort Ascending [ ]Sort Descending | [x]\u2026 Favorite [ ]Legacy "
              "[x]Manufacturer [x]Part Number [x]Description [x]Outer Diameter "
              "[x]Inner Diameter [x]Length");
}

TEST_F(ComponentPresetTableTest, TheLegacyBoxShowsTheColumnAndTheRows)
{
    // probe ("== legacy filter"): the bulkheads "all 0 1; legacy hidden 0".
    SmallPresetTable bulkheads(Type::BULK_HEAD);
    const QSignalSpy shown(&bulkheads.table, &ComponentPresetTable::legacyShownChanged);
    EXPECT_EQ(bulkheads.rows(), "0");
    bulkheads.table.setLegacyShown(true);
    EXPECT_TRUE(bulkheads.table.isLegacyShown());
    EXPECT_TRUE(bulkheads.table.isColumnVisible(1));
    EXPECT_EQ(bulkheads.rows(), "0 1");
    // The width the box gives the column (getFilterCheckboxes(): 50).
    EXPECT_EQ(bulkheads.table.horizontalHeader()->sectionSize(1),
              ComponentPresetTable::kLegacyColumnWidth);
    bulkheads.table.setLegacyShown(true);
    bulkheads.table.setLegacyShown(false);
    EXPECT_FALSE(bulkheads.table.isColumnVisible(1));
    EXPECT_EQ(bulkheads.rows(), "0");
    ASSERT_EQ(shown.count(), 2);
    EXPECT_TRUE(shown.at(0).at(0).toBool());
    EXPECT_FALSE(shown.at(1).at(0).toBool());
}

TEST_F(ComponentPresetTableTest, TheHeaderMenusLegacyItemShowsTheRowsToo)
{
    // Bug 14 (fixed): OpenRocket's header item shows the column and ticks the box, but the rows
    // stay filtered (the M3 scouting's probe: "column shown: rows 1089 box true").
    SmallPresetTable bulkheads(Type::BULK_HEAD);
    const QSignalSpy shown(&bulkheads.table, &ComponentPresetTable::legacyShownChanged);
    bulkheads.click(-1, QStringLiteral("Legacy"));
    EXPECT_TRUE(bulkheads.table.isLegacyShown());
    EXPECT_TRUE(bulkheads.table.isColumnVisible(1));
    EXPECT_EQ(bulkheads.rows(), "0 1");
    bulkheads.click(-1, QStringLiteral("Legacy"));
    EXPECT_FALSE(bulkheads.table.isLegacyShown());
    EXPECT_EQ(bulkheads.rows(), "0");
    EXPECT_EQ(shown.count(), 2);
    // Hiding the column at view index 1 while Legacy is elsewhere leaves Legacy alone
    // (OpenRocket compares that index with Legacy's model index 1 and hides Legacy too).
    bulkheads.table.setLegacyShown(true);
    QHeaderView& header = *bulkheads.table.horizontalHeader();
    header.moveSection(header.visualIndex(1), header.count() - 1);
    ASSERT_EQ(bulkheads.table.viewColumn(2), 1);
    bulkheads.table.setColumnVisible(2, false);
    EXPECT_TRUE(bulkheads.table.isLegacyShown());
    EXPECT_TRUE(bulkheads.table.isColumnVisible(1));
    EXPECT_EQ(bulkheads.rows(), "0 1");
}

/// The keys of the node "tables" of @p preferences.
[[nodiscard]] std::vector<std::string> tableKeys(const Preferences& preferences)
{
    const Preferences* node = preferences.findNode(GuiPreferenceKeys::kNodeTables);
    return node == nullptr ? std::vector<std::string>{} : node->keys();
}

TEST_F(ComponentPresetTableTest, TheLayoutIsStoredUnderTheEnglishHeaders)
{
    // The M3 scouting's dialog after Close (body tube): 22 keys, "CmpPrst.Body Tube.co.Outer
    // Diameter = 4", ".cv.Legacy = false".
    SmallPresetTable  tubes(Type::BODY_TUBE);
    const std::string table = GuiPreferenceKeys::componentPresetTableName(ComponentKind::BODY_TUBE);
    tubes.table.loadUiPreferences(table);
    tubes.table.storeUiPreferences(table);
    EXPECT_EQ(tableKeys(tubes.preferences).size(), 22U);
    const Preferences* node = tubes.preferences.findNode(GuiPreferenceKeys::kNodeTables);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->getInt("CmpPrst.Body Tube.co.Outer Diameter", -1), 4);
    EXPECT_EQ(node->getInt("CmpPrst.Body Tube.co.\u2026 Favorite", -1), 0);
    EXPECT_EQ(node->get("CmpPrst.Body Tube.cv.Legacy"), std::optional<std::string>("false"));
    EXPECT_EQ(node->get("CmpPrst.Body Tube.cw.Legacy"), std::nullopt);
    EXPECT_EQ(node->getInt("CmpPrst.Body Tube.cw.\u2026 Favorite", -1),
              tubes.table.verticalHeader()->defaultSectionSize() + 4);
}

TEST_F(ComponentPresetTableTest, LoadingTheLayoutHidesLegacyAgain)
{
    // The dialog's constructor: the stored layout may show the Legacy column (as the first open
    // does, every column shown by default); the box's state of before comes back, unticked.
    InMemoryPreferences preferences;
    const std::string name = GuiPreferenceKeys::componentPresetTableName(ComponentKind::BODY_TUBE);
    const ComponentPresetDatabase database = QtRocket::Test::smallPresetDatabase();
    {
        ComponentPresetTable table(Type::BODY_TUBE, database.listForType(Type::BODY_TUBE),
                                   preferences, nullptr);
        table.setLegacyShown(true);
        QHeaderView& header = *table.horizontalHeader();
        header.moveSection(header.visualIndex(1), header.visualIndex(9));
        table.storeUiPreferences(name);
    }
    const Preferences* node = preferences.findNode(GuiPreferenceKeys::kNodeTables);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->get("CmpPrst.Body Tube.cv.Legacy"), std::optional<std::string>("true"));
    EXPECT_EQ(node->getInt("CmpPrst.Body Tube.co.Legacy", -1), 7);

    ComponentPresetTable table(Type::BODY_TUBE, database.listForType(Type::BODY_TUBE), preferences,
                               nullptr);
    const QSignalSpy     shown(&table, &ComponentPresetTable::legacyShownChanged);
    table.loadUiPreferences(name);
    EXPECT_FALSE(table.isLegacyShown());
    EXPECT_FALSE(table.isColumnVisible(1));
    EXPECT_TRUE(table.filterModel().hidesLegacy());
    EXPECT_EQ(shown.count(), 0);
    // The order was restored with Legacy shown: shown again, it is last.
    table.setLegacyShown(true);
    EXPECT_EQ(table.visibleColumns(), (std::vector<int>{0, 2, 3, 4, 5, 8, 9, 1}));
    EXPECT_EQ(table.horizontalHeader()->sectionSize(0),
              table.verticalHeader()->defaultSectionSize() + 4);
}

/// The view's selected row as the probe's selected() writes it: "<row> (model <row>)", or "-1".
[[nodiscard]] std::string selectedRowText(const SmallPresetTable& tubes)
{
    const int row = TableRowTraversal::selectedRow(tubes.table);
    return row < 0 ? "-1"
                   : std::to_string(row) + " (model " +
                         std::to_string(tubes.table.filterModel().modelRow(row)) + ")";
}

/// The probe's texts, each followed by the selected row.
[[nodiscard]] std::string selectionThroughTexts(const SmallPresetTable& tubes)
{
    std::string line;
    for (const char* text : {"B", "BT", "BT-20", "BT", "B", ""})
    {
        tubes.table.filterModel().setTextFilter(QString::fromLatin1(text));
        line += std::string(" '") + text + "' " + selectedRowText(tubes) + ";";
    }
    return line;
}

TEST_F(ComponentPresetTableTest, TheSelectedPresetIsTheSelectedRows)
{
    SmallPresetTable tubes(Type::BODY_TUBE);
    EXPECT_EQ(tubes.table.selectedPreset(), nullptr);
    tubes.table.filterModel().toggleSortOrder(9);
    tubes.select(0);
    // probe: sorted by Length the first row is model row 2.
    EXPECT_EQ(tubes.table.selectedPreset(), tubes.table.tableModel().preset(2));
    // A filter that hides the row leaves nothing selected.
    tubes.table.filterModel().setTextFilter(QStringLiteral("BT-20"));
    EXPECT_EQ(tubes.table.selectedPreset(), nullptr);
}

TEST_F(ComponentPresetTableTest, TheSelectionStaysWithItsModelRowThroughTheFilters)
{
    // probe ("== the selection through the filters").
    SmallPresetTable tubes(Type::BODY_TUBE);
    tubes.select(2);
    EXPECT_EQ(selectionThroughTexts(tubes),
              " 'B' 2 (model 2); 'BT' -1; 'BT-20' -1; 'BT' -1; 'B' 2 "
              "(model 2); '' 2 (model 2);");
    // "hidden, the selection cleared, shown: 2 (model 2)".
    tubes.table.filterModel().setTextFilter(QStringLiteral("BT-20"));
    tubes.table.clearSelection();
    tubes.table.filterModel().setTextFilter(QString());
    EXPECT_EQ(selectedRowText(tubes), "2 (model 2)");
    // "hidden, sorted by Length, shown: 0 (model 2)".
    tubes.table.filterModel().setTextFilter(QStringLiteral("BT-20"));
    tubes.table.filterModel().toggleSortOrder(9);
    tubes.table.filterModel().setTextFilter(QString());
    EXPECT_EQ(selectedRowText(tubes), "0 (model 2)");
    // "hidden, the shown row selected, the filter gone: 8 (model 11)".
    tubes.table.filterModel().setTextFilter(QStringLiteral("BT-20"));
    tubes.select(0);
    tubes.table.filterModel().setTextFilter(QString());
    EXPECT_EQ(selectedRowText(tubes), "8 (model 11)");
    // "that row unselected, hidden, shown: -1".
    tubes.table.clearSelection();
    tubes.table.filterModel().setTextFilter(QStringLiteral("BT-20"));
    tubes.table.filterModel().setTextFilter(QString());
    EXPECT_EQ(selectedRowText(tubes), "-1");
    // The legacy filter is one of the filters: a legacy row comes back with Legacy.
    SmallPresetTable streamers(Type::STREAMER);
    streamers.table.setLegacyShown(true);
    streamers.select(0);
    ASSERT_EQ(streamers.table.selectedPreset(), streamers.table.tableModel().preset(0));
    streamers.table.setLegacyShown(false);
    EXPECT_EQ(streamers.table.selectedPreset(), nullptr);
    streamers.table.setLegacyShown(true);
    EXPECT_EQ(streamers.table.selectedPreset(), streamers.table.tableModel().preset(0));
}

TEST_F(ComponentPresetTableTest, ATickedFavouriteKeepsItsRowSelected)
{
    // probe ("== favourites"): "model row 5 ticked ... selected view row 5"; sorted by the
    // favourite descending: "5 0 1 2 3 4 6 7 8 9 10 11; selected view row 0 (model 5)".
    SmallPresetTable tubes(Type::BODY_TUBE);
    tubes.select(5);
    ComponentPresetFilterModel& filter = tubes.table.filterModel();
    EXPECT_TRUE(
        filter.setData(filter.index(5, 0), static_cast<int>(Qt::Checked), Qt::CheckStateRole));
    EXPECT_TRUE(ComponentPresetFavorites::isFavorite(
        tubes.preferences, *tubes.table.tableModel().preset(5), Type::BODY_TUBE));
    EXPECT_EQ(tubes.table.selectedPreset(), tubes.table.tableModel().preset(5));
    filter.setSortKeys(
        {TableSortKeys::SortKey{.column = 0, .order = TableSortKeys::SortOrder::DESCENDING}});
    EXPECT_EQ(tubes.rows(), "5 0 1 2 3 4 6 7 8 9 10 11");
    EXPECT_EQ(tubes.table.selectionModel()->selectedRows().size(), 1);
    EXPECT_EQ(tubes.table.selectionModel()->selectedRows().front().row(), 0);
    EXPECT_EQ(tubes.table.selectedPreset(), tubes.table.tableModel().preset(5));
}

TEST_F(ComponentPresetTableTest, TabMovesTheSelectionFromRowToRow)
{
    // TableRowTraversal is installed: Tab selects the next row, in the first shown column.
    SmallPresetTable tubes(Type::BODY_TUBE);
    QTest::keyClick(&tubes.table, Qt::Key_Tab);
    EXPECT_EQ(tubes.table.selectedPreset(), tubes.table.tableModel().preset(0));
    EXPECT_EQ(tubes.table.currentIndex().column(), 0);
    QTest::keyClick(&tubes.table, Qt::Key_Right);
    EXPECT_EQ(tubes.table.selectedPreset(), tubes.table.tableModel().preset(1));
}

TEST_F(ComponentPresetTableTest, SpaceTicksTheCurrentRowsFavourite)
{
    // A deviation: Qt's check box items take Space (OpenRocket: JTable's addToSelection; F2 and
    // Space tick the favourite there).
    SmallPresetTable tubes(Type::BODY_TUBE);
    QTest::keyClick(&tubes.table, Qt::Key_Tab);
    QTest::keyClick(&tubes.table, Qt::Key_Space);
    EXPECT_TRUE(tubes.table.tableModel().isFavorite(0));
    QTest::keyClick(&tubes.table, Qt::Key_Space);
    EXPECT_FALSE(tubes.table.tableModel().isFavorite(0));
}

}  // namespace

}  // namespace QtRocket::Gui
