#include "QtRocket/unit/GeneralUnit.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/unit/FixedPrecisionUnit.h"
#include "QtRocket/unit/InchUnit.h"
#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::BugError;
using QtRocket::GeneralUnit;
using QtRocket::Tick;
using QtRocket::Unit;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// ---- QtRocket additions ----

TEST(GeneralUnit, RoundMatchesOpenRocket)
{
    const GeneralUnit g2(1, "u");              // 2 significant digits, tenths
    const GeneralUnit g3(1, "u", 3, 100);      // 3 significant digits, hundredths
    const GeneralUnit g1(1, "u", 1, 10, 0.1);  // 1 significant digit
    EXPECT_EQ(g2.getSignificantNumbers(), 2);
    EXPECT_EQ(g2.getDecimalRounding(), 10);
    EXPECT_EQ(g2.getStepValue(), 1.0);
    EXPECT_EQ(g1.getStepValue(), 0.1);

    EXPECT_DOUBLE_EQ(g2.round(0.04), 0.0);
    EXPECT_DOUBLE_EQ(g3.round(0.04), 0.04);
    EXPECT_DOUBLE_EQ(g2.round(0.05), 0.0);  // half to even
    EXPECT_DOUBLE_EQ(g2.round(0.15), 0.2);
    EXPECT_DOUBLE_EQ(g2.round(0.25), 0.2);
    EXPECT_DOUBLE_EQ(g2.round(0.35), 0.4);
    EXPECT_DOUBLE_EQ(g2.round(9.94), 9.9);
    EXPECT_DOUBLE_EQ(g3.round(9.94), 9.94);
    EXPECT_DOUBLE_EQ(g1.round(9.94), 10.0);
    EXPECT_DOUBLE_EQ(g2.round(9.95), 10.0);
    EXPECT_DOUBLE_EQ(g2.round(10.0), 10.0);
    EXPECT_DOUBLE_EQ(g2.round(10.4), 10.0);
    EXPECT_DOUBLE_EQ(g3.round(10.4), 10.4);
    EXPECT_DOUBLE_EQ(g1.round(10.4), 10.0);
    EXPECT_DOUBLE_EQ(g2.round(10.5), 10.0);
    EXPECT_DOUBLE_EQ(g2.round(11.5), 12.0);
    EXPECT_DOUBLE_EQ(g1.round(11.5), 10.0);
    EXPECT_DOUBLE_EQ(g2.round(99.0), 99.0);
    EXPECT_DOUBLE_EQ(g1.round(99.0), 100.0);
    EXPECT_DOUBLE_EQ(g2.round(99.5), 100.0);
    EXPECT_DOUBLE_EQ(g3.round(99.5), 99.5);
    EXPECT_DOUBLE_EQ(g2.round(101.0), 100.0);
    EXPECT_DOUBLE_EQ(g3.round(101.0), 101.0);
    EXPECT_DOUBLE_EQ(g2.round(105.0), 100.0);
    EXPECT_DOUBLE_EQ(g2.round(115.0), 120.0);
    EXPECT_DOUBLE_EQ(g1.round(115.0), 100.0);
    EXPECT_DOUBLE_EQ(g1.round(150.0), 200.0);
    EXPECT_DOUBLE_EQ(g2.round(999.0), 1000.0);
    EXPECT_DOUBLE_EQ(g3.round(999.0), 999.0);
    EXPECT_DOUBLE_EQ(g2.round(1049.0), 1000.0);
    EXPECT_DOUBLE_EQ(g3.round(1049.0), 1050.0);
    EXPECT_DOUBLE_EQ(g2.round(1050.0), 1000.0);
    EXPECT_DOUBLE_EQ(g2.round(1051.0), 1100.0);
    EXPECT_DOUBLE_EQ(g3.round(1051.0), 1050.0);
    EXPECT_DOUBLE_EQ(g2.round(12345.0), 12000.0);
    EXPECT_DOUBLE_EQ(g3.round(12345.0), 12300.0);
    EXPECT_DOUBLE_EQ(g1.round(12345.0), 10000.0);
    EXPECT_DOUBLE_EQ(g2.round(15000.0), 15000.0);
    EXPECT_DOUBLE_EQ(g1.round(15000.0), 20000.0);
    EXPECT_DOUBLE_EQ(g1.round(25000.0), 20000.0);
    EXPECT_DOUBLE_EQ(g2.round(-0.05), -0.0);
    EXPECT_DOUBLE_EQ(g3.round(-0.05), -0.05);
    EXPECT_DOUBLE_EQ(g2.round(-1.25), -1.2);
    EXPECT_DOUBLE_EQ(g2.round(-105.0), -105.0);  // negatives never reach the significant path
    EXPECT_DOUBLE_EQ(g2.round(0.0), 0.0);
    EXPECT_TRUE(std::isnan(g2.round(kNaN)));
    // Deviation: OpenRocket never returns for an infinity.
    EXPECT_EQ(g2.round(kInf), kInf);
    EXPECT_EQ(g2.round(-kInf), -kInf);
}

