// The wind editor's table model (OpenRocket's MultiLevelWindTable): its header, rows, cells,
// editing, bounds and actions replay the probe tools/openrocket-probes/wind-editor
// (WindEditorPins.h), which drives Java's table over the same levels, with QtRocket's documented
// fixes of OpenRocket bugs 31 (no two levels at one altitude, a row deleted by its index) and 32
// (the model's levels sorted again after an altitude edit) as edits of the pins; and it follows
// its wind model, under QAbstractItemModelTester, also when the model goes.

#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableModel.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QAbstractItemModelTester>
#include <QColor>
#include <QIcon>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QVariant>
#include <Qt>
#include <QtCore/qtestsupport_core.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"
#include "dialogs/simulation/WindEditorPins.h"
#include "dialogs/simulation/WindEditorTranscript.h"

namespace QtRocket::Gui
{

namespace
{

using MultiLevelWindTableModelTest = Gui::Test::GuiTest;
using Model                        = MultiLevelWindTableModel;
using Level                        = Model::Level;
using Setup                        = std::function<void(MultiLevelPinkNoiseWindModel&)>;
namespace Pins                     = Gui::Test::WindEditorPins;

/// The prefixes of the probe's lines about the table (its TIP and WALK lines are the profile's:
/// WindProfilePlotTests).
constexpr std::array<std::string_view, 5> kTableLines{"HEADER ", "ROW ", "MODEL", "STEP ",
                                                      "SORTED "};

/// Adds the level @p altitude, @p speed, @p direction, @p deviation to @p model (a test failure
/// when the model refuses it).
void addWindLevel(MultiLevelPinkNoiseWindModel& model, double altitude, double speed,
                  double direction, double deviation)
{
    EXPECT_TRUE(model.addWindLevel(altitude, speed, direction, deviation).has_value()) << altitude;
}

/// The probe's threeLevels(): 0, 500 and 1250.5 m.
void threeLevels(MultiLevelPinkNoiseWindModel& model)
{
    model.clearLevels();
    addWindLevel(model, 0, 3, std::numbers::pi / 2, 0.3);
    addWindLevel(model, 500, 7.5, std::numbers::pi, 1.0);
    addWindLevel(model, 1250.5, 12.25, 0.0, 3.0);
}

/// The table of a wind model of fresh preferences, which @p setup gives its levels before the
/// table is made, under QAbstractItemModelTester.
class WindTable
{
public:
    explicit WindTable(const Setup& setup = {})
      : m_table(prepared(m_model, setup), m_preferences, nullptr),
        m_tester(&m_table, QAbstractItemModelTester::FailureReportingMode::Warning)
    {
    }

    [[nodiscard]] InMemoryPreferences&          preferences() noexcept { return m_preferences; }
    [[nodiscard]] MultiLevelPinkNoiseWindModel& model() noexcept { return m_model; }
    [[nodiscard]] Model&                        table() noexcept { return m_table; }

    /// The probe's ROW and MODEL lines.
    [[nodiscard]] std::vector<std::string> rows() const
    {
        return Gui::Test::windTableRowLines(m_table, m_model);
    }

    /// The probe's type(): @p typed set as the text of the cell @p row, @p column, then the STEP,
    /// ROW and MODEL lines.
    void type(std::vector<std::string>& lines, int row, int column, const std::string& typed)
    {
        m_table.setData(m_table.index(row, column), QtStrings::fromUtf8(typed), Qt::EditRole);
        lines.push_back("STEP row " + std::to_string(row) + " column " + std::to_string(column) +
                        " typed=[" + typed + "]");
        append(lines, rows());
    }

    /// The probe's clickDelete(): the row's delete button, then the STEP, ROW and MODEL lines.
    void clickDelete(std::vector<std::string>& lines, int row)
    {
        m_table.deleteLevel(row);
        lines.push_back("STEP delete button of row " + std::to_string(row));
        append(lines, rows());
    }

    /// The probe's addRow() step: the STEP, ROW and MODEL lines.
    void add(std::vector<std::string>& lines)
    {
        m_table.addLevel();
        lines.emplace_back("STEP addRow");
        append(lines, rows());
    }

    static void append(std::vector<std::string>& lines, const std::vector<std::string>& more)
    {
        lines.insert(lines.end(), more.begin(), more.end());
    }

private:
    static MultiLevelPinkNoiseWindModel& prepared(MultiLevelPinkNoiseWindModel& model,
                                                  const Setup&                  setup)
    {
        if (setup)
        {
            setup(model);
        }
        return model;
    }

