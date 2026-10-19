#include "QtRocket/gui/configdialog/ConfigFields.h"

#include <memory>

#include <QGridLayout>
#include <QLabel>
#include <QString>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using ConfigFieldsTest = Test::GuiTest;

TEST_F(ConfigFieldsTest, ANumberRowFillsTheFourColumns)
{
    QWidget    panel;
    auto*      grid    = makeChild<QGridLayout>(&panel);
    const auto binding = std::make_shared<DoubleBinding>(0.5, unitGroup(UnitGroupId::LENGTH));
    const ConfigFields::NumberWidgets widgets = ConfigFields::addNumberRow(
        *grid, 2, QStringLiteral("Length:"), binding, QStringLiteral("Length"),
        {.slider = SliderMapping::linear(0.0, 1.0)});
    ASSERT_NE(widgets.label, nullptr);
    ASSERT_NE(widgets.spinBox, nullptr);
    ASSERT_NE(widgets.unitSelector, nullptr);
    ASSERT_NE(widgets.slider, nullptr);
    EXPECT_EQ(widgets.label->text(), QStringLiteral("Length:"));
    EXPECT_EQ(grid->itemAtPosition(2, 0)->widget(), widgets.label);
    EXPECT_EQ(grid->itemAtPosition(2, 1)->widget(), widgets.spinBox);
    EXPECT_EQ(grid->itemAtPosition(2, 2)->widget(), widgets.unitSelector);
    EXPECT_EQ(grid->itemAtPosition(2, 3)->widget(), widgets.slider);
    EXPECT_EQ(widgets.spinBox->objectName(), QStringLiteral("Length"));
    EXPECT_EQ(widgets.unitSelector->objectName(), QStringLiteral("Length"));
    EXPECT_EQ(widgets.slider->objectName(), QStringLiteral("Length"));
    EXPECT_EQ(widgets.spinBox->binding(), binding);
    EXPECT_EQ(widgets.slider->mapping().getMax(), 1.0);
}

TEST_F(ConfigFieldsTest, ARowMayLackItsLabelUnitAndSlider)
{
    QWidget                           panel;
    auto*                             grid    = makeChild<QGridLayout>(&panel);
    const auto                        binding = std::make_shared<DoubleBinding>(0.5);
    const ConfigFields::NumberWidgets widgets = ConfigFields::addNumberRow(
        *grid, 0, QString(), binding, QStringLiteral("OverrideCD"), {.unitSelector = false});
    EXPECT_EQ(widgets.label, nullptr);
    EXPECT_EQ(widgets.unitSelector, nullptr);
    EXPECT_EQ(widgets.slider, nullptr);
    ASSERT_NE(widgets.spinBox, nullptr);
    EXPECT_EQ(grid->itemAtPosition(0, 0), nullptr);
}

TEST_F(ConfigFieldsTest, NoBindingOrNoWidgetIsABug)
{
    QWidget panel;
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks): its Qt parent, panel, owns grid
    auto* grid = makeChild<QGridLayout>(&panel);
    EXPECT_THROW(static_cast<void>(ConfigFields::addNumberRow(*grid, 0, QString(), nullptr,
                                                              QStringLiteral("x"), {})),
                 BugError);
    QGridLayout loose;
    EXPECT_THROW(
        static_cast<void>(ConfigFields::addNumberRow(
            loose, 0, QString(), std::make_shared<DoubleBinding>(0.0), QStringLiteral("x"), {})),
        BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