TEST(GeneralUnit, NextAndPreviousStepByOne)
{
    const GeneralUnit g(1, "u");
    EXPECT_EQ(g.getNextValue(2.5), 3.5);
    EXPECT_EQ(g.getPreviousValue(2.5), 1.5);
}

TEST(GeneralUnit, RejectsNonPositiveSignificantNumbersAndRounding)
{
    EXPECT_THROW(GeneralUnit(1, "u", 0), BugError);
    EXPECT_THROW(GeneralUnit(1, "u", 2, 0), BugError);
    EXPECT_THROW(GeneralUnit(1, "u", -1, 10), BugError);
}

// ---- getTicks ----

// The positions are the exact doubles Java computes (pos * minstep, then fromUnit), so they are
// compared exactly.
void expectTick(const Tick& actual, const Tick& expected, std::size_t index)
{
    EXPECT_EQ(actual.value, expected.value) << "tick " << index;
    EXPECT_EQ(actual.unitValue, expected.unitValue) << "tick " << index;
    EXPECT_EQ(actual.major, expected.major) << "tick " << index;
    EXPECT_EQ(actual.notable, expected.notable) << "tick " << index;
}

void expectTicks(const std::vector<Tick>& actual, const std::vector<Tick>& expected)
{
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); i++)
    {
        expectTick(actual[i], expected[i], i);
    }
}

/// GeneralUnit.main's printTicks(0, 100, 1, 10): every tenth major, every fifth notable.
void expectTicksToAHundred(const std::vector<Tick>& hundred)
{
    ASSERT_EQ(hundred.size(), 101U);
    for (std::size_t i = 0; i < hundred.size(); i++)
    {
        EXPECT_EQ(hundred[i].value, static_cast<double>(i));
        EXPECT_EQ(hundred[i].major, i % 10 == 0) << i;
        EXPECT_EQ(hundred[i].notable, i % 100 == 0 || (i % 10 != 0 && i % 5 == 0)) << i;
    }
}

Tick tick(double value, double unitValue, bool major, bool notable)
{
    return {.value = value, .unitValue = unitValue, .major = major, .notable = notable};
}