    InMemoryPreferences          m_preferences;
    MultiLevelPinkNoiseWindModel m_model{m_preferences};
    Model                        m_table;
    QAbstractItemModelTester     m_tester;
};

/// A unit of @p group named @p name (a test failure, and the default unit, when there is none).
[[nodiscard]] const Unit& unitNamed(UnitGroupId group, std::string_view name)
{
    const Unit* unit = unitGroup(group).getUnit(name);
    if (unit == nullptr)
    {
        ADD_FAILURE() << "no unit " << name;
        return unitGroup(group).getDefaultUnit();
    }
    return *unit;
}

/// The probe's units(): every number column's unit set, then the STEP, HEADER, ROW and MODEL
/// lines.
void setUnits(WindTable& table, std::vector<std::string>& lines,
              const std::array<std::string_view, Model::kNumberColumnCount>& names)
{
    constexpr std::array<UnitGroupId, Model::kNumberColumnCount> kGroups{
        UnitGroupId::DISTANCE, UnitGroupId::WINDSPEED, UnitGroupId::ANGLE, UnitGroupId::WINDSPEED,
        UnitGroupId::RELATIVE};
    for (int column = 0; column < Model::kNumberColumnCount; ++column)
    {
        const auto at = static_cast<std::size_t>(column);
        table.table().setColumnUnit(column, unitNamed(kGroups.at(at), names.at(at)));
    }
    lines.push_back("STEP units altitude=[" + std::string(names[0]) + "] speed=[" +
                    std::string(names[1]) + "] direction=[" + std::string(names[2]) +
                    "] deviation=[" + std::string(names[3]) + "] turbulence=[" +
                    std::string(names[4]) + "]");
    WindTable::append(lines, Gui::Test::windTableHeaderLines(table.table()));
    WindTable::append(lines, table.rows());
}

/// The probe's "three levels" section without its TIP and WALK lines.
[[nodiscard]] std::vector<std::string> threeLevelLines(WindTable& table)
{
    std::vector<std::string> lines = table.rows();
    table.model().setAltitudeReference(WindModel::AltitudeReference::AGL);
    lines.emplace_back("STEP AGL");
    WindTable::append(lines, Gui::Test::windTableHeaderLines(table.table()));
    table.model().setAltitudeReference(WindModel::AltitudeReference::MSL);
    lines.emplace_back("STEP MSL");
    WindTable::append(lines, Gui::Test::windTableHeaderLines(table.table()));
    return lines;
}

/// The probe's "the units" section without its TIP lines.
[[nodiscard]] std::vector<std::string> unitLines(WindTable& table)
{
    std::vector<std::string> lines;
    setUnits(table, lines, {"ft", "mph", "rad", "km/h", "\u2030"});
    setUnits(table, lines, {"m", "m/s", "\u00B0", "m/s", "%"});
    return lines;
}

/// The probe's "the cells' bounds and couplings" section.
[[nodiscard]] std::vector<std::string> cellBoundLines(WindTable& table)
{
    struct Typed
    {
        int         row;
        int         column;
        std::string text;
    };
    const std::vector<Typed> steps{
        {.row = 0, .column = 0, .text = "-200"}, {.row = 0, .column = 0, .text = "0"},
        {.row = 0, .column = 1, .text = "-5"},   {.row = 0, .column = 1, .text = "6"},
        {.row = 0, .column = 2, .text = "400"},  {.row = 0, .column = 2, .text = "-30"},
        {.row = 0, .column = 2, .text = "90"},   {.row = 0, .column = 3, .text = "10"},
        {.row = 0, .column = 3, .text = "1.2"},  {.row = 0, .column = 4, .text = "150"},
        {.row = 0, .column = 4, .text = "4"},    {.row = 0, .column = 4, .text = "12"},
        {.row = 0, .column = 4, .text = "22"},   {.row = 0, .column = 1, .text = "0"},
        {.row = 0, .column = 1, .text = "3"},    {.row = 1, .column = 0, .text = "2*300"},
        {.row = 1, .column = 0, .text = "abc"},
    };
    std::vector<std::string> lines;
    for (const Typed& step : steps)
    {
        table.type(lines, step.row, step.column, step.text);
    }
    return lines;
}

/// Whether @p model's levels are sorted by altitude.
[[nodiscard]] bool isSorted(const MultiLevelPinkNoiseWindModel& model)
{
    const std::vector<const Level*> levels = model.getLevels();
    return std::ranges::is_sorted(
        levels, [](const Level* a, const Level* b) { return a->getAltitude() < b->getAltitude(); });
}

/// @p lines with the line @p from replaced by @p to (a test failure when it has none).
[[nodiscard]] std::vector<std::string> replaced(std::vector<std::string> lines,
                                                std::string_view from, std::string_view to)
{
    const auto found = std::ranges::find(lines, from);
    if (found == lines.end())
    {
        ADD_FAILURE() << "no line " << from;
        return lines;
    }
    *found = std::string(to);
    return lines;
}

/// The probe's "add a level" section.
[[nodiscard]] std::vector<std::string> addLines(WindTable& table)
{
    std::vector<std::string> lines;
    table.add(lines);
    return lines;
}

/// The probe's "an altitude edit that reorders the rows" section: row 0's altitude 800 m, then
/// whether the model's levels are sorted (SORTED).
[[nodiscard]] std::vector<std::string> reorderLines(WindTable& table)
{
    std::vector<std::string> lines;
    table.type(lines, 0, 0, "800");
    lines.push_back(std::string("SORTED ") + (isSorted(table.model()) ? "true" : "false"));
    return lines;
}

/// The probe's "the delete buttons" section: the delete button of row @p row.
[[nodiscard]] std::vector<std::string> deleteLines(WindTable& table, int row)
{
    std::vector<std::string> lines;
    table.clickDelete(lines, row);
    return lines;
}

/// The probe's "reset levels" section.
[[nodiscard]] std::vector<std::string> resetLines(WindTable& table)
{
    table.table().resetLevels();
    std::vector<std::string> lines{"STEP resetLevels"};
    WindTable::append(lines, table.rows());
    return lines;
}

/// The setup of a model whose levels are out of order (threeLevels() with the first at 800 m)
/// and have a listener before the table (as SimulationOptions' forwarding of the model's changes
/// is): it records the levels (windModelLine()) at every change the model announces from then on.
struct ListenedOutOfOrderLevels
{
    std::vector<std::string>* heard;

