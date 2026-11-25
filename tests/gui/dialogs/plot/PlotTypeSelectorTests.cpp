// A Y axis row of the plot tab (OpenRocket's PlotTypeSelector) and the X axis line of the panel
// above the rows (PlotPanel), the half of PlotTypeSelectorTest that is about them; its
// DataTypeComboBox half is tests/gui/widgets/DataTypeComboBoxTests.cpp's.
//
// Expectations marked "probe" are OpenRocket's own, printed by tools/openrocket-gui-pins'
// plot-dialog probe PlotTabProbe (OpenRocket 5f164fd0e, JDK 17, FlatLaf Light under xvfb; its
// transcript is the end of plot-dialog/hand-pinned.txt, which generate.sh checks) and by the
// plot-widgets probe (plot-widgets/hand-pinned.txt): the MigLayout constraints, and the gaps
// between the parts after pack(), which do not depend on the fonts (MigLayout's related gap is 6
// px there, its unrelated gap 12 and its paragraph gap 18, which QtRocket's panels take as 12).

#include "QtRocket/gui/dialogs/plot/PlotTypeSelector.h"

#include <vector>

#include <QComboBox>
#include <QLabel>
#include <QList>
#include <QPoint>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/dialogs/plot/PlotPanel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/DataTypeComboBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "TestGuiServices.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using PlotTypeSelectorTest = Gui::Test::GuiTest;
using Id                   = FlightDataTypeId;

[[nodiscard]] const FlightDataType* type(Id id)
{
    return &FlightDataType::builtin(id);
}

/// The label of @p parent's children that shows @p text (a test failure, and null, when there is
/// none or more than one).
[[nodiscard]] QLabel* labelOf(const QWidget& parent, const QString& text)
{
    QLabel* found = nullptr;
    int     count = 0;
    for (QLabel* const label : parent.findChildren<QLabel*>())
    {
        if (label->text() == text)
        {
            found = label;
            ++count;
        }
    }
    EXPECT_EQ(count, 1) << text.toStdString();
    return count == 1 ? found : nullptr;
}

/// The horizontal gap from @p left's right edge to @p right's left edge, in @p frame.
[[nodiscard]] int gapBetween(const QWidget& frame, const QWidget* left, const QWidget* right)
{
    if (left == nullptr || right == nullptr)
    {
        ADD_FAILURE() << "no widget";
        return -1;
    }
    return right->mapTo(&frame, QPoint(0, 0)).x() -
           (left->mapTo(&frame, QPoint(0, 0)).x() + left->width());
}

/// A window that shows @p widget, its child, laid out: the geometry of a shown widget.
class Shown
{
public:
    explicit Shown(QWidget& window) : m_window(&window) { }
    [[nodiscard]] bool show(QWidget* widget) const
    {
        auto* layout = makeChild<QVBoxLayout>(m_window);
        layout->addWidget(widget);
        layout->activate();
        return Gui::Test::showActive(*m_window);
    }

private:
    QWidget* m_window;
};

