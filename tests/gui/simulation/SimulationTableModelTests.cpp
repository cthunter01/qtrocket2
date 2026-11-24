// The simulations table's model (Java: SimulationPanel's SimulationTableModel behind its
// ColumnTable and ColumnTableRowSorter). OpenRocket has no JUnit test of it. Two kinds of tests:
// - SimulationTableModelTest: the model's own behaviour (its rows and roles, the readings once per
//   document event, the order, the indexes that follow their simulations: OpenRocket bug 28),
//   under QAbstractItemModelTester;
// - the golden suites: the table of the sixteen examples and a new design, every cell, tooltip,
//   status and warning count and the order after each header click, compared with OpenRocket's own
//   table through the stored data only (SimulationTableModelGolden), and two scenarios of edits
//   with every status after each step (SimulationTableScenarioGolden; the one run of a fresh flight
//   is "A simple model rocket" #1's, whose values are not pinned), all from
//   tests/data/goldens/gui/simulation-table (SimulationTablePins.h).

#include "QtRocket/gui/simulation/SimulationTableModel.h"

#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <QAbstractItemModel>
#include <QAbstractItemModelTester>
#include <QColor>
#include <QFont>
#include <QGuiApplication>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QPoint>
#include <QRect>
#include <QSignalSpy>
#include <QString>
#include <QTableView>
#include <QTest>
#include <QVariant>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationWarningsDelegate.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/TableSortKeys.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenExamples.h"
#include "simulation/SimulationTablePins.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationTableModelTest = Test::GuiTest;
using Column                   = SimulationColumns::Column;
using Status                   = Simulation::Status;
using SortKey                  = TableSortKeys::SortKey;
using SortOrder                = TableSortKeys::SortOrder;

/// The preferences of the examples' loads (DesignFileEnvironment: a motor by its common name, as
/// the pins' probe has it).
[[nodiscard]] const Preferences& examplePreferences()
{
    return QtRocket::Test::goldenExampleEnvironment().preferences();
}

/// The example design whose file in data/examples is @p file, as loaded; null (and a test
/// failure) when it cannot be loaded.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> loadExample(std::string_view file)
{
    const QtRocket::Test::GoldenExample* example = QtRocket::Test::findGoldenExample(file);
    if (example == nullptr)
    {
        ADD_FAILURE() << "no example " << file;
        return nullptr;
    }
    Result<LoadedDocument> loaded = QtRocket::Test::loadGoldenExample(*example);
    if (!loaded.has_value())
    {
        ADD_FAILURE() << loaded.error().toString();
        return nullptr;
    }
    return std::move(loaded->document);
}

/// A document and its simulations table, under QAbstractItemModelTester (the document outlives
/// the model).
class Table
{
public:
    explicit Table(std::unique_ptr<OpenRocketDocument> document)
      : m_document(std::move(document)),
        m_model(*m_document, examplePreferences(), nullptr),
        m_tester(&m_model, QAbstractItemModelTester::FailureReportingMode::Warning)
    {
    }

    [[nodiscard]] OpenRocketDocument&         document() const noexcept { return *m_document; }
    [[nodiscard]] SimulationTableModel&       model() noexcept { return m_model; }
    [[nodiscard]] const SimulationTableModel& model() const noexcept { return m_model; }

    /// The rows' simulations as their indices in the document.
    [[nodiscard]] std::vector<int> documentOrder() const
    {
        std::vector<int> order;
        for (int row = 0; row < m_model.rowCount(); ++row)
        {
            const std::optional<std::size_t> index =
                m_document->getSimulationIndex(*m_model.simulation(row));
            order.push_back(index.has_value() ? static_cast<int>(*index) : -1);
        }
        return order;
    }

    /// The status of each simulation of the document, in its order, as the table shows it.
    [[nodiscard]] std::vector<std::string> statuses() const
    {
        std::vector<std::string> statuses;
        for (const std::shared_ptr<Simulation>& simulation : m_document->getSimulations())
        {
            const int row = m_model.rowOf(*simulation);
            statuses.emplace_back(row < 0 ? std::string_view("<no row>")
                                          : name(m_model.status(row)));
        }
        return statuses;
    }

private:
    std::unique_ptr<OpenRocketDocument> m_document;
    SimulationTableModel                m_model;
    QAbstractItemModelTester            m_tester;
};

/// The example "A simple model rocket" (five simulations, the third with a warning) in a table.
[[nodiscard]] std::unique_ptr<Table> simpleModelRocket()
{
    std::unique_ptr<OpenRocketDocument> document = loadExample("A simple model rocket.ork");
    if (document == nullptr)
    {
        return nullptr;
    }
    return std::make_unique<Table>(std::move(document));
}

/// The texts of a QVariant (empty for none).
[[nodiscard]] std::string textOf(const QVariant& value)
{
    return QtStrings::toUtf8(value.toString());
}

/// The cell of @p table at @p row and @p column, for @p role.
[[nodiscard]] QVariant cell(const SimulationTableModel& model, int row, Column column, int role)
{
    return model.data(model.index(row, SimulationColumns::indexOf(column)), role);
}

/// The row @p row of @p table is the document's simulation @p row, LOADED, the element @p row of
/// @p simulations.
void expectDocumentRow(const Table&                                    table,
                       const std::vector<std::shared_ptr<Simulation>>& simulations, int row)
{
    const SimulationTableModel& model = table.model();
    EXPECT_EQ(model.simulation(row), table.document().getSimulation(static_cast<std::size_t>(row)));
    EXPECT_EQ(simulations.at(static_cast<std::size_t>(row)), model.simulation(row));
    EXPECT_EQ(model.rowOf(*model.simulation(row)), row);
    EXPECT_EQ(model.status(row), Status::LOADED);
}

