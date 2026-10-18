#include "QtRocket/figure/FigurePath.h"

#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigureLine.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/figure/WindingRule.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Geometry2D.h"
#include "figure/FigureTestSupport.h"

namespace
{

using QtRocket::BugError;
using QtRocket::FigureEllipse;
using QtRocket::FigureLine;
using QtRocket::FigurePath;
using QtRocket::FigureRect;
using QtRocket::FigureShape;
using QtRocket::PathSegment;
using QtRocket::Point2D;
using QtRocket::Rectangle2D;
using QtRocket::WindingRule;
using QtRocket::Test::containsAsJava;
using QtRocket::Test::ContainsCase;

using Precision = FigurePath::Precision;

// Every value below was printed by java.awt.geom (JDK 17, the probe ShapeProbe.java of the port's
// scratch directory) for paths built by the same calls: Path2D.Double's and Path2D.Float's
// PathIterator, getBounds2D and contains. Paths store what they are given (rounded to floats for
// Path2D.Float), so everything is pinned exactly.

/// mixedPath of the probe: every segment type, a moveTo that replaces the one before it, a
/// second closePath that adds nothing, and an open subpath after a closed one.
[[nodiscard]] FigurePath mixedPath()
{
    FigurePath path;
    path.moveTo(9, 9);
    path.moveTo(0.1, 0.2);
    path.lineTo(0.4, 0.2);
    path.quadTo(0.5, 0.3, 0.4, 0.4);
    path.curveTo(0.3, 0.5, 0.2, -0.1, 0.1, 0.4);
    path.closePath();
    path.closePath();
    path.moveTo(1, 1);
    path.lineTo(2, 1);
    return path;
}

/// floatPath of the probe: a Path2D.Float of every segment type.
[[nodiscard]] FigurePath floatPath()
{
    FigurePath path{Precision::FLOAT};
    path.moveTo(0.1, 0.2);
    path.lineTo(0.30000000000000004, 0.2);
    path.lineTo(0.30000000000000004, 0.7);
    path.quadTo(0.123456789, 0.987654321, 1e-3, 0.7);
    path.curveTo(1.0 / 3, 2.0 / 3, 1.0 / 7, 0.0125, 0.1, 0.2);
    path.closePath();
    return path;
}

/// Two overlapping squares wound the same way, under @p rule.
[[nodiscard]] FigurePath overlappingSquares(WindingRule rule)
{
    FigurePath path{Precision::DOUBLE, rule};
    path.moveTo(0, 0);
    path.lineTo(2, 0);
    path.lineTo(2, 2);
    path.lineTo(0, 2);
    path.closePath();
    path.moveTo(1, 1);
    path.lineTo(3, 1);
    path.lineTo(3, 3);
    path.lineTo(1, 3);
    path.closePath();
    return path;
}

TEST(FigurePath, KeepsSegmentsAsPath2DDoubleDoes)
{
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({0.1, 0.2}),
        PathSegment::lineTo({0.4, 0.2}),
        PathSegment::quadTo({0.5, 0.3}, {0.4, 0.4}),
        PathSegment::cubicTo({0.3, 0.5}, {0.2, -0.1}, {0.1, 0.4}),
        PathSegment::close(),
        PathSegment::moveTo({1.0, 1.0}),
        PathSegment::lineTo({2.0, 1.0}),
    };
    // PINS-END
    const FigurePath path = mixedPath();
    EXPECT_EQ(path.segments(), expected);
    EXPECT_EQ(path.precision(), Precision::DOUBLE);
    EXPECT_EQ(path.windingRule(), WindingRule::NON_ZERO);
    EXPECT_FALSE(path.isEmpty());
}

TEST(FigurePath, BoundsIncludeTheControlPoints)
{
    // JDK 17's Path2D.getBounds2D takes every coordinate, so the cubic's control point at
    // y = -0.1 extends the bounds below the curve.
    // PINS-BEGIN (ShapeProbe.java)
    EXPECT_EQ(mixedPath().bounds(),
              (Rectangle2D{.x = 0.1, .y = -0.1, .width = 1.9, .height = 1.1}));
    // PINS-END
}

