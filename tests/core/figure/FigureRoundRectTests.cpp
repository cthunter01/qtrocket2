#include "QtRocket/figure/FigureRoundRect.h"

#include <algorithm>
#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/util/Geometry2D.h"
#include "figure/FigureTestSupport.h"

namespace
{

using QtRocket::FigureRoundRect;
using QtRocket::FigureShape;
using QtRocket::PathSegment;
using QtRocket::Rectangle2D;
using QtRocket::Test::containsAsJava;
using QtRocket::Test::ContainsCase;

// Every value below was printed by java.awt.geom (JDK 17, the probe ShapeProbe.java of the port's
// scratch directory) for the same rectangles: RoundRectangle2D.Double's PathIterator
// (RoundRectIterator), getBounds2D and contains. The iterator only multiplies and adds Java's
// constants, so the segments are pinned exactly.

/// A mass object's side shape as MassComponentShapes makes it: length 50 mm, radius 10 mm at
/// y = 25.4 mm, arcs min(length, 2 radius) * 0.7.
[[nodiscard]] FigureRoundRect massObjectRoundRect()
{
    constexpr double kLength = 0.05;
    constexpr double kRadius = 0.01;
    const double     arc     = std::min(kLength, 2 * kRadius) * 0.7;
    return FigureRoundRect{0.2, 0.0254 - kRadius, kLength, 2 * kRadius, arc, arc};
}

TEST(FigureRoundRect, SegmentsFollowRoundRectIterator)
{
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({0.2, 0.022399999999999996}),
        PathSegment::lineTo({0.2, 0.0284}),
        PathSegment::cubicTo({0.2, 0.03226599324881556}, {0.20313400675118445, 0.0354},
                             {0.20700000000000002, 0.0354}),
        PathSegment::lineTo({0.243, 0.0354}),
        PathSegment::cubicTo({0.24686599324881556, 0.0354}, {0.25, 0.03226599324881556},
                             {0.25, 0.0284}),
        PathSegment::lineTo({0.25, 0.022399999999999996}),
        PathSegment::cubicTo({0.25, 0.018534006751184447},
                             {0.24686599324881556, 0.015399999999999999},
                             {0.243, 0.015399999999999999}),
        PathSegment::lineTo({0.20700000000000002, 0.015399999999999999}),
        PathSegment::cubicTo({0.20313400675118445, 0.015399999999999999},
                             {0.2, 0.018534006751184447}, {0.2, 0.022399999999999996}),
        PathSegment::close(),
    };
    // PINS-END
    const FigureRoundRect rect = massObjectRoundRect();
    EXPECT_EQ(rect.arcWidth(), 0.013999999999999999);
    EXPECT_EQ(rect.segments(), expected);
    EXPECT_EQ(rect.bounds(),
              (Rectangle2D{.x = 0.2, .y = 0.015399999999999999, .width = 0.05, .height = 0.02}));
}

TEST(FigureRoundRect, ArcsAreClampedToTheFrame)
{
    // MassComponentShapes' recovery hardware symbol asks for arcs of 15 m by 5 m: the iterator
    // clamps them to the width and the height, so the corners meet in the middle of each side.
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({0.0, 0.01}),
        PathSegment::lineTo({0.0, 0.01}),
        PathSegment::cubicTo({0.0, 0.015522847498307932}, {0.006715728752538099, 0.02},
                             {0.015, 0.02}),
        PathSegment::lineTo({0.015, 0.02}),
        PathSegment::cubicTo({0.0232842712474619, 0.02}, {0.03, 0.015522847498307932},
                             {0.03, 0.01}),
        PathSegment::lineTo({0.03, 0.01}),
        PathSegment::cubicTo({0.03, 0.004477152501692067}, {0.0232842712474619, 0.0}, {0.015, 0.0}),
        PathSegment::lineTo({0.015, 0.0}),
        PathSegment::cubicTo({0.006715728752538099, 0.0}, {0.0, 0.004477152501692067}, {0.0, 0.01}),
        PathSegment::close(),
    };
    constexpr std::array<ContainsCase, 6> kCases{{
        {{0.015, 0.01}, true},
        {{0.001, 0.001}, false},
        {{0.0045, 0.0015}, false},
        {{0.015, 0.0}, true},
        {{0.0, 0.01}, true},
        {{0.029, 0.019}, false},
    }};
    // PINS-END
    const FigureRoundRect rect{0.0, 0.0, 0.03, 0.02, 15, 5};
    EXPECT_EQ(rect.segments(), expected);
    // The arcs are kept as given; only the iterator and contains() clamp them.
    EXPECT_EQ(rect.arcWidth(), 15.0);
    EXPECT_EQ(rect.arcHeight(), 5.0);
    EXPECT_EQ(rect.bounds(), (Rectangle2D{.x = 0.0, .y = 0.0, .width = 0.03, .height = 0.02}));
    EXPECT_TRUE(containsAsJava(FigureShape{rect}, kCases));
}

