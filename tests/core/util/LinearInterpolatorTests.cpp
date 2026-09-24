#include "QtRocket/util/LinearInterpolator.h"

#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::BugError;
using QtRocket::LinearInterpolator;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// LinearInterpolatorTest.oldMainTest
TEST(LinearInterpolator, OldMainTest)
{
    const LinearInterpolator interpolator({1, 1.5, 2, 4, 5}, {0, 1, 0, 2, 2});

    constexpr std::array<double, 61> kAnswer = {
        /* x=0 */ 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00,
        /* x=1 */ 0.00, 0.20, 0.40, 0.60, 0.80, 1.00, 0.80, 0.60, 0.40, 0.20,
        /* x=2 */ 0.00, 0.10, 0.20, 0.30, 0.40, 0.50, 0.60, 0.70, 0.80, 0.90,
        /* x=3 */ 1.00, 1.10, 1.20, 1.30, 1.40, 1.50, 1.60, 1.70, 1.80, 1.90,
        /* x=4 */ 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00,
        /* x=5 */ 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00,
        /* x=6 */ 2.00};

    double x = 0;
    for (const double v : kAnswer)
    {
        EXPECT_NEAR(interpolator.getValue(x), v, 0.01) << "Answer wrong for x = " << x;
        x += 0.1;
    }
}

TEST(LinearInterpolator, IsEmptyByDefault)
{
    const LinearInterpolator interpolator;
    EXPECT_TRUE(interpolator.empty());
    EXPECT_EQ(interpolator.size(), 0U);
    EXPECT_TRUE(interpolator.xPoints().empty());
}

TEST(LinearInterpolator, ThrowsWithoutPoints)
{
    const LinearInterpolator interpolator;
    EXPECT_THROW(static_cast<void>(interpolator.getValue(1.0)), BugError);
}

TEST(LinearInterpolator, ReturnsThePointsThemselvesExactly)
{
    const LinearInterpolator interpolator({1, 1.5, 2}, {0.1, 0.7, 0.3});
    EXPECT_DOUBLE_EQ(interpolator.getValue(1), 0.1);
    EXPECT_DOUBLE_EQ(interpolator.getValue(1.5), 0.7);
    EXPECT_DOUBLE_EQ(interpolator.getValue(2), 0.3);
}

TEST(LinearInterpolator, InterpolatesLinearlyBetweenPoints)
{
    const LinearInterpolator interpolator({0, 2, 4}, {0, 4, -4});
    EXPECT_NEAR(interpolator.getValue(0.5), 1.0, 1e-12);
    EXPECT_NEAR(interpolator.getValue(1.0), 2.0, 1e-12);
    EXPECT_NEAR(interpolator.getValue(3.0), 0.0, 1e-12);
    EXPECT_NEAR(interpolator.getValue(3.75), -3.0, 1e-12);
}

TEST(LinearInterpolator, HoldsTheEndValuesOutsideTheRange)
{
    const LinearInterpolator interpolator({1, 2, 3}, {10, 20, 30});
    EXPECT_DOUBLE_EQ(interpolator.getValue(-1e9), 10);
    EXPECT_DOUBLE_EQ(interpolator.getValue(0.999), 10);
    EXPECT_DOUBLE_EQ(interpolator.getValue(3.001), 30);
    EXPECT_DOUBLE_EQ(interpolator.getValue(1e9), 30);
    EXPECT_DOUBLE_EQ(interpolator.getValue(std::numeric_limits<double>::infinity()), 30);
    EXPECT_DOUBLE_EQ(interpolator.getValue(-std::numeric_limits<double>::infinity()), 10);
}

TEST(LinearInterpolator, NanIsOrderedAfterEveryPointLikeJava)
{
    // Double.compareTo puts NaN after every number, so OpenRocket returns the last value for it.
    const LinearInterpolator interpolator({1, 2, 3}, {10, 20, 30});
    EXPECT_DOUBLE_EQ(interpolator.getValue(kNaN), 30);
}

