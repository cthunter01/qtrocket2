#include "QtRocket/figure/FinSetShapes.h"

#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Transformation.h"
#include "figure/FigureTestSupport.h"
#include "figure/FinMassShapePins.h"
#include "figure/ShapePinSupport.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentShape;
using QtRocket::Coordinate;
using QtRocket::EllipticalFinSet;
using QtRocket::FigurePath;
using QtRocket::FinSet;
using QtRocket::FreeformFinSet;
using QtRocket::PathSegment;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Transformation;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::TrapezoidFinSet;
using QtRocket::Test::handBuiltVariantsAsJava;
using QtRocket::Test::kFloatTolerance;
using QtRocket::Test::FinMassShapePins::kHandBuilt;

namespace FinSetShapes = QtRocket::FinSetShapes;

/// A fin set the probe builds, with the rocket that holds it.
template <class Fins>
struct FinsOnRocket
{
    std::unique_ptr<Rocket> rocket;
    Fins*                   fins;
};

/// A rocket with a stage and @p parent, to which @p fins are added (the probe's bodyOfNewRocket()
/// and transitionOfNewRocket()).
template <class Fins>
[[nodiscard]] FinsOnRocket<Fins> addedTo(std::unique_ptr<RocketComponent> parent,
                                         std::unique_ptr<Fins>            fins)
{
    auto  rocket = std::make_unique<Rocket>();
    auto& stage  = rocket->addChild(std::make_unique<AxialStage>());
    auto& mount  = stage.addChild(std::move(parent));
    Fins& added  = mount.addChild(std::move(fins));
    return FinsOnRocket<Fins>{.rocket = std::move(rocket), .fins = &added};
}

/// The probe's body tube: length 0.3, radius 0.02.
[[nodiscard]] std::unique_ptr<RocketComponent> probeBody()
{
    auto body = std::make_unique<BodyTube>();
    body->setLength(0.3);
    body->setOuterRadius(0.02);
    return body;
}

/// The probe's conical transition: length 0.1, radii 0.02 and 0.03.
[[nodiscard]] std::unique_ptr<RocketComponent> probeTransition()
{
    auto transition = std::make_unique<Transition>();
    transition->setShapeType(TransitionShape::CONICAL);
    transition->setLength(0.1);
    transition->setForeRadius(0.02);
    transition->setAftRadius(0.03);
    return transition;
}

/// The probe's trapezoid(): three fins, root 0.08, tip 0.04, sweep 0.03, height 0.05, 3 mm thick.
[[nodiscard]] std::unique_ptr<TrapezoidFinSet> trapezoid()
{
    auto fins = std::make_unique<TrapezoidFinSet>();
    fins->setFinCount(3);
    fins->setRootChord(0.08);
    fins->setTipChord(0.04);
    fins->setSweep(0.03);
    fins->setHeight(0.05);
    fins->setThickness(0.003);
    return fins;
}

/// The probe's tab: 0.01 high, 0.04 long, offset 0.01.
void addTab(FinSet& fins)
{
    fins.setTabHeight(0.01);
    fins.setTabLength(0.04);
    fins.setTabOffset(0.01);
}

/// A freeform fin set of @p points, 3 mm thick, on the probe's transition (the points are set after
/// the fins are added, as the probe does).
[[nodiscard]] FinsOnRocket<FreeformFinSet> freeformOnTransition(
    const std::vector<Coordinate>& points)
{
    auto onCone = addedTo(probeTransition(), std::make_unique<FreeformFinSet>());
    onCone.fins->setPoints(points);
    onCone.fins->setThickness(0.003);
    return onCone;
}

/// Success when the hand-built case @p name of @p fins is as Java's: exactly, or for canted and
/// elliptical fins, whose points went through sin and cos before the float rounding, within a
/// float's last place.
[[nodiscard]] ::testing::AssertionResult finsAsJava(const FinSet& fins, const char* name,
                                                    double tolerance = 0)
{
    return handBuiltVariantsAsJava(&FinSetShapes::getShapesSide, &FinSetShapes::getShapesBack, fins,
                                   kHandBuilt, name, tolerance);
}

TEST(FinSetShapes, TrapezoidAsJava)
{
    const auto plain = addedTo(probeBody(), trapezoid());
    plain.rocket->enableEvents();
    EXPECT_TRUE(finsAsJava(*plain.fins, "fin-trapezoid"));

    const auto tabbed = addedTo(probeBody(), trapezoid());
    addTab(*tabbed.fins);
    tabbed.rocket->enableEvents();
    EXPECT_TRUE(finsAsJava(*tabbed.fins, "fin-trapezoid-tab"));
}