TEST(FigureRoundRect, NegativeArcsCountByTheirMagnitude)
{
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({1.0, 2.025}),
        PathSegment::lineTo({1.0, 2.225}),
        PathSegment::cubicTo({1.0, 2.2388071187457697}, {1.0223857625084602, 2.25}, {1.05, 2.25}),
        PathSegment::lineTo({1.45, 2.25}),
        PathSegment::cubicTo({1.4776142374915398, 2.25}, {1.5, 2.2388071187457697}, {1.5, 2.225}),
        PathSegment::lineTo({1.5, 2.025}),
        PathSegment::cubicTo({1.5, 2.0111928812542303}, {1.4776142374915398, 2.0}, {1.45, 2.0}),
        PathSegment::lineTo({1.05, 2.0}),
        PathSegment::cubicTo({1.0223857625084602, 2.0}, {1.0, 2.0111928812542303}, {1.0, 2.025}),
        PathSegment::close(),
    };
    // PINS-END
    EXPECT_EQ((FigureRoundRect{1.0, 2.0, 0.5, 0.25, -0.1, -0.05}.segments()), expected);
}

TEST(FigureRoundRect, ZeroArcsGiveDegenerateCornersAndANegativeWidthNothing)
{
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> zeroArcs{
        PathSegment::moveTo({1.0, 2.0}),
        PathSegment::lineTo({1.0, 2.25}),
        PathSegment::cubicTo({1.0, 2.25}, {1.0, 2.25}, {1.0, 2.25}),
        PathSegment::lineTo({1.5, 2.25}),
        PathSegment::cubicTo({1.5, 2.25}, {1.5, 2.25}, {1.5, 2.25}),
        PathSegment::lineTo({1.5, 2.0}),
        PathSegment::cubicTo({1.5, 2.0}, {1.5, 2.0}, {1.5, 2.0}),
        PathSegment::lineTo({1.0, 2.0}),
        PathSegment::cubicTo({1.0, 2.0}, {1.0, 2.0}, {1.0, 2.0}),
        PathSegment::close(),
    };
    constexpr std::array<ContainsCase, 3> kZeroArcCases{{
        {{1.0, 2.0}, true},
        {{1.25, 2.1}, true},
        {{1.5, 2.1}, false},
    }};
    // PINS-END
    const FigureRoundRect rect{1.0, 2.0, 0.5, 0.25, 0, 0};
    EXPECT_EQ(rect.segments(), zeroArcs);
    EXPECT_TRUE(containsAsJava(FigureShape{rect}, kZeroArcCases));
    const FigureRoundRect negative{1.0, 2.0, -0.5, 0.25, 0.1, 0.05};
    EXPECT_TRUE(negative.segments().empty());
    EXPECT_TRUE(negative.isEmpty());
    EXPECT_FALSE(negative.contains({0.8, 2.1}));
    EXPECT_EQ(negative.bounds(), (Rectangle2D{.x = 1.0, .y = 2.0, .width = -0.5, .height = 0.25}));
}

TEST(FigureRoundRect, ContainsTestsTheCornersAgainstTheirEllipses)
{
    // PINS-BEGIN (ShapeProbe.java)
    constexpr std::array<ContainsCase, 12> kCases{{
        {{0.2, 0.0254}, true},
        {{0.2, 0.0154}, false},
        {{0.2005, 0.0156}, false},
        {{0.2025, 0.0174}, true},
        {{0.2015, 0.0164}, false},
        {{0.225, 0.0154}, true},
        {{0.25, 0.0254}, false},
        {{0.2499, 0.0253}, true},
        {{0.2475, 0.0334}, true},
        {{0.2495, 0.035}, false},
        {{0.225, 0.0354}, false},
        {{0.205, 0.035}, true},
    }};
    // PINS-END
    EXPECT_TRUE(containsAsJava(FigureShape{massObjectRoundRect()}, kCases));
}

}  // namespace
