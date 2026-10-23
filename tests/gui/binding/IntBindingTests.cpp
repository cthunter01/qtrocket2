// The model of a count field (Java: IntegerModel with its spinner model).
//
// Expectations marked "probe" are OpenRocket's own results, printed by the Java probe of this part
// (tools/openrocket-gui-pins/material-binding/probe/B2Probe.java, run on OpenRocket
// 5f164fd0e with JDK 17: IntegerModel(fins, "FinCount", 1, 8) and IntegerModel(parachute,
// "LineCount", 0)).

#include "QtRocket/gui/binding/IntBinding.h"

#include <array>
#include <limits>
#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include "BindingTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

namespace
{

using IntBindingTest = Test::GuiTest;
using Test::ChangeCounter;
using Test::RocketEventRecorder;
using Test::TwoTubeRocket;

constexpr int kIntMax = std::numeric_limits<int>::max();

/// The source of a fin set's fin count.
[[nodiscard]] IntSource finCount()
{
    return IntSource::of<FinSet>({
        .get = [](const FinSet& fins) { return fins.getFinCount(); },
        .set = [](FinSet& fins, int count) { fins.setFinCount(count); },
    });
}

/// The source of a fin set's fin count without a setter.
[[nodiscard]] IntSource finCountReadOnly()
{
    return IntSource::of<FinSet>({.get = [](const FinSet& fins) { return fins.getFinCount(); }});
}

/// The source of a parachute's line count.
[[nodiscard]] IntSource lineCount()
{
    return IntSource::of<Parachute>({
        .get = [](const Parachute& parachute) { return parachute.getLineCount(); },
        .set = [](Parachute& parachute, int count) { parachute.setLineCount(count); },
    });
}

/// An integer that is not a component's (the support's IntBeanProperty).
using IntBean = Test::IntBeanProperty;

/// A rocket of two tubes, with a trapezoidal fin set on each.
struct FinRocket
{
    TwoTubeRocket    design;
    TrapezoidFinSet* finsA{&design.tubeA->addChild(std::make_unique<TrapezoidFinSet>())};
    TrapezoidFinSet* finsB{&design.tubeB->addChild(std::make_unique<TrapezoidFinSet>())};
};

TEST_F(IntBindingTest, FinCountStepsAndClampsAsOpenRocket)
{
    // probe: FinCount=3 next=4 previous=2; setValue(10): 8 next=null previous=7;
    // setValue(-5): 1 next=2 previous=null; spinner setValue(4): 4.
    const FinRocket rocket;
    const auto      count = std::make_shared<IntBinding>(
        std::make_shared<ComponentSelection>(*rocket.finsA), finCount(), 1, 8);
    EXPECT_EQ(count->value(), 3);
    EXPECT_EQ(count->nextValue(), 4);
    EXPECT_EQ(count->previousValue(), 2);

    count->setValue(10);
    EXPECT_EQ(rocket.finsA->getFinCount(), 8);
    EXPECT_EQ(count->value(), 8);
    EXPECT_EQ(count->nextValue(), std::nullopt);
    EXPECT_EQ(count->previousValue(), 7);

    count->setValue(-5);
    EXPECT_EQ(count->value(), 1);
    EXPECT_EQ(count->nextValue(), 2);
    EXPECT_EQ(count->previousValue(), std::nullopt);

    EXPECT_TRUE(count->setSpinnerValue(4));
    EXPECT_EQ(rocket.finsA->getFinCount(), 4);
    EXPECT_EQ(count->minimum(), 1);
    EXPECT_EQ(count->maximum(), 8);
}

TEST_F(IntBindingTest, LineCountIsBoundedBelowOnly)
{
    // probe: LineCount=0 next=1 previous=null; LineCount=MAX next=null previous=2147483646.
    TwoTubeRocket design;
    Parachute&    chute = design.tubeA->addChild(std::make_unique<Parachute>());
    const auto    lines =
        std::make_shared<IntBinding>(std::make_shared<ComponentSelection>(chute), lineCount(), 0);
    lines->setValue(0);
    EXPECT_EQ(lines->nextValue(), 1);
    EXPECT_EQ(lines->previousValue(), std::nullopt);
    lines->setValue(kIntMax);
    EXPECT_EQ(chute.getLineCount(), kIntMax);
    EXPECT_EQ(lines->nextValue(), std::nullopt);
    EXPECT_EQ(lines->previousValue(), kIntMax - 1);
    EXPECT_EQ(lines->maximum(), kIntMax);
}

TEST_F(IntBindingTest, AValueBeyondABoundStepsBackIn)
{
    // Java: getPreviousValue() of 12 under the maximum 8 is 11, which setValue() clamps to 8.
    TwoTubeRocket design;
    IntBean       bean{.value = 12};
    const auto    count = std::make_shared<IntBinding>(
        std::make_shared<ComponentSelection>(*design.tubeA), bean.source(), 1, 8);
    EXPECT_EQ(count->nextValue(), std::nullopt);
    EXPECT_EQ(count->previousValue(), 11);
    EXPECT_FALSE(count->stepUp());
    EXPECT_TRUE(count->stepDown());
    EXPECT_EQ(bean.value, 8);
    EXPECT_EQ(count->value(), 8);
    EXPECT_TRUE(count->stepDown());
    EXPECT_EQ(bean.value, 7);
    EXPECT_TRUE(count->stepUp());
    EXPECT_EQ(bean.value, 8);
}

TEST_F(IntBindingTest, SetsTheValueEvenWhenTheMainHasItAlready)
{
    // Java's setValue() calls the setter every time; the main's setter forwards it to the others.
    TwoTubeRocket                         design;
    IntBean                               bean{.value = 5};
    const std::array<RocketComponent*, 1> others{design.tubeB};
    const auto                            count = std::make_shared<IntBinding>(
        std::make_shared<ComponentSelection>(*design.tubeA, others), bean.source());
    const ChangeCounter changes(*count);
    count->setValue(5);
    EXPECT_EQ(bean.sets, 2);  // the other, then the main
    EXPECT_EQ(changes.count(), 0);
}

TEST_F(IntBindingTest, AMultiEditSetsEveryFinSetInOneEvent)
{
    FinRocket const rocket;
    rocket.finsB->setFinCount(5);
    const std::array<RocketComponent*, 1> others{rocket.finsB};
    const auto                            count = std::make_shared<IntBinding>(
        std::make_shared<ComponentSelection>(*rocket.finsA, others), finCount(), 1, 8);
    const RocketEventRecorder events(*rocket.design.rocket);
    const ChangeCounter       changes(*count);

    count->setValue(3);  // the main's own count: the other gets it too
    EXPECT_EQ(rocket.finsA->getFinCount(), 3);
    EXPECT_EQ(rocket.finsB->getFinCount(), 3);
    EXPECT_EQ(events.count(), 1U);
    EXPECT_EQ(changes.count(), 0);  // the main's count did not change

    count->setValue(6);
    EXPECT_EQ(rocket.finsA->getFinCount(), 6);
    EXPECT_EQ(rocket.finsB->getFinCount(), 6);
    EXPECT_EQ(events.count(), 2U);
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(count->value(), 6);
}

TEST_F(IntBindingTest, ReReadsTheSourceOnEveryEvent)
{
    // The scout's decision D16: Java's IntegerModel is no listener of its source.
    FinRocket const rocket;
    const auto      count = std::make_shared<IntBinding>(
        std::make_shared<ComponentSelection>(*rocket.finsA), finCount(), 1, 8);
    const ChangeCounter changes(*count);
    rocket.finsA->setFinCount(6);
    EXPECT_EQ(count->value(), 6);
    EXPECT_EQ(changes.count(), 1);
    rocket.finsA->setName("Fins");  // another property: nothing to tell
    EXPECT_EQ(changes.count(), 1);
}

TEST_F(IntBindingTest, FollowsTheEnabledStateOfTheSource)
{
    TwoTubeRocket design;
    IntBean       bean{.value = 2, .enabled = false};
    const auto    count = std::make_shared<IntBinding>(
        std::make_shared<ComponentSelection>(*design.tubeA), bean.source());
    EXPECT_FALSE(count->isEnabled());
    const ChangeCounter changes(*count);
    bean.enabled = true;
    count->getSelection()->refresh();
    EXPECT_TRUE(count->isEnabled());
    EXPECT_EQ(changes.count(), 1);
}

/// What the spin box's writes gave while the binding told its listeners.
struct FiringWrites
{
    std::optional<bool> spinner;
    std::optional<bool> up;
    std::optional<bool> down;
};

/// Tries every widget write of @p binding from its change listener, into @p writes.
[[nodiscard]] Signal<>::ScopedConnection tryWritesWhileFiring(IntBinding&   binding,
                                                              FiringWrites& writes)
{
    return Signal<>::ScopedConnection(binding.addChangeListener([&binding, &writes] {
        writes.spinner = binding.setSpinnerValue(9);
        writes.up      = binding.stepUp();
        writes.down    = binding.stepDown();
    }));
}

TEST_F(IntBindingTest, WidgetWritesAreDroppedWhileFiring)
{
    TwoTubeRocket design;
    IntBean       bean{.value = 2};
    const auto    count = std::make_shared<IntBinding>(
        std::make_shared<ComponentSelection>(*design.tubeA), bean.source());
    FiringWrites                     writes;
    const Signal<>::ScopedConnection connection = tryWritesWhileFiring(*count, writes);
    count->setValue(4);
    EXPECT_EQ(writes.spinner, false);
    EXPECT_EQ(writes.up, false);
    EXPECT_EQ(writes.down, false);
    EXPECT_EQ(bean.value, 4);
    EXPECT_EQ(bean.sets, 1);
}

TEST_F(IntBindingTest, AnInvalidSelectionShowsAndAcceptsNothing)
{
    FinRocket const rocket;
    const auto      count = std::make_shared<IntBinding>(
        std::make_shared<ComponentSelection>(*rocket.finsA), finCount(), 1, 8);
    const ChangeCounter changes(*count);
    rocket.design.rocket->fireComponentChangeEvent(ComponentChangeEvent::kTreeChange);
    EXPECT_FALSE(count->isValid());
    EXPECT_EQ(changes.count(), 1);
    EXPECT_FALSE(count->isEnabled());
    EXPECT_EQ(count->value(), 3);  // the last value read
    EXPECT_EQ(count->nextValue(), std::nullopt);
    EXPECT_EQ(count->previousValue(), std::nullopt);
    EXPECT_FALSE(count->setSpinnerValue(5));
    EXPECT_FALSE(count->stepUp());
    count->setValue(5);
    EXPECT_EQ(rocket.finsA->getFinCount(), 3);
}

TEST_F(IntBindingTest, RefusesAMissingSelectionSetterOrBounds)
{
    const FinRocket rocket;
    EXPECT_THROW(static_cast<void>(IntBinding(nullptr, finCount())), BugError);
    const auto selection = std::make_shared<ComponentSelection>(*rocket.finsA);
    EXPECT_THROW(static_cast<void>(IntBinding(selection, finCountReadOnly())), BugError);
    EXPECT_THROW(static_cast<void>(IntBinding(selection, finCount(), 8, 1)), BugError);
    // The main component must be of the source's class.
    const auto tube = std::make_shared<ComponentSelection>(*rocket.design.tubeA);
    EXPECT_THROW(static_cast<void>(IntBinding(tube, finCount())), BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