TEST(LinearInterpolator, NanKeyIsOrderedLastAndEqualToItself)
{
    // Double.compareTo makes NaN one key, greater than every number, so TreeMap keeps it last
    // and a second NaN point replaces it.
    LinearInterpolator interpolator({1, 2}, {10, 20});
    interpolator.addPoint(kNaN, 7);
    interpolator.addPoint(kNaN, 8);
    ASSERT_EQ(interpolator.size(), 3U);
    EXPECT_TRUE(std::isnan(interpolator.xPoints()[2]));
    EXPECT_DOUBLE_EQ(interpolator.getValue(kNaN), 8);
    EXPECT_DOUBLE_EQ(interpolator.getValue(1.5), 15);
    // Past the last number the NaN point is the ceiling, so the interpolation is NaN, as in Java.
    EXPECT_TRUE(std::isnan(interpolator.getValue(3)));
}

TEST(LinearInterpolator, NegativeZeroIsAPointBelowPositiveZeroLikeJava)
{
    // Double.compareTo orders -0.0 before 0.0, so TreeMap keeps them as two points.
    LinearInterpolator interpolator;
    interpolator.addPoint(-0.0, 1);
    interpolator.addPoint(0.0, 2);
    ASSERT_EQ(interpolator.size(), 2U);
    EXPECT_TRUE(std::signbit(interpolator.xPoints()[0]));
    EXPECT_FALSE(std::signbit(interpolator.xPoints()[1]));
    EXPECT_DOUBLE_EQ(interpolator.getValue(-0.0), 1);
    EXPECT_DOUBLE_EQ(interpolator.getValue(0.0), 2);
}

TEST(LinearInterpolator, NegativeZeroBelowAFirstPointAtZeroHoldsItsValue)
{
    // Java's TreeMap.subMap(0.0, -0.0) throws IllegalArgumentException here; the port holds the
    // first value like any x below the first point.
    const LinearInterpolator interpolator({0.0, 1.0}, {5, 6});
    EXPECT_DOUBLE_EQ(interpolator.getValue(-0.0), 5);
}

TEST(LinearInterpolator, SinglePointIsConstantEverywhere)
{
    LinearInterpolator interpolator;
    interpolator.addPoint(5, 42);
    EXPECT_DOUBLE_EQ(interpolator.getValue(-100), 42);
    EXPECT_DOUBLE_EQ(interpolator.getValue(5), 42);
    EXPECT_DOUBLE_EQ(interpolator.getValue(100), 42);
}

TEST(LinearInterpolator, AddPointReplacesAnExistingX)
{
    LinearInterpolator interpolator({1, 2}, {10, 20});
    interpolator.addPoint(2, 200);
    EXPECT_EQ(interpolator.size(), 2U);
    EXPECT_DOUBLE_EQ(interpolator.getValue(2), 200);
    EXPECT_DOUBLE_EQ(interpolator.getValue(1.5), 105);
}

TEST(LinearInterpolator, XPointsAreSortedAndUnique)
{
    LinearInterpolator interpolator;
    interpolator.addPoints({3, 1, 2, 1}, {30, 10, 20, 11});
    const std::vector<double> expected{1, 2, 3};
    EXPECT_EQ(interpolator.xPoints(), expected);
    EXPECT_DOUBLE_EQ(interpolator.getValue(1), 11);
}

TEST(LinearInterpolator, AddsPointsFromSpans)
{
    const std::vector<double>   x{0, 1};
    const std::array<double, 2> y{5, 6};
    const LinearInterpolator    fromVectors(x, y);
    EXPECT_NEAR(fromVectors.getValue(0.5), 5.5, 1e-12);

    LinearInterpolator added;
    added.addPoints(x, y);
    EXPECT_EQ(added.xPoints(), x);
}

TEST(LinearInterpolator, RejectsMismatchedLengths)
{
    EXPECT_THROW(LinearInterpolator({1, 2, 3}, {1, 2}), BugError);

    LinearInterpolator interpolator({1, 2}, {10, 20});
    EXPECT_THROW(interpolator.addPoints({3}, {30, 40}), BugError);
    EXPECT_EQ(interpolator.size(), 2U) << "nothing is added when the lengths differ";
}

TEST(LinearInterpolator, CopyIsIndependent)
{
    const LinearInterpolator original({1, 2}, {10, 20});
    LinearInterpolator       copy = original;
    copy.addPoint(3, 30);
    EXPECT_EQ(original.size(), 2U);
    EXPECT_EQ(copy.size(), 3U);
    EXPECT_DOUBLE_EQ(original.getValue(3), 20);
    EXPECT_DOUBLE_EQ(copy.getValue(3), 30);
}

}  // namespace
