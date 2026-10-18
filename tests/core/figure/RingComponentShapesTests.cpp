#include "QtRocket/figure/RingComponentShapes.h"

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Transformation.h"
#include "figure/BodyShapePins.h"
#include "figure/ShapePinSupport.h"

namespace
{

using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::Bulkhead;
using QtRocket::CenteringRing;
using QtRocket::ComponentShape;
using QtRocket::EngineBlock;
using QtRocket::FigureRect;
using QtRocket::InnerTube;
using QtRocket::RingComponent;
using QtRocket::Transformation;
using QtRocket::TubeCoupler;
using QtRocket::Test::handBuiltCasesAsJava;
using QtRocket::Test::BodyShapePins::kHandBuilt;

namespace RingComponentShapes = QtRocket::RingComponentShapes;

/// The InnerTube of the probe's innerTube(length, outer, inner).
std::unique_ptr<InnerTube> makeInnerTube(double length, double outer, double inner)
{
    auto tube = std::make_unique<InnerTube>();
    tube->setLength(length);
    tube->setOuterRadius(outer);
    tube->setInnerRadius(inner);
    return tube;
}

/// Success when the shapes of @p ring in the probe's cases "<name>/axial" and "<name>/off" are
/// what RingComponentShapes returned in Java.
::testing::AssertionResult ringAsJava(const RingComponent& ring, const char* name)
{
    return handBuiltCasesAsJava(&RingComponentShapes::getShapesSide,
                                &RingComponentShapes::getShapesBack, ring, kHandBuilt, name);
}

TEST(RingComponentShapes, InnerTubesAsJava)
{
    // A wall of 0.0125 - 0.0113 is 0.0012000000000000014 in doubles (two outlines), one of
    // 0.02 - 0.0188 is 0.0011999999999999997 (the outer only).
    EXPECT_TRUE(ringAsJava(*makeInnerTube(0.075, 0.0125, 0.0113), "ring-inner-tube-0.0113"));
    EXPECT_TRUE(ringAsJava(*makeInnerTube(0.075, 0.0125, 0.0112), "ring-inner-tube-0.0112"));
    EXPECT_TRUE(ringAsJava(*makeInnerTube(0.075, 0.0125, 0.0114), "ring-inner-tube-0.0114"));
    EXPECT_TRUE(ringAsJava(*makeInnerTube(0.075, 0.02, 0.0188), "ring-inner-tube-0.0188"));

    InnerTube thick;
    thick.setLength(0.06);
    thick.setOuterRadius(0.012);
    thick.setThickness(0.0012);
    EXPECT_TRUE(ringAsJava(thick, "ring-inner-tube-thickness"));
}

TEST(RingComponentShapes, BulkheadCenteringRingCouplerAndEngineBlockAsJava)
{
    Bulkhead bulkhead;
    bulkhead.setLength(0.002);
    bulkhead.setOuterRadius(0.02);
    EXPECT_TRUE(ringAsJava(bulkhead, "ring-bulkhead"));

    CenteringRing ring;
    ring.setLength(0.002);
    ring.setOuterRadius(0.03);
    ring.setInnerRadius(0.01);
    EXPECT_TRUE(ringAsJava(ring, "ring-centering-ring"));

    CenteringRing closed;
    closed.setLength(0.003);
    closed.setOuterRadius(0.01);
    closed.setInnerRadius(0.01);
    EXPECT_TRUE(ringAsJava(closed, "ring-centering-ring-closed"));

    TubeCoupler coupler;
    coupler.setLength(0.04);
    coupler.setOuterRadius(0.015);
    coupler.setInnerRadius(0.0145);
    EXPECT_TRUE(ringAsJava(coupler, "ring-tube-coupler"));

    EngineBlock block;
    block.setLength(0.005);
    block.setOuterRadius(0.009);
    block.setThickness(0.003);
    EXPECT_TRUE(ringAsJava(block, "ring-engine-block"));
}

TEST(RingComponentShapes, TheInnerOutlineNeedsAWallOfTheMinimumAndAnOpening)
{
    const Transformation t = Transformation::translation(0.25, 0, 0);
    // The outer outline, then the inner one.
    const std::vector<ComponentShape> both =
        RingComponentShapes::getShapesSide(*makeInnerTube(0.05, 0.02, 0.01), t);
    ASSERT_EQ(both.size(), 2U);
    EXPECT_EQ(both[0].shape(), (QtRocket::FigureShape{FigureRect{0.25, 0 - 0.02, 0.05, 2 * 0.02}}));
    EXPECT_EQ(both[1].shape(), (QtRocket::FigureShape{FigureRect{0.25, 0 - 0.01, 0.05, 2 * 0.01}}));
    EXPECT_EQ(RingComponentShapes::getShapesBack(*makeInnerTube(0.05, 0.02, 0.01), t).size(), 2U);

    // A wall thinner than kMinimumWall: the outer outline only.
    EXPECT_EQ(RingComponentShapes::getShapesSide(*makeInnerTube(0.05, 0.02, 0.0195), t).size(), 1U);
    EXPECT_EQ(RingComponentShapes::getShapesBack(*makeInnerTube(0.05, 0.02, 0.0195), t).size(), 1U);

    // No opening (a bulkhead's inner radius is 0): the outer outline only.
    Bulkhead bulkhead;
    bulkhead.setOuterRadius(0.02);
    EXPECT_EQ(bulkhead.getInnerRadius(), 0.0);
    EXPECT_EQ(RingComponentShapes::getShapesSide(bulkhead, t).size(), 1U);
    EXPECT_EQ(RingComponentShapes::getShapesBack(bulkhead, t).size(), 1U);
    EXPECT_EQ(RingComponentShapes::kMinimumWall, 0.0012);
}

TEST(RingComponentShapes, AnotherComponentIsABug)
{
    // Java's cast to RingComponent throws a ClassCastException.
    const BodyTube tube;
    EXPECT_THROW((void)RingComponentShapes::getShapesSide(tube, Transformation::kIdentity),
                 BugError);
    EXPECT_THROW((void)RingComponentShapes::getShapesBack(tube, Transformation::kIdentity),
                 BugError);
}

}  // namespace