// ------------------------------------------------------------------------------ the rows

TEST_F(SimulationTableModelTest, ShowsTheDocumentsSimulationsInItsOrder)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    const SimulationTableModel& model = table->model();
    ASSERT_EQ(model.rowCount(), 5);
    EXPECT_EQ(model.columnCount(), SimulationColumns::kColumnCount);
    EXPECT_EQ(&model.document(), &table->document());
    EXPECT_EQ(table->documentOrder(), (std::vector<int>{0, 1, 2, 3, 4}));
    const std::vector<std::shared_ptr<Simulation>> simulations = model.simulations();
    ASSERT_EQ(simulations.size(), 5U);
    for (int row = 0; row < 5; ++row)
    {
        expectDocumentRow(*table, simulations, row);
    }
}

TEST_F(SimulationTableModelTest, HasNoRowOfAnotherDocumentsSimulation)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    const std::unique_ptr<OpenRocketDocument> other = OpenRocketDocumentFactory::createNewRocket();
    const Simulation                          stranger(other->getRocket());
    EXPECT_EQ(table->model().rowOf(stranger), -1);
}

TEST_F(SimulationTableModelTest, RefusesARowOutsideTheTable)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    const SimulationTableModel& model = table->model();
    EXPECT_THROW(static_cast<void>(model.simulation(-1)), BugError);
    EXPECT_THROW(static_cast<void>(model.simulation(5)), BugError);
    EXPECT_THROW(static_cast<void>(model.status(5)), BugError);
    EXPECT_THROW(static_cast<void>(model.cellText(5, Column::NAME)), BugError);
}

TEST_F(SimulationTableModelTest, HasNoRowsForADesignWithoutSimulations)
{
    Table table(OpenRocketDocumentFactory::createNewRocket());
    EXPECT_EQ(table.model().rowCount(), 0);
    EXPECT_EQ(table.model().columnCount(), SimulationColumns::kColumnCount);
    EXPECT_TRUE(table.model().simulations().empty());
    // A header click sorts nothing.
    table.model().toggleSortOrder(2);
    EXPECT_EQ(table.model().rowCount(), 0);
}

TEST_F(SimulationTableModelTest, ShowsTheCellsRoles)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    const SimulationTableModel& model = table->model();
    // The texts.
    EXPECT_EQ(textOf(cell(model, 0, Column::NAME, Qt::DisplayRole)), "Simulation 1");
    EXPECT_EQ(textOf(cell(model, 0, Column::APOGEE, Qt::DisplayRole)), "50.6 m");
    EXPECT_EQ(model.cellText(0, Column::APOGEE), QStringLiteral("50.6 m"));
    EXPECT_FALSE(cell(model, 0, Column::STATUS, Qt::DisplayRole).isValid());
    EXPECT_FALSE(cell(model, 0, Column::WARNINGS, Qt::DisplayRole).isValid());
    // The status: an icon and its accessible text.
    EXPECT_FALSE(cell(model, 0, Column::STATUS, Qt::DecorationRole).value<QIcon>().isNull());
    EXPECT_FALSE(cell(model, 0, Column::NAME, Qt::DecorationRole).isValid());
    EXPECT_EQ(textOf(cell(model, 0, Column::STATUS, Qt::AccessibleTextRole)), "Loaded from File");
    // The tooltips: the warnings' in the warnings column, the status's elsewhere.
    EXPECT_EQ(textOf(cell(model, 2, Column::WARNINGS, Qt::ToolTipRole)),
              QtStrings::toUtf8(SimulationColumns::warningsToolTip(*model.simulation(2), true,
                                                                   QGuiApplication::palette())));
    const std::string statusTip = QtStrings::toUtf8(SimulationColumns::statusToolTip(
        Status::LOADED, *model.simulation(2), true, QGuiApplication::palette()));
    EXPECT_EQ(textOf(cell(model, 2, Column::STATUS, Qt::ToolTipRole)), statusTip);
    EXPECT_EQ(textOf(cell(model, 2, Column::NAME, Qt::ToolTipRole)), statusTip);
    EXPECT_EQ(textOf(cell(model, 2, Column::APOGEE, Qt::ToolTipRole)), statusTip);
    // The counts of the warnings column alone.
    EXPECT_EQ(cell(model, 2, Column::WARNINGS, SimulationTableModel::kCriticalWarningsRole), 0);
    EXPECT_EQ(cell(model, 2, Column::WARNINGS, SimulationTableModel::kNormalWarningsRole), 1);
    EXPECT_EQ(cell(model, 2, Column::WARNINGS, SimulationTableModel::kInformationalWarningsRole),
              0);
    EXPECT_FALSE(cell(model, 2, Column::NAME, SimulationTableModel::kNormalWarningsRole).isValid());
    EXPECT_EQ(model.warningCounts(2),
              (SimulationColumns::WarningCounts{.critical = 0, .normal = 1, .informational = 0}));
    // Enabled and selectable, never editable.
    EXPECT_EQ(model.flags(model.index(0, 2)),
              Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren);
}

