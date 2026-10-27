// MotorMountConfigurationPanel: the check table of the motor mounts (OpenRocket's
// MotorMountConfigurationPanel with GUIUtil.BooleanTableClickListener): its look, and the clicks
// that toggle a mount through FlightConfigurationEditing, each with its own undo step (bug 5).

#include "QtRocket/gui/flightconfig/MotorMountConfigurationPanel.h"

#include <string>

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLayout>
#include <QMargins>
#include <QModelIndex>
#include <QPoint>
#include <QString>
#include <QTableView>
#include <QTest>
#include <QVariant>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/flightconfig/MotorConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/MotorMountTableModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/rocket/MotorMount.h"
#include "flightconfig/FlightConfigurationTabSupport.h"
#include "tree/TreeViewMouse.h"

namespace QtRocket::Gui
{

namespace
{

using MotorMountConfigurationPanelTest = Gui::Test::GuiTest;
using Tab                              = Gui::Test::FlightConfigurationTab;
using Design                           = Tab::Design;

/// The rows of @p panel's table, "[x] Body, [ ] Motor tube".
[[nodiscard]] std::string rows(const MotorMountConfigurationPanel& panel)
{
    const MotorMountTableModel& model = panel.model();
    std::string                 text;
    for (int row = 0; row < model.rowCount(); ++row)
    {
        const bool checked =
            model
                .data(model.index(row, MotorMountTableModel::kMotorMountColumn), Qt::CheckStateRole)
                .toInt() == static_cast<int>(Qt::Checked);
        text += (row > 0 ? ", " : "") + std::string(checked ? "[x] " : "[ ] ") +
                QtStrings::toUtf8(
                    model.data(model.index(row, MotorMountTableModel::kNameColumn)).toString());
    }
    return text;
}

/// The centre of the cell (@p row, @p column) of @p panel's table, in its viewport.
[[nodiscard]] QPoint center(const MotorMountConfigurationPanel& panel, int row, int column)
{
    return panel.table().visualRect(panel.model().index(row, column)).center();
}

TEST_F(MotorMountConfigurationPanelTest, ItIsATableWithoutHeaderGridOrSelection)
{
    const Tab                           tab(Design::TWO_STAGES);
    const MotorMountConfigurationPanel& panel = tab.motors().motorMountConfigurationPanel();
    const QTableView&                   table = panel.table();
    EXPECT_EQ(rows(panel), "[x] Body, [x] Motor tube, [x] Booster tube");
    EXPECT_EQ(table.objectName(), QStringLiteral("table"));
    EXPECT_TRUE(table.horizontalHeader()->isHidden());
    EXPECT_TRUE(table.verticalHeader()->isHidden());
    EXPECT_FALSE(table.showGrid());
    EXPECT_EQ(table.selectionMode(), QAbstractItemView::NoSelection);
    // The check box column: a row's height plus 2; the name the rest.
    EXPECT_EQ(table.columnWidth(MotorMountTableModel::kMotorMountColumn),
              table.verticalHeader()->defaultSectionSize() + 2);
    EXPECT_TRUE(table.horizontalHeader()->stretchLastSection());
    // "w 200lp, h 150lp".
    const QMargins margins = panel.layout()->contentsMargins();
    EXPECT_EQ(panel.sizeHint().width(), 200 + margins.left() + margins.right());
    EXPECT_EQ(panel.sizeHint().height(), 150 + margins.top() + margins.bottom());
}

TEST_F(MotorMountConfigurationPanelTest, AClickOnTheNameTogglesTheMountWithAnUndoStep)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    const MotorMountConfigurationPanel& panel = tab.motors().motorMountConfigurationPanel();
    QTest::mouseClick(panel.table().viewport(), Qt::LeftButton, {},
                      center(panel, 1, MotorMountTableModel::kNameColumn));
    EXPECT_EQ(rows(panel), "[x] Body, [ ] Motor tube, [x] Booster tube");
    EXPECT_FALSE(dynamic_cast<const MotorMount&>(tab.find("Motor tube")).isMotorMount());
    EXPECT_EQ(tab.document().getUndoDescription().value_or("-"), "Modify motor mount");
    EXPECT_EQ(tab.motors().tableModel().columnCount(), 3);

    // The check box's cell toggles once too.
    QTest::mouseClick(panel.table().viewport(), Qt::LeftButton, {},
                      center(panel, 1, MotorMountTableModel::kMotorMountColumn));
    EXPECT_EQ(rows(panel), "[x] Body, [x] Motor tube, [x] Booster tube");
    EXPECT_EQ(tab.motors().tableModel().columnCount(), 4);

    // The undo steps are the mount's own.
    tab.document().undo();
    EXPECT_EQ(rows(panel), "[x] Body, [ ] Motor tube, [x] Booster tube");
    tab.document().undo();
    EXPECT_EQ(rows(panel), "[x] Body, [x] Motor tube, [x] Booster tube");
}

TEST_F(MotorMountConfigurationPanelTest, EachClickOfADoubleClickTogglesAndTheRightButtonNone)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    const MotorMountConfigurationPanel& panel = tab.motors().motorMountConfigurationPanel();
    Gui::Test::doubleClickAt(*panel.table().viewport(),
                             center(panel, 2, MotorMountTableModel::kNameColumn));
    EXPECT_EQ(rows(panel), "[x] Body, [x] Motor tube, [x] Booster tube");
    EXPECT_EQ(tab.document().getUndoDescription().value_or("-"), "Modify motor mount");
    tab.document().undo();
    EXPECT_EQ(rows(panel), "[x] Body, [x] Motor tube, [ ] Booster tube");

    QTest::mouseClick(panel.table().viewport(), Qt::RightButton, {},
                      center(panel, 0, MotorMountTableModel::kNameColumn));
    EXPECT_EQ(rows(panel), "[x] Body, [x] Motor tube, [ ] Booster tube");
}

}  // namespace

}  // namespace QtRocket::Gui
