#include "QtRocket/util/MathUtil.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Coordinate.h"

namespace
{

namespace MathUtil = QtRocket::MathUtil;
using QtRocket::Coordinate;

constexpr double kEps = 0.00000000001;
constexpr double kPi  = std::numbers::pi;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// ---- Ported from MathUtilTest.java ----

TEST(MathUtil, MiscMath)
{
    EXPECT_NEAR(kPi * kPi, MathUtil::pow2(kPi), kEps);
    EXPECT_NEAR(kPi * kPi * kPi, MathUtil::pow3(kPi), kEps);
    EXPECT_NEAR(kPi * kPi * kPi * kPi, MathUtil::pow4(kPi), kEps);

    EXPECT_EQ(1.0, MathUtil::clamp(0.9999, 1.0, 2.0));
    EXPECT_EQ(1.23, MathUtil::clamp(1.23, 1.0, 2.0));
    EXPECT_EQ(2.0, MathUtil::clamp(2 + (kEps / 100), 1.0, 2.0));

    // Java's clamp(float, float, float) is not ported (see MathUtil.h); its three assertions run
    // through the double overload with the same float values.
    EXPECT_EQ(1.0, MathUtil::clamp(static_cast<double>(0.9999F), 1.0, 2.0));
    EXPECT_EQ(static_cast<double>(1.23F), MathUtil::clamp(static_cast<double>(1.23F), 1.0, 2.0));
    EXPECT_EQ(2.0, MathUtil::clamp(static_cast<double>(2.0001F), 1.0, 2.0));

    EXPECT_EQ(1, MathUtil::clamp(-3, 1, 5));
    EXPECT_EQ(3, MathUtil::clamp(3, 1, 5));
    EXPECT_EQ(5, MathUtil::clamp(6, 1, 5));

    EXPECT_NEAR(-1.0, MathUtil::sign(-kInf), kEps);
    EXPECT_NEAR(-1.0, MathUtil::sign(-100), kEps);
    EXPECT_NEAR(-1.0, MathUtil::sign(std::nextafter(0.0, -1.0)), kEps);
    EXPECT_NEAR(1.0, MathUtil::sign(std::nextafter(0.0, 1.0)), kEps);
    EXPECT_NEAR(1.0, MathUtil::sign(100), kEps);
    EXPECT_NEAR(1.0, MathUtil::sign(kInf), kEps);
}

TEST(MathUtil, Hypot)
{
    // Java uses Math.random(); a fixed seed keeps this deterministic, the values only feed a
    // comparison against the library hypot.
    // NOLINTNEXTLINE(bugprone-random-generator-seed)
    std::mt19937                           rng{20240923};
    std::uniform_real_distribution<double> random(0.0, 1.0);

    for (int i = 0; i < 10000; i++)
    {
        const double x = (random(rng) * 100) - 50;
        const double y = (random(rng) * i) - (static_cast<double>(i) / 2);
        const double z = std::hypot(x, y);
        EXPECT_NEAR(z, MathUtil::hypot(x, y), kEps);
    }
}

TEST(MathUtil, Reduce)
{
    // NOLINTNEXTLINE(bugprone-random-generator-seed)
    std::mt19937                           rng{20240923};
    std::uniform_real_distribution<double> random(0.0, 1.0);

    for (int i = -1000; i < 1000; i++)
    {
        const double angle = random(rng) * 2 * kPi;
        const double shift = angle + (i * 2 * kPi);
        EXPECT_NEAR(angle, MathUtil::reduce2Pi(shift), kEps) << "i=" << i;
    }

    for (int i = -1000; i < 1000; i++)
    {
        const double angle = (random(rng) * 2 * kPi) - kPi;
        const double shift = angle + (i * 2 * kPi);
        EXPECT_NEAR(angle, MathUtil::reducePi(shift), kEps) << "i=" << i;
    }
}

TEST(MathUtil, MinMax)
{
    const double nextUp = std::nextafter(1.0, 2.0);

    EXPECT_EQ(1.0, MathUtil::min(1.0, nextUp));
    EXPECT_EQ(1.0, MathUtil::min(1.0, kInf));
    EXPECT_EQ(1.0, MathUtil::min(kNaN, 1.0));
    EXPECT_EQ(1.0, MathUtil::min(1.0, kNaN));
    EXPECT_TRUE(std::isnan(MathUtil::min(kNaN, kNaN)));

    EXPECT_EQ(nextUp, MathUtil::max(1.0, nextUp));
    EXPECT_EQ(kInf, MathUtil::max(1.0, kInf));
    EXPECT_EQ(1.0, MathUtil::max(kNaN, 1.0));
    EXPECT_EQ(1.0, MathUtil::max(1.0, kNaN));
    EXPECT_TRUE(std::isnan(MathUtil::max(kNaN, kNaN)));

    EXPECT_EQ(1.0, MathUtil::min(1.0, 2.0, 3.0));
    EXPECT_EQ(1.0, MathUtil::min(1.0, kNaN, kNaN));
    EXPECT_EQ(1.0, MathUtil::min(kNaN, 1.0, kNaN));
    EXPECT_EQ(1.0, MathUtil::min(kNaN, kNaN, 1.0));
    EXPECT_EQ(1.0, MathUtil::min(2.0, kNaN, 1.0));
    EXPECT_EQ(1.0, MathUtil::min(1.0, 2.0, kNaN));
    EXPECT_EQ(1.0, MathUtil::min(kNaN, 2.0, 1.0));

    EXPECT_EQ(3.0, MathUtil::max(1.0, 3.0, 2.0));
    EXPECT_EQ(1.0, MathUtil::max(1.0, kNaN, kNaN));
    EXPECT_EQ(1.0, MathUtil::max(kNaN, 1.0, kNaN));
    EXPECT_EQ(1.0, MathUtil::max(kNaN, kNaN, 1.0));
    EXPECT_EQ(2.0, MathUtil::max(2.0, kNaN, 1.0));
    EXPECT_EQ(2.0, MathUtil::max(1.0, 2.0, kNaN));
    EXPECT_EQ(2.0, MathUtil::max(kNaN, 2.0, 1.0));

    EXPECT_EQ(1.0, MathUtil::min(1.0, 2.0, 3.0, 4.0));
    EXPECT_EQ(1.0, MathUtil::min(1.0, kNaN, kNaN, kNaN));
    EXPECT_EQ(1.0, MathUtil::min(kNaN, 1.0, kNaN, kNaN));
    EXPECT_EQ(1.0, MathUtil::min(kNaN, kNaN, 1.0, kNaN));
    EXPECT_EQ(1.0, MathUtil::min(2.0, kNaN, 1.0, kNaN));
    EXPECT_EQ(1.0, MathUtil::min(2.0, kNaN, kNaN, 1.0));
    EXPECT_EQ(1.0, MathUtil::min(1.0, 2.0, kNaN, 3.0));
    EXPECT_EQ(1.0, MathUtil::min(kNaN, 2.0, 3.0, 1.0));
}

TEST(MathUtil, Map)
{
    EXPECT_NEAR(1.0, MathUtil::map(1.0, 0.0, 5.0, -1.0, 9.0), kEps);
    EXPECT_NEAR(7.0, MathUtil::map(1.0, 5.0, 0.0, -1.0, 9.0), kEps);
    EXPECT_NEAR(7.0, MathUtil::map(1.0, 0.0, 5.0, 9.0, -1.0), kEps);
    EXPECT_NEAR(6.0, MathUtil::map(6.0, 0.0, 5.0, std::nextafter(6.0, 7.0), 6.0), kEps);
    EXPECT_NEAR(6.0, MathUtil::map(6.0, 0.0, 0.0, std::nextafter(6.0, 7.0), 6.0), kEps);
    EXPECT_THROW(static_cast<void>(MathUtil::map(6.0, 1.0, std::nextafter(1.0, 2.0), 1.0, 2.0)),
                 std::invalid_argument);

    EXPECT_NEAR(7.0, MathUtil::map(std::nextafter(1.0, 2.0), 0.0, 5.0, 9.0, -1.0), kEps);
}

TEST(MathUtil, MapCoordinate)
{
    const Coordinate mapped =
        MathUtil::map(1.0, 0.0, 5.0, Coordinate(0, 1, 2, 3), Coordinate(4, 6, 0, 8));
    EXPECT_EQ(Coordinate(0.8, 2.0, 1.6, 4.0), mapped);
}

// The body of the Java test's first loop, in two halves (clang-tidy's cognitive-complexity limit
// allows about six assertions per helper): values within kEpsilon/2 of zero compare like zero, as
// the first argument ...
void expectEqualsWithZeroFirst(double zero)
{
    EXPECT_TRUE(MathUtil::equals(zero, MathUtil::kEpsilon / 3)) << "zero=" << zero;
    EXPECT_TRUE(MathUtil::equals(zero, -MathUtil::kEpsilon / 3)) << "zero=" << zero;
    EXPECT_FALSE(MathUtil::equals(zero, MathUtil::kEpsilon * 2)) << "zero=" << zero;
    EXPECT_FALSE(MathUtil::equals(zero, -MathUtil::kEpsilon * 2)) << "zero=" << zero;
}

// ... and as the second.
void expectEqualsWithZeroSecond(double zero)
{
    EXPECT_TRUE(MathUtil::equals(MathUtil::kEpsilon / 3, zero)) << "zero=" << zero;
    EXPECT_TRUE(MathUtil::equals(-MathUtil::kEpsilon / 3, zero)) << "zero=" << zero;
    EXPECT_FALSE(MathUtil::equals(MathUtil::kEpsilon * 2, zero)) << "zero=" << zero;
    EXPECT_FALSE(MathUtil::equals(-MathUtil::kEpsilon * 2, zero)) << "zero=" << zero;
}

// The body of the Java test's second loop: the tolerance is relative, so huge values still compare.
void expectEqualsAtLargeValue(double value)
{
    EXPECT_TRUE(MathUtil::equals(value, value + 1)) << "value=" << value;
    EXPECT_TRUE(MathUtil::equals(value, std::nextafter(value, kInf))) << "value=" << value;
    EXPECT_TRUE(MathUtil::equals(value, value * (1 + MathUtil::kEpsilon))) << "value=" << value;
}

TEST(MathUtil, Equals)
{
    EXPECT_TRUE(MathUtil::equals(1.0, 1.0 + (MathUtil::kEpsilon / 3)));
    EXPECT_FALSE(MathUtil::equals(1.0, 1.0 + (MathUtil::kEpsilon * 2)));
    EXPECT_TRUE(MathUtil::equals(-1.0, -1.0 + (MathUtil::kEpsilon / 3)));
    EXPECT_FALSE(MathUtil::equals(-1.0, -1.0 + (MathUtil::kEpsilon * 2)));

    expectEqualsWithZeroFirst(0.0);
    expectEqualsWithZeroSecond(0.0);
    expectEqualsWithZeroFirst(MathUtil::kEpsilon / 10);
    expectEqualsWithZeroSecond(MathUtil::kEpsilon / 10);
    expectEqualsWithZeroFirst(-MathUtil::kEpsilon / 10);
    expectEqualsWithZeroSecond(-MathUtil::kEpsilon / 10);

    expectEqualsAtLargeValue(kPi * 1.0e20);
    expectEqualsAtLargeValue(-kPi * 1.0e20);

    EXPECT_FALSE(MathUtil::equals(kNaN, 0.0));
    EXPECT_FALSE(MathUtil::equals(0.0, kNaN));
    EXPECT_FALSE(MathUtil::equals(kNaN, kNaN));
}

TEST(MathUtil, AverageStddev)
{
    const std::vector<double> ints    = {3, 4, 7, 5};
    const std::vector<double> doubles = {3.4, 2.9, 7.5, 5.43, 2.8, 6.6};

    EXPECT_NEAR(4.75, MathUtil::average(ints), kEps);
    EXPECT_NEAR(1.707825127659933, MathUtil::stddev(ints), kEps);
    EXPECT_NEAR(4.771666666666667, MathUtil::average(doubles), kEps);
    EXPECT_NEAR(2.024454659078999, MathUtil::stddev(doubles), kEps);
}

TEST(MathUtil, Median)
{
    std::vector<double> ints    = {3, 4, 7, 5};
    std::vector<double> doubles = {3.4, 2.9, 7.5, 5.43, 2.8, 6.6};

    EXPECT_NEAR(4.5, MathUtil::median(ints), kEps);
    EXPECT_NEAR(4.415, MathUtil::median(doubles), kEps);

    ints.push_back(9);
    doubles.push_back(10.0);

    EXPECT_NEAR(5, MathUtil::median(ints), kEps);
    EXPECT_NEAR(5.43, MathUtil::median(doubles), kEps);
}

TEST(MathUtil, Interpolate)
{
    // Java passes null lists; an empty span is the closest thing, and gives NaN the same way.
    std::vector<double> x;
    std::vector<double> y = {1.0};

    EXPECT_TRUE(std::isnan(MathUtil::interpolate({}, y, 0.0))) << "Failed to test for domain null";
    EXPECT_TRUE(std::isnan(MathUtil::interpolate(x, y, 0.0))) << "Failed to test for empty domain";

    x = {1.0};
    y = {};

    EXPECT_TRUE(std::isnan(MathUtil::interpolate(x, {}, 0.0))) << "Failed to test for range null";
    EXPECT_TRUE(std::isnan(MathUtil::interpolate(x, y, 0.0))) << "Failed to test for empty range";

    x = {1.0, 2.0};
    y = {15.0, 17.0};

    EXPECT_TRUE(std::isnan(MathUtil::interpolate(x, y, 0.0))) << "Failed to test t out of domain";
    EXPECT_TRUE(std::isnan(MathUtil::interpolate(x, y, 5.0))) << "Failed to test t out of domain";

    EXPECT_NEAR(15.0, MathUtil::interpolate(x, y, 1.0), kEps)
        << "Failed to calculate left endpoint";
    EXPECT_NEAR(17.0, MathUtil::interpolate(x, y, 2.0), kEps)
        << "Failed to calculate right endpoint";
    EXPECT_NEAR(16.0, MathUtil::interpolate(x, y, 1.5), kEps) << "Failed to calculate center";

    x = {0.25, 0.5, 1.0, 2.0};
    y = {0.0, 0.0, 15.0, 17.0};
    EXPECT_NEAR(16.0, MathUtil::interpolate(x, y, 1.5), kEps)
        << "Failed to calculate center with longer list";
}

TEST(MathUtil, SafeSqrtHandlesNegativeInput)
{
    EXPECT_NEAR(0.0, MathUtil::safeSqrt(-1.0e-6), kEps);
    EXPECT_NEAR(5.0, MathUtil::safeSqrt(25.0), kEps);
}

TEST(MathUtil, InterpolateHandlesDegenerateSegment)
{
    const std::array<double, 2> domain  = {1.0, 1.0 + (MathUtil::kEpsilon / 10)};
    const std::array<double, 2> rising  = {0.0, 1.0};
    const std::array<double, 2> falling = {0.0, -1.0};
    const std::array<double, 2> flat    = {1.0, 1.0};

    const double t = 1.0 + (MathUtil::kEpsilon / 20);
    EXPECT_EQ(kInf, MathUtil::interpolate(domain, rising, t));
    EXPECT_EQ(-kInf, MathUtil::interpolate(domain, falling, t));
    EXPECT_NEAR(0.0, MathUtil::interpolate(domain, flat, t), kEps);
}

TEST(MathUtil, InterpolateSimpleDouble)
{
    EXPECT_NEAR(7.5, MathUtil::interpolate(5.0, 10.0, 0.5), kEps);
    EXPECT_NEAR(10.0, MathUtil::interpolate(5.0, 10.0, 1.0), kEps);
    EXPECT_NEAR(5.0, MathUtil::interpolate(5.0, 10.0, 0.0), kEps);
}

TEST(MathUtil, AngleConversionsAreConsistent)
{
    EXPECT_NEAR(kPi, MathUtil::deg2rad(180.0), kEps);
    EXPECT_NEAR(180.0, MathUtil::rad2deg(kPi), kEps);
    EXPECT_NEAR(45.0, MathUtil::rad2deg(MathUtil::deg2rad(45.0)), kEps);
}

TEST(MathUtil, MapToConstantRangeReturnsConstant)
{
    EXPECT_NEAR(42.0, MathUtil::map(10.0, 0.0, 100.0, 42.0, 42.0), kEps);
}

// ---- Behaviour OpenRocket does not test ----

TEST(MathUtil, ConstexprFunctions)
{
    static_assert(MathUtil::kEpsilon == 1e-8);
    static_assert(MathUtil::pow2(3.0) == 9.0);
    static_assert(MathUtil::pow3(2.0) == 8.0);
    static_assert(MathUtil::pow4(2.0) == 16.0);
    static_assert(MathUtil::clamp(5.0, 0.0, 1.0) == 1.0);
    static_assert(MathUtil::clamp(-5.0, 0.0, 1.0) == 0.0);
    static_assert(MathUtil::clamp(0.5, 0.0, 1.0) == 0.5);
    static_assert(MathUtil::clamp(7, 1, 5) == 5);
    static_assert(MathUtil::sign(-2.0) == -1.0);
    static_assert(MathUtil::sign(0.0) == 1.0);
    static_assert(MathUtil::interpolate(5.0, 10.0, 0.5) == 7.5);
    SUCCEED();
}

// True when clamp(x, min, max) compiles for these argument types.
template <typename X, typename Min, typename Max>
concept Clampable = requires(X x, Min min, Max max) { MathUtil::clamp(x, min, max); };

TEST(MathUtil, ClampOverloadsResolveLikeJava)
{
    // Mixed integer/double arguments widen to the double overload; all-integer calls stay integral.
    static_assert(std::is_same_v<decltype(MathUtil::clamp(1.5, 0, 1)), double>);
    static_assert(std::is_same_v<decltype(MathUtil::clamp(1, 0.0, 1.0)), double>);
    static_assert(std::is_same_v<decltype(MathUtil::clamp(1, 0, 1)), int>);
    static_assert(
        std::is_same_v<decltype(MathUtil::clamp(std::size_t{9}, std::size_t{0}, std::size_t{5})),
                       std::size_t>);
    // Integers of mixed types are rejected (the deleted overload) rather than degraded to double.
    static_assert(Clampable<double, int, int>);
    static_assert(Clampable<float, float, float>);
    static_assert(Clampable<int, int, int>);
    static_assert(Clampable<std::size_t, std::size_t, std::size_t>);
    static_assert(!Clampable<std::size_t, int, int>);
    static_assert(!Clampable<int, std::size_t, int>);
    static_assert(!Clampable<int, int, unsigned>);
    EXPECT_EQ(1.0, MathUtil::clamp(1.5, 0, 1));
    EXPECT_EQ(std::size_t{5}, MathUtil::clamp(std::size_t{9}, std::size_t{0}, std::size_t{5}));
    // NaN passes through: neither bound comparison holds.
    EXPECT_TRUE(std::isnan(MathUtil::clamp(kNaN, 0.0, 1.0)));
    EXPECT_EQ(kInf, MathUtil::clamp(kInf, 0.0, kInf));
}

TEST(MathUtil, EqualsWithExplicitEpsilon)
{
    EXPECT_TRUE(MathUtil::equals(100.0, 101.0, 0.02));
    EXPECT_FALSE(MathUtil::equals(100.0, 103.0, 0.02));
    // The tolerance is relative to the second argument.
    EXPECT_TRUE(MathUtil::equals(1.0e6, 1.0e6 + 0.005));
    EXPECT_FALSE(MathUtil::equals(1.0e6 + 0.02, 1.0e6));
    // Near zero the comparison is absolute, at half the epsilon.
    EXPECT_TRUE(MathUtil::equals(0.004, 0.0, 0.01));
    EXPECT_FALSE(MathUtil::equals(0.006, 0.0, 0.01));
    // An infinity never equals itself, exactly as in Java: |inf - inf| is NaN, and NaN < inf * eps
    // is false. Callers comparing possibly infinite values must special-case them.
    EXPECT_FALSE(MathUtil::equals(kInf, kInf));
    EXPECT_FALSE(MathUtil::equals(-kInf, -kInf));
    EXPECT_FALSE(MathUtil::equals(kInf, -kInf));
    EXPECT_FALSE(MathUtil::equals(1.0, kInf));
    EXPECT_FALSE(MathUtil::equals(kInf, 1.0));
}

TEST(MathUtil, SafeSqrtEdgeCases)
{
    EXPECT_EQ(0.0, MathUtil::safeSqrt(0.0));
    EXPECT_EQ(0.0, MathUtil::safeSqrt(-0.0));
    EXPECT_EQ(0.0, MathUtil::safeSqrt(-1.0e300));
    EXPECT_EQ(kInf, MathUtil::safeSqrt(kInf));
    EXPECT_TRUE(std::isnan(MathUtil::safeSqrt(kNaN)));
}

TEST(MathUtil, HypotOfZeroAndInfinity)
{
    EXPECT_EQ(0.0, MathUtil::hypot(0.0, 0.0));
    EXPECT_EQ(5.0, MathUtil::hypot(3.0, -4.0));
    EXPECT_EQ(kInf, MathUtil::hypot(kInf, 1.0));
    // Unlike std::hypot, the direct formula overflows for large inputs, as OpenRocket's does.
    EXPECT_EQ(kInf, MathUtil::hypot(1.0e200, 1.0e200));
}

TEST(MathUtil, ReduceAtExactMultiples)
{
    EXPECT_NEAR(0.0, MathUtil::reduce2Pi(0.0), kEps);
    EXPECT_NEAR(0.0, MathUtil::reduce2Pi(2 * kPi), kEps);
    EXPECT_NEAR(0.0, MathUtil::reduce2Pi(-4 * kPi), kEps);
    EXPECT_NEAR(kPi, MathUtil::reduce2Pi(-kPi), kEps);
    EXPECT_NEAR(kPi, MathUtil::reduce2Pi(3 * kPi), kEps);

    EXPECT_NEAR(0.0, MathUtil::reducePi(0.0), kEps);
    EXPECT_NEAR(0.0, MathUtil::reducePi(2 * kPi), kEps);
    EXPECT_NEAR(0.0, MathUtil::reducePi(-6 * kPi), kEps);
    // Half-way cases round to even like Java's Math.rint: 1.5 turns go to 2, -1.5 to -2.
    EXPECT_NEAR(-kPi, MathUtil::reducePi(3 * kPi), kEps);
    EXPECT_NEAR(kPi, MathUtil::reducePi(-3 * kPi), kEps);
    // pi itself stays pi (0.5 turns round to 0), -pi stays -pi.
    EXPECT_NEAR(kPi, MathUtil::reducePi(kPi), kEps);
    EXPECT_NEAR(-kPi, MathUtil::reducePi(-kPi), kEps);
    EXPECT_NEAR(-kPi / 2, MathUtil::reducePi(1.5 * kPi), kEps);
}

TEST(MathUtil, AverageStddevMedianOfShortInputs)
{
    const std::vector<double> none;
    const std::vector<double> one = {4.2};
    const std::vector<double> two = {1.0, 3.0};

    EXPECT_TRUE(std::isnan(MathUtil::average(none)));
    EXPECT_EQ(4.2, MathUtil::average(one));
    EXPECT_EQ(2.0, MathUtil::average(two));

    EXPECT_TRUE(std::isnan(MathUtil::stddev(none)));
    EXPECT_TRUE(std::isnan(MathUtil::stddev(one)));
    EXPECT_NEAR(std::sqrt(2.0), MathUtil::stddev(two), kEps);

    EXPECT_TRUE(std::isnan(MathUtil::median(none)));
    EXPECT_EQ(4.2, MathUtil::median(one));
    EXPECT_EQ(2.0, MathUtil::median(two));
}

TEST(MathUtil, MedianOrdersLikeJavaDoubleCompare)
{
    // NaN sorts after everything (Java's Double.compare), so it never poisons the sort.
    const std::vector<double> odd  = {3.0, kNaN, 1.0};
    const std::vector<double> even = {kNaN, 2.0, 1.0, 4.0};
    EXPECT_EQ(3.0, MathUtil::median(odd));
    EXPECT_EQ(3.0, MathUtil::median(even));
    // -0.0 sorts before 0.0.
    const std::vector<double> zeros = {0.0, -0.0, 1.0};
    EXPECT_EQ(0.0, MathUtil::median(zeros));
    EXPECT_FALSE(std::signbit(MathUtil::median(zeros)));
    // The input is left untouched.
    const std::vector<double> unsorted = {5.0, 1.0, 3.0};
    EXPECT_EQ(3.0, MathUtil::median(unsorted));
    EXPECT_EQ(5.0, unsorted[0]);
}

TEST(MathUtil, InterpolateEdgeCases)
{
    const std::vector<double> domain  = {1.0, 2.0, 4.0};
    const std::vector<double> range   = {10.0, 20.0, 40.0};
    const std::vector<double> shorter = {10.0, 20.0};

    EXPECT_TRUE(std::isnan(MathUtil::interpolate(domain, shorter, 1.5)));
    EXPECT_TRUE(std::isnan(MathUtil::interpolate(domain, range, kNaN)));
    EXPECT_TRUE(std::isnan(MathUtil::interpolate(domain, range, std::nextafter(1.0, 0.0))));
    // A single sample cannot be interpolated, even at its own abscissa.
    const std::vector<double> single = {1.0};
    EXPECT_TRUE(std::isnan(MathUtil::interpolate(single, single, 1.0)));
    // Up to kEpsilon past the last sample is still accepted, extrapolating the last segment.
    EXPECT_NEAR(40.0, MathUtil::interpolate(domain, range, 4.0 + (MathUtil::kEpsilon / 2)), 1e-7);
    EXPECT_TRUE(std::isnan(MathUtil::interpolate(domain, range, 4.0 + (MathUtil::kEpsilon * 2))));
    // Interior points on each segment.
    EXPECT_NEAR(15.0, MathUtil::interpolate(domain, range, 1.5), kEps);
    EXPECT_NEAR(30.0, MathUtil::interpolate(domain, range, 3.0), kEps);
    EXPECT_NEAR(20.0, MathUtil::interpolate(domain, range, 2.0), kEps);
}

TEST(MathUtil, JavaIntCastTruncatesSaturatesAndZeroesNaN)
{
    EXPECT_EQ(MathUtil::javaIntCast(0.0), 0);
    EXPECT_EQ(MathUtil::javaIntCast(-0.0), 0);
    EXPECT_EQ(MathUtil::javaIntCast(1.999), 1);
    EXPECT_EQ(MathUtil::javaIntCast(-1.999), -1);
    EXPECT_EQ(MathUtil::javaIntCast(600000.7), 600000);
    EXPECT_EQ(MathUtil::javaIntCast(2147483647.0), std::numeric_limits<int>::max());
    EXPECT_EQ(MathUtil::javaIntCast(-2147483648.0), std::numeric_limits<int>::min());
    // Java's (int) cast saturates instead of being undefined ...
    EXPECT_EQ(MathUtil::javaIntCast(2147483648.0), std::numeric_limits<int>::max());
    EXPECT_EQ(MathUtil::javaIntCast(-2147483649.0), std::numeric_limits<int>::min());
    EXPECT_EQ(MathUtil::javaIntCast(1e300), std::numeric_limits<int>::max());
    EXPECT_EQ(MathUtil::javaIntCast(kInf), std::numeric_limits<int>::max());
    EXPECT_EQ(MathUtil::javaIntCast(-kInf), std::numeric_limits<int>::min());
    // ... and NaN is zero.
    EXPECT_EQ(MathUtil::javaIntCast(kNaN), 0);
}

TEST(MathUtil, MapCoordinateEdgeCases)
{
    const Coordinate a(0, 1, 2, 3);
    const Coordinate b(4, 6, 0, 8);

    // A singular destination is returned as is, whatever the source range.
    EXPECT_TRUE(MathUtil::map(6.0, 0.0, 0.0, a, a) == a);
    // Endpoints map to the endpoints, weights included.
    EXPECT_TRUE(MathUtil::map(0.0, 0.0, 5.0, a, b) == a);
    EXPECT_TRUE(MathUtil::map(5.0, 0.0, 5.0, a, b) == b);
    // Beyond the source range the mapping extrapolates.
    EXPECT_TRUE(MathUtil::map(10.0, 0.0, 5.0, a, b) == Coordinate(8, 11, -2, 13));
    EXPECT_THROW(static_cast<void>(MathUtil::map(6.0, 1.0, std::nextafter(1.0, 2.0), a, b)),
                 std::invalid_argument);
}

}  // namespace
