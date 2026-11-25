// A Y axis row of a simulation's plot tab (OpenRocket's SimulationPlotTypeSelector): the second
// line with the row's colour and line style. What the panel does with them is
// SimulationPlotPanelTests.cpp's.
//
// Expectations marked "probe" are OpenRocket's own, printed by tools/openrocket-gui-pins'
// plot-dialog probe PlotTabProbe (see PlotTypeSelectorTests.cpp).

#include "QtRocket/gui/dialogs/plot/SimulationPlotTypeSelector.h"

#include <vector>

#include <QColor>
#include <QLabel>
#include <QList>
#include <QPoint>
#include <QPushButton>
#include <QSignalSpy>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/dialogs/plot/PlotTypeSelector.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/ColorChooserButton.h"
#include "QtRocket/gui/widgets/DataTypeComboBox.h"
#include "QtRocket/gui/widgets/LineStyleComboBox.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/util/LineStyle.h"
#include "ScriptedUserPrompts.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationPlotTypeSelectorTest = Gui::Test::GuiTest;
using Id                             = FlightDataTypeId;

[[nodiscard]] const FlightDataType& altitudeType()
{
    return FlightDataType::builtin(Id::TYPE_ALTITUDE);
}

/// The label of @p parent's children that shows @p text, or null.
[[nodiscard]] const QLabel* labelOf(const QWidget& parent, const QString& text)
{
    for (const QLabel* const label : parent.findChildren<QLabel*>())
    {
        if (label->text() == text)
        {
            return label;
        }
    }
    ADD_FAILURE() << "no label " << text.toStdString();
    return nullptr;
}

/// The left edge of @p widget in @p frame.
[[nodiscard]] int leftIn(const QWidget& frame, const QWidget* widget)
{
    return widget == nullptr ? -1 : widget->mapTo(&frame, QPoint(0, 0)).x();
}

TEST_F(SimulationPlotTypeSelectorTest, TheSecondLineShowsTheLook)
{
    // probe: "row 0 appearance" 'Color:', the ColorChooserButton, 'Line style:', the line style
    // box, then the SET_AS_DEFAULT and RESET icon buttons with their HTML tooltips, the two
    // disabled for a row at the factory default (SimulationPlotPanel's syncButtonStates()).
    Gui::Test::ScriptedUserPrompts   prompts;
    const SimulationPlotTypeSelector row(1, altitudeType(), nullptr, -1, {&altitudeType()},
                                         QColor(0xd9, 0x53, 0x19), LineStyle::DASHDOT, prompts,
                                         nullptr);
    EXPECT_EQ(row.selectedColor(), QColor(0xd9, 0x53, 0x19));
    EXPECT_EQ(row.selectedLineStyle(), LineStyle::DASHDOT);
    EXPECT_EQ(row.colorButton().objectName(), QStringLiteral("colorButton"));
    EXPECT_EQ(row.lineStyleSelector().objectName(), QStringLiteral("lineStyleSelector"));
    EXPECT_EQ(row.setDefaultButton().objectName(), QStringLiteral("setDefaultButton"));
    EXPECT_EQ(row.resetToFactoryButton().objectName(), QStringLiteral("resetToFactoryButton"));
    EXPECT_EQ(row.setDefaultButton().toolTip(),
              QStringLiteral("<html>Set as default<br>Save the current color and line style as "
                             "the application default for this data type</html>"));
    EXPECT_EQ(row.resetToFactoryButton().toolTip(),
              QStringLiteral("<html>Reset to factory default<br>Revert the color and line style "
                             "to their built-in defaults for this plot</html>"));
    EXPECT_FALSE(row.setDefaultButton().icon().isNull());
    EXPECT_FALSE(row.resetToFactoryButton().icon().isNull());
    EXPECT_TRUE(row.setDefaultButton().text().isEmpty());
    EXPECT_NE(labelOf(row, QStringLiteral("Color:")), nullptr);
    EXPECT_NE(labelOf(row, QStringLiteral("Line style:")), nullptr);
    // The first line's parts are there too, with the remove button.
    EXPECT_EQ(row.selectedType(), &altitudeType());
    EXPECT_TRUE(row.removeButton().isVisibleTo(&row));
}

