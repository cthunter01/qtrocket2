#include "QtRocket/figure/PathSegment.h"

#include <sstream>

#include <gtest/gtest.h>

#include "QtRocket/util/Geometry2D.h"

namespace
{

using QtRocket::PathSegment;
using QtRocket::Point2D;

using Type = PathSegment::Type;

TEST(PathSegment, FactoriesFillTheUsedPoints)
{
    constexpr PathSegment kMove = PathSegment::moveTo({1, 2});
    static_assert(kMove.type == Type::MOVE_TO);
    static_assert(kMove.points[0] == Point2D{1, 2});
    static_assert(kMove.points[1] == Point2D{});
    static_assert(kMove.pointCount() == 1);
    constexpr PathSegment kLine = PathSegment::lineTo({3, 4});
    static_assert(kLine.type == Type::LINE_TO && kLine.pointCount() == 1);
    constexpr PathSegment kQuad = PathSegment::quadTo({1, 2}, {3, 4});
    static_assert(kQuad.type == Type::QUAD_TO && kQuad.pointCount() == 2);
    static_assert(kQuad.points[1] == Point2D{3, 4});
    constexpr PathSegment kCubic = PathSegment::cubicTo({1, 2}, {3, 4}, {5, 6});
    static_assert(kCubic.type == Type::CUBIC_TO && kCubic.pointCount() == 3);
    static_assert(kCubic.points[2] == Point2D{5, 6});
    constexpr PathSegment kClose = PathSegment::close();
    static_assert(kClose.type == Type::CLOSE && kClose.pointCount() == 0);
    EXPECT_EQ(kClose, PathSegment::close());
}

TEST(PathSegment, ComparesTypeAndPoints)
{
    EXPECT_EQ(PathSegment::moveTo({1, 2}), PathSegment::moveTo({1, 2}));
    EXPECT_NE(PathSegment::moveTo({1, 2}), PathSegment::lineTo({1, 2}));
    EXPECT_NE(PathSegment::moveTo({1, 2}), PathSegment::moveTo({1, 3}));
}

TEST(PathSegment, PrintsAsSvgPathData)
{
    EXPECT_EQ(QtRocket::pathSegmentLetter(Type::MOVE_TO), 'M');
    EXPECT_EQ(QtRocket::pathSegmentLetter(Type::LINE_TO), 'L');
    EXPECT_EQ(QtRocket::pathSegmentLetter(Type::QUAD_TO), 'Q');
    EXPECT_EQ(QtRocket::pathSegmentLetter(Type::CUBIC_TO), 'C');
    EXPECT_EQ(QtRocket::pathSegmentLetter(Type::CLOSE), 'Z');
    std::ostringstream os;
    os << PathSegment::moveTo({0.5, -2}) << "; " << PathSegment::cubicTo({1, 2}, {3, 4}, {5, 6.25})
       << "; " << PathSegment::close();
    EXPECT_EQ(os.str(), "M 0.5,-2; C 1,2 3,4 5,6.25; Z");
}

}  // namespace
