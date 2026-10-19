// The shape combo box's binding (Java: TransitionShapeModel).
//
// Expectations marked "probe" are OpenRocket's own results, printed by the Java probe of this part
// (scratchpad probes/m2r2-B2/java/src/probe/B2Probe.java -> b2-probe.out, run on OpenRocket
// 5f164fd0e with JDK 17: TransitionShapeModel(transition) of a transition set to OGIVE).

#include "QtRocket/gui/binding/TransitionShapeBinding.h"

#include <array>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "BindingTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using TransitionShapeBindingTest = Test::GuiTest;
using Test::ChangeCounter;
using Test::RocketEventRecorder;
using Test::TwoTubeRocket;

/// A rocket of two tubes with a transition after them.
struct TransitionRocket
{
    TwoTubeRocket design;
    Transition*   transition{&design.stage->addChild(std::make_unique<Transition>())};
};

TEST_F(TransitionShapeBindingTest, EveryShapeAsOpenRocket)
{
    // probe: size=6 selected=OGIVE values=CONICAL OGIVE ELLIPSOID POWER PARABOLIC HAACK;
    // set HAACK: source=HAACK clipped=true parameter=0.0.
    TransitionRocket rocket;
    rocket.transition->setShapeType(TransitionShape::OGIVE);
    const auto shapes =
        makeTransitionShapeBinding(std::make_shared<ComponentSelection>(*rocket.transition));
    EXPECT_EQ(shapes->size(), 6);
    EXPECT_EQ(shapes->value(), TransitionShape::OGIVE);
    EXPECT_EQ(std::vector<TransitionShape>(shapes->displayedValues().begin(),
                                           shapes->displayedValues().end()),
              (std::vector<TransitionShape>{TransitionShape::CONICAL, TransitionShape::OGIVE,
                                            TransitionShape::ELLIPSOID, TransitionShape::POWER,
                                            TransitionShape::PARABOLIC, TransitionShape::HAACK}));
    EXPECT_TRUE(shapes->select(TransitionShape::HAACK));
    EXPECT_EQ(rocket.transition->getShapeType(), TransitionShape::HAACK);
    EXPECT_TRUE(rocket.transition->isClipped());
    EXPECT_EQ(rocket.transition->getShapeParameter(), 0.0);
    EXPECT_EQ(shapes->value(), TransitionShape::HAACK);
}

TEST_F(TransitionShapeBindingTest, AShapeChangedElsewhereIsShown)
{
    // Java: componentChanged() tells the combo when the shape changed.
    TransitionRocket rocket;
    const auto       shapes =
        makeTransitionShapeBinding(std::make_shared<ComponentSelection>(*rocket.transition));
    const ChangeCounter changes(*shapes);
    rocket.transition->setLength(0.2);
    EXPECT_EQ(changes.count(), 0);
    rocket.transition->setShapeType(TransitionShape::POWER);
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(shapes->value(), TransitionShape::POWER);
}

TEST_F(TransitionShapeBindingTest, TheMainsShapeReachesTheOthers)
{
    // Java's TransitionShapeModel calls setShapeType() every time, and the main's setter forwards
    // it to the other selected transitions.
    TransitionRocket rocket;
    NoseCone&        nose = *rocket.design.nose;
    nose.setShapeType(TransitionShape::ELLIPSOID);
    rocket.transition->setShapeType(TransitionShape::OGIVE);
    const std::array<RocketComponent*, 2> others{&nose, rocket.design.tubeA};
    const auto                            shapes = makeTransitionShapeBinding(
        std::make_shared<ComponentSelection>(*rocket.transition, others));
    const RocketEventRecorder events(*rocket.design.rocket);
    EXPECT_TRUE(shapes->select(TransitionShape::OGIVE));
    EXPECT_EQ(nose.getShapeType(), TransitionShape::OGIVE);
    EXPECT_EQ(rocket.transition->getShapeType(), TransitionShape::OGIVE);
    EXPECT_EQ(events.count(), 1U);
}

TEST_F(TransitionShapeBindingTest, RefusesAComponentThatIsNoTransition)
{
    TwoTubeRocket design;
    EXPECT_THROW(static_cast<void>(makeTransitionShapeBinding(
                     std::make_shared<ComponentSelection>(*design.tubeA))),
                 BugError);
    EXPECT_THROW(static_cast<void>(makeTransitionShapeBinding(nullptr)), BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
