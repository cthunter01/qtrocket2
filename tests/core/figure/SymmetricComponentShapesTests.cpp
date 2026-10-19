#include "QtRocket/figure/SymmetricComponentShapes.h"

#include <cmath>
#include <format>
#include <limits>
#include <numbers>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/NoseCone.h"
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
using QtRocket::NoseCone;
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

/// The outlines of nose cones of every sampled shape and of @p length that are not one closed
/// path of the 91 unrefined samples (184 segments), as "<shape> <length>: <what>".
[[nodiscard]] std::vector<std::string> unrefinedOutlineProblems(double length)
{
    std::vector<std::string> problems;
    for (const TransitionShape shape :
         {TransitionShape::OGIVE, TransitionShape::ELLIPSOID, TransitionShape::POWER,
          TransitionShape::PARABOLIC, TransitionShape::HAACK})
    {
        const NoseCone                    nose{shape, length, 0.05};
        const std::vector<ComponentShape> shapes =
            SymmetricComponentShapes::getShapesSide(nose, Transformation::kIdentity);
        const std::vector<PathSegment>& s = onlyPathOf(shapes).segments();
        if (s.size() != 184 || s.back() != PathSegment::close())
        {
            problems.push_back(
                std::format("{} {}: {} segments", static_cast<int>(shape), length, s.size()));
        }
    }
    return problems;
}

TEST(SymmetricComponentShapes, AProfileThatIsNotFiniteIsNotRefined)
{
    // Java refines these forever (a deviation, see getShapesSide): with a length of +inf or NaN
    // every sample but the first has a coordinate that is not finite; at 1e300 m the squares of
    // the segments overflow (and the ogive's radius is NaN). No angle has a cosine, so no
    // segment is split.
    EXPECT_EQ(unrefinedOutlineProblems(std::numeric_limits<double>::infinity()),
              std::vector<std::string>{});
    EXPECT_EQ(unrefinedOutlineProblems(std::numeric_limits<double>::quiet_NaN()),
              std::vector<std::string>{});
    EXPECT_EQ(unrefinedOutlineProblems(1e300), std::vector<std::string>{});
    // Finite and large, as Java: nothing to refine.
    EXPECT_EQ(unrefinedOutlineProblems(1e16), std::vector<std::string>{});
}

/// A transition 1e14 m long whose profile steps from 0 to 1e12 m at 5.05e13 m, where neighbouring
/// doubles lie 7.8 mm apart. No transition shape has a corner, but this one keeps a sharp angle
/// at the step however finely it is sampled.
class SteppedProfile : public Transition
{
public:
    static constexpr double kStep = 5.05e13;

    [[nodiscard]] double getRadius(double x) const override { return x < kStep ? 0 : 1e12; }
};

TEST(SymmetricComponentShapes, ASegmentThatCannotBeHalvedIsNotSplit)
{
    // The refinement closes in on the step until two neighbouring doubles straddle it: Java's
    // 1 mm rule never stops it there, and the midpoint of the two is one of them, inserted again
    // and again. The segment is not split instead, and the outline is closed.
    SteppedProfile stepped;
    stepped.setLength(1e14);
    const std::vector<ComponentShape> shapes =
        SymmetricComponentShapes::getShapesSide(stepped, Transformation::kIdentity);
    const std::vector<PathSegment>& s = onlyPathOf(shapes).segments();
    // 91 points inserted on the way to the step (366 segments).
    EXPECT_GT(s.size(), 184U);
    EXPECT_LT(s.size(), 1000U);
    EXPECT_EQ(s.back(), PathSegment::close());
}

TEST(SymmetricComponentShapes, AnotherComponentIsABug)
{
    // Java's cast to SymmetricComponent throws a ClassCastException.
    const LaunchLug lug;
    EXPECT_THROW((void)SymmetricComponentShapes::getShapesSide(lug, Transformation::kIdentity),
                 BugError);
}

}  // namespace
