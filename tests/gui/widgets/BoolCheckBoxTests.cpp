// The check box of a property (Java: a JCheckBox on a BooleanModel) and of a number field's
// automatic state (a JCheckBox on DoubleModel.getAutomaticAction()), with its enable followers
// (BooleanModel.addEnableComponent()).

#include "QtRocket/gui/widgets/BoolCheckBox.h"

#include <array>
#include <memory>
#include <tuple>
#include <utility>

#include <QLabel>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/BoolBinding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "RecordingGuardHook.h"
#include "binding/BindingTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using BoolCheckBoxTest = Test::GuiTest;
using Test::bodyTubeLength;
using Test::bodyTubeOuterRadius;
using Test::RecordingGuardHook;
using Test::TwoTubeRocket;

/// The source of a component's mass override (Java: "MassOverridden").
[[nodiscard]] BoolSource massOverridden()
{
    return BoolSource::of<RocketComponent>({
        .get = [](const RocketComponent& component) { return component.isMassOverridden(); },
        .set = [](RocketComponent& component, bool on) { component.setMassOverridden(on); },
    });
}

/// A binding of a component's mass override on @p selection.
[[nodiscard]] std::shared_ptr<BoolBinding> massBinding(
    const std::shared_ptr<ComponentSelection>& selection)
{
    return std::make_shared<BoolBinding>(selection, massOverridden());
}

/// A binding of @p source on @p component in the length units.
[[nodiscard]] std::shared_ptr<DoubleBinding> lengthBinding(RocketComponent&    component,
                                                           const DoubleSource& source)
{
    return std::make_shared<DoubleBinding>(std::make_shared<ComponentSelection>(component), source,
                                           unitGroup(UnitGroupId::LENGTH),
                                           DoubleBinding::Options{});
}

/// A boolean that is not a component's, whose setter refuses true: the check box shows false
/// again after a click.
struct RefusingBean
{
    bool value{false};
    int  sets{0};

    [[nodiscard]] BoolSource source()
    {
        return BoolSource::of<RocketComponent>({
            .get = [this](const RocketComponent&) { return value; },
            .set = [this](RocketComponent&, bool) { ++sets; },
        });
    }
};

TEST_F(BoolCheckBoxTest, NeedsABinding)
{
    EXPECT_THROW(std::ignore = BoolCheckBox(std::shared_ptr<BoolBinding>(), QString(), nullptr),
                 BugError);
    EXPECT_THROW(std::ignore = BoolCheckBox(std::shared_ptr<DoubleBinding>(), QString(), nullptr),
                 BugError);
}

TEST_F(BoolCheckBoxTest, ShowsTheValueAndTheText)
{
    const TwoTubeRocket design;
    design.tubeA->setMassOverridden(true);
    const auto         selection = std::make_shared<ComponentSelection>(*design.tubeA);
    const BoolCheckBox check(massBinding(selection), QStringLiteral("Override mass:"), nullptr);
    EXPECT_TRUE(check.isChecked());
    EXPECT_EQ(check.text(), QStringLiteral("Override mass:"));
    EXPECT_TRUE(check.isEnabled());
    EXPECT_NE(check.binding(), nullptr);
    EXPECT_EQ(check.numberBinding(), nullptr);

    design.tubeA->setMassOverridden(false);  // a change elsewhere
    EXPECT_FALSE(check.isChecked());
}

TEST_F(BoolCheckBoxTest, AClickSetsEverySelectedComponent)
{
    const TwoTubeRocket design;
    const auto          selection = std::make_shared<ComponentSelection>(
        *design.tubeA, std::array<RocketComponent*, 1>{design.tubeB});
    BoolCheckBox check(massBinding(selection), QString(), nullptr);
    check.click();
    EXPECT_TRUE(check.isChecked());
    EXPECT_TRUE(design.tubeA->isMassOverridden());
    EXPECT_TRUE(design.tubeB->isMassOverridden());
    check.click();
    EXPECT_FALSE(design.tubeA->isMassOverridden());
    EXPECT_FALSE(design.tubeB->isMassOverridden());
}

TEST_F(BoolCheckBoxTest, ShowsTheValueAgainWhenTheSetterRefusesIt)
{
    const TwoTubeRocket design;
    RefusingBean        bean;
    BoolCheckBox check(std::make_shared<BoolBinding>(
                           std::make_shared<ComponentSelection>(*design.tubeA), bean.source()),
                       QString(), nullptr);
    check.click();
    EXPECT_EQ(bean.sets, 1);
    EXPECT_FALSE(check.isChecked());
}

