#include "QtRocket/util/FloatingDecimal.h"

#include <limits>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::BugError;
namespace FloatingDecimal = QtRocket::FloatingDecimal;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// The strings are JDK 17's Double.toString and the fields its FloatingDecimal holds after the
// conversion (read through jdk.internal.math).

TEST(FloatingDecimal, JavaFormatStringIsJdk17DoubleToString)
{
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(0.3), "0.3");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(1.0 / 3), "0.3333333333333333");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(0.001), "0.001");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(100.0), "100.0");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(1234567.0), "1234567.0");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(12345678.0), "1.2345678E7");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(1e7), "1.0E7");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(5e-6), "5.0E-6");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(1e-5), "1.0E-5");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(-9.5e18), "-9.5E18");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(123456789012345678.0), "1.2345678901234568E17");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(1.7976931348623157e308),
              "1.7976931348623157E308");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(2.2250738585072014E-308),
              "2.2250738585072014E-308");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(-0.0), "-0.0");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(0.0), "0.0");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(kNaN), "NaN");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(kInf), "Infinity");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(-kInf), "-Infinity");
}

TEST(FloatingDecimal, KeepsJdk17sNonShortestDigits)
{
    // JDK-4511638: the shortest digits would give "1.0E23", "2.0E23", "5.0E-324",
    // "5.902958103587057E20" and "8.41E21"; an integer below 2^63 keeps its digits down to a
    // double's precision.
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(1e23), "9.999999999999999E22");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(2e23), "1.9999999999999998E23");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(4.9e-324), "4.9E-324");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(0x1p69), "5.9029581035870565E20");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(8.41e21), "8.409999999999999E21");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(0x1p60), "1.15292150460684698E18");
}

TEST(FloatingDecimal, FlagsTellHowTheLastDigitRelatesToTheValue)
{
    // 0.15 is 0.1499999999999999944...: the digits "14" were rounded up to "15".
    const FloatingDecimal::BinaryToAscii below = FloatingDecimal::binaryToAscii(0.15);
    EXPECT_EQ(below.digits, "15");
    EXPECT_EQ(below.decimalExponent, 0);
    EXPECT_TRUE(below.digitsRoundedUp);
    EXPECT_FALSE(below.decimalDigitsExact);

    // 0.45 is 0.4500000000000000111...: truncated.
    const FloatingDecimal::BinaryToAscii above = FloatingDecimal::binaryToAscii(0.45);
    EXPECT_EQ(above.digits, "45");
    EXPECT_FALSE(above.digitsRoundedUp);
    EXPECT_FALSE(above.decimalDigitsExact);

    const FloatingDecimal::BinaryToAscii exact = FloatingDecimal::binaryToAscii(-0.25);
    EXPECT_TRUE(exact.negative);
    EXPECT_EQ(exact.digits, "25");
    EXPECT_FALSE(exact.digitsRoundedUp);
    EXPECT_TRUE(exact.decimalDigitsExact);

    // The exact path of an integer below 2^63 sets neither flag.
    const FloatingDecimal::BinaryToAscii integer = FloatingDecimal::binaryToAscii(1235000.0);
    EXPECT_EQ(integer.digits, "1235");
    EXPECT_EQ(integer.decimalExponent, 7);
    EXPECT_FALSE(integer.digitsRoundedUp);
    EXPECT_FALSE(integer.decimalDigitsExact);

    const FloatingDecimal::BinaryToAscii zero = FloatingDecimal::binaryToAscii(-0.0);
    EXPECT_TRUE(zero.negative);
    EXPECT_EQ(zero.digits, "0");
    EXPECT_EQ(zero.decimalExponent, 0);
}

