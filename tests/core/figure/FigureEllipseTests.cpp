#include "QtRocket/figure/FigureEllipse.h"

#include <array>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/util/Geometry2D.h"
#include "figure/FigureTestSupport.h"

namespace
{

using QtRocket::FigureEllipse;
using QtRocket::FigureShape;
using QtRocket::PathSegment;
using QtRocket::Rectangle2D;
using QtRocket::Test::containsAsJava;
using QtRocket::Test::ContainsCase;

// Every value below was printed by java.awt.geom (JDK 17, the probe ShapeProbe.java of the port's
// scratch directory) for the same ellipses: Ellipse2D.Double's PathIterator (EllipseIterator),
// getBounds2D and contains. The iterator only multiplies and adds Java's constants, so the
// segments are pinned exactly.

TEST(FigureEllipse, SegmentsOfACircleFollowEllipseIterator)
{
    // A body tube's back view: Ellipse2D.Double(z - R, y - R, 2R, 2R) with R = 12.5 mm.
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({0.0125, 0.0}),
        PathSegment::cubicTo({0.0125, 0.0069035593728849175}, {0.0069035593728849175, 0.0125},
                             {0.0, 0.0125}),
        PathSegment::cubicTo({-0.0069035593728849175, 0.0125}, {-0.0125, 0.0069035593728849175},
                             {-0.0125, 0.0}),
        PathSegment::cubicTo({-0.0125, -0.0069035593728849175}, {-0.0069035593728849175, -0.0125},
                             {0.0, -0.0125}),
        PathSegment::cubicTo({0.0069035593728849175, -0.0125}, {0.0125, -0.0069035593728849175},
                             {0.0125, 0.0}),
        PathSegment::close(),
    };
    // PINS-END
    const FigureEllipse circle{-0.0125, -0.0125, 0.025, 0.025};
    EXPECT_EQ(circle.segments(), expected);
    EXPECT_EQ(circle.bounds(),
              (Rectangle2D{.x = -0.0125, .y = -0.0125, .width = 0.025, .height = 0.025}));
}

TEST(FigureEllipse, SegmentsOfAnEllipseFollowEllipseIterator)
{
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({0.37, 0.11}),
        PathSegment::cubicTo({0.37, 0.11552284749830793},
                             {0.35432996624407775, 0.12000000000000001},
                             {0.33499999999999996, 0.12000000000000001}),
        PathSegment::cubicTo({0.31567003375592223, 0.12000000000000001}, {0.3, 0.11552284749830793},
                             {0.3, 0.11}),
        PathSegment::cubicTo({0.3, 0.10447715250169207}, {0.31567003375592223, 0.1},
                             {0.33499999999999996, 0.1}),
        PathSegment::cubicTo({0.35432996624407775, 0.1}, {0.37, 0.10447715250169207}, {0.37, 0.11}),
        PathSegment::close(),
    };
    // PINS-END
    const FigureEllipse ellipse{0.3, 0.1, 0.07, 0.02};
    EXPECT_EQ(ellipse.segments(), expected);
    EXPECT_EQ(ellipse.bounds(), (Rectangle2D{.x = 0.3, .y = 0.1, .width = 0.07, .height = 0.02}));
    EXPECT_FALSE(ellipse.isEmpty());
}

TEST(FigureEllipse, AZeroExtentStillIteratesANegativeOneDoesNot)
{
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> zeroWidth{
        PathSegment::moveTo({1.0, 2.0}),
        PathSegment::cubicTo({1.0, 2.5522847498307932}, {1.0, 3.0}, {1.0, 3.0}),
        PathSegment::cubicTo({1.0, 3.0}, {1.0, 2.5522847498307932}, {1.0, 2.0}),
        PathSegment::cubicTo({1.0, 1.4477152501692068}, {1.0, 1.0}, {1.0, 1.0}),
        PathSegment::cubicTo({1.0, 1.0}, {1.0, 1.4477152501692068}, {1.0, 2.0}),
        PathSegment::close(),
    };
    // PINS-END
    EXPECT_EQ((FigureEllipse{1, 1, 0, 2}.segments()), zeroWidth);
    EXPECT_TRUE((FigureEllipse{1, 1, 0, 2}.isEmpty()));
    EXPECT_FALSE((FigureEllipse{1, 1, 0, 2}.contains({1, 2})));
    EXPECT_TRUE((FigureEllipse{0, 0, -1, 1}.segments().empty()));
    EXPECT_EQ((FigureEllipse{0, 0, -1, 1}.bounds()),
              (Rectangle2D{.x = 0.0, .y = 0.0, .width = -1.0, .height = 1.0}));
}

TEST(FigureEllipse, ContainsTheOpenInteriorOnly)
{
    // (0.335, 0.12) is on the upper end of the vertical axis, but (0.12 - 0.1) / 0.02 rounds
    // below 1, so Java finds it inside; the other three axis ends are outside.
    // PINS-BEGIN (ShapeProbe.java)
    constexpr std::array<ContainsCase, 9> kCases{{
        {{0.335, 0.11}, true},
        {{0.3, 0.11}, false},
        {{0.37, 0.11}, false},
        {{0.335, 0.1}, false},
        {{0.335, 0.12}, true},
        {{0.3001, 0.11}, true},
        {{0.3, 0.1}, false},
        {{0.36, 0.118}, false},
        {{0.36, 0.1185}, false},
    }};
    // PINS-END
    EXPECT_TRUE(containsAsJava(FigureShape{FigureEllipse{0.3, 0.1, 0.07, 0.02}}, kCases));
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE((FigureEllipse{0.3, 0.1, 0.07, 0.02}.contains({kNaN, 0.11})));
}

TEST(FigureEllipse, TheControlValueIsJavas)
{
    // EllipseIterator.CtrlVal, which is also ArcIterator's control value for a quarter circle.
    EXPECT_EQ(FigureEllipse::kControlValue, 0.5522847498307933);
}

}  // namespace