TEST(FigurePath, ContainsCountsCrossingsOfEveryCurve)
{
    // PINS-BEGIN (ShapeProbe.java)
    constexpr std::array<ContainsCase, 9> kCases{{
        {{0.2, 0.3}, false},
        {{0.45, 0.3}, false},
        {{0.47, 0.3}, false},
        {{0.1, 0.2}, true},
        {{0.4, 0.2}, false},
        {{0.15, 0.39}, false},
        {{0.25, 0.25}, true},
        {{1.5, 1.0}, false},
        {{0.3, 0.1}, false},
    }};
    // PINS-END
    EXPECT_TRUE(containsAsJava(FigureShape{mixedPath()}, kCases));
}

TEST(FigurePath, AFloatPathRoundsEveryCoordinate)
{
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({0.10000000149011612, 0.20000000298023224}),
        PathSegment::lineTo({0.30000001192092896, 0.20000000298023224}),
        PathSegment::lineTo({0.30000001192092896, 0.699999988079071}),
        PathSegment::quadTo({0.12345679104328156, 0.9876543283462524},
                            {0.0010000000474974513, 0.699999988079071}),
        PathSegment::cubicTo({0.3333333432674408, 0.6666666865348816},
                             {0.1428571492433548, 0.012500000186264515},
                             {0.10000000149011612, 0.20000000298023224}),
        PathSegment::close(),
    };
    const Rectangle2D                     expectedBounds{.x      = 0.0010000000474974513,
                                                         .y      = 0.012500000186264515,
                                                         .width  = 0.33233335614204407,
                                                         .height = 0.9751543402671814};
    constexpr std::array<ContainsCase, 5> kCases{{
        {{0.2, 0.3}, true},
        {{0.1, 0.2}, false},
        {{0.30000000000000004, 0.5}, true},
        {{0.29, 0.5}, true},
        {{0.30000001192092896, 0.5}, false},
    }};
    // PINS-END
    const FigurePath path = floatPath();
    EXPECT_EQ(path.precision(), Precision::FLOAT);
    EXPECT_EQ(path.segments(), expected);
    EXPECT_EQ(path.bounds(), expectedBounds);
    EXPECT_TRUE(containsAsJava(FigureShape{path}, kCases));
}

TEST(FigurePath, AFloatPathComputesItsBoundsInFloat)
{
    // The width of the float bounds is the float difference of two floats (16777216 + 0.1 rounds
    // to 16777216 in float); a double subtraction would give 16777216.100000001.
    FigurePath path{Precision::FLOAT};
    path.moveTo(16777217.0, -3.1);
    path.lineTo(-0.1, 16777219.5);
    // PINS-BEGIN (ShapeProbe.java)
    EXPECT_EQ(path.segments(),
              (std::vector<PathSegment>{PathSegment::moveTo({1.6777216E7, -3.0999999046325684}),
                                        PathSegment::lineTo({-0.10000000149011612, 1.677722E7})}));
    EXPECT_EQ(path.bounds(), (Rectangle2D{.x      = -0.10000000149011612,
                                          .y      = -3.0999999046325684,
                                          .width  = 1.6777216E7,
                                          .height = 1.6777224E7}));
    // PINS-END
}

TEST(FigurePath, BoundsScanFromTheLastPointAsJavaDoes)
{
    // The scan starts at the last coordinate and replaces only on < and >, so the last point's
    // zero signs win: Java's bounds of (0, -0) -> (-0, 0) are (-0, 0, 0, 0).
    FigurePath path;
    path.moveTo(0.0, -0.0);
    path.lineTo(-0.0, 0.0);
    const Rectangle2D bounds = path.bounds();
    EXPECT_TRUE(std::signbit(bounds.x));
    EXPECT_FALSE(std::signbit(bounds.y));
    EXPECT_EQ(bounds.width, 0.0);
    EXPECT_EQ(bounds.height, 0.0);
}

