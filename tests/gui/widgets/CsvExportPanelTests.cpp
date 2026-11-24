// The choice of the variables a CSV file is written with (OpenRocket's CSVExportPanel): its
// table, the ticks and units, the buttons and the count (the export scout's Swing probe of the
// export tab, swing-probe.txt: "AFTER SELECT ALL: Exporting 58 variables out of 58." ...), where
// the options go, the rows' backgrounds and the extra column of a subclass.

#include "QtRocket/gui/widgets/CsvExportPanel.h"

#include <chrono>
#include <cstddef>
#include <print>
#include <ratio>
#include <string>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QColor>
#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QObject>
#include <QPalette>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QRegion>
#include <QRgb>
#include <QScrollArea>
#include <QString>
#include <QStyle>
#include <QStyleOption>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/settings/CsvOptionPreferences.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/widgets/CsvOptionPanel.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "ScopedApplicationPalette.h"
#include "figure/RecordingPaintDevice.h"

namespace QtRocket::Gui
{

namespace
{

using CsvExportPanelTest = Test::GuiTest;
using Test::RecordedPaint;
using Test::RecordingPaintDevice;
using Rows = std::vector<CsvExportPanel::Row>;

/// The CSV options of the simulation data export, without comment texts.
[[nodiscard]] CsvOptionPanel* exportOptionsPanel(QWidget& holder)
{
    return makeChild<CsvOptionPanel>(
        &holder, std::string(CsvOptionPreferences::kSimulationExportPanel),
        std::vector<CsvOptionPanel::CommentOption>(3), CsvOptionPanel::Arrangement::ON_NEW_ROWS);
}

/// The row of the built-in flight data type @p id, ticked when @p selected, named as the export
/// tab names it.
[[nodiscard]] CsvExportPanel::Row typeRow(FlightDataTypeId id, bool selected)
{
    const FlightDataType& type = FlightDataType::builtin(id);
    return {
        .name = flightDataTypeText(type), .unitGroup = &type.getUnitGroup(), .selected = selected};
}

/// Time (ticked), Altitude and Stability margin calibers: the first two rows of the probe's
/// export tab and a dimensionless one.
[[nodiscard]] Rows threeRows()
{
    return {typeRow(FlightDataTypeId::TYPE_TIME, true),
            typeRow(FlightDataTypeId::TYPE_ALTITUDE, false),
            typeRow(FlightDataTypeId::TYPE_STABILITY, false)};
}

/// A panel and what it is made of, on the stack of a test.
class ExportPanel
{
public:
    explicit ExportPanel(
        Rows                             rows      = threeRows(),
        CsvExportPanel::OptionsPlacement placement = CsvExportPanel::OptionsPlacement::RIGHT_COLUMN,
        const std::vector<QWidget*>&     extras    = {})
      : m_panel(std::move(rows), exportOptionsPanel(m_holder), placement, extras, nullptr)
    {
    }