TEST_F(SimulationTableModelTest, ShowsTheHeadersTextsAndTooltips)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    const SimulationTableModel& model = table->model();
    EXPECT_EQ(model.headerData(2, Qt::Horizontal, Qt::DisplayRole), QStringLiteral("Name"));
    EXPECT_EQ(model.headerData(2, Qt::Horizontal, Qt::ToolTipRole), QStringLiteral("Name"));
    EXPECT_EQ(model.headerData(4, Qt::Horizontal, Qt::ToolTipRole),
              QStringLiteral("Simulation Stepper Method"));
    EXPECT_EQ(model.headerData(0, Qt::Horizontal, Qt::DisplayRole), QString());
    // No tooltip for the status column's empty one, no row header.
    EXPECT_FALSE(model.headerData(0, Qt::Horizontal, Qt::ToolTipRole).isValid());
    EXPECT_FALSE(model.headerData(0, Qt::Vertical, Qt::DisplayRole).isValid());
    EXPECT_FALSE(model.headerData(SimulationColumns::kColumnCount, Qt::Horizontal, Qt::DisplayRole)
                     .isValid());
}

// ------------------------------------------------------------------------------ the readings

TEST_F(SimulationTableModelTest, ReadsTheStatusesAtEveryDocumentEvent)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    Simulation&      simulation = *table->document().getSimulation(0);
    const double     rodLength  = simulation.getOptions().getLaunchRodLength();
    const QSignalSpy refreshed(&table->model(), &SimulationTableModel::refreshed);

    // The options' change makes it OUTDATED, which the reading stores (getStatus()): back to the
    // first length it stays OUTDATED, as in OpenRocket once its table was painted in between.
    simulation.getOptions().setLaunchRodLength(rodLength + 0.5);
    EXPECT_EQ(table->model().status(0), Status::OUTDATED);
    EXPECT_EQ(simulation.getStoredStatus(), Status::OUTDATED);
    simulation.getOptions().setLaunchRodLength(rodLength);
    EXPECT_EQ(table->model().status(0), Status::OUTDATED);
    EXPECT_EQ(simulation.getStoredStatus(), Status::OUTDATED);
    EXPECT_EQ(refreshed.count(), 2);

    // A rename tells the views of new data: the same rows in the same order.
    const QSignalSpy dataChanged(&table->model(), &QAbstractItemModel::dataChanged);
    const QSignalSpy layoutChanged(&table->model(), &QAbstractItemModel::layoutChanged);
    table->document().getSimulation(1)->setName("Renamed");
    EXPECT_EQ(textOf(cell(table->model(), 1, Column::NAME, Qt::DisplayRole)), "Renamed");
    EXPECT_EQ(dataChanged.count(), 1);
    EXPECT_EQ(layoutChanged.count(), 0);
}

TEST_F(SimulationTableModelTest, ReadsTheUnitsAgainOnlyWhenRefreshed)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    const std::unique_ptr<Table>            table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    ASSERT_TRUE(unitGroup(UnitGroupId::DISTANCE).setDefaultUnit("ft"));
    // No document event: the cells keep their units until refresh().
    EXPECT_EQ(table->model().cellText(0, Column::APOGEE), QStringLiteral("50.6 m"));
    const QSignalSpy refreshed(&table->model(), &SimulationTableModel::refreshed);
    table->model().refresh();
    EXPECT_EQ(refreshed.count(), 1);
    EXPECT_EQ(table->model().cellText(0, Column::APOGEE), QStringLiteral("166 ft"));
}

TEST_F(SimulationTableModelTest, FollowsTheListOfSimulations)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    OpenRocketDocument& document = table->document();
    // A new simulation: a row of its own, NOT_SIMULATED, without data.
    const auto added = std::make_shared<Simulation>(&document, document.getRocket());
    added->setName(document.getNextSimulationName());
    document.addUndoPosition("Add " + added->getName());
    document.addSimulation(added);
    ASSERT_EQ(table->model().rowCount(), 6);
    EXPECT_EQ(table->model().simulation(5), added);
    EXPECT_EQ(table->model().status(5), Status::NOT_SIMULATED);
    EXPECT_FALSE(cell(table->model(), 5, Column::APOGEE, Qt::DisplayRole).isValid());
    EXPECT_EQ(table->model().warningCounts(5), std::nullopt);
    // Its undo takes the row away, the redo brings it back (another object: the list is new).
    document.undo();
    EXPECT_EQ(table->model().rowCount(), 5);
    document.redo();
    ASSERT_EQ(table->model().rowCount(), 6);
    EXPECT_EQ(table->model().simulation(5)->getName(), "Simulation 6");
    // A removed simulation leaves.
    const std::shared_ptr<Simulation> second = document.getSimulation(1);
    document.removeSimulation(*second);
    EXPECT_EQ(table->model().rowCount(), 5);
    EXPECT_EQ(table->model().rowOf(*second), -1);
}

TEST_F(SimulationTableModelTest, ShowsTheListsChangesInAView)
{
    // A layout change that adds or takes rows: the view's headers follow it.
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    QTableView view;
    view.setModel(&table->model());
    OpenRocketDocument& document = table->document();
    const auto          added    = std::make_shared<Simulation>(&document, document.getRocket());
    document.addSimulation(added);
    EXPECT_EQ(view.verticalHeader()->count(), 6);
    EXPECT_EQ(view.model()->rowCount(), 6);
    document.removeSimulation(*document.getSimulation(0));
    document.removeSimulation(*document.getSimulation(0));
    EXPECT_EQ(view.verticalHeader()->count(), 4);
    EXPECT_EQ(view.horizontalHeader()->count(), SimulationColumns::kColumnCount);
    while (document.getSimulationCount() > 0)
    {
        document.removeSimulation(*document.getSimulation(0));
    }
    EXPECT_EQ(view.verticalHeader()->count(), 0);
}

// ------------------------------------------------------------------------------ the order