TEST(GeneralUnit, TicksMatchOpenRocket)
{
    const Unit& none = Unit::noUnit();

    expectTicksToAHundred(none.getTicks(0, 100, 1, 10));

    // printTicks(4.7, 11.0, 0.15, 0.7)
    expectTicks(
        none.getTicks(4.7, 11.0, 0.15, 0.7),
        {tick(5.0, 5.0, true, false), tick(5.5, 5.5, false, true), tick(6.0, 6.0, true, false),
         tick(6.5, 6.5, false, true), tick(7.0, 7.0, true, false), tick(7.5, 7.5, false, true),
         tick(8.0, 8.0, true, false), tick(8.5, 8.5, false, true), tick(9.0, 9.0, true, false),
         tick(9.5, 9.5, false, true), tick(10.0, 10.0, true, true), tick(10.5, 10.5, false, true),
         tick(11.0, 11.0, true, false)});

    expectTicks(
        none.getTicks(0, 5, 0.5, 1),
        {tick(0.0, 0.0, true, true), tick(0.5, 0.5, false, true), tick(1.0, 1.0, true, false),
         tick(1.5, 1.5, false, true), tick(2.0, 2.0, true, false), tick(2.5, 2.5, false, true),
         tick(3.0, 3.0, true, false), tick(3.5, 3.5, false, true), tick(4.0, 4.0, true, false),
         tick(4.5, 4.5, false, true), tick(5.0, 5.0, true, false)});

    // The positions are pos * minstep in double arithmetic, so 3 * 0.1 is 0.30000000000000004.
    expectTicks(
        none.getTicks(0, 1, 0.1, 0.5),
        {tick(0.0, 0.0, true, true), tick(0.1, 0.1, false, false), tick(0.2, 0.2, false, false),
         tick(0.30000000000000004, 0.30000000000000004, false, false), tick(0.4, 0.4, false, false),
         tick(0.5, 0.5, true, false), tick(0.6000000000000001, 0.6000000000000001, false, false),
         tick(0.7000000000000001, 0.7000000000000001, false, false), tick(0.8, 0.8, false, false),
         tick(0.9, 0.9, false, false), tick(1.0, 1.0, true, true)});

    expectTicks(
        none.getTicks(-1, 1, 0.25, 0.5),
        {tick(-1.0, -1.0, true, true), tick(-0.5, -0.5, true, false), tick(0.0, 0.0, true, true),
         tick(0.5, 0.5, true, false), tick(1.0, 1.0, true, true)});

    expectTicks(
        none.getTicks(0, 10, 3, 7),
        {tick(0.0, 0.0, true, true), tick(5.0, 5.0, false, true), tick(10.0, 10.0, true, false)});

    expectTicks(
        none.getTicks(0.35, 2.7, 0.2, 1.0),
        {tick(0.5, 0.5, false, true), tick(1.0, 1.0, true, false), tick(1.5, 1.5, false, true),
         tick(2.0, 2.0, true, false), tick(2.5, 2.5, false, true)});

    EXPECT_TRUE(none.getTicks(5, 1, 0.5, 1).empty());  // start beyond end

    // The distances are converted to the unit; the SI values come back through fromUnit.
    const GeneralUnit cm(0.01, "cm");
    expectTicks(
        cm.getTicks(0, 0.1, 0.01, 0.05),
        {tick(0.0, 0.0, true, true), tick(0.01, 1.0, false, false), tick(0.02, 2.0, false, false),
         tick(0.03, 3.0, false, false), tick(0.04, 4.0, false, false), tick(0.05, 5.0, true, false),
         tick(0.06, 6.0, false, false), tick(0.07, 7.0, false, false),
         tick(0.08, 8.0, false, false), tick(0.09, 9.0, false, false),
         tick(0.1, 10.0, true, true)});
    expectTicks(cm.getTicks(0.003, 0.021, 0.002, 0.01),
                {tick(0.005, 0.5, false, true), tick(0.01, 1.0, true, false),
                 tick(0.015, 1.5, false, true), tick(0.02, 2.0, true, false)});
}

TEST(GeneralUnit, TicksRejectBadDistances)
{
    const Unit& none = Unit::noUnit();
    EXPECT_THROW(static_cast<void>(none.getTicks(0, 1, 0, 1)), BugError);
    EXPECT_THROW(static_cast<void>(none.getTicks(0, 1, 0.5, 0.25)), BugError);
    EXPECT_THROW(static_cast<void>(none.getTicks(0, 1, -1, 1)), BugError);
    EXPECT_THROW(static_cast<void>(none.getTicks(0, 1, 1, -1)), BugError);
    try
    {
        static_cast<void>(none.getTicks(0, 1, 0, 1));
        FAIL();
    }
    catch (const BugError& e)
    {
        EXPECT_TRUE(std::string_view(e.what()).starts_with(
            "BUG: getTicks called with minor=0.0 major=1.0 ("));
    }
}

