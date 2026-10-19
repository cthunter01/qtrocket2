#include "QtRocket/figure/ShockCordShapes.h"

#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Transformation.h"
#include "figure/FigureTestSupport.h"
#include "figure/FinMassShapePins.h"
#include "figure/ShapePinSupport.h"

namespace
{

using QtRocket::BugError;
using QtRocket::ComponentShape;
using QtRocket::FigureEllipse;
using QtRocket::FigurePath;
using QtRocket::FigureShape;
using QtRocket::Parachute;
using QtRocket::PathSegment;
using QtRocket::ShockCord;
using QtRocket::Transformation;
using QtRocket::Test::handBuiltVariantsAsJava;
using QtRocket::Test::kTranscendentalTolerance;
using QtRocket::Test::pinnedSegmentTypes;
using QtRocket::Test::FinMassShapePins::kHandBuilt;

namespace ShockCordShapes = QtRocket::ShockCordShapes;

/// The probe's shock cord: 0.03 long, radius 0.008, 5 mm off the axis at 2 rad.
[[nodiscard]] ShockCord probeCord()
{
    ShockCord cord;
    cord.setLength(0.03);
    cord.setRadius(0.008);
    cord.setRadialPosition(0.005);
    cord.setRadialDirection(2.0);
    return cord;
}

TEST(ShockCordShapes, AsJava)
{
    EXPECT_TRUE(handBuiltVariantsAsJava(&ShockCordShapes::getShapesSide,
                                        &ShockCordShapes::getShapesBack, probeCord(), kHandBuilt,
                                        "shock-cord", kTranscendentalTolerance));
}

TEST(ShockCordShapes, TheCordIsEightCurvesAcrossTheMiddle)
{
    ShockCord cord;
    cord.setLength(0.04);
    cord.setRadius(0.01);
    const std::vector<ComponentShape> side =
        ShockCordShapes::getShapesSide(cord, Transformation::kIdentity);
    ASSERT_EQ(side.size(), 2U);
    const auto* path = side[1].shape().getIf<FigurePath>();
    ASSERT_TRUE(path != nullptr);
    std::vector<PathSegment::Type> types;
    for (const PathSegment& segment : path->segments())
    {
        types.push_back(segment.type);
    }
    EXPECT_EQ(types, pinnedSegmentTypes("MC8"));
    // From a quarter of the length to three quarters, at the centre line.
    EXPECT_EQ(path->segments().front().points[0], (QtRocket::Point2D{0.01, 0}));
    EXPECT_EQ(path->segments().back().points[2], (QtRocket::Point2D{0.03, 0}));

    const std::vector<ComponentShape> back =
        ShockCordShapes::getShapesBack(cord, Transformation::kIdentity);
    ASSERT_EQ(back.size(), 1U);
    EXPECT_EQ(back[0].shape(), (FigureShape{FigureEllipse{-0.01, -0.01, 0.02, 0.02}}));
}

TEST(ShockCordShapes, OnlyAShockCordWillDo)
{
    // Java casts to ShockCord.
    const Parachute chute;
    EXPECT_THROW((void)ShockCordShapes::getShapesSide(chute, Transformation::kIdentity), BugError);
    EXPECT_THROW((void)ShockCordShapes::getShapesBack(chute, Transformation::kIdentity), BugError);
}

}  // namespace
