#include "QtRocket/util/DecimalFormat.h"

#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::BugError;
using QtRocket::DecimalFormat;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// Every string below is what java.text.DecimalFormat prints on JDK 17 with Locale.US.

TEST(DecimalFormat, DigitCountsFollowJavasPatternRules)
{
    const DecimalFormat integer("#");
    EXPECT_EQ(integer.getMinimumIntegerDigits(), 0);
    EXPECT_EQ(integer.getMaximumIntegerDigits(), std::numeric_limits<int>::max());
    EXPECT_EQ(integer.getMinimumFractionDigits(), 0);
    EXPECT_EQ(integer.getMaximumFractionDigits(), 0);
    EXPECT_FALSE(integer.usesExponentialNotation());

    const DecimalFormat decimal("0.0##");
    EXPECT_EQ(decimal.getMinimumIntegerDigits(), 1);
    EXPECT_EQ(decimal.getMinimumFractionDigits(), 1);
    EXPECT_EQ(decimal.getMaximumFractionDigits(), 3);

    // "#.###" is read as "#0.###".
    const DecimalFormat hashes("#.###");
    EXPECT_EQ(hashes.getMinimumIntegerDigits(), 1);
    EXPECT_EQ(hashes.getMinimumFractionDigits(), 0);
    EXPECT_EQ(hashes.getMaximumFractionDigits(), 3);

    const DecimalFormat exponential("0.00E0");
    EXPECT_TRUE(exponential.usesExponentialNotation());
    EXPECT_EQ(exponential.getMinimumIntegerDigits(), 1);
    EXPECT_EQ(exponential.getMaximumIntegerDigits(), 1);
    EXPECT_EQ(exponential.getMinimumFractionDigits(), 2);
    EXPECT_EQ(exponential.getMaximumFractionDigits(), 2);

    const DecimalFormat engineering("##0.##E0");
    EXPECT_EQ(engineering.getMinimumIntegerDigits(), 1);
    EXPECT_EQ(engineering.getMaximumIntegerDigits(), 3);
}

TEST(DecimalFormat, IntegerPatternRoundsHalfEvenOnJavasDigits)
{
    EXPECT_EQ(DecimalFormat("#").format(0.0), "0");
    EXPECT_EQ(DecimalFormat("#").format(-0.0), "-0");
    EXPECT_EQ(DecimalFormat("#").format(0.3), "0");
    EXPECT_EQ(DecimalFormat("#").format(-0.3), "-0");
    EXPECT_EQ(DecimalFormat("#").format(0.5), "0");
    EXPECT_EQ(DecimalFormat("#").format(1.5), "2");
    EXPECT_EQ(DecimalFormat("#").format(2.5), "2");
    EXPECT_EQ(DecimalFormat("#").format(-2.5), "-2");
    EXPECT_EQ(DecimalFormat("#").format(3.5), "4");
    EXPECT_EQ(DecimalFormat("#").format(1235000.0), "1235000");
    // FloatingDecimal's digits, not the shortest ones ("590295810358705700000") nor the exact
    // binary value ("590295810358705651712").
    EXPECT_EQ(DecimalFormat("#").format(0x1p69), "590295810358705650000");
    EXPECT_EQ(DecimalFormat("#").format(1e23), "99999999999999990000000");
    EXPECT_EQ(DecimalFormat("#").format(1.245e19), "12450000000000000000");
    EXPECT_EQ(DecimalFormat("#").format(9.223372036854775807e18), "9223372036854776000");
    EXPECT_EQ(DecimalFormat("0").format(0.5), "0");
    EXPECT_EQ(DecimalFormat("0").format(-0.4), "-0");
}