TEST(FinSetShapes, CantedAsJava)
{
    const auto canted = addedTo(probeBody(), trapezoid());
    canted.fins->setCantAngle(0.1);
    addTab(*canted.fins);
    canted.rocket->enableEvents();
    EXPECT_TRUE(finsAsJava(*canted.fins, "fin-canted-tab", kFloatTolerance));

    const auto negative = addedTo(probeBody(), trapezoid());
    negative.fins->setCantAngle(-0.05);
    negative.rocket->enableEvents();
    EXPECT_TRUE(finsAsJava(*negative.fins, "fin-canted-negative", kFloatTolerance));

    // At most 0.5 mm thick: the side panels only.
    const auto thin = addedTo(probeBody(), trapezoid());
    thin.fins->setThickness(0.0004);
    thin.fins->setCantAngle(0.05);
    thin.fins->setTabHeight(0.008);
    thin.fins->setTabLength(0.03);
    thin.rocket->enableEvents();
    EXPECT_TRUE(finsAsJava(*thin.fins, "fin-canted-thin", kFloatTolerance));
}

TEST(FinSetShapes, EllipticalAsJava)
{
    const auto elliptical = addedTo(probeBody(), std::make_unique<EllipticalFinSet>());
    elliptical.fins->setLength(0.07);
    elliptical.fins->setHeight(0.04);
    elliptical.fins->setThickness(0.002);
    elliptical.rocket->enableEvents();
    EXPECT_TRUE(finsAsJava(*elliptical.fins, "fin-elliptical", kFloatTolerance));
}

TEST(FinSetShapes, FreeformOnATransitionAsJava)
{
    // The root follows the cone, so the lowest fin point is not at y = 0 and the uncanted back
    // view moves up by it.
    const auto onCone = freeformOnTransition(
        {Coordinate{0, 0}, Coordinate{0.02, 0.05}, Coordinate{0.06, 0.05}, Coordinate{0.08, 0}});
    onCone.rocket->enableEvents();
    EXPECT_TRUE(finsAsJava(*onCone.fins, "fin-freeform-on-transition"));

    const std::vector<Coordinate> outline{Coordinate{0, 0}, Coordinate{0.03, 0.04},
                                          Coordinate{0.07, 0.04}, Coordinate{0.09, 0}};
    const auto                    tabbed = freeformOnTransition(outline);
    addTab(*tabbed.fins);
    tabbed.rocket->enableEvents();
    EXPECT_TRUE(finsAsJava(*tabbed.fins, "fin-freeform-tab-on-transition"));

    const auto canted = freeformOnTransition(outline);
    canted.fins->setCantAngle(0.08);
    addTab(*canted.fins);
    canted.rocket->enableEvents();
    EXPECT_TRUE(finsAsJava(*canted.fins, "fin-canted-tab-on-transition", kFloatTolerance));
}

TEST(FinSetShapes, TheSideViewIsThreeOpenFloatPolylines)
{
    const auto plain = addedTo(probeBody(), trapezoid());
    plain.rocket->enableEvents();
    const FinSet& fins = *plain.fins;

    const std::vector<ComponentShape> side =
        FinSetShapes::getShapesSide(fins, Transformation::kIdentity);
    ASSERT_EQ(side.size(), 3U);
    const auto* outline = side[0].shape().getIf<FigurePath>();
    const auto* tab     = side[1].shape().getIf<FigurePath>();
    const auto* root    = side[2].shape().getIf<FigurePath>();
    ASSERT_TRUE(outline != nullptr && tab != nullptr && root != nullptr);
    EXPECT_EQ(outline->precision(), FigurePath::Precision::FLOAT);
    EXPECT_EQ(outline->segments().size(), fins.getFinPoints().size());
    EXPECT_EQ(outline->segments().front().type, PathSegment::Type::MOVE_TO);
    EXPECT_NE(outline->segments().back().type, PathSegment::Type::CLOSE);
    // Without a tab the tab's path is empty.
    EXPECT_TRUE(tab->isEmpty());
    EXPECT_EQ(root->segments().size(), fins.getRootPoints().size());
}

TEST(FinSetShapes, AnotherComponentIsABug)
{
    // Java's cast to FinSet throws a ClassCastException.
    const BodyTube tube;
    EXPECT_THROW((void)FinSetShapes::getShapesSide(tube, Transformation::kIdentity), BugError);
    EXPECT_THROW((void)FinSetShapes::getShapesBack(tube, Transformation::kIdentity), BugError);
}

}  // namespace