TEST_F(SimulationTableModelTest, SortsByTheHeaderClicks)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    SimulationTableModel& model = table->model();
    const QSignalSpy      keys(&model, &SimulationTableModel::sortKeysChanged);
    // Apogee: 50.6, 135, 279, 317, 320 m.
    model.toggleSortOrder(SimulationColumns::indexOf(Column::APOGEE));
    EXPECT_EQ(table->documentOrder(), (std::vector<int>{0, 1, 2, 3, 4}));
    model.toggleSortOrder(SimulationColumns::indexOf(Column::APOGEE));
    EXPECT_EQ(table->documentOrder(), (std::vector<int>{4, 3, 2, 1, 0}));
    EXPECT_EQ(model.sortKeys().keys(),
              (std::vector<SortKey>{
                  {SimulationColumns::indexOf(Column::APOGEE), SortOrder::DESCENDING}}));
    // Back to the document's order.
    model.setSortKeys({});
    EXPECT_EQ(table->documentOrder(), (std::vector<int>{0, 1, 2, 3, 4}));
    EXPECT_EQ(keys.count(), 3);
    EXPECT_THROW(model.toggleSortOrder(SimulationColumns::kColumnCount), BugError);
}

TEST_F(SimulationTableModelTest, KeepsTheSelectionOnItsSimulationsAcrossANewOrder)
{
    // OpenRocket bug 28, fixed: OpenRocket selects the same view rows again after a re-sort.
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    SimulationTableModel& model = table->model();
    QItemSelectionModel   selection(&model);
    selection.select(model.index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
    selection.select(model.index(3, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
    selection.setCurrentIndex(model.index(3, 2), QItemSelectionModel::NoUpdate);
    const std::shared_ptr<Simulation> first  = model.simulation(0);
    const std::shared_ptr<Simulation> fourth = model.simulation(3);
    const QPersistentModelIndex       kept(model.index(3, 6));

    model.toggleSortOrder(SimulationColumns::indexOf(Column::APOGEE));
    model.toggleSortOrder(SimulationColumns::indexOf(Column::APOGEE));
    ASSERT_EQ(table->documentOrder(), (std::vector<int>{4, 3, 2, 1, 0}));
    EXPECT_TRUE(selection.isRowSelected(model.rowOf(*first)));
    EXPECT_TRUE(selection.isRowSelected(model.rowOf(*fourth)));
    EXPECT_EQ(selection.selectedRows().size(), 2);
    EXPECT_EQ(selection.currentIndex(), model.index(model.rowOf(*fourth), 2));
    EXPECT_EQ(kept.row(), model.rowOf(*fourth));
    EXPECT_EQ(kept.column(), 6);

    // A status that changes in a table sorted by status moves its row, and the selection with it.
    model.setSortKeys({SortKey{.column = SimulationColumns::indexOf(Column::STATUS),
                               .order  = SortOrder::ASCENDING}});
    fourth->getOptions().setLaunchRodLength(fourth->getOptions().getLaunchRodLength() + 0.5);
    // "Loaded from file-" before "Out of date-".
    EXPECT_EQ(model.rowOf(*fourth), 4);
    EXPECT_EQ(model.status(4), Status::OUTDATED);
    EXPECT_TRUE(selection.isRowSelected(4));
    EXPECT_TRUE(selection.isRowSelected(model.rowOf(*first)));
    EXPECT_EQ(selection.selectedRows().size(), 2);
}

TEST_F(SimulationTableModelTest, DropsTheIndexesOfASimulationThatLeaves)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    const SimulationTableModel&       model = table->model();
    const QPersistentModelIndex       second(model.index(1, 2));
    const QPersistentModelIndex       fifth(model.index(4, 2));
    const std::shared_ptr<Simulation> removed = model.simulation(1);
    table->document().removeSimulation(*removed);
    EXPECT_FALSE(second.isValid());
    ASSERT_TRUE(fifth.isValid());
    EXPECT_EQ(fifth.row(), 3);
    EXPECT_EQ(model.simulation(fifth.row())->getName(), "Simulation 5");
}

TEST_F(SimulationTableModelTest, ShowsTheSortArrowOfThePrimaryKeyOnItsHeader)
{
    const std::unique_ptr<Table> table = simpleModelRocket();
    ASSERT_NE(table, nullptr);
    SimulationTableModel& model = table->model();
    QTableView            view;
    view.setModel(&model);
    QHeaderView& header = *view.horizontalHeader();
    model.connectHeader(header);
    EXPECT_TRUE(header.sectionsClickable());
    EXPECT_TRUE(header.isSortIndicatorShown());
    EXPECT_EQ(header.sortIndicatorSection(), -1);

    // A click on the Name header (its section's middle) sorts by name, a second one the other way.
    view.resize(1200, 300);
    const int  name = SimulationColumns::indexOf(Column::NAME);
    const auto middle =
        QPoint(header.sectionViewportPosition(name) + (header.sectionSize(name) / 2), 5);
    QTest::mouseClick(header.viewport(), Qt::LeftButton, {}, middle);
    EXPECT_EQ(header.sortIndicatorSection(), name);
    EXPECT_EQ(header.sortIndicatorOrder(), Qt::AscendingOrder);
    QTest::mouseClick(header.viewport(), Qt::LeftButton, {}, middle);
    EXPECT_EQ(header.sortIndicatorOrder(), Qt::DescendingOrder);
    EXPECT_EQ(table->documentOrder(), (std::vector<int>{4, 3, 2, 1, 0}));
    model.setSortKeys({});
    EXPECT_EQ(header.sortIndicatorSection(), -1);
}

// ------------------------------------------------------------------------------ the pins

/// The pins, or a test failure.
[[nodiscard]] const Gui::Test::SimulationTablePins* pins()
{
    const Result<Gui::Test::SimulationTablePins>& loaded = Gui::Test::simulationTablePins();
    if (!loaded.has_value())
    {
        ADD_FAILURE() << loaded.error().toString();
        return nullptr;
    }
    return &*loaded;
}

/// What OpenRocket's WarningsBox shows for the counts of the cell at @p index: the delegate's
/// parts, a text as it is and an icon as [HIGH], [NORMAL] or [LOW].
[[nodiscard]] std::string boxText(const QModelIndex& index)
{
    const std::optional<SimulationColumns::WarningCounts> counts =
        SimulationWarningsDelegate::counts(index);
    if (!counts.has_value())
    {
        return {};
    }
    std::string text;
    for (const SimulationWarningsDelegate::Part& part :
         SimulationWarningsDelegate::layOut(*counts, QRect(0, 0, 400, 20), QFont()))
    {
        if (!part.icon.has_value())
        {
            text += QtStrings::toUtf8(part.text);
        }
        else if (*part.icon == Icons::Id::WARNING_HIGH)
        {
            text += "[HIGH]";
        }
        else
        {
            text += *part.icon == Icons::Id::WARNING_NORMAL ? "[NORMAL]" : "[LOW]";
        }
    }
    return text;
}

/// The priority's name as OpenRocket prints it (MessagePriority.name()).
[[nodiscard]] std::string_view priorityName(MessagePriority priority)
{
    switch (priority)
    {
        case MessagePriority::HIGH:
            return "HIGH";
        case MessagePriority::NORMAL:
            return "NORMAL";
        case MessagePriority::LOW:
            return "LOW";
    }
    return {};
}

/// The warning counts of the row @p row as the pins hold them: critical, normal, informational.
[[nodiscard]] std::optional<std::array<std::size_t, 3>> countsOf(const SimulationTableModel& model,
                                                                 int                         row)
{
    const std::optional<SimulationColumns::WarningCounts> counts = model.warningCounts(row);
    if (!counts.has_value())
    {
        return std::nullopt;
    }
    return std::array{counts->critical, counts->normal, counts->informational};
}

/// The warnings of @p simulation, in the set's order: their priorities' names and texts.
[[nodiscard]] std::vector<std::pair<std::string, std::string>> messagesOf(
    const Simulation& simulation)
{
    std::vector<std::pair<std::string, std::string>> messages;
    if (const WarningSet* warnings = simulation.getSimulatedWarnings(); warnings != nullptr)
    {
        for (const Warning& warning : *warnings)
        {
            messages.emplace_back(priorityName(warning.priority()), warning.toString());
        }
    }
    return messages;
}

/// The warnings column of the row @p row is the pin's @p pin.
void expectWarnings(const SimulationTableModel& model, int row,
                    const Gui::Test::SimulationTablePinWarnings& pin)
{
    EXPECT_EQ(countsOf(model, row), pin.counts);
    const QModelIndex index = model.index(row, SimulationColumns::indexOf(Column::WARNINGS));
    EXPECT_EQ(boxText(index), pin.box);
    EXPECT_EQ(textOf(model.data(index, Qt::ToolTipRole)), pin.toolTip);
    EXPECT_EQ(messagesOf(*model.simulation(row)), pin.messages);
}

/// The status of the row @p row of @p model is the pin's row @p pin's.
void expectStatus(const SimulationTableModel& model, int row,
                  const Gui::Test::SimulationTablePinRow& pin)
{
    EXPECT_EQ(name(model.status(row)), pin.status);
    EXPECT_EQ(textOf(cell(model, row, Column::STATUS, Qt::AccessibleTextRole)), pin.statusIcon);
    const auto* sortText = std::get_if<std::string>(&model.cellValue(row, Column::STATUS));
    EXPECT_EQ(sortText != nullptr ? *sortText : std::string("<none>"), pin.statusSortText);
    EXPECT_EQ(QtStrings::toUtf8(
                  SimulationColumns::statusDescription(model.status(row), *model.simulation(row))),
              pin.description);
    EXPECT_EQ(textOf(cell(model, row, Column::NAME, Qt::ToolTipRole)), pin.statusToolTip);
    EXPECT_EQ(textOf(cell(model, row, Column::STATUS, Qt::ToolTipRole)), pin.statusToolTip);
}

/// The cells of the model columns 2 to 13 of the row @p row as the pins hold them: the text,
/// nullopt for none.
[[nodiscard]] std::vector<std::optional<std::string>> cellsOf(const SimulationTableModel& model,
                                                              int                         row)
{
    std::vector<std::optional<std::string>> cells;
    for (int column = 2; column < SimulationColumns::kColumnCount; ++column)
    {
        const QVariant shown = model.data(model.index(row, column), Qt::DisplayRole);
        cells.push_back(shown.isValid() ? std::optional(textOf(shown)) : std::nullopt);
    }
    return cells;
}

/// The row @p row of @p model shows the pin's row @p pin (only its status for a fresh one).
void expectRow(const SimulationTableModel& model, int row,
               const Gui::Test::SimulationTablePinRow& pin)
{
    SCOPED_TRACE(testing::Message() << "row " << row << " (" << pin.row << ")");
    expectStatus(model, row, pin);
    if (pin.fresh)
    {
        return;
    }
    EXPECT_TRUE(pin.warnings.has_value());
    if (pin.warnings.has_value())
    {
        expectWarnings(model, row, *pin.warnings);
    }
    EXPECT_EQ(cellsOf(model, row), pin.cells);
}

/// Every row of @p table shows its pin of @p pins (the rows in the document's order: no keys).
void expectRows(const Table& table, const std::vector<Gui::Test::SimulationTablePinRow>& pins)
{
    ASSERT_EQ(table.model().rowCount(), static_cast<int>(pins.size()));
    ASSERT_TRUE(table.model().sortKeys().keys().empty());
    for (int row = 0; row < table.model().rowCount(); ++row)
    {
        expectRow(table.model(), row, pins.at(static_cast<std::size_t>(row)));
    }
}

/// A click and a second click on the header of each column of @p orders give their orders.
void expectOrders(Table&                                                           table,
                  const std::map<std::string, Gui::Test::SimulationTablePinOrder>& orders)
{
    for (const auto& [id, order] : orders)
    {
        const std::optional<Column> column = SimulationColumns::fromId(id);
        if (!column.has_value())
        {
            continue;  // OpenRocket's own order where QtRocket sorts otherwise ("openRocket:...")
        }
        SCOPED_TRACE(testing::Message() << "column " << id);
        table.model().setSortKeys({});
        table.model().toggleSortOrder(SimulationColumns::indexOf(*column));
        EXPECT_EQ(table.documentOrder(), order.ascending);
        table.model().toggleSortOrder(SimulationColumns::indexOf(*column));
        EXPECT_EQ(table.documentOrder(), order.descending);
    }
    table.model().setSortKeys({});
}

TEST(SimulationTablePinsGolden, ComeFromTheGoldensOpenRocket)
{
    const Gui::Test::SimulationTablePins* table = pins();
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->commit, "5f164fd0e05d6e188857b0185601293b7a6e9eaf");
    EXPECT_EQ(table->columns.size(), SimulationColumns::kAllColumns.size());
    // The sixteen examples and a new design.
    EXPECT_EQ(table->designs.size(), 17U);
    EXPECT_NE(table->findDesign("example_a_simple_model_rocket"), nullptr);
    EXPECT_NE(table->findDesign("new_design"), nullptr);
    EXPECT_NE(table->findScenario("status_a_simple_model_rocket"), nullptr);
    EXPECT_NE(table->findScenario("status_two_stage_high_power_rocket"), nullptr);
    EXPECT_NE(table->findPairing("copy_status_pairing"), nullptr);
}

/// "#rrggbb" of ThemeColors' light colour of @p key.
[[nodiscard]] std::string lightHex(ThemeColors::Key key)
{
    return QtStrings::toUtf8(
        ThemeColors::themeColor(key, ThemeColors::Theme::LIGHT).name(QColor::HexRgb));
}

TEST(SimulationTablePinsGolden, HaveThemeColorsLightColoursInTheTooltips)
{
    // OpenRocket's light theme's colours of the tooltips are ThemeColors' light ones.
    const Gui::Test::SimulationTablePins* table = pins();
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->colors.at("text"), lightHex(ThemeColors::Key::TEXT));
    EXPECT_EQ(table->colors.at("textDim"), lightHex(ThemeColors::Key::TEXT_DIM));
    EXPECT_EQ(table->colors.at("warning"), lightHex(ThemeColors::Key::WARNING));
    EXPECT_EQ(table->colors.at("error"), lightHex(ThemeColors::Key::ERROR));
    EXPECT_EQ(table->colors.at("info"), lightHex(ThemeColors::Key::INFO));
}

