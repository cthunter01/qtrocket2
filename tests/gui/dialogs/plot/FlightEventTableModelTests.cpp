// The table of the flight events a simulation plot marks (OpenRocket's FlightEventTableModel, a
// private class of SimulationPlotPanel), under QAbstractItemModelTester. The table in the plot
// tab, with OpenRocket's rows of a stored flight, is SimulationPlotPanelTests.cpp's.

#include "QtRocket/gui/dialogs/plot/FlightEventTableModel.h"

#include <initializer_list>
#include <memory>
#include <vector>

#include <QAbstractItemModelTester>
#include <QList>
#include <QModelIndex>
#include <QSignalSpy>
#include <QString>
#include <QVariant>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/util/BugError.h"
#include "dialogs/plot/PlotPanelTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using FlightEventTableModelTest = Gui::Test::GuiTest;
using Type                      = FlightEvent::Type;

/// A stage named @p name of two points of time, with events of @p types at times 0, 1, ...
[[nodiscard]] std::shared_ptr<FlightDataBranch> stageWith(const char*              name,
                                                          const std::vector<Type>& types)
{
    const FlightDataType& time   = FlightDataType::builtin(FlightDataTypeId::TYPE_TIME);
    auto                  branch = std::make_shared<FlightDataBranch>(name, std::vector{&time});
    for (int i = 0; i < 2; ++i)
    {
        branch->addPoint();
        branch->setValue(time, i);
    }
    double at = 0;
    for (const Type type : types)
    {
        branch->addEvent(FlightEvent(type, at));
        at += 0.25;
    }
    return branch;
}

/// A flight of a sustainer with apogee, burnout, an altitude event and ground hit, and a booster
/// with ignition, burnout and ground hit.
[[nodiscard]] std::shared_ptr<FlightData> twoStages()
{
    return std::make_shared<FlightData>(std::initializer_list{
        stageWith("Sustainer", {Type::APOGEE, Type::BURNOUT, Type::ALTITUDE, Type::GROUND_HIT}),
        stageWith("Booster", {Type::IGNITION, Type::BURNOUT, Type::GROUND_HIT})});
}

/// The model over @p configuration and the types of twoStages(), under QAbstractItemModelTester.
class TwoStageTable
{
public:
    explicit TwoStageTable(SimulationPlotConfiguration& configuration)
      : m_model(FlightEventTableModel::eventTypesOf(*twoStages()), configuration),
        m_tester(&m_model, QAbstractItemModelTester::FailureReportingMode::Warning)
    {
    }

    [[nodiscard]] FlightEventTableModel& model() noexcept { return m_model; }

private:
    FlightEventTableModel    m_model;
    QAbstractItemModelTester m_tester;
};

TEST_F(FlightEventTableModelTest, ItsRowsAreTheFlightsEventTypesInDeclarationOrder)
{
    // Java: an EnumSet of every branch's events' types, ALTITUDE removed, in Type.values() order.
    EXPECT_EQ(FlightEventTableModel::eventTypesOf(*twoStages()),
              (std::vector{Type::IGNITION, Type::BURNOUT, Type::APOGEE, Type::GROUND_HIT}));
    EXPECT_TRUE(FlightEventTableModel::eventTypesOf(FlightData()).empty());
}