TEST_F(BoolCheckBoxTest, TheSourcesEnabledStateAndTheFieldEnableItTogether)
{
    // Java: BooleanModel's is<Name>Enabled (Transition.isClippedEnabled(): the shapes that can be
    // clipped), and a dialog's setEnabled().
    const TwoTubeRocket design;
    auto&               transition = design.stage->addChild(std::make_unique<Transition>());
    const auto          clipped    = BoolSource::of<Transition>({
        .get       = [](const Transition& part) { return part.isClipped(); },
        .set       = [](Transition& part, bool on) { part.setClipped(on); },
        .isEnabled = [](const Transition& part) { return part.isClippedEnabled(); },
    });
    transition.setShapeType(TransitionShape::CONICAL);
    BoolCheckBox check(
        std::make_shared<BoolBinding>(std::make_shared<ComponentSelection>(transition), clipped),
        QString(), nullptr);
    EXPECT_FALSE(check.isEnabled());
    transition.setShapeType(TransitionShape::ELLIPSOID);
    EXPECT_TRUE(check.isEnabled());
    check.setFieldEnabled(false);
    EXPECT_FALSE(check.isEnabled());
    EXPECT_FALSE(check.isFieldEnabled());
    transition.setShapeType(TransitionShape::HAACK);  // the binding allows it, the dialog not
    EXPECT_FALSE(check.isEnabled());
    check.setFieldEnabled(true);
    EXPECT_TRUE(check.isEnabled());
}

TEST_F(BoolCheckBoxTest, AnAutomaticCheckBoxSetsTheAutomaticState)
{
    // Java: JCheckBox(od.getAutomaticAction()) of BodyTubeConfig's outer diameter.
    const TwoTubeRocket design;
    BoolCheckBox        check(lengthBinding(*design.tubeA, bodyTubeOuterRadius()),
                              QStringLiteral("Automatic"), nullptr);
    EXPECT_EQ(check.binding(), nullptr);
    EXPECT_NE(check.numberBinding(), nullptr);
    EXPECT_FALSE(check.isChecked());
    EXPECT_TRUE(check.isEnabled());
    check.click();
    EXPECT_TRUE(design.tubeA->isOuterRadiusAutomatic());
    EXPECT_TRUE(check.isChecked());

    design.tubeA->setOuterRadius(0.05);  // typing a radius turns automatic off (the core's rule)
    EXPECT_FALSE(design.tubeA->isOuterRadiusAutomatic());
    EXPECT_FALSE(check.isChecked());
}

TEST_F(BoolCheckBoxTest, AnAutomaticCheckBoxWithoutAnAutomaticStateIsDisabled)
{
    // Java: AutomaticActionModel.isEnabled() is isAutomaticAvailable().
    const TwoTubeRocket design;
    BoolCheckBox        check(lengthBinding(*design.tubeA, bodyTubeLength()), QString(), nullptr);
    EXPECT_FALSE(check.isEnabled());
    check.setFieldEnabled(true);
    EXPECT_FALSE(check.isEnabled());
    EXPECT_THROW(check.addEnableFollower(check), BugError);
}

TEST_F(BoolCheckBoxTest, FollowersAreEnabledExactlyWhileTheValueIsTheirState)
{
    // Java: the override tab's spinner, unit selector and slider follow "MassOverridden"
    // (RocketComponentConfig.java:570-603), a label of the opposite state for good measure.
    const TwoTubeRocket design;
    const auto          selection = std::make_shared<ComponentSelection>(*design.tubeA);
    QWidget             window;
    auto*               check = makeChild<BoolCheckBox>(&window, massBinding(selection), QString());
    auto const          mass  = std::make_shared<DoubleBinding>(
        selection, bodyTubeLength(), unitGroup(UnitGroupId::LENGTH), DoubleBinding::Options{});
    auto* spin    = makeChild<UnitSpinBox>(&window, mass);
    auto* units   = makeChild<UnitSelector>(&window, mass);
    auto* slider  = makeChild<ValueSlider>(&window, mass, SliderMapping::linear(0, 1));
    auto* opposed = makeChild<QLabel>(&window);
    check->addEnableFollower(*spin);
    check->addEnableFollower(*units);
    check->addEnableFollower(*slider);
    check->addEnableFollower(*opposed, false);
    EXPECT_TRUE(check->hasEnableFollower(*spin));
    EXPECT_FALSE(spin->isEnabled());
    EXPECT_FALSE(units->isEnabled());
    EXPECT_FALSE(slider->isEnabled());
    EXPECT_TRUE(opposed->isEnabled());
    EXPECT_FALSE(
        spin->isFieldEnabled());  // the dialog's part: the spin box's binding keeps its say

    check->click();
    EXPECT_TRUE(spin->isEnabled());
    EXPECT_TRUE(units->isEnabled());
    EXPECT_TRUE(slider->isEnabled());
    EXPECT_FALSE(opposed->isEnabled());

    design.tubeA->setMassOverridden(false);  // a change elsewhere
    EXPECT_FALSE(spin->isEnabled());
    EXPECT_TRUE(opposed->isEnabled());
}

