// The model of a check box (Java: BooleanModel) and its enable followers.
//
// Expectations marked "probe" are OpenRocket's own results, printed by the Java probe of this part
// (scratchpad probes/m2r2-B2/java/src/probe/B2Probe.java -> b2-probe.out, run on OpenRocket
// 5f164fd0e with JDK 17: BooleanModel(tube, "MassOverridden") with two JPanels added by
// addEnableComponent(panel, true) and (panel, false); BooleanModel(transition, "Clipped");
// BooleanModel(true)).

#include "QtRocket/gui/binding/BoolBinding.h"

#include <array>
#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include "BindingTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

namespace
{

using BoolBindingTest = Test::GuiTest;
using Test::ChangeCounter;
using Test::RocketEventRecorder;
using Test::TwoTubeRocket;

/// The source of a component's mass override (Java: "MassOverridden").
[[nodiscard]] BoolSource massOverridden()
{
    return BoolSource::of<RocketComponent>({
        .get = [](const RocketComponent& component) { return component.isMassOverridden(); },
        .set = [](RocketComponent& component, bool on) { component.setMassOverridden(on); },
    });
}

/// The source of a component's mass override without a setter.
[[nodiscard]] BoolSource massOverriddenReadOnly()
{
    return BoolSource::of<RocketComponent>({
        .get = [](const RocketComponent& component) { return component.isMassOverridden(); },
    });
}

/// The source of a transition's "Clipped", enabled for the shapes that can be clipped.
[[nodiscard]] BoolSource clipped()
{
    return BoolSource::of<Transition>({
        .get       = [](const Transition& transition) { return transition.isClipped(); },
        .set       = [](Transition& transition, bool on) { transition.setClipped(on); },
        .isEnabled = [](const Transition& transition) { return transition.isClippedEnabled(); },
    });
}

/// A boolean that is not a component's, with a count of the setter's calls; the setter fires a
/// NONFUNCTIONAL change of the component it is called for.
struct BoolBean
{
    bool value{false};
    bool enabled{true};
    int  sets{0};