TEST(FloatingDecimal, TheFormattersModeAlwaysGeneratesASecondDigit)
{
    // The compatible mode (Double.toString, DecimalFormat) forces a second digit only in E-form.
    EXPECT_EQ(FloatingDecimal::binaryToAscii(0.5).digits, "5");
    EXPECT_EQ(FloatingDecimal::binaryToAscii(0.5, false).digits, "50");
    EXPECT_EQ(FloatingDecimal::binaryToAscii(0.3, false).digits, "30");
    EXPECT_TRUE(FloatingDecimal::binaryToAscii(0.3, false).digitsRoundedUp);
    EXPECT_EQ(FloatingDecimal::binaryToAscii(5e-6).digits, "50");
    EXPECT_EQ(FloatingDecimal::binaryToAscii(5e-6, false).digits, "50");
    EXPECT_EQ(FloatingDecimal::binaryToAscii(5e-6).decimalExponent, -5);
    EXPECT_EQ(FloatingDecimal::binaryToAscii(4.9e-324).digits, "49");
    EXPECT_EQ(FloatingDecimal::binaryToAscii(4.9e-324).decimalExponent, -323);
    // The exact path for integers is the same in both modes.
    EXPECT_EQ(FloatingDecimal::binaryToAscii(1234.0, false).digits, "1234");
}

TEST(FloatingDecimal, NaNAndInfinitiesHaveNoDigits)
{
    EXPECT_THROW(static_cast<void>(FloatingDecimal::binaryToAscii(kNaN)), BugError);
    EXPECT_THROW(static_cast<void>(FloatingDecimal::binaryToAscii(kInf)), BugError);
}

// The tests below pin the places where JDK 17's conversion departs from a shortest-digits one,
// each with a value that shows it. Every expectation is JDK 17's Double.toString or the state of
// its converter (jdk.internal.math.FloatingDecimal, read through reflection in a JDK 17 run).

TEST(FloatingDecimal, IntegersBelow2To63AreWrittenFromTheirExactValue)
{
    // Exact up to 2^58; from there rounded half up to tens, from 2^61 to hundreds. No flags.
    const FloatingDecimal::BinaryToAscii exact = FloatingDecimal::binaryToAscii(0x1p57);
    EXPECT_EQ(exact.digits, "144115188075855872");
    EXPECT_EQ(exact.decimalExponent, 18);
    EXPECT_FALSE(exact.digitsRoundedUp);
    EXPECT_FALSE(exact.decimalDigitsExact);
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(0x1p57), "1.44115188075855872E17");

    // 288230376151711744: the 4 is dropped; 288230376151711808: the 8 rounds the 0 up.
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(0x1p58), "2.8823037615171174E17");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(0x1p58 + 64), "2.8823037615171181E17");

    // 9223372036854774784, the largest double below 2^63: 84 rounds up to a hundred, unflagged.
    const FloatingDecimal::BinaryToAscii largest = FloatingDecimal::binaryToAscii(0x1p63 - 1024);
    EXPECT_EQ(largest.digits, "92233720368547748");
    EXPECT_EQ(largest.decimalExponent, 19);
    EXPECT_FALSE(largest.digitsRoundedUp);
    EXPECT_FALSE(largest.decimalDigitsExact);
}

TEST(FloatingDecimal, TheUpperEndCountsInOnlyInBigIntegerArithmetic)
{
    // Both values lie exactly half a gap below a shorter decimal: 1e23 below 10^23, where JDK 17
    // computes in 64-bit integers and leaves the end out, and the same significand at 2^87
    // (204799999999999982820130816) below 2.048e26, where it computes in big integers and
    // counts the end in.
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(0x1.52d02c7e14af6p76), "9.999999999999999E22");
    const FloatingDecimal::BinaryToAscii inclusive =
        FloatingDecimal::binaryToAscii(0x1.52d02c7e14af6p87);
    EXPECT_EQ(inclusive.digits, "2048");
    EXPECT_EQ(inclusive.decimalExponent, 27);
    EXPECT_TRUE(inclusive.digitsRoundedUp);
    EXPECT_FALSE(inclusive.decimalDigitsExact);
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(0x1.52d02c7e14af6p87), "2.048E26");
}

