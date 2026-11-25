// The columns of the simulations table (Java: the columns SimulationPanel.java:1487-1760 gives its
// SimulationTableModel, with the adaptors of swing gui/adaptors/). OpenRocket has no JUnit test of
// them; the columns, their widths and every cell, tooltip and order of the sixteen examples are
// pinned against OpenRocket's own table (tests/data/goldens/gui/simulation-table, read by
// SimulationTablePins.h: the header-level values here, the rows in
// SimulationTableModelTests.cpp). These tests also pin what the examples do not reach: critical
// warnings, the warnings column's order (OpenRocket bug 23), an aborted simulation's description,
// a simulation without data, the comparators' rules.

#include "QtRocket/gui/simulation/SimulationColumns.h"

#include <array>
#include <compare>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <QGuiApplication>
#include <QPalette>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/unit/Value.h"
#include "QtRocket/util/BugError.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationStatusSupport.h"
#include "simulation/SimulationTablePins.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationColumnsTest = Test::GuiTest;
using Column                = SimulationColumns::Column;
using Counts                = SimulationColumns::WarningCounts;
using Status                = Simulation::Status;

/// The counts of @p critical, @p normal and @p informational warnings.
[[nodiscard]] constexpr Counts levels(std::size_t critical, std::size_t normal,
                                      std::size_t informational) noexcept
{
    return Counts{.critical = critical, .normal = normal, .informational = informational};
}

/// The pins, or a test failure.
[[nodiscard]] const Gui::Test::SimulationTablePins* pins()
{
    return Gui::Test::simulationTablePinsOrFail();
}

/// An Estes Alpha III whose selected configuration is the first test configuration (a C6).
struct Alpha
{
    QtRocket::Test::TestEstesAlphaIII alpha;

    Alpha() { alpha.rocket->setSelectedConfiguration(QtRocket::Test::testFcid(0)); }

    [[nodiscard]] Rocket& rocket() const { return *alpha.rocket; }
};

/// Flight data with only summary values (apogee 38.733 m, ...) and @p warnings in its set.
[[nodiscard]] std::shared_ptr<FlightData> summaryData(
    std::initializer_list<Warning::Other> warnings)
{
    auto data = std::make_shared<FlightData>(38.733, 24.764, 124.056, 0.073, 3.085, 12.194, 4.691,
                                             13.962, 5.886, 2.3559535841670214);
    for (const Warning::Other& warning : warnings)
    {
        data->getWarningSet().add(warning);
    }
    return data;
}

/// A simulation of @p rocket as a file describes it, named @p name, with @p status and @p data.
[[nodiscard]] std::unique_ptr<Simulation> loaded(Rocket& rocket, std::string name, Status status,
                                                 std::shared_ptr<FlightData> data)
{
    return std::make_unique<Simulation>(
        nullptr, rocket, status, std::move(name), SimulationOptions(),
        std::vector<std::shared_ptr<SimulationExtension>>{}, std::move(data));
}

/// Flight data with a branch per element of @p causes, a SIM_ABORT event in those given.
[[nodiscard]] std::shared_ptr<FlightData> abortedData(
    std::initializer_list<std::optional<SimulationAbort::Cause>> causes)
{
    std::vector<std::shared_ptr<FlightDataBranch>> branches;
    for (const std::optional<SimulationAbort::Cause>& cause : causes)
    {
        const std::shared_ptr<FlightDataBranch> branch =
            QtRocket::Test::newBranch("b" + std::to_string(branches.size()));
        if (cause.has_value())
        {
            branch->addEvent(
                FlightEvent(FlightEvent::Type::SIM_ABORT, 1.0, nullptr, SimulationAbort(*cause)));
        }
        branches.push_back(branch);
    }
    return std::make_shared<FlightData>(
        std::span<const std::shared_ptr<FlightDataBranch>>(branches));
}

/// "#rrggbb" of ThemeColors' light colour of @p key, as Java's tooltips write it.
[[nodiscard]] std::string lightHex(ThemeColors::Key key)
{
    return QtStrings::toUtf8(
        ThemeColors::themeColor(key, ThemeColors::Theme::LIGHT).name(QColor::HexRgb));
}