/// The names of the pins' designs (none when they cannot be read: ComeFromTheGoldensOpenRocket
/// fails then).
[[nodiscard]] std::vector<std::string> pinnedDesigns()
{
    std::vector<std::string>                      names;
    const Result<Gui::Test::SimulationTablePins>& loaded = Gui::Test::simulationTablePins();
    if (loaded.has_value())
    {
        for (const Gui::Test::SimulationTablePinDesign& design : loaded->designs)
        {
            names.push_back(design.name);
        }
    }
    return names;
}

class SimulationTableModelGolden : public Gui::Test::GuiTest,
                                   public ::testing::WithParamInterface<std::string>
{
protected:
    /// The pinned design of the test's parameter (null and a failure when there is none).
    [[nodiscard]] static const Gui::Test::SimulationTablePinDesign* design()
    {
        const Gui::Test::SimulationTablePins*      table = pins();
        const Gui::Test::SimulationTablePinDesign* found =
            table == nullptr ? nullptr : table->findDesign(GetParam());
        if (table != nullptr && found == nullptr)
        {
            ADD_FAILURE() << "no pins of " << GetParam();
        }
        return found;
    }

    /// The design of @p pin, loaded as the probe loaded it, in a table.
    [[nodiscard]] static std::unique_ptr<Table> tableOf(
        const Gui::Test::SimulationTablePinDesign& pin)
    {
        if (pin.kind == "new")
        {
            return std::make_unique<Table>(OpenRocketDocumentFactory::createNewRocket());
        }
        std::unique_ptr<OpenRocketDocument> document = loadExample(pin.file);
        return document == nullptr ? nullptr : std::make_unique<Table>(std::move(document));
    }
};