TEST(FloatingDecimal, Jdk17s64BitUpperEndTestWrapsAround)
{
    // 0x1.7606c1e42a3b5p84 = 28260635462337084975480832: after "28260635462337084" the rest,
    // 0.975 of a unit, would round up ("...085") in exact arithmetic; JDK 17's 64-bit sum wraps
    // and the digit stays.
    const FloatingDecimal::BinaryToAscii wrapped =
        FloatingDecimal::binaryToAscii(0x1.7606c1e42a3b5p84);
    EXPECT_EQ(wrapped.digits, "28260635462337084");
    EXPECT_EQ(wrapped.decimalExponent, 26);
    EXPECT_FALSE(wrapped.digitsRoundedUp);
    EXPECT_FALSE(wrapped.decimalDigitsExact);
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(0x1.26732daed94d7p84), "2.2248006613431304E25");
    // 2^85 is in the same range but does not reach the wrap.
    const FloatingDecimal::BinaryToAscii power = FloatingDecimal::binaryToAscii(0x1p85);
    EXPECT_EQ(power.digits, "38685626227668134");
    EXPECT_TRUE(power.digitsRoundedUp);
}

TEST(FloatingDecimal, ATieOfTheLastDigitGoesToTheEvenDigit)
{
    // 2^-25 = 2.98023223876953125e-8 and 2^50 + 0.25 end in a 5 one place beyond the digits
    // generated, inside the rounding interval: the last digit stays even, unflagged.
    const FloatingDecimal::BinaryToAscii down = FloatingDecimal::binaryToAscii(0x1p-25);
    EXPECT_EQ(down.digits, "29802322387695312");
    EXPECT_EQ(down.decimalExponent, -7);
    EXPECT_FALSE(down.digitsRoundedUp);
    EXPECT_FALSE(down.decimalDigitsExact);
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(0x1p50 + 0.25), "1.1258999068426242E15");
    // 2^50 + 0.75: the odd 7 goes up to 8.
    const FloatingDecimal::BinaryToAscii up = FloatingDecimal::binaryToAscii(0x1p50 + 0.75);
    EXPECT_EQ(up.digits, "11258999068426248");
    EXPECT_TRUE(up.digitsRoundedUp);
}

TEST(FloatingDecimal, ASingleBitSignificandGetsAQuarterGapMargin)
{
    // Also where the neighbours are equally far on both sides: the subnormal powers of two. With
    // half a gap, "1.6E-322" and "8.095E-320" would read back as 2^-1069 and 2^-1060 too.
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(0x1p-1069), "1.58E-322");
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(0x1p-1060), "8.0948E-320");
    EXPECT_TRUE(FloatingDecimal::binaryToAscii(0x1p-1060).digitsRoundedUp);
    // Above 2^-1022 the lower gap is half the upper one, and 2^69 gets a digit more than it needs.
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(0x1p69), "5.9029581035870565E20");
}

TEST(FloatingDecimal, AnEstimateOneTooHighStartsWithARoundedUpZero)
{
    // The nearest doubles to 1e-7 and 1e-322 lie within their margin below the power of ten, and
    // the decimal exponent estimate is one too high for both: "0" then "9", rounded up to "10".
    const FloatingDecimal::BinaryToAscii tenth = FloatingDecimal::binaryToAscii(1e-7);
    EXPECT_EQ(tenth.digits, "10");
    EXPECT_EQ(tenth.decimalExponent, -6);
    EXPECT_TRUE(tenth.digitsRoundedUp);
    EXPECT_FALSE(tenth.decimalDigitsExact);
    const FloatingDecimal::BinaryToAscii tiny = FloatingDecimal::binaryToAscii(1e-322);
    EXPECT_EQ(tiny.digits, "10");
    EXPECT_EQ(tiny.decimalExponent, -321);
    EXPECT_TRUE(tiny.digitsRoundedUp);
    // 1e-321 is the only such power of ten whose estimate is right: "99" carries to "10", which
    // leaves the flag unset.
    const FloatingDecimal::BinaryToAscii carried = FloatingDecimal::binaryToAscii(1e-321);
    EXPECT_EQ(carried.digits, "10");
    EXPECT_EQ(carried.decimalExponent, -320);
    EXPECT_FALSE(carried.digitsRoundedUp);
    EXPECT_EQ(FloatingDecimal::toJavaFormatString(1e-321), "1.0E-321");
}

}  // namespace
