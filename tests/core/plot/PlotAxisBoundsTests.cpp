#include "QtRocket/plot/PlotAxisBounds.h"

#include <cmath>
#include <limits>
#include <type_traits>

#include <gtest/gtest.h>

namespace
{

using QtRocket::PlotAxisBounds;

constexpr double kNaN      = std::numeric_limits<double>::quiet_NaN();
constexpr double kInfinity = std::numeric_limits<double>::infinity();

// A value type: a copy is Java's clone().
static_assert(std::is_trivially_copyable_v<PlotAxisBounds>);

// OpenRocket has no test of Axis; these follow Axis.java line by line.

TEST(PlotAxisBounds, StartsWithoutBounds)
{
    const PlotAxisBounds bounds;
    EXPECT_TRUE(std::isnan(bounds.getMinValue()));
    EXPECT_TRUE(std::isnan(bounds.getMaxValue()));
    EXPECT_TRUE(std::isnan(bounds.getRangeLength()));
}

TEST(PlotAxisBounds, TheFirstBoundIsBothEnds)
{
    PlotAxisBounds bounds;
    bounds.addBound(2.5);
    EXPECT_EQ(bounds.getMinValue(), 2.5);
    EXPECT_EQ(bounds.getMaxValue(), 2.5);
    EXPECT_EQ(bounds.getRangeLength(), 0.0);
}

TEST(PlotAxisBounds, WidensToEveryBound)
{
    PlotAxisBounds bounds;
    bounds.addBound(2.5);
    bounds.addBound(-1.25);
    bounds.addBound(10.0);
    bounds.addBound(3.0);
    EXPECT_EQ(bounds.getMinValue(), -1.25);
    EXPECT_EQ(bounds.getMaxValue(), 10.0);
    EXPECT_EQ(bounds.getRangeLength(), 11.25);

    bounds.addBound(-kInfinity);
    EXPECT_EQ(bounds.getMinValue(), -kInfinity);
    EXPECT_EQ(bounds.getMaxValue(), 10.0);
}

TEST(PlotAxisBounds, ANaNBoundChangesNothingOnceThereIsABound)
{
    PlotAxisBounds bounds;
    bounds.addBound(kNaN);
    EXPECT_TRUE(std::isnan(bounds.getMinValue()));
    EXPECT_TRUE(std::isnan(bounds.getMaxValue()));

    bounds.addBound(4.0);
    bounds.addBound(kNaN);
    EXPECT_EQ(bounds.getMinValue(), 4.0);
    EXPECT_EQ(bounds.getMaxValue(), 4.0);
}

TEST(PlotAxisBounds, ASignedZeroDoesNotReplaceTheOther)
{
    // value < minValue and value > maxValue: -0.0 and 0.0 compare equal.
    PlotAxisBounds bounds;
    bounds.addBound(0.0);
    bounds.addBound(-0.0);
    EXPECT_FALSE(std::signbit(bounds.getMinValue()));
    EXPECT_FALSE(std::signbit(bounds.getMaxValue()));

    PlotAxisBounds negative;
    negative.addBound(-0.0);
    negative.addBound(0.0);
    EXPECT_TRUE(std::signbit(negative.getMinValue()));
    EXPECT_TRUE(std::signbit(negative.getMaxValue()));
}

TEST(PlotAxisBounds, ResetForgetsTheBounds)
{
    PlotAxisBounds bounds;
    bounds.addBound(1.0);
    bounds.addBound(2.0);
    const PlotAxisBounds copy = bounds;

    bounds.reset();
    EXPECT_TRUE(std::isnan(bounds.getMinValue()));
    EXPECT_TRUE(std::isnan(bounds.getMaxValue()));
    bounds.addBound(-3.0);
    EXPECT_EQ(bounds.getMinValue(), -3.0);
    EXPECT_EQ(bounds.getMaxValue(), -3.0);

    // The copy keeps its own bounds (Java: clone()).
    EXPECT_EQ(copy.getMinValue(), 1.0);
    EXPECT_EQ(copy.getMaxValue(), 2.0);
}

}  // namespace
