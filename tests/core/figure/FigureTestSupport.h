#pragma once

// Comparisons for the figure's shapes: segment lists and rectangles within a tolerance (for
// values Java computes with sin, cos or atan2, whose last bits differ between Java and the C
// libraries), and tables of points with Java's contains() answers. Exact comparisons need none of
// this: EXPECT_EQ on two std::vector<PathSegment> prints both lists (PathSegment's operator<<).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket::Test
{

/// A point and whether Java's Shape.contains() finds it inside.
struct ContainsCase
{
    constexpr ContainsCase(Point2D p, bool isInside) noexcept : point(p), inside(isInside) { }

    Point2D point;
    bool    inside;
};

/// The relative tolerance for coordinates Java computes with transcendental functions: a few
/// units in the last place of a value of order 1, with an absolute floor of the same size for
/// values that are zero but for rounding.
inline constexpr double kTranscendentalTolerance = 1e-14;

/// The relative tolerance for a Path2D.Float's coordinates whose double inputs went through sin or
/// cos (a cant rotation, an elliptical fin, a rotated figure): one unit in the last place of a
/// float of order 1, since a double that differs from Java's in its last bit can round to the
/// neighbouring float.
inline constexpr double kFloatTolerance = 0x1p-23;

/// True when @p actual is within @p tolerance of @p expected, relative to max(1, |expected|).
[[nodiscard]] inline bool figureValueNear(double actual, double expected, double tolerance)
{
    return std::abs(actual - expected) <= tolerance * std::max(1.0, std::abs(expected));
}

/// Success when both lists have the same segment types and every used coordinate is within
/// @p tolerance (figureValueNear), else a failure naming the first difference and both lists.
[[nodiscard]] inline ::testing::AssertionResult segmentsNear(
    const std::vector<PathSegment>& actual, const std::vector<PathSegment>& expected,
    double tolerance = kTranscendentalTolerance)
{
    const auto describe = [&] {
        std::ostringstream os;
        os << "\n  actual:  ";
        for (const PathSegment& s : actual)
        {
            os << s << "; ";
        }
        os << "\n  expected: ";
        for (const PathSegment& s : expected)
        {
            os << s << "; ";
        }
        return os.str();
    };
    if (actual.size() != expected.size())
    {
        return ::testing::AssertionFailure()
               << actual.size() << " segments, expected " << expected.size() << describe();
    }
    for (std::size_t i = 0; i < actual.size(); ++i)
    {
        if (actual[i].type != expected[i].type)
        {
            return ::testing::AssertionFailure()
                   << "segment " << i << " has another type" << describe();
        }
        const auto count = static_cast<std::size_t>(actual[i].pointCount());
        for (std::size_t p = 0; p < count; ++p)
        {
            const Point2D a = actual[i].points.at(p);
            const Point2D e = expected[i].points.at(p);
            if (!figureValueNear(a.x, e.x, tolerance) || !figureValueNear(a.y, e.y, tolerance))
            {
                return ::testing::AssertionFailure()
                       << std::format("segment {} point {} is {},{}, expected {},{}", i, p, a.x,
                                      a.y, e.x, e.y)
                       << describe();
            }
        }
    }
    return ::testing::AssertionSuccess();
}

/// Success when every field of @p actual is within @p tolerance of @p expected's.
[[nodiscard]] inline ::testing::AssertionResult rectanglesNear(
    const Rectangle2D& actual, const Rectangle2D& expected,
    double tolerance = kTranscendentalTolerance)
{
    if (figureValueNear(actual.x, expected.x, tolerance) &&
        figureValueNear(actual.y, expected.y, tolerance) &&
        figureValueNear(actual.width, expected.width, tolerance) &&
        figureValueNear(actual.height, expected.height, tolerance))
    {
        return ::testing::AssertionSuccess();
    }
    std::ostringstream os;
    os << actual << ", expected " << expected;
    return ::testing::AssertionFailure() << os.str();
}

/// Success when @p shape's contains() answers every case as Java does, else a failure listing
/// the points it answers differently.
[[nodiscard]] inline ::testing::AssertionResult containsAsJava(const FigureShape&            shape,
                                                               std::span<const ContainsCase> cases)
{
    std::string wrong;
    for (const ContainsCase& c : cases)
    {
        if (shape.contains(c.point) != c.inside)
        {
            wrong += std::format(" ({},{}) should be {};", c.point.x, c.point.y,
                                 c.inside ? "inside" : "outside");
        }
    }
    if (wrong.empty())
    {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << "contains() differs from Java at" << wrong;
}

}  // namespace QtRocket::Test