/// The ordering of two warning counts in the warnings column.
[[nodiscard]] std::weak_ordering compareCounts(std::optional<Counts> a, std::optional<Counts> b)
{
    const auto value = [](const std::optional<Counts>& counts) -> SimulationColumns::CellValue {
        if (!counts.has_value())
        {
            return std::monostate{};
        }
        return *counts;
    };
    return SimulationColumns::compare(Column::WARNINGS, value(a), value(b));
}

/// The ordering of two names in the name column.
[[nodiscard]] std::weak_ordering compareNames(const std::string& a, const std::string& b)
{
    return SimulationColumns::compare(Column::NAME, a, b);
}

/// The ordering of two configurations' names in the configuration column.
[[nodiscard]] std::weak_ordering compareConfigurations(const std::string& a, const std::string& b)
{
    return SimulationColumns::compare(Column::CONFIGURATION, a, b);
}

/// The text @p simulation, LOADED, shows in @p column, with @p preferences.
[[nodiscard]] QString cellTextOf(Column column, const Simulation& simulation,
                                 const Preferences& preferences)
{
    return SimulationColumns::cellText(
        column, SimulationColumns::cellValue(column, simulation, Status::LOADED, preferences));
}

/// The column @p c of SimulationColumns is OpenRocket's column @p pin.
void expectColumn(const Gui::Test::SimulationTablePinColumn& pin, std::size_t c)
{
    const Column column = SimulationColumns::columnAt(static_cast<int>(c));
    EXPECT_EQ(SimulationColumns::indexOf(column), static_cast<int>(c));
    EXPECT_EQ(SimulationColumns::id(column), pin.id);
    EXPECT_EQ(QtStrings::toUtf8(SimulationColumns::headerText(column)), pin.header) << pin.id;
    EXPECT_EQ(QtStrings::toUtf8(SimulationColumns::headerToolTip(column)), pin.toolTip) << pin.id;
    // Column.getUnits(): the value column's group, UNITS_NONE's zero-width space for the others.
    EXPECT_EQ(SimulationColumns::units(column).getDefaultUnit().getUnit(), pin.unit) << pin.id;
    EXPECT_EQ(SimulationColumns::isValueColumn(column), c >= 5) << pin.id;
}

/// Where the width rules of the view column @p width differ from those OpenRocket gave it ("" when
/// nowhere): an exact width is the preferred, minimum and maximum one; an auto-sized column's
/// minimum is its default width (its preferred one is the widest cell's plus 10); any other
/// column's preferred width is its default width.
[[nodiscard]] std::string widthDifference(const Gui::Test::SimulationTablePinWidth& width)
{
    const std::optional<Column> found = SimulationColumns::fromId(width.id);
    if (!found.has_value())
    {
        return "no column " + width.id;
    }
    const Column column = *found;
    if (const int exact = SimulationColumns::exactWidth(column); exact > 0)
    {
        const bool same =
            width.preferred == exact && width.minimum == exact && width.maximum == exact;
        return same ? std::string() : width.id + ": not exactly " + std::to_string(exact);
    }
    if (SimulationColumns::isAutoSize(column))
    {
        return width.minimum == SimulationColumns::defaultWidth(column)
                   ? std::string()
                   : width.id + ": another minimum width";
    }
    return width.preferred == SimulationColumns::defaultWidth(column)
               ? std::string()
               : width.id + ": another preferred width";
}

/// The width rules of the view column @p width are those OpenRocket gave it.
void expectWidth(const Gui::Test::SimulationTablePinWidth& width)
{
    EXPECT_EQ(widthDifference(width), "");
}

/// The value column @p column of @p simulation, which has no data, shows nothing.
void expectNoValue(Column column, const Simulation& simulation, const Preferences& preferences)
{
    EXPECT_EQ(SimulationColumns::summaryValue(column, simulation), std::nullopt);
    const SimulationColumns::CellValue value =
        SimulationColumns::cellValue(column, simulation, Status::NOT_SIMULATED, preferences);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(value)) << SimulationColumns::id(column);
    EXPECT_TRUE(SimulationColumns::cellText(column, value).isEmpty());
}

