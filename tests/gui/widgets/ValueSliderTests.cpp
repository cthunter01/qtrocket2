// The slider of a number field (Java: BasicSlider on DoubleModel's slider models).
//
// Expectations marked "probe" are OpenRocket's own results, printed by the bindings scout's Java
// probe (handoff/m2-scout/evidence/m2-scout-bindings/java/src/probe/SliderProbe.java ->
// slider-java.out, OpenRocket 5f164fd0e, JDK 17): DoubleModel(bean, "Value", UNITS_LENGTH,
// -infinity) with getSliderModel(0, 0.04, 0.2), each position set and the position read back.
// The values are + - * / and sqrt and the unit's rounding, so they are pinned exactly.

#include "QtRocket/gui/widgets/ValueSlider.h"

#include <limits>
#include <memory>

#include <QSlider>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "WidgetTestSupport.h"
#include "binding/BindingTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using ValueSliderTest = Test::GuiTest;
using Test::BeanProperty;
using Test::turnWheel;
using Test::TwoTubeRocket;

/// The probe's model of @p bean on @p component: the length units, a minimum of -infinity.
[[nodiscard]] std::shared_ptr<DoubleBinding> probeBinding(RocketComponent& component,
                                                          BeanProperty&    bean)
{
    return std::make_shared<DoubleBinding>(
        std::make_shared<ComponentSelection>(component), bean.source(),
        unitGroup(UnitGroupId::LENGTH),
        DoubleBinding::Options{.min = -std::numeric_limits<double>::infinity()});
}

TEST_F(ValueSliderTest, NeedsABinding)
{
    EXPECT_THROW(static_cast<void>(ValueSlider(nullptr, SliderMapping::linear(0, 1), nullptr)),
                 BugError);
}

TEST_F(ValueSliderTest, IsAHorizontalSliderOfAThousandStepsWithoutFocus)
{
    // Java's BasicSlider: horizontal, not focusable, no labels; the model's range 0 to 1000.
    const TwoTubeRocket rocket;
    BeanProperty        bean{.value = 0.02};
    const ValueSlider   slider(probeBinding(*rocket.tubeA, bean), SliderMapping::linear(0, 0.04),
                               nullptr);
    EXPECT_EQ(slider.orientation(), Qt::Horizontal);
    EXPECT_EQ(slider.minimum(), 0);
    EXPECT_EQ(slider.maximum(), SliderMapping::kMaximum);
    EXPECT_EQ(slider.pageStep(), 100);  // a tenth: Swing's BasicSliderUI.scrollByBlock()
    EXPECT_EQ(slider.focusPolicy(), Qt::NoFocus);
    EXPECT_EQ(slider.tickPosition(), QSlider::NoTicks);
    EXPECT_EQ(slider.value(), 500);
    EXPECT_TRUE(slider.mapping().isLinear());
    EXPECT_EQ(slider.dynamicBounds().min, nullptr);
    EXPECT_EQ(slider.dynamicBounds().max, nullptr);
}

TEST_F(ValueSliderTest, AMoveSetsTheRoundedValueAndShowsWhereItEndedUp)
{
    // probe: [0.0, 0.04, 0.2] mm: 125->0.01->125 625->0.058->627 875->0.14->880;
    // in: 625->0.05841999999999999->629.
    const TwoTubeRocket rocket;
    BeanProperty        bean{.value = 0};
    const auto          binding = probeBinding(*rocket.tubeA, bean);
    ValueSlider         slider(binding, SliderMapping::fromRange(0, 0.04, 0.2), nullptr);
    binding->setCurrentUnit(*unitGroup(UnitGroupId::LENGTH).getUnit("mm"));
    slider.setValue(125);
    EXPECT_EQ(bean.value, 0.01);
    EXPECT_EQ(slider.value(), 125);
    slider.setValue(625);
    EXPECT_EQ(bean.value, 0.058);
    EXPECT_EQ(slider.value(), 627);
    slider.setValue(875);
    EXPECT_EQ(bean.value, 0.14);
    EXPECT_EQ(slider.value(), 880);
    EXPECT_EQ(bean.sets, 3);

    binding->setCurrentUnit(*unitGroup(UnitGroupId::LENGTH).getUnit("in"));
    slider.setValue(625);
    EXPECT_EQ(bean.value, 0.05841999999999999);
    EXPECT_EQ(slider.value(), 629);
}

TEST_F(ValueSliderTest, FollowsTheValueWithoutWritingIt)
{
    const TwoTubeRocket rocket;
    BeanProperty        bean{.value = 0.01};
    const ValueSlider   slider(probeBinding(*rocket.tubeA, bean), SliderMapping::linear(0, 0.04),
                               nullptr);
    EXPECT_EQ(slider.value(), 250);
    bean.value = 0.03;
    rocket.tubeA->fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_EQ(slider.value(), 750);
    EXPECT_EQ(bean.sets, 0);  // a refresh is no move
}

TEST_F(ValueSliderTest, TakesNoTurnOfTheWheel)
{
    // Swing's slider takes no wheel: the turn goes on to the widgets around (a scroll area).
    const TwoTubeRocket rocket;
    BeanProperty        bean{.value = 0.02};
    ValueSlider slider(probeBinding(*rocket.tubeA, bean), SliderMapping::linear(0, 0.04), nullptr);
    EXPECT_FALSE(turnWheel(slider, 120));
    EXPECT_EQ(slider.value(), 500);
    EXPECT_EQ(bean.sets, 0);
}

