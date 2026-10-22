#include "QtRocket/gui/configdialog/PlacementPanel.h"

#include <array>
#include <memory>

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using PlacementPanelTest = Test::GuiTest;
using Test::EveryKindDesign;
using Test::PanelHost;

/// The texts of the items of @p combo.
[[nodiscard]] QStringList itemTexts(const QComboBox& combo)
{
    QStringList texts;
    for (int i = 0; i < combo.count(); ++i)
    {
        texts.append(combo.itemText(i));
    }
    return texts;
}

TEST_F(PlacementPanelTest, ShowsTheMethodAndTheOffset)
{
    const EveryKindDesign design;
    RocketComponent&      fins = design.component(ComponentKind::TRAPEZOID_FIN_SET);
    PanelHost             host(fins);
    auto*                 panel = &host.make<PlacementPanel>();
    EXPECT_EQ(panel->title(), QStringLiteral("Placement"));
    EXPECT_EQ(itemTexts(panel->methodComboBox()),
              (QStringList{QStringLiteral("Tip of the rocket"),
                           QStringLiteral("Top of the parent component"),
                           QStringLiteral("Middle of the parent component"),
                           QStringLiteral("Bottom of the parent component")}));
    EXPECT_EQ(panel->methodComboBox().objectName(), QStringLiteral("AxialMethod"));
    EXPECT_EQ(panel->methodComboBox().shownText(), enumText(fins.getAxialMethod()));
    const ConfigFields::NumberWidgets& offset = panel->offsetWidgets();
    ASSERT_NE(offset.label, nullptr);
    EXPECT_EQ(offset.label->text(), QStringLiteral("plus"));
    ASSERT_NE(offset.spinBox, nullptr);
    EXPECT_EQ(offset.spinBox->objectName(), QStringLiteral("AxialOffset"));
    EXPECT_EQ(offset.spinBox->text(),
              QtStrings::unitText(unitGroup(UnitGroupId::LENGTH).getDefaultUnit(),
                                  fins.getAxialOffset()));
    ASSERT_NE(offset.unitSelector, nullptr);
    ASSERT_NE(offset.slider, nullptr);
    // From minus to plus the parent's length.
    const double length = fins.getParent()->getLength();
    EXPECT_EQ(offset.slider->mapping().getMin(), -length);
    EXPECT_EQ(offset.slider->mapping().getMax(), length);
    EXPECT_EQ(panel->nextRow(), 2);
}

TEST_F(PlacementPanelTest, ChoosingAMethodKeepsThePosition)
{
    const EveryKindDesign design;
    RocketComponent&      fins     = design.component(ComponentKind::TRAPEZOID_FIN_SET);
    const double          absolute = fins.getAxialOffset(AxialMethod::ABSOLUTE);
    const double          bottom   = fins.getAxialOffset(AxialMethod::BOTTOM);
    PanelHost             host(fins);
    auto*                 panel = &host.make<PlacementPanel>();
    Q_EMIT panel->methodComboBox().activated(3);  // Bottom of the parent component
    EXPECT_EQ(fins.getAxialMethod(), AxialMethod::BOTTOM);
    EXPECT_DOUBLE_EQ(fins.getAxialOffset(AxialMethod::ABSOLUTE), absolute);
    // setAxialMethod() fires nothing: the offset field shows the new offset all the same.
    EXPECT_EQ(panel->offsetWidgets().spinBox->text(),
              QtStrings::unitText(unitGroup(UnitGroupId::LENGTH).getDefaultUnit(), bottom));
}

TEST_F(PlacementPanelTest, EverySelectedComponentKeepsItsOwnPosition)
{
    // The user's decision U3: each component its own setter; each converts its own offset. The
    // offset reaches every selected component too, a deviation: Java's setAxialOffset(double) has
    // no config-listener loop, so its "plus" field moves the main component only (review probe
    // OffsetProbe.java, OpenRocket 5f164fd0e, JDK 17: rail buttons at 0.02 (main) and 0.05, 0.01
    // set: 0.01 and 0.05).
    const EveryKindDesign design;
    RocketComponent&      lug    = design.component(ComponentKind::LAUNCH_LUG);
    RocketComponent&      button = design.component(ComponentKind::RAIL_BUTTON);
    button.setAxialOffset(0.05);
    const double lugAbsolute    = lug.getAxialOffset(AxialMethod::ABSOLUTE);
    const double buttonAbsolute = button.getAxialOffset(AxialMethod::ABSOLUTE);
    ASSERT_NE(lugAbsolute, buttonAbsolute);
    const std::array<RocketComponent*, 1> others{&button};
    PanelHost                             host(lug, others);
    auto*                                 panel = &host.make<PlacementPanel>();
    Q_EMIT panel->methodComboBox().activated(2);  // Middle of the parent component
    EXPECT_EQ(lug.getAxialMethod(), AxialMethod::MIDDLE);
    EXPECT_EQ(button.getAxialMethod(), AxialMethod::MIDDLE);
    EXPECT_DOUBLE_EQ(lug.getAxialOffset(AxialMethod::ABSOLUTE), lugAbsolute);
    EXPECT_DOUBLE_EQ(button.getAxialOffset(AxialMethod::ABSOLUTE), buttonAbsolute);

    ASSERT_TRUE(panel->offsetWidgets().spinBox->binding()->commitText("1"));
    EXPECT_DOUBLE_EQ(lug.getAxialOffset(), 0.01);
    EXPECT_DOUBLE_EQ(button.getAxialOffset(), 0.01);
}

TEST_F(PlacementPanelTest, TheSliderFollowsTheParentsLength)
{
    const EveryKindDesign design;
    RocketComponent&      fins = design.component(ComponentKind::TRAPEZOID_FIN_SET);
    PanelHost             host(fins);
    auto*                 panel = &host.make<PlacementPanel>();
    dynamic_cast<BodyTube&>(*fins.getParent()).setLength(0.7);
    EXPECT_EQ(panel->offsetWidgets().slider->mapping().getMin(), -0.7);
    EXPECT_EQ(panel->offsetWidgets().slider->mapping().getMax(), 0.7);
}

TEST_F(PlacementPanelTest, AComponentWithoutAParentIsABug)
{
    const EveryKindDesign design;
    PanelHost             host(design.document->getRocket());
    EXPECT_THROW(static_cast<void>(&host.make<PlacementPanel>()), BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