// ------------------------------------------------------------------------------ the columns

TEST_F(SimulationColumnsTest, AreOpenRocketsColumnsInItsOrder)
{
    const Gui::Test::SimulationTablePins* table = pins();
    ASSERT_NE(table, nullptr);
    ASSERT_EQ(table->columns.size(), SimulationColumns::kAllColumns.size());
    for (std::size_t c = 0; c < table->columns.size(); ++c)
    {
        expectColumn(table->columns[c], c);
    }
}

TEST_F(SimulationColumnsTest, FindsAColumnByItsId)
{
    for (const Column column : SimulationColumns::kAllColumns)
    {
        EXPECT_EQ(SimulationColumns::fromId(SimulationColumns::id(column)), column);
    }
    // An unknown id, or one of another case, is none (the chooser ignores it).
    EXPECT_EQ(SimulationColumns::fromId("Apogee"), std::nullopt);
    EXPECT_EQ(SimulationColumns::fromId(""), std::nullopt);
    EXPECT_EQ(SimulationColumns::fromId("motors"), std::nullopt);
}

TEST_F(SimulationColumnsTest, RefusesAColumnOutsideTheTable)
{
    EXPECT_THROW(static_cast<void>(SimulationColumns::columnAt(-1)), BugError);
    EXPECT_THROW(static_cast<void>(SimulationColumns::columnAt(SimulationColumns::kColumnCount)),
                 BugError);
}

TEST_F(SimulationColumnsTest, NamesTheStatusColumnStatusInTheChooserOnly)
{
    EXPECT_TRUE(SimulationColumns::headerText(Column::STATUS).isEmpty());
    EXPECT_TRUE(SimulationColumns::headerToolTip(Column::STATUS).isEmpty());
    EXPECT_EQ(SimulationColumns::chooserText(Column::STATUS), QStringLiteral("Status"));
    EXPECT_EQ(SimulationColumns::chooserText(Column::APOGEE), QStringLiteral("Apogee"));
    EXPECT_EQ(SimulationColumns::chooserText(Column::SIMULATION_STEPPER),
              QStringLiteral("Sim. Stepper"));
}

TEST_F(SimulationColumnsTest, HidesTheStepperAloneByDefault)
{
    for (const Column column : SimulationColumns::kAllColumns)
    {
        EXPECT_EQ(SimulationColumns::isHiddenByDefault(column),
                  column == Column::SIMULATION_STEPPER)
            << SimulationColumns::id(column);
    }
}

TEST_F(SimulationColumnsTest, HasOpenRocketsWidths)
{
    const Gui::Test::SimulationTablePins* table = pins();
    ASSERT_NE(table, nullptr);
    ASSERT_FALSE(table->designs.empty());
    // The view's columns of the first example (the stepper hidden): what setColumnWidths() and
    // setupAutoSizeColumns() gave them.
    const std::vector<Gui::Test::SimulationTablePinWidth>& widths = table->designs.front().widths;
    ASSERT_EQ(widths.size(), SimulationColumns::kAllColumns.size() - 1);
    for (const Gui::Test::SimulationTablePinWidth& width : widths)
    {
        expectWidth(width);
    }
    EXPECT_EQ(SimulationColumns::exactWidth(Column::STATUS), 26);
    EXPECT_TRUE(SimulationColumns::isAutoSize(Column::WARNINGS));
    EXPECT_FALSE(SimulationColumns::includesHeaderInAutoSize(Column::WARNINGS));
    EXPECT_TRUE(SimulationColumns::includesHeaderInAutoSize(Column::NAME));
    EXPECT_FALSE(SimulationColumns::isAutoSize(Column::NAME));
}

// ------------------------------------------------------------------------------ the cells