TEST(FigurePath, AnOpenPolylineIsClosedForContains)
{
    // A fin outline is an open Path2D.Float; a hit test closes it with a line back to the start.
    FigurePath triangle{Precision::FLOAT};
    triangle.moveTo(0.0, 0.0);
    triangle.lineTo(0.1, 0.0);
    triangle.lineTo(0.0, 0.05);
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({0.0, 0.0}),
        PathSegment::lineTo({0.10000000149011612, 0.0}),
        PathSegment::lineTo({0.0, 0.05000000074505806}),
    };
    constexpr std::array<ContainsCase, 6> kCases{{
        {{0.01, 0.01}, true},
        {{0.06, 0.03}, false},
        {{0.0, 0.0}, true},
        {{0.05, 0.0}, true},
        {{0.0, 0.02}, true},
        {{0.049, 0.0249}, true},
    }};
    // PINS-END
    EXPECT_EQ(triangle.segments(), expected);
    EXPECT_EQ(
        triangle.bounds(),
        (Rectangle2D{
            .x = 0.0, .y = 0.0, .width = 0.10000000149011612, .height = 0.05000000074505806}));
    EXPECT_TRUE(containsAsJava(FigureShape{triangle}, kCases));
}

TEST(FigurePath, TheWindingRuleDecidesOverlaps)
{
    // PINS-BEGIN (ShapeProbe.java)
    constexpr std::array<ContainsCase, 6> kNonZero{{
        {{1.5, 1.5}, true},
        {{0.5, 0.5}, true},
        {{2.5, 2.5}, true},
        {{2.5, 0.5}, false},
        {{1.0, 1.0}, true},
        {{2.0, 2.0}, true},
    }};
    constexpr std::array<ContainsCase, 6> kEvenOdd{{
        {{1.5, 1.5}, false},
        {{0.5, 0.5}, true},
        {{2.5, 2.5}, true},
        {{2.5, 0.5}, false},
        {{1.0, 1.0}, false},
        {{2.0, 2.0}, true},
    }};
    // PINS-END
    const FigurePath nonZero = overlappingSquares(WindingRule::NON_ZERO);
    const FigurePath evenOdd = overlappingSquares(WindingRule::EVEN_ODD);
    EXPECT_EQ(nonZero.segments(), evenOdd.segments());
    EXPECT_EQ(evenOdd.windingRule(), WindingRule::EVEN_ODD);
    EXPECT_EQ(FigureShape{evenOdd}.windingRule(), WindingRule::EVEN_ODD);
    EXPECT_TRUE(containsAsJava(FigureShape{nonZero}, kNonZero));
    EXPECT_TRUE(containsAsJava(FigureShape{evenOdd}, kEvenOdd));
}

TEST(FigurePath, OppositeWindingsCancelUnderNonZero)
{
    FigurePath path;
    path.moveTo(0, 0);
    path.lineTo(2, 0);
    path.lineTo(2, 2);
    path.lineTo(0, 2);
    path.closePath();
    path.moveTo(1, 1);
    path.lineTo(1, 3);
    path.lineTo(3, 3);
    path.lineTo(3, 1);
    path.closePath();
    // PINS-BEGIN (ShapeProbe.java)
    constexpr std::array<ContainsCase, 3> kCases{{
        {{1.5, 1.5}, false},
        {{0.5, 0.5}, true},
        {{2.5, 2.5}, true},
    }};
    // PINS-END
    EXPECT_TRUE(containsAsJava(FigureShape{path}, kCases));
}

TEST(FigurePath, AppendTakesTheShapesIteratorSegments)
{
    FigurePath path;
    path.append(FigureShape{FigureEllipse{0, 0, 1, 0.5}}, false);
    path.append(FigureShape{FigureRect{2, 0, 1, 1}}, false);
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({1.0, 0.25}),
        PathSegment::cubicTo({1.0, 0.3880711874576983}, {0.7761423749153966, 0.5}, {0.5, 0.5}),
        PathSegment::cubicTo({0.22385762508460333, 0.5}, {0.0, 0.3880711874576983}, {0.0, 0.25}),
        PathSegment::cubicTo({0.0, 0.11192881254230166}, {0.22385762508460333, 0.0}, {0.5, 0.0}),
        PathSegment::cubicTo({0.7761423749153966, 0.0}, {1.0, 0.11192881254230166}, {1.0, 0.25}),
        PathSegment::close(),
        PathSegment::moveTo({2.0, 0.0}),
        PathSegment::lineTo({3.0, 0.0}),
        PathSegment::lineTo({3.0, 1.0}),
        PathSegment::lineTo({2.0, 1.0}),
        PathSegment::lineTo({2.0, 0.0}),
        PathSegment::close(),
    };
    constexpr std::array<ContainsCase, 9> kCases{{
        {{0.5, 0.25}, true},
        {{0.01, 0.25}, true},
        {{0.99, 0.25}, true},
        {{0.15, 0.05}, false},
        {{0.15, 0.06}, false},
        {{0.5, 0.4999}, true},
        {{2.5, 0.5}, true},
        {{3.0, 0.5}, false},
        {{1.5, 0.5}, false},
    }};
    // PINS-END
    EXPECT_EQ(path.segments(), expected);
    EXPECT_EQ(path.bounds(), (Rectangle2D{.x = 0.0, .y = 0.0, .width = 3.0, .height = 1.0}));
    EXPECT_TRUE(containsAsJava(FigureShape{path}, kCases));
}