TEST(DecimalFormat, FractionPatternsDecideTiesByFloatingDecimalsFlags)
{
    // 0.15 and 0.35 lie just below their ties, 0.05 and 0.45 just above, 0.25 is exact.
    EXPECT_EQ(DecimalFormat("0.#").format(0.05), "0.1");
    EXPECT_EQ(DecimalFormat("0.#").format(0.15), "0.1");
    EXPECT_EQ(DecimalFormat("0.#").format(0.25), "0.2");
    EXPECT_EQ(DecimalFormat("0.#").format(0.35), "0.3");
    EXPECT_EQ(DecimalFormat("0.#").format(0.45), "0.5");
    EXPECT_EQ(DecimalFormat("0.#").format(-0.04), "-0");
    EXPECT_EQ(DecimalFormat("0.#").format(-0.05), "-0.1");
    EXPECT_EQ(DecimalFormat("0.#").format(9.95), "9.9");
    EXPECT_EQ(DecimalFormat("0.#").format(0x1p69), "590295810358705650000");
    EXPECT_EQ(DecimalFormat("0.#").format(8.41e21), "8409999999999999000000");

    EXPECT_EQ(DecimalFormat("0.0").format(0.0), "0.0");
    EXPECT_EQ(DecimalFormat("0.0").format(-0.0), "-0.0");
    EXPECT_EQ(DecimalFormat("0.0").format(0.05), "0.1");
    EXPECT_EQ(DecimalFormat("0.0").format(0.25), "0.2");
    EXPECT_EQ(DecimalFormat("0.0").format(-1e-300), "-0.0");
    EXPECT_EQ(DecimalFormat("0.0").format(123.45), "123.5");

    EXPECT_EQ(DecimalFormat("0.0##").format(0.0), "0.0");
    EXPECT_EQ(DecimalFormat("0.0##").format(-0.0), "-0.0");
    EXPECT_EQ(DecimalFormat("0.0##").format(0.0005), "0.0");
    EXPECT_EQ(DecimalFormat("0.0##").format(-0.0004), "-0.0");
    EXPECT_EQ(DecimalFormat("0.0##").format(1.0005), "1.0");
    EXPECT_EQ(DecimalFormat("0.0##").format(0.1235), "0.123");
    EXPECT_EQ(DecimalFormat("0.0##").format(2.675), "2.675");
    EXPECT_EQ(DecimalFormat("0.0##").format(99.9995), "99.999");
    EXPECT_EQ(DecimalFormat("0.0##").format(1e-12), "0.0");

    EXPECT_EQ(DecimalFormat("#.###").format(0.3), "0.3");
    EXPECT_EQ(DecimalFormat("#.###").format(-0.3), "-0.3");
    EXPECT_EQ(DecimalFormat("#.###").format(0.0), "0");
    EXPECT_EQ(DecimalFormat("#.###").format(0.0005), "0");
    EXPECT_EQ(DecimalFormat("#.###").format(1.0005), "1");
    EXPECT_EQ(DecimalFormat("#.###").format(2.0625), "2.062");
    EXPECT_EQ(DecimalFormat("#.###").format(1.1875), "1.188");
    EXPECT_EQ(DecimalFormat("#.###").format(123.456), "123.456");
}

TEST(DecimalFormat, RoundingIntoTheFirstDigitLooksAtTheTrailingZeros)
{
    // Double.toString(5e-6) is "5.0E-6": DigitList sees "50", and a '5' followed by a zero is no
    // tie there, so it rounds down, where the exact value (5.000000000000000409e-6) is above it.
    EXPECT_EQ(DecimalFormat("0.#####").format(5e-6), "0");
    EXPECT_EQ(DecimalFormat("0.#####").format(-5e-6), "-0");
    EXPECT_EQ(DecimalFormat("0.#####").format(1.5e-5), "0.00002");
    EXPECT_EQ(DecimalFormat("0.#####").format(2.5e-5), "0.00003");
    EXPECT_EQ(DecimalFormat("0.#####").format(4.9e-6), "0");
    EXPECT_EQ(DecimalFormat("0.#####").format(1.23456785), "1.23457");
}

TEST(DecimalFormat, ExponentialPatterns)
{
    EXPECT_EQ(DecimalFormat("0.00E0").format(0.0), "0.00E0");
    EXPECT_EQ(DecimalFormat("0.00E0").format(-0.0), "-0.00E0");
    // Integers below 2^63 are converted exactly but never flagged exact: their ties round up.
    EXPECT_EQ(DecimalFormat("0.00E0").format(1235000.0), "1.24E6");
    EXPECT_EQ(DecimalFormat("0.00E0").format(1245000.0), "1.25E6");
    EXPECT_EQ(DecimalFormat("0.00E0").format(1.245e19), "1.24E19");
    EXPECT_EQ(DecimalFormat("0.00E0").format(9.995e6), "1.00E7");
    EXPECT_EQ(DecimalFormat("0.00E0").format(0x1p69), "5.90E20");
    EXPECT_EQ(DecimalFormat("0.00E0").format(1e23), "1.00E23");
    EXPECT_EQ(DecimalFormat("0.00E0").format(4.9e-324), "4.90E-324");
    EXPECT_EQ(DecimalFormat("0.00E0").format(1.7976931348623157e308), "1.80E308");
    EXPECT_EQ(DecimalFormat("0.00E0").format(0.00012345), "1.23E-4");
    EXPECT_EQ(DecimalFormat("0.00E0").format(-2.5e-7), "-2.50E-7");
    EXPECT_EQ(DecimalFormat("##0.##E0").format(12345.0), "12.345E3");
    EXPECT_EQ(DecimalFormat("##0.##E0").format(0.00012345), "123.45E-6");
    EXPECT_EQ(DecimalFormat("##0.##E0").format(0.0), "0E0");
    EXPECT_EQ(DecimalFormat("00.00E00").format(12345.0), "12.35E03");
    EXPECT_EQ(DecimalFormat("00.00E00").format(-0.00012345), "-12.34E-05");
}

TEST(DecimalFormat, DecimalSeparatorAtEitherEnd)
{
    EXPECT_EQ(DecimalFormat(".00").format(0.5), ".50");
    EXPECT_EQ(DecimalFormat(".00").format(0.125), ".12");
    EXPECT_EQ(DecimalFormat("0.").format(1.5), "2.");
    EXPECT_EQ(DecimalFormat("0.").format(-0.0), "-0.");
}