TEST_F(SimulationColumnsTest, ShowsTheSummaryValuesInTheDefaultUnits)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    const Alpha                             s;
    const InMemoryPreferences               preferences;
    const std::unique_ptr<Simulation>       simulation =
        loaded(s.rocket(), "Loaded", Status::LOADED, summaryData({}));

    const Simulation& sim = *simulation;
    EXPECT_EQ(cellTextOf(Column::NAME, sim, preferences), QStringLiteral("Loaded"));
    EXPECT_EQ(
        cellTextOf(Column::CONFIGURATION, sim, preferences),
        QtStrings::fromUtf8(s.rocket().getFlightConfiguration(sim.getId()).getName(preferences)));
    EXPECT_EQ(cellTextOf(Column::SIMULATION_STEPPER, sim, preferences), QStringLiteral("RK4"));
    EXPECT_EQ(cellTextOf(Column::LAUNCH_ROD_VELOCITY, sim, preferences), QStringLiteral("14 m/s"));
    EXPECT_EQ(cellTextOf(Column::APOGEE, sim, preferences), QStringLiteral("38.7 m"));
    EXPECT_EQ(cellTextOf(Column::DEPLOYMENT_VELOCITY, sim, preferences),
              QStringLiteral("5.89 m/s"));
    EXPECT_EQ(cellTextOf(Column::OPTIMUM_COAST_TIME, sim, preferences), QStringLiteral("2.36 s"));
    EXPECT_EQ(cellTextOf(Column::MAX_VELOCITY, sim, preferences), QStringLiteral("24.8 m/s"));
    EXPECT_EQ(cellTextOf(Column::MAX_ACCELERATION, sim, preferences),
              QStringLiteral("124 m/s\u00b2"));
    EXPECT_EQ(cellTextOf(Column::TIME_TO_APOGEE, sim, preferences), QStringLiteral("3.08 s"));
    EXPECT_EQ(cellTextOf(Column::FLIGHT_TIME, sim, preferences), QStringLiteral("12.2 s"));
    EXPECT_EQ(cellTextOf(Column::GROUND_HIT_VELOCITY, sim, preferences),
              QStringLiteral("4.69 m/s"));
    // An icon and a box, never a text.
    EXPECT_TRUE(cellTextOf(Column::STATUS, sim, preferences).isEmpty());
    EXPECT_TRUE(cellTextOf(Column::WARNINGS, sim, preferences).isEmpty());

    // In the default unit of the moment the value is made.
    ASSERT_TRUE(unitGroup(UnitGroupId::DISTANCE).setDefaultUnit("ft"));
    EXPECT_EQ(cellTextOf(Column::APOGEE, sim, preferences), QStringLiteral("127 ft"));
}

TEST_F(SimulationColumnsTest, ShowsTheStepperByItsShortName)
{
    const Alpha                       s;
    const InMemoryPreferences         preferences;
    const std::unique_ptr<Simulation> simulation =
        loaded(s.rocket(), "Loaded", Status::LOADED, summaryData({}));
    simulation->getOptions().setSimulationStepperMethodChoice(SimulationStepperMethod::RK6);
    EXPECT_EQ(SimulationColumns::cellValue(Column::SIMULATION_STEPPER, *simulation, Status::LOADED,
                                           preferences),
              SimulationColumns::CellValue(std::string("RK6")));
}

TEST_F(SimulationColumnsTest, ShowsNothingOfASimulationWithoutData)
{
    const Alpha                       s;
    const InMemoryPreferences         preferences;
    const std::unique_ptr<Simulation> simulation =
        loaded(s.rocket(), "New", Status::NOT_SIMULATED, nullptr);
    for (const Column column : SimulationColumns::kAllColumns)
    {
        if (SimulationColumns::isValueColumn(column))
        {
            expectNoValue(column, *simulation, preferences);
        }
    }
    EXPECT_TRUE(std::holds_alternative<std::monostate>(SimulationColumns::cellValue(
        Column::WARNINGS, *simulation, Status::NOT_SIMULATED, preferences)));
    EXPECT_EQ(SimulationColumns::warningCounts(*simulation), std::nullopt);
    EXPECT_EQ(SimulationColumns::statusSortText(Status::NOT_SIMULATED, *simulation),
              QStringLiteral("No simulation data available."));
}