TEST(FigurePath, AppendWithConnectJoinsOrCollapsesTheFirstMove)
{
    FigurePath path;
    path.append(FigureShape{FigureLine{0, 0, 1, 0}}, true);  // an empty path just moves
    path.append(FigureShape{FigureLine{1, 0, 2, 1}}, true);  // lands on the last point: collapsed
    path.append(FigureShape{FigureLine{5, 5, 6, 6}}, true);  // becomes a line
    path.closePath();
    path.append(FigureShape{FigureLine{0, 0, 1, 1}}, true);  // after a close: always a line
    path.append(FigureShape{FigureRect{1, 1, 1, 1}}, true);  // lands on (1, 1): collapsed
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({0.0, 0.0}),
        PathSegment::lineTo({1.0, 0.0}),
        PathSegment::lineTo({2.0, 1.0}),
        PathSegment::lineTo({5.0, 5.0}),
        PathSegment::lineTo({6.0, 6.0}),
        PathSegment::close(),
        PathSegment::lineTo({0.0, 0.0}),
        PathSegment::lineTo({1.0, 1.0}),
        PathSegment::lineTo({2.0, 1.0}),
        PathSegment::lineTo({2.0, 2.0}),
        PathSegment::lineTo({1.0, 2.0}),
        PathSegment::lineTo({1.0, 1.0}),
        PathSegment::close(),
    };
    // PINS-END
    EXPECT_EQ(path.segments(), expected);
}

TEST(FigurePath, AFloatPathCollapsesMovesThatAreEqualAsFloats)
{
    FigurePath path{Precision::FLOAT};
    path.moveTo(0.1, 0.1);
    path.lineTo(0.30000000000000004, 0.1);
    // 0.3 and 0.30000000000000004 are the same float, so the line's move collapses.
    path.append(FigureShape{FigureLine{0.3, 0.1, 0.4, 0.2}}, true);
    path.append(FigureShape{FigureEllipse{0.123456789, 0.1, 0.2, 0.3}}, true);
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({0.10000000149011612, 0.10000000149011612}),
        PathSegment::lineTo({0.30000001192092896, 0.10000000149011612}),
        PathSegment::lineTo({0.4000000059604645, 0.20000000298023224}),
        PathSegment::lineTo({0.3234567940235138, 0.25}),
        PathSegment::cubicTo({0.3234567940235138, 0.33284270763397217},
                             {0.2786852717399597, 0.4000000059604645},
                             {0.22345678508281708, 0.4000000059604645}),
        PathSegment::cubicTo({0.16822831332683563, 0.4000000059604645},
                             {0.12345679104328156, 0.33284270763397217},
                             {0.12345679104328156, 0.25}),
        PathSegment::cubicTo({0.12345679104328156, 0.16715729236602783},
                             {0.16822831332683563, 0.10000000149011612},
                             {0.22345678508281708, 0.10000000149011612}),
        PathSegment::cubicTo({0.2786852717399597, 0.10000000149011612},
                             {0.3234567940235138, 0.16715729236602783}, {0.3234567940235138, 0.25}),
        PathSegment::close(),
    };
    const Rectangle2D expectedBounds{.x      = 0.10000000149011612,
                                     .y      = 0.10000000149011612,
                                     .width  = 0.30000001192092896,
                                     .height = 0.30000001192092896};
    // PINS-END
    EXPECT_EQ(path.segments(), expected);
    EXPECT_EQ(path.bounds(), expectedBounds);
}

