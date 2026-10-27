// The model of the parts library's table (Java: ComponentPresetTable's AbstractTableModel and
// ComponentPresetTableColumn, swing gui/dialogs/preset/). OpenRocket has no JUnit test of either.
//
// Expectations marked "probe" are OpenRocket's own results, printed by
// tools/openrocket-gui-pins/preset-chooser/probe/ComponentPresetTableProbe.java (run on OpenRocket
// 5f164fd0e with JDK 17; tools/openrocket-gui-pins/preset-chooser/hand-pinned.txt keeps its
// transcript): ComponentPresetTable over the presets of smallPresetDatabase() (the same three files
// in the same order), its favourites in SwingPreferences, and over the bundled presets.

#include "QtRocket/gui/dialogs/preset/ComponentPresetTableModel.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QAbstractItemModelTester>
#include <QModelIndex>
#include <QSignalSpy>
#include <QString>
#include <QVariant>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "PresetTableTestSupport.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetFavorites.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "rocket/preset/TestPresetDatabase.h"

namespace QtRocket::Gui
{

namespace
{

using ComponentPresetTableModelTest = Test::GuiTest;
using Model                         = ComponentPresetTableModel;
using Type                          = ComponentPresetType;
using Preset                        = ComponentPreset;
using Test::presetCellsLine;

/// probe ("== cells"): the texts of every model column of every preset of the small database,
/// by type (in Type.values() order) and row, the empty ones left out.
constexpr std::array<std::string_view, 43> kCells{
    "0=false | 1=false | 2=Estes | 3=30615 | 4=Body tube, 2.0 dia, white, 13.5 in., PN 30615 | "
    "5=5.08 cm | 8=4.91 cm | 9=34.3 cm | 19=0.086 cm | 30=Paper, spiral kraft glassine, Estes avg, "
    "bulk (0.894 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=31360 | 4=Body tube, paper, 29mm ID, white, 8.0 in., PN 31360 "
    "| 5=3.07 cm | 8=2.9 cm | 9=20.3 cm | 19=0.086 cm | 30=Paper, spiral kraft glassine, Estes "
    "avg, bulk (0.894 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=31361 | 4=Body tube, paper, 29mm+, green, 0.5 in., PN 31361 | "
    "5=4.88 cm | 8=3.09 cm | 9=1.27 cm | 19=0.894 cm | 30=Fiber, bulk (0.657 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=31380 | 4=Body tube, 2.5 dia, white, 6.5 in., PN 31390 | "
    "5=6.35 cm | 8=6.18 cm | 9=16.5 cm | 19=0.086 cm | 30=Paper, spiral kraft glassine, Estes avg, "
    "bulk (0.894 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=31390 | 4=Body tube, 2.5 dia, white, 15.5 in., PN 31390 | "
    "5=6.35 cm | 8=6.18 cm | 9=39.4 cm | 19=0.086 cm | 30=Paper, spiral kraft glassine, Estes avg, "
    "bulk (0.894 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=31751 | 4=Body tube, paper, 3.0 dia, white, 10.0 in., 4 "
    "slots, PN 31751 | 5=7.62 cm | 8=7.45 cm | 9=25.4 cm | 19=0.086 cm | 30=Paper, spiral kraft "
    "glassine, Estes avg, bulk (0.894 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=31799 | 4=Body tube, paper, 3.0 dia, white, 21.5 in., PN "
    "31799 | 5=7.62 cm | 8=7.45 cm | 9=54.6 cm | 19=0.086 cm | 30=Paper, spiral kraft glassine, "
    "Estes avg, bulk (0.894 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=85865 | 4=Body tube, 2.5 dia, white, 15.5 in., 3 slots, PN "
    "85865 | 5=6.35 cm | 8=6.18 cm | 9=39.4 cm | 19=0.086 cm | 30=Paper, spiral kraft glassine, "
    "Estes avg, bulk (0.894 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=85866 | 4=Body tube, 2.0 dia, white, 13.5 in., 4 slots, PN "
    "85866 | 5=5.08 cm | 8=4.91 cm | 9=34.3 cm | 19=0.086 cm | 30=Paper, spiral kraft glassine, "
    "Estes avg, bulk (0.894 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=85867 | 4=Body tube, paper, 3.0 dia, white, 23.67 in., 4 "
    "slots, PN 31751 | 5=7.62 cm | 8=7.45 cm | 9=60.1 cm | 19=0.086 cm | 30=Paper, spiral kraft "
    "glassine, Estes avg, bulk (0.894 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=85868 | 4=Body tube, paper, 4.0\" dia, white, 20.75 in., PN "
    "85868 | 5=10.2 cm | 8=9.91 cm | 9=52.7 cm | 19=0.127 cm | 30=Paper, spiral kraft glassine, "
    "Estes avg, bulk (0.894 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=BT-20 | 4=Body tube, BT-20, 18 in. | 5=1.87 cm | 8=1.8 cm | "
    "9=45.7 cm | 19=0.033 cm | 30=Paper, spiral kraft glassine, bulk (0.799 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=72413 | 4=Nose cone, plastic, 2.5\" series, 10.75\" len, PN "
    "72413 | 7=6.35 cm | 9=24.8 cm | 11=6.15 cm | 12=6.17 cm | 18=Ogive | 19=0.318 cm | 20=false | "
    "30=Polystyrene, cast, bulk (1.05 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=72415 | 4=Nose cone, plastic, 4.0\" series, 12.5\" ogive, PN "
    "72415 | 7=10.2 cm | 9=32.4 cm | 11=9.86 cm | 12=9.52 cm | 18=Ogive | 19=0.318 cm | "
    "30=Polystyrene, cast, bulk (1.05 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=72653 | 4=Nose cone, plastic, 2.0\" series, 9.6\" len, PN "
    "72653 | 7=5.08 cm | 9=24.4 cm | 11=4.88 cm | 12=5.08 cm | 18=Ogive | 19=0.318 cm | 20=false | "
    "30=Polystyrene, cast, bulk (1.05 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=72695 | 4=Nose cone, plastic, 3.0\" series, 8.0\" len, PN "
    "72695 | 7=7.62 cm | 9=20.3 cm | 11=7.42 cm | 12=7.62 cm | 18=Ogive | 19=0.318 cm | 20=false | "
    "30=Polystyrene, cast, bulk (1.05 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=72753 | 4=Nose cone, plastic, 3.0\" series, 17.4\" len, PN "
    "72753 | 7=7.62 cm | 9=44.2 cm | 11=7.42 cm | 12=7.62 cm | 18=Conical | 19=0.318 cm | 20=false "
    "| 30=Polystyrene, cast, bulk (1.05 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=PNC-20A | 4=Nose cone, plastic, BT-20 | 7=1.87 cm | 9=6.98 cm "
    "| 11=1.8 cm | 12=1.9 cm | 18=Ogive | 19=0.157 cm | 20=false | 26=8.5 g | 30=Polystyrene, "
    "cast, bulk (1.51 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=72657 | 4=Transition, plastic, PSII 2.0 to 2.5 series, "
    "increasing, PN 72657 | 6=5.08 cm | 7=6.35 cm | 9=5.72 cm | 11=6.15 cm | 12=6.35 cm | 13=4.88 "
    "cm | 14=5.08 cm | 18=Conical | 19=0.318 cm | 30=Polystyrene, cast, bulk (1.05 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=TA-2050 | 4=Transition, BT-20 to BT-50, balsa | 6=1.87 cm | "
    "7=2.48 cm | 9=3.18 cm | 11=2.41 cm | 12=1.27 cm | 13=1.8 cm | 14=1.27 cm | 18=Conical | "
    "20=true | 30=Balsa, bulk (0.128 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=30189 | 4=Tube coupler, 2.5\" series, paper, kraft, PN 30189 "
    "| 5=6.15 cm | 8=5.99 cm | 9=10.2 cm | 19=0.076 cm | 30=Fiber, bulk (0.657 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=30618 | 4=Tube coupler, 2.0\" series, paper, white,PN 30618 | "
    "5=4.88 cm | 8=4.72 cm | 9=10.2 cm | 19=0.076 cm | 30=Paper, spiral kraft glassine, Estes avg, "
    "bulk (0.894 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=31800 | 4=Tube coupler, 3.0\" series, paper, PN 31800 | "
    "5=7.42 cm | 8=7.26 cm | 9=10.2 cm | 19=0.076 cm | 30=Fiber, bulk (0.657 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=JT-20C | 4=Tube coupler, BT-20 | 5=1.8 cm | 8=1.7 cm | 9=2.5 "
    "cm | 19=0.05 cm | 26=1.2 g | 30=Paper, spiral kraft glassine, bulk (1.75 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=BHB-20 | 4=Bulkhead, balsa, BT-20 | 5=1.79 cm | 9=0.635 cm | "
    "30=Balsa (0.17 g/cm\u00b3)",
    "0=false | 1=true | 2=Quest | 3=BH-1  | 4=Bulkhead\tplywood | 5=2.41 cm | 9=0.318 cm | 26=2.83 "
    "g | 30=Plywood (birch) (1.95 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=66468 | 4=Centering ring, plywood, 29mm to PSII 3.0\", PN "
    "66468 | 5=7.43 cm | 8=3.08 cm | 9=0.318 cm | 19=2.18 cm | 30=Plywood, light, bulk (0.352 "
    "g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=66475 | 4=Centering ring, plywood, 29mm to PSII 2.5\", PN "
    "66475 | 5=6.16 cm | 8=3.08 cm | 9=0.318 cm | 19=1.54 cm | 30=Plywood, light, bulk (0.352 "
    "g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=66540 | 4=Centering ring, plywood, 29mm to PSII 2.0\", PN "
    "66540 | 5=4.89 cm | 8=3.08 cm | 9=0.318 cm | 19=0.907 cm | 30=Plywood, light, bulk (0.352 "
    "g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=66611 | 4=Centering ring, plywood, 29mm to PSII 3.0\", "
    "slotted, PN 66611 | 5=7.43 cm | 8=3.08 cm | 9=0.318 cm | 19=2.18 cm | 30=Plywood, light, bulk "
    "(0.352 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=66735 | 4=Centering ring, plywood, 29mm to PSII 4.0\", PN "
    "66735 | 5=9.89 cm | 8=3.08 cm | 9=0.318 cm | 19=3.41 cm | 30=Plywood, light, bulk (0.352 "
    "g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=66736 | 4=Centering ring, plywood, 29mm to PSII 4.0\", 3 fin "
    "slots, PN 66736 | 5=9.89 cm | 8=3.08 cm | 9=0.318 cm | 19=3.41 cm | 30=Plywood, light, bulk "
    "(0.352 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=66743 | 4=Centering ring, plywood, 29mm to PSII 4.0\", shock "
    "cord slot, PN 66743 | 5=9.89 cm | 8=3.08 cm | 9=0.318 cm | 19=3.41 cm | 30=Plywood, light, "
    "bulk (0.352 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=RA-1320 | 4=Centering ring, BT-5 to BT-20 | 5=1.8 cm | 8=1.38 "
    "cm | 9=0.159 cm | 19=0.212 cm | 30=Paper, spiral kraft glassine, bulk (0.799 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=31362 | 4=Engine block, fiber, green, 29mm, 0.25\" len, PN "
    "31362 | 5=2.87 cm | 8=2.22 cm | 9=0.635 cm | 19=0.324 cm | 30=Fiber, bulk (0.657 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=EB-20A | 4=Engine block, BT-20 | 5=1.8 cm | 8=1.27 cm | "
    "9=0.635 cm | 19=0.267 cm | 30=Paper, spiral kraft glassine, bulk (0.799 g/cm\u00b3)",
    "0=false | 1=false | 2=Estes | 3=LL-2A | 4=Launch lug, 1/8 in. x 2 in. | 5=0.396 cm | 8=0.33 "
    "cm | 9=5.08 cm | 19=0.033 cm | 30=Paper, spiral kraft glassine, bulk (0.799 g/cm\u00b3)",
    "0=false | 1=false | 2=Rail-Buttons.com | 3=1010 | 4=Standard 1010 rail button | 5=1.11 cm | "
    "8=0.58 cm | 15=0.185 cm | 16=0.185 cm | 17=0 cm | 26=0.445 g | 27=1.4 g | 28=1.03 g | "
    "30=Delrin (0.964 g/cm\u00b3)",
    "0=false | 1=true | 2=Quest | 3=ST-1 | 4=Streamer, mylar, 1 in. x 30 in. | 9=76.2 cm | 10=2.54 "
    "cm | 19=0.003 cm | 30=Mylar, 1 mil (35.5 g/m\u00b2)",
    "0=false | 1=false | 2=Top Flight Recovery | 3=STR-2 | 4=Streamer, 2 in. x 40 in. | 9=102 cm | "
    "10=5.08 cm | 19=0.005 cm | 30=Nylon fabric, ripstop, 1.7 oz actual (67 g/m\u00b2)",
    "0=false | 1=false | 2=Estes | 3=002261 | 4=Parachute, 24 in., nylon, 6 lines, PN 002261 | "
    "21=61 cm | 22=6 | 23=6 | 24=61 cm | 25=Nylon Paracord, 110 lb, 1/16 in. dia. (1.6 g/m) | "
    "30=Nylon fabric, ripstop, 1.7 oz actual (57.6 g/m\u00b2)",
    "0=false | 1=false | 2=Estes | 3=002273 | 4=Parachute, 30 in., nylon, 8 lines, PN 002273 | "
    "21=76.2 cm | 22=8 | 23=8 | 24=76.2 cm | 25=Nylon Paracord, 110 lb, 1/16 in. dia. (1.6 g/m) | "
    "30=Nylon fabric, ripstop, 1.7 oz actual (57.6 g/m\u00b2)",
    "0=false | 1=false | 2=Estes | 3=PK-12 | 4=Parachute, 12 in., 6 lines | 21=30.5 cm | 22=6 | "
    "23=6 | 24=30.5 cm | 25=Nylon Paracord, 110 lb, 1/16 in. dia. (0.97 g/m) | 30=Nylon fabric, "
    "ripstop, 1.7 oz actual (67 g/m\u00b2)",
};

/// The presets of the small database of @p type and an empty preference store.
struct SmallTable
{
    InMemoryPreferences preferences;
    Model               model;