TEST_F(SimulationColumnsTest, SaysThatASimulationWithoutDataHasNoneInTheTooltips)
{
    const Alpha                       s;
    const std::unique_ptr<Simulation> simulation =
        loaded(s.rocket(), "New", Status::NOT_SIMULATED, nullptr);
    const QPalette palette = QGuiApplication::palette();
    EXPECT_EQ(SimulationColumns::statusToolTip(Status::NOT_SIMULATED, *simulation, true, palette),
              QStringLiteral("<html><b>New</b><br>No simulation data available."));
    EXPECT_EQ(SimulationColumns::statusToolTip(Status::NOT_SIMULATED, *simulation, false, palette),
              QStringLiteral("<html>No simulation data available."));
    EXPECT_EQ(SimulationColumns::warningsToolTip(*simulation, true, palette),
              QStringLiteral("<html><b>New</b><br>No simulation data available."));
    EXPECT_EQ(SimulationColumns::warningsToolTip(*simulation, false, palette),
              QStringLiteral("<html><br>No simulation data available."));
}

TEST_F(SimulationColumnsTest, RefusesTheSummaryValueOfAColumnThatHasNone)
{
    const Alpha                       s;
    const std::unique_ptr<Simulation> simulation =
        loaded(s.rocket(), "Loaded", Status::LOADED, summaryData({}));
    EXPECT_THROW(static_cast<void>(SimulationColumns::summaryValue(Column::NAME, *simulation)),
                 BugError);
    EXPECT_THROW(static_cast<void>(SimulationColumns::summaryValue(Column::WARNINGS, *simulation)),
                 BugError);
}

// ------------------------------------------------------------------------------ the orders

TEST_F(SimulationColumnsTest, SortsTheNamesAsAlphanumComparatorDoes)
{
    EXPECT_EQ(compareNames("Simulation 2", "Simulation 10"), std::weak_ordering::less);
    EXPECT_EQ(compareNames("Simulation 10", "Simulation 9"), std::weak_ordering::greater);
    EXPECT_EQ(compareNames("Simulation 3", "Simulation 3"), std::weak_ordering::equivalent);
}

TEST_F(SimulationColumnsTest, SortsTheTextsWithTheJavaCollator)
{
    // The tertiary collator of en_US: case counts after the letters, lower case first.
    EXPECT_EQ(compareConfigurations("[a8-3]", "[A8-3]"), std::weak_ordering::less);
    EXPECT_EQ(compareConfigurations("[B6-4]", "[a8-3]"), std::weak_ordering::greater);
    EXPECT_EQ(SimulationColumns::compare(Column::STATUS, std::string("Loaded from file-"),
                                         std::string("No simulation data available.")),
              std::weak_ordering::less);
}

TEST_F(SimulationColumnsTest, PutsAMissingValueFirst)
{
    const Value apogee(38.7, unitGroup(UnitGroupId::DISTANCE));
    const Value higher(112.0, unitGroup(UnitGroupId::DISTANCE));
    EXPECT_EQ(SimulationColumns::compare(Column::APOGEE, std::monostate{}, apogee),
              std::weak_ordering::less);
    EXPECT_EQ(SimulationColumns::compare(Column::APOGEE, apogee, std::monostate{}),
              std::weak_ordering::greater);
    EXPECT_EQ(SimulationColumns::compare(Column::APOGEE, std::monostate{}, std::monostate{}),
              std::weak_ordering::equivalent);
    EXPECT_EQ(SimulationColumns::compare(Column::APOGEE, apogee, higher), std::weak_ordering::less);
    EXPECT_EQ(SimulationColumns::compare(Column::APOGEE, higher, apogee),
              std::weak_ordering::greater);
}

TEST_F(SimulationColumnsTest, SortsTheWarningsByTheCriticalThenTheNormalThenTheInformationalCount)
{
    // OpenRocket bug 23, fixed: OpenRocket's warnings column does not sort.
    EXPECT_EQ(compareCounts(levels(1, 0, 0), levels(0, 5, 5)), std::weak_ordering::greater);
    EXPECT_EQ(compareCounts(levels(0, 1, 0), levels(0, 0, 9)), std::weak_ordering::greater);
    EXPECT_EQ(compareCounts(levels(0, 1, 1), levels(0, 1, 2)), std::weak_ordering::less);
    EXPECT_EQ(compareCounts(levels(2, 1, 0), levels(2, 1, 0)), std::weak_ordering::equivalent);
    EXPECT_EQ(compareCounts(levels(0, 0, 0), levels(0, 0, 1)), std::weak_ordering::less);
    // A simulation without data has no counts: a missing value, before no warnings at all.
    EXPECT_EQ(compareCounts(std::nullopt, levels(0, 0, 0)), std::weak_ordering::less);
    EXPECT_EQ(compareCounts(std::nullopt, std::nullopt), std::weak_ordering::equivalent);
}