    void operator()(MultiLevelPinkNoiseWindModel& model) const
    {
        threeLevels(model);
        model.getLevels().front()->setAltitude(800);
        // The connection goes with the model.
        model.changed().connect(
            [lines = heard, &model] { lines->push_back(Gui::Test::windModelLine(model)); });
    }
};

/// Whether the cell @p row, @p column of @p table has @p flag.
[[nodiscard]] bool cellHas(const Model& table, int row, int column, Qt::ItemFlag flag)
{
    return table.flags(table.index(row, column)).testFlag(flag);
}

/// The three levels in a wind model of the test's own, which it may destroy.
[[nodiscard]] std::unique_ptr<MultiLevelPinkNoiseWindModel> ownModel(
    const InMemoryPreferences& preferences)
{
    auto model = std::make_unique<MultiLevelPinkNoiseWindModel>(preferences);
    threeLevels(*model);
    return model;
}

}  // namespace

TEST_F(MultiLevelWindTableModelTest, TheTableOfTheDefaultLevelIsOpenRockets)
{
    WindTable                table;
    std::vector<std::string> lines = Gui::Test::windTableHeaderLines(table.table());
    WindTable::append(lines, table.rows());
    PinkNoiseWindModel average;
    average.loadFrom(table.preferences());
    lines.push_back("PREFERENCES average=" + Strings::javaDoubleToString(average.getAverage()) +
                    " direction=" + Strings::javaDoubleToString(average.getDirection()) +
                    " deviation=" + Strings::javaDoubleToString(average.getStandardDeviation()));
    EXPECT_EQ(lines, Gui::Test::windPinLines(Pins::kDefaultModel));
}

TEST_F(MultiLevelWindTableModelTest, TheProbesStepsShowWhatOpenRocketShows)
{
    WindTable table(threeLevels);
    EXPECT_EQ(threeLevelLines(table), Gui::Test::windPinLinesWith(Pins::kThreeLevels, kTableLines));
    EXPECT_EQ(unitLines(table), Gui::Test::windPinLinesWith(Pins::kUnits, kTableLines));
    EXPECT_EQ(cellBoundLines(table), Gui::Test::windPinLines(Pins::kCellBounds));
    EXPECT_EQ(addLines(table), Gui::Test::windPinLines(Pins::kAddLevel));
    // OpenRocket bug 32: Java's model keeps the edited level first ("MODEL 800.0 600.0 1250.5
    // 1350.5") while its table shows the rows sorted; QtRocket sorts the model's levels too.
    EXPECT_EQ(reorderLines(table), replaced(replaced(Gui::Test::windPinLines(Pins::kReorder),
                                                     "MODEL 800.0 600.0 1250.5 1350.5",
                                                     "MODEL 600.0 800.0 1250.5 1350.5"),
                                            "SORTED false", "SORTED true"));
    EXPECT_EQ(deleteLines(table, 1), Gui::Test::windPinLines(Pins::kDeleteButton));
    // Java's Reset levels leaves the lone level's delete button enabled (its rows' buttons are
    // set only when a row is added or deleted); QtRocket disables it, as for any lone level.
    EXPECT_EQ(resetLines(table),
              replaced(Gui::Test::windPinLines(Pins::kResetLevels),
                       "ROW 0 [0] [2] [90] [0.2] [10] intensity=[Medium] delete=enabled",
                       "ROW 0 [0] [2] [90] [0.2] [10] intensity=[Medium] delete=disabled"));
}

TEST_F(MultiLevelWindTableModelTest, AnAltitudeAnotherLevelHasIsRefused)
{
    // OpenRocket bug 31: Java takes the altitude (MODEL 0.0 0.0 1250.5) and its delete then
    // removes both levels at 0 m (MODEL 1250.5); the design saved with the two cannot be opened.
    WindTable                table(threeLevels);
    std::vector<std::string> lines = table.rows();
    table.type(lines, 1, 0, "0");
    table.clickDelete(lines, 0);
    const std::vector<std::string> pins = Gui::Test::windPinLines(Pins::kDuplicateAltitude);
    ASSERT_EQ(pins.size(), 13U);
    EXPECT_EQ(std::vector<std::string>(lines.begin(), lines.begin() + 4),
              std::vector<std::string>(pins.begin(), pins.begin() + 4));
    EXPECT_EQ(lines, (std::vector<std::string>{
                         pins.at(0),
                         pins.at(1),
                         pins.at(2),
                         pins.at(3),
                         "STEP row 1 column 0 typed=[0]",
                         pins.at(0),
                         pins.at(1),
                         pins.at(2),
                         pins.at(3),
                         "STEP delete button of row 0",
                         "ROW 0 [500] [7.5] [180] [1] [13.3] intensity=[Medium] delete=enabled",
                         "ROW 1 [1250] [12.2] [0] [3] [24.5] intensity=[Very high] delete=enabled",
                         "MODEL 500.0 1250.5",
                     }));
}

TEST_F(MultiLevelWindTableModelTest, TheLastLevelCannotBeDeleted)
{
    WindTable                table([](MultiLevelPinkNoiseWindModel& model) {
        model.clearLevels();
        addWindLevel(model, 0, 2, 0, 0.1);
        addWindLevel(model, 100, 4, 1, 0.2);
    });
    std::vector<std::string> lines = table.rows();
    table.clickDelete(lines, 0);
    table.clickDelete(lines, 0);
    EXPECT_EQ(lines, Gui::Test::windPinLines(Pins::kDeleteDownToOne));
    EXPECT_FALSE(table.table().deleteLevel(0));
    EXPECT_EQ(table.model().getLevels().size(), 1U);
}

TEST_F(MultiLevelWindTableModelTest, AddingToOneLevelCopiesItsWind)
{
    WindTable                table([](MultiLevelPinkNoiseWindModel& model) {
        model.clearLevels();
        addWindLevel(model, -50, 5.5, 2, 0.5);
    });
    std::vector<std::string> lines = table.rows();
    table.add(lines);
    EXPECT_EQ(lines, Gui::Test::windPinLines(Pins::kAddToOneLevel));
}

TEST_F(MultiLevelWindTableModelTest, AddingWithoutLevelsTakesThePreferencesSpeed)
{
    // Java: altitude 0, the preferences' average speed, direction 0, deviation 0.
    WindTable table([](MultiLevelPinkNoiseWindModel& model) { model.clearLevels(); });
    table.preferences().setWindAverage(4.5);
    EXPECT_EQ(table.table().addLevel(), 0);
    ASSERT_EQ(table.model().getLevels().size(), 1U);
    const Level& level = *table.model().getLevels().front();
    EXPECT_EQ(level.getAltitude(), 0.0);
    EXPECT_EQ(level.getSpeed(), 4.5);
    EXPECT_EQ(level.getDirection(), 0.0);
    EXPECT_EQ(level.getStandardDeviation(), 0.0);
}

TEST_F(MultiLevelWindTableModelTest, NoLevelIsAddedBeyondTheInputRanges)
{
    // QtRocket's: a level the run would refuse (above 1000000 m) is not added.
    WindTable table([](MultiLevelPinkNoiseWindModel& model) {
        model.clearLevels();
        addWindLevel(model, 999950, 5, 0, 0);
    });
    EXPECT_EQ(table.table().addLevel(), -1);
    EXPECT_EQ(table.model().getLevels().size(), 1U);
}

TEST_F(MultiLevelWindTableModelTest, TheCellsAreBoundedAsJavasButTheirMaximaAreTheInputRanges)
{
    WindTable table(threeLevels);
    Model&    model = table.table();
    // Java: no maximum of the altitude and the speed (U-M4-7: SimulationInputRanges').
    EXPECT_TRUE(model.setData(model.index(2, Model::kAltitudeColumn), QStringLiteral("2000000"),
                              Qt::EditRole));
    EXPECT_EQ(table.model().getLevels().at(2)->getAltitude(), 1000000.0);
    EXPECT_TRUE(
        model.setData(model.index(2, Model::kSpeedColumn), QStringLiteral("5000"), Qt::EditRole));
    EXPECT_EQ(table.model().getLevels().at(2)->getSpeed(), 1000.0);
    // Another role or column, or text that is no expression, sets nothing.
    EXPECT_FALSE(model.setData(model.index(0, Model::kSpeedColumn), 5.0, Qt::DisplayRole));
    EXPECT_FALSE(
        model.setData(model.index(0, Model::kIntensityColumn), QStringLiteral("1"), Qt::EditRole));
    EXPECT_FALSE(
        model.setData(model.index(0, Model::kSpeedColumn), QStringLiteral("x"), Qt::EditRole));
    EXPECT_EQ(table.model().getLevels().front()->getSpeed(), 3.0);
}

TEST_F(MultiLevelWindTableModelTest, ACellBindingEditsItsLevelInTheColumnsUnit)
{
    WindTable table(threeLevels);
    Model&    model = table.table();
    model.setColumnUnit(Model::kDeviationColumn, unitNamed(UnitGroupId::WINDSPEED, "km/h"));
    const std::shared_ptr<DoubleBinding> deviation =
        model.makeCellBinding(1, Model::kDeviationColumn);
    ASSERT_NE(deviation, nullptr);
    EXPECT_EQ(deviation->getCurrentUnit().getUnit(), "km/h");
    EXPECT_EQ(deviation->text(), "3.6");
    // At most the level's speed (Java: the speed's DoubleModel as the maximum).
    EXPECT_EQ(deviation->maximum(), 7.5);
    EXPECT_TRUE(deviation->commitText("100"));
    EXPECT_EQ(table.model().getLevels().at(1)->getStandardDeviation(), 7.5);
    EXPECT_EQ(model.cellText(1, Model::kTurbulenceColumn), QStringLiteral("100"));
    // Neither the other columns nor a row that is not there have a binding.
    EXPECT_EQ(model.makeCellBinding(1, Model::kIntensityColumn), nullptr);
    EXPECT_EQ(model.makeCellBinding(1, Model::kDeleteColumn), nullptr);
    EXPECT_EQ(model.makeCellBinding(3, Model::kSpeedColumn), nullptr);
}

TEST_F(MultiLevelWindTableModelTest, ABindingOfALevelThatWentWritesNothing)
{
    WindTable                            table(threeLevels);
    const std::shared_ptr<DoubleBinding> speed =
        table.table().makeCellBinding(0, Model::kSpeedColumn);
    ASSERT_NE(speed, nullptr);
    ASSERT_TRUE(table.table().deleteLevel(0));
    EXPECT_FALSE(speed->isValid());
    EXPECT_FALSE(speed->commitText("9"));
    EXPECT_EQ(Gui::Test::windModelLine(table.model()), "MODEL 500.0 1250.5");
}

TEST_F(MultiLevelWindTableModelTest, TheHeaderFollowsTheAltitudeReference)
{
    WindTable        table(threeLevels);
    const QSignalSpy headers(&table.table(), &Model::headerDataChanged);
    table.model().setAltitudeReference(WindModel::AltitudeReference::AGL);
    ASSERT_EQ(headers.count(), 1);
    EXPECT_EQ(headers.at(0).at(1).toInt(), Model::kAltitudeColumn);
    EXPECT_EQ(table.table().headerData(Model::kAltitudeColumn, Qt::Horizontal, Qt::DisplayRole),
              QStringLiteral("Altitude AGL"));
    // A change of a level is no change of the header.
    table.model().getLevels().front()->setSpeed(4);
    EXPECT_EQ(headers.count(), 1);
    EXPECT_EQ(Model::columnName(Model::kAltitudeColumn, WindModel::AltitudeReference::MSL),
              QStringLiteral("Altitude MSL"));
    EXPECT_THROW(static_cast<void>(
                     Model::columnName(Model::kColumnCount, WindModel::AltitudeReference::MSL)),
                 BugError);
}

TEST_F(MultiLevelWindTableModelTest, AColumnTakesOnlyAUnitOfItsGroup)
{
    WindTable        table(threeLevels);
    const QSignalSpy units(&table.table(), &Model::columnUnitChanged);
    table.table().setColumnUnit(Model::kAltitudeColumn, unitNamed(UnitGroupId::DISTANCE, "ft"));
    ASSERT_EQ(units.count(), 1);
    EXPECT_EQ(units.at(0).at(0).toInt(), Model::kAltitudeColumn);
    EXPECT_EQ(table.table().cellText(1, Model::kAltitudeColumn), QStringLiteral("1640"));
    EXPECT_THROW(
        table.table().setColumnUnit(Model::kSpeedColumn, unitNamed(UnitGroupId::DISTANCE, "ft")),
        BugError);
    EXPECT_THROW(static_cast<void>(table.table().columnUnit(Model::kIntensityColumn)), BugError);
    EXPECT_THROW(static_cast<void>(Model::columnUnitGroup(Model::kDeleteColumn)), BugError);
}

TEST_F(MultiLevelWindTableModelTest, TheModelsLevelsAreSortedFromTheStart)
{
    // OpenRocket bug 32: levels out of order when the table is made are sorted as well.
    WindTable table([](MultiLevelPinkNoiseWindModel& model) {
        threeLevels(model);
        model.getLevels().front()->setAltitude(800);
    });
    EXPECT_EQ(Gui::Test::windModelLine(table.model()), "MODEL 500.0 800.0 1250.5");
    EXPECT_EQ(table.table().cellText(1, Model::kAltitudeColumn), QStringLiteral("800"));
}

TEST_F(MultiLevelWindTableModelTest, TheSortIsAnnouncedToTheModelsEarlierListeners)
{
    // OpenRocket bug 32: the table sorts the levels in its own slot on the model's changed(),
    // after the listeners connected before it have heard of the change; it announces the new
    // order (the model's fireChangeEvent()), so that they read it too.
    std::vector<std::string> heard;
    WindTable                table(ListenedOutOfOrderLevels{.heard = &heard});
    Model&                   model = table.table();
    // Made over levels out of order.
    EXPECT_EQ(heard, std::vector<std::string>{"MODEL 500.0 800.0 1250.5"});
    // An altitude edit that moves its level: heard out of order, then sorted.
    heard.clear();
    ASSERT_TRUE(
        model.setData(model.index(2, Model::kAltitudeColumn), QStringLiteral("100"), Qt::EditRole));
    EXPECT_EQ(heard,
              (std::vector<std::string>{"MODEL 500.0 800.0 100.0", "MODEL 100.0 500.0 800.0"}));
    EXPECT_EQ(model.cellText(0, Model::kAltitudeColumn), QStringLiteral("100"));
    // An edit that keeps the order: heard once.
    heard.clear();
    ASSERT_TRUE(
        model.setData(model.index(0, Model::kAltitudeColumn), QStringLiteral("50"), Qt::EditRole));
    EXPECT_EQ(heard, std::vector<std::string>{"MODEL 50.0 500.0 800.0"});
    // A level moved by another than the table: the table sorts and announces at once, within the
    // model's announcement of the move.
    heard.clear();
    table.model().getLevels().front()->setAltitude(900);
    EXPECT_EQ(heard,
              (std::vector<std::string>{"MODEL 900.0 500.0 800.0", "MODEL 500.0 800.0 900.0"}));
    EXPECT_EQ(model.cellText(2, Model::kAltitudeColumn), QStringLiteral("900"));
}

TEST_F(MultiLevelWindTableModelTest, AnAltitudeEditMovesItsRowAndFlashesItsNeighbourhood)
{
    WindTable table(threeLevels);
    Model&    model = table.table();
    model.setFlashDuration(std::chrono::milliseconds(0));
    const QPersistentModelIndex edited = model.index(0, Model::kSpeedColumn);
    const QSignalSpy            layouts(&model, &Model::layoutChanged);
    ASSERT_TRUE(
        model.setData(model.index(0, Model::kAltitudeColumn), QStringLiteral("700"), Qt::EditRole));
    EXPECT_EQ(layouts.count(), 1);
    EXPECT_EQ(edited.row(), 1);
    EXPECT_EQ(model.cellText(1, Model::kAltitudeColumn), QStringLiteral("700"));
    // Java: the moved row and the rows around it flash.
    EXPECT_TRUE(model.isFlashing(0));
    EXPECT_TRUE(model.isFlashing(1));
    EXPECT_TRUE(model.isFlashing(2));
    EXPECT_EQ(model.data(model.index(1, 0), Qt::BackgroundRole).value<QColor>(),
              ThemeColors::color(ThemeColors::Key::TABLE_FLASH));
    EXPECT_TRUE(QTest::qWaitFor([&model] { return !model.isFlashing(1); }));
    EXPECT_EQ(model.data(model.index(1, 0), Qt::BackgroundRole).value<QColor>(),
              ThemeColors::color(ThemeColors::Key::ROW_DARKER));
    EXPECT_EQ(model.data(model.index(2, 0), Qt::BackgroundRole).value<QColor>(),
              ThemeColors::color(ThemeColors::Key::ROW_LIGHTER));
    // An edit that keeps the order moves nothing and flashes nothing.
    ASSERT_TRUE(
        model.setData(model.index(1, Model::kAltitudeColumn), QStringLiteral("650"), Qt::EditRole));
    EXPECT_EQ(layouts.count(), 1);
    EXPECT_FALSE(model.isFlashing(1));
}

TEST_F(MultiLevelWindTableModelTest, AddedAndDeletedRowsFlash)
{
    WindTable table(threeLevels);
    Model&    model = table.table();
    model.setFlashDuration(std::chrono::milliseconds(0));
    EXPECT_EQ(model.addLevel(), 3);
    EXPECT_TRUE(model.isFlashing(3));
    EXPECT_FALSE(model.isFlashing(2));
    EXPECT_TRUE(QTest::qWaitFor([&model] { return !model.isFlashing(3); }));
    // Java: the row that takes the deleted row's place.
    ASSERT_TRUE(model.deleteLevel(1));
    EXPECT_TRUE(model.isFlashing(1));
    EXPECT_FALSE(model.isFlashing(0));
    EXPECT_TRUE(QTest::qWaitFor([&model] { return !model.isFlashing(1); }));
    // The last row deleted: no row takes its place.
    ASSERT_TRUE(model.deleteLevel(2));
    EXPECT_FALSE(model.isFlashing(0));
    EXPECT_FALSE(model.isFlashing(1));
}

TEST_F(MultiLevelWindTableModelTest, TheRowsFollowTheModel)
{
    WindTable        table(threeLevels);
    Model&           model = table.table();
    const QSignalSpy inserted(&model, &Model::rowsInserted);
    const QSignalSpy removed(&model, &Model::rowsRemoved);
    const QSignalSpy reset(&model, &Model::modelReset);
    const QSignalSpy levels(&model, &Model::levelsChanged);
    addWindLevel(table.model(), 250, 1, 0, 0);
    EXPECT_EQ(inserted.count(), 1);
    EXPECT_EQ(model.cellText(1, Model::kAltitudeColumn), QStringLiteral("250"));
    table.model().removeWindLevel(500);
    EXPECT_EQ(removed.count(), 1);
    EXPECT_EQ(model.rowCount(), 3);
    table.model().clearLevels();
    EXPECT_EQ(removed.count(), 2);
    EXPECT_EQ(model.rowCount(), 0);
    table.model().resetLevels(table.preferences());
    EXPECT_EQ(model.rowCount(), 1);
    EXPECT_EQ(reset.count(), 0);
    EXPECT_EQ(levels.count(), 5);
}

TEST_F(MultiLevelWindTableModelTest, LevelsReplacedSilentlyShowNothingUntilTheNextChange)
{
    // loadFrom() announces nothing (as Java's): the rows' levels have gone, and they show nothing.
    WindTable                          table(threeLevels);
    const MultiLevelPinkNoiseWindModel other(table.preferences());
    table.model().loadFrom(other);
    EXPECT_EQ(table.table().rowCount(), 3);
    EXPECT_EQ(table.table().levelAt(0), nullptr);
    EXPECT_TRUE(table.table().data(table.table().index(0, 0), Qt::DisplayRole).isNull());
    EXPECT_FALSE(table.table().deleteLevel(0));
    // Inert: no editor, no delete button.
    EXPECT_FALSE(cellHas(table.table(), 0, Model::kSpeedColumn, Qt::ItemIsEditable));
    EXPECT_FALSE(cellHas(table.table(), 0, Model::kDeleteColumn, Qt::ItemIsEnabled));
    table.model().setAltitudeReference(WindModel::AltitudeReference::MSL);
    EXPECT_EQ(table.table().rowCount(), 1);
    EXPECT_EQ(
        Gui::Test::windTableRowLines(table.table(), table.model()),
        (std::vector<std::string>{
            "ROW 0 [0] [2] [90] [0.2] [10] intensity=[Medium] delete=disabled", "MODEL 0.0"}));
}

TEST_F(MultiLevelWindTableModelTest, TheTableOutlivesItsModel)
{
    const InMemoryPreferences                     preferences;
    std::unique_ptr<MultiLevelPinkNoiseWindModel> wind = ownModel(preferences);
    Model                                         model(*wind, preferences, nullptr);
    const QAbstractItemModelTester tester(&model,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    EXPECT_TRUE(cellHas(model, 0, Model::kDeleteColumn, Qt::ItemIsEnabled));
    wind.reset();
    EXPECT_FALSE(model.hasModel());
    // The rows stay (a destroyed signal calls nobody), inert.
    EXPECT_EQ(model.rowCount(), 3);
    EXPECT_EQ(model.levelAt(0), nullptr);
    EXPECT_FALSE(cellHas(model, 0, Model::kDeleteColumn, Qt::ItemIsEnabled));
    EXPECT_FALSE(cellHas(model, 1, Model::kAltitudeColumn, Qt::ItemIsEditable));
    EXPECT_TRUE(model.cellText(0, Model::kAltitudeColumn).isEmpty());
    EXPECT_EQ(model.addLevel(), -1);
    EXPECT_FALSE(model.deleteLevel(0));
    EXPECT_EQ(model.makeCellBinding(0, Model::kSpeedColumn), nullptr);
    model.resetLevels();
    EXPECT_FALSE(
        model.setData(model.index(0, Model::kSpeedColumn), QStringLiteral("1"), Qt::EditRole));
}

TEST_F(MultiLevelWindTableModelTest, TheDeleteColumnShowsItsIcon)
{
    WindTable    table(threeLevels);
    const Model& model   = table.table();
    const auto   delete0 = model.index(0, Model::kDeleteColumn);
    EXPECT_TRUE(model.data(delete0, Qt::DisplayRole).isNull());
    EXPECT_FALSE(model.data(delete0, Qt::DecorationRole).value<QIcon>().isNull());
    EXPECT_EQ(model.data(delete0, Qt::ToolTipRole),
              QStringLiteral("Delete the selected wind level"));
    EXPECT_TRUE(model.flags(delete0).testFlag(Qt::ItemIsEnabled));
    EXPECT_FALSE(model.flags(delete0).testFlag(Qt::ItemIsEditable));
    EXPECT_TRUE(model.flags(model.index(0, Model::kSpeedColumn)).testFlag(Qt::ItemIsEditable));
    EXPECT_FALSE(model.flags(model.index(0, Model::kIntensityColumn)).testFlag(Qt::ItemIsEditable));
}

}  // namespace QtRocket::Gui