TEST_F(FlightEventTableModelTest, TheChecksAreTheEventsTheConfigurationMarks)
{
    SimulationPlotConfiguration configuration("Test");
    configuration.setEvent(Type::APOGEE, true);
    configuration.setEvent(Type::IGNITION, true);
    TwoStageTable                table(configuration);
    const FlightEventTableModel& model = table.model();
    ASSERT_EQ(model.rowCount(), 4);
    EXPECT_EQ(model.columnCount(), FlightEventTableModel::kColumnCount);
    EXPECT_EQ(model.rowCount(model.index(0, 0)), 0);
    EXPECT_EQ(Gui::Test::eventsText(model),
              "4: Motor ignition=true; Motor burnout=false; Apogee=true; Ground hit=false");
    EXPECT_EQ(model.eventType(2), Type::APOGEE);
    EXPECT_EQ(model.index(0, FlightEventTableModel::kSelectedColumn).data(Qt::CheckStateRole),
              QVariant(Qt::Checked));
    EXPECT_EQ(model.index(1, FlightEventTableModel::kSelectedColumn).data(Qt::CheckStateRole),
              QVariant(Qt::Unchecked));
    EXPECT_FALSE(model.index(0, FlightEventTableModel::kSelectedColumn).data().isValid());
    EXPECT_EQ(model.index(3, FlightEventTableModel::kNameColumn).data(),
              QVariant(QStringLiteral("Ground hit")));
    EXPECT_FALSE(
        model.index(3, FlightEventTableModel::kNameColumn).data(Qt::CheckStateRole).isValid());
    // Only the check can be changed (isCellEditable(row, 0)); nothing can be selected.
    EXPECT_EQ(model.flags(model.index(0, FlightEventTableModel::kSelectedColumn)),
              Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    EXPECT_EQ(model.flags(model.index(0, FlightEventTableModel::kNameColumn)), Qt::ItemIsEnabled);
    EXPECT_THROW(static_cast<void>(model.eventType(4)), BugError);
    EXPECT_THROW(static_cast<void>(model.isChecked(-1)), BugError);
}

TEST_F(FlightEventTableModelTest, ACheckMarksTheTypeOrStopsMarkingIt)
{
    SimulationPlotConfiguration configuration("Test");
    TwoStageTable               table(configuration);
    FlightEventTableModel&      model = table.model();
    const QSignalSpy            changed(&model, &FlightEventTableModel::dataChanged);

    // setValueAt(true, row, 0), fireTableCellUpdated(row, 0).
    const QModelIndex burnout = model.index(1, FlightEventTableModel::kSelectedColumn);
    EXPECT_TRUE(model.setData(burnout, Qt::Checked, Qt::CheckStateRole));
    EXPECT_TRUE(configuration.isEventActive(Type::BURNOUT));
    ASSERT_EQ(changed.count(), 1);
    EXPECT_EQ(changed.at(0).at(0).value<QModelIndex>(), burnout);
    EXPECT_EQ(changed.at(0).at(1).value<QModelIndex>(), burnout);

    // A click on a name (BooleanTableClickListener): the other state.
    model.toggle(1);
    EXPECT_FALSE(configuration.isEventActive(Type::BURNOUT));
    model.toggle(0);
    EXPECT_TRUE(configuration.isEventActive(Type::IGNITION));
    EXPECT_EQ(changed.count(), 3);

    // Java's setValueAt() throws for another column or a value that is not a Boolean.
    EXPECT_FALSE(model.setData(model.index(1, FlightEventTableModel::kNameColumn), Qt::Checked,
                               Qt::CheckStateRole));
    EXPECT_FALSE(model.setData(burnout, Qt::Checked, Qt::EditRole));
    EXPECT_FALSE(model.setData(burnout, QStringLiteral("yes"), Qt::CheckStateRole));
    EXPECT_FALSE(model.setData(burnout, Qt::PartiallyChecked, Qt::CheckStateRole));
    EXPECT_FALSE(configuration.isEventActive(Type::BURNOUT));
    EXPECT_EQ(changed.count(), 3);
}

TEST_F(FlightEventTableModelTest, RefreshTellsOfEveryCheck)
{
    SimulationPlotConfiguration configuration("Test");
    TwoStageTable               table(configuration);
    FlightEventTableModel&      model = table.model();
    const QSignalSpy            changed(&model, &FlightEventTableModel::dataChanged);
    // The configuration changed elsewhere (All, None, a new configuration): fireTableDataChanged().
    configuration.setEvent(Type::GROUND_HIT, true);
    model.refresh();
    ASSERT_EQ(changed.count(), 1);
    EXPECT_EQ(changed.at(0).at(0).value<QModelIndex>(), model.index(0, 0));
    EXPECT_EQ(changed.at(0).at(1).value<QModelIndex>(), model.index(3, 1));
    EXPECT_TRUE(model.isChecked(3));

    // A table without rows tells of nothing.
    FlightEventTableModel          empty({}, configuration);
    const QAbstractItemModelTester tester(&empty,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    const QSignalSpy               emptyChanged(&empty, &FlightEventTableModel::dataChanged);
    empty.refresh();
    EXPECT_EQ(empty.rowCount(), 0);
    EXPECT_EQ(emptyChanged.count(), 0);
}

}  // namespace

}  // namespace QtRocket::Gui
