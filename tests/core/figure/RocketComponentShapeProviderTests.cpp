#include "QtRocket/figure/RocketComponentShapeProvider.h"

#include <cstddef>
#include <memory>
#include <set>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/BodyTubeShapes.h"
#include "QtRocket/figure/ComponentAssemblyShapes.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/FinSetShapes.h"
#include "QtRocket/figure/LaunchLugShapes.h"
#include "QtRocket/figure/MassComponentShapes.h"
#include "QtRocket/figure/ParachuteShapes.h"
#include "QtRocket/figure/RailButtonShapes.h"
#include "QtRocket/figure/RingComponentShapes.h"
#include "QtRocket/figure/ShockCordShapes.h"
#include "QtRocket/figure/StreamerShapes.h"
#include "QtRocket/figure/TransitionShapes.h"
#include "QtRocket/figure/TubeFinSetShapes.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Transformation.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::Bulkhead;
using QtRocket::CenteringRing;
using QtRocket::ComponentKind;
using QtRocket::ComponentShape;
using QtRocket::EllipticalFinSet;
using QtRocket::EngineBlock;
using QtRocket::FigureView;
using QtRocket::FreeformFinSet;
using QtRocket::InnerTube;
using QtRocket::LaunchLug;
using QtRocket::MassComponent;
using QtRocket::NoseCone;
using QtRocket::Parachute;
using QtRocket::ParallelStage;
using QtRocket::PodSet;
using QtRocket::RailButton;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::ShockCord;
using QtRocket::Streamer;
using QtRocket::Transformation;
using QtRocket::Transition;
using QtRocket::TrapezoidFinSet;
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

/// A rocket with a component of every kind: a stage with a nose cone, a transition and a body
/// tube; on the body tube every fin set, a launch lug, a rail button, every inner component, a
/// pod set and a booster set (each with a body tube of its own).
struct RocketOfEveryKind
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    BodyTube*               body{nullptr};

    RocketOfEveryKind()
    {
        auto& stage = rocket->addChild(std::make_unique<AxialStage>());
        stage.addChild(std::make_unique<NoseCone>());
        stage.addChild(std::make_unique<Transition>());
        body = &stage.addChild(std::make_unique<BodyTube>());
        body->setLength(0.3);
        body->setOuterRadius(0.02);
        body->addChild(std::make_unique<TrapezoidFinSet>());
        body->addChild(std::make_unique<EllipticalFinSet>());
        body->addChild(std::make_unique<FreeformFinSet>());
        body->addChild(std::make_unique<TubeFinSet>());
        body->addChild(std::make_unique<LaunchLug>());
        body->addChild(std::make_unique<RailButton>());
        body->addChild(std::make_unique<InnerTube>());
        body->addChild(std::make_unique<TubeCoupler>());
        body->addChild(std::make_unique<EngineBlock>());
        body->addChild(std::make_unique<CenteringRing>());
        body->addChild(std::make_unique<Bulkhead>());
        body->addChild(std::make_unique<MassComponent>());
        body->addChild(std::make_unique<ShockCord>());
        body->addChild(std::make_unique<Parachute>());
        body->addChild(std::make_unique<Streamer>());
        auto& pods = body->addChild(std::make_unique<PodSet>());
        pods.addChild(std::make_unique<BodyTube>());
        auto& boosters = body->addChild(std::make_unique<ParallelStage>());
        boosters.addChild(std::make_unique<BodyTube>());
        rocket->enableEvents();
    }
};

TEST(RocketComponentShapeProvider, EveryKindHasAProviderInBothViews)
{
    // The observable part of Swing's RocketComponentShapeProviderTest
    // (resolvesBuiltInProviderIndependentlyOfThreadContextClassLoader, which tests ServiceLoader
    // mechanics and is not ported): every component finds its provider; only a plain stage has
    // no shapes.
    const RocketOfEveryKind built;
    std::set<ComponentKind> kinds;
    const Transformation    t = offAxis();
    for (const RocketComponent& component : built.rocket->subtree(true))
    {
        kinds.insert(component.kind());
        const std::vector<ComponentShape> side = Provider::getShapesSide(component, t);
        const std::vector<ComponentShape> back = Provider::getShapesBack(component, t);
        const bool plainStage                  = component.kind() == ComponentKind::AXIAL_STAGE;
        EXPECT_EQ(side.empty(), plainStage) << component.toString();
        EXPECT_EQ(back.empty(), plainStage) << component.toString();
    }
    EXPECT_EQ(kinds.size(), QtRocket::kAllComponentKinds.size());

    // Java's test: a trapezoid fin set's side shapes at the identity.
    const TrapezoidFinSet fins;
    EXPECT_EQ(Provider::getShapesSide(fins, Transformation::kIdentity).size(), 3U);
}