TEST(DecimalFormat, PatternsWithoutARequiredDigit)
{
    // Without any '0', Java reads the '#' next to the point as one.
    const DecimalFormat hashes("##.##");
    EXPECT_EQ(hashes.getMinimumIntegerDigits(), 1);
    EXPECT_EQ(hashes.format(0.25), "0.25");
    const DecimalFormat fraction(".##");
    EXPECT_EQ(fraction.getMinimumIntegerDigits(), 0);
    EXPECT_EQ(fraction.getMinimumFractionDigits(), 1);
    EXPECT_EQ(fraction.getMaximumFractionDigits(), 2);
    EXPECT_EQ(fraction.format(0.25), ".25");
    EXPECT_EQ(fraction.format(0.0), ".0");
    EXPECT_EQ(fraction.format(1.5), "1.5");
    // A point alone requires no digit and is always shown.
    EXPECT_EQ(DecimalFormat(".").format(1.5), "2.");
    EXPECT_EQ(DecimalFormat(".").format(0.25), "0.");
    EXPECT_EQ(DecimalFormat("#.0").format(0.045), ".0");
    // Nor does scientific notation need an integer digit.
    EXPECT_EQ(DecimalFormat("#E0").format(1.5), ".2E1");
    EXPECT_EQ(DecimalFormat("#E0").format(-0.0), "-E0");
    EXPECT_EQ(DecimalFormat(".00E0").format(1e-5), ".10E-4");
}

TEST(DecimalFormat, DigitCountsAreLimitedForADouble)
{
    // At most 309 integer and 340 fraction digits, whatever the pattern asks for.
    const std::string zeros(400, '0');
    EXPECT_EQ(DecimalFormat("0." + zeros).format(1.0), "1." + std::string(340, '0'));
    EXPECT_EQ(DecimalFormat(zeros).format(1.0), std::string(308, '0') + "1");
    EXPECT_EQ(DecimalFormat("#." + std::string(400, '#')).format(4.9e-324),
              "0." + std::string(323, '0') + "49");
}

TEST(DecimalFormat, StartsFromJdk17sDigitsWhereTheyAreNotTheShortest)
{
    // FloatingDecimal's departures from the shortest digits (see FloatingDecimalTests.cpp) show
    // through: the wrapped 64-bit test, the end of the interval counted in with big integers, and
    // the quarter-gap margin of a subnormal power of two.
    EXPECT_EQ(DecimalFormat("#").format(0x1.7606c1e42a3b5p84), "28260635462337084000000000");
    EXPECT_EQ(DecimalFormat("##0.##E0").format(0x1.7606c1e42a3b5p84), "28.261E24");
    EXPECT_EQ(DecimalFormat("#").format(0x1.52d02c7e14af6p87), "204800000000000000000000000");
    EXPECT_EQ(DecimalFormat("0.00E0").format(0x1.52d02c7e14af6p87), "2.05E26");
    EXPECT_EQ(DecimalFormat("0.00E0").format(0x1p-1069), "1.58E-322");
    EXPECT_EQ(DecimalFormat("##0.##E0").format(0x1p-1069), "158E-324");
    EXPECT_EQ(DecimalFormat("##0.##E0").format(1e-7), "100E-9");
}

TEST(DecimalFormat, NaNAndInfinities)
{
    EXPECT_EQ(DecimalFormat("0.0##").format(kNaN), "NaN");
    EXPECT_EQ(DecimalFormat("0.0##").format(-kNaN), "NaN");
    EXPECT_EQ(DecimalFormat("#").format(kInf), "∞");
    EXPECT_EQ(DecimalFormat("#").format(-kInf), "-∞");
    EXPECT_EQ(DecimalFormat("0.00E0").format(kInf), "∞");
}

TEST(DecimalFormat, MalformedAndUnsupportedPatternsAreBugs)
{
    // Malformed in Java too (IllegalArgumentException).
    EXPECT_THROW(DecimalFormat("0.0.0"), BugError);
    EXPECT_THROW(DecimalFormat("#.0#0"), BugError);
    EXPECT_THROW(DecimalFormat("#0#"), BugError);
    EXPECT_THROW(DecimalFormat("0.00E"), BugError);
    // Valid in Java, outside the supported subset ("E0" is the prefix "E" and "0").
    EXPECT_THROW(DecimalFormat("E0"), BugError);
    EXPECT_THROW(DecimalFormat(""), BugError);
    EXPECT_THROW(DecimalFormat("#,##0.00"), BugError);
    EXPECT_THROW(DecimalFormat("0.0%"), BugError);
    EXPECT_THROW(DecimalFormat("0.0;(0.0)"), BugError);
    EXPECT_THROW(DecimalFormat("0.00E0 m"), BugError);
}

}  // namespace