TEST(FigurePath, PathsAppendAcrossPrecisions)
{
    // A double path compares doubles (0.3 is not 0.30000000000000004, so a line is added) and
    // keeps an appended float path's floats; a float path rounds an appended double path.
    FigurePath doubles;
    doubles.moveTo(0.1, 0.1);
    doubles.lineTo(0.30000000000000004, 0.1);
    doubles.append(FigureShape{FigureLine{0.3, 0.1, 0.4, 0.2}}, true);
    doubles.append(FigureShape{floatPath()}, false);
    FigurePath floats{Precision::FLOAT};
    floats.append(FigureShape{mixedPath()}, false);
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expectedDoubles{
        PathSegment::moveTo({0.1, 0.1}),
        PathSegment::lineTo({0.30000000000000004, 0.1}),
        PathSegment::lineTo({0.3, 0.1}),
        PathSegment::lineTo({0.4, 0.2}),
        PathSegment::moveTo({0.10000000149011612, 0.20000000298023224}),
        PathSegment::lineTo({0.30000001192092896, 0.20000000298023224}),
        PathSegment::lineTo({0.30000001192092896, 0.699999988079071}),
        PathSegment::quadTo({0.12345679104328156, 0.9876543283462524},
                            {0.0010000000474974513, 0.699999988079071}),
        PathSegment::cubicTo({0.3333333432674408, 0.6666666865348816},
                             {0.1428571492433548, 0.012500000186264515},
                             {0.10000000149011612, 0.20000000298023224}),
        PathSegment::close(),
    };
    const Rectangle2D              expectedDoublesBounds{.x      = 0.0010000000474974513,
                                                         .y      = 0.012500000186264515,
                                                         .width  = 0.39899999995250257,
                                                         .height = 0.9751543281599879};
    const std::vector<PathSegment> expectedFloats{
        PathSegment::moveTo({0.10000000149011612, 0.20000000298023224}),
        PathSegment::lineTo({0.4000000059604645, 0.20000000298023224}),
        PathSegment::quadTo({0.5, 0.30000001192092896}, {0.4000000059604645, 0.4000000059604645}),
        PathSegment::cubicTo({0.30000001192092896, 0.5},
                             {0.20000000298023224, -0.10000000149011612},
                             {0.10000000149011612, 0.4000000059604645}),
        PathSegment::close(),
        PathSegment::moveTo({1.0, 1.0}),
        PathSegment::lineTo({2.0, 1.0}),
    };
    const Rectangle2D expectedFloatsBounds{.x      = 0.10000000149011612,
                                           .y      = -0.10000000149011612,
                                           .width  = 1.899999976158142,
                                           .height = 1.100000023841858};
    // PINS-END
    EXPECT_EQ(doubles.segments(), expectedDoubles);
    EXPECT_EQ(doubles.bounds(), expectedDoublesBounds);
    EXPECT_EQ(floats.segments(), expectedFloats);
    EXPECT_EQ(floats.bounds(), expectedFloatsBounds);
}

TEST(FigurePath, AnEmptyOrSingleMovePathContainsNothing)
{
    const FigurePath empty;
    EXPECT_TRUE(empty.isEmpty());
    EXPECT_TRUE(empty.segments().empty());
    EXPECT_EQ(empty.bounds(), Rectangle2D{});
    EXPECT_FALSE(empty.contains({0, 0}));

    FigurePath single;
    single.moveTo(1, 1);
    EXPECT_EQ(single.segments(), (std::vector<PathSegment>{PathSegment::moveTo({1.0, 1.0})}));
    EXPECT_EQ(single.bounds(), (Rectangle2D{.x = 1.0, .y = 1.0, .width = 0.0, .height = 0.0}));
    EXPECT_FALSE(single.contains({1, 1}));
}

