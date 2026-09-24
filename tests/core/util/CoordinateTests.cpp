#include "QtRocket/util/Coordinate.h"

#include <cstddef>
#include <format>
#include <functional>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace
{

using QtRocket::Coordinate;

constexpr double kEps = 0.0000000001;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// Java's assertEquals(Coordinate, Coordinate, EPS) helper: every component within kEps, printing
// the whole coordinate on failure.
void expectCoordinateNear(const Coordinate& expected, const Coordinate& actual)
{
    SCOPED_TRACE(std::format("expected {}, actual {}", expected.toString(), actual.toString()));
    EXPECT_NEAR(expected.x, actual.x, kEps);
    EXPECT_NEAR(expected.y, actual.y, kEps);
    EXPECT_NEAR(expected.z, actual.z, kEps);
    EXPECT_NEAR(expected.weight, actual.weight, kEps);
}

// ---- Ported from CoordinateTest.java ----

TEST(Coordinate, Constructors)
{
    const Coordinate c1;
    expectCoordinateNear(Coordinate(0, 0, 0, 0), c1);

    const Coordinate c2(1);
    expectCoordinateNear(Coordinate(1, 0, 0, 0), c2);

    const Coordinate c3(1, 2);
    expectCoordinateNear(Coordinate(1, 2, 0, 0), c3);

    const Coordinate c4(1, 2, 3);
    expectCoordinateNear(Coordinate(1, 2, 3, 0), c4);

    const Coordinate c5(1, 2, 3, 4);
    expectCoordinateNear(Coordinate(1, 2, 3, 4), c5);
}

TEST(Coordinate, Setters)
{
    const Coordinate x(1, 1, 1, 1);

    expectCoordinateNear(Coordinate(2, 1, 1, 1), x.setX(2));
    expectCoordinateNear(Coordinate(1, 2, 1, 1), x.setY(2));
    expectCoordinateNear(Coordinate(1, 1, 2, 1), x.setZ(2));
    expectCoordinateNear(Coordinate(1, 1, 1, 2), x.setWeight(2));

    const Coordinate y(1, 2, 3, 4);
    expectCoordinateNear(Coordinate(1, 2, 3, 1), x.setXYZ(y));
}

TEST(Coordinate, IsWeighted)
{
    EXPECT_TRUE(Coordinate(1, 1, 1, 1).isWeighted());
    EXPECT_FALSE(Coordinate(1, 1, 1, 0).isWeighted());
}

TEST(Coordinate, IsNaN)
{
    EXPECT_FALSE(Coordinate(1, 1, 1, 1).isNaN());
    EXPECT_TRUE(Coordinate(kNaN, 1, 1, 1).isNaN());
    EXPECT_TRUE(Coordinate(1, kNaN, 1, 1).isNaN());
    EXPECT_TRUE(Coordinate(1, 1, kNaN, 1).isNaN());
    EXPECT_TRUE(Coordinate(1, 1, 1, kNaN).isNaN());
    EXPECT_TRUE(Coordinate::kNaN.isNaN());
}

TEST(Coordinate, Add)
{
    const Coordinate x(1, 1, 1, 1);
    const Coordinate y(1, 2, 3, 4);

    expectCoordinateNear(Coordinate(2, 3, 4, 5), x.add(y));
    expectCoordinateNear(Coordinate(2, 3, 4, 1), x.add(1, 2, 3));
    expectCoordinateNear(Coordinate(2, 3, 4, 5), x.add(1, 2, 3, 4));
}

TEST(Coordinate, Sub)
{
    const Coordinate x(1, 1, 1, 1);
    const Coordinate y(1, 2, 3, 4);

    expectCoordinateNear(Coordinate(0, -1, -2, 1), x.sub(y));
    expectCoordinateNear(Coordinate(0, -1, -2, 1), x.sub(1, 2, 3));
}

TEST(Coordinate, Multiply)
{
    const Coordinate x(1, 2, 3, 4);

    expectCoordinateNear(Coordinate(2, 4, 6, 8), x.multiply(2));
    expectCoordinateNear(Coordinate(1, 4, 9, 16), x.multiply(x));
}

TEST(Coordinate, Dot)
{
    const Coordinate x(1, 1, 1, 1);
    const Coordinate y(1, 2, 3, 4);

    EXPECT_NEAR(6, x.dot(y), kEps);
    EXPECT_NEAR(6, y.dot(x), kEps);
    EXPECT_NEAR(6, Coordinate::dot(x, y), kEps);
}

TEST(Coordinate, Length)
{
    const Coordinate x(3, 4, 0, 1);

    EXPECT_NEAR(5, x.length(), kEps);
    EXPECT_NEAR(25, x.length2(), kEps);
}

TEST(Coordinate, Max)
{
    EXPECT_NEAR(3, Coordinate(1, -2, 3, 4).max(), kEps);
}

TEST(Coordinate, Normalize)
{
    const Coordinate x(3, 4, 0, 2);
    const Coordinate normalized = x.normalize();

    EXPECT_NEAR(1, normalized.length(), kEps);
    EXPECT_NEAR(2, normalized.weight, kEps);
}

TEST(Coordinate, Cross)
{
    const Coordinate x(1, 0, 0);
    const Coordinate y(0, 1, 0);

    expectCoordinateNear(Coordinate(0, 0, 1), x.cross(y));
    expectCoordinateNear(Coordinate(0, 0, 1), Coordinate::cross(x, y));
}

TEST(Coordinate, Average)
{
    const Coordinate x(1, 2, 4, 1);
    Coordinate       y(3, 5, 9, 1);

    expectCoordinateNear(Coordinate(2, 3.5, 6.5, 2), x.average(y));

    y = Coordinate(3, 5, 9, 3);

    expectCoordinateNear(Coordinate(2.5, 4.25, 7.75, 4), x.average(y));
}

TEST(Coordinate, Interpolate)
{
    const Coordinate x(0, 0, 0, 0);
    const Coordinate y(10, 10, 10, 10);

    expectCoordinateNear(Coordinate(5, 5, 5, 5), x.interpolate(y, 0.5));
}

// ---- Behaviour OpenRocket does not test ----

TEST(Coordinate, Constants)
{
    EXPECT_TRUE(Coordinate::kZero.exactlyEquals(Coordinate()));
    EXPECT_TRUE(Coordinate::kNul.exactlyEquals(Coordinate::kZero));
    EXPECT_FALSE(Coordinate::kZero.isWeighted());
    EXPECT_TRUE(Coordinate::kNaN.isNaN());
    EXPECT_TRUE(Coordinate::kXUnit.exactlyEquals(Coordinate(1, 0, 0)));
    EXPECT_TRUE(Coordinate::kYUnit.exactlyEquals(Coordinate(0, 1, 0)));
    EXPECT_TRUE(Coordinate::kZUnit.exactlyEquals(Coordinate(0, 0, 1)));
    EXPECT_NEAR(1, Coordinate::kXUnit.cross(Coordinate::kYUnit).dot(Coordinate::kZUnit), kEps);

    constexpr double kMaxDouble = std::numeric_limits<double>::max();
    EXPECT_TRUE(
        Coordinate::kMax.exactlyEquals(Coordinate(kMaxDouble, kMaxDouble, kMaxDouble, kMaxDouble)));
    EXPECT_TRUE(
        Coordinate::kMin.exactlyEquals(Coordinate(-kMaxDouble, -kMaxDouble, -kMaxDouble, 0)));
    EXPECT_FALSE(Coordinate::kMin.isWeighted());
}

TEST(Coordinate, ArithmeticIsConstexpr)
{
    static_assert(Coordinate::kXUnit.x == 1.0);
    static_assert(
        Coordinate(1, 2, 3, 4).add(Coordinate(1, 1, 1, 1)).exactlyEquals(Coordinate(2, 3, 4, 5)));
    static_assert(
        Coordinate(1, 2, 3, 4).sub(Coordinate(1, 1, 1, 1)).exactlyEquals(Coordinate(0, 1, 2, 4)));
    static_assert(Coordinate(1, 2, 3, 4).multiply(2).exactlyEquals(Coordinate(2, 4, 6, 8)));
    static_assert(Coordinate(1, 2, 3, 4)
                      .addScaled(Coordinate(1, 1, 1, 1), 2)
                      .exactlyEquals(Coordinate(3, 4, 5, 6)));
    static_assert(Coordinate(1, 2, 3).dot(Coordinate(4, 5, 6)) == 32.0);
    static_assert(Coordinate::cross(Coordinate::kYUnit, Coordinate::kZUnit)
                      .exactlyEquals(Coordinate::kXUnit));
    static_assert(Coordinate(3, 4, 0, 9).length2() == 25.0);
    static_assert(Coordinate(0, 0, 0, 0)
                      .interpolate(Coordinate(10, 10, 10, 10), 0.25)
                      .exactlyEquals(Coordinate(2.5, 2.5, 2.5, 2.5)));
    static_assert(Coordinate(1, 2, 3, 4).setX(9).exactlyEquals(Coordinate(9, 2, 3, 4)));
    static_assert(Coordinate(1, 1, 1, 1).isWeighted());
    static_assert(!Coordinate(1, 1, 1, 0).isWeighted());
    SUCCEED();
}

TEST(Coordinate, AddScaled)
{
    const Coordinate x(1, 1, 1, 1);
    const Coordinate y(1, 2, 3, 4);

    expectCoordinateNear(Coordinate(3, 5, 7, 9), x.addScaled(y, 2));
    expectCoordinateNear(x, x.addScaled(y, 0));
}

TEST(Coordinate, Operators)
{
    const Coordinate x(1, 1, 1, 1);
    const Coordinate y(1, 2, 3, 4);

    // + sums the weights like add(); - keeps the left weight like sub().
    expectCoordinateNear(Coordinate(2, 3, 4, 5), x + y);
    expectCoordinateNear(Coordinate(0, -1, -2, 1), x - y);
    expectCoordinateNear(Coordinate(0, 1, 2, 4), y - x);
    expectCoordinateNear(Coordinate(-1, -2, -3, -4), -y);
    expectCoordinateNear(Coordinate(2, 4, 6, 8), y * 2.0);
    expectCoordinateNear(Coordinate(2, 4, 6, 8), 2.0 * y);
    expectCoordinateNear(Coordinate(0.5, 1, 1.5, 2), y / 2.0);
    expectCoordinateNear(y, (y * 3.0) / 3.0);

    static_assert(
        (Coordinate(1, 2, 3, 4) + Coordinate(1, 1, 1, 1)).exactlyEquals(Coordinate(2, 3, 4, 5)));
    static_assert(
        (Coordinate(1, 2, 3, 4) - Coordinate(1, 1, 1, 1)).exactlyEquals(Coordinate(0, 1, 2, 4)));
    static_assert((-Coordinate(1, 2, 3, 4)).exactlyEquals(Coordinate(-1, -2, -3, -4)));
    static_assert((Coordinate(1, 2, 3, 4) * 2.0).exactlyEquals(Coordinate(2, 4, 6, 8)));
    static_assert((2.0 * Coordinate(1, 2, 3, 4)).exactlyEquals(Coordinate(2, 4, 6, 8)));
    static_assert((Coordinate(2, 4, 6, 8) / 2.0).exactlyEquals(Coordinate(1, 2, 3, 4)));
}

TEST(Coordinate, TolerantEqualityFollowsMathUtil)
{
    const Coordinate c(1, 2, 3, 4);

    EXPECT_TRUE(c == Coordinate(1, 2, 3, 4));
    EXPECT_FALSE(c != Coordinate(1, 2, 3, 4));
    // EXPECT_EQ and EXPECT_NE use the same tolerant comparison (and print toString() on failure).
    EXPECT_EQ(c, Coordinate(1, 2, 3, 4));
    // Within 1e-8 relative: equal but not exactly equal.
    const Coordinate nearby(1 + 1e-9, 2, 3, 4);
    EXPECT_EQ(c, nearby);
    EXPECT_FALSE(c.exactlyEquals(nearby));
    // Beyond it: different.
    EXPECT_NE(c, Coordinate(1 + 2e-8, 2, 3, 4));
    EXPECT_NE(c, Coordinate(1, 2, 3, 4 + 1e-7));
    // The weight takes part in equality.
    EXPECT_FALSE(Coordinate(1, 2, 3, 4) == Coordinate(1, 2, 3, 5));
    // NaN equals nothing, not even itself, in either comparison.
    EXPECT_FALSE(Coordinate::kNaN == Coordinate::kNaN);
    EXPECT_FALSE(Coordinate::kNaN.exactlyEquals(Coordinate::kNaN));
    // Signed zeros are the same number.
    EXPECT_TRUE(Coordinate(-0.0, 0, 0) == Coordinate(0, 0, 0));
    EXPECT_TRUE(Coordinate(-0.0, 0, 0).exactlyEquals(Coordinate(0, 0, 0)));
}

TEST(Coordinate, NormalizeRejectsZeroLength)
{
    EXPECT_THROW(static_cast<void>(Coordinate::kZero.normalize()), std::domain_error);
    EXPECT_THROW(static_cast<void>(Coordinate(1e-8, 0, 0, 5).normalize()), std::domain_error);
    EXPECT_NO_THROW(static_cast<void>(Coordinate(1e-6, 0, 0).normalize()));
    expectCoordinateNear(Coordinate(-1, 0, 0, 3), Coordinate(-2, 0, 0, 3).normalize());
}

TEST(Coordinate, AverageOfUnweightedIsMidpoint)
{
    const Coordinate a(1, 2, 3, 0);
    const Coordinate b(3, 4, 5, 0);

    expectCoordinateNear(Coordinate(2, 3, 4, 0), a.average(b));
    expectCoordinateNear(Coordinate(2, 3, 4, 0), b.average(a));
    // One weighted operand dominates completely.
    expectCoordinateNear(Coordinate(3, 4, 5, 2), a.average(Coordinate(3, 4, 5, 2)));
    expectCoordinateNear(Coordinate(1, 2, 3, 2), Coordinate(1, 2, 3, 2).average(b));
    // Weights below kEpsilon^2 in total count as unweighted.
    expectCoordinateNear(Coordinate(2, 3, 4, 0), a.setWeight(1e-17).average(b.setWeight(1e-17)));
}

TEST(Coordinate, NegativeWeightIsNotWeighted)
{
    EXPECT_FALSE(Coordinate(1, 1, 1, -1).isWeighted());
    EXPECT_FALSE(Coordinate(1, 1, 1, kNaN).isWeighted());
}

TEST(Coordinate, MaxUsesAbsoluteValuesAndIgnoresWeight)
{
    EXPECT_NEAR(5, Coordinate(-5, 1, 2, 100).max(), kEps);
    EXPECT_NEAR(7, Coordinate(0, -7, 2).max(), kEps);
    EXPECT_NEAR(0, Coordinate::kZero.max(), kEps);
    EXPECT_NEAR(0, Coordinate::kZero.length(), kEps);
}

TEST(Coordinate, ToString)
{
    EXPECT_EQ(Coordinate(1, 2, 3).toString(), "(1.00000,2.00000,3.00000)");
    EXPECT_EQ(Coordinate(1, 2, 3, 4).toString(), "(1.00000,2.00000,3.00000,w=4.00000)");
    EXPECT_EQ(Coordinate(-1.5, 0.123456).toString(), "(-1.50000,0.12346,0.00000)");
    EXPECT_EQ(Coordinate::kZero.toString(), "(0.00000,0.00000,0.00000)");
    EXPECT_EQ(Coordinate(1, 2, 3, 4).toPreciseString(),
              "cm= 4.00000000g @[1.00000000,2.00000000,3.00000000]");
}

TEST(Coordinate, ToStringDeviatesFromJavaAsDocumented)
{
    // 0.015625 (2^-6) is an exact tie at five decimals: std::format rounds it half to even,
    // Java's Formatter half up ("(0.01563,0.00000,0.00000)").
    EXPECT_EQ(Coordinate(0.015625, 0, 0).toString(), "(0.01562,0.00000,0.00000)");
    EXPECT_EQ(Coordinate(0.015625, 0, 0, 0.015625).toString(),
              "(0.01562,0.00000,0.00000,w=0.01562)");
    EXPECT_EQ(Coordinate(0, 0, 0, 0.001953125).toPreciseString(),
              "cm= 0.00195312g @[0.00000000,0.00000000,0.00000000]");
    // NaN and infinity: nan/inf, where Java prints NaN/Infinity. kNaN's weight is NaN, which is
    // not "weighted", so the three-field form.
    EXPECT_EQ(Coordinate::kNaN.toString(), "(nan,nan,nan)");
    EXPECT_EQ(Coordinate(kInf, 0, 0).toString(), "(inf,0.00000,0.00000)");
    EXPECT_EQ(Coordinate(-kInf, 0, 0, 1).toString(), "(-inf,0.00000,0.00000,w=1.00000)");
    EXPECT_EQ(Coordinate::kNaN.toPreciseString(), "cm= nang @[nan,nan,nan]");
}

TEST(Coordinate, StreamInsertionWritesToString)
{
    const Coordinate   c(1, 2, 3, 4);
    std::ostringstream os;
    os << c;
    EXPECT_EQ(os.str(), c.toString());
    // GoogleTest picks the operator up, so a failed EXPECT_EQ prints the value, not a hex dump.
    EXPECT_EQ(::testing::PrintToString(Coordinate(1, 2, 3)), "(1.00000,2.00000,3.00000)");
}

TEST(Coordinate, HashFollowsOpenRocket)
{
    const std::hash<Coordinate> hash;

    // (int)((x + y + z) * 100000): the weight is not part of it.
    EXPECT_EQ(hash(Coordinate(1, 2, 3)), static_cast<std::size_t>(600000));
    EXPECT_EQ(hash(Coordinate(1, 2, 3, 4)), hash(Coordinate(1, 2, 3, 7)));
    EXPECT_EQ(hash(Coordinate(-1, -2, -3)), static_cast<std::size_t>(-600000));
    EXPECT_EQ(hash(Coordinate(0.000001, 0, 0)), static_cast<std::size_t>(0));
    // Java's (int) cast: NaN is 0 and huge values saturate instead of being undefined.
    EXPECT_EQ(hash(Coordinate::kNaN), static_cast<std::size_t>(0));
    EXPECT_EQ(hash(Coordinate::kMax), static_cast<std::size_t>(std::numeric_limits<int>::max()));
    EXPECT_EQ(hash(Coordinate::kMin), static_cast<std::size_t>(std::numeric_limits<int>::min()));
}

}  // namespace