TEST_F(SimulationColumnsTest, RefusesAValueOfAnotherKind)
{
    const Value apogee(38.7, unitGroup(UnitGroupId::DISTANCE));
    EXPECT_THROW(static_cast<void>(SimulationColumns::compare(Column::NAME, apogee, apogee)),
                 BugError);
    EXPECT_THROW(static_cast<void>(SimulationColumns::compare(Column::APOGEE, std::string("a"),
                                                              std::string("b"))),
                 BugError);
    EXPECT_THROW(
        static_cast<void>(SimulationColumns::compare(Column::WARNINGS, apogee, std::monostate{})),
        BugError);
}

// ------------------------------------------------------------------------------ the status

TEST_F(SimulationColumnsTest, WritesTheStatusToolTipInTheStatusColour)
{
    const Alpha                       s;
    const std::unique_ptr<Simulation> simulation =
        loaded(s.rocket(), "Simulation 1", Status::LOADED, summaryData({}));
    const QPalette palette = QGuiApplication::palette();
    ASSERT_EQ(ThemeColors::theme(palette), ThemeColors::Theme::LIGHT);
    EXPECT_EQ(QtStrings::toUtf8(
                  SimulationColumns::statusToolTip(Status::LOADED, *simulation, true, palette)),
              "<html><b>Simulation 1</b><br><font color=\"" + lightHex(ThemeColors::Key::TEXT) +
                  "\"><i>Loaded from file</i></font><br>");
    EXPECT_EQ(QtStrings::toUtf8(
                  SimulationColumns::statusToolTip(Status::OUTDATED, *simulation, false, palette)),
              "<html><font color=\"" + lightHex(ThemeColors::Key::WARNING) +
                  "\"><i>Out of date</i></font><br>");
    EXPECT_EQ(QtStrings::toUtf8(
                  SimulationColumns::statusToolTip(Status::CANT_RUN, *simulation, false, palette)),
              "<html><font color=\"" + lightHex(ThemeColors::Key::ERROR) +
                  "\"><i>Errors in simulation prevent running</i></font><br>");
    EXPECT_EQ(SimulationColumns::statusSortText(Status::OUTDATED, *simulation),
              QStringLiteral("Out of date-"));
    EXPECT_EQ(SimulationColumns::statusSortText(Status::UPTODATE, *simulation),
              QStringLiteral("Up to date-"));
    EXPECT_EQ(SimulationColumns::statusDescription(Status::EXTERNAL, *simulation),
              QStringLiteral("<i>Imported data</i>"));
}

TEST_F(SimulationColumnsTest, DescribesAnAbortedSimulationByEachBranchsAbort)
{
    const Alpha                       s;
    const std::unique_ptr<Simulation> simulation =
        loaded(s.rocket(), "Aborted", Status::UPTODATE,
               abortedData({SimulationAbort::Cause::NO_MOTORS_FIRED, std::nullopt,
                            SimulationAbort::Cause::TUMBLE_UNDER_THRUST}));
    const QString expected = QStringLiteral(
        "<i><b>Simulation Aborted</b></i><i>: No motors ignited</i><br>"
        "<i><b>Simulation Aborted</b></i><i>: Stage began to tumble under thrust.</i><br>");
    EXPECT_EQ(SimulationColumns::statusDescription(Status::ABORTED, *simulation), expected);
    // The core's description, which asks getStatus() itself, is the same.
    EXPECT_EQ(QtStrings::fromUtf8(simulation->getStatusDescription()), expected);
    EXPECT_EQ(SimulationColumns::statusSortText(Status::ABORTED, *simulation),
              QStringLiteral("Simulation Aborted: No motors ignited-Simulation Aborted: Stage "
                             "began to tumble under thrust.--"));
}

