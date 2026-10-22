#include "QtRocket/gui/configdialog/InstancesPanel.h"

#include <array>

#include <QLabel>
#include <QString>
#include <QWidget>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using InstancesPanelTest = Test::GuiTest;
using Test::EveryKindDesign;
using Test::PanelHost;

TEST_F(InstancesPanelTest, ShowsTheCountAndTheSeparation)
{
    const EveryKindDesign design;
    auto& lug = dynamic_cast<LaunchLug&>(design.component(ComponentKind::LAUNCH_LUG));
    lug.setInstanceCount(3);
    PanelHost host(lug);
    auto*     panel = &host.make<InstancesPanel>();
    EXPECT_EQ(panel->title(), QStringLiteral("Instances"));
    const IntSpinBox& count = panel->countSpinBox();
    EXPECT_EQ(count.objectName(), QStringLiteral("InstanceCount"));
    EXPECT_EQ(count.value(), 3);
    EXPECT_EQ(count.minimum(), 1);
    const ConfigFields::NumberWidgets& separation = panel->separationWidgets();
    ASSERT_NE(separation.label, nullptr);
    EXPECT_EQ(separation.label->text(), QStringLiteral("Instance Separation:"));
    ASSERT_NE(separation.spinBox, nullptr);
    EXPECT_EQ(separation.spinBox->objectName(), QStringLiteral("InstanceSeparation"));
    ASSERT_NE(separation.slider, nullptr);
    // From minus to plus the parent's length, fixed when the panel is made.
    const double length = lug.getParent()->getLength();
    EXPECT_EQ(separation.slider->mapping().getMin(), -length);
    EXPECT_EQ(separation.slider->mapping().getMax(), length);
    dynamic_cast<BodyTube&>(*lug.getParent()).setLength(length * 2);
    EXPECT_EQ(separation.slider->mapping().getMax(), length);
}

TEST_F(InstancesPanelTest, AParentWithoutLengthGivesTheSliderTenCentimetres)
{
    const EveryKindDesign design;
    auto& button = dynamic_cast<RailButton&>(design.component(ComponentKind::RAIL_BUTTON));
    dynamic_cast<BodyTube&>(*button.getParent()).setLength(0.0);
    PanelHost host(button);
    auto*     panel = &host.make<InstancesPanel>();
    EXPECT_EQ(panel->separationWidgets().slider->mapping().getMin(), -0.1);
    EXPECT_EQ(panel->separationWidgets().slider->mapping().getMax(), 0.1);
}

TEST_F(InstancesPanelTest, EditsReachEverySelectedComponent)
{
    const EveryKindDesign design;
    auto& lug    = dynamic_cast<LaunchLug&>(design.component(ComponentKind::LAUNCH_LUG));
    auto& button = dynamic_cast<RailButton&>(design.component(ComponentKind::RAIL_BUTTON));
    const std::array<RocketComponent*, 1> others{&button};
    PanelHost                             host(lug, others);
    auto*                                 panel = &host.make<InstancesPanel>();
    panel->countSpinBox().stepBy(3);
    EXPECT_EQ(lug.getInstanceCount(), 4);
    EXPECT_EQ(button.getInstanceCount(), 4);
    // At least 1.
    panel->countSpinBox().stepBy(-10);
    EXPECT_EQ(lug.getInstanceCount(), 1);
    EXPECT_EQ(button.getInstanceCount(), 1);
    ASSERT_TRUE(panel->separationWidgets().spinBox->binding()->commitText("2"));
    EXPECT_DOUBLE_EQ(lug.getInstanceSeparation(), 0.02);
    EXPECT_DOUBLE_EQ(button.getInstanceSeparation(), 0.02);
}

TEST_F(InstancesPanelTest, AComponentNotRepeatedAlongTheAxisIsABug)
{
    const EveryKindDesign design;
    PanelHost             host(design.component(ComponentKind::BODY_TUBE));
    EXPECT_THROW(static_cast<void>(&host.make<InstancesPanel>()), BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
