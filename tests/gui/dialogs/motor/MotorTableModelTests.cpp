// The motor chooser's table (Java: ThrustCurveMotorDatabaseModel behind the JTable and the
// TableRowSorter of ThrustCurveMotorSelectionPanel). OpenRocket has no JUnit test of the model; the
// rows, the order and the texts are the core's (ThrustCurveMotorSelection, pinned against
// OpenRocket in tests/core/motor/chooser), which these tests compare the model with, over the
// eleven sets of smallMotorDatabase() under QAbstractItemModelTester. motor_table_golden_tests.cpp
// compares the whole database's table with OpenRocket's.

#include "QtRocket/gui/dialogs/motor/MotorTableModel.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QAbstractItemModel>
#include <QAbstractItemModelTester>
#include <QAbstractItemView>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QPoint>
#include <QSignalSpy>
#include <QString>
#include <QTableView>
#include <QTest>
#include <QVariant>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "MotorChooserTestSupport.h"
#include "QtRocket/document/MotorChooserInput.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/motor/chooser/MotorRowFilter.h"
#include "QtRocket/motor/chooser/MotorTableSorter.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorColumns.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorDatabaseModel.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "motor/TestMotorDatabase.h"
#include "unit/DefaultUnitsGuard.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using MotorTableModelTest = Test::GuiTest;
using Test::BodyTubeMount;
using Test::showActiveWith;
using Test::sortKeysText;

/// The table of a chooser over smallMotorDatabase() (eleven sets), with fresh preferences, under
/// QAbstractItemModelTester.
class SmallTable
{
public:
    SmallTable()
      : m_database(QtRocket::Test::smallMotorDatabase()),
        m_selection(m_database, m_preferences),
        m_model(m_selection, m_preferences, nullptr),
        m_tester(&m_model, QAbstractItemModelTester::FailureReportingMode::Warning)
    {
    }

    [[nodiscard]] InMemoryPreferences&       preferences() noexcept { return m_preferences; }
    [[nodiscard]] ThrustCurveMotorSelection& selection() noexcept { return m_selection; }
    [[nodiscard]] MotorTableModel&           model() noexcept { return m_model; }

    /// The set of the database made by @p manufacturer (a display name) as @p designation.
    [[nodiscard]] const ThrustCurveMotorSet& set(const std::string& manufacturer,
                                                 const std::string& designation) const
    {
        for (const ThrustCurveMotorSet& candidate : m_database.getMotorSets())
        {
            if (candidate.getManufacturer()->getDisplayName() == manufacturer &&
                candidate.getDesignation() == designation)
            {
                return candidate;
            }
        }
        ADD_FAILURE() << "no set " << manufacturer << " " << designation;
        return m_database.getMotorSets().front();
    }

    /// The text of the view row @p row in the column @p column.
    [[nodiscard]] std::string text(int row, ThrustCurveMotorColumns column) const
    {
        return QtStrings::toUtf8(
            m_model.data(m_model.index(row, static_cast<int>(column)), Qt::DisplayRole).toString());
    }

private:
    InMemoryPreferences         m_preferences;
    ThrustCurveMotorSetDatabase m_database;
    ThrustCurveMotorSelection   m_selection;
    MotorTableModel             m_model;
    QAbstractItemModelTester    m_tester;
};

/// Where the model's rows and texts differ from what the core gives for @p selection ("" when
/// nowhere).
[[nodiscard]] std::string differencesFromTheCore(const MotorTableModel&           model,
                                                 const ThrustCurveMotorSelection& selection,
                                                 const InMemoryPreferences&       preferences)
{
    const std::vector<std::size_t> rows = selection.getViewRows();
    std::string                    differences;
    if (model.rowCount() != static_cast<int>(rows.size()))
    {
        return "the row counts differ\n";
    }
    const ThrustCurveMotorDatabaseModel& core = selection.getModel();
    for (int row = 0; row < model.rowCount(); ++row)
    {
        const std::size_t modelRow = rows.at(static_cast<std::size_t>(row));
        if (model.modelRow(row) != modelRow || model.row(modelRow) != row ||
            &model.motorSet(row) != &core.getMotorSet(modelRow))
        {
            differences += "row " + std::to_string(row) + " is another model row\n";
        }
        for (const ThrustCurveMotorColumns column : kAllThrustCurveMotorColumns)
        {
            const QVariant shown =
                model.data(model.index(row, static_cast<int>(column)), Qt::DisplayRole);
            if (QtStrings::toUtf8(shown.toString()) !=
                core.getValueText(modelRow, column, preferences))
            {
                differences += "row " + std::to_string(row) + " column " +
                               std::to_string(static_cast<int>(column)) + " differs\n";
            }
        }
    }
    return differences;
}

