#include "QtRocket/util/DecimalFormatter.h"

#include <limits>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/util/DecimalFormat.h"

// The first seven tests port OpenRocket's swing utils DecimalFormatterTest; the others pin what
// DecimalFormatter.df() gives on JDK 17 (the "DF" lines of tools/openrocket-probes/plot-model's
// expected.txt). Java's decimal separator is that of the default locale, '.' in en_US, as
// QtRocket's DecimalFormat always writes it.

namespace
{

using QtRocket::DecimalFormat;
using QtRocket::DecimalFormatter;

constexpr char   kSeparator = '.';
constexpr double kNaN       = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf       = std::numeric_limits<double>::infinity();

// Port of DecimalFormatterTest.returnsWholeNumberPatternWhenDecimalsAreNonPositive.
TEST(DecimalFormatter, ReturnsWholeNumberPatternWhenDecimalsAreNonPositive)
{
    const DecimalFormat format = DecimalFormatter::df(12.34, 0, false);

    EXPECT_EQ(format.toPattern(), "#");
    EXPECT_EQ(format.format(12.34), "12");
}

// Port of DecimalFormatterTest.respectsDisplayZerosFlag.
TEST(DecimalFormatter, RespectsDisplayZerosFlag)
{
    const DecimalFormat withZeros         = DecimalFormatter::df(8.0, 2, true);
    const DecimalFormat withoutZeros      = DecimalFormatter::df(8.0, 2, false);
    const std::string   expectedWithZeros = std::string("8") + kSeparator + "00";

    EXPECT_EQ(withZeros.format(8.0), expectedWithZeros);
    EXPECT_EQ(withoutZeros.format(8.0), "8");
    EXPECT_EQ(withZeros.getMinimumFractionDigits(), 2);
    EXPECT_EQ(withoutZeros.getMinimumFractionDigits(), 0);
}

// Port of DecimalFormatterTest.negativeValuesPreserveSignAndFractionDigits.
TEST(DecimalFormatter, NegativeValuesPreserveSignAndFractionDigits)
{
    const DecimalFormat format = DecimalFormatter::df(-8.75, 2, false);

    EXPECT_EQ(format.format(-8.75), std::string("-8") + kSeparator + "75");
    EXPECT_EQ(format.getMaximumFractionDigits(), 2);
}

// Port of DecimalFormatterTest.largeValuesDoNotTriggerExtraPrecision.
TEST(DecimalFormatter, LargeValuesDoNotTriggerExtraPrecision)
{
    const DecimalFormat format = DecimalFormatter::df(123.456, 2, true);

    EXPECT_EQ(format.getMaximumFractionDigits(), 2);
    EXPECT_EQ(format.toPattern(), "#.00");
}

// Port of DecimalFormatterTest.displayZerosFalseStillHonorsActualFractionalPart.
TEST(DecimalFormatter, DisplayZerosFalseStillHonorsActualFractionalPart)
{
    const DecimalFormat format = DecimalFormatter::df(8.5, 2, false);

    EXPECT_EQ(format.format(8.5), std::string("8") + kSeparator + "5");
    EXPECT_EQ(format.getMinimumFractionDigits(), 0);
}

// Port of DecimalFormatterTest.expandsPrecisionForSmallNonZeroValues.
TEST(DecimalFormatter, ExpandsPrecisionForSmallNonZeroValues)
{
    const DecimalFormat format = DecimalFormatter::df(0.00345, 2, false);

    EXPECT_EQ(format.getMaximumFractionDigits(), 4);
}

// Port of DecimalFormatterTest.zeroValueDoesNotForceExtraPrecision.
TEST(DecimalFormatter, ZeroValueDoesNotForceExtraPrecision)
{
    const DecimalFormat format = DecimalFormatter::df(0.0, 3, true);

    EXPECT_EQ(format.toPattern(), "#.000");
    EXPECT_EQ(format.getMaximumFractionDigits(), 3);
}

/// What the probe printed for df(value, decimals, displayZeros): toPattern(), the fraction digits
/// and format(value).
struct DfPin
{
    DfPin(double pinValue, int pinDecimals, bool pinZeros, std::string_view pinPattern,
          int pinMinFraction, int pinMaxFraction, std::string_view pinText)
      : value(pinValue),
        decimals(pinDecimals),
        zeros(pinZeros),
        pattern(pinPattern),
        minFraction(pinMinFraction),
        maxFraction(pinMaxFraction),
        text(pinText)
    {
    }