TEST(FigurePath, ContainsNoNonFinitePoint)
{
    constexpr double kNaN      = std::numeric_limits<double>::quiet_NaN();
    constexpr double kInfinity = std::numeric_limits<double>::infinity();
    const FigurePath squares   = overlappingSquares(WindingRule::NON_ZERO);
    EXPECT_TRUE(squares.contains({0.5, 0.5}));
    EXPECT_FALSE(squares.contains({kNaN, 0.5}));
    EXPECT_FALSE(squares.contains({0.5, kInfinity}));
    EXPECT_FALSE(squares.contains({-kInfinity, 0.5}));
}

TEST(FigurePath, SegmentsBeforeAMoveAreAProgrammingError)
{
    // Java throws IllegalPathStateException("missing initial moveto in path definition").
    FigurePath path;
    EXPECT_THROW(path.lineTo(1, 1), BugError);
    EXPECT_THROW(path.quadTo(1, 1, 2, 2), BugError);
    EXPECT_THROW(path.curveTo(1, 1, 2, 2, 3, 3), BugError);
    EXPECT_THROW(path.closePath(), BugError);
    EXPECT_TRUE(path.isEmpty());
    EXPECT_THROW(path.append(std::vector<PathSegment>{PathSegment::lineTo({1, 1})}, false),
                 BugError);
}

// RailButtonShapes.getShapesSide's path and its invisible hit-test rectangles, built as Java
// builds them, for a button of outer diameter 9.7 mm and inner diameter 8 mm, heights 2, 3 and
// 1.5 mm, at (0.25, 0.0254), with Java's sinr = |sin(0.7)| and cosr = cos(0.7) pinned (the
// probe's values).
// NOLINTBEGIN(modernize-use-std-numbers): Java's sin and cos results, not constants
constexpr double kSinR = 0.644217687237691;
constexpr double kCosR = 0.7648421872844885;
// NOLINTEND(modernize-use-std-numbers)

/// The paths of one rail button cylinder.
struct ButtonPaths
{
    FigurePath visible;
    FigurePath invisible;
};

/// One cylinder of the button as RailButtonShapes appends it: the lower face, the two sides, the
/// upper face, and an invisible rectangle between the faces.
void appendCylinder(ButtonPaths& paths, Point2D center, double radius, double heightCos)
{
    const double  drawWidth  = 2 * radius;
    const double  drawHeight = drawWidth * kSinR;
    const Point2D lowerLeft{center.x - radius, center.y - (radius * kSinR)};
    paths.visible.append(
        FigureShape{FigureEllipse{lowerLeft.x, lowerLeft.y, drawWidth, drawHeight}}, false);
    paths.visible.append(
        FigureShape{FigureLine{lowerLeft.x, center.y, lowerLeft.x, center.y + heightCos}}, false);
    paths.visible.append(FigureShape{FigureLine{center.x + radius, center.y, center.x + radius,
                                                center.y + heightCos}},
                         false);
    paths.visible.append(
        FigureShape{FigureEllipse{lowerLeft.x, lowerLeft.y + heightCos, drawWidth, drawHeight}},
        false);
    const double yInvisible = heightCos >= 0 ? center.y : center.y + heightCos;
    paths.invisible.append(
        FigureShape{FigureRect{center.x - radius, yInvisible, drawWidth, std::abs(heightCos)}},
        false);
}

[[nodiscard]] ButtonPaths railButtonPaths()
{
    constexpr double kOuterDiameter  = 0.0097;
    constexpr double kInnerDiameter  = 0.008;
    const double     baseHeightCos   = 0.002 * kCosR;
    const double     innerHeightCos  = 0.003 * kCosR;
    const double     flangeHeightCos = 0.0015 * kCosR;
    ButtonPaths      paths;
    appendCylinder(paths, {0.25, 0.0254}, kOuterDiameter / 2, baseHeightCos);
    appendCylinder(paths, {0.25, 0.0254 + baseHeightCos}, kInnerDiameter / 2, innerHeightCos);
    appendCylinder(paths, {0.25, 0.0254 + baseHeightCos + innerHeightCos}, kOuterDiameter / 2,
                   flangeHeightCos);
    return paths;
}