TEST_P(SimulationTableModelGolden, ShowsEveryCellAsOpenRocket)
{
    const Gui::Test::SimulationTablePinDesign* pin = design();
    ASSERT_NE(pin, nullptr);
    const std::unique_ptr<Table> table = tableOf(*pin);
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->document().getSimulationCount(), pin->simulationCount);
    expectRows(*table, pin->simulations);
}

TEST_P(SimulationTableModelGolden, SortsEachColumnAsOpenRocket)
{
    const Gui::Test::SimulationTablePinDesign* pin = design();
    ASSERT_NE(pin, nullptr);
    const std::unique_ptr<Table> table = tableOf(*pin);
    ASSERT_NE(table, nullptr);
    // Every column, the warnings column as QtRocket sorts it (bug 23).
    EXPECT_EQ(pin->orders.size(), SimulationColumns::kAllColumns.size() + 1);
    expectOrders(*table, pin->orders);
}

/// The name of a design's tests: the design's name in the pins ("example_a_simple_model_rocket").
[[nodiscard]] std::string designTestName(const ::testing::TestParamInfo<std::string>& param)
{
    return param.param;
}

INSTANTIATE_TEST_SUITE_P(Designs, SimulationTableModelGolden, ::testing::ValuesIn(pinnedDesigns()),
                         designTestName);

