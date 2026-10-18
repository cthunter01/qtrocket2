#include "QtRocket/figure/SymmetricComponentShapes.h"

#include <cmath>
#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Transformation.h"
#include "figure/BodyShapePins.h"
#include "figure/ShapePinSupport.h"
#include "figure/ShapePins.h"

namespace
{

using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentShape;
using QtRocket::FigurePath;
using QtRocket::FigureView;
using QtRocket::LaunchLug;
using QtRocket::PathSegment;
using QtRocket::Transformation;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::Test::handBuiltPin;
using QtRocket::Test::HandBuiltPin;
using QtRocket::Test::onlyPathOf;
using QtRocket::Test::pinnedTransformation;
using QtRocket::Test::shapesMatchPins;
using QtRocket::Test::BodyShapePins::kHandBuilt;

namespace SymmetricComponentShapes = QtRocket::SymmetricComponentShapes;

TEST(SymmetricComponentShapes, TheConstantsAreJavas)
{
    EXPECT_EQ(SymmetricComponentShapes::kMinPoints, 91);
    EXPECT_EQ(SymmetricComponentShapes::kEndOffset, 0.0000001);
    EXPECT_EQ(SymmetricComponentShapes::kMinimumRefinedLength, 0.001);
    // Java's Math.cos(7.0 * Math.PI / 180.0), 0x1.fc2f025a23e8bp-1; the C library may differ in the
    // last bit.
    EXPECT_NEAR(SymmetricComponentShapes::kAcceptableAngle,
                std::cos(7.0 * std::numbers::pi / 180.0), 2e-16);
}

TEST(SymmetricComponentShapes, AnOgiveTransitionAsJava)
{
    // Java's TransitionShapes hands every shape but the conical one to SymmetricComponentShapes;
    // without shoulders the outline is all it returns.
    Transition ogive;
    ogive.setShapeType(TransitionShape::OGIVE);
    ogive.setShapeParameter(0.6);
    ogive.setLength(0.08);
    ogive.setForeRadius(0.005);
    ogive.setAftRadius(0.02);
    const HandBuiltPin& pin = handBuiltPin(kHandBuilt, "transition-ogive", FigureView::SIDE);
    EXPECT_TRUE(shapesMatchPins(
        SymmetricComponentShapes::getShapesSide(ogive, pinnedTransformation(pin.transform)),
        pin.shapes, ogive));
}

TEST(SymmetricComponentShapes, TheOutlineRunsForwardAlongTheTopAndBackAlongTheBottom)
{
    // A straight profile needs no refinement: 91 samples, so a move, 90 lines forward along the
    // top, 91 back along the bottom, one up to the start and a close.
    BodyTube tube;
    tube.setLength(0.09);
    tube.setOuterRadius(0.01);
    const Transformation              translation = Transformation::translation(0.2, 0.003, 0);
    const std::vector<ComponentShape> shapes =
        SymmetricComponentShapes::getShapesSide(tube, translation);
    const FigurePath&               path = onlyPathOf(shapes);
    const std::vector<PathSegment>& s    = path.segments();
    EXPECT_EQ(path.precision(), FigurePath::Precision::DOUBLE);
    ASSERT_EQ(s.size(), 184U);
    EXPECT_EQ(s.front(), PathSegment::moveTo({0.2 + (0.09 - 0.0000001), 0.003 + 0.01}));
    EXPECT_EQ(s[90], PathSegment::lineTo({0.2 + 0.0000001, 0.003 + 0.01}));
    EXPECT_EQ(s[91], PathSegment::lineTo({0.2 + 0.0000001, 0.003 - 0.01}));
    EXPECT_EQ(s[45], PathSegment::lineTo({0.2 + (0.09 * 45 / 90), 0.003 + 0.01}));
    EXPECT_EQ(s[181], PathSegment::lineTo({0.2 + (0.09 - 0.0000001), 0.003 - 0.01}));
    EXPECT_EQ(s[182], PathSegment::lineTo(s.front().points[0]));
    EXPECT_EQ(s.back(), PathSegment::close());
}

TEST(SymmetricComponentShapes, TheScaleFactorMultipliesEveryPoint)
{
    BodyTube tube;
    tube.setLength(0.09);
    tube.setOuterRadius(0.01);
    const std::vector<ComponentShape> shapes =
        SymmetricComponentShapes::getShapesSide(tube, Transformation::kIdentity, 10.0);
    const FigurePath& path = onlyPathOf(shapes);
    EXPECT_EQ(path.segments().front(),
              PathSegment::moveTo({(0.09 - 0.0000001) * 10.0, 0.01 * 10.0}));
}

TEST(SymmetricComponentShapes, AnotherComponentIsABug)
{
    // Java's cast to SymmetricComponent throws a ClassCastException.
    const LaunchLug lug;
    EXPECT_THROW((void)SymmetricComponentShapes::getShapesSide(lug, Transformation::kIdentity),
                 BugError);
}

}  // namespace
