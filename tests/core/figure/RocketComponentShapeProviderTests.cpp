#include "QtRocket/figure/RocketComponentShapeProvider.h"

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/BodyTubeShapes.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/LaunchLugShapes.h"
#include "QtRocket/figure/RingComponentShapes.h"
#include "QtRocket/figure/TransitionShapes.h"
#include "QtRocket/figure/TubeFinSetShapes.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/util/Transformation.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::Bulkhead;
using QtRocket::CenteringRing;
using QtRocket::ComponentShape;
using QtRocket::EngineBlock;
using QtRocket::FigureView;
using QtRocket::InnerTube;
using QtRocket::LaunchLug;
using QtRocket::NoseCone;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Transformation;
using QtRocket::Transition;
using QtRocket::TubeCoupler;
using QtRocket::TubeFinSet;

namespace Provider = QtRocket::RocketComponentShapeProvider;

/// A provider's getShapesSide or getShapesBack.
using ShapeFunction = std::vector<ComponentShape> (*)(const RocketComponent&,
                                                      const Transformation&);

/// A transformation off the axis and rotated, as an instance of a pod in a rotated figure.
Transformation offAxis()
{
    return Transformation::rotateX(0.7).applyTransformation(
        Transformation::translation(0.1, 0.02, -0.03));
}

/// Whether two lists hold the same shapes of the same component, in order.
bool sameShapes(const std::vector<ComponentShape>& a, const std::vector<ComponentShape>& b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (a[i].shape() != b[i].shape() || &a[i].component() != &b[i].component())
        {
            return false;
        }
    }
    return true;
}

/// Success when the dispatch gives for @p component in every view what @p side and @p back,
/// the functions of its provider, give.
::testing::AssertionResult dispatchesTo(const RocketComponent& component, ShapeFunction side,
                                        ShapeFunction back)
{
    const Transformation t = offAxis();
    if (!sameShapes(Provider::getShapesSide(component, t), side(component, t)) ||
        !sameShapes(Provider::getShapesBack(component, t), back(component, t)))
    {
        return ::testing::AssertionFailure() << component.toString() << " goes elsewhere";
    }
    for (const FigureView view : QtRocket::kAllFigureViews)
    {
        const std::vector<ComponentShape> expected =
            QtRocket::usesSideShapes(view) ? side(component, t) : back(component, t);
        if (!sameShapes(Provider::getShapes(view, component, t), expected))
        {
            return ::testing::AssertionFailure()
                   << component.toString() << " has other shapes in the "
                   << QtRocket::figureViewName(view) << " view";
        }
    }
    return ::testing::AssertionSuccess();
}

/// TransitionShapes::getShapesSide() with the figure's scale factor.
std::vector<ComponentShape> transitionSide(const RocketComponent& component,
                                           const Transformation&  transformation)
{
    return QtRocket::TransitionShapes::getShapesSide(component, transformation);
}

TEST(RocketComponentShapeProvider, TheRocketAndAPlainStageHaveNoShapesInAnyView)
{
    // RocketFigure.addThisShape skips the Rocket and a plain AxialStage, and
    // ComponentAssemblyShapes returns no shapes for a plain stage.
    const Rocket     rocket;
    const AxialStage stage;
    for (const FigureView view : QtRocket::kAllFigureViews)
    {
        EXPECT_TRUE(Provider::getShapes(view, rocket, Transformation::kIdentity).empty());
        EXPECT_TRUE(Provider::getShapes(view, stage, Transformation::kIdentity).empty());
    }
    EXPECT_TRUE(Provider::getShapesSide(stage, Transformation::kIdentity).empty());
    EXPECT_TRUE(Provider::getShapesBack(stage, Transformation::kIdentity).empty());
}

TEST(RocketComponentShapeProvider, BodyComponentsGoToTheirProviders)
{
    // Java's ServiceLoader picks the service of the class or of its nearest superclass: BodyTube,
    // Transition (also for NoseCone), TubeFinSet and LaunchLug have their own.
    BodyTube tube;
    tube.setOuterRadius(0.02);
    EXPECT_TRUE(dispatchesTo(tube, &QtRocket::BodyTubeShapes::getShapesSide,
                             &QtRocket::BodyTubeShapes::getShapesBack));
    const Transition transition;
    EXPECT_TRUE(
        dispatchesTo(transition, &transitionSide, &QtRocket::TransitionShapes::getShapesBack));
    const NoseCone nose;
    EXPECT_TRUE(dispatchesTo(nose, &transitionSide, &QtRocket::TransitionShapes::getShapesBack));
    TubeFinSet fins;
    fins.setOuterRadius(0.01);
    EXPECT_TRUE(dispatchesTo(fins, &QtRocket::TubeFinSetShapes::getShapesSide,
                             &QtRocket::TubeFinSetShapes::getShapesBack));
    LaunchLug lug;
    lug.setOuterRadius(0.003);
    EXPECT_TRUE(dispatchesTo(lug, &QtRocket::LaunchLugShapes::getShapesSide,
                             &QtRocket::LaunchLugShapes::getShapesBack));
}

TEST(RocketComponentShapeProvider, RingComponentsGoToRingComponentShapes)
{
    // Their nearest superclass with a service is RingComponent.
    InnerTube inner;
    inner.setOuterRadius(0.01);
    EXPECT_TRUE(dispatchesTo(inner, &QtRocket::RingComponentShapes::getShapesSide,
                             &QtRocket::RingComponentShapes::getShapesBack));
    TubeCoupler coupler;
    coupler.setOuterRadius(0.015);
    EXPECT_TRUE(dispatchesTo(coupler, &QtRocket::RingComponentShapes::getShapesSide,
                             &QtRocket::RingComponentShapes::getShapesBack));
    EngineBlock block;
    block.setOuterRadius(0.009);
    EXPECT_TRUE(dispatchesTo(block, &QtRocket::RingComponentShapes::getShapesSide,
                             &QtRocket::RingComponentShapes::getShapesBack));
    CenteringRing ring;
    ring.setOuterRadius(0.03);
    EXPECT_TRUE(dispatchesTo(ring, &QtRocket::RingComponentShapes::getShapesSide,
                             &QtRocket::RingComponentShapes::getShapesBack));
    Bulkhead bulkhead;
    bulkhead.setOuterRadius(0.02);
    EXPECT_TRUE(dispatchesTo(bulkhead, &QtRocket::RingComponentShapes::getShapesSide,
                             &QtRocket::RingComponentShapes::getShapesBack));
}

}  // namespace
