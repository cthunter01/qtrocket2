#include "QtRocket/gui/configdialog/FinPointTableModel.h"

#include <QAbstractItemModelTester>
#include <QModelIndex>
#include <QSignalSpy>
#include <QString>
#include <QVariant>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/unit/UnitGroup.h"
#include "figure/FinPointTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using FinPointTableModelTest = Test::GuiTest;
using Test::FreeformDesign;

TEST_F(FinPointTableModelTest, ShowsThePointsInTheDefaultUnit)
{
    const FreeformDesign     design;
    const FinPointTableModel model(design.selection(), nullptr);
    EXPECT_EQ(model.rowCount(), 4);
    EXPECT_EQ(model.columnCount(), 2);
    EXPECT_EQ(model.headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(),
              QStringLiteral("X / cm"));
    EXPECT_EQ(model.headerData(1, Qt::Horizontal, Qt::DisplayRole).toString(),
              QStringLiteral("Y / cm"));
    EXPECT_EQ(model.data(model.index(1, 0), Qt::DisplayRole).toString(), QStringLiteral("2.5"));
    EXPECT_EQ(model.data(model.index(1, 1), Qt::EditRole).toString(), QStringLiteral("5"));
    EXPECT_EQ(model.data(model.index(3, 0), Qt::DisplayRole).toString(), QStringLiteral("5"));
    EXPECT_FALSE(model.data(model.index(1, 0), Qt::ToolTipRole).isValid());

    const QtRocket::Test::DefaultUnitsGuard units;
    ASSERT_TRUE(unitGroup(UnitGroupId::LENGTH).setDefaultUnit("mm"));
    EXPECT_EQ(model.headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(),
              QStringLiteral("X / mm"));
    EXPECT_EQ(model.valueText(1, 0), QStringLiteral("25"));
}

TEST_F(FinPointTableModelTest, TheFirstAndTheLastPointTakeOnlyAnX)
{
    const FreeformDesign     design;
    const FinPointTableModel model(design.selection(), nullptr);
    EXPECT_TRUE(model.flags(model.index(0, 0)).testFlag(Qt::ItemIsEditable));
    EXPECT_FALSE(model.flags(model.index(0, 1)).testFlag(Qt::ItemIsEditable));
    EXPECT_TRUE(model.flags(model.index(1, 0)).testFlag(Qt::ItemIsEditable));
    EXPECT_TRUE(model.flags(model.index(2, 1)).testFlag(Qt::ItemIsEditable));
    EXPECT_TRUE(model.flags(model.index(3, 0)).testFlag(Qt::ItemIsEditable));
    EXPECT_FALSE(model.flags(model.index(3, 1)).testFlag(Qt::ItemIsEditable));
    EXPECT_TRUE(model.flags(model.index(0, 1)).testFlag(Qt::ItemIsSelectable));
}

TEST_F(FinPointTableModelTest, ParsesTheEditorsText)
{
    const FreeformDesign design;
    FinPointTableModel   model(design.selection(), nullptr);
    const QSignalSpy     entered(&model, &FinPointTableModel::pointEntered);
    EXPECT_TRUE(model.setData(model.index(1, 0), QStringLiteral("3"), Qt::EditRole));
    EXPECT_TRUE(model.setData(model.index(1, 1), QStringLiteral("45 mm"), Qt::EditRole));
    ASSERT_EQ(entered.count(), 2);
    EXPECT_EQ(entered.at(0).at(0).toInt(), 1);
    EXPECT_EQ(entered.at(0).at(1).toDouble(), 0.03);
    EXPECT_EQ(entered.at(0).at(2).toDouble(), 0.05);
    EXPECT_EQ(entered.at(1).at(1).toDouble(), 0.025);
    EXPECT_DOUBLE_EQ(entered.at(1).at(2).toDouble(), 0.045);

    // What does not parse, and a row the fin set lacks, are ignored.
    EXPECT_FALSE(model.setData(model.index(1, 0), QStringLiteral("abc"), Qt::EditRole));
    EXPECT_FALSE(model.setData(model.index(1, 0), QString(), Qt::EditRole));
    EXPECT_FALSE(model.setData(model.index(9, 0), QStringLiteral("1"), Qt::EditRole));
    EXPECT_FALSE(model.setData(model.index(1, 0), QStringLiteral("1"), Qt::DisplayRole));
    EXPECT_EQ(entered.count(), 2);
    // The model itself changes nothing.
    EXPECT_EQ(design.fins().getFinPoints().at(1).x, 0.025);
}

TEST_F(FinPointTableModelTest, FollowsThePointsThroughTheRocketsEvents)
{
    const FreeformDesign           design;
    FinPointTableModel             model(design.selection(), nullptr);
    const QAbstractItemModelTester tester(&model,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    const QSignalSpy               inserted(&model, &FinPointTableModel::rowsInserted);
    const QSignalSpy               removed(&model, &FinPointTableModel::rowsRemoved);
    ASSERT_TRUE(design.fins().addPoint(1, {0.01, 0.02}).has_value());
    EXPECT_EQ(model.rowCount(), 5);
    ASSERT_EQ(inserted.count(), 1);
    // A row added at the end, as Java's table, whose rows are its model's live count.
    EXPECT_EQ(inserted.at(0).at(1).toInt(), 4);
    EXPECT_EQ(model.valueText(1, 0), QStringLiteral("1"));

    design.rocket().freeze();
    ASSERT_TRUE(design.fins().removePoint(1).has_value());
    ASSERT_TRUE(design.fins().removePoint(1).has_value());
    EXPECT_EQ(model.rowCount(), 5);
    design.rocket().thaw();
    EXPECT_EQ(model.rowCount(), 3);
    EXPECT_EQ(removed.count(), 1);
}

TEST_F(FinPointTableModelTest, WritesTheCsvOfOpenRocketWithNewlines)
{
    // writeCSVFile(): every text followed by ", ", the lines ending with '\n' (Java: "\r\n").
    const FreeformDesign     design;
    const FinPointTableModel model(design.selection(), nullptr);
    EXPECT_EQ(model.csvText(),
              QStringLiteral("X / cm, Y / cm, \n0, 0, \n2.5, 5, \n7.5, 5, \n5, 0, \n"));
}

TEST_F(FinPointTableModelTest, AFinSetThatLeftTheRocketHasNoRows)
{
    const FreeformDesign design;
    FinPointTableModel   model(design.selection(), nullptr);
    const auto           removed = design.fins().getParent()->removeChild(&design.fins());
    EXPECT_EQ(model.rowCount(), 0);
    EXPECT_EQ(model.valueText(0, 0), QString());
    EXPECT_FALSE(model.setData(model.index(0, 0), QStringLiteral("1"), Qt::EditRole));
}

}  // namespace

}  // namespace QtRocket::Gui