    [[nodiscard]] CsvExportPanel& panel() noexcept { return m_panel; }
    /// A parent of the widgets before the panel takes them over.
    [[nodiscard]] QWidget& holder() noexcept { return m_holder; }

private:
    QWidget        m_holder;
    CsvExportPanel m_panel;
};

/// Lays @p widget out, and every widget in it, as showing it would (a hidden widget's layout
/// lays its children out only when it is shown).
void layOut(QWidget& widget)
{
    widget.resize(widget.sizeHint());
    widget.layout()->activate();
    for (const QWidget* const child : widget.findChildren<QWidget*>())
    {
        if (child->layout() != nullptr)
        {
            child->layout()->activate();
        }
    }
}

/// The geometry of @p widget in @p panel's coordinates.
[[nodiscard]] QRect geometryIn(const QWidget& panel, const QWidget& widget)
{
    return {widget.mapTo(&panel, QPoint()), widget.size()};
}

/// The widget in the cell @p row, @p column of @p panel's table grid.
[[nodiscard]] QWidget* tableCell(const CsvExportPanel& panel, int row, int column)
{
    const auto* grid = qobject_cast<const QGridLayout*>(panel.contentPanel()->layout());
    if (grid == nullptr)
    {
        ADD_FAILURE() << "the table has no grid";
        return nullptr;
    }
    const QLayoutItem* const item = grid->itemAtPosition(row, column);
    return item != nullptr ? item->widget() : nullptr;
}

/// The text of the label in the cell @p row, @p column of @p panel's table, or "<none>".
[[nodiscard]] QString cellText(const CsvExportPanel& panel, int row, int column)
{
    const auto* label = qobject_cast<const QLabel*>(tableCell(panel, row, column));
    return label != nullptr ? label->text() : QStringLiteral("<none>");
}

/// Whether the label in the cell @p row, @p column of @p panel's table is bold.
[[nodiscard]] bool cellIsBold(const CsvExportPanel& panel, int row, int column)
{
    const auto* label = qobject_cast<const QLabel*>(tableCell(panel, row, column));
    return label != nullptr && label->font().bold();
}

/// The colours of the rectangles @p panel's table fills itself (its background, its rows'
/// bands, then the line below the headers), without its children.
[[nodiscard]] std::vector<QColor> tableFills(CsvExportPanel& panel)
{
    layOut(panel);
    QWidget* const       table = panel.contentPanel();
    RecordingPaintDevice device(table->size());
    table->render(&device, QPoint(), QRegion(), {});
    std::vector<QColor> fills;
    for (const RecordedPaint& record : device.records())
    {
        if (record.kind == RecordedPaint::Kind::RECT && record.brush.style() == Qt::SolidPattern)
        {
            fills.push_back(record.brush.color());
        }
    }
    return fills;
}

/// The style's colour of a table's grid lines for @p widget (what QTableView draws its grid
/// with).
[[nodiscard]] QColor gridLineColor(const QWidget& widget)
{
    QStyleOption option;
    option.initFrom(&widget);
    return QColor::fromRgba(static_cast<QRgb>(
        widget.style()->styleHint(QStyle::SH_Table_GridLineColor, &option, &widget)));
}

/// A panel with M7's extra column: a label "<name> components" per row under "Components".
class ComponentsPanel final : public CsvExportPanel
{
public:
    ComponentsPanel(Rows rows, CsvOptionPanel* options, bool giveWidgets)
      : CsvExportPanel(DeferredBuild(), std::move(rows), options, OptionsPlacement::RIGHT_COLUMN,
                       {}, nullptr),
        m_giveWidgets(giveWidgets)
    {
        build();
    }

