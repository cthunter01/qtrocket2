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
    TransitionRocket const rocket;
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
    TransitionRocket const rocket;
    const auto             shapes =
        makeTransitionShapeBinding(std::make_shared<ComponentSelection>(*rocket.transition));
    const ChangeCounter changes(*shapes);
    rocket.transition->setLength(0.2);
    EXPECT_EQ(changes.count(), 0);
    rocket.transition->setShapeType(TransitionShape::POWER);
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(shapes->value(), TransitionShape::POWER);
}

TEST_F(TransitionShapeBindingTest, PickingTheMainsShapeLeavesTheOthersAlone)
{
    // Java's dialogs show the model in a plain JComboBox, which never calls the model for the
    // shape it shows (review probe ShapeComboProbe.java, OpenRocket 5f164fd0e, JDK 17: main A
    // CONICAL, other B HAACK with the parameter 0.333 and clipped; A's own CONICAL picked: B stays
    // HAACK 0.333, events 0; OGIVE picked: B OGIVE; B set to POWER, the shown OGIVE picked again:
    // B stays POWER).
    TransitionRocket const rocket;
    Transition&            main  = *rocket.transition;
    Transition&            other = rocket.design.stage->addChild(std::make_unique<Transition>());
    main.setShapeType(TransitionShape::CONICAL);
    other.setShapeType(TransitionShape::HAACK);
    other.setShapeParameter(0.333);
    other.setClipped(true);
    const std::array<RocketComponent*, 2> others{&other, rocket.design.tubeA};
    const auto                            shapes =
        makeTransitionShapeBinding(std::make_shared<ComponentSelection>(main, others));
    // Deviation: Java's model gives every other transition the main's shape when it is made.
    EXPECT_EQ(other.getShapeType(), TransitionShape::HAACK);
    EXPECT_EQ(other.getShapeParameter(), 0.333);
    EXPECT_TRUE(other.isClipped());

    const RocketEventRecorder events(*rocket.design.rocket);
    EXPECT_FALSE(shapes->select(TransitionShape::CONICAL));
    EXPECT_EQ(other.getShapeType(), TransitionShape::HAACK);
    EXPECT_EQ(other.getShapeParameter(), 0.333);
    EXPECT_TRUE(other.isClipped());
    EXPECT_EQ(events.count(), 0U);

    EXPECT_TRUE(shapes->select(TransitionShape::OGIVE));
    EXPECT_EQ(main.getShapeType(), TransitionShape::OGIVE);
    EXPECT_EQ(other.getShapeType(), TransitionShape::OGIVE);
    EXPECT_EQ(events.count(), 1U);

    other.setShapeType(TransitionShape::POWER);
    EXPECT_FALSE(shapes->select(TransitionShape::OGIVE));
    EXPECT_EQ(other.getShapeType(), TransitionShape::POWER);
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