    double           value;
    int              decimals;
    bool             zeros;
    std::string_view pattern;
    int              minFraction;
    int              maxFraction;
    std::string_view text;
};

void expectDfPin(const DfPin& pin)
{
    SCOPED_TRACE(testing::Message()
                 << "df(" << pin.value << ", " << pin.decimals << ", " << pin.zeros << ")");
    const DecimalFormat format = DecimalFormatter::df(pin.value, pin.decimals, pin.zeros);
    EXPECT_EQ(format.toPattern(), pin.pattern);
    EXPECT_EQ(format.getMinimumFractionDigits(), pin.minFraction);
    EXPECT_EQ(format.getMaximumFractionDigits(), pin.maxFraction);
    EXPECT_EQ(format.format(pin.value), pin.text);
    EXPECT_EQ(DecimalFormatter::format(pin.value, pin.decimals, pin.zeros), pin.text);
}

TEST(DecimalFormatter, TwoDecimalsAsThePlotsToolTipsShowThem)
{
    // (Values away from the powers of ten that pow() and log10() decide: their last bits differ
    // between libraries.)
    for (const DfPin& pin : {
             DfPin(12.34, 2, false, "#0.##", 0, 2, "12.34"),
             DfPin(123.456, 2, false, "#0.##", 0, 2, "123.46"),
             DfPin(0.00345, 2, false, "#0.####", 0, 4, "0.0034"),
             DfPin(0.0, 2, false, "#0.##", 0, 2, "0"),
             DfPin(-0.0, 2, false, "#0.##", 0, 2, "-0"),
             DfPin(0.099, 2, false, "#0.###", 0, 3, "0.099"),
             DfPin(0.05, 2, false, "#0.###", 0, 3, "0.05"),
             DfPin(9.999E-4, 2, false, "#0.#####", 0, 5, "0.001"),
             DfPin(2.6E-4, 2, false, "#0.#####", 0, 5, "0.00026"),
             DfPin(-2.599E-4, 2, false, "#0.#####", 0, 5, "-0.00026"),
             DfPin(1.0E-5, 2, false, "#0.#######", 0, 7, "0.00001"),
             DfPin(-0.00345, 2, false, "#0.####", 0, 4, "-0.0034"),
             DfPin(25.625, 2, false, "#0.##", 0, 2, "25.62"),
             DfPin(0.125, 2, false, "#0.##", 0, 2, "0.12"),
             DfPin(1234567.891, 2, false, "#0.##", 0, 2, "1234567.89"),
             DfPin(kNaN, 2, false, "#0.##", 0, 2, "NaN"),
             DfPin(kInf, 2, false, "#0.##", 0, 2, "\xE2\x88\x9E"),
             DfPin(-kInf, 2, false, "#0.##", 0, 2, "-\xE2\x88\x9E"),
         })
    {
        expectDfPin(pin);
    }
}

TEST(DecimalFormatter, DisplayedZerosHaveNoIntegerZero)
{
    for (const DfPin& pin : {
             DfPin(8.5, 2, true, "#.00", 2, 2, "8.50"),
             DfPin(0.00345, 2, true, "#.0000", 4, 4, ".0034"),
             DfPin(0.0, 2, true, "#.00", 2, 2, ".00"),
             DfPin(-0.0, 2, true, "#.00", 2, 2, "-.00"),
             DfPin(0.099, 2, true, "#.000", 3, 3, ".099"),
             DfPin(-2.599E-4, 2, true, "#.00000", 5, 5, "-.00026"),
             DfPin(0.125, 3, true, "#.000", 3, 3, ".125"),
             DfPin(12.34, 3, true, "#.000", 3, 3, "12.340"),
             DfPin(2.6E-4, 3, true, "#.000000", 6, 6, ".000260"),
         })
    {
        expectDfPin(pin);
    }
}

TEST(DecimalFormatter, OtherDecimals)
{
    for (const DfPin& pin : {
             DfPin(123.456, 3, false, "#0.###", 0, 3, "123.456"),
             DfPin(0.00345, 3, false, "#0.#####", 0, 5, "0.00345"),
             DfPin(0.05, 3, false, "#0.###", 0, 3, "0.05"),
             DfPin(1.0E-5, 3, false, "#0.########", 0, 8, "0.00001"),
             DfPin(12.34, -1, false, "#", 0, 0, "12"),
             DfPin(0.00345, -1, true, "#", 0, 0, "0"),
             DfPin(0.5, 0, false, "#", 0, 0, "0"),
             DfPin(1.0E-310, 0, true, "#", 0, 0, "0"),
             DfPin(12.34, 1, false, "#0.#", 0, 1, "12.3"),
             DfPin(0.00345, 1, false, "#0.###", 0, 3, "0.003"),
             DfPin(0.00345, 1, true, "#.000", 3, 3, ".003"),
             DfPin(0.5, 1, true, "#.0", 1, 1, ".5"),
             DfPin(0.05, 1, false, "#0.##", 0, 2, "0.05"),
             DfPin(12.34, 4, true, "#.0000", 4, 4, "12.3400"),
             DfPin(0.05, 4, false, "#0.####", 0, 4, "0.05"),
         })
    {
        expectDfPin(pin);
    }
}

TEST(DecimalFormatter, TheSmallestNormalValuesGetTheirDigits)
{
    // 1e-308: floor(log10(1e308)) = 308 more decimals.
    const DecimalFormat format = DecimalFormatter::df(1.0E-308, 2, false);
    EXPECT_EQ(format.getMaximumFractionDigits(), 310);
    EXPECT_EQ(format.toPattern(), "#0." + std::string(310, '#'));
    EXPECT_EQ(DecimalFormatter::df(2.2250738585072014E-308, 3, true).getMaximumFractionDigits(),
              310);
}

TEST(DecimalFormatter, ASubnormalValueWhoseInverseOverflowsGetsTheMostDigits)
{
    // Java throws NegativeArraySizeException for these (its int cast of an infinite digit
    // count saturates and the sum overflows: the probe's "-2147483647"); see the class comment.
    for (const double value : {1.0E-310, 4.9E-324, -1.0E-310})
    {
        const DecimalFormat format = DecimalFormatter::df(value, 2, false);
        EXPECT_EQ(format.getMaximumFractionDigits(), DecimalFormatter::kMaximumFractionDigits + 2);
        EXPECT_EQ(format.getMinimumFractionDigits(), 0);
    }
    EXPECT_EQ(DecimalFormatter::df(1.0E-310, 3, true).getMinimumFractionDigits(),
              DecimalFormatter::kMaximumFractionDigits + 3);
    // 340 fraction digits, the most DecimalFormat writes of a double, hold 1e-310's leading
    // digits.
    const std::string text = DecimalFormatter::format(1.0E-310, 2, false);
    EXPECT_TRUE(text.starts_with("0." + std::string(309, '0'))) << text;
    EXPECT_NE(text.find_first_of("123456789"), std::string::npos) << text;
}

}  // namespace