    explicit SmallTable(Type type)
      : model(type, QtRocket::Test::smallPresetDatabase().listForType(type), preferences, nullptr)
    {
    }
};

/// Every cell text of the small database's tables, in kCells' order.
[[nodiscard]] std::vector<std::string> smallDatabaseCells()
{
    std::vector<std::string> lines;
    for (const Type type : kAllComponentPresetTypes)
    {
        const SmallTable table(type);
        for (int row = 0; row < table.model.rowCount(); ++row)
        {
            lines.push_back(presetCellsLine(table.model, row));
        }
    }
    return lines;
}

/// The texts of @p column of every row of @p model, separated by " | ".
[[nodiscard]] std::string columnTexts(const Model& model, int column)
{
    std::string line;
    for (int row = 0; row < model.rowCount(); ++row)
    {
        line += (row == 0 ? "" : " | ") + QtStrings::toUtf8(model.cellText(row, column));
    }
    return line;
}

/// The model columns @p type's table shows, in model order.
[[nodiscard]] std::vector<int> displayedColumns(Type type)
{
    std::vector<int> columns;
    for (int column = 0; column < Model::kColumnCount; ++column)
    {
        if (Model::isDisplayed(type, column))
        {
            columns.push_back(column);
        }
    }
    return columns;
}

/// The columns of @p group's unit group, and the Boolean columns.
[[nodiscard]] std::vector<int> columnsOf(std::optional<UnitGroupId> group)
{
    std::vector<int> columns;
    for (int column = 0; column < Model::kColumnCount; ++column)
    {
        if (Model::columnUnitGroup(column) == group)
        {
            columns.push_back(column);
        }
    }
    return columns;
}

[[nodiscard]] std::vector<int> booleanColumns()
{
    std::vector<int> columns;
    for (int column = 0; column < Model::kColumnCount; ++column)
    {
        if (Model::isBooleanColumn(column))
        {
            columns.push_back(column);
        }
    }
    return columns;
}

/// The English headers, which the columns are stored under, equal the headers without a
/// translator.
[[nodiscard]] std::vector<std::string> headersDifferingFromTheirIdentifiers()
{
    std::vector<std::string> differing;
    for (int column = 0; column < Model::kColumnCount; ++column)
    {
        if (QtStrings::toUtf8(Model::columnName(column)) != Model::columnIdentifier(column))
        {
            differing.emplace_back(Model::columnIdentifier(column));
        }
    }
    return differing;
}

/// The rows of @p model that are favourites.
[[nodiscard]] std::vector<int> favoriteRows(const Model& model)
{
    std::vector<int> rows;
    for (int row = 0; row < model.rowCount(); ++row)
    {
        if (model.isFavorite(row))
        {
            rows.push_back(row);
        }
    }
    return rows;
}

TEST_F(ComponentPresetTableModelTest, TheColumnsAreTheFavouriteAndTheOrderedKeys)
{
    // probe: "model 31 columns", the favourite a Boolean column, the others Object ones.
    const SmallTable table(Type::BODY_TUBE);
    EXPECT_EQ(table.model.columnCount(), 31);
    EXPECT_EQ(Model::kColumnCount, 31);
    EXPECT_EQ(Model::columnKey(0), std::nullopt);
    EXPECT_EQ(Model::columnKey(1), std::optional<AnyTypedKey>(Preset::kLegacy));
    EXPECT_EQ(Model::columnKey(3), std::optional<AnyTypedKey>(Preset::kPartNo));
    EXPECT_EQ(Model::columnKey(30), std::optional<AnyTypedKey>(Preset::kMaterial));
    EXPECT_EQ(Model::columnOf(Preset::kOuterDiameter), 5);
    EXPECT_EQ(Model::columnOf(Preset::kFinish), 29);
    EXPECT_EQ(Model::columnOf(Preset::kType), -1);
    EXPECT_EQ(Model::columnOf(Preset::kCd), -1);
    EXPECT_THROW(static_cast<void>(Model::columnKey(31)), BugError);
    EXPECT_THROW(static_cast<void>(Model::columnName(-1)), BugError);
    // probe: "Boolean columns 0 1 20".
    EXPECT_EQ(booleanColumns(), (std::vector<int>{0, 1, 20}));
    // probe: the headers ("... Favorite [0] | Legacy [1] | ... | Material [30]").
    EXPECT_EQ(Model::columnIdentifier(0), "\u2026 Favorite");
    EXPECT_EQ(Model::columnIdentifier(3), "Part Number");
    EXPECT_EQ(Model::columnIdentifier(18), "Shape");
    EXPECT_EQ(Model::columnIdentifier(25), "Line Material");
    EXPECT_TRUE(headersDifferingFromTheirIdentifiers().empty());
    EXPECT_EQ(table.model.headerData(5, Qt::Horizontal, Qt::DisplayRole).toString(),
              QStringLiteral("Outer Diameter"));
}

TEST_F(ComponentPresetTableModelTest, EachTypeShowsTheFavouriteAndItsDisplayedKeys)
{
    // probe ("== columns"): the table's column model of each type, in model order.
    EXPECT_EQ(displayedColumns(Type::BODY_TUBE), (std::vector<int>{0, 1, 2, 3, 4, 5, 8, 9}));
    EXPECT_EQ(displayedColumns(Type::NOSE_CONE),
              (std::vector<int>{0, 1, 2, 3, 4, 7, 9, 11, 12, 18}));
    EXPECT_EQ(displayedColumns(Type::TRANSITION),
              (std::vector<int>{0, 1, 2, 3, 4, 6, 7, 9, 11, 12, 13, 14, 18}));
    EXPECT_EQ(displayedColumns(Type::TUBE_COUPLER), (std::vector<int>{0, 1, 2, 3, 4, 5, 8, 9}));
    EXPECT_EQ(displayedColumns(Type::BULK_HEAD), (std::vector<int>{0, 1, 2, 3, 4, 5, 9}));
    EXPECT_EQ(displayedColumns(Type::CENTERING_RING), (std::vector<int>{0, 1, 2, 3, 4, 5, 8, 9}));
    EXPECT_EQ(displayedColumns(Type::ENGINE_BLOCK), (std::vector<int>{0, 1, 2, 3, 4, 5, 8, 9}));
    EXPECT_EQ(displayedColumns(Type::LAUNCH_LUG), (std::vector<int>{0, 1, 2, 3, 4, 5, 8, 9}));
    // The rail button's Height and Drag Coefficient are no columns (not in ORDERED_KEY_LIST).
    EXPECT_EQ(displayedColumns(Type::RAIL_BUTTON),
              (std::vector<int>{0, 1, 2, 3, 4, 5, 8, 15, 16, 17, 26, 27, 28}));
    EXPECT_EQ(displayedColumns(Type::STREAMER), (std::vector<int>{0, 1, 2, 3, 4, 9, 10, 19, 30}));
    EXPECT_EQ(displayedColumns(Type::PARACHUTE),
              (std::vector<int>{0, 1, 2, 3, 4, 21, 22, 23, 24, 25, 30}));
}

TEST_F(ComponentPresetTableModelTest, TheLengthsAndMassesHaveUnits)
{
    // probe: the DoubleWithUnit columns' menus ("[Units: mm *cm m in in/64 ft]", "[Units: *g kg
    // oz lb]"), their units the groups' defaults ("Length's unit cm").
    EXPECT_EQ(columnsOf(UnitGroupId::LENGTH),
              (std::vector<int>{5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 21, 24}));
    EXPECT_EQ(columnsOf(UnitGroupId::MASS), (std::vector<int>{26, 27, 28}));
    EXPECT_EQ(columnsOf(std::nullopt),
              (std::vector<int>{0, 1, 2, 3, 4, 18, 20, 22, 23, 25, 29, 30}));
    const SmallTable table(Type::RAIL_BUTTON);
    ASSERT_NE(table.model.columnUnit(9), nullptr);
    EXPECT_EQ(table.model.columnUnit(9)->getUnit(), "cm");
    ASSERT_NE(table.model.columnUnit(26), nullptr);
    EXPECT_EQ(table.model.columnUnit(26)->getUnit(), "g");
    EXPECT_EQ(table.model.columnUnit(22), nullptr);
}

TEST_F(ComponentPresetTableModelTest, TheCellTextsAreJavasToString)
{
    // probe ("== cells"): Booleans as "true" and "false", lengths and masses in the default unit,
    // the manufacturer's display name, the shape's name, a material with its density, and the
    // part number and description as they are (Quest's "BH-1 " with its space, its description
    // with a tab).
    const std::vector<std::string> cells = smallDatabaseCells();
    ASSERT_EQ(cells.size(), kCells.size());
    for (std::size_t i = 0; i < cells.size(); ++i)
    {
        EXPECT_EQ(cells.at(i), kCells.at(i)) << "line " << i;
    }
}

TEST_F(ComponentPresetTableModelTest, TheDisplayRoleShowsTheTextAndTheFavouriteACheckBox)
{
    const SmallTable table(Type::NOSE_CONE);
    const Model&     model = table.model;
    // probe: row 1 has no Filled (20) value; row 0's is "false".
    EXPECT_EQ(model.data(model.index(0, 20), Qt::DisplayRole).toString(), QStringLiteral("false"));
    EXPECT_FALSE(model.data(model.index(1, 20), Qt::DisplayRole).isValid());
    EXPECT_FALSE(model.hasValue(1, 20));
    EXPECT_EQ(model.cellText(1, 20), QString());
    EXPECT_EQ(model.booleanValue(0, 20), std::optional<bool>(false));
    EXPECT_EQ(model.booleanValue(1, 20), std::nullopt);
    EXPECT_EQ(model.booleanValue(0, 9), std::nullopt);
    EXPECT_EQ(model.numberValue(0, 7), std::optional<double>(0.0635));
    EXPECT_EQ(model.numberValue(0, 5), std::nullopt);
    EXPECT_EQ(model.numberValue(0, 3), std::nullopt);
    // The favourite: a check box, no text.
    EXPECT_FALSE(model.data(model.index(0, 0), Qt::DisplayRole).isValid());
    EXPECT_EQ(model.data(model.index(0, 0), Qt::CheckStateRole).toInt(),
              static_cast<int>(Qt::Unchecked));
    EXPECT_FALSE(model.data(model.index(0, 1), Qt::CheckStateRole).isValid());
    // probe: "editable: true false" (only the favourite).
    EXPECT_TRUE(model.flags(model.index(0, 0)).testFlag(Qt::ItemIsUserCheckable));
    EXPECT_FALSE(model.flags(model.index(0, 1)).testFlag(Qt::ItemIsUserCheckable));
    EXPECT_FALSE(model.flags(model.index(0, 1)).testFlag(Qt::ItemIsEditable));
    EXPECT_TRUE(model.flags(model.index(0, 1)).testFlag(Qt::ItemIsSelectable));
}

TEST_F(ComponentPresetTableModelTest, TickingAFavouriteStoresIt)
{
    SmallTable       table(Type::BODY_TUBE);
    Model&           model = table.model;
    const QSignalSpy changed(&model, &Model::dataChanged);
    const QSignalSpy favorites(&model, &Model::favoritesChanged);
    // probe: "model row 5 ticked: favourites 5 Estes|31751".
    EXPECT_TRUE(
        model.setData(model.index(5, 0), static_cast<int>(Qt::Checked), Qt::CheckStateRole));
    EXPECT_TRUE(model.isFavorite(5));
    EXPECT_EQ(model.data(model.index(5, 0), Qt::CheckStateRole).toInt(),
              static_cast<int>(Qt::Checked));
    EXPECT_EQ(model.cellText(5, 0), QStringLiteral("true"));
    EXPECT_EQ(ComponentPresetFavorites::getFavorites(table.preferences, Type::BODY_TUBE),
              (ComponentPresetFavorites::FavoriteKeys{"Estes|31751"}));
    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(favorites.count(), 1);
    // Only the favourite can be set.
    EXPECT_FALSE(
        model.setData(model.index(5, 1), static_cast<int>(Qt::Checked), Qt::CheckStateRole));
    EXPECT_FALSE(model.setData(model.index(5, 0), true, Qt::EditRole));
    // probe: "unticked: favourites []".
    model.setFavorite(5, false);
    EXPECT_FALSE(model.isFavorite(5));
    EXPECT_TRUE(ComponentPresetFavorites::getFavorites(table.preferences, Type::BODY_TUBE).empty());
    // The favourites stored before the model is made are read.
    ComponentPresetFavorites::setFavorite(table.preferences, *model.preset(2), Type::BODY_TUBE,
                                          true);
    const Model again(Type::BODY_TUBE, model.presets(), table.preferences, nullptr);
    EXPECT_TRUE(again.isFavorite(2));
    EXPECT_FALSE(model.isFavorite(2));
    model.updateFavorites();
    EXPECT_TRUE(model.isFavorite(2));
}

TEST_F(ComponentPresetTableModelTest, AUnitChangesTheTextsOfItsColumn)
{
    SmallTable       table(Type::BODY_TUBE);
    Model&           model = table.model;
    const QSignalSpy changed(&model, &Model::dataChanged);
    const QSignalSpy units(&model, &Model::unitChanged);
    const Unit*      millimetres = unitGroup(UnitGroupId::LENGTH).getUnit("mm");
    ASSERT_NE(millimetres, nullptr);
    model.setColumnUnit(5, *millimetres);
    // probe: "mm chosen: 50.8 mm | 30.7 mm | ..."; "a new table: 5.08 cm".
    EXPECT_EQ(columnTexts(model, 5),
              "50.8 mm | 30.7 mm | 48.8 mm | 63.5 mm | 63.5 mm | 76.2 mm | "
              "76.2 mm | 63.5 mm | 50.8 mm | 76.2 mm | 102 mm | 18.7 mm");
    EXPECT_EQ(model.columnUnit(5), millimetres);
    EXPECT_EQ(model.cellText(0, 8), QStringLiteral("4.91 cm"));
    ASSERT_EQ(changed.count(), 1);
    EXPECT_EQ(changed.at(0).at(0).value<QModelIndex>(), model.index(0, 5));
    EXPECT_EQ(changed.at(0).at(1).value<QModelIndex>(), model.index(11, 5));
    ASSERT_EQ(units.count(), 1);
    EXPECT_EQ(units.at(0).at(0).toInt(), 5);
    const SmallTable fresh(Type::BODY_TUBE);
    EXPECT_EQ(fresh.model.cellText(0, 5), QStringLiteral("5.08 cm"));
    // Only a unit of the column's group.
    const Unit* grams = unitGroup(UnitGroupId::MASS).getUnit("g");
    ASSERT_NE(grams, nullptr);
    EXPECT_THROW(model.setColumnUnit(5, *grams), BugError);
    EXPECT_THROW(model.setColumnUnit(3, *millimetres), BugError);
    EXPECT_EQ(model.columnUnit(5), millimetres);
}

TEST_F(ComponentPresetTableModelTest, ThePresetsMustBeOfTheTablesType)
{
    InMemoryPreferences                 preferences;
    const ComponentPresetDatabase       database = QtRocket::Test::smallPresetDatabase();
    std::vector<Model::PresetPtr>       presets  = database.listForType(Type::BODY_TUBE);
    const std::vector<Model::PresetPtr> noses    = database.listForType(Type::NOSE_CONE);
    std::vector<Model::PresetPtr>       mixed    = presets;
    mixed.push_back(noses.front());
    EXPECT_THROW(Model(Type::BODY_TUBE, mixed, preferences, nullptr), BugError);
    std::vector<Model::PresetPtr> withNull = presets;
    withNull.emplace_back(nullptr);
    EXPECT_THROW(Model(Type::BODY_TUBE, withNull, preferences, nullptr), BugError);
    const Model model(Type::BODY_TUBE, presets, preferences, nullptr);
    EXPECT_THROW(static_cast<void>(model.preset(12)), BugError);
    EXPECT_THROW(static_cast<void>(model.cellText(-1, 0)), BugError);
    EXPECT_EQ(model.preset(0), presets.front());
}

TEST_F(ComponentPresetTableModelTest, TheModelPassesQAbstractItemModelTester)
{
    SmallTable                     table(Type::NOSE_CONE);
    const QAbstractItemModelTester tester(&table.model,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    table.model.setFavorite(2, true);
    const Unit* inches = unitGroup(UnitGroupId::LENGTH).getUnit("in");
    ASSERT_NE(inches, nullptr);
    table.model.setColumnUnit(7, *inches);
    EXPECT_EQ(table.model.cellText(0, 7), QStringLiteral("2.5 in"));
}

TEST_F(ComponentPresetTableModelTest, PresetsWithOneFavouriteKeyAreTickedTogether)
{
    // probe ("== bundled favourites"): "'BalsaMachining|T5Plus-34' of model rows [54, 55]: row 54
    // ticked, ticked rows [54, 55]"; "row 55 unticked: favourites []".
    InMemoryPreferences preferences;
    Model model(Type::BODY_TUBE,
                QtRocket::Test::bundledPresetDatabase().listForType(Type::BODY_TUBE), preferences,
                nullptr);
    ASSERT_EQ(model.rowCount(), 1496);
    EXPECT_EQ(model.preset(54)->preferenceKey(), "BalsaMachining|T5Plus-34");
    EXPECT_EQ(model.preset(55)->preferenceKey(), "BalsaMachining|T5Plus-34");
    model.setFavorite(54, true);
    EXPECT_EQ(favoriteRows(model), (std::vector<int>{54, 55}));
    model.setFavorite(55, false);
    EXPECT_FALSE(model.isFavorite(54));
    EXPECT_TRUE(ComponentPresetFavorites::getFavorites(preferences, Type::BODY_TUBE).empty());
}

}  // namespace

}  // namespace QtRocket::Gui