/// The first child of @p body of @p kind; a BugError when there is none.
[[nodiscard]] const RocketComponent& childOfKind(const BodyTube& body, ComponentKind kind)
{
    for (const RocketComponent* component : body.getChildren())
    {
        if (component->kind() == kind)
        {
            return *component;
        }
    }
    QtRocket::bug("no child of that kind");
}

TEST(RocketComponentShapeProvider, FinSetsGoToFinSetShapes)
{
    // Their nearest superclass with a service is FinSet.
    const RocketOfEveryKind built;
    const BodyTube&         body = *built.body;
    for (const ComponentKind kind :
         {ComponentKind::TRAPEZOID_FIN_SET, ComponentKind::ELLIPTICAL_FIN_SET,
          ComponentKind::FREEFORM_FIN_SET})
    {
        EXPECT_TRUE(dispatchesTo(childOfKind(body, kind), &QtRocket::FinSetShapes::getShapesSide,
                                 &QtRocket::FinSetShapes::getShapesBack));
    }
}

TEST(RocketComponentShapeProvider, MassObjectsAndRailButtonsGoToTheirProviders)
{
    const RocketOfEveryKind built;
    const BodyTube&         body = *built.body;
    EXPECT_TRUE(dispatchesTo(childOfKind(body, ComponentKind::MASS_COMPONENT),
                             &QtRocket::MassComponentShapes::getShapesSide,
                             &QtRocket::MassComponentShapes::getShapesBack));
    EXPECT_TRUE(dispatchesTo(childOfKind(body, ComponentKind::SHOCK_CORD),
                             &QtRocket::ShockCordShapes::getShapesSide,
                             &QtRocket::ShockCordShapes::getShapesBack));
    EXPECT_TRUE(dispatchesTo(childOfKind(body, ComponentKind::PARACHUTE),
                             &QtRocket::ParachuteShapes::getShapesSide,
                             &QtRocket::ParachuteShapes::getShapesBack));
    EXPECT_TRUE(dispatchesTo(childOfKind(body, ComponentKind::STREAMER),
                             &QtRocket::StreamerShapes::getShapesSide,
                             &QtRocket::StreamerShapes::getShapesBack));
    EXPECT_TRUE(dispatchesTo(childOfKind(body, ComponentKind::RAIL_BUTTON),
                             &QtRocket::RailButtonShapes::getShapesSide,
                             &QtRocket::RailButtonShapes::getShapesBack));
}

TEST(RocketComponentShapeProvider, AssembliesGoToComponentAssemblyShapes)
{
    // PodSetShapes and ParallelStageShapes are ComponentAssemblyShapes; the Rocket's nearest
    // service is ComponentAssemblyShapes too, which RocketFigure never asks.
    const RocketOfEveryKind built;
    const BodyTube&         body = *built.body;
    EXPECT_TRUE(dispatchesTo(childOfKind(body, ComponentKind::POD_SET),
                             &QtRocket::ComponentAssemblyShapes::getShapesSide,
                             &QtRocket::ComponentAssemblyShapes::getShapesBack));
    EXPECT_TRUE(dispatchesTo(childOfKind(body, ComponentKind::PARALLEL_STAGE),
                             &QtRocket::ComponentAssemblyShapes::getShapesSide,
                             &QtRocket::ComponentAssemblyShapes::getShapesBack));
    const Transformation t = offAxis();
    EXPECT_TRUE(sameShapes(Provider::getShapesSide(*built.rocket, t),
                           QtRocket::ComponentAssemblyShapes::getShapesSide(*built.rocket, t)));
    EXPECT_FALSE(Provider::getShapesSide(*built.rocket, t).empty());
}

}  // namespace