TEST_F(PlotTypeSelectorTest, DataTypeSelectorsUseFixedWidthAndFullNameTooltips)
{
    // Port of PlotTypeSelectorTest.testDataTypeSelectorsUseFixedWidthAndFullNameTooltips, its row
    // and panel half: long data type names must not widen the X or Y axis selectors, a compact gap
    // follows them, and their full text remains available as a tooltip after the selection
    // changes. Java asserts MigLayout's constraints ("width 200lp!", "gapright unrel"), QtRocket
    // the geometry they give. probe: constraints typeSelector='width 200lp!, gapright unrel, top',
    // unitSelector='width 40lp, gapright unrel, top', domainTypeSelector='width 200lp!, gapright
    // unrel'; "row 0 geometry: type width=200 gap type-'Unit:'=18 ... gap unit-'Axis:'=18" (the
    // row's parts are cells of their own: the related gap of 6 between them, the unrelated 12 after
    // the selector), "x axis geometry: type width=200 gap type-'Unit:'=12" (one split cell).
    const FlightDataType* longType  = type(Id::TYPE_DAMPING_MOMENT_COEFF_AERODYNAMIC);
    const FlightDataType* shortType = type(Id::TYPE_ALTITUDE);

    QWidget yWindow;
    auto*   yAxisSelector = makeChild<PlotTypeSelector>(&yWindow, 0U, *longType,
                                                        &longType->getUnitGroup().getDefaultUnit(),
                                                        -1, std::vector{longType, shortType});
    ASSERT_TRUE(Shown(yWindow).show(yAxisSelector));
    EXPECT_EQ(yAxisSelector->typeSelector().toolTip(),
              QStringLiteral("Damping moment coefficient (aerodynamic)"));
    EXPECT_EQ(yAxisSelector->typeSelector().width(), PlotTypeSelector::kDataTypeSelectorWidth);
    EXPECT_EQ(gapBetween(*yAxisSelector, &yAxisSelector->typeSelector(),
                         labelOf(*yAxisSelector, QStringLiteral("Unit:"))),
              PlotTypeSelector::kRelatedGap + PlotTypeSelector::kUnrelatedGap);
    EXPECT_EQ(gapBetween(*yAxisSelector, &yAxisSelector->unitSelector(),
                         labelOf(*yAxisSelector, QStringLiteral("Axis:"))),
              PlotTypeSelector::kRelatedGap + PlotTypeSelector::kUnrelatedGap);

    yAxisSelector->typeSelector().setSelectedType(shortType);
    EXPECT_EQ(yAxisSelector->typeSelector().toolTip(), QStringLiteral("Altitude"));
    EXPECT_EQ(yAxisSelector->typeSelector().width(), PlotTypeSelector::kDataTypeSelectorWidth);

    const Gui::Test::TestGuiServices  services;
    const SimulationPlotConfiguration customConfiguration("Custom", *longType);
    const SimulationPlotConfiguration defaultConfiguration("Default", *longType);
    QWidget                           xWindow;
    auto*                             plotPanel = makeChild<PlotPanel>(
        &xWindow, services.services(), PlotPanel::DataTypes{longType, shortType},
        PlotPanel::DataTypes{longType, shortType},
        PlotPanel::Configurations{defaultConfiguration, customConfiguration}, defaultConfiguration);
    ASSERT_TRUE(Shown(xWindow).show(plotPanel));
    EXPECT_EQ(plotPanel->domainTypeSelector().toolTip(),
              QStringLiteral("Damping moment coefficient (aerodynamic)"));
    EXPECT_EQ(plotPanel->domainTypeSelector().width(), PlotTypeSelector::kDataTypeSelectorWidth);
    const QWidget* xLine = plotPanel->domainTypeSelector().parentWidget();
    ASSERT_NE(xLine, nullptr);
    EXPECT_EQ(gapBetween(*plotPanel, &plotPanel->domainTypeSelector(),
                         labelOf(*xLine, QStringLiteral("Unit:"))),
              PlotTypeSelector::kUnrelatedGap);

    plotPanel->domainTypeSelector().setSelectedType(shortType);
    EXPECT_EQ(plotPanel->domainTypeSelector().toolTip(), QStringLiteral("Altitude"));
}

TEST_F(PlotTypeSelectorTest, ARowShowsItsTypeUnitAndAxis)
{
    // probe: row 0's components 'Unit:', 'Axis:' (Java's literals) and the remove button with
    // the tooltip 'Remove this plot'; the axis box's items are Util.PlotAxisSelection's names, its
    // index the axis + 1.
    const FlightDataType*  altitude = type(Id::TYPE_ALTITUDE);
    const PlotTypeSelector row(3, *altitude, nullptr, 1, {altitude, type(Id::TYPE_TIME)}, nullptr);
    EXPECT_EQ(row.index(), 3U);
    EXPECT_EQ(row.selectedType(), altitude);
    EXPECT_EQ(&row.selectedUnit(), &altitude->getUnitGroup().getDefaultUnit());
    EXPECT_EQ(row.selectedAxis(), 1);
    EXPECT_EQ(
        PlotTypeSelector::axisNames(),
        (std::vector{QStringLiteral("Auto"), QStringLiteral("Left"), QStringLiteral("Right")}));
    EXPECT_EQ(row.axisSelector().count(), 3);
    EXPECT_EQ(row.axisSelector().itemText(0), QStringLiteral("Auto"));
    EXPECT_EQ(row.axisSelector().itemText(1), QStringLiteral("Left"));
    EXPECT_EQ(row.axisSelector().itemText(2), QStringLiteral("Right"));
    EXPECT_EQ(row.axisSelector().currentIndex(), 2);
    EXPECT_EQ(row.typeSelector().objectName(), QStringLiteral("typeSelector"));
    EXPECT_EQ(row.unitSelector().objectName(), QStringLiteral("unitSelector"));
    EXPECT_EQ(row.axisSelector().objectName(), QStringLiteral("axisSelector"));
    EXPECT_EQ(row.removeButton().objectName(), QStringLiteral("removeButton"));
    EXPECT_EQ(row.removeButton().toolTip(), QStringLiteral("Remove this plot"));
    EXPECT_FALSE(row.removeButton().icon().isNull());
    EXPECT_TRUE(row.removeButton().text().isEmpty());
    EXPECT_TRUE(row.removeButton().isFlat());
    EXPECT_FALSE(row.removeButton().autoDefault());
    EXPECT_TRUE(row.removeButton().isVisibleTo(&row));
    EXPECT_NE(labelOf(row, QStringLiteral("Unit:")), nullptr);
    EXPECT_NE(labelOf(row, QStringLiteral("Axis:")), nullptr);
}

