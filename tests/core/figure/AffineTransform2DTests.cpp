#include "QtRocket/figure/AffineTransform2D.h"

#include <cmath>
#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/FigureArc.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/figure/WindingRule.h"
#include "QtRocket/util/Geometry2D.h"
#include "figure/FigureTestSupport.h"

namespace
{

using QtRocket::AffineTransform2D;
using QtRocket::FigureArc;
using QtRocket::FigurePath;
using QtRocket::FigureRect;
using QtRocket::FigureShape;
using QtRocket::PathSegment;
using QtRocket::Point2D;
using QtRocket::Rectangle2D;
using QtRocket::WindingRule;
using QtRocket::Test::figureValueNear;
using QtRocket::Test::kTranscendentalTolerance;
using QtRocket::Test::rectanglesNear;
using QtRocket::Test::segmentsNear;

// The values below are java.awt.geom.AffineTransform's (JDK 17, the probe ShapeProbe.java of the
// port's scratch directory). Where Java's rotation takes its quadrant shortcuts (a sine of
// exactly 1 or a cosine of exactly 1 or -1) every result is pinned exactly, signed zeros
// included; a general angle's sine and cosine are compared within kTranscendentalTolerance.

/// True when the six terms are Java's within the tolerance.
[[nodiscard]] bool termsNear(const AffineTransform2D& t, double scaleX, double shearY,
                             double shearX, double scaleY, double translateX, double translateY)
{
    constexpr double kTol = kTranscendentalTolerance;
    return figureValueNear(t.scaleX(), scaleX, kTol) && figureValueNear(t.shearY(), shearY, kTol) &&
           figureValueNear(t.shearX(), shearX, kTol) && figureValueNear(t.scaleY(), scaleY, kTol) &&
           figureValueNear(t.translateX(), translateX, kTol) &&
           figureValueNear(t.translateY(), translateY, kTol);
}

TEST(AffineTransform2D, TheDefaultIsTheIdentity)
{
    const AffineTransform2D identity;
    EXPECT_TRUE(identity.isIdentity());
    EXPECT_EQ(identity.transform({0.25, -0.0}), (Point2D{0.25, -0.0}));
    EXPECT_TRUE(std::signbit(identity.transform({0.25, -0.0}).y));
    EXPECT_EQ(identity, AffineTransform2D::rotation(0.0));
}

TEST(AffineTransform2D, AQuarterTurnSwapsTheAxesExactly)
{
    // sin(PI/2) is exactly 1, so Java takes the cosine as 0 and transforms by (-y, x) alone:
    // (0, 0) goes to (-0, 0).
    const AffineTransform2D quarter = AffineTransform2D::rotation(std::numbers::pi / 2);
    EXPECT_EQ(quarter.scaleX(), 0.0);
    EXPECT_EQ(quarter.shearY(), 1.0);
    EXPECT_EQ(quarter.shearX(), -1.0);
    EXPECT_EQ(quarter.scaleY(), 0.0);
    EXPECT_FALSE(quarter.isIdentity());
    EXPECT_EQ(quarter.transform({0.25, -0.0}), (Point2D{0.0, 0.25}));
    const Point2D origin = quarter.transform({0.0, 0.0});
    EXPECT_TRUE(std::signbit(origin.x));
    EXPECT_FALSE(std::signbit(origin.y));

    const AffineTransform2D back = AffineTransform2D::rotation(-std::numbers::pi / 2);
    EXPECT_EQ(back.transform({0.25, -0.0}), (Point2D{-0.0, -0.25}));
    EXPECT_TRUE(std::signbit(back.transform({0.25, -0.0}).x));
    EXPECT_TRUE(std::signbit(back.transform({0.0, 0.0}).y));
}

TEST(AffineTransform2D, AHalfTurnNegatesExactly)
{
    // cos(PI) is exactly -1, so Java takes the sine as 0 and scales by -1 alone.
    const AffineTransform2D half = AffineTransform2D::rotation(std::numbers::pi);
    EXPECT_EQ(half.scaleX(), -1.0);
    EXPECT_EQ(half.shearY(), 0.0);
    EXPECT_EQ(half.scaleY(), -1.0);
    EXPECT_EQ(half.transform({0.25, -0.0}), (Point2D{-0.25, 0.0}));
    EXPECT_FALSE(std::signbit(half.transform({0.25, -0.0}).y));
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({-1.0, -2.0}), PathSegment::lineTo({-4.0, -2.0}),
        PathSegment::lineTo({-4.0, -6.0}), PathSegment::lineTo({-1.0, -6.0}),
        PathSegment::lineTo({-1.0, -2.0}), PathSegment::close(),
    };
    // PINS-END
    const FigurePath rotated = half.createTransformedShape(FigureShape{FigureRect{1, 2, 3, 4}});
    EXPECT_EQ(rotated.segments(), expected);
    EXPECT_EQ(rotated.precision(), FigurePath::Precision::DOUBLE);
    EXPECT_EQ(rotated.bounds(), (Rectangle2D{.x = -4.0, .y = -6.0, .width = 3.0, .height = 4.0}));
    // A full turn: cos(2 PI) is exactly 1, the identity.
    EXPECT_TRUE(AffineTransform2D::rotation(2 * std::numbers::pi).isIdentity());
}