/// A motor in no set of the database: an Estes "D10" of 24 x 70 mm with the standard delay 7.
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> motorOfNoSet()
{
    Result<ThrustCurveMotor> motor =
        ThrustCurveMotor::Builder()
            .setManufacturer(Manufacturer::getManufacturer("Estes"))
            .setDesignation("D10")
            .setDiameter(0.024)
            .setLength(0.07)
            .setTimePoints({0, 1, 2})
            .setThrustPoints({0, 10, 0})
            .setCGPoints({Coordinate(0.035, 0, 0, 0.03), Coordinate(0.035, 0, 0, 0.02),
                          Coordinate(0.035, 0, 0, 0.01)})
            .setStandardDelays({7})
            .setDigest("digestD10")
            .setMotorType(Motor::Type::SINGLE)
            .build();
    EXPECT_TRUE(motor.has_value());
    return motor.has_value() ? std::make_shared<const ThrustCurveMotor>(std::move(*motor))
                             : nullptr;
}

TEST_F(MotorTableModelTest, ShowsTheCoresRowsInTheTablesOrder)
{
    SmallTable             table;
    const MotorTableModel& model = table.model();
    EXPECT_EQ(model.rowCount(), 11);
    EXPECT_EQ(model.columnCount(), 6);
    EXPECT_EQ(MotorTableModel::kColumnCount, 6);
    EXPECT_EQ(differencesFromTheCore(model, table.selection(), table.preferences()), "");
    // Diameter, Total Impulse, Manufacturer, all ascending.
    EXPECT_EQ(table.text(0, ThrustCurveMotorColumns::NAME), "1/2A3");
    EXPECT_EQ(table.text(2, ThrustCurveMotorColumns::NAME), "B6W");
    EXPECT_EQ(table.text(8, ThrustCurveMotorColumns::MANUFACTURER), "Cesaroni Technology Inc.");
    EXPECT_EQ(table.text(8, ThrustCurveMotorColumns::TOTAL_IMPULSE), "116");
    EXPECT_EQ(table.text(8, ThrustCurveMotorColumns::CASEINFO), "Pro38-1G");
    EXPECT_EQ(table.text(8, ThrustCurveMotorColumns::DIAMETER), "38 mm");
    EXPECT_EQ(table.text(8, ThrustCurveMotorColumns::LENGTH), "127 mm");
    EXPECT_EQ(&model.motorSet(3), &table.set("Estes", "C6"));
    EXPECT_EQ(model.rowOf(table.set("Klima", "C6")), 4);
    EXPECT_EQ(sortKeysText(table.selection().getSorter()), "4A 2A 0A");
}

/// The display texts of @p model's horizontal header, each followed by "|" (and by "(tooltip)"
/// when the section has a tooltip).
[[nodiscard]] std::string headerTexts(const MotorTableModel& model)
{
    std::string texts;
    for (int column = 0; column < model.columnCount(); ++column)
    {
        texts +=
            QtStrings::toUtf8(model.headerData(column, Qt::Horizontal, Qt::DisplayRole).toString());
        texts += model.headerData(column, Qt::Horizontal, Qt::ToolTipRole).isValid() ? "(tooltip)|"
                                                                                     : "|";
    }
    return texts;
}

/// The preferred widths of the columns, separated by spaces.
[[nodiscard]] std::string preferredWidths()
{
    std::string widths;
    for (int column = 0; column < MotorTableModel::kColumnCount; ++column)
    {
        widths +=
            (widths.empty() ? "" : " ") + std::to_string(MotorTableModel::preferredWidth(column));
    }
    return widths;
}

TEST_F(MotorTableModelTest, HasTheColumnTitlesOfOpenRocketAndOnlyTheDisplayRole)
{
    SmallTable             table;
    const MotorTableModel& model = table.model();
    EXPECT_EQ(headerTexts(model), "Manufacturer|Name|Total Impulse|Case|Diameter|Length|");
    EXPECT_FALSE(model.headerData(6, Qt::Horizontal, Qt::DisplayRole).isValid());
    EXPECT_FALSE(model.headerData(-1, Qt::Horizontal, Qt::DisplayRole).isValid());
    EXPECT_FALSE(model.headerData(0, Qt::Vertical, Qt::DisplayRole).isValid());

    const QModelIndex cell = model.index(0, 0);
    EXPECT_TRUE(model.data(cell, Qt::DisplayRole).isValid());
    EXPECT_FALSE(model.data(cell, Qt::ToolTipRole).isValid());
    EXPECT_FALSE(model.data(cell, Qt::EditRole).isValid());
    EXPECT_FALSE(model.data(cell, Qt::TextAlignmentRole).isValid());
    EXPECT_FALSE(model.flags(cell).testFlag(Qt::ItemIsEditable));
    EXPECT_TRUE(model.flags(cell).testFlag(Qt::ItemIsSelectable));

    // The columns' preferred widths (getWidth()).
    EXPECT_EQ(preferredWidths(), "100 50 50 50 50 50");
    EXPECT_EQ(MotorTableModel::columnAt(3), ThrustCurveMotorColumns::CASEINFO);
}

