#include "QtRocket/figure/FigureRect.h"

#include <array>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/util/Geometry2D.h"
#include "figure/FigureTestSupport.h"

namespace
{

using QtRocket::FigurePath;
using QtRocket::FigureRect;
using QtRocket::FigureShape;
using QtRocket::PathSegment;
using QtRocket::Rectangle2D;
using QtRocket::Test::containsAsJava;
using QtRocket::Test::ContainsCase;

// Every value below was printed by java.awt.geom (JDK 17, the probe ShapeProbe.java of the port's
// scratch directory) for the same rectangles: Rectangle2D.Double's PathIterator, getBounds2D and
// contains. The iterator only adds, so the segments are pinned exactly.

TEST(FigureRect, SegmentsFollowRectIterator)
{
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({0.1, -0.0125}), PathSegment::lineTo({0.4, -0.0125}),
        PathSegment::lineTo({0.4, 0.0125}),  PathSegment::lineTo({0.1, 0.0125}),
        PathSegment::lineTo({0.1, -0.0125}), PathSegment::close(),
    };
    // PINS-END
    const FigureRect rect{0.1, -0.0125, 0.3, 0.025};
    EXPECT_EQ(rect.segments(), expected);
    EXPECT_EQ(rect.bounds(), (Rectangle2D{.x = 0.1, .y = -0.0125, .width = 0.3, .height = 0.025}));
    EXPECT_FALSE(rect.isEmpty());
}

TEST(FigureRect, AZeroExtentStillIteratesANegativeOneDoesNot)
{
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> zeroWidth{
        PathSegment::moveTo({1.0, 2.0}), PathSegment::lineTo({1.0, 2.0}),
        PathSegment::lineTo({1.0, 5.0}), PathSegment::lineTo({1.0, 5.0}),
        PathSegment::lineTo({1.0, 2.0}), PathSegment::close(),
    };
    // PINS-END
    EXPECT_EQ((FigureRect{1, 2, 0, 3}.segments()), zeroWidth);
    EXPECT_TRUE((FigureRect{1, 2, 0, 3}.isEmpty()));
    EXPECT_TRUE((FigureRect{1, 2, -1, 3}.segments().empty()));
    EXPECT_TRUE((FigureRect{1, 2, 1, -3}.segments().empty()));
    // The bounds keep a negative extent as given.
    EXPECT_EQ((FigureRect{1, 2, -1, 3}.bounds()),
              (Rectangle2D{.x = 1.0, .y = 2.0, .width = -1.0, .height = 3.0}));
    EXPECT_EQ((FigureRect{1, 2, 1, -3}.bounds()),
              (Rectangle2D{.x = 1.0, .y = 2.0, .width = 1.0, .height = -3.0}));
}

TEST(FigureRect, ContainsIncludesTheLowerEdgesOnly)
{
    // PINS-BEGIN (ShapeProbe.java)
    constexpr std::array<ContainsCase, 7> kCases{{
        {{0.1, -0.0125}, true},
        {{0.4, 0.0}, false},
        {{0.25, 0.0125}, false},
        {{0.25, 0.0124}, true},
        {{0.0999, 0.0}, false},
        {{0.3999, 0.0124}, true},
        {{0.25, -0.0126}, false},
    }};
    // PINS-END
    EXPECT_TRUE(containsAsJava(FigureShape{FigureRect{0.1, -0.0125, 0.3, 0.025}}, kCases));
}

TEST(FigureRect, ContainsNothingWhenEmptyOrNotFinite)
{
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE((FigureRect{0, 0, 0, 1}.contains({0, 0.5})));
    EXPECT_FALSE((FigureRect{0, 0, -1, 1}.contains({-0.5, 0.5})));
    EXPECT_FALSE((FigureRect{0, 0, 1, 1}.contains({kNaN, 0.5})));
    EXPECT_FALSE((FigureRect{0, 0, 1, 1}.contains({0.5, kNaN})));
    EXPECT_FALSE((FigureRect{kNaN, 0, 1, 1}.contains({0.5, 0.5})));
}

TEST(FigureRect, FloatSegmentsAddInFloat)
{
    // RectIterator.currentSegment(float[]) adds the float-rounded width to the float-rounded
    // corner. For x = 0.0013 and w = 0.0213 that sum (0.022599998861551285) is not the rounded
    // double sum (0.022600000724196434): what Path2D.Float.append(rect) stores.
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> expected{
        PathSegment::moveTo({0.0013000000035390258, 0.0013000000035390258}),
        PathSegment::lineTo({0.022599998861551285, 0.0013000000035390258}),
        PathSegment::lineTo({0.022599998861551285, 0.022599998861551285}),
        PathSegment::lineTo({0.0013000000035390258, 0.022599998861551285}),
        PathSegment::lineTo({0.0013000000035390258, 0.0013000000035390258}),
        PathSegment::close(),
    };
    const Rectangle2D expectedBounds{.x      = 0.0013000000035390258,
                                     .y      = 0.0013000000035390258,
                                     .width  = 0.021299999207258224,
                                     .height = 0.021299999207258224};
    // PINS-END
    const FigureRect rect{0.0013, 0.0013, 0.0213, 0.0213};
    EXPECT_EQ(rect.floatSegments(), expected);
    EXPECT_NE(static_cast<double>(static_cast<float>(0.0013 + 0.0213)), 0.022599998861551285);

    FigurePath path{FigurePath::Precision::FLOAT};
    path.append(FigureShape{rect}, false);
    EXPECT_EQ(path.segments(), expected);
    EXPECT_EQ(path.bounds(), expectedBounds);
    EXPECT_TRUE(rect.floatSegments().empty() == rect.segments().empty());
    EXPECT_TRUE((FigureRect{0, 0, -1, 1}.floatSegments().empty()));
}

}  // namespace
