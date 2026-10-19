#include "QtRocket/gui/configdialog/ShoulderPanel.h"

#include <array>
#include <memory>
#include <string>

#include <QGroupBox>
#include <QObject>
#include <QString>
#include <QWidget>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using ShoulderPanelTest = Test::GuiTest;
using Test::childNamed;
using Test::EveryKindDesign;
using Test::PanelHost;

/// The names of the fields of a shoulder (@p prefix: "ForeShoulder", "AftShoulder", "Shoulder")
/// that @p panel lacks.
[[nodiscard]] std::string missingShoulderFields(const QWidget& panel, const QString& prefix)
{
    std::string missing;
    for (const char* property : {"Radius", "Length", "Thickness"})
    {
        const QString name = prefix + QString::fromLatin1(property);
        if (childNamed<UnitSpinBox>(panel, name) == nullptr ||
            childNamed<UnitSelector>(panel, name) == nullptr ||
            childNamed<ValueSlider>(panel, name) == nullptr)
        {
            missing += QtStrings::toUtf8(name) + '\n';
        }
    }
    if (childNamed<BoolCheckBox>(panel, prefix + QStringLiteral("Capped")) == nullptr)
    {
        missing += QtStrings::toUtf8(prefix) + "Capped\n";
    }
    return missing;
}

TEST_F(ShoulderPanelTest, ATransitionHasBothShoulders)
{
    const EveryKindDesign design;
    auto&     transition = dynamic_cast<Transition&>(design.component(ComponentKind::TRANSITION));
    PanelHost host(transition);
    auto*     panel = &host.make<ShoulderPanel>();
    ASSERT_NE(panel->foreShoulder(), nullptr);
    EXPECT_EQ(panel->foreShoulder()->title(), QStringLiteral("Fore shoulder"));
    auto* aft = qobject_cast<QGroupBox*>(&panel->aftShoulder());
    ASSERT_NE(aft, nullptr);
    EXPECT_EQ(aft->title(), QStringLiteral("Aft shoulder"));
    EXPECT_EQ(missingShoulderFields(*panel, QStringLiteral("ForeShoulder")), "");
    EXPECT_EQ(missingShoulderFields(*panel, QStringLiteral("AftShoulder")), "");
    EXPECT_EQ(childNamed<BoolCheckBox>(*panel, QStringLiteral("ForeShoulderCapped"))->text(),
              QStringLiteral("End capped"));

    // The diameter field shows the radius times 2; its slider ends at the end's diameter.
    auto* diameter = childNamed<UnitSpinBox>(*panel, QStringLiteral("ForeShoulderRadius"));
    EXPECT_EQ(diameter->binding()->getMultiplier(), 2.0);
    auto* slider = childNamed<ValueSlider>(*panel, QStringLiteral("ForeShoulderRadius"));
    EXPECT_EQ(slider->mapping().getMax(), 2 * transition.getForeRadius());
    // The length's slider: 0, 2 cm, 20 cm.
    auto* length = childNamed<ValueSlider>(*panel, QStringLiteral("AftShoulderLength"));
    EXPECT_FALSE(length->mapping().isLinear());
    EXPECT_EQ(length->mapping().getMid(), 0.02);
    EXPECT_EQ(length->mapping().getMax(), 0.2);
}

TEST_F(ShoulderPanelTest, TheSlidersFollowTheRadiiTheyEndAt)
{
    const EveryKindDesign design;
    auto& transition = dynamic_cast<Transition&>(design.component(ComponentKind::TRANSITION));
    transition.setForeRadiusAutomatic(false);
    transition.setForeRadius(0.03);
    PanelHost host(transition);
    auto*     panel  = &host.make<ShoulderPanel>();
    auto*     slider = childNamed<ValueSlider>(*panel, QStringLiteral("ForeShoulderRadius"));
    EXPECT_EQ(slider->mapping().getMax(), 0.06);
    transition.setForeRadius(0.04);
    EXPECT_EQ(slider->mapping().getMax(), 0.08);
    transition.setForeShoulderRadius(0.02);
    auto* thickness = childNamed<ValueSlider>(*panel, QStringLiteral("ForeShoulderThickness"));
    EXPECT_EQ(thickness->mapping().getMax(), 0.02);
}

TEST_F(ShoulderPanelTest, ANoseConeHasItsOwnShoulderWithoutABorder)
{
    const EveryKindDesign design;
    auto&     nose = dynamic_cast<NoseCone&>(design.component(ComponentKind::NOSE_CONE));
    PanelHost host(nose);
    auto*     panel = &host.make<ShoulderPanel>();
    EXPECT_EQ(panel->foreShoulder(), nullptr);
    EXPECT_EQ(qobject_cast<QGroupBox*>(&panel->aftShoulder()), nullptr);
    EXPECT_EQ(missingShoulderFields(*panel, QStringLiteral("Shoulder")), "");
    EXPECT_EQ(childNamed<UnitSpinBox>(*panel, QStringLiteral("AftShoulderRadius")), nullptr);
    auto* slider = childNamed<ValueSlider>(*panel, QStringLiteral("ShoulderRadius"));
    EXPECT_EQ(slider->mapping().getMax(), 2 * nose.getBaseRadius());
}

TEST_F(ShoulderPanelTest, EditsReachEverySelectedTransition)
{
    const EveryKindDesign design;
    auto&       first  = dynamic_cast<Transition&>(design.component(ComponentKind::TRANSITION));
    Transition& second = design.boosterTube->getParent()->addChild(std::make_unique<Transition>());
    const std::array<RocketComponent*, 1> others{&second};
    PanelHost                             host(first, others);
    auto*                                 panel = &host.make<ShoulderPanel>();
    auto* length = childNamed<UnitSpinBox>(*panel, QStringLiteral("AftShoulderLength"));
    ASSERT_TRUE(length->binding()->commitText("3"));
    EXPECT_DOUBLE_EQ(first.getAftShoulderLength(), 0.03);
    EXPECT_DOUBLE_EQ(second.getAftShoulderLength(), 0.03);
    const bool capped = first.isAftShoulderCapped();
    childNamed<BoolCheckBox>(*panel, QStringLiteral("AftShoulderCapped"))->click();
    EXPECT_EQ(first.isAftShoulderCapped(), !capped);
    EXPECT_EQ(second.isAftShoulderCapped(), !capped);
}

TEST_F(ShoulderPanelTest, AComponentThatIsNoTransitionIsABug)
{
    const EveryKindDesign design;
    PanelHost             host(design.component(ComponentKind::BODY_TUBE));
    EXPECT_THROW(static_cast<void>(&host.make<ShoulderPanel>()), BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