TEST_F(BoolCheckBoxTest, ARemovedFollowerKeepsItsState)
{
    // Java: removeEnableComponent() for the "overridden by" case, then setEnabled(false).
    const TwoTubeRocket design;
    const auto          selection = std::make_shared<ComponentSelection>(*design.tubeA);
    QWidget             window;
    auto*               check = makeChild<BoolCheckBox>(&window, massBinding(selection), QString());
    auto*               label = makeChild<QLabel>(&window);
    check->addEnableFollower(*label);
    EXPECT_FALSE(label->isEnabled());
    check->removeEnableFollower(*label);
    EXPECT_FALSE(check->hasEnableFollower(*label));
    check->click();
    EXPECT_FALSE(label->isEnabled());
    check->removeEnableFollower(*label);  // no follower: nothing

    // A follower added again takes its new state.
    check->addEnableFollower(*label, true);
    EXPECT_TRUE(label->isEnabled());
    check->addEnableFollower(*label, false);
    EXPECT_FALSE(label->isEnabled());
}

TEST_F(BoolCheckBoxTest, ADestroyedFollowerIsForgotten)
{
    const TwoTubeRocket design;
    const auto          selection = std::make_shared<ComponentSelection>(*design.tubeA);
    QWidget             window;
    auto*               check = makeChild<BoolCheckBox>(&window, massBinding(selection), QString());
    {
        QLabel label;
        check->addEnableFollower(label);
        EXPECT_TRUE(check->hasEnableFollower(label));
    }
    check->click();  // reaches no destroyed follower (ASan)
    EXPECT_TRUE(check->isChecked());
}

TEST_F(BoolCheckBoxTest, AnInvalidSelectionDisablesTheCheckBoxAndItsFollowers)
{
    const TwoTubeRocket design;
    design.tubeA->setMassOverridden(true);
    const auto selection = std::make_shared<ComponentSelection>(*design.tubeA);
    QWidget    window;
    auto*      check = makeChild<BoolCheckBox>(&window, massBinding(selection), QString());
    auto*      label = makeChild<QLabel>(&window);
    check->addEnableFollower(*label);
    EXPECT_TRUE(label->isEnabled());
    std::ignore = design.stage->addChild(std::make_unique<BodyTube>());
    EXPECT_FALSE(check->isEnabled());
    EXPECT_FALSE(label->isEnabled());
    EXPECT_TRUE(check->isChecked());  // the last value
    check->click();                   // disabled: nothing
    EXPECT_TRUE(design.tubeA->isMassOverridden());
}

TEST_F(BoolCheckBoxTest, ACoreErrorOfAClickIsReported)
{
    const TwoTubeRocket      design;
    const RecordingGuardHook hook;
    const auto               throwing = BoolSource::of<RocketComponent>({
        .get = [](const RocketComponent&) { return false; },
        .set = [](RocketComponent&, bool) { bug("a setter's broken invariant"); },
    });
    BoolCheckBox check(std::make_shared<BoolBinding>(
                           std::make_shared<ComponentSelection>(*design.tubeA), throwing),
                       QString(), nullptr);
    check.setObjectName(QStringLiteral("Throwing"));
    check.click();
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().context, QStringLiteral("Throwing"));
    EXPECT_FALSE(check.isChecked());
}

}  // namespace

}  // namespace QtRocket::Gui