TEST(FigurePath, ARailButtonSidePathMatchesJavaSegmentBySegment)
{
    const ButtonPaths paths = railButtonPaths();
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({0.25485, 0.0254}),
        PathSegment::cubicTo({0.25485, 0.027125589280528304},
                             {0.25267858103667934, 0.0285244557831028}, {0.25, 0.0285244557831028}),
        PathSegment::cubicTo({0.24732141896332066, 0.0285244557831028},
                             {0.24515, 0.027125589280528304}, {0.24515, 0.0254}),
        PathSegment::cubicTo({0.24515, 0.02367441071947169},
                             {0.24732141896332066, 0.022275544216897196},
                             {0.25, 0.022275544216897196}),
        PathSegment::cubicTo({0.25267858103667934, 0.022275544216897196},
                             {0.25485, 0.02367441071947169}, {0.25485, 0.0254}),
        PathSegment::close(),
        PathSegment::moveTo({0.24515, 0.0254}),
        PathSegment::lineTo({0.24515, 0.026929684374568975}),
        PathSegment::moveTo({0.25485, 0.0254}),
        PathSegment::lineTo({0.25485, 0.026929684374568975}),
        PathSegment::moveTo({0.25485, 0.026929684374568975}),
        PathSegment::cubicTo({0.25485, 0.02865527365509728},
                             {0.25267858103667934, 0.030054140157671775},
                             {0.25, 0.030054140157671775}),
        PathSegment::cubicTo({0.24732141896332066, 0.030054140157671775},
                             {0.24515, 0.02865527365509728}, {0.24515, 0.026929684374568975}),
        PathSegment::cubicTo({0.24515, 0.025204095094040667},
                             {0.24732141896332066, 0.023805228591466172},
                             {0.25, 0.023805228591466172}),
        PathSegment::cubicTo({0.25267858103667934, 0.023805228591466172},
                             {0.25485, 0.025204095094040667}, {0.25485, 0.026929684374568975}),
        PathSegment::close(),
        PathSegment::moveTo({0.254, 0.026929684374568975}),
        PathSegment::cubicTo({0.254, 0.02835285079149954},
                             {0.2522091389993232, 0.02950655512351974},
                             {0.25, 0.02950655512351974}),
        PathSegment::cubicTo({0.24779086100067682, 0.02950655512351974},
                             {0.246, 0.02835285079149954}, {0.246, 0.026929684374568975}),
        PathSegment::cubicTo({0.246, 0.025506517957638412},
                             {0.24779086100067682, 0.02435281362561821},
                             {0.25, 0.02435281362561821}),
        PathSegment::cubicTo({0.2522091389993232, 0.02435281362561821},
                             {0.254, 0.025506517957638412}, {0.254, 0.026929684374568975}),
        PathSegment::close(),
        PathSegment::moveTo({0.246, 0.026929684374568975}),
        PathSegment::lineTo({0.246, 0.02922421093642244}),
        PathSegment::moveTo({0.254, 0.026929684374568975}),
        PathSegment::lineTo({0.254, 0.02922421093642244}),
        PathSegment::moveTo({0.254, 0.02922421093642244}),
        PathSegment::cubicTo({0.254, 0.030647377353353003},
                             {0.2522091389993232, 0.031801081685373205},
                             {0.25, 0.031801081685373205}),
        PathSegment::cubicTo({0.24779086100067682, 0.031801081685373205},
                             {0.246, 0.030647377353353003}, {0.246, 0.02922421093642244}),
        PathSegment::cubicTo({0.246, 0.027801044519491877},
                             {0.24779086100067682, 0.026647340187471676},
                             {0.25, 0.026647340187471676}),
        PathSegment::cubicTo({0.2522091389993232, 0.026647340187471676},
                             {0.254, 0.027801044519491877}, {0.254, 0.02922421093642244}),
        PathSegment::close(),
        PathSegment::moveTo({0.25485, 0.02922421093642244}),
        PathSegment::cubicTo({0.25485, 0.030949800216950745},
                             {0.25267858103667934, 0.03234866671952524},
                             {0.25, 0.03234866671952524}),
        PathSegment::cubicTo({0.24732141896332066, 0.03234866671952524},
                             {0.24515, 0.030949800216950745}, {0.24515, 0.02922421093642244}),
        PathSegment::cubicTo({0.24515, 0.02749862165589413},
                             {0.24732141896332066, 0.026099755153319637},
                             {0.25, 0.026099755153319637}),
        PathSegment::cubicTo({0.25267858103667934, 0.026099755153319637},
                             {0.25485, 0.02749862165589413}, {0.25485, 0.02922421093642244}),
        PathSegment::close(),
        PathSegment::moveTo({0.24515, 0.02922421093642244}),
        PathSegment::lineTo({0.24515, 0.030371474217349172}),
        PathSegment::moveTo({0.25485, 0.02922421093642244}),
        PathSegment::lineTo({0.25485, 0.030371474217349172}),
        PathSegment::moveTo({0.25485, 0.030371474217349172}),
        PathSegment::cubicTo({0.25485, 0.03209706349787748},
                             {0.25267858103667934, 0.03349593000045197},
                             {0.25, 0.03349593000045197}),
        PathSegment::cubicTo({0.24732141896332066, 0.03349593000045197},
                             {0.24515, 0.03209706349787748}, {0.24515, 0.030371474217349172}),
        PathSegment::cubicTo({0.24515, 0.028645884936820864},
                             {0.24732141896332066, 0.02724701843424637},
                             {0.25, 0.02724701843424637}),
        PathSegment::cubicTo({0.25267858103667934, 0.02724701843424637},
                             {0.25485, 0.028645884936820864}, {0.25485, 0.030371474217349172}),
        PathSegment::close(),
    };
    const Rectangle2D                     expectedBounds{.x      = 0.24515,
                                                         .y      = 0.022275544216897196,
                                                         .width  = 0.009700000000000014,
                                                         .height = 0.011220385783554776};
    constexpr std::array<ContainsCase, 4> kCases{{
        {{0.25, 0.0254}, true},
        {{0.25, 0.0262}, true},
        {{0.2452, 0.0262}, false},
        {{0.25, 0.02745}, true},
    }};
    // PINS-END
    EXPECT_EQ(paths.visible.segments(), expected);
    EXPECT_EQ(paths.visible.bounds(), expectedBounds);
    EXPECT_TRUE(containsAsJava(FigureShape{paths.visible}, kCases));
}