/// The ticks as one character each: 'N' major and notable, 'M' major, 'n' notable, '.' minor.
std::string tickPattern(const std::vector<Tick>& ticks)
{
    std::string pattern;
    for (const Tick& t : ticks)
    {
        if (t.major)
        {
            pattern += t.notable ? 'N' : 'M';
        }
        else
        {
            pattern += t.notable ? 'n' : '.';
        }
    }
    return pattern;
}

// Java narrows the step ratios to int and multiplies and takes remainders with int arithmetic,
// which wraps. Every result below was produced by OpenRocket's GeneralUnit on JDK 17.

TEST(GeneralUnit, TicksWrapTheMajorNotableModulusAsJavaDoes)
{
    const Unit& none = Unit::noUnit();

    // minstep 1e-10 and a round-ten major step of 1: mod3 = 10^10 narrows to 1410065408, and
    // mod3 * 10 wraps to 1215752192 (a C++ int overflow would be undefined).
    const std::vector<Tick> tens = none.getTicks(0, 1e-9, 1e-10, 1);
    EXPECT_EQ(tickPattern(tens), "N.........");
    EXPECT_EQ(tens[1].value, 1.0000000000000003E-10);
    EXPECT_EQ(tens[3].value, 3.000000000000001E-10);
    EXPECT_EQ(tens[9].value, 9.000000000000003E-10);
    EXPECT_EQ(tens[9].unitValue, 9.000000000000003E-10);

    // A round-five major step of 0.5 over minstep 5e-11: mod3 * 2 wraps.
    const std::vector<Tick> fives = none.getTicks(0, 1e-9, 5e-11, 0.5);
    EXPECT_EQ(tickPattern(fives), "N.n.n.n.n.n.n.n.n.n.");
    EXPECT_EQ(fives[1].value, 5.0000000000000015E-11);
    EXPECT_EQ(fives[19].value, 9.500000000000002E-10);

    // FixedPrecisionUnit copies GeneralUnit's ticks.
    EXPECT_EQ(tickPattern(QtRocket::FixedPrecisionUnit("u", 0.5).getTicks(0, 1e-9, 1e-10, 1)),
              "N.........");
}

TEST(GeneralUnit, TicksTakeJavasRemainderOfIntMinByMinusOne)
{
    // Math.round saturates at Long.MAX_VALUE, which narrows to -1, and the start position
    // saturates at Integer.MIN_VALUE: Java's INT_MIN % -1 is 0 (in C++ it overflows).
    const std::vector<Tick> saturated = Unit::noUnit().getTicks(-1e10, -2147483647, 1, 1e20);
    EXPECT_EQ(tickPattern(saturated), "MM");
    EXPECT_EQ(saturated[0].value, -2147483648.0);
    EXPECT_EQ(saturated[1].value, -2147483647.0);

    const GeneralUnit       m(1, "m");
    const std::vector<Tick> tiny = m.getTicks(-1, -2.1474836e-11, 1e-20, 1);
    ASSERT_EQ(tiny.size(), 49U);
    EXPECT_EQ(tickPattern(tiny), "MMMMMMMMNMMMMMMMMMNMMMMMMMMMNMMMMMMMMMNMMMMMMMMMN");
    EXPECT_EQ(tiny[0].value, -2.1474836480000002E-11);
    EXPECT_EQ(tiny[6].value, -2.147483642E-11);
}

TEST(GeneralUnit, TicksWithAZeroModulusThrowAsJavasDivisionByZero)
{
    // A NaN major passes the argument check but makes mod3 Math.round(0.2) = 0: Java throws
    // ArithmeticException ("/ by zero") at the first tick, where a C++ % would trap.
    const QtRocket::InchUnit inch(0.0254, "in", 1);
    try
    {
        static_cast<void>(inch.getTicks(0, 1, 0.1, kNaN));
        FAIL() << "no BugError";
    }
    catch (const BugError& e)
    {
        EXPECT_TRUE(std::string_view(e.what()).starts_with("BUG: / by zero ("));
    }
    // With no tick to place nothing is divided, so nothing is thrown.
    EXPECT_TRUE(inch.getTicks(1, 0, 0.1, kNaN).empty());
}

}  // namespace
