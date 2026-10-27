// The rows of the parts library's table and their order (Java: ComponentPresetTable's
// TableRowSorter, ComponentPresetRowFilter and ComponentPresetChooserDialog.updateFilters(), swing
// gui/dialogs/preset/). OpenRocket has no JUnit test of them.
//
// Expectations marked "probe" are OpenRocket's own results, printed by
// tools/openrocket-gui-pins/preset-chooser/probe/ComponentPresetTableProbe.java (run on OpenRocket
// 5f164fd0e with JDK 17; tools/openrocket-gui-pins/preset-chooser/hand-pinned.txt keeps its
// transcript): ComponentPresetTable over the presets of smallPresetDatabase() and over the bundled
// presets, the view's rows written as their model rows, a view of the bundled presets as the MD5 of
// its presets' digests in view order. Where QtRocket fixes an OpenRocket bug the probe prints the
// fixed result too ("fixed:"), computed with OpenRocket's classes.

#include "QtRocket/gui/dialogs/preset/ComponentPresetFilterModel.h"

#include <array>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "PresetTableTestSupport.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetTableModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/TableSortKeys.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/util/BugError.h"
#include "rocket/preset/TestPresetDatabase.h"

namespace QtRocket::Gui
{

namespace
{

using ComponentPresetFilterModelTest = Test::GuiTest;
using Filter                         = ComponentPresetFilterModel;
using Model                          = ComponentPresetTableModel;
using Type                           = ComponentPresetType;
using SortKey                        = TableSortKeys::SortKey;
using SortOrder                      = TableSortKeys::SortOrder;
using Test::presetDigestOfView;
using Test::presetSortKeys;
using Test::presetViewRows;

/// A table model of @p database's presets of @p type and its filter model.
struct FilteredTable
{
    InMemoryPreferences preferences;
    Model               model;
    Filter              filter{model, nullptr};