    using CsvExportPanel::build;

protected:
    [[nodiscard]] bool    hasExtraColumn() const override { return true; }
    [[nodiscard]] QString extraColumnLabel() const override { return QStringLiteral("Components"); }
    [[nodiscard]] QWidget* createExtraColumnWidget(std::size_t row, QWidget* parent) override
    {
        if (!m_giveWidgets)
        {
            return nullptr;
        }
        return makeChild<QLabel>(parent, name(row) + QStringLiteral(" components"));
    }

private:
    bool m_giveWidgets;
};

}  // namespace

TEST_F(CsvExportPanelTest, ShowsATableOfTheRows)
{
    // The probe: the bold headers Export, Variable and Unit, then per row a check box without
    // text, the name and the unit selector at the group's default unit.
    ExportPanel           made;
    const CsvExportPanel& panel = made.panel();
    ASSERT_EQ(panel.rowCount(), 3U);
    EXPECT_EQ(cellText(panel, 0, 0), QStringLiteral("Export"));
    EXPECT_EQ(cellText(panel, 0, 1), QStringLiteral("Variable"));
    EXPECT_EQ(cellText(panel, 0, 2), QStringLiteral("Unit"));
    EXPECT_TRUE(cellIsBold(panel, 0, 0));
    EXPECT_TRUE(cellIsBold(panel, 0, 1));
    EXPECT_TRUE(cellIsBold(panel, 0, 2));
    EXPECT_EQ(tableCell(panel, 0, 3), nullptr);

    EXPECT_EQ(tableCell(panel, 1, 0), panel.exportCheckBox(0));
    EXPECT_EQ(tableCell(panel, 1, 1), panel.nameLabel(0));
    EXPECT_EQ(tableCell(panel, 1, 2), panel.unitSelector(0));
    EXPECT_EQ(panel.exportCheckBox(0)->text(), QString());
    EXPECT_EQ(panel.exportCheckBox(0)->accessibleName(), QStringLiteral("Time"));
    EXPECT_EQ(panel.exportCheckBox(0)->objectName(), QStringLiteral("exportCheckBox"));
    EXPECT_EQ(panel.nameLabel(0)->objectName(), QStringLiteral("nameLabel"));
    EXPECT_EQ(panel.unitSelector(0)->objectName(), QStringLiteral("unitSelector"));
    EXPECT_EQ(panel.nameLabel(0)->text(), QStringLiteral("Time"));
    EXPECT_EQ(panel.nameLabel(1)->text(), QStringLiteral("Altitude"));
    EXPECT_EQ(panel.nameLabel(2)->text(), QStringLiteral("Stability margin calibers"));
    EXPECT_FALSE(panel.nameLabel(2)->font().bold());
    EXPECT_EQ(panel.name(1), QStringLiteral("Altitude"));
    EXPECT_EQ(panel.unitSelector(0)->text(), QStringLiteral("s"));
    EXPECT_EQ(panel.unitSelector(1)->text(), QStringLiteral("m"));
    EXPECT_EQ(panel.unitSelector(2)->text(), QStringLiteral(u"\u200B"));
    EXPECT_EQ(panel.selected(), (std::vector<bool>{true, false, false}));
    EXPECT_EQ(panel.extraColumnWidget(0), nullptr);

    // In a scroll area that never scrolls sideways.
    const QScrollArea* const scroll = panel.scrollPane();
    EXPECT_EQ(scroll->widget(), panel.contentPanel());
    EXPECT_EQ(scroll->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    EXPECT_EQ(scroll->verticalScrollBarPolicy(), Qt::ScrollBarAsNeeded);
    EXPECT_TRUE(scroll->widgetResizable());
    EXPECT_GE(scroll->minimumWidth(), panel.contentPanel()->sizeHint().width());
    EXPECT_EQ(scroll->objectName(), QStringLiteral("scrollPane"));
    EXPECT_EQ(panel.contentPanel()->objectName(), QStringLiteral("contentPanel"));

    EXPECT_THROW(static_cast<void>(panel.exportCheckBox(3)), BugError);
    EXPECT_THROW(static_cast<void>(panel.name(3)), BugError);
}

TEST_F(CsvExportPanelTest, CountsTheTicksAsJavaWritesThem)
{
    // updateSelectedCount(): the probe's three texts.
    EXPECT_EQ(CsvExportPanel::selectedCountText(58, 58),
              QStringLiteral("Exporting 58 variables out of 58."));
    EXPECT_EQ(CsvExportPanel::selectedCountText(0, 58),
              QStringLiteral("Exporting 0 variables out of 58."));
    EXPECT_EQ(CsvExportPanel::selectedCountText(1, 58),
              QStringLiteral("Exporting 1 variable out of 58."));
    EXPECT_EQ(CsvExportPanel::selectedCountText(2, 3),
              QStringLiteral("Exporting 2 variables out of 3."));

    ExportPanel           made;
    const CsvExportPanel& panel = made.panel();
    EXPECT_EQ(panel.selectedCountLabel()->objectName(), QStringLiteral("selectedCountLabel"));
    EXPECT_EQ(panel.selectedCountLabel()->text(), QStringLiteral("Exporting 1 variable out of 3."));
    EXPECT_EQ(panel.selectedCount(), 1U);
}

TEST_F(CsvExportPanelTest, SelectsAllOrNone)
{
    ExportPanel     made;
    CsvExportPanel& panel = made.panel();
    EXPECT_EQ(panel.selectAllButton()->text(), QStringLiteral("Select all"));
    EXPECT_EQ(panel.selectNoneButton()->text(), QStringLiteral("Select none"));
    EXPECT_EQ(panel.selectAllButton()->objectName(), QStringLiteral("selectAllButton"));
    EXPECT_EQ(panel.selectNoneButton()->objectName(), QStringLiteral("selectNoneButton"));
    // Return in a dialog stays the dialog's.
    EXPECT_FALSE(panel.selectAllButton()->autoDefault());
    EXPECT_FALSE(panel.selectNoneButton()->autoDefault());

    panel.selectAllButton()->click();
    EXPECT_EQ(panel.selected(), (std::vector<bool>{true, true, true}));
    EXPECT_EQ(panel.selectedCountLabel()->text(),
              QStringLiteral("Exporting 3 variables out of 3."));
    panel.selectNoneButton()->click();
    EXPECT_EQ(panel.selected(), (std::vector<bool>{false, false, false}));
    EXPECT_EQ(panel.selectedCountLabel()->text(),
              QStringLiteral("Exporting 0 variables out of 3."));
    panel.selectAll();
    EXPECT_EQ(panel.selectedCount(), 3U);
    panel.selectNone();
    EXPECT_EQ(panel.selectedCount(), 0U);
}

TEST_F(CsvExportPanelTest, ARowsCheckBoxTicksIt)
{
    // Java: the check box's action sets selected[i] and the count.
    ExportPanel     made;
    CsvExportPanel& panel = made.panel();
    panel.exportCheckBox(1)->click();
    EXPECT_TRUE(panel.isSelected(1));
    EXPECT_EQ(panel.selectedCountLabel()->text(),
              QStringLiteral("Exporting 2 variables out of 3."));
    panel.setSelected(0, false);
    EXPECT_FALSE(panel.exportCheckBox(0)->isChecked());
    EXPECT_EQ(panel.selected(), (std::vector<bool>{false, true, false}));
    EXPECT_EQ(panel.selectedCountLabel()->text(), QStringLiteral("Exporting 1 variable out of 3."));
}

TEST_F(CsvExportPanelTest, EachRowHasItsOwnUnit)
{
    // The default unit of each row's group to begin with, then the selector's choice, which the
    // group's default unit does not follow (Java: units[i], never stored).
    ExportPanel      made;
    CsvExportPanel&  panel = made.panel();
    const UnitGroup& length =
        FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE).getUnitGroup();
    const Unit* feet = length.getUnit("ft");
    ASSERT_NE(feet, nullptr);
    EXPECT_EQ(&panel.unit(1), &length.getDefaultUnit());
    panel.setSelectedUnit(1, *feet);
    EXPECT_EQ(&panel.unit(1), feet);
    EXPECT_EQ(panel.unitSelector(1)->text(), QStringLiteral("ft"));
    EXPECT_NE(&length.getDefaultUnit(), feet);
    const std::vector<const Unit*> units = panel.units();
    ASSERT_EQ(units.size(), 3U);
    EXPECT_EQ(
        units[0],
        &FlightDataType::builtin(FlightDataTypeId::TYPE_TIME).getUnitGroup().getDefaultUnit());
    EXPECT_EQ(units[1], feet);
    // A unit of another group is a programming error.
    EXPECT_THROW(panel.setSelectedUnit(0, *feet), BugError);
}

TEST_F(CsvExportPanelTest, TheOptionsAndExtrasGoInAColumnOnTheRight)
{
    // Java's separateRowForOptions false: the options, then the extra widgets ("Stage to export")
    // below each other, right of the table.
    QWidget         holder;
    auto*           stage = makeChild<QLabel>(&holder, QStringLiteral("Stage to export"));
    ExportPanel     made(threeRows(), CsvExportPanel::OptionsPlacement::RIGHT_COLUMN, {stage});
    CsvExportPanel& panel = made.panel();
    layOut(panel);
    EXPECT_EQ(panel.csvOptions().objectName(), QStringLiteral("csvOptions"));
    EXPECT_TRUE(panel.isAncestorOf(&panel.csvOptions()));
    EXPECT_TRUE(panel.isAncestorOf(stage));
    const QRect options = geometryIn(panel, panel.csvOptions());
    const QRect extra   = geometryIn(panel, *stage);
    const QRect table   = geometryIn(panel, *panel.scrollPane());
    EXPECT_GT(options.left(), table.right());
    EXPECT_EQ(extra.left(), options.left());
    EXPECT_GT(extra.top(), options.bottom());
    EXPECT_EQ(options.top(), table.top());
}

TEST_F(CsvExportPanelTest, TheOptionsAndExtrasGoBelowTheTableOnASeparateRow)
{
    // Java's separateRowForOptions true: the table across the panel, then the options and the
    // extra widgets two to a row.
    QWidget     holder;
    auto*       first  = makeChild<QLabel>(&holder, QStringLiteral("first"));
    auto*       second = makeChild<QLabel>(&holder, QStringLiteral("second"));
    ExportPanel made(threeRows(), CsvExportPanel::OptionsPlacement::SEPARATE_ROW, {first, second});
    CsvExportPanel& panel = made.panel();
    layOut(panel);
    const QRect options = geometryIn(panel, panel.csvOptions());
    const QRect table   = geometryIn(panel, *panel.scrollPane()->parentWidget());
    EXPECT_GT(options.top(), table.bottom());
    EXPECT_GT(geometryIn(panel, *first).left(), options.right());
    EXPECT_EQ(geometryIn(panel, *first).top(), options.top());
    EXPECT_GT(geometryIn(panel, *second).top(), options.bottom());
    EXPECT_EQ(geometryIn(panel, *second).left(), options.left());
}

TEST_F(CsvExportPanelTest, RefusesWhatItCannotShow)
{
    QWidget holder;
    EXPECT_THROW(
        static_cast<void>(CsvExportPanel(
            threeRows(), nullptr, CsvExportPanel::OptionsPlacement::RIGHT_COLUMN, {}, nullptr)),
        BugError);
    Rows rows         = threeRows();
    rows[1].unitGroup = nullptr;
    EXPECT_THROW(static_cast<void>(CsvExportPanel(rows, exportOptionsPanel(holder),
                                                  CsvExportPanel::OptionsPlacement::RIGHT_COLUMN,
                                                  {}, nullptr)),
                 BugError);
    EXPECT_THROW(static_cast<void>(CsvExportPanel(threeRows(), exportOptionsPanel(holder),
                                                  CsvExportPanel::OptionsPlacement::RIGHT_COLUMN,
                                                  {nullptr}, nullptr)),
                 BugError);
    // No rows: nothing to tick.
    ExportPanel empty{Rows{}};
    EXPECT_EQ(empty.panel().rowCount(), 0U);
    EXPECT_EQ(empty.panel().selectedCountLabel()->text(),
              QStringLiteral("Exporting 0 variables out of 0."));
}

TEST_F(CsvExportPanelTest, TheRowsAlternateBetweenTheTablesAndTheLighterRowsColour)
{
    // Java: Table.background for an even row, OR.colors.row.lighter for an odd one.
    const QPalette light = Gui::Test::ScopedApplicationPalette::light();
    const QPalette dark  = Gui::Test::ScopedApplicationPalette::dark();
    EXPECT_EQ(CsvExportPanel::rowBackground(0, light), light.color(QPalette::Base));
    EXPECT_EQ(CsvExportPanel::rowBackground(1, light),
              ThemeColors::themeColor(ThemeColors::Key::ROW_LIGHTER, ThemeColors::Theme::LIGHT));
    EXPECT_EQ(CsvExportPanel::rowBackground(2, dark), dark.color(QPalette::Base));
    EXPECT_EQ(CsvExportPanel::rowBackground(3, dark),
              ThemeColors::themeColor(ThemeColors::Key::ROW_LIGHTER, ThemeColors::Theme::DARK));

    // Painted as the palette is then: the table's background (a scroll area's widget fills it,
    // as Java's panel), the three rows' bands, then the headers' line.
    const Gui::Test::ScopedApplicationPalette palette(dark);
    ExportPanel                               made;
    const std::vector<QColor>                 fills = tableFills(made.panel());
    ASSERT_EQ(fills.size(), 5U);
    const QWidget&  table   = *made.panel().contentPanel();
    const QPalette& painted = table.palette();
    EXPECT_EQ(fills[0], painted.color(QPalette::Window));
    EXPECT_EQ(fills[1], CsvExportPanel::rowBackground(0, painted));
    EXPECT_EQ(fills[2], CsvExportPanel::rowBackground(1, painted));
    EXPECT_EQ(fills[3], CsvExportPanel::rowBackground(2, painted));
    EXPECT_NE(fills[1], fills[2]);
    EXPECT_EQ(fills[4], gridLineColor(table));
}

TEST_F(CsvExportPanelTest, ASubclassAddsAnExtraColumn)
{
    // M7's CAExportPanel: hasExtraComponent(), getExtraColumnLabelKey() and
    // createExtraComponent() for each row, called once the subclass is made (build()).
    QWidget         holder;
    ComponentsPanel panel(threeRows(), exportOptionsPanel(holder), true);
    EXPECT_EQ(cellText(panel, 0, 3), QStringLiteral("Components"));
    EXPECT_TRUE(cellIsBold(panel, 0, 3));
    ASSERT_NE(panel.extraColumnWidget(2), nullptr);
    EXPECT_EQ(tableCell(panel, 3, 3), panel.extraColumnWidget(2));
    EXPECT_EQ(cellText(panel, 3, 3), QStringLiteral("Stability margin calibers components"));
    EXPECT_EQ(cellText(panel, 1, 3), QStringLiteral("Time components"));
    // Built once.
    EXPECT_THROW(panel.build(), BugError);
    // Without a widget for a row.
    EXPECT_THROW(static_cast<void>(ComponentsPanel(threeRows(), exportOptionsPanel(holder), false)),
                 BugError);
}

TEST_F(CsvExportPanelTest, DISABLED_MeasureTheConstructionOfTheExportTabsRows)
{
    // How long the export tab's panel takes to make: OpenRocket's 71 types (the export tab shows
    // branch 0's, 58 for "A simple model rocket"). A measurement, run by hand (the plan: "about
    // 60 rows under ASan").
    Rows rows;
    for (const FlightDataType* const type : FlightDataType::allTypesSorted())
    {
        rows.push_back({.name      = flightDataTypeText(*type),
                        .unitGroup = &type->getUnitGroup(),
                        .selected  = false});
    }
    const auto  start = std::chrono::steady_clock::now();
    ExportPanel made(rows);
    const auto  built = std::chrono::steady_clock::now();
    layOut(made.panel());
    const auto laidOut = std::chrono::steady_clock::now();
    std::println("{} rows: made in {} ms, laid out in {} ms", rows.size(),
                 std::chrono::duration<double, std::milli>(built - start).count(),
                 std::chrono::duration<double, std::milli>(laidOut - built).count());
    EXPECT_EQ(made.panel().rowCount(), rows.size());
}

}  // namespace QtRocket::Gui
