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

}  // namespace