TEST_F(PlotTypeSelectorTest, TheAxisAndTheUnitAreTheRowsOwn)
{
    const FlightDataType* altitude = type(Id::TYPE_ALTITUDE);
    const Unit*           feet     = altitude->getUnitGroup().getUnit("ft");
    ASSERT_NE(feet, nullptr);
    PlotTypeSelector automatic(0, *altitude, feet, -1, {altitude}, nullptr);
    EXPECT_EQ(&automatic.selectedUnit(), feet);
    EXPECT_EQ(automatic.unitSelector().text(), QStringLiteral("ft"));
    EXPECT_EQ(automatic.axisSelector().currentIndex(), 0);
    EXPECT_EQ(automatic.selectedAxis(), -1);
    automatic.axisSelector().setCurrentIndex(1);
    EXPECT_EQ(automatic.selectedAxis(), 0);

    // Another group: its default unit (UnitSelector::setUnitGroup()).
    const FlightDataType* time = type(Id::TYPE_TIME);
    automatic.setUnitGroup(time->getUnitGroup());
    EXPECT_EQ(&automatic.unitSelector().getUnitGroup(), &time->getUnitGroup());
    EXPECT_EQ(&automatic.selectedUnit(), &time->getUnitGroup().getDefaultUnit());

    const PlotTypeSelector left(0, *altitude, nullptr, 0, {altitude}, nullptr);
    EXPECT_EQ(left.axisSelector().currentIndex(), 1);
    // A unit of another group is a programming error (Java: IllegalArgumentException).
    EXPECT_THROW(PlotTypeSelector(0, *altitude, &time->getUnitGroup().getDefaultUnit(), 0,
                                  {altitude}, nullptr),
                 BugError);
}

TEST_F(PlotTypeSelectorTest, TheLineIsLaidOutAsJavasCells)
{
    // probe: "row 0 geometry: type width=200 gap type-'Unit:'=18 gap 'Unit:'-unit=6 unit
    // width=40 gap unit-'Axis:'=18 gap 'Axis:'-axis=6 gap axis-remove=6 type y=0"; every part at
    // the top ("top").
    const FlightDataType* altitude = type(Id::TYPE_ALTITUDE);
    QWidget               window;
    auto*                 row =
        makeChild<PlotTypeSelector>(&window, 0U, *altitude, nullptr, -1, std::vector{altitude});
    ASSERT_TRUE(Shown(window).show(row));
    const QLabel* unitLabel = labelOf(*row, QStringLiteral("Unit:"));
    const QLabel* axisLabel = labelOf(*row, QStringLiteral("Axis:"));
    EXPECT_EQ(gapBetween(*row, unitLabel, &row->unitSelector()), PlotTypeSelector::kRelatedGap);
    EXPECT_GE(row->unitSelector().width(), PlotTypeSelector::kUnitSelectorWidth);
    EXPECT_EQ(gapBetween(*row, axisLabel, &row->axisSelector()), PlotTypeSelector::kRelatedGap);
    EXPECT_EQ(gapBetween(*row, &row->axisSelector(), &row->removeButton()),
              PlotTypeSelector::kRelatedGap);
    EXPECT_EQ(row->typeSelector().mapTo(row, QPoint(0, 0)), QPoint(0, 0));
    EXPECT_EQ(row->unitSelector().mapTo(row, QPoint(0, 0)).y(), 0);
    EXPECT_EQ(row->axisSelector().mapTo(row, QPoint(0, 0)).y(), 0);
}

}  // namespace

}  // namespace QtRocket::Gui