TEST_F(SimulationPlotTypeSelectorTest, TheButtonsStatesAreThePanels)
{
    Gui::Test::ScriptedUserPrompts prompts;
    SimulationPlotTypeSelector     row(0, altitudeType(), nullptr, 0, {&altitudeType()}, QColor(),
                                       LineStyle::SOLID, prompts, nullptr);
    // None given yet: both enabled, as a new JButton.
    EXPECT_TRUE(row.setDefaultButton().isEnabled());
    EXPECT_TRUE(row.resetToFactoryButton().isEnabled());
    row.updateButtonStates(true, true);
    EXPECT_FALSE(row.setDefaultButton().isEnabled());
    EXPECT_FALSE(row.resetToFactoryButton().isEnabled());
    row.updateButtonStates(true, false);
    EXPECT_TRUE(row.setDefaultButton().isEnabled());
    EXPECT_FALSE(row.resetToFactoryButton().isEnabled());
    row.updateButtonStates(false, true);
    EXPECT_FALSE(row.setDefaultButton().isEnabled());
    EXPECT_TRUE(row.resetToFactoryButton().isEnabled());
}

TEST_F(SimulationPlotTypeSelectorTest, TheLookIsShownAsItIsSet)
{
    Gui::Test::ScriptedUserPrompts prompts;
    SimulationPlotTypeSelector     row(0, altitudeType(), nullptr, 0, {&altitudeType()}, QColor(),
                                       LineStyle::SOLID, prompts, nullptr);
    EXPECT_FALSE(row.selectedColor().isValid());
    const QSignalSpy colors(&row.colorButton(), &ColorChooserButton::selectedColorChanged);
    const QSignalSpy styles(&row.lineStyleSelector(), &LineStyleComboBox::lineStyleSelected);
    row.setSelectedColor(QColor(10, 20, 30));
    row.setSelectedColor(QColor(10, 20, 30));
    row.setSelectedLineStyle(LineStyle::DOTTED);
    row.setSelectedLineStyle(LineStyle::DOTTED);
    EXPECT_EQ(row.selectedColor(), QColor(10, 20, 30));
    EXPECT_EQ(row.selectedLineStyle(), LineStyle::DOTTED);
    // A colour tells of a change only; a line style of every setting (Java's ActionEvent).
    EXPECT_EQ(colors.count(), 1);
    EXPECT_EQ(styles.count(), 2);
}

TEST_F(SimulationPlotTypeSelectorTest, TheSecondLineIsIndentedBelowTheFirst)
{
    // probe: "row 0 appearance: x=18 (the type box's x=0) gaps: 6 24 6 24 6 vertical gap to the
    // first line=6": the line "gapleft para" (MigLayout's paragraph gap, 18 there; QtRocket's
    // kParagraphGap), 'Line style:' and the Set as default button a paragraph gap after the cell
    // before them (6 + 18), the other parts the related gap.
    Gui::Test::ScriptedUserPrompts prompts;
    QWidget                        window;
    auto*                          row = makeChild<SimulationPlotTypeSelector>(
        &window, 0U, altitudeType(), nullptr, 0, std::vector{&altitudeType()},
        QColor(0, 0x72, 0xbd), LineStyle::SOLID, prompts);
    auto* layout = makeChild<QVBoxLayout>(&window);
    layout->addWidget(row);
    layout->activate();
    ASSERT_TRUE(Gui::Test::showActive(window));
    const QLabel* color     = labelOf(*row, QStringLiteral("Color:"));
    const QLabel* lineStyle = labelOf(*row, QStringLiteral("Line style:"));
    ASSERT_NE(color, nullptr);
    ASSERT_NE(lineStyle, nullptr);
    EXPECT_EQ(leftIn(*row, color), PlotTypeSelector::kParagraphGap);
    EXPECT_EQ(leftIn(*row, &row->colorButton()) - (leftIn(*row, color) + color->width()),
              PlotTypeSelector::kRelatedGap);
    EXPECT_EQ(
        leftIn(*row, lineStyle) - (leftIn(*row, &row->colorButton()) + row->colorButton().width()),
        PlotTypeSelector::kRelatedGap + PlotTypeSelector::kParagraphGap);
    EXPECT_EQ(leftIn(*row, &row->setDefaultButton()) -
                  (leftIn(*row, &row->lineStyleSelector()) + row->lineStyleSelector().width()),
              PlotTypeSelector::kRelatedGap + PlotTypeSelector::kParagraphGap);
    EXPECT_EQ(leftIn(*row, &row->resetToFactoryButton()) -
                  (leftIn(*row, &row->setDefaultButton()) + row->setDefaultButton().width()),
              PlotTypeSelector::kRelatedGap);
    const int firstLineBottom =
        row->typeSelector().mapTo(row, QPoint(0, 0)).y() + row->typeSelector().height();
    const int firstLineHeight = row->typeSelector().parentWidget()->height();
    EXPECT_GE(firstLineHeight, firstLineBottom);
    EXPECT_EQ(color->mapTo(row, QPoint(0, 0)).y() - firstLineHeight, PlotTypeSelector::kRelatedGap);
}

}  // namespace

}  // namespace QtRocket::Gui