TEST_F(SimulationColumnsTest, DescribesAnAbortWithoutAnAbortEventByTheStatusAlone)
{
    // Status.getDescription(): "It shouldn't be possible to abort without an abort event. But just
    // in case...": the plain description, for data without a SIM_ABORT event and without data.
    const Alpha                       s;
    const QString                     plain = QStringLiteral("<i><b>Simulation Aborted</b></i>");
    const std::unique_ptr<Simulation> withoutAborts =
        loaded(s.rocket(), "Aborted", Status::UPTODATE, abortedData({std::nullopt, std::nullopt}));
    EXPECT_EQ(SimulationColumns::statusDescription(Status::ABORTED, *withoutAborts), plain);
    const std::unique_ptr<Simulation> withoutData =
        loaded(s.rocket(), "Aborted", Status::NOT_SIMULATED, nullptr);
    EXPECT_EQ(SimulationColumns::statusDescription(Status::ABORTED, *withoutData), plain);
    // The status tooltip and the sort text read the same description.
    EXPECT_EQ(SimulationColumns::statusSortText(Status::ABORTED, *withoutAborts),
              QStringLiteral("Simulation Aborted-"));
}

TEST_F(SimulationColumnsTest, ListsTheWarningsOfEachPriorityInItsColour)
{
    const Alpha                       s;
    const std::unique_ptr<Simulation> simulation =
        loaded(s.rocket(), "Warned", Status::LOADED,
               summaryData({Warning::fromString("Informational one", MessagePriority::LOW),
                            Warning::fromString("Critical one", MessagePriority::HIGH),
                            Warning::fromString("Normal one", MessagePriority::NORMAL),
                            Warning::fromString("Critical two", MessagePriority::HIGH)}));
    EXPECT_EQ(SimulationColumns::warningCounts(*simulation), levels(2, 1, 1));
    const QPalette palette = QGuiApplication::palette();
    EXPECT_EQ(
        QtStrings::toUtf8(SimulationColumns::warningsToolTip(*simulation, true, palette)),
        "<html><b>Warned</b><br><b><font color=\"" + lightHex(ThemeColors::Key::ERROR) +
            "\">Critical(s):</font></b><br>Critical one<br>Critical two<br><b><font color=\"" +
            lightHex(ThemeColors::Key::WARNING) +
            "\">Warning(s):</font></b><br>Normal one<br><b><font color=\"" +
            lightHex(ThemeColors::Key::INFO) +
            "\">Informational(s):</font></b><br>Informational one");

    const std::unique_ptr<Simulation> clean =
        loaded(s.rocket(), "Clean", Status::LOADED, summaryData({}));
    EXPECT_EQ(SimulationColumns::warningCounts(*clean), levels(0, 0, 0));
    EXPECT_EQ(QtStrings::toUtf8(SimulationColumns::warningsToolTip(*clean, false, palette)),
              "<html><br><font color=\"" + lightHex(ThemeColors::Key::TEXT_DIM) +
                  "\">No warnings.</font>");
}

TEST_F(SimulationColumnsTest, DescribesEachStatusIconAsOpenRocketsIcons)
{
    EXPECT_EQ(SimulationColumns::statusIconText(Status::NOT_SIMULATED),
              QStringLiteral("Not simulated"));
    EXPECT_EQ(SimulationColumns::statusIconText(Status::CANT_RUN),
              QStringLiteral("Can't run, no motors assigned."));
    EXPECT_EQ(SimulationColumns::statusIconText(Status::UPTODATE), QStringLiteral("Up to date"));
    EXPECT_EQ(SimulationColumns::statusIconText(Status::LOADED),
              QStringLiteral("Loaded from File"));
    EXPECT_EQ(SimulationColumns::statusIconText(Status::OUTDATED), QStringLiteral("Out-of-date"));
    EXPECT_EQ(SimulationColumns::statusIconText(Status::EXTERNAL), QStringLiteral("Imported data"));
    EXPECT_EQ(SimulationColumns::statusIconText(Status::ABORTED),
              QStringLiteral("Simulation run aborted"));
}

}  // namespace

}  // namespace QtRocket::Gui