    FilteredTable(const ComponentPresetDatabase& database, Type type)
      : model(type, database.listForType(type), preferences, nullptr)
    {
    }
    explicit FilteredTable(Type type) : FilteredTable(QtRocket::Test::smallPresetDatabase(), type)
    {
    }
};

/// @p lines as strings.
[[nodiscard]] std::vector<std::string> asStrings(std::span<const std::string_view> lines)
{
    return {lines.begin(), lines.end()};
}

/// The probe's sortEachColumn(): for each column the view's rows sorted by it alone, ascending
/// and descending, a line for each column whose orders are not both the model order, then the
/// columns whose are.
[[nodiscard]] std::vector<std::string> sortEachColumn(Type type)
{
    FilteredTable            table(type);
    const std::string        title = std::string(componentPresetTypeName(type));
    std::vector<std::string> lines;
    std::string              modelOrder;
    std::string              unsorted;
    table.filter.setSortKeys({});
    modelOrder = presetViewRows(table.filter);
    for (int column = 0; column < Model::kColumnCount; ++column)
    {
        table.filter.setSortKeys({SortKey{.column = column, .order = SortOrder::ASCENDING}});
        const std::string ascending = presetViewRows(table.filter);
        table.filter.setSortKeys({SortKey{.column = column, .order = SortOrder::DESCENDING}});
        const std::string descending = presetViewRows(table.filter);
        if (ascending == modelOrder && descending == modelOrder)
        {
            unsorted += " " + std::to_string(column);
        }
        else
        {
            lines.push_back(std::format("{} column {} asc: {}; desc: {}", title, column, ascending,
                                        descending));
        }
    }
    lines.push_back(std::format("{} model order both ways:{}", title, unsorted));
    return lines;
}

// NOLINTBEGIN(bugprone-suspicious-missing-comma): clang-format splits the long lines
constexpr auto kSortEachBodyTube = std::to_array<std::string_view>({
    "BODY_TUBE column 3 asc: 0 1 2 3 4 5 6 7 8 9 10 11; desc: 11 10 9 8 7 6 5 4 3 2 1 0",
    "BODY_TUBE column 4 asc: 8 0 7 4 3 11 2 1 5 6 9 10; desc: 10 9 6 5 1 2 11 3 4 7 0 8",
    "BODY_TUBE column 5 asc: 11 1 2 0 8 3 4 7 5 6 9 10; desc: 10 5 6 9 3 4 7 0 8 2 1 11",
    "BODY_TUBE column 8 asc: 11 1 2 0 8 3 4 7 5 6 9 10; desc: 10 5 6 9 3 4 7 0 8 2 1 11",
    "BODY_TUBE column 9 asc: 2 3 1 5 0 8 4 7 11 10 6 9; desc: 9 6 10 11 4 7 0 8 5 1 3 2",
    "BODY_TUBE column 19 asc: 11 0 5 6 8 9 1 3 4 7 10 2; desc: 2 10 3 4 7 1 0 5 6 8 9 11",
    "BODY_TUBE column 30 asc: 2 11 0 1 3 4 5 6 7 8 9 10; desc: 0 1 3 4 5 6 7 8 9 10 11 2",
    "BODY_TUBE model order both ways: 0 1 2 6 7 10 11 12 13 14 15 16 17 18 20 21 22 23 24 25 26 27 "
    "28 29",
});
// NOLINTEND(bugprone-suspicious-missing-comma)
// NOLINTBEGIN(bugprone-suspicious-missing-comma): clang-format splits the long lines
constexpr auto kSortEachNoseCone = std::to_array<std::string_view>({
    "NOSE_CONE column 3 asc: 0 1 2 3 4 5; desc: 5 4 3 2 1 0",
    "NOSE_CONE column 4 asc: 2 0 4 3 1 5; desc: 5 1 3 4 0 2",
    "NOSE_CONE column 7 asc: 5 2 0 3 4 1; desc: 1 3 4 0 2 5",
    "NOSE_CONE column 9 asc: 5 3 2 0 1 4; desc: 4 1 0 2 3 5",
    "NOSE_CONE column 11 asc: 5 2 0 3 4 1; desc: 1 3 4 0 2 5",
    "NOSE_CONE column 12 asc: 5 2 0 3 4 1; desc: 1 3 4 0 2 5",
    "NOSE_CONE column 18 asc: 4 0 1 2 3 5; desc: 0 1 2 3 5 4",
    "NOSE_CONE column 19 asc: 5 0 1 2 3 4; desc: 0 1 2 3 4 5",
    "NOSE_CONE column 20 asc: 1 0 2 3 4 5; desc: 0 2 3 4 5 1",
    "NOSE_CONE column 26 asc: 0 1 2 3 4 5; desc: 5 0 1 2 3 4",
    "NOSE_CONE column 30 asc: 0 1 2 3 4 5; desc: 5 0 1 2 3 4",
    "NOSE_CONE model order both ways: 0 1 2 5 6 8 10 13 14 15 16 17 21 22 23 24 25 27 28 29",
});
// NOLINTEND(bugprone-suspicious-missing-comma)
// NOLINTBEGIN(bugprone-suspicious-missing-comma): clang-format splits the long lines
constexpr auto kSortEachCenteringRing = std::to_array<std::string_view>({
    "CENTERING_RING column 3 asc: 0 1 2 3 4 5 6 7; desc: 7 6 5 4 3 2 1 0",
    "CENTERING_RING column 4 asc: 7 2 1 0 3 5 4 6; desc: 6 4 5 3 0 1 2 7",
    "CENTERING_RING column 5 asc: 7 2 1 0 3 4 5 6; desc: 4 5 6 0 3 1 2 7",
    "CENTERING_RING column 8 asc: 7 0 1 2 3 4 5 6; desc: 0 1 2 3 4 5 6 7",
    "CENTERING_RING column 9 asc: 7 0 1 2 3 4 5 6; desc: 0 1 2 3 4 5 6 7",
    "CENTERING_RING column 19 asc: 7 2 1 0 3 4 5 6; desc: 4 5 6 0 3 1 2 7",
    "CENTERING_RING column 30 asc: 7 0 1 2 3 4 5 6; desc: 0 1 2 3 4 5 6 7",
    "CENTERING_RING model order both ways: 0 1 2 6 7 10 11 12 13 14 15 16 17 18 20 21 22 23 24 25 "
    "26 27 28 29",
});
// NOLINTEND(bugprone-suspicious-missing-comma)
// NOLINTBEGIN(bugprone-suspicious-missing-comma): clang-format splits the long lines
constexpr auto kSortEachStreamer = std::to_array<std::string_view>({
    "STREAMER column 1 asc: 1 0; desc: 0 1",
    "STREAMER column 2 asc: 0 1; desc: 1 0",
    "STREAMER column 3 asc: 0 1; desc: 1 0",
    "STREAMER column 4 asc: 1 0; desc: 0 1",
    "STREAMER column 9 asc: 0 1; desc: 1 0",
    "STREAMER column 10 asc: 0 1; desc: 1 0",
    "STREAMER column 19 asc: 0 1; desc: 1 0",
    "STREAMER column 30 asc: 0 1; desc: 1 0",
    "STREAMER model order both ways: 0 5 6 7 8 11 12 13 14 15 16 17 18 20 21 22 23 24 25 26 27 28 "
    "29",
});
// NOLINTEND(bugprone-suspicious-missing-comma)
// NOLINTBEGIN(bugprone-suspicious-missing-comma): clang-format splits the long lines
constexpr auto kSortEachParachute = std::to_array<std::string_view>({
    "PARACHUTE column 3 asc: 0 1 2; desc: 2 1 0",
    "PARACHUTE column 4 asc: 2 0 1; desc: 1 0 2",
    "PARACHUTE column 21 asc: 2 0 1; desc: 1 0 2",
    "PARACHUTE column 22 asc: 0 2 1; desc: 1 0 2",
    "PARACHUTE column 23 asc: 0 2 1; desc: 1 0 2",
    "PARACHUTE column 24 asc: 2 0 1; desc: 1 0 2",
    "PARACHUTE column 25 asc: 2 0 1; desc: 0 1 2",
    "PARACHUTE column 30 asc: 0 1 2; desc: 2 0 1",
    "PARACHUTE model order both ways: 0 1 2 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 26 27 28 29",
});
// NOLINTEND(bugprone-suspicious-missing-comma)

// NOLINTBEGIN(bugprone-suspicious-missing-comma): clang-format splits the long lines
constexpr auto kToggles = std::to_array<std::string_view>({
    "toggle 9: [9 asc]: 2 3 1 5 0 8 4 7 11 10 6 9",
    "toggle 9: [9 desc]: 9 6 10 11 4 7 0 8 5 1 3 2",
    "toggle 8: [8 asc, 9 desc]: 11 1 2 0 8 4 7 3 9 6 5 10",
    "toggle 5: [5 asc, 8 asc, 9 desc]: 11 1 2 0 8 4 7 3 9 6 5 10",
    "toggle 9: [9 asc, 5 asc, 8 asc]: 2 3 1 5 0 8 4 7 11 10 6 9",
    "toggle 4: [4 asc, 9 asc, 5 asc]: 8 0 7 4 3 11 2 1 5 6 9 10",
    "keys [5 desc, 8 asc, 9 desc]: 10 9 6 5 4 7 3 0 8 2 1 11",
});
// NOLINTEND(bugprone-suspicious-missing-comma)

/// The probe's header clicks on the small database's body tubes without keys, then the three keys
/// it sets.
[[nodiscard]] std::vector<std::string> toggleTranscript()
{
    FilteredTable            table(Type::BODY_TUBE);
    std::vector<std::string> lines;
    table.filter.setSortKeys({});
    for (const int column : {9, 9, 8, 5, 9, 4})
    {
        table.filter.toggleSortOrder(column);
        lines.push_back("toggle " + std::to_string(column) + ": " + presetSortKeys(table.filter) +
                        ": " + presetViewRows(table.filter));
    }
    table.filter.setSortKeys({SortKey{.column = 5, .order = SortOrder::DESCENDING},
                              SortKey{.column = 8, .order = SortOrder::ASCENDING},
                              SortKey{.column = 9, .order = SortOrder::DESCENDING}});
    lines.push_back("keys " + presetSortKeys(table.filter) + ": " + presetViewRows(table.filter));
    return lines;
}

struct TextCase
{
    Type             type;
    std::string_view pattern;
    std::string_view rows;
};

constexpr std::array kTextCases{
    TextCase{.type = Type::BODY_TUBE, .pattern = "", .rows = "0 1 2 3 4 5 6 7 8 9 10 11"},
    TextCase{.type = Type::BODY_TUBE, .pattern = "estes", .rows = "0 1 2 3 4 5 6 7 8 9 10 11"},
    TextCase{.type = Type::BODY_TUBE, .pattern = "ESTES", .rows = "0 1 2 3 4 5 6 7 8 9 10 11"},
    TextCase{.type = Type::BODY_TUBE, .pattern = "bt-20", .rows = "11"},
    TextCase{.type = Type::BODY_TUBE, .pattern = "e", .rows = "0 1 2 3 4 5 6 7 8 9 10 11"},
    TextCase{.type = Type::BODY_TUBE, .pattern = "true", .rows = ""},
    TextCase{.type    = Type::BODY_TUBE,
             .pattern = "false",
             .rows    = ""},  // OpenRocket: "0 1 2 3 4 5 6 7 8 9 10 11"
    TextCase{.type = Type::BODY_TUBE, .pattern = "Kraft", .rows = "0 1 3 4 5 6 7 8 9 10 11"},
    TextCase{.type = Type::BODY_TUBE, .pattern = "cm", .rows = "0 1 2 3 4 5 6 7 8 9 10 11"},
    TextCase{.type = Type::BODY_TUBE, .pattern = "2.5", .rows = "3 4 7"},
    TextCase{.type = Type::BODY_TUBE, .pattern = "^B", .rows = "0 1 2 3 4 5 6 7 8 9 10 11"},
    TextCase{.type    = Type::BODY_TUBE,
             .pattern = "[",
             .rows    = ""},  // OpenRocket: every row (no text filter)
    TextCase{.type    = Type::BODY_TUBE,
             .pattern = "(",
             .rows    = ""},  // OpenRocket: every row (no text filter)
    TextCase{.type = Type::BODY_TUBE, .pattern = "0.5 mil", .rows = ""},
    TextCase{.type = Type::BODY_TUBE, .pattern = "Ogive", .rows = ""},
    TextCase{.type = Type::BODY_TUBE, .pattern = "\\d{5}", .rows = "0 1 2 3 4 5 6 7 8 9 10"},
    TextCase{.type = Type::NOSE_CONE, .pattern = "", .rows = "0 1 2 3 4 5"},
    TextCase{.type = Type::NOSE_CONE, .pattern = "estes", .rows = "0 1 2 3 4 5"},
    TextCase{.type = Type::NOSE_CONE, .pattern = "ESTES", .rows = "0 1 2 3 4 5"},
    TextCase{.type = Type::NOSE_CONE, .pattern = "bt-20", .rows = "5"},
    TextCase{.type = Type::NOSE_CONE, .pattern = "e", .rows = "0 1 2 3 4 5"},
    TextCase{.type = Type::NOSE_CONE, .pattern = "true", .rows = ""},
    TextCase{.type = Type::NOSE_CONE, .pattern = "false", .rows = ""},  // OpenRocket: "0 1 2 3 4 5"
    TextCase{.type = Type::NOSE_CONE, .pattern = "Kraft", .rows = ""},
    TextCase{.type = Type::NOSE_CONE, .pattern = "cm", .rows = "0 1 2 3 4 5"},
    TextCase{.type = Type::NOSE_CONE, .pattern = "2.5", .rows = "0 1 2 4"},
    TextCase{.type = Type::NOSE_CONE, .pattern = "^B", .rows = ""},
    TextCase{.type    = Type::NOSE_CONE,
             .pattern = "[",
             .rows    = ""},  // OpenRocket: every row (no text filter)
    TextCase{.type    = Type::NOSE_CONE,
             .pattern = "(",
             .rows    = ""},  // OpenRocket: every row (no text filter)
    TextCase{.type = Type::NOSE_CONE, .pattern = "0.5 mil", .rows = ""},
    TextCase{.type = Type::NOSE_CONE, .pattern = "Ogive", .rows = "0 1 2 3 5"},
    TextCase{.type = Type::NOSE_CONE, .pattern = "\\d{5}", .rows = "0 1 2 3 4"},
    TextCase{.type = Type::STREAMER, .pattern = "", .rows = "0 1"},
    TextCase{.type = Type::STREAMER, .pattern = "estes", .rows = ""},
    TextCase{.type = Type::STREAMER, .pattern = "ESTES", .rows = ""},
    TextCase{.type = Type::STREAMER, .pattern = "bt-20", .rows = ""},
    TextCase{.type = Type::STREAMER, .pattern = "e", .rows = "0 1"},
    TextCase{.type = Type::STREAMER, .pattern = "true", .rows = ""},   // OpenRocket: "0"
    TextCase{.type = Type::STREAMER, .pattern = "false", .rows = ""},  // OpenRocket: "0 1"
    TextCase{.type = Type::STREAMER, .pattern = "Kraft", .rows = ""},
    TextCase{.type = Type::STREAMER, .pattern = "cm", .rows = "0 1"},
    TextCase{.type = Type::STREAMER, .pattern = "2.5", .rows = "0"},
    TextCase{.type = Type::STREAMER, .pattern = "^B", .rows = ""},
    TextCase{.type    = Type::STREAMER,
             .pattern = "[",
             .rows    = ""},  // OpenRocket: every row (no text filter)
    TextCase{.type    = Type::STREAMER,
             .pattern = "(",
             .rows    = ""},  // OpenRocket: every row (no text filter)
    TextCase{.type = Type::STREAMER, .pattern = "0.5 mil", .rows = ""},
    TextCase{.type = Type::STREAMER, .pattern = "Ogive", .rows = ""},
    TextCase{.type = Type::STREAMER, .pattern = "\\d{5}", .rows = ""},
    TextCase{.type = Type::PARACHUTE, .pattern = "", .rows = "0 1 2"},
    TextCase{.type = Type::PARACHUTE, .pattern = "estes", .rows = "0 1 2"},
    TextCase{.type = Type::PARACHUTE, .pattern = "ESTES", .rows = "0 1 2"},
    TextCase{.type = Type::PARACHUTE, .pattern = "bt-20", .rows = ""},
    TextCase{.type = Type::PARACHUTE, .pattern = "e", .rows = "0 1 2"},
    TextCase{.type = Type::PARACHUTE, .pattern = "true", .rows = ""},
    TextCase{.type = Type::PARACHUTE, .pattern = "false", .rows = ""},  // OpenRocket: "0 1 2"
    TextCase{.type = Type::PARACHUTE, .pattern = "Kraft", .rows = ""},
    TextCase{.type = Type::PARACHUTE, .pattern = "cm", .rows = "0 1 2"},
    TextCase{.type = Type::PARACHUTE, .pattern = "2.5", .rows = ""},
    TextCase{.type = Type::PARACHUTE, .pattern = "^B", .rows = ""},
    TextCase{.type    = Type::PARACHUTE,
             .pattern = "[",
             .rows    = ""},  // OpenRocket: every row (no text filter)
    TextCase{.type    = Type::PARACHUTE,
             .pattern = "(",
             .rows    = ""},  // OpenRocket: every row (no text filter)
    TextCase{.type = Type::PARACHUTE, .pattern = "0.5 mil", .rows = ""},
    TextCase{.type = Type::PARACHUTE, .pattern = "Ogive", .rows = ""},
    TextCase{.type = Type::PARACHUTE, .pattern = "\\d{5}", .rows = "0 1"},
};

/// The cases of kTextCases whose filter lets other rows through, as "<type> '<pattern>': <rows>".
[[nodiscard]] std::vector<std::string> textFilterMismatches()
{
    std::vector<std::string> mismatches;
    for (const TextCase& textCase : kTextCases)
    {
        FilteredTable table(textCase.type);
        table.filter.setTextFilter(QtStrings::fromUtf8(textCase.pattern));
        const std::string rows = presetViewRows(table.filter);
        if (rows != textCase.rows)
        {
            mismatches.push_back(std::format("{} '{}': {}", componentPresetTypeName(textCase.type),
                                             textCase.pattern, rows));
        }
    }
    return mismatches;
}

/// The view's rows of the small database's body tubes with an outer diameter filter of
/// @p diameter.
[[nodiscard]] std::string outerDiameterRows(double diameter)
{
    FilteredTable table(Type::BODY_TUBE);
    table.filter.setAftDiameterFilter(diameter);
    return presetViewRows(table.filter);
}

TEST_F(ComponentPresetFilterModelTest, ANewModelSortsByManufacturer)
{
    // probe: "BODY_TUBE at open [2 asc]: 0 1 2 3 4 5 6 7 8 9 10 11"; every type's view in model
    // order ("== cells": "view 0 1 ...").
    const FilteredTable table(Type::BODY_TUBE);
    EXPECT_EQ(presetSortKeys(table.filter), "[2 asc]");
    EXPECT_EQ(presetViewRows(table.filter), "0 1 2 3 4 5 6 7 8 9 10 11");
    const FilteredTable streamers(Type::STREAMER);
    EXPECT_EQ(presetViewRows(streamers.filter), "0 1");
    EXPECT_EQ(streamers.filter.modelRow(1), 1);
    EXPECT_EQ(streamers.filter.modelRow(2), -1);
    EXPECT_EQ(streamers.filter.viewRow(0), 0);
    EXPECT_EQ(streamers.filter.viewRow(-1), -1);
    // probe: "BODY_TUBE no keys: 0 1 ...": the model order.
    FilteredTable unsorted(Type::BODY_TUBE);
    unsorted.filter.setSortKeys({});
    EXPECT_EQ(presetViewRows(unsorted.filter), "0 1 2 3 4 5 6 7 8 9 10 11");
}

TEST_F(ComponentPresetFilterModelTest, EachColumnSortsByItsComparator)
{
    // probe ("== sorting"): the part number by AlphanumComparator, the lengths and masses by
    // their values, the Integers and Booleans by theirs, the texts with the tertiary collator, a
    // missing value first, equal rows in model order both ways.
    EXPECT_EQ(sortEachColumn(Type::BODY_TUBE), asStrings(kSortEachBodyTube));
    EXPECT_EQ(sortEachColumn(Type::NOSE_CONE), asStrings(kSortEachNoseCone));
    EXPECT_EQ(sortEachColumn(Type::CENTERING_RING), asStrings(kSortEachCenteringRing));
    EXPECT_EQ(sortEachColumn(Type::STREAMER), asStrings(kSortEachStreamer));
    EXPECT_EQ(sortEachColumn(Type::PARACHUTE), asStrings(kSortEachParachute));
}

TEST_F(ComponentPresetFilterModelTest, HeaderClicksToggleTheSortKeys)
{
    // probe: toggleSortOrder() on Length, Length, Inner Diameter, Outer Diameter, Length and
    // Description: at most three keys, the clicked column first.
    EXPECT_EQ(toggleTranscript(), asStrings(kToggles));
    FilteredTable    table(Type::BODY_TUBE);
    const QSignalSpy keys(&table.filter, &Filter::sortKeysChanged);
    table.filter.toggleSortOrder(3);
    table.filter.setSortKeys({SortKey{.column = 4, .order = SortOrder::ASCENDING}});
    EXPECT_EQ(keys.count(), 2);
    EXPECT_THROW(table.filter.toggleSortOrder(31), BugError);
}

TEST_F(ComponentPresetFilterModelTest, TheTextFilterSearchesTheTextColumns)
{
    // probe ("== text filter", "fixed:"): every column of the model, the hidden ones too, but not
    // the Boolean ones (bug 17: OpenRocket's "false" matches every row by the favourite column);
    // case-insensitive. An invalid pattern matches no row (OpenRocket shows every row).
    EXPECT_EQ(textFilterMismatches(), std::vector<std::string>{});
    FilteredTable table(Type::BODY_TUBE);
    table.filter.setTextFilter(QStringLiteral("["));
    EXPECT_FALSE(table.filter.isTextFilterValid());
    EXPECT_EQ(table.filter.textFilter(), QStringLiteral("["));
    table.filter.setTextFilter(QString());
    EXPECT_TRUE(table.filter.isTextFilterValid());
    EXPECT_EQ(table.filter.rowCount(), 12);
}

TEST_F(ComponentPresetFilterModelTest, TheDiameterFiltersAllowFivePercentButAtLeastOneMillimetre)
{
    // probe: "aft filter value 0.0248 eps 0.00124", "fore filter value 0.05 eps
    // 0.0025000000000000005" (the M3 scouting's dialog), 1 mm below 2 cm and for NaN.
    EXPECT_EQ(Filter::diameterTolerance(0.0248), 0.0248 * 0.05);
    EXPECT_EQ(Filter::diameterTolerance(0.05), 0.0025000000000000005);
    EXPECT_EQ(Filter::diameterTolerance(0.019), 0.001);
    EXPECT_EQ(Filter::diameterTolerance(0.0), 0.001);
    EXPECT_EQ(Filter::diameterTolerance(std::numeric_limits<double>::quiet_NaN()), 0.001);
    EXPECT_TRUE(Filter::matchesDiameter(0.02, std::nullopt));
    EXPECT_TRUE(Filter::matchesDiameter(0.02, 0.0209));
    EXPECT_FALSE(Filter::matchesDiameter(0.02, 0.021));
    EXPECT_FALSE(Filter::matchesDiameter(std::numeric_limits<double>::quiet_NaN(), 0.02));
    // probe ("== diameter filters"): the body tubes' outer diameters 0.0508 0.0306832 0.048768
    // 0.0635 0.0635 0.0762 0.0762 0.0635 0.0508 0.0762 0.1016 0.0186944.
    EXPECT_EQ(outerDiameterRows(0.0248), "");
    EXPECT_EQ(outerDiameterRows(0.0187), "11");
    EXPECT_EQ(outerDiameterRows(0.0337), "");
    EXPECT_EQ(outerDiameterRows(0.0414), "");
    EXPECT_EQ(outerDiameterRows(0.0), "");
    EXPECT_EQ(outerDiameterRows(0.02), "");
    EXPECT_EQ(outerDiameterRows(0.019), "11");
    EXPECT_EQ(outerDiameterRows(0.0182), "11");
    EXPECT_EQ(outerDiameterRows(std::numeric_limits<double>::quiet_NaN()), "");
}

TEST_F(ComponentPresetFilterModelTest, TheTransitionsFilterByTheirForeAndAftDiameters)
{
    // probe: "transitions 0.0508>0.0635 0.0186944>0.024790399999999997"; "fore of the first: 0";
    // "aft of the second: 1".
    FilteredTable table(Type::TRANSITION);
    EXPECT_EQ(table.filter.foreDiameterColumn(), 6);
    EXPECT_EQ(table.filter.aftDiameterColumn(), 7);
    table.filter.setForeDiameterFilter(0.0508);
    EXPECT_EQ(presetViewRows(table.filter), "0");
    EXPECT_EQ(table.filter.foreDiameterFilter(), std::optional<double>(0.0508));
    table.filter.setForeDiameterFilter(std::nullopt);
    table.filter.setAftDiameterFilter(0.024790399999999997);
    EXPECT_EQ(presetViewRows(table.filter), "1");
    // Both filters together: none passes.
    table.filter.setForeDiameterFilter(0.0508);
    EXPECT_EQ(presetViewRows(table.filter), "");
    // A type without the column has no such filter (OpenRocket makes no box).
    FilteredTable noses(Type::NOSE_CONE);
    EXPECT_EQ(noses.filter.foreDiameterColumn(), -1);
    EXPECT_EQ(noses.filter.aftDiameterColumn(), 7);
    EXPECT_THROW(noses.filter.setForeDiameterFilter(0.0248), BugError);
    noses.filter.setForeDiameterFilter(std::nullopt);
    FilteredTable chutes(Type::PARACHUTE);
    EXPECT_THROW(chutes.filter.setAftDiameterFilter(0.0248), BugError);
}

TEST_F(ComponentPresetFilterModelTest, TheLegacyFilterHidesTheLegacyRows)
{
    // probe ("== legacy filter"): "BULK_HEAD: all 0 1; legacy hidden 0", "STREAMER: all 0 1;
    // legacy hidden 1".
    FilteredTable bulkheads(Type::BULK_HEAD);
    EXPECT_FALSE(bulkheads.filter.hidesLegacy());
    EXPECT_EQ(presetViewRows(bulkheads.filter), "0 1");
    bulkheads.filter.setHideLegacy(true);
    EXPECT_TRUE(bulkheads.filter.hidesLegacy());
    EXPECT_EQ(presetViewRows(bulkheads.filter), "0");
    bulkheads.filter.setHideLegacy(false);
    EXPECT_EQ(presetViewRows(bulkheads.filter), "0 1");
    FilteredTable streamers(Type::STREAMER);
    streamers.filter.setHideLegacy(true);
    EXPECT_EQ(presetViewRows(streamers.filter), "1");
}

TEST_F(ComponentPresetFilterModelTest, AFavouriteSortedByMovesItsRow)
{
    // probe ("== favourites"): model row 5 ticked, sorted by the favourite descending: "5 0 1 2
    // 3 4 6 7 8 9 10 11"; unticked: the model order again.
    FilteredTable table(Type::BODY_TUBE);
    table.filter.setSortKeys({SortKey{.column = 0, .order = SortOrder::DESCENDING}});
    table.model.setFavorite(5, true);
    EXPECT_EQ(presetViewRows(table.filter), "5 0 1 2 3 4 6 7 8 9 10 11");
    table.model.setFavorite(5, false);
    EXPECT_EQ(presetViewRows(table.filter), "0 1 2 3 4 5 6 7 8 9 10 11");
}

TEST_F(ComponentPresetFilterModelTest, TheModelPassesQAbstractItemModelTester)
{
    FilteredTable                  table(Type::NOSE_CONE);
    const QAbstractItemModelTester tester(&table.filter,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    table.filter.toggleSortOrder(9);
    table.filter.setTextFilter(QStringLiteral("2.5"));
    table.filter.setAftDiameterFilter(0.0635);
    table.filter.setHideLegacy(true);
    table.model.setFavorite(1, true);
    table.filter.setTextFilter(QString());
    EXPECT_EQ(presetViewRows(table.filter), "0");
}

struct BundledView
{
    Type             type;
    std::string_view all;
    std::string_view legacyHidden;
};

constexpr std::array kBundledViews{
    BundledView{.type         = Type::BODY_TUBE,
                .all          = "rows 1496 md5 883c0d80e21a09c80fcbe21ab241fd73",
                .legacyHidden = "rows 1089 md5 e123acb02aba2bf54cf770e790515b71"},
    BundledView{.type         = Type::NOSE_CONE,
                .all          = "rows 1252 md5 de5e21507bb1f79fb984032aef2cb48a",
                .legacyHidden = "rows 855 md5 1cc3c6121e6984f5a3de7d9a679f7973"},
    BundledView{.type         = Type::TRANSITION,
                .all          = "rows 613 md5 0b5b1227e9b22ed5ee8b6d59dfc16f3b",
                .legacyHidden = "rows 360 md5 6404b883442b3a1b855ab37a4982714e"},
    BundledView{.type         = Type::TUBE_COUPLER,
                .all          = "rows 332 md5 f13396b22d64c3c2cd91b2a2def73d45",
                .legacyHidden = "rows 237 md5 03c0a4b4d5c589b7ca94867b3ad224ec"},
    BundledView{.type         = Type::BULK_HEAD,
                .all          = "rows 186 md5 9677f77cd486f431c887c92b8f9ed7de",
                .legacyHidden = "rows 115 md5 96049eb66928a484f923a32028aaec7d"},
    BundledView{.type         = Type::CENTERING_RING,
                .all          = "rows 695 md5 40ab62bc484d30121f0bd8fef4391e32",
                .legacyHidden = "rows 496 md5 7b0773605336912307151b91f23b7792"},
    BundledView{.type         = Type::ENGINE_BLOCK,
                .all          = "rows 64 md5 9d4116edc06c262f7aaa080af8adccfc",
                .legacyHidden = "rows 38 md5 1294aac3bae19d228ab28603e9f29099"},
    BundledView{.type         = Type::LAUNCH_LUG,
                .all          = "rows 102 md5 a4735680ecc7924018a025856cb2f776",
                .legacyHidden = "rows 59 md5 e60f20c54a106614fbe9f2239a1a7529"},
    BundledView{.type         = Type::RAIL_BUTTON,
                .all          = "rows 8 md5 6ea35c8dda98190a78ece043b1d9d10a",
                .legacyHidden = "rows 8 md5 6ea35c8dda98190a78ece043b1d9d10a"},
    BundledView{.type         = Type::STREAMER,
                .all          = "rows 58 md5 cb1547f54b2e1fdbbb9d4a48bb39a739",
                .legacyHidden = "rows 46 md5 9a4723f58bf5e0afb14c2885737322af"},
    BundledView{.type         = Type::PARACHUTE,
                .all          = "rows 422 md5 38fe7c2f3c733e4723c3e10d70e4d76f",
                .legacyHidden = "rows 381 md5 c447946757c96fdcfa7517495e7281dd"},
};

struct BundledSort
{
    Type             type;
    int              column;
    std::string_view ascending;
    std::string_view descending;
};

constexpr std::array kBundledSorts{
    BundledSort{.type       = Type::BODY_TUBE,
                .column     = 1,
                .ascending  = "rows 1496 md5 176363750a1a4c108cc911966f31716f",
                .descending = "rows 1496 md5 70372cf06aac31658b74af2aaef3cdd0"},
    BundledSort{.type       = Type::BODY_TUBE,
                .column     = 2,
                .ascending  = "rows 1496 md5 883c0d80e21a09c80fcbe21ab241fd73",
                .descending = "rows 1496 md5 faa39d1fcc6709a3a1c51d3a6908a4b5"},
    BundledSort{.type       = Type::BODY_TUBE,
                .column     = 3,
                .ascending  = "rows 1496 md5 8cd7e492f541d55396eb952c895c664f",
                .descending = "rows 1496 md5 467a887587fd5202ebd7058d6d24b3e3"},
    BundledSort{.type       = Type::BODY_TUBE,
                .column     = 4,
                .ascending  = "rows 1496 md5 3d59d0baeecab4ba1a76753737403f5c",
                .descending = "rows 1496 md5 777181adda8261e2fae82ac6796d4cee"},
    BundledSort{.type       = Type::BODY_TUBE,
                .column     = 8,
                .ascending  = "rows 1496 md5 a242040bf641661beb9e88b5c253bb01",
                .descending = "rows 1496 md5 893d6dfb040b51c52c438783c1be8101"},
    BundledSort{.type       = Type::BODY_TUBE,
                .column     = 5,
                .ascending  = "rows 1496 md5 e812cbb42ee0fb36a4db6e98de2f236c",
                .descending = "rows 1496 md5 6acd377a886d895b3657578ca7cc8acb"},
    BundledSort{.type       = Type::BODY_TUBE,
                .column     = 9,
                .ascending  = "rows 1496 md5 8485bd2594316769602be30a5c343168",
                .descending = "rows 1496 md5 aeb9b1629bbe0dfd378399151d094ef8"},
    BundledSort{.type       = Type::NOSE_CONE,
                .column     = 1,
                .ascending  = "rows 1252 md5 af94d9b9d14a74a087ad043c1148ea87",
                .descending = "rows 1252 md5 7e466865395dab86da415ee367307011"},
    BundledSort{.type       = Type::NOSE_CONE,
                .column     = 2,
                .ascending  = "rows 1252 md5 de5e21507bb1f79fb984032aef2cb48a",
                .descending = "rows 1252 md5 0844b886924c766e518872970beb7bda"},
    BundledSort{.type       = Type::NOSE_CONE,
                .column     = 3,
                .ascending  = "rows 1252 md5 cae371a29922d93a2bd5856305644582",
                .descending = "rows 1252 md5 233a51710500aa3df72f398c99d8582b"},
    BundledSort{.type       = Type::NOSE_CONE,
                .column     = 4,
                .ascending  = "rows 1252 md5 1994d63abef5bcee353c300e241213b1",
                .descending = "rows 1252 md5 b8068f45bbd15cd79bf50a1fb8a43f17"},
    BundledSort{.type       = Type::NOSE_CONE,
                .column     = 18,
                .ascending  = "rows 1252 md5 8c1d62ff6a310eda7037195cb64185d9",
                .descending = "rows 1252 md5 7af0fb9e5f93df6103f0d3e29b6ee746"},
    BundledSort{.type       = Type::NOSE_CONE,
                .column     = 7,
                .ascending  = "rows 1252 md5 950167a2198fbcfe7ca7775f7db4545b",
                .descending = "rows 1252 md5 5211084b8ea4ae1def45ed10dc3d4a79"},
    BundledSort{.type       = Type::NOSE_CONE,
                .column     = 11,
                .ascending  = "rows 1252 md5 64afdf27cde2664d10c048cd4ad5ef5b",
                .descending = "rows 1252 md5 b21bc2f612f6dfe189fe98808ac4049b"},
    BundledSort{.type       = Type::NOSE_CONE,
                .column     = 12,
                .ascending  = "rows 1252 md5 2563a61decae708dccae0da0e88443d5",
                .descending = "rows 1252 md5 8b89598d863376bd5b8037af4a727f4a"},
    BundledSort{.type       = Type::NOSE_CONE,
                .column     = 9,
                .ascending  = "rows 1252 md5 837d38ffdfaf8aa00c9cc69c5c31db1a",
                .descending = "rows 1252 md5 08a8dbffd38d53bad1d61a51baf2ae81"},
    BundledSort{.type       = Type::NOSE_CONE,
                .column     = 30,
                .ascending  = "rows 1252 md5 0c0c991ec53ec1ce23760892adf3f134",
                .descending = {}},
    BundledSort{.type       = Type::NOSE_CONE,
                .column     = 29,
                .ascending  = "rows 1252 md5 681f8e44be1399de46286df8ab256f1a",
                .descending = {}},
    BundledSort{.type       = Type::PARACHUTE,
                .column     = 1,
                .ascending  = "rows 422 md5 7e951cead4b667e9115386a24cca04fd",
                .descending = "rows 422 md5 6527f6b7e80348121ebe0a30f8cd4732"},
    BundledSort{.type       = Type::PARACHUTE,
                .column     = 2,
                .ascending  = "rows 422 md5 38fe7c2f3c733e4723c3e10d70e4d76f",
                .descending = "rows 422 md5 0e8193639092dd8dba178bbfa696eed1"},
    BundledSort{.type       = Type::PARACHUTE,
                .column     = 3,
                .ascending  = "rows 422 md5 665887dd4b164d7ddd672be3de3e71fc",
                .descending = "rows 422 md5 9053ec7f943c48c6a3892182fcd72014"},
    BundledSort{.type       = Type::PARACHUTE,
                .column     = 4,
                .ascending  = "rows 422 md5 360d65fb10c973d6c6aafcee24c7417e",
                .descending = "rows 422 md5 67f78b36202bbb90fa53ef9901c32f36"},
    BundledSort{.type       = Type::PARACHUTE,
                .column     = 21,
                .ascending  = "rows 422 md5 bfc8f81d9b65a2debffe086bcb26f728",
                .descending = "rows 422 md5 da79f5f838e9df2f15cbd133dada718f"},
    BundledSort{.type       = Type::PARACHUTE,
                .column     = 30,
                .ascending  = "rows 422 md5 7d7c6d4ca29776e0fd9aa6fcbce20e8b",
                .descending = "rows 422 md5 1cb0d5abb7c5026ea6650cbfb8004c92"},
    BundledSort{.type       = Type::PARACHUTE,
                .column     = 22,
                .ascending  = "rows 422 md5 25a6c15fce267c50122b834b11ade3f7",
                .descending = "rows 422 md5 f8a00667e0e16755d4e0e43d7be323b9"},
    BundledSort{.type       = Type::PARACHUTE,
                .column     = 23,
                .ascending  = "rows 422 md5 cea678a4a182b4efdac43b0fcd8ec30d",
                .descending = "rows 422 md5 a621ab63bf65f7f9452504823d053b69"},
    BundledSort{.type       = Type::PARACHUTE,
                .column     = 24,
                .ascending  = "rows 422 md5 c71793f97a870d50e9ac5954c3d75d88",
                .descending = "rows 422 md5 c4e3d1aca478f03cbfaa0d88e806a9f6"},
    BundledSort{.type       = Type::PARACHUTE,
                .column     = 25,
                .ascending  = "rows 422 md5 f1503af41719f114e3936583ded22052",
                .descending = "rows 422 md5 b77fc70ffad340f9517667604e70eea1"},
};

struct BundledText
{
    Type             type;
    std::string_view pattern;
    int              rows;
};

constexpr std::array kBundledTexts{
    BundledText{.type = Type::BODY_TUBE, .pattern = "estes", .rows = 806},
    BundledText{.type = Type::BODY_TUBE, .pattern = "BT-20", .rows = 69},
    BundledText{.type = Type::BODY_TUBE, .pattern = "e", .rows = 1496},
    BundledText{.type = Type::BODY_TUBE, .pattern = "true", .rows = 0},  // OpenRocket: 407
    BundledText{.type = Type::BODY_TUBE, .pattern = "Cardboard", .rows = 0},
    BundledText{.type = Type::BODY_TUBE, .pattern = "cm", .rows = 1496},
    BundledText{.type = Type::BODY_TUBE, .pattern = "ogive", .rows = 0},
    BundledText{.type = Type::BODY_TUBE, .pattern = "[", .rows = 0},  // OpenRocket: every row
    BundledText{.type = Type::NOSE_CONE, .pattern = "estes", .rows = 243},
    BundledText{.type = Type::NOSE_CONE, .pattern = "BT-20", .rows = 14},
    BundledText{.type = Type::NOSE_CONE, .pattern = "e", .rows = 1245},  // OpenRocket: 1252
    BundledText{.type = Type::NOSE_CONE, .pattern = "true", .rows = 0},  // OpenRocket: 1125
    BundledText{.type = Type::NOSE_CONE, .pattern = "Cardboard", .rows = 0},
    BundledText{.type = Type::NOSE_CONE, .pattern = "cm", .rows = 1252},
    BundledText{.type = Type::NOSE_CONE, .pattern = "ogive", .rows = 725},
    BundledText{.type = Type::NOSE_CONE, .pattern = "[", .rows = 0},  // OpenRocket: every row
    BundledText{.type = Type::CENTERING_RING, .pattern = "estes", .rows = 19},
    BundledText{.type = Type::CENTERING_RING, .pattern = "BT-20", .rows = 36},
    BundledText{.type = Type::CENTERING_RING, .pattern = "e", .rows = 695},
    BundledText{.type = Type::CENTERING_RING, .pattern = "true", .rows = 0},  // OpenRocket: 199
    BundledText{.type = Type::CENTERING_RING, .pattern = "Cardboard", .rows = 10},
    BundledText{.type = Type::CENTERING_RING, .pattern = "cm", .rows = 695},
    BundledText{.type = Type::CENTERING_RING, .pattern = "ogive", .rows = 0},
    BundledText{.type = Type::CENTERING_RING, .pattern = "[", .rows = 0},  // OpenRocket: every row
    BundledText{.type = Type::PARACHUTE, .pattern = "estes", .rows = 22},
    BundledText{.type = Type::PARACHUTE, .pattern = "BT-20", .rows = 0},
    BundledText{.type = Type::PARACHUTE, .pattern = "e", .rows = 422},
    BundledText{.type = Type::PARACHUTE, .pattern = "true", .rows = 0},  // OpenRocket: 41
    BundledText{.type = Type::PARACHUTE, .pattern = "Cardboard", .rows = 0},
    BundledText{.type = Type::PARACHUTE, .pattern = "cm", .rows = 422},
    BundledText{.type = Type::PARACHUTE, .pattern = "ogive", .rows = 0},
    BundledText{.type = Type::PARACHUTE, .pattern = "[", .rows = 0},  // OpenRocket: every row
};

TEST_F(ComponentPresetFilterModelTest, TheBundledPresetsShowAsInOpenRocket)
{
    // probe ("== bundled views"): every type sorted by Manufacturer, with the legacy rows and
    // without them.
    const ComponentPresetDatabase& database = QtRocket::Test::bundledPresetDatabase();
    for (const BundledView& view : kBundledViews)
    {
        FilteredTable table(database, view.type);
        EXPECT_EQ(presetDigestOfView(table.filter), view.all) << componentPresetTypeName(view.type);
        table.filter.setHideLegacy(true);
        EXPECT_EQ(presetDigestOfView(table.filter), view.legacyHidden)
            << componentPresetTypeName(view.type);
    }
}

/// The sorts of kBundledSorts whose view differs, as "<type> <column> <order>: <view>".
[[nodiscard]] std::vector<std::string> bundledSortMismatches(
    const ComponentPresetDatabase& database)
{
    std::vector<std::string> mismatches;
    const auto check = [&mismatches](const BundledSort& sort, SortOrder order,
                                     std::string_view expected, const Filter& filter) {
        const std::string view = presetDigestOfView(filter);
        if (view != expected)
        {
            mismatches.push_back(std::format("{} {} {}: {}", componentPresetTypeName(sort.type),
                                             sort.column,
                                             order == SortOrder::ASCENDING ? "asc" : "desc", view));
        }
    };
    for (const BundledSort& sort : kBundledSorts)
    {
        FilteredTable table(database, sort.type);
        table.filter.setSortKeys({SortKey{.column = sort.column, .order = SortOrder::ASCENDING}});
        check(sort, SortOrder::ASCENDING, sort.ascending, table.filter);
        if (!sort.descending.empty())
        {
            table.filter.setSortKeys(
                {SortKey{.column = sort.column, .order = SortOrder::DESCENDING}});
            check(sort, SortOrder::DESCENDING, sort.descending, table.filter);
        }
    }
    return mismatches;
}

TEST_F(ComponentPresetFilterModelTest, TheBundledPresetsSortAsInOpenRocket)
{
    // probe ("== bundled sorts"): the displayed columns of the body tubes, nose cones and
    // parachutes, ascending and descending, the nose cones' hidden Material and Finish, and three
    // keys.
    const ComponentPresetDatabase& database = QtRocket::Test::bundledPresetDatabase();
    EXPECT_EQ(bundledSortMismatches(database), std::vector<std::string>{});
    // probe: "[4 desc, 9 asc, 2 asc]".
    const std::vector<SortKey> keys{SortKey{.column = 4, .order = SortOrder::DESCENDING},
                                    SortKey{.column = 9, .order = SortOrder::ASCENDING},
                                    SortKey{.column = 2, .order = SortOrder::ASCENDING}};
    FilteredTable              tubes(database, Type::BODY_TUBE);
    tubes.filter.setSortKeys(keys);
    EXPECT_EQ(presetDigestOfView(tubes.filter), "rows 1496 md5 650dc7cf19d7b340ff17057803e9c82a");
    FilteredTable noses(database, Type::NOSE_CONE);
    noses.filter.setSortKeys(keys);
    EXPECT_EQ(presetDigestOfView(noses.filter), "rows 1252 md5 09661386fefb5e35fb21f602201ec1d8");
    FilteredTable chutes(database, Type::PARACHUTE);
    chutes.filter.setSortKeys(keys);
    EXPECT_EQ(presetDigestOfView(chutes.filter), "rows 422 md5 67f78b36202bbb90fa53ef9901c32f36");
}

TEST_F(ComponentPresetFilterModelTest, TheBundledPresetsFilterByTextAsFixed)
{
    // probe ("== bundled text filters", "fixed"): with the legacy rows; "Cardboard" finds ten
    // centering rings by their hidden Material.
    const ComponentPresetDatabase& database = QtRocket::Test::bundledPresetDatabase();
    for (const BundledText& text : kBundledTexts)
    {
        FilteredTable table(database, text.type);
        table.filter.setTextFilter(QtStrings::fromUtf8(text.pattern));
        EXPECT_EQ(table.filter.rowCount(), text.rows)
            << componentPresetTypeName(text.type) << " '" << text.pattern << "'";
    }
}

/// The view of the bundled presets of @p type without the legacy rows and with the given
/// diameter filters.
[[nodiscard]] std::string bundledDiameterView(Type type, std::optional<double> fore,
                                              std::optional<double> aft)
{
    FilteredTable table(QtRocket::Test::bundledPresetDatabase(), type);
    table.filter.setHideLegacy(true);
    table.filter.setForeDiameterFilter(fore);
    table.filter.setAftDiameterFilter(aft);
    return presetDigestOfView(table.filter);
}

TEST_F(ComponentPresetFilterModelTest, TheBundledPresetsFilterByDiameterAsInOpenRocket)
{
    // probe ("== bundled diameter filters"): the M3 scouting's rocket's boxes (its dialog's row
    // counts at open: 93, 0, 129 with the fore box off, 11, 54 and 105).
    EXPECT_EQ(bundledDiameterView(Type::NOSE_CONE, std::nullopt, 0.0248),
              "rows 93 md5 26772867017071affefecb63f83f474d");
    EXPECT_EQ(bundledDiameterView(Type::BODY_TUBE, 0.05, 0.0248),
              "rows 0 md5 d41d8cd98f00b204e9800998ecf8427e");
    EXPECT_EQ(bundledDiameterView(Type::BODY_TUBE, std::nullopt, 0.0248),
              "rows 129 md5 57efe5ffabe925b2f5f4c8e7d7d510a3");
    EXPECT_EQ(bundledDiameterView(Type::TRANSITION, 0.0248, 0.0416),
              "rows 11 md5 ebfb49e11d3758119462dd2a4c8b9ab1");
    EXPECT_EQ(bundledDiameterView(Type::TRANSITION, std::nullopt, 0.0416),
              "rows 54 md5 5bb9773b93ff135012d86decff6fb5a3");
    EXPECT_EQ(bundledDiameterView(Type::BODY_TUBE, 0.0416, std::nullopt),
              "rows 133 md5 8fa2e7b8f7ffa5cf74fd189f850f2a7a");
    EXPECT_EQ(bundledDiameterView(Type::NOSE_CONE, std::nullopt, 0.0416),
              "rows 105 md5 16ba25be024736abc4e5ae41018c865c");
}

}  // namespace

}  // namespace QtRocket::Gui
