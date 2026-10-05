#include "QtRocket/util/BigDecimal.h"

#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/util/FloatingDecimal.h"

namespace
{

using QtRocket::BigDecimal;

// OpenRocket has no test of java.math.BigDecimal. Every expectation below is what JDK 17 prints
// for the same text (probes/options-extensions-impl/BigDecimalProbe.java): the unscaled value,
// the scale, toString(), intValue(), longValue() and Double.toString(doubleValue()), or "NFE"
// where the constructor throws NumberFormatException. The same probe's random mode was compared
// with this implementation over 20000 generated texts.

/// The six values of @p value as the probe prints them.
[[nodiscard]] std::string describeAll(const BigDecimal& value)
{
    return std::format("{} {} {} {} {} {}", value.unscaledValue(), value.scale(), value.toString(),
                       value.intValue(), value.longValue(),
                       QtRocket::FloatingDecimal::toJavaFormatString(value.doubleValue()));
}

/// describeAll() of the BigDecimal @p text parses to, or "NFE".
[[nodiscard]] std::string describe(std::string_view text)
{
    const std::optional<BigDecimal> value = BigDecimal::parse(text);
    return value.has_value() ? describeAll(*value) : "NFE";
}

/// The unscaled value, scale and toString() of @p value.
[[nodiscard]] std::string describe(const BigDecimal& value)
{
    return std::format("{} {} {}", value.unscaledValue(), value.scale(), value.toString());
}

/// describe() of BigDecimal.valueOf(@p value), or "NFE".
[[nodiscard]] std::string describeValueOf(double value)
{
    const std::optional<BigDecimal> big = BigDecimal::valueOfDouble(value);
    return big.has_value() ? describe(*big) : "NFE";
}

/// The BigDecimal @p text parses to; zero (and a failed expectation) when it does not parse.
[[nodiscard]] BigDecimal parsed(std::string_view text)
{
    const std::optional<BigDecimal> value = BigDecimal::parse(text);
    EXPECT_TRUE(value.has_value()) << text;
    return value.value_or(BigDecimal{});
}

TEST(BigDecimal, DefaultIsZero)
{
    const BigDecimal zero;
    EXPECT_EQ(describeAll(zero), "0 0 0 0 0 0.0");
    EXPECT_EQ(zero.signum(), 0);
    EXPECT_TRUE(zero == parsed("0"));
}

TEST(BigDecimal, Zeros)
{
    // Every zero has the unscaled value 0, whatever its sign, and keeps its scale.
    EXPECT_EQ(describe("0"), "0 0 0 0 0 0.0");
    EXPECT_EQ(describe("-0"), "0 0 0 0 0 0.0");
    EXPECT_EQ(describe("+0"), "0 0 0 0 0 0.0");
    EXPECT_EQ(describe("0.00"), "0 2 0.00 0 0 0.0");
    EXPECT_EQ(describe("-0.00"), "0 2 0.00 0 0 0.0");
    EXPECT_EQ(describe("0E+3"), "0 -3 0E+3 0 0 0.0");
    EXPECT_EQ(describe("0E-10"), "0 10 0E-10 0 0 0.0");
    EXPECT_EQ(describe("000"), "0 0 0 0 0 0.0");
    EXPECT_EQ(parsed("-0.00").signum(), 0);
}

TEST(BigDecimal, Integers)
{
    EXPECT_EQ(describe("1"), "1 0 1 1 1 1.0");
    EXPECT_EQ(describe("-1"), "-1 0 -1 -1 -1 -1.0");
    EXPECT_EQ(describe("+1"), "1 0 1 1 1 1.0");
    EXPECT_EQ(describe("123"), "123 0 123 123 123 123.0");
    EXPECT_EQ(describe("-123"), "-123 0 -123 -123 -123 -123.0");
    EXPECT_EQ(parsed("123").signum(), 1);
    EXPECT_EQ(parsed("-123").signum(), -1);
}

TEST(BigDecimal, IntValueKeepsTheLowBits)
{
    // intValue() and longValue() wrap, where the (int) cast of a double saturates.
    EXPECT_EQ(describe("2147483647"),
              "2147483647 0 2147483647 2147483647 2147483647 2.147483647E9");
    EXPECT_EQ(describe("2147483648"),
              "2147483648 0 2147483648 -2147483648 2147483648 2.147483648E9");
    EXPECT_EQ(describe("-2147483648"),
              "-2147483648 0 -2147483648 -2147483648 -2147483648 -2.147483648E9");
    EXPECT_EQ(describe("-2147483649"),
              "-2147483649 0 -2147483649 2147483647 -2147483649 -2.147483649E9");
    EXPECT_EQ(describe("4294967296"), "4294967296 0 4294967296 0 4294967296 4.294967296E9");
    EXPECT_EQ(describe("9223372036854775807"),
              "9223372036854775807 0 9223372036854775807 -1 "
              "9223372036854775807 9.223372036854776E18");
    EXPECT_EQ(describe("9223372036854775808"),
              "9223372036854775808 0 9223372036854775808 0 "
              "-9223372036854775808 9.223372036854776E18");
    EXPECT_EQ(describe("-9223372036854775808"),
              "-9223372036854775808 0 -9223372036854775808 0 "
              "-9223372036854775808 -9.223372036854776E18");
    EXPECT_EQ(describe("-9223372036854775809"),
              "-9223372036854775809 0 -9223372036854775809 -1 "
              "9223372036854775807 -9.223372036854776E18");
    EXPECT_EQ(describe("18446744073709551616"),
              "18446744073709551616 0 18446744073709551616 0 0 1.8446744073709552E19");
    EXPECT_EQ(describe("18446744073709551617"),
              "18446744073709551617 0 18446744073709551617 1 1 1.8446744073709552E19");
    EXPECT_EQ(describe("123456789012345678901234567890"),
              "123456789012345678901234567890 0 123456789012345678901234567890 1312754386 "
              "-4362896299872285998 1.2345678901234568E29");
    EXPECT_EQ(describe("-123456789012345678901234567890"),
              "-123456789012345678901234567890 0 -123456789012345678901234567890 -1312754386 "
              "4362896299872285998 -1.2345678901234568E29");
}

TEST(BigDecimal, Fractions)
{
    // The fraction is dropped by intValue() and longValue(), towards zero.
    EXPECT_EQ(describe("00.10"), "10 2 0.10 0 0 0.1");
    EXPECT_EQ(describe("0.10"), "10 2 0.10 0 0 0.1");
    EXPECT_EQ(describe("1.5"), "15 1 1.5 1 1 1.5");
    EXPECT_EQ(describe("-1.5"), "-15 1 -1.5 -1 -1 -1.5");
    EXPECT_EQ(describe("2.9999"), "29999 4 2.9999 2 2 2.9999");
    EXPECT_EQ(describe("-2.9999"), "-29999 4 -2.9999 -2 -2 -2.9999");
    EXPECT_EQ(describe("0.1"), "1 1 0.1 0 0 0.1");
    EXPECT_EQ(describe("0.5"), "5 1 0.5 0 0 0.5");
    EXPECT_EQ(describe(".5"), "5 1 0.5 0 0 0.5");
    EXPECT_EQ(describe("5."), "5 0 5 5 5 5.0");
    EXPECT_EQ(describe("-.5"), "-5 1 -0.5 0 0 -0.5");
    EXPECT_EQ(describe("0.30000000000000004"),
              "30000000000000004 17 0.30000000000000004 0 0 0.30000000000000004");
}

TEST(BigDecimal, Exponents)
{
    EXPECT_EQ(describe("1E3"), "1 -3 1E+3 1000 1000 1000.0");
    EXPECT_EQ(describe("1e3"), "1 -3 1E+3 1000 1000 1000.0");
    EXPECT_EQ(describe("1E+3"), "1 -3 1E+3 1000 1000 1000.0");
    EXPECT_EQ(describe("1E-3"), "1 3 0.001 0 0 0.001");
    EXPECT_EQ(describe("1.0E-4"), "10 5 0.00010 0 0 1.0E-4");
    EXPECT_EQ(describe("1.00E+2"), "100 0 100 100 100 100.0");
    EXPECT_EQ(describe("12.34E+1"), "1234 1 123.4 123 123 123.4");
    EXPECT_EQ(describe("12.34E-1"), "1234 3 1.234 1 1 1.234");
    EXPECT_EQ(describe("100E-2"), "100 2 1.00 1 1 1.0");
    EXPECT_EQ(describe("1E+0000000000000000001"), "1 -1 1E+1 10 10 10.0");
}

TEST(BigDecimal, ToStringTurnsScientificBelowAnAdjustedExponentOfMinusSix)
{
    EXPECT_EQ(describe("1E-6"), "1 6 0.000001 0 0 1.0E-6");
    EXPECT_EQ(describe("1E-7"), "1 7 1E-7 0 0 1.0E-7");
    EXPECT_EQ(describe("0.000001"), "1 6 0.000001 0 0 1.0E-6");
    EXPECT_EQ(describe("0.0000001"), "1 7 1E-7 0 0 1.0E-7");
    EXPECT_EQ(describe("123E-9"), "123 9 1.23E-7 0 0 1.23E-7");
    EXPECT_EQ(describe("1.23E-7"), "123 9 1.23E-7 0 0 1.23E-7");
    EXPECT_EQ(describe("12345.6789E-10"), "123456789 14 0.00000123456789 0 0 1.23456789E-6");
}

TEST(BigDecimal, TrailingZerosOfANegativeScale)
{
    // The low bits of 10^n: none are left from 10^64 on.
    EXPECT_EQ(describe("1E+20"), "1 -20 1E+20 1661992960 7766279631452241920 1.0E20");
    EXPECT_EQ(describe("1E+63"), "1 -63 1E+63 0 -9223372036854775808 1.0E63");
    EXPECT_EQ(describe("1E+64"), "1 -64 1E+64 0 0 1.0E64");
    EXPECT_EQ(describe("3E+63"), "3 -63 3E+63 0 -9223372036854775808 3.0E63");
    EXPECT_EQ(describe("-3E+63"), "-3 -63 -3E+63 0 -9223372036854775808 -3.0E63");
    EXPECT_EQ(describe("7E+19"), "7 -19 7E+19 -984088576 -3786976294838206464 7.0E19");
}

TEST(BigDecimal, DoubleValueIsCorrectlyRounded)
{
    // The exact expansion of the double pi, which Java's new BigDecimal(Math.PI) gives.
    EXPECT_EQ(describe("3.141592653589793115997963468544185161590576171875"),
              "3141592653589793115997963468544185161590576171875 48 "
              "3.141592653589793115997963468544185161590576171875 3 3 3.141592653589793");
    EXPECT_EQ(describe("3.141592653589793"),
              "3141592653589793 15 3.141592653589793 3 3 3.141592653589793");
    // Ties go to the even double.
    EXPECT_EQ(describe("9007199254740993"),
              "9007199254740993 0 9007199254740993 1 9007199254740993 9.007199254740992E15");
    EXPECT_EQ(describe("9007199254740992.5"),
              "90071992547409925 1 9007199254740992.5 0 9007199254740992 9.007199254740992E15");
}

TEST(BigDecimal, DoubleValueBeyondTheDoubleRange)
{
    EXPECT_EQ(describe("1E+400"), "1 -400 1E+400 0 0 Infinity");
    EXPECT_EQ(describe("-1E+400"), "-1 -400 -1E+400 0 0 -Infinity");
    EXPECT_EQ(describe("1E-400"), "1 400 1E-400 0 0 0.0");
    EXPECT_EQ(describe("4.9E-324"), "49 325 4.9E-324 0 0 4.9E-324");
    EXPECT_EQ(describe("2.4E-324"), "24 325 2.4E-324 0 0 0.0");
    EXPECT_EQ(describe("2.5E-324"), "25 325 2.5E-324 0 0 4.9E-324");
    EXPECT_EQ(describe("1.7976931348623157E+308"),
              "17976931348623157 -292 1.7976931348623157E+308 0 0 1.7976931348623157E308");
    EXPECT_EQ(describe("1.7976931348623159E+308"),
              "17976931348623159 -292 1.7976931348623159E+308 0 0 Infinity");
    // A negative value below the double range is a negative zero (ConfigProbe: -1E-400).
    const double underflow = parsed("-1E-400").doubleValue();
    EXPECT_EQ(underflow, 0.0);
    EXPECT_TRUE(std::signbit(underflow));
    EXPECT_FALSE(std::signbit(parsed("-0.00").doubleValue()));
}

TEST(BigDecimal, ANegativeValueFarBeyondTheDoubleRangeKeepsItsSign)
{
    // Further out than the texts above the exponent alone decides, without parsing the text
    // (BigNegativeProbe: -Infinity, bits fff0000000000000, and -0.0, bits 8000000000000000).
    EXPECT_EQ(describe("-1E+401"), "-1 -401 -1E+401 0 0 -Infinity");
    EXPECT_EQ(describe("-9.99E+400"), "-999 -398 -9.99E+400 0 0 -Infinity");
    EXPECT_EQ(describe("-1E+2147483647"), "-1 -2147483647 -1E+2147483647 0 0 -Infinity");
    EXPECT_EQ(parsed("-1E+401").doubleValue(), -std::numeric_limits<double>::infinity());
    EXPECT_EQ(parsed("-1E+2147483647").doubleValue(), -std::numeric_limits<double>::infinity());
    EXPECT_EQ(describe("-1E-401"), "-1 401 -1E-401 0 0 -0.0");
    EXPECT_EQ(describe("-1E-2147483647"), "-1 2147483647 -1E-2147483647 0 0 -0.0");
    const double tiny = parsed("-1E-401").doubleValue();
    EXPECT_EQ(tiny, 0.0);
    EXPECT_TRUE(std::signbit(tiny));
    const double tiniest = parsed("-1E-2147483647").doubleValue();
    EXPECT_EQ(tiniest, 0.0);
    EXPECT_TRUE(std::signbit(tiniest));
    // The positive twins stay positive.
    EXPECT_EQ(parsed("1E+401").doubleValue(), std::numeric_limits<double>::infinity());
    EXPECT_FALSE(std::signbit(parsed("1E-401").doubleValue()));
}

TEST(BigDecimal, ScaleLimits)
{
    EXPECT_EQ(describe("1E+2147483647"), "1 -2147483647 1E+2147483647 0 0 Infinity");
    EXPECT_EQ(describe("1E-2147483647"), "1 2147483647 1E-2147483647 0 0 0.0");
    // The exponent must fit an int, and so must the scale.
    EXPECT_EQ(describe("1E+2147483648"), "NFE");
    EXPECT_EQ(describe("1E-2147483648"), "NFE");
    EXPECT_EQ(describe("1.5E-2147483647"), "NFE");
    // toString()'s exponent is the adjusted one, which may exceed an int (and then does not read
    // back, in Java either).
    EXPECT_EQ(describe("10E+2147483647"), "10 -2147483647 1.0E+2147483648 0 0 Infinity");
    EXPECT_EQ(describe("1.0E+2147483648"), "NFE");
    EXPECT_EQ(describe("0.1E+2147483647"), "1 -2147483646 1E+2147483646 0 0 Infinity");
    // More than ten exponent digits once the leading zeros beyond ten characters are skipped.
    EXPECT_EQ(describe("1E+12345678901"), "NFE");
}

TEST(BigDecimal, RejectsWhatJavaRejects)
{
    EXPECT_EQ(describe(""), "NFE");
    EXPECT_EQ(describe("+"), "NFE");
    EXPECT_EQ(describe("-"), "NFE");
    EXPECT_EQ(describe("."), "NFE");
    EXPECT_EQ(describe("E5"), "NFE");
    EXPECT_EQ(describe("1E"), "NFE");
    EXPECT_EQ(describe("1E+"), "NFE");
    EXPECT_EQ(describe("1.2.3"), "NFE");
    EXPECT_EQ(describe(" 1"), "NFE");
    EXPECT_EQ(describe("1 "), "NFE");
    EXPECT_EQ(describe("1x"), "NFE");
    EXPECT_EQ(describe("0x10"), "NFE");
    EXPECT_EQ(describe("NaN"), "NFE");
    EXPECT_EQ(describe("Infinity"), "NFE");
    EXPECT_EQ(describe("1,5"), "NFE");
    EXPECT_EQ(describe("--1"), "NFE");
    EXPECT_EQ(describe("+-1"), "NFE");
    EXPECT_EQ(describe("1E5E5"), "NFE");
    EXPECT_EQ(describe("1e+-5"), "NFE");
    EXPECT_EQ(describe("1_000"), "NFE");
    EXPECT_EQ(describe("1d"), "NFE");
    EXPECT_EQ(describe("1f"), "NFE");
}

TEST(BigDecimal, DigitsOfOtherScriptsAreRejected)
{
    // Deviation: Java reads the Arabic-Indic digit one (U+0661) as 1.
    EXPECT_EQ(describe("\xD9\xA1"), "NFE");
}

TEST(BigDecimal, ValueOfLong)
{
    EXPECT_EQ(describe(BigDecimal::valueOf(0)), "0 0 0");
    EXPECT_EQ(describe(BigDecimal::valueOf(1)), "1 0 1");
    EXPECT_EQ(describe(BigDecimal::valueOf(-1)), "-1 0 -1");
    EXPECT_EQ(describe(BigDecimal::valueOf(100)), "100 0 100");
    EXPECT_EQ(describe(BigDecimal::valueOf(std::numeric_limits<std::int64_t>::max())),
              "9223372036854775807 0 9223372036854775807");
    EXPECT_EQ(describe(BigDecimal::valueOf(std::numeric_limits<std::int64_t>::min())),
              "-9223372036854775808 0 -9223372036854775808");
    EXPECT_EQ(BigDecimal::valueOf(std::numeric_limits<std::int64_t>::min()).longValue(),
              std::numeric_limits<std::int64_t>::min());
    EXPECT_TRUE(BigDecimal::valueOf(-5) == parsed("-5"));
}

TEST(BigDecimal, ValueOfDouble)
{
    // BigDecimal.valueOf(double) goes through Double.toString, so 100.0 has scale 1.
    EXPECT_EQ(describeValueOf(0.0), "0 1 0.0");
    EXPECT_EQ(describeValueOf(-0.0), "0 1 0.0");
    EXPECT_EQ(describeValueOf(1.0), "10 1 1.0");
    EXPECT_EQ(describeValueOf(0.1), "1 1 0.1");
    EXPECT_EQ(describeValueOf(100.0), "1000 1 100.0");
    EXPECT_EQ(describeValueOf(1.0E-4), "10 5 0.00010");
    EXPECT_EQ(describeValueOf(1.0E7), "10 -6 1.0E+7");
    // JDK 17 prints 1.0E23 as 9.999999999999999E22.
    EXPECT_EQ(describeValueOf(1.0E23), "9999999999999999 -7 9.999999999999999E+22");
    EXPECT_EQ(describeValueOf(123456.789), "123456789 3 123456.789");
    EXPECT_EQ(describeValueOf(3.141592653589793), "3141592653589793 15 3.141592653589793");
    EXPECT_EQ(describeValueOf(std::numeric_limits<double>::denorm_min()), "49 325 4.9E-324");
    EXPECT_EQ(describeValueOf(std::numeric_limits<double>::max()),
              "17976931348623157 -292 1.7976931348623157E+308");
    EXPECT_EQ(describeValueOf(-2.5), "-25 1 -2.5");
    EXPECT_EQ(describeValueOf(1.0E-5), "10 6 0.000010");
    EXPECT_EQ(describeValueOf(9.007199254740992E15), "9007199254740992 0 9007199254740992");
    EXPECT_EQ(describeValueOf(1.15292150460684698E18),
              "115292150460684698 -1 1.15292150460684698E+18");
}

TEST(BigDecimal, ValueOfDoubleRejectsNaNAndInfinities)
{
    EXPECT_EQ(describeValueOf(std::numeric_limits<double>::quiet_NaN()), "NFE");
    EXPECT_EQ(describeValueOf(std::numeric_limits<double>::infinity()), "NFE");
    EXPECT_EQ(describeValueOf(-std::numeric_limits<double>::infinity()), "NFE");
}

TEST(BigDecimal, EqualityIncludesTheScale)
{
    EXPECT_FALSE(parsed("1.0") == parsed("1.00"));
    EXPECT_TRUE(parsed("1.0") == parsed("1.0"));
    EXPECT_TRUE(parsed("0") == parsed("-0"));
    EXPECT_FALSE(parsed("0") == parsed("0.0"));
    EXPECT_FALSE(parsed("1E+2") == parsed("100"));
    EXPECT_TRUE(parsed("100") == parsed("100"));
    EXPECT_FALSE(parsed("-5") == parsed("5"));
    EXPECT_FALSE(parsed("0.10") == parsed("0.1"));
    EXPECT_TRUE(parsed("1E+2") == parsed("1E2"));
    EXPECT_TRUE(parsed("0.10") != parsed("0.1"));
}

TEST(BigDecimal, ToStringReadsBackAsAnEqualValue)
{
    EXPECT_TRUE(parsed(parsed("12345.6789E-10").toString()) == parsed("12345.6789E-10"));
    EXPECT_TRUE(parsed(parsed("-1.00E+2").toString()) == parsed("-1.00E+2"));
    EXPECT_TRUE(parsed(parsed("0E-10").toString()) == parsed("0E-10"));
    EXPECT_TRUE(parsed(parsed("7E+19").toString()) == parsed("7E+19"));
}

}  // namespace