TEST(FigurePath, ARailButtonsInvisibleRectanglesMatchJava)
{
    const ButtonPaths paths = railButtonPaths();
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({0.24515, 0.0254}),
        PathSegment::lineTo({0.25485, 0.0254}),
        PathSegment::lineTo({0.25485, 0.026929684374568975}),
        PathSegment::lineTo({0.24515, 0.026929684374568975}),
        PathSegment::lineTo({0.24515, 0.0254}),
        PathSegment::close(),
        PathSegment::moveTo({0.246, 0.026929684374568975}),
        PathSegment::lineTo({0.254, 0.026929684374568975}),
        PathSegment::lineTo({0.254, 0.02922421093642244}),
        PathSegment::lineTo({0.246, 0.02922421093642244}),
        PathSegment::lineTo({0.246, 0.026929684374568975}),
        PathSegment::close(),
        PathSegment::moveTo({0.24515, 0.02922421093642244}),
        PathSegment::lineTo({0.25485, 0.02922421093642244}),
        PathSegment::lineTo({0.25485, 0.030371474217349172}),
        PathSegment::lineTo({0.24515, 0.030371474217349172}),
        PathSegment::lineTo({0.24515, 0.02922421093642244}),
        PathSegment::close(),
    };
    const Rectangle2D expectedBounds{
        .x = 0.24515, .y = 0.0254, .width = 0.009700000000000014, .height = 0.004971474217349173};
    constexpr std::array<ContainsCase, 6> kCases{{
        {{0.25, 0.026}, true},
        {{0.25, 0.0254}, true},
        {{0.2452, 0.0262}, true},
        {{0.2449, 0.0262}, false},
        {{0.25, 0.031}, false},
        {{0.25, 0.0295}, true},
    }};
    // PINS-END
    EXPECT_EQ(paths.invisible.segments(), expected);
    EXPECT_EQ(paths.invisible.bounds(), expectedBounds);
    EXPECT_TRUE(containsAsJava(FigureShape{paths.invisible}, kCases));
}

}  // namespace