// ------------------------------------------------------------------------------ the scenarios

/// A scenario of the pins replayed on its design: each step compared with OpenRocket's.
class ScenarioReplay
{
public:
    ScenarioReplay(const Table& table, const Gui::Test::SimulationTablePinScenario& pin)
      : m_table(&table), m_pin(&pin)
    {
    }

    /// The step just made, named @p what, left the statuses, the undo and redo descriptions and
    /// the saved state OpenRocket's had.
    void step(const std::string& what)
    {
        if (m_next >= m_pin->steps.size())
        {
            ADD_FAILURE() << "the pins have no step " << what;
            return;
        }
        const Gui::Test::SimulationTablePinStep& pin = m_pin->steps.at(m_next++);
        SCOPED_TRACE(testing::Message() << "step " << what);
        EXPECT_EQ(what, pin.step);
        EXPECT_EQ(m_table->statuses(), pin.statuses);
        EXPECT_EQ(m_table->document().getUndoDescription(), pin.undo);
        EXPECT_EQ(m_table->document().getRedoDescription(), pin.redo);
        EXPECT_EQ(m_table->document().isSaved(), pin.saved);
    }

    /// Every step of the pins was made.
    void finish() const { EXPECT_EQ(m_next, m_pin->steps.size()); }

private:
    const Table*                                 m_table;
    const Gui::Test::SimulationTablePinScenario* m_pin;
    std::size_t                                  m_next = 0;
};

/// The first component of @p rocket of the type @p T (null when there is none).
template <class T>
[[nodiscard]] T* firstOf(Rocket& rocket)
{
    for (RocketComponent& component : rocket.subtree())
    {
        if (auto* found = dynamic_cast<T*>(&component); found != nullptr)
        {
            return found;
        }
    }
    return nullptr;
}

/// The first motor mount of @p rocket that is one and has a motor.
[[nodiscard]] MotorMount* firstMountWithMotor(Rocket& rocket)
{
    for (RocketComponent& component : rocket.subtree())
    {
        if (auto* mount = dynamic_cast<MotorMount*>(&component);
            mount != nullptr && mount->isMotorMount() && mount->hasMotor())
        {
            return mount;
        }
    }
    return nullptr;
}

/// The pins' scenario named @p name (null and a failure when there is none).
[[nodiscard]] const Gui::Test::SimulationTablePinScenario* scenario(std::string_view name)
{
    const Gui::Test::SimulationTablePins* table = pins();
    if (table == nullptr)
    {
        return nullptr;
    }
    const Gui::Test::SimulationTablePinScenario* found = table->findScenario(name);
    if (found == nullptr)
    {
        ADD_FAILURE() << "no pinned scenario " << name;
    }
    return found;
}

/// The simulations of @p document as the scenario's SIM lines: their names and configurations'
/// names and the first 8 characters of their configurations' ids.
void expectInitial(const OpenRocketDocument&                    document,
                   const Gui::Test::SimulationTablePinScenario& pin)
{
    ASSERT_EQ(document.getSimulationCount(), pin.initial.size());
    for (std::size_t i = 0; i < pin.initial.size(); ++i)
    {
        const Simulation& simulation = *document.getSimulation(i);
        EXPECT_EQ(simulation.getName(), pin.initial[i].name);
        EXPECT_EQ(simulation.getId().toString().substr(0, 8), pin.initial[i].configurationId);
        EXPECT_EQ(document.getRocket()
                      .getFlightConfiguration(simulation.getId())
                      .getName(examplePreferences()),
                  pin.initial[i].configuration);
    }
}

/// The options' changes of the scenario's first steps on "Simulation 1", the configurations' of
/// "Simulation 2" and "Simulation 3", and the body tube's with its undo and redo.
void editsBeforeTheRun(const Table& table, ScenarioReplay& replay)
{
    OpenRocketDocument& document  = table.document();
    Rocket&             rocket    = document.getRocket();
    Simulation&         first     = *document.getSimulation(0);
    const double        rodLength = first.getOptions().getLaunchRodLength();
    first.getOptions().setLaunchRodLength(rodLength);
    replay.step("sim0 rod length set to the same value");
    first.getOptions().setLaunchRodLength(1.5);
    replay.step("sim0 rod length 1.5");
    first.getOptions().setLaunchRodLength(rodLength);
    replay.step("sim0 rod length back");
    const FlightConfigurationId id1 = document.getSimulation(1)->getId();
    rocket.getFlightConfiguration(id1).setName("Renamed {motors}");
    rocket.fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange, id1);
    replay.step("rename sim1's configuration");
    rocket.setSelectedConfiguration(document.getSimulation(2)->getId());
    replay.step("select sim2's configuration");
    MotorMount* mount = firstMountWithMotor(rocket);
    ASSERT_NE(mount, nullptr);
    MotorConfiguration& motor = mount->getMotorConfig(id1);
    motor.setEjectionDelay(motor.getEjectionDelay() + 1);
    rocket.fireComponentChangeEvent(ComponentChangeEvent::kMotorChange, id1);
    replay.step("sim1's ejection delay +1 (MOTOR_CHANGE for its id)");
    document.addUndoPosition("Modify body tube");
    auto* tube = firstOf<BodyTube>(rocket);
    ASSERT_NE(tube, nullptr);
    tube->setLength(tube->getLength() + 0.01);
    replay.step("body tube +1 cm");
    document.undo();
    replay.step("undo");
    document.redo();
    replay.step("redo");
    // The undo and redo replaced the components.
    tube = firstOf<BodyTube>(rocket);
    ASSERT_NE(tube, nullptr);
    tube->setColor(Color(10, 20, 30));
    replay.step("body tube colour (GRAPHIC)");
}

