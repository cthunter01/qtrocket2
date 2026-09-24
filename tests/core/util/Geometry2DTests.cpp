#include "QtRocket/util/Geometry2D.h"

#include <array>
#include <cstddef>
#include <limits>
#include <numbers>
#include <span>

#include <gtest/gtest.h>

namespace
{

using QtRocket::distance;
using QtRocket::distanceSquared;
using QtRocket::Point2D;
using QtRocket::pointToSegmentDistance;
using QtRocket::pointToSegmentDistanceSquared;
using QtRocket::relativeCcw;
using QtRocket::segmentsIntersect;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

TEST(Geometry2D, DistanceIsEuclidean)
{
    EXPECT_DOUBLE_EQ(distance({0, 0}, {3, 4}), 5.0);
    EXPECT_DOUBLE_EQ(distance({3, 4}, {0, 0}), 5.0);
    EXPECT_DOUBLE_EQ(distance({1, 1}, {1, 1}), 0.0);
    EXPECT_DOUBLE_EQ(distance({-1, -1}, {1, 1}), 2 * std::numbers::sqrt2);
    EXPECT_DOUBLE_EQ(distanceSquared({0, 0}, {3, 4}), 25.0);
    EXPECT_DOUBLE_EQ(distanceSquared({-1, 2}, {2, -2}), 25.0);
}

TEST(Geometry2D, PointsCompareByValue)
{
    EXPECT_EQ((Point2D{1, 2}), (Point2D{1, 2}));
    EXPECT_NE((Point2D{1, 2}), (Point2D{2, 1}));
    EXPECT_EQ(Point2D{}.x, 0.0);
    EXPECT_EQ(Point2D{}.y, 0.0);
}

TEST(Geometry2D, RelativeCcwFollowsLine2D)
{
    const Point2D a{0, 0};
    const Point2D b{2, 0};
    // Towards +y the segment turns from +x to +y: -1, as Line2D.relativeCCW.
    EXPECT_EQ(relativeCcw(a, b, {1, 1}), -1);
    EXPECT_EQ(relativeCcw(a, b, {1, -1}), 1);
    // On the segment, including its end points.
    EXPECT_EQ(relativeCcw(a, b, {1, 0}), 0);
    EXPECT_EQ(relativeCcw(a, b, a), 0);
    EXPECT_EQ(relativeCcw(a, b, b), 0);
    // Collinear but outside: beyond a is -1, beyond b is +1.
    EXPECT_EQ(relativeCcw(a, b, {-1, 0}), -1);
    EXPECT_EQ(relativeCcw(a, b, {3, 0}), 1);
    // The direction of the segment flips the answer.
    EXPECT_EQ(relativeCcw(b, a, {1, 1}), 1);
    EXPECT_EQ(relativeCcw(b, a, {-1, 0}), 1);
    EXPECT_EQ(relativeCcw(b, a, {3, 0}), -1);
}

TEST(Geometry2D, RelativeCcwOfADegenerateSegmentIsZero)
{
    EXPECT_EQ(relativeCcw({1, 1}, {1, 1}, {5, -3}), 0);
    EXPECT_EQ(relativeCcw({1, 1}, {1, 1}, {1, 1}), 0);
}

TEST(Geometry2D, RelativeCcwWithNanIsZero)
{
    EXPECT_EQ(relativeCcw({0, 0}, {1, 0}, {kNaN, 1}), 0);
    EXPECT_EQ(relativeCcw({0, 0}, {kNaN, 0}, {1, 1}), 0);
}

TEST(Geometry2D, CrossingSegmentsIntersect)
{
    EXPECT_TRUE(segmentsIntersect({0, 0}, {2, 2}, {0, 2}, {2, 0}));
    EXPECT_TRUE(segmentsIntersect({-1, 0}, {1, 0}, {0, -1}, {0, 1}));
    // Symmetric in the two segments and in each segment's direction.
    EXPECT_TRUE(segmentsIntersect({0, 2}, {2, 0}, {0, 0}, {2, 2}));
    EXPECT_TRUE(segmentsIntersect({2, 2}, {0, 0}, {2, 0}, {0, 2}));
}

TEST(Geometry2D, SeparateSegmentsDoNotIntersect)
{
    // Parallel.
    EXPECT_FALSE(segmentsIntersect({0, 0}, {2, 0}, {0, 1}, {2, 1}));
    // The lines cross, the segments do not.
    EXPECT_FALSE(segmentsIntersect({0, 0}, {1, 1}, {2, 0}, {3, -1}));
    EXPECT_FALSE(segmentsIntersect({0, 0}, {1, 0}, {2, 1}, {2, -1}));
    // Far apart.
    EXPECT_FALSE(segmentsIntersect({0, 0}, {1, 1}, {10, 10}, {11, 12}));
}

TEST(Geometry2D, TouchingSegmentsIntersect)
{
    // Sharing an end point (an L).
    EXPECT_TRUE(segmentsIntersect({0, 0}, {1, 0}, {1, 0}, {1, 1}));
    // An end point on the other segment's interior (a T).
    EXPECT_TRUE(segmentsIntersect({0, 0}, {2, 0}, {1, 0}, {1, 1}));
    EXPECT_TRUE(segmentsIntersect({1, 0}, {1, 1}, {0, 0}, {2, 0}));
    // Just past the touch is not touching.
    EXPECT_FALSE(segmentsIntersect({0, 0}, {2, 0}, {1, 1e-9}, {1, 1}));
}

TEST(Geometry2D, CollinearSegmentsIntersectWhenTheyOverlapOrTouch)
{
    // Overlapping.
    EXPECT_TRUE(segmentsIntersect({0, 0}, {2, 0}, {1, 0}, {3, 0}));
    EXPECT_TRUE(segmentsIntersect({1, 0}, {3, 0}, {0, 0}, {2, 0}));
    // One inside the other.
    EXPECT_TRUE(segmentsIntersect({0, 0}, {3, 0}, {1, 0}, {2, 0}));
    // Identical.
    EXPECT_TRUE(segmentsIntersect({0, 0}, {1, 1}, {0, 0}, {1, 1}));
    EXPECT_TRUE(segmentsIntersect({0, 0}, {1, 1}, {1, 1}, {0, 0}));
    // Touching end to end.
    EXPECT_TRUE(segmentsIntersect({0, 0}, {1, 0}, {1, 0}, {2, 0}));
    EXPECT_TRUE(segmentsIntersect({0, 0}, {1, 1}, {2, 2}, {1, 1}));
    // A gap between them.
    EXPECT_FALSE(segmentsIntersect({0, 0}, {1, 0}, {2, 0}, {3, 0}));
    EXPECT_FALSE(segmentsIntersect({2, 0}, {3, 0}, {0, 0}, {1, 0}));
    EXPECT_FALSE(segmentsIntersect({0, 0}, {1, 1}, {1.0000001, 1.0000001}, {2, 2}));
}

TEST(Geometry2D, DegenerateSegmentsIntersectWhatTheyLieOn)
{
    // A point on a segment.
    EXPECT_TRUE(segmentsIntersect({1, 1}, {1, 1}, {0, 0}, {2, 2}));
    EXPECT_TRUE(segmentsIntersect({0, 0}, {2, 2}, {1, 1}, {1, 1}));
    // A point at an end.
    EXPECT_TRUE(segmentsIntersect({2, 2}, {2, 2}, {0, 0}, {2, 2}));
    // A point beside a segment, or collinear beyond it.
    EXPECT_FALSE(segmentsIntersect({1, 2}, {1, 2}, {0, 0}, {2, 2}));
    EXPECT_FALSE(segmentsIntersect({5, 5}, {5, 5}, {0, 0}, {2, 2}));
    // Two points: relativeCCW is 0 for every point relative to a zero-length segment, so Line2D
    // reports even two distinct points as intersecting.
    EXPECT_TRUE(segmentsIntersect({1, 1}, {1, 1}, {1, 1}, {1, 1}));
    EXPECT_TRUE(segmentsIntersect({1, 1}, {1, 1}, {1, 2}, {1, 2}));
}

TEST(Geometry2D, DecimalCoordinatesOnASegmentAreClassifiedExactlyLikeJava)
{
    // Line2D.relativeCCW compares px*y2 - py*x2 with 0. Java rounds each product separately, so
    // for a point on the segment the two products are equal and the difference is exactly 0; a
    // fused multiply-subtract keeps the product's rounding error instead (hence -ffp-contract=off
    // in QtRocket_configure_target). Dyadic coordinates cannot tell the two apart, decimals can.
    EXPECT_EQ(relativeCcw({0.1, 0.1}, {0.7, 0.7}, {0.3, 0.3}), 0);
    EXPECT_EQ(relativeCcw({0.1, 0.1}, {0.3, 0.3}, {0.2, 0.2}), 0);
    // The end points themselves.
    EXPECT_EQ(relativeCcw({0.2, 0.16}, {0.3, 0.19}, {0.3, 0.19}), 0);
    EXPECT_EQ(relativeCcw({0.2, 0.16}, {0.3, 0.19}, {0.2, 0.16}), 0);
    EXPECT_EQ(relativeCcw({1.1, 2.3}, {0.4, 0.6}, {0.4, 0.6}), 0);
}

TEST(Geometry2D, DecimalSegmentsSharingAPointIntersect)
{
    // An L: the shared end point is on both segments.
    EXPECT_TRUE(segmentsIntersect({0.2, 0.16}, {0.3, 0.19}, {0.3, 0.19}, {0.4, 0.22}));
    EXPECT_TRUE(segmentsIntersect({0.1, 0.5}, {0.7, 0.3}, {0.7, 0.3}, {0.9, 0.8}));
    EXPECT_TRUE(segmentsIntersect({1.1, 2.3}, {0.4, 0.6}, {0.4, 0.6}, {3.3, 0.7}));
    EXPECT_TRUE(segmentsIntersect({0.3, 0.3}, {0.6, 0.1}, {0.6, 0.1}, {0.9, 0.5}));
    // A T: one segment ends on the other's interior, with the rest of it on the side that a
    // misclassified junction would put the junction on too.
    EXPECT_TRUE(segmentsIntersect({0.1, 0.1}, {0.7, 0.7}, {0.3, 0.3}, {0.3, -0.5}));
    EXPECT_TRUE(segmentsIntersect({0.3, 0.3}, {0.3, -0.5}, {0.1, 0.1}, {0.7, 0.7}));
    // Collinear along y = x: touching end to end, and a point on the segment.
    EXPECT_TRUE(segmentsIntersect({0.1, 0.1}, {0.3, 0.3}, {0.3, 0.3}, {0.7, 0.7}));
    EXPECT_TRUE(segmentsIntersect({0.1, 0.1}, {0.2, 0.2}, {0.2, 0.2}, {0.3, 0.3}));
    EXPECT_TRUE(segmentsIntersect({0.1, 0.1}, {0.3, 0.3}, {0.2, 0.2}, {0.2, 0.2}));
    EXPECT_TRUE(segmentsIntersect({0.1, 0.1}, {0.2, 0.2}, {0.1, 0.1}, {0.5, 0.5}));
}

TEST(Geometry2D, NanCoordinatesCountAsIntersectingLikeLine2D)
{
    EXPECT_TRUE(segmentsIntersect({kNaN, 0}, {1, 0}, {5, 5}, {6, 6}));
}

TEST(Geometry2D, FreeformFinOutlineSelfIntersection)
{
    // FreeformFinSet checks each edge against every non-adjacent edge. A bow tie crosses itself,
    // a trapezoid does not.
    constexpr std::array<Point2D, 5> kBowTie    = {{{0, 0}, {1, 1}, {1, 0}, {0, 1}, {0, 0}}};
    constexpr std::array<Point2D, 5> kTrapezoid = {
        {{0, 0}, {0.2, 0.5}, {0.6, 0.5}, {1, 0}, {0, 0}}};

    const auto selfIntersects = [](std::span<const Point2D> outline) {
        for (std::size_t i = 0; i + 1 < outline.size(); ++i)
        {
            for (std::size_t j = i + 2; j + 1 < outline.size(); ++j)
            {
                if (i == 0 && j + 2 == outline.size())
                {
                    continue;  // the closing edge shares the first point with the first edge
                }
                if (segmentsIntersect(outline[i], outline[i + 1], outline[j], outline[j + 1]))
                {
                    return true;
                }
            }
        }
        return false;
    };
    EXPECT_TRUE(selfIntersects(kBowTie));
    EXPECT_FALSE(selfIntersects(kTrapezoid));
}

TEST(Geometry2D, PointToSegmentDistanceIsTheNearestPointOnTheSegment)
{
    const Point2D a{0, 0};
    const Point2D b{4, 0};
    // Alongside: the perpendicular distance.
    EXPECT_DOUBLE_EQ(pointToSegmentDistance({2, 3}, a, b), 3.0);
    EXPECT_DOUBLE_EQ(pointToSegmentDistance({2, -3}, a, b), 3.0);
    EXPECT_DOUBLE_EQ(pointToSegmentDistanceSquared({2, 3}, a, b), 9.0);
    // Beyond a or b: the distance to that end point.
    EXPECT_DOUBLE_EQ(pointToSegmentDistance({-3, 4}, a, b), 5.0);
    EXPECT_DOUBLE_EQ(pointToSegmentDistance({7, 4}, a, b), 5.0);
    EXPECT_DOUBLE_EQ(pointToSegmentDistanceSquared({7, 4}, a, b), 25.0);
    // On the segment.
    EXPECT_DOUBLE_EQ(pointToSegmentDistance({1, 0}, a, b), 0.0);
    EXPECT_DOUBLE_EQ(pointToSegmentDistance(a, a, b), 0.0);
    EXPECT_DOUBLE_EQ(pointToSegmentDistance(b, a, b), 0.0);
    // The segment's direction does not matter.
    EXPECT_DOUBLE_EQ(pointToSegmentDistance({7, 4}, b, a), 5.0);
    EXPECT_DOUBLE_EQ(pointToSegmentDistance({2, 3}, b, a), 3.0);
}

TEST(Geometry2D, PointToDiagonalSegmentDistance)
{
    // The segment y = x from (0,0) to (2,2); (0,2) projects onto its middle at distance sqrt(2).
    EXPECT_DOUBLE_EQ(pointToSegmentDistance({0, 2}, {0, 0}, {2, 2}), std::numbers::sqrt2);
    EXPECT_DOUBLE_EQ(pointToSegmentDistanceSquared({0, 2}, {0, 0}, {2, 2}), 2.0);
}

TEST(Geometry2D, PointToDegenerateSegmentDistanceIsThePointDistance)
{
    EXPECT_DOUBLE_EQ(pointToSegmentDistance({3, 4}, {0, 0}, {0, 0}), 5.0);
    EXPECT_DOUBLE_EQ(pointToSegmentDistance({1, 1}, {1, 1}, {1, 1}), 0.0);
}

}  // namespace