TEST_F(ValueSliderTest, ALinearMappingFollowsItsBoundBindings)
{
    // Java: getSliderModel(new DoubleModel(0), od), the inner diameter's slider ending at the
    // outer diameter, and getSliderModel(DoubleModel min, double max) with a constant maximum.
    const TwoTubeRocket rocket;
    BeanProperty        bean{.value = 0.01};
    const auto          outer = std::make_shared<DoubleBinding>(0.04);
    const auto          lower = std::make_shared<DoubleBinding>(0.0);
    const ValueSlider   slider(probeBinding(*rocket.tubeA, bean),
                               SliderMapping::linear(lower->value(), outer->value()),
                               ValueSlider::DynamicBounds{.min = lower, .max = outer}, nullptr);
    EXPECT_EQ(slider.value(), 250);
    outer->setValue(0.08);
    EXPECT_EQ(slider.mapping().getMax(), 0.08);
    EXPECT_EQ(slider.mapping().getMid(), 0.08);
    EXPECT_EQ(slider.value(), 125);
    lower->setValue(-0.08);
    EXPECT_EQ(slider.mapping().getMin(), -0.08);
    EXPECT_EQ(slider.value(), 562);
    EXPECT_EQ(bean.sets, 0);

    BeanProperty      other{.value = 0.01};
    const ValueSlider constantMax(probeBinding(*rocket.tubeA, other),
                                  SliderMapping::linear(lower->value(), 0.02),
                                  ValueSlider::DynamicBounds{.min = lower}, nullptr);
    lower->setValue(0.0);
    EXPECT_EQ(constantMax.mapping().getMax(), 0.02);
    EXPECT_EQ(constantMax.value(), 500);
}

TEST_F(ValueSliderTest, AQuadraticMappingPutsItsMiddleAtAThirdWhenItsMaximumMoves)
{
    // Java: getSliderModel(min, mid, DoubleModel max) (BodyTubeConfig's length up to maxLength);
    // a change of the maximum puts mid at (max - min) / 3.
    const TwoTubeRocket rocket;
    BeanProperty        bean{.value = 0.5};
    const auto          maximum = std::make_shared<DoubleBinding>(2.0);
    const ValueSlider   slider(probeBinding(*rocket.tubeA, bean),
                               SliderMapping::quadratic(0, 0.5, 0.5, maximum->value()),
                               ValueSlider::DynamicBounds{.max = maximum}, nullptr);
    EXPECT_EQ(slider.mapping().getMid(), 0.5);
    EXPECT_EQ(slider.value(), 500);
    maximum->setValue(3.0);
    EXPECT_FALSE(slider.mapping().isLinear());
    EXPECT_EQ(slider.mapping().getMax(), 3.0);
    EXPECT_EQ(slider.mapping().getMid(), 1.0);
    EXPECT_EQ(slider.value(), 250);
}

TEST_F(ValueSliderTest, AnotherBindingTakesThePlaceOfTheFirst)
{
    // Java's InnerTubeConfig: the separation's slider between a relative and an absolute model.
    const TwoTubeRocket rocket;
    BeanProperty        first{.value = 0.01};
    BeanProperty        second{.value = 3.0};
    const auto          bound = std::make_shared<DoubleBinding>(0.04);
    ValueSlider         slider(probeBinding(*rocket.tubeA, first), SliderMapping::linear(0, 0.04),
                               ValueSlider::DynamicBounds{.max = bound}, nullptr);
    slider.setBinding(probeBinding(*rocket.tubeA, second), SliderMapping::fromRange(0, 1, 4));
    EXPECT_EQ(slider.dynamicBounds().max, nullptr);
    EXPECT_FALSE(slider.mapping().isLinear());
    const int position = slider.value();
    bound->setValue(0.02);  // no longer a bound of the slider
    EXPECT_EQ(slider.value(), position);
    EXPECT_EQ(slider.mapping().getMax(), 4.0);
    first.value = 0.03;
    rocket.tubeA->fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_EQ(slider.value(), position);
    slider.setValue(1000);
    EXPECT_EQ(second.value, 4.0);
    EXPECT_EQ(first.sets, 0);
}

TEST_F(ValueSliderTest, TheBindingAndTheFieldEnableIt)
{
    const TwoTubeRocket rocket;
    BeanProperty        bean{.value = 0.02};
    ValueSlider         slider(
        std::make_shared<DoubleBinding>(std::make_shared<ComponentSelection>(*rocket.tubeA),
                                        bean.checkedSource(), unitGroup(UnitGroupId::LENGTH),
                                        DoubleBinding::Options{}),
        SliderMapping::linear(0, 0.04), nullptr);
    EXPECT_TRUE(slider.isEnabled());
    slider.setFieldEnabled(false);
    EXPECT_FALSE(slider.isEnabled());
    EXPECT_FALSE(slider.isFieldEnabled());
    slider.setFieldEnabled(true);
    bean.enabled = false;
    rocket.tubeA->fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_FALSE(slider.isEnabled());
}

TEST_F(ValueSliderTest, BoundBindingsOutliveTheSlider)
{
    // The slider's connections go with it (proved under ASan).
    const TwoTubeRocket rocket;
    BeanProperty        bean{.value = 0.01};
    const auto          outer   = std::make_shared<DoubleBinding>(0.04);
    const auto          binding = probeBinding(*rocket.tubeA, bean);
    auto slider = std::make_unique<ValueSlider>(binding, SliderMapping::linear(0, 0.04),
                                                ValueSlider::DynamicBounds{.max = outer}, nullptr);
    slider.reset();
    outer->setValue(0.08);
    bean.value = 0.02;
    rocket.tubeA->fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_EQ(binding->value(), 0.02);
}

}  // namespace

}  // namespace QtRocket::Gui
