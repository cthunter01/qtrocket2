// The comparisons of tests/core/rocket/JavaValueDifferences.h themselves: what they accept decides
// what every test that pins OpenRocket's values can notice, so a value that is infinite where
// OpenRocket's is finite (a division by zero), or finite where it is infinite, must not pass.

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "QtRocket/util/Coordinate.h"
#include "rocket/JavaValueDifferences.h"

namespace
{

using QtRocket::Coordinate;
using QtRocket::Test::JavaValueDifferences;
using QtRocket::Test::matchesJavaValue;
using QtRocket::Test::matchesPinnedValue;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

TEST(JavaValueDifferences, AnInfiniteValueNeverMatchesAFiniteJavaValue)
{
    // Regression: inf <= 1e-12 * inf + 1e-15 holds, so these used to match.
    EXPECT_FALSE(matchesJavaValue(0.5, kInf));
    EXPECT_FALSE(matchesJavaValue(0.5, -kInf));
    EXPECT_FALSE(matchesJavaValue(0.0, kInf));
    EXPECT_FALSE(matchesJavaValue(0.0, -kInf));
    EXPECT_FALSE(matchesJavaValue(-3e300, -kInf));
}

TEST(JavaValueDifferences, MatchesJavaValueWithinItsTolerances)
{
    EXPECT_TRUE(matchesJavaValue(0.5, 0.5));
    EXPECT_TRUE(matchesJavaValue(0.5, std::nextafter(0.5, 1.0)));
    EXPECT_TRUE(matchesJavaValue(1.0, 1.0 + 5e-13));
    EXPECT_FALSE(matchesJavaValue(1.0, 1.0 + 5e-12));
    // The absolute allowance near zero.
    EXPECT_TRUE(matchesJavaValue(0.0, 5e-16));
    EXPECT_TRUE(matchesJavaValue(0.0, -0.0));
    EXPECT_FALSE(matchesJavaValue(0.0, 5e-15));

    EXPECT_TRUE(matchesJavaValue(kNaN, kNaN));
    EXPECT_FALSE(matchesJavaValue(kNaN, 1.0));
    EXPECT_FALSE(matchesJavaValue(1.0, kNaN));
    EXPECT_FALSE(matchesJavaValue(kNaN, kInf));

    // An infinite Java value also matches a huge one of its sign (see the header).
    EXPECT_TRUE(matchesJavaValue(kInf, kInf));
    EXPECT_TRUE(matchesJavaValue(-kInf, -kInf));
    EXPECT_TRUE(matchesJavaValue(kInf, 1e13));
    EXPECT_TRUE(matchesJavaValue(-kInf, -1e13));
    EXPECT_FALSE(matchesJavaValue(kInf, 1e11));
    EXPECT_FALSE(matchesJavaValue(kInf, -kInf));
    EXPECT_FALSE(matchesJavaValue(-kInf, kInf));
    EXPECT_FALSE(matchesJavaValue(kInf, -1e13));
}

TEST(JavaValueDifferences, APinnedInfinityMatchesOnlyTheSameInfinity)
{
    EXPECT_TRUE(matchesPinnedValue(kInf, kInf));
    EXPECT_TRUE(matchesPinnedValue(-kInf, -kInf));
    EXPECT_FALSE(matchesPinnedValue(kInf, -kInf));
    EXPECT_FALSE(matchesPinnedValue(-kInf, kInf));
    EXPECT_FALSE(matchesPinnedValue(kInf, 1e13));
    EXPECT_FALSE(matchesPinnedValue(kInf, std::numeric_limits<double>::max()));
    EXPECT_FALSE(matchesPinnedValue(-kInf, -1e13));
    EXPECT_FALSE(matchesPinnedValue(kInf, kNaN));

    EXPECT_FALSE(matchesPinnedValue(0.5, kInf));
    EXPECT_FALSE(matchesPinnedValue(0.5, -kInf));
    EXPECT_FALSE(matchesPinnedValue(0.0, kInf));
    EXPECT_FALSE(matchesPinnedValue(0.0, -kInf));
    EXPECT_FALSE(matchesPinnedValue(std::numeric_limits<double>::max(), kInf));
}

TEST(JavaValueDifferences, APinnedValueIsHeldRelativeToItself)
{
    EXPECT_TRUE(matchesPinnedValue(0.5, 0.5));
    EXPECT_TRUE(matchesPinnedValue(1.0, 1.0 + 5e-13));
    EXPECT_FALSE(matchesPinnedValue(1.0, 1.0 + 5e-12));
    EXPECT_TRUE(matchesPinnedValue(-2e-20, -2e-20 * (1 + 5e-13)));
    EXPECT_FALSE(matchesPinnedValue(-2e-20, -2e-20 * (1 + 5e-12)));
    EXPECT_FALSE(matchesPinnedValue(2e-20, -2e-20));

    // A zero must be a zero, of either sign: there is no absolute allowance.
    EXPECT_TRUE(matchesPinnedValue(0.0, 0.0));
    EXPECT_TRUE(matchesPinnedValue(0.0, -0.0));
    EXPECT_FALSE(matchesPinnedValue(0.0, 1e-300));
    EXPECT_FALSE(matchesPinnedValue(0.0, std::numeric_limits<double>::denorm_min()));
    EXPECT_FALSE(matchesPinnedValue(1e-300, 0.0));

    EXPECT_TRUE(matchesPinnedValue(kNaN, kNaN));
    EXPECT_FALSE(matchesPinnedValue(kNaN, 0.0));
    EXPECT_FALSE(matchesPinnedValue(0.0, kNaN));

    // A wider tolerance, for a pin that says why it needs one.
    EXPECT_TRUE(matchesPinnedValue(1.0, 1.0 + 5e-12, 1e-11));
    EXPECT_FALSE(matchesPinnedValue(1.0, 1.0 + 5e-11, 1e-11));
    EXPECT_FALSE(matchesPinnedValue(0.5, kInf, 1e-9));
    EXPECT_FALSE(matchesPinnedValue(0.0, 1e-300, 1e-9));
}

TEST(JavaValueDifferences, CollectsOneLineForEachDifference)
{
    JavaValueDifferences same;
    same.number("a", 0.5, 0.5);
    same.number("b", kInf, 1e13);
    same.pinned("c", 0.0, -0.0);
    same.pinned("d", kNaN, kNaN);
    same.pinned("e", kInf, kInf);
    same.pinned("f", 1.0, 1.0 + 5e-12, 1e-11);
    same.coordinate("g", Coordinate{1, 2, 3, 4}, Coordinate{1, 2, 3, 4});
    EXPECT_EQ(same.text(), "");

    JavaValueDifferences different;
    different.number("drag", 0.5, kInf);
    different.pinned("zero", 0.0, kInf);
    different.pinned("roll damping", kInf, 5e13);
    different.pinned("small", 0.0, 1e-300);
    different.pinned("wide", 1.0, 1.0 + 5e-11, 1e-11);
    const std::string& text = different.text();
    EXPECT_NE(text.find("  drag: expected 0.5, got inf\n"), std::string::npos) << text;
    EXPECT_NE(text.find("  zero: expected 0, got inf\n"), std::string::npos) << text;
    EXPECT_NE(text.find("  roll damping: expected inf, got 5e+13\n"), std::string::npos) << text;
    EXPECT_NE(text.find("  small: expected 0, got 1e-300\n"), std::string::npos) << text;
    EXPECT_NE(text.find("  wide: expected 1, got "), std::string::npos) << text;
    EXPECT_EQ(std::ranges::count(text, '\n'), 5);
}

}  // namespace