TEST(AffineTransform2D, AGeneralRotationUsesItsSineAndCosine)
{
    // PINS-BEGIN (ShapeProbe.java)
    const AffineTransform2D small = AffineTransform2D::rotation(0.3);
    EXPECT_TRUE(termsNear(small, 0.955336489125606, 0.29552020666133955, -0.29552020666133955,
                          0.955336489125606, 0.0, 0.0));
    const Point2D p = small.transform({0.25, -0.0});
    EXPECT_TRUE(figureValueNear(p.x, 0.2388341222814015, kTranscendentalTolerance));
    EXPECT_TRUE(figureValueNear(p.y, 0.07388005166533489, kTranscendentalTolerance));
    const AffineTransform2D large = AffineTransform2D::rotation(-2.5);
    EXPECT_TRUE(termsNear(large, -0.8011436155469337, -0.5984721441039564, 0.5984721441039564,
                          -0.8011436155469337, 0.0, 0.0));
    // PINS-END
}

TEST(AffineTransform2D, ARotationAboutAnAnchorKeepsTheAnchor)
{
    // PINS-BEGIN (ShapeProbe.java)
    const AffineTransform2D quarter =
        AffineTransform2D::rotation(std::numbers::pi / 2, 0.25, 0.0254);
    EXPECT_EQ(quarter.translateX(), 0.2754);
    EXPECT_EQ(quarter.translateY(), -0.2246);
    EXPECT_EQ(quarter.transform({0.3, 0.02}), (Point2D{0.25539999999999996, 0.0754}));
    EXPECT_EQ(quarter.transform({0.25, 0.0254}),
              (Point2D{0.24999999999999997, 0.025400000000000006}));
    const AffineTransform2D half = AffineTransform2D::rotation(std::numbers::pi, 0.25, 0.0254);
    EXPECT_EQ(half.translateX(), 0.5);
    EXPECT_EQ(half.translateY(), 0.0508);
    EXPECT_EQ(half.transform({0.3, 0.02}), (Point2D{0.2, 0.030799999999999998}));
    const AffineTransform2D general = AffineTransform2D::rotation(0.3, 0.25, 0.0254);
    EXPECT_TRUE(termsNear(general, 0.955336489125606, 0.29552020666133955, -0.29552020666133955,
                          0.955336489125606, 0.01867209096779653, -0.07274559848912528));
    const Point2D p = general.transform({0.3, 0.02});
    EXPECT_TRUE(figureValueNear(p.x, 0.2993626335722515, kTranscendentalTolerance));
    EXPECT_TRUE(figureValueNear(p.y, 0.03501719329178869, kTranscendentalTolerance));
    // PINS-END
    // An anchored identity has no translation.
    EXPECT_TRUE(AffineTransform2D::rotation(0.0, 0.25, 0.0254).isIdentity());
}

TEST(AffineTransform2D, ARotatedArcBecomesADoublePath)
{
    // RailButtonShapes.getShapesBack's screw: an OPEN half arc rotated by -0.7 about the button.
    constexpr double kOuterRadius = 0.0097 / 2;
    const FigureArc  screw{
        0.0 - kOuterRadius,   0.0254 + (0.002 + 0.003 + 0.0015 - 0.001), 0.0097, 2 * 0.001, 0, -180,
        FigureArc::Type::OPEN};
    const FigurePath rotated =
        AffineTransform2D::rotation(-0.7, 0.0, 0.0254).createTransformedShape(FigureShape{screw});
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({0.007896899575374757, 0.02724701843424637}),
        PathSegment::cubicTo({0.008252691179607401, 0.027669429110310822},
                             {0.006880324433195268, 0.02941072712410536},
                             {0.0048316326542826825, 0.031136316404633664}),
        PathSegment::cubicTo({0.002782940875370097, 0.032861905685161966},
                             {8.337219629478629E-4, 0.033918340676516424},
                             {4.7793035871522213E-4, 0.03349593000045197}),
    };
    const Rectangle2D expectedBounds{.x      = 4.7793035871522213E-4,
                                     .y      = 0.02724701843424637,
                                     .width  = 0.007774760820892179,
                                     .height = 0.006671322242270055};
    // PINS-END
    EXPECT_EQ(rotated.precision(), FigurePath::Precision::DOUBLE);
    EXPECT_EQ(rotated.windingRule(), WindingRule::NON_ZERO);
    EXPECT_TRUE(segmentsNear(rotated.segments(), expected));
    EXPECT_TRUE(rectanglesNear(rotated.bounds(), expectedBounds));
}

TEST(AffineTransform2D, ARotatedFloatPathKeepsItsRule)
{
    FigurePath path{FigurePath::Precision::FLOAT, WindingRule::EVEN_ODD};
    path.moveTo(0.1, 0.2);
    path.lineTo(0.3, 0.2);
    path.quadTo(0.4, 0.3, 0.3, 0.4);
    path.closePath();
    const FigurePath rotated = AffineTransform2D::rotation(std::numbers::pi / 2, 0.1, 0.2)
                                   .createTransformedShape(FigureShape{path});
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({0.0999999970197678, 0.20000000149011612}),
        PathSegment::lineTo({0.0999999970197678, 0.40000001192092893}),
        PathSegment::quadTo({-1.1920928910669204E-8, 0.5000000059604645},
                            {-0.10000000596046443, 0.40000001192092893}),
        PathSegment::close(),
    };
    const Rectangle2D expectedBounds{.x      = -0.10000000596046443,
                                     .y      = 0.20000000149011612,
                                     .width  = 0.20000000298023224,
                                     .height = 0.30000000447034836};
    // PINS-END
    EXPECT_EQ(rotated.precision(), FigurePath::Precision::DOUBLE);
    EXPECT_EQ(rotated.windingRule(), WindingRule::EVEN_ODD);
    EXPECT_EQ(rotated.segments(), expected);
    EXPECT_EQ(rotated.bounds(), expectedBounds);
}

}  // namespace