/// The scenario's run of "Simulation 1" (the one fresh flight QtRocket's tests may make of the
/// design), a time step, a new simulation with its undo and redo, and a run that aborts.
void runsAndNewSimulation(const Table& table, ScenarioReplay& replay)
{
    OpenRocketDocument& document = table.document();
    Rocket&             rocket   = document.getRocket();
    ASSERT_TRUE(document.getSimulation(0)->simulate().has_value());
    replay.step("sim0 simulated");
    document.getSimulation(0)->getOptions().setTimeStep(0.02);
    replay.step("sim0 time step 0.02");
    const auto added = std::make_shared<Simulation>(&document, rocket);
    added->setName(document.getNextSimulationName());
    document.addUndoPosition("Add " + added->getName());
    document.addSimulation(added);
    replay.step("new simulation " + added->getName() + " config=" +
                rocket.getFlightConfiguration(added->getId()).getName(examplePreferences()));
    document.undo();
    replay.step("undo add");
    document.redo();
    replay.step("redo add");
    const FlightConfigurationId id0   = document.getSimulation(0)->getId();
    MotorMount*                 mount = firstMountWithMotor(rocket);
    ASSERT_NE(mount, nullptr);
    mount->getMotorConfig(id0).setIgnitionEvent(IgnitionEvent::NEVER);
    rocket.fireComponentChangeEvent(ComponentChangeEvent::kMotorChange, id0);
    replay.step("sim0's motor never ignites (MOTOR_CHANGE for its id)");
    ASSERT_TRUE(document.getSimulation(0)->simulate().has_value());
    replay.step("sim0 simulated");
}

/// The second scenario's edits: the motors of "Simulation 1"'s configuration taken out, its
/// options changed, the motors of "Simulation 2"'s configuration made never to ignite.
void twoStageEdits(const Table& table, ScenarioReplay& replay)
{
    OpenRocketDocument&         document = table.document();
    Rocket&                     rocket   = document.getRocket();
    const FlightConfigurationId id0      = document.getSimulation(0)->getId();
    for (RocketComponent& component : rocket.subtree())
    {
        if (auto* mount = dynamic_cast<MotorMount*>(&component);
            mount != nullptr && mount->isMotorMount())
        {
            mount->setMotorConfig(std::nullopt, id0);
        }
    }
    rocket.fireComponentChangeEvent(ComponentChangeEvent::kMotorChange, id0);
    replay.step("motors of sim0's configuration removed");
    document.getSimulation(0)->getOptions().setLaunchRodLength(2.0);
    replay.step("sim0 rod length 2.0");
    const FlightConfigurationId id1 = document.getSimulation(1)->getId();
    for (RocketComponent& component : rocket.subtree())
    {
        if (auto* mount = dynamic_cast<MotorMount*>(&component);
            mount != nullptr && mount->isMotorMount() && !mount->getMotorConfig(id1).isEmpty())
        {
            mount->getMotorConfig(id1).setIgnitionEvent(IgnitionEvent::NEVER);
        }
    }
    rocket.fireComponentChangeEvent(ComponentChangeEvent::kMotorChange, id1);
    replay.step("sim1's motors never ignite");
}

using SimulationTableScenarioGolden = Gui::Test::GuiTest;

TEST_F(SimulationTableScenarioGolden, ShowsEveryStatusOfASimpleModelRocketsEditsAsOpenRocket)
{
    const Gui::Test::SimulationTablePinScenario* pin = scenario("status_a_simple_model_rocket");
    ASSERT_NE(pin, nullptr);
    std::unique_ptr<OpenRocketDocument> document = loadExample(pin->file);
    ASSERT_NE(document, nullptr);
    expectInitial(*document, *pin);
    Table          table(std::move(document));
    ScenarioReplay replay(table, *pin);
    replay.step("loaded");
    editsBeforeTheRun(table, replay);
    runsAndNewSimulation(table, replay);
    replay.finish();
    expectRows(table, pin->simulations);
    // The status column's orders (the aborted "Simulation 1" sorts by its abort's text).
    expectOrders(table, pin->orders);
}

TEST_F(SimulationTableScenarioGolden, ShowsCantRunAndOutdatedOfATwoStageRocketAsOpenRocket)
{
    const Gui::Test::SimulationTablePinScenario* pin =
        scenario("status_two_stage_high_power_rocket");
    ASSERT_NE(pin, nullptr);
    std::unique_ptr<OpenRocketDocument> document = loadExample(pin->file);
    ASSERT_NE(document, nullptr);
    expectInitial(*document, *pin);
    Table          table(std::move(document));
    ScenarioReplay replay(table, *pin);
    replay.step("loaded");
    twoStageEdits(table, replay);
    replay.finish();
    expectRows(table, pin->simulations);
    expectOrders(table, pin->orders);
}

}  // namespace

}  // namespace QtRocket::Gui