    [[nodiscard]] BoolSource source()
    {
        return BoolSource::of<RocketComponent>({
            .get = [this](const RocketComponent&) { return value; },
            .set =
                [this](RocketComponent& component, bool newValue) {
                    ++sets;
                    value = newValue;
                    component.fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
                },
            .isEnabled = [this](const RocketComponent&) { return enabled; },
        });
    }
};

/// An enable follower: the states it was told, the last one first.
struct Follower
{
    std::optional<bool> enabled;
    int                 calls{0};
};

/// Adds @p follower to @p binding as a follower of @p enableState.
[[nodiscard]] BoolBinding::FollowerSignal::ScopedConnection follow(BoolBinding& binding,
                                                                   Follower&    follower,
                                                                   bool         enableState)
{
    return BoolBinding::FollowerSignal::ScopedConnection(binding.addEnableFollower(
        [&follower](bool enabled) {
            follower.enabled = enabled;
            ++follower.calls;
        },
        enableState));
}

TEST_F(BoolBindingTest, FollowersFollowTheValueAsOpenRocket)
{
    // probe: value=false enabled=true follower(true)=false follower(false)=true;
    // source set true: value=true follower(true)=true follower(false)=false;
    // setValue(false): value=false follower(true)=false follower(false)=true.
    TwoTubeRocket design;
    const auto    overridden = std::make_shared<BoolBinding>(
        std::make_shared<ComponentSelection>(*design.tubeA), massOverridden());
    Follower   whenTrue;
    Follower   whenFalse;
    const auto first  = follow(*overridden, whenTrue, true);
    const auto second = follow(*overridden, whenFalse, false);
    EXPECT_FALSE(overridden->value());
    EXPECT_TRUE(overridden->isEnabled());
    EXPECT_EQ(whenTrue.enabled, false);
    EXPECT_EQ(whenFalse.enabled, true);

    design.tubeA->setMassOverridden(true);
    EXPECT_TRUE(overridden->value());
    EXPECT_EQ(whenTrue.enabled, true);
    EXPECT_EQ(whenFalse.enabled, false);
    EXPECT_TRUE(overridden->isFollowerEnabled(true));
    EXPECT_FALSE(overridden->isFollowerEnabled(false));

    overridden->setValue(false);
    EXPECT_FALSE(design.tubeA->isMassOverridden());
    EXPECT_EQ(whenTrue.enabled, false);
    EXPECT_EQ(whenFalse.enabled, true);
    EXPECT_EQ(whenTrue.calls, 3);
}

TEST_F(BoolBindingTest, ClippedIsEnabledForTheShapesThatClipAsOpenRocket)
{
    // probe: Transition Conical: clipped=false enabled=false; Ellipsoid: clipped=true
    // enabled=true; Ogive: clipped=false enabled=false.
    TwoTubeRocket design;
    Transition&   transition = design.stage->addChild(std::make_unique<Transition>());
    const auto    clip =
        std::make_shared<BoolBinding>(std::make_shared<ComponentSelection>(transition), clipped());
    EXPECT_EQ(transition.getShapeType(), TransitionShape::CONICAL);
    EXPECT_FALSE(clip->value());
    EXPECT_FALSE(clip->isEnabled());
    const ChangeCounter changes(*clip);
    transition.setShapeType(TransitionShape::ELLIPSOID);
    EXPECT_TRUE(clip->value());
    EXPECT_TRUE(clip->isEnabled());
    transition.setShapeType(TransitionShape::OGIVE);
    EXPECT_FALSE(clip->value());
    EXPECT_FALSE(clip->isEnabled());
    EXPECT_EQ(changes.count(), 2);
}

TEST_F(BoolBindingTest, OwnValueAsOpenRocket)
{
    // probe: BooleanModel(true), addEnableComponent(panel), setValue(false): own value=false
    // follower=false.
    const auto own = std::make_shared<BoolBinding>(true);
    Follower   follower;
    const auto connection = follow(*own, follower, true);
    EXPECT_EQ(follower.enabled, true);
    const ChangeCounter changes(*own);
    own->setValue(false);
    EXPECT_FALSE(own->value());
    EXPECT_EQ(follower.enabled, false);
    EXPECT_EQ(changes.count(), 1);
    own->setValue(false);
    EXPECT_EQ(changes.count(), 1);
    EXPECT_TRUE(own->setChecked(true));
    EXPECT_TRUE(own->value());
    EXPECT_TRUE(own->isValid());
    EXPECT_TRUE(own->isEnabled());
    EXPECT_EQ(own->getSelection(), nullptr);
}

TEST_F(BoolBindingTest, SetCheckedWritesOnlyANewValue)
{
    // Java: actionPerformed() calls setValue() only when the box's state differs from the value.
    TwoTubeRocket                         design;
    BoolBean                              bean;
    const std::array<RocketComponent*, 1> others{design.tubeB};
    const auto                            check = std::make_shared<BoolBinding>(
        std::make_shared<ComponentSelection>(*design.tubeA, others), bean.source());
    EXPECT_FALSE(check->setChecked(false));
    EXPECT_EQ(bean.sets, 0);
    EXPECT_TRUE(check->setChecked(true));
    EXPECT_EQ(bean.sets, 2);  // the other, then the main
    EXPECT_TRUE(check->value());
    // setValue() writes the value shown all the same (Java's setValue()).
    check->setValue(true);
    EXPECT_EQ(bean.sets, 4);
}

TEST_F(BoolBindingTest, AMultiEditSetsEveryComponent)
{
    TwoTubeRocket design;
    design.tubeB->setMassOverridden(true);
    const std::array<RocketComponent*, 1> others{design.tubeB};
    const auto                            overridden = std::make_shared<BoolBinding>(
        std::make_shared<ComponentSelection>(*design.tubeA, others), massOverridden());
    const RocketEventRecorder events(*design.rocket);
    overridden->setValue(false);  // the main's own value reaches the other
    EXPECT_FALSE(design.tubeB->isMassOverridden());
    EXPECT_EQ(events.count(), 1U);
    EXPECT_TRUE(overridden->setChecked(true));
    EXPECT_TRUE(design.tubeA->isMassOverridden());
    EXPECT_TRUE(design.tubeB->isMassOverridden());
    EXPECT_EQ(events.count(), 2U);
}

TEST_F(BoolBindingTest, FollowersHearOfTheValueOnly)
{
    // Java's updateEnableStatus() runs when the value changes, not when the enabled state does.
    TwoTubeRocket design;
    BoolBean      bean;
    const auto    check = std::make_shared<BoolBinding>(
        std::make_shared<ComponentSelection>(*design.tubeA), bean.source());
    Follower   follower;
    const auto connection = follow(*check, follower, false);
    EXPECT_EQ(follower.calls, 1);
    bean.enabled = false;
    check->getSelection()->refresh();
    EXPECT_FALSE(check->isEnabled());
    EXPECT_EQ(follower.calls, 1);
    EXPECT_EQ(follower.enabled, true);
}

TEST_F(BoolBindingTest, AFollowerGoesWithItsConnection)
{
    // Java: removeEnableComponent().
    TwoTubeRocket design;
    const auto    overridden = std::make_shared<BoolBinding>(
        std::make_shared<ComponentSelection>(*design.tubeA), massOverridden());
    Follower follower;
    {
        const auto connection = follow(*overridden, follower, true);
    }
    design.tubeA->setMassOverridden(true);
    EXPECT_EQ(follower.calls, 1);
    EXPECT_EQ(follower.enabled, false);
}

/// What the check box's write gave while the binding told its listeners.
struct FiringWrite
{
    std::optional<bool> checked;
};

/// Tries the check box's write of @p binding from its change listener, into @p write.
[[nodiscard]] Signal<>::ScopedConnection tryWriteWhileFiring(BoolBinding& binding,
                                                             FiringWrite& write)
{
    return Signal<>::ScopedConnection(binding.addChangeListener(
        [&binding, &write] { write.checked = binding.setChecked(!binding.value()); }));
}

TEST_F(BoolBindingTest, TheCheckBoxWriteIsDroppedWhileFiring)
{
    TwoTubeRocket design;
    BoolBean      bean;
    const auto    check = std::make_shared<BoolBinding>(
        std::make_shared<ComponentSelection>(*design.tubeA), bean.source());
    FiringWrite                      write;
    const Signal<>::ScopedConnection connection = tryWriteWhileFiring(*check, write);
    check->setValue(true);
    EXPECT_EQ(write.checked, false);
    EXPECT_TRUE(bean.value);
    EXPECT_EQ(bean.sets, 1);
}

TEST_F(BoolBindingTest, AnInvalidSelectionDisablesTheFollowers)
{
    TwoTubeRocket design;
    const auto    overridden = std::make_shared<BoolBinding>(
        std::make_shared<ComponentSelection>(*design.tubeA), massOverridden());
    Follower            whenFalse;
    const auto          connection = follow(*overridden, whenFalse, false);
    const ChangeCounter changes(*overridden);
    design.rocket->fireComponentChangeEvent(ComponentChangeEvent::kUndoChange);
    EXPECT_FALSE(overridden->isValid());
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(whenFalse.enabled, false);
    EXPECT_FALSE(overridden->isFollowerEnabled(false));
    EXPECT_FALSE(overridden->isEnabled());
    EXPECT_FALSE(overridden->setChecked(true));
    overridden->setValue(true);
    EXPECT_FALSE(design.tubeA->isMassOverridden());
    // A follower added now is disabled at once.
    Follower   late;
    const auto lateConnection = follow(*overridden, late, false);
    EXPECT_EQ(late.enabled, false);
}

TEST_F(BoolBindingTest, RefusesAMissingSelectionOrSetter)
{
    TwoTubeRocket design;
    EXPECT_THROW(static_cast<void>(BoolBinding(nullptr, massOverridden())), BugError);
    EXPECT_THROW(static_cast<void>(BoolBinding(std::make_shared<ComponentSelection>(*design.tubeA),
                                               massOverriddenReadOnly())),
                 BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
