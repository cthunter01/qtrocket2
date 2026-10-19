#include "QtRocket/figure/FigureLine.h"

#include <cmath>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/util/Geometry2D.h"

namespace
{

using QtRocket::FigureLine;
using QtRocket::FigureShape;
using QtRocket::PathSegment;
using QtRocket::Rectangle2D;

// The values below are java.awt.geom's (JDK 17, the probe ShapeProbe.java of the port's scratch
// directory) for Line2D.Double: its PathIterator, getBounds2D and contains.

TEST(FigureLine, SegmentsAreAMoveAndALine)
{
    const FigureLine line{1.0, 2.0, 3.0, -4.0};
    EXPECT_EQ(line.segments(), (std::vector<PathSegment>{PathSegment::moveTo({1.0, 2.0}),
                                                         PathSegment::lineTo({3.0, -4.0})}));
    const FigureLine point{0.5, 0.25, 0.5, 0.25};
    EXPECT_EQ(point.segments(), (std::vector<PathSegment>{PathSegment::moveTo({0.5, 0.25}),
                                                          PathSegment::lineTo({0.5, 0.25})}));
}

TEST(FigureLine, BoundsStartAtTheSmallerEnd)
{
    // PINS-BEGIN (ShapeProbe.java)
    constexpr Rectangle2D kBounds{.x = 1.0, .y = -4.0, .width = 2.0, .height = 6.0};
    // PINS-END
    EXPECT_EQ((FigureLine{1.0, 2.0, 3.0, -4.0}.bounds()), kBounds);
    EXPECT_EQ((FigureLine{3.0, -4.0, 1.0, 2.0}.bounds()), kBounds);
    EXPECT_EQ((FigureLine{0.5, 0.25, 0.5, 0.25}.bounds()),
              (Rectangle2D{.x = 0.5, .y = 0.25, .width = 0.0, .height = 0.0}));
}

TEST(FigureLine, ABoundsCornerWithANaNTakesTheSecondEnd)
{
    // x1 < x2 is false for a NaN, so Java takes x2 and x1 - x2.
    constexpr double  kNaN   = std::numeric_limits<double>::quiet_NaN();
    const Rectangle2D bounds = FigureLine{kNaN, 1.0, 2.0, 3.0}.bounds();
    EXPECT_EQ(bounds.x, 2.0);
    EXPECT_TRUE(std::isnan(bounds.width));
    EXPECT_EQ(bounds.y, 1.0);
    EXPECT_EQ(bounds.height, 2.0);
}

TEST(FigureLine, ContainsNothing)
{
    // Line2D.contains is always false, even on the line or at an end.
    const FigureShape line{FigureLine{1.0, 2.0, 3.0, -4.0}};
    EXPECT_FALSE(line.contains({2.0, -1.0}));
    EXPECT_FALSE(line.contains({1.0, 2.0}));
    EXPECT_FALSE(FigureLine::contains({0.0, 0.0}));
}

}  // namespace