TEST_F(MotorTableModelTest, RefusesRowsAndColumnsOfNoCell)
{
    SmallTable       table;
    MotorTableModel& model = table.model();
    EXPECT_THROW(static_cast<void>(model.modelRow(-1)), BugError);
    EXPECT_THROW(static_cast<void>(model.modelRow(11)), BugError);
    EXPECT_THROW(static_cast<void>(model.motorSet(11)), BugError);
    EXPECT_THROW(static_cast<void>(MotorTableModel::columnAt(6)), BugError);
    EXPECT_THROW(static_cast<void>(MotorTableModel::preferredWidth(-1)), BugError);
    EXPECT_THROW(model.toggleSortOrder(6), BugError);
    EXPECT_EQ(model.row(11), -1);
    EXPECT_EQ(model.row(std::numeric_limits<std::size_t>::max()), -1);
}

TEST_F(MotorTableModelTest, RefreshFollowsTheFilterAndKeepsTheSelectionOnItsSet)
{
    SmallTable          table;
    MotorTableModel&    model = table.model();
    QItemSelectionModel selection(&model);
    const int           estesC6 = model.rowOf(table.set("Estes", "C6"));
    selection.select(model.index(estesC6, 0),
                     QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    const QSignalSpy layouts(&model, &QAbstractItemModel::layoutChanged);

    // Nothing changed: nothing happens.
    model.refresh();
    EXPECT_EQ(layouts.count(), 0);

    table.selection().getRowFilter().setSearchText("c6");
    model.refresh();
    EXPECT_EQ(layouts.count(), 1);
    EXPECT_EQ(model.rowCount(), 2);
    EXPECT_EQ(differencesFromTheCore(model, table.selection(), table.preferences()), "");
    ASSERT_EQ(selection.selectedRows().size(), 1);
    EXPECT_EQ(&model.motorSet(selection.selectedRows().front().row()), &table.set("Estes", "C6"));

    // Filtered out, the selection goes, as JTable's of a row the filter leaves out.
    table.selection().getRowFilter().setSearchText("klima");
    model.refresh();
    EXPECT_EQ(model.rowCount(), 1);
    EXPECT_TRUE(selection.selectedRows().isEmpty());
    EXPECT_EQ(model.rowOf(table.set("Estes", "C6")), -1);

    table.selection().getRowFilter().setSearchText("");
    model.refresh();
    EXPECT_EQ(model.rowCount(), 11);
    EXPECT_EQ(differencesFromTheCore(model, table.selection(), table.preferences()), "");
}

TEST_F(MotorTableModelTest, ASortMovesTheSelectionWithItsSet)
{
    SmallTable          table;
    MotorTableModel&    model = table.model();
    QItemSelectionModel selection(&model);
    selection.setCurrentIndex(model.index(model.rowOf(table.set("Quest", "B6W")), 2),
                              QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    const QPersistentModelIndex kept(model.index(0, 1));
    const QSignalSpy            layouts(&model, &QAbstractItemModel::layoutChanged);

    // A click on "Manufacturer": Manufacturer, Diameter, Total Impulse (the names of this
    // database already sort as its diameters do).
    model.toggleSortOrder(static_cast<int>(ThrustCurveMotorColumns::MANUFACTURER));
    EXPECT_EQ(sortKeysText(table.selection().getSorter()), "0A 4A 2A");
    EXPECT_EQ(layouts.count(), 1);
    EXPECT_EQ(differencesFromTheCore(model, table.selection(), table.preferences()), "");
    EXPECT_EQ(table.text(0, ThrustCurveMotorColumns::NAME), "E15W");
    EXPECT_EQ(table.text(10, ThrustCurveMotorColumns::NAME), "B6W");
    ASSERT_EQ(selection.selectedRows().size(), 1);
    EXPECT_EQ(selection.currentIndex().row(), 10);
    EXPECT_EQ(&model.motorSet(selection.currentIndex().row()), &table.set("Quest", "B6W"));
    EXPECT_EQ(selection.currentIndex().column(), 2);
    // The first row's index went with Estes 1/2A3.
    EXPECT_EQ(kept.row(), 6);
    EXPECT_EQ(&model.motorSet(kept.row()), &table.set("Estes", "1/2A3"));
    EXPECT_EQ(kept.column(), 1);

    // Again: descending.
    model.toggleSortOrder(static_cast<int>(ThrustCurveMotorColumns::MANUFACTURER));
    EXPECT_EQ(sortKeysText(table.selection().getSorter()), "0D 4A 2A");
    EXPECT_EQ(layouts.count(), 2);
    EXPECT_EQ(table.text(0, ThrustCurveMotorColumns::NAME), "B6W");
    EXPECT_EQ(differencesFromTheCore(model, table.selection(), table.preferences()), "");
    EXPECT_EQ(selection.currentIndex().row(), 0);
    EXPECT_EQ(&model.motorSet(selection.currentIndex().row()), &table.set("Quest", "B6W"));

    // Sorted again with nothing changed: nothing to tell.
    model.refresh();
    EXPECT_EQ(layouts.count(), 2);
}

TEST_F(MotorTableModelTest, TheTextsFollowTheNameColumnAndTheUnit)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    SmallTable                              table;
    MotorTableModel&                        model = table.model();
    const int                               b6w   = model.rowOf(table.set("Quest", "B6W"));
    EXPECT_EQ(table.text(b6w, ThrustCurveMotorColumns::NAME), "B6W");
    const QSignalSpy data(&model, &QAbstractItemModel::dataChanged);
    const QSignalSpy layouts(&model, &QAbstractItemModel::layoutChanged);

    // The common name: the same rows in the same order, other texts.
    table.preferences().setMotorNameColumn(false);
    model.refresh();
    EXPECT_EQ(data.count(), 1);
    EXPECT_EQ(layouts.count(), 0);
    EXPECT_EQ(table.text(b6w, ThrustCurveMotorColumns::NAME), "B6");
    EXPECT_EQ(differencesFromTheCore(model, table.selection(), table.preferences()), "");

    // Inches.
    ASSERT_TRUE(unitGroup(UnitGroupId::MOTOR_DIMENSIONS).setDefaultUnit("in"));
    model.refresh();
    EXPECT_EQ(data.count(), 2);
    EXPECT_EQ(differencesFromTheCore(model, table.selection(), table.preferences()), "");
    EXPECT_EQ(table.text(0, ThrustCurveMotorColumns::DIAMETER),
              unitGroup(UnitGroupId::MOTOR_DIMENSIONS).toValue(0.013).toString());
    EXPECT_NE(table.text(0, ThrustCurveMotorColumns::DIAMETER), "13 mm");

    // Sorted by name, the common names reorder the rows.
    model.toggleSortOrder(static_cast<int>(ThrustCurveMotorColumns::NAME));
    table.preferences().setMotorNameColumn(true);
    model.refresh();
    EXPECT_EQ(differencesFromTheCore(model, table.selection(), table.preferences()), "");
}

TEST_F(MotorTableModelTest, AMotorOfNoSetGetsARowAndTheSelectionStaysOnItsSet)
{
    SmallTable          table;
    MotorTableModel&    model = table.model();
    QItemSelectionModel selection(&model);
    selection.select(model.index(model.rowOf(table.set("AeroTech", "M1850W")), 0),
                     QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    // The chooser opened for a mount holding a motor of no set: the core sorts its list of sets
    // again with a set for the motor (ThrustCurveMotorSet::compareTo()), so the model rows of
    // every set change.
    BodyTubeMount mount;
    const auto    motor = motorOfNoSet();
    ASSERT_NE(motor, nullptr);
    const FlightConfigurationId fcid = mount.configure(motor, 7);
    table.selection().setMotorMountAndConfig(MotorChooserInput::fromMount(mount.mount(), fcid));
    ASSERT_EQ(table.selection().getModel().getRowCount(), 12U);
    // The rows show the old list until refresh().
    EXPECT_EQ(model.rowCount(), 11);
    EXPECT_EQ(model.rowOf(table.set("AeroTech", "M1850W")), 10);

    model.refresh();
    EXPECT_EQ(model.rowCount(), 12);
    EXPECT_EQ(differencesFromTheCore(model, table.selection(), table.preferences()), "");
    const ThrustCurveMotorSet* own = table.selection().getSelectedMotorSet();
    ASSERT_NE(own, nullptr);
    EXPECT_GE(model.rowOf(*own), 0);
    EXPECT_EQ(model.data(model.index(model.rowOf(*own), 1), Qt::DisplayRole).toString(),
              QStringLiteral("D10"));
    ASSERT_EQ(selection.selectedRows().size(), 1);
    EXPECT_EQ(&model.motorSet(selection.selectedRows().front().row()),
              &table.set("AeroTech", "M1850W"));
}

TEST_F(MotorTableModelTest, TheHeaderTogglesTheSortKeysAndShowsThePrimaryKey)
{
    SmallTable       table;
    MotorTableModel& model = table.model();
    QWidget          window;
    auto*            view = makeChild<QTableView>(&window);
    view->setModel(&model);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    QHeaderView* header = view->horizontalHeader();
    model.connectHeader(*header);
    EXPECT_TRUE(header->sectionsClickable());
    EXPECT_TRUE(header->isSortIndicatorShown());
    EXPECT_FALSE(view->isSortingEnabled());
    EXPECT_EQ(header->sortIndicatorSection(), static_cast<int>(ThrustCurveMotorColumns::DIAMETER));
    EXPECT_EQ(header->sortIndicatorOrder(), Qt::AscendingOrder);
    ASSERT_TRUE(showActiveWith(window, {view}));

    const auto click = [header](ThrustCurveMotorColumns column) {
        const int section = static_cast<int>(column);
        QTest::mouseClick(
            header->viewport(), Qt::LeftButton, {},
            QPoint(header->sectionViewportPosition(section) + (header->sectionSize(section) / 2),
                   header->height() / 2));
    };
    // The probe's header clicks (ChooserScenarios.rows()).
    click(ThrustCurveMotorColumns::NAME);
    EXPECT_EQ(sortKeysText(table.selection().getSorter()), "1A 4A 2A");
    EXPECT_EQ(header->sortIndicatorSection(), 1);
    EXPECT_EQ(header->sortIndicatorOrder(), Qt::AscendingOrder);
    click(ThrustCurveMotorColumns::NAME);
    EXPECT_EQ(sortKeysText(table.selection().getSorter()), "1D 4A 2A");
    EXPECT_EQ(header->sortIndicatorSection(), 1);
    EXPECT_EQ(header->sortIndicatorOrder(), Qt::DescendingOrder);
    click(ThrustCurveMotorColumns::MANUFACTURER);
    EXPECT_EQ(sortKeysText(table.selection().getSorter()), "0A 1D 4A");
    EXPECT_EQ(header->sortIndicatorSection(), 0);
    EXPECT_EQ(header->sortIndicatorOrder(), Qt::AscendingOrder);
    click(ThrustCurveMotorColumns::CASEINFO);
    EXPECT_EQ(sortKeysText(table.selection().getSorter()), "3A 0A 1D");
    EXPECT_EQ(header->sortIndicatorSection(), 3);
    EXPECT_EQ(differencesFromTheCore(model, table.selection(), table.preferences()), "");

    // Keys set elsewhere show at the next refresh().
    table.selection().getSorter().setSortKeys(MotorTableSorter::kInitialSortKeys);
    model.refresh();
    EXPECT_EQ(header->sortIndicatorSection(), 4);
    EXPECT_EQ(header->sortIndicatorOrder(), Qt::AscendingOrder);
    EXPECT_EQ(differencesFromTheCore(model, table.selection(), table.preferences()), "");
    table.selection().getSorter().setSortKeys({});
    model.refresh();
    EXPECT_EQ(header->sortIndicatorSection(), -1);
}

TEST_F(MotorTableModelTest, FollowsOnlyTheLastHeaderGiven)
{
    SmallTable       table;
    MotorTableModel& model = table.model();
    QWidget          window;
    auto*            first  = makeChild<QTableView>(&window);
    auto*            second = makeChild<QTableView>(&window);
    first->setModel(&model);
    second->setModel(&model);
    model.connectHeader(*first->horizontalHeader());
    model.connectHeader(*second->horizontalHeader());
    Q_EMIT first->horizontalHeader()->sectionClicked(1);
    EXPECT_EQ(sortKeysText(table.selection().getSorter()), "4A 2A 0A");
    Q_EMIT second->horizontalHeader()->sectionClicked(1);
    EXPECT_EQ(sortKeysText(table.selection().getSorter()), "1A 4A 2A");
    EXPECT_EQ(second->horizontalHeader()->sortIndicatorSection(), 1);
}

}  // namespace

}  // namespace QtRocket::Gui
