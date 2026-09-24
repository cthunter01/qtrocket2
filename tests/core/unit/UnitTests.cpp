#include "QtRocket/unit/Unit.h"

#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/unit/CaliberUnit.h"
#include "QtRocket/unit/DegreeUnit.h"
#include "QtRocket/unit/FixedPrecisionUnit.h"
#include "QtRocket/unit/FrequencyUnit.h"
#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/InchUnit.h"
#include "QtRocket/unit/PercentageOfLengthUnit.h"
#include "QtRocket/unit/RadianUnit.h"
#include "QtRocket/unit/TemperatureUnit.h"
#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/unit/Value.h"
#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::BugError;
using QtRocket::CaliberUnit;
using QtRocket::DegreeUnit;
using QtRocket::FixedPrecisionUnit;
using QtRocket::FrequencyUnit;
using QtRocket::GeneralUnit;
using QtRocket::InchUnit;
using QtRocket::PercentageOfLengthUnit;
using QtRocket::RadianUnit;
using QtRocket::TemperatureUnit;
using QtRocket::Tick;
using QtRocket::Unit;
using QtRocket::unitGroup;
using QtRocket::UnitGroupId;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// Every expected string below was produced by OpenRocket's own Unit classes running on JDK 17
// with Locale.US (the values of UnitToStringTest.java plus a wider sweep).

// ---- Ported from UnitToStringTest.java ----

TEST(UnitToString, PositiveToString)
{
    // very small positive numbers ( < 0.0005) are returned as "0"
    EXPECT_EQ("0", Unit::noUnit().toString(0.00040));
    EXPECT_EQ("0", Unit::noUnit().toString(0.00050));  // check boundary of change in format

    // positive numbers <= 1E6
    EXPECT_EQ("123", Unit::noUnit().toString(123.20));
    EXPECT_EQ("124", Unit::noUnit().toString(123.50));  // round to even
    EXPECT_EQ("124", Unit::noUnit().toString(123.55));
    EXPECT_EQ("124", Unit::noUnit().toString(124.20));
    EXPECT_EQ("124", Unit::noUnit().toString(124.50));  // round to even
    EXPECT_EQ("125", Unit::noUnit().toString(124.55));

    EXPECT_EQ("1234", Unit::noUnit().toString(1234.2));
    EXPECT_EQ("1234", Unit::noUnit().toString(1234.5));  // round to even
    EXPECT_EQ("1235", Unit::noUnit().toString(1234.6));
    EXPECT_EQ("1235", Unit::noUnit().toString(1235.2));
    EXPECT_EQ("1236", Unit::noUnit().toString(1235.5));  // round to even
    EXPECT_EQ("1236", Unit::noUnit().toString(1235.6));

    EXPECT_EQ("123457", Unit::noUnit().toString(123456.789));

    EXPECT_EQ("1000000", Unit::noUnit().toString(1000000));  // boundary check

    // testPositiveWithPoint: the output never depends on the process locale here.
    // positive number < 0.095 use 3 digit decimal format
    EXPECT_EQ("0.001", Unit::noUnit().toString(0.00051));  // check boundary of change in format
    EXPECT_EQ("0.001", Unit::noUnit().toString(0.00060));

    // rounding at third digit.
    EXPECT_EQ("0.001", Unit::noUnit().toString(0.0014));
    EXPECT_EQ("0.002", Unit::noUnit().toString(0.0015));  // round to even
    EXPECT_EQ("0.002", Unit::noUnit().toString(0.0016));
    EXPECT_EQ("0.002", Unit::noUnit().toString(0.0024));
    EXPECT_EQ("0.002", Unit::noUnit().toString(0.0025));  // round to even
    EXPECT_EQ("0.003", Unit::noUnit().toString(0.0026));
    EXPECT_EQ("0.009", Unit::noUnit().toString(0.0094));

    EXPECT_EQ("0.01", Unit::noUnit().toString(0.0095));  // no trailing zeros after rounding

    EXPECT_EQ("0.011", Unit::noUnit().toString(0.0114));
    EXPECT_EQ("0.012", Unit::noUnit().toString(0.0115));  // round to even
    EXPECT_EQ("0.012", Unit::noUnit().toString(0.0119));
    EXPECT_EQ("0.012", Unit::noUnit().toString(0.0124));
    EXPECT_EQ("0.012", Unit::noUnit().toString(0.0125));  // round to even
    EXPECT_EQ("0.013", Unit::noUnit().toString(0.0129));

    EXPECT_EQ("0.095", Unit::noUnit().toString(0.0949));  // boundary check

    // positive numbers < 100
    EXPECT_EQ("0.01", Unit::noUnit().toString(0.0095));  // boundary check

    EXPECT_EQ("0.111", Unit::noUnit().toString(0.1111));
    EXPECT_EQ("0.112", Unit::noUnit().toString(0.1115));  // round to even
    EXPECT_EQ("0.112", Unit::noUnit().toString(0.1117));
    EXPECT_EQ("0.112", Unit::noUnit().toString(0.1121));
    EXPECT_EQ("0.112", Unit::noUnit().toString(0.1125));  // round to even
    EXPECT_EQ("0.113", Unit::noUnit().toString(0.1127));

    EXPECT_EQ("1.11", Unit::noUnit().toString(1.113));
    EXPECT_EQ("1.12", Unit::noUnit().toString(1.115));  // round to even
    EXPECT_EQ("1.12", Unit::noUnit().toString(1.117));
    EXPECT_EQ("1.12", Unit::noUnit().toString(1.123));
    EXPECT_EQ("1.12", Unit::noUnit().toString(1.125));  // round to even
    EXPECT_EQ("1.13", Unit::noUnit().toString(1.127));

    EXPECT_EQ("12.3", Unit::noUnit().toString(12.320));
    EXPECT_EQ("12.4", Unit::noUnit().toString(12.350));  // round to even
    EXPECT_EQ("12.4", Unit::noUnit().toString(12.355));
    EXPECT_EQ("12.4", Unit::noUnit().toString(12.420));
    EXPECT_EQ("12.4", Unit::noUnit().toString(12.450));  // round to even
    EXPECT_EQ("12.5", Unit::noUnit().toString(12.455));
    // positive numbers > 1E6
    EXPECT_EQ("1.23E6", Unit::noUnit().toString(1234567.89));
    EXPECT_EQ("1.23E7", Unit::noUnit().toString(12345678.9));

    // Inch precision
    const Unit* inch = unitGroup(UnitGroupId::LENGTH).findApproximate("in");
    ASSERT_NE(inch, nullptr);
    EXPECT_EQ("25.125", inch->toString(25.125 * 25.4 / 1000));
}

TEST(UnitToString, NegativeToString)
{
    // very small negative numbers ( < 0.0005) are returned as "0");
    EXPECT_EQ("0", Unit::noUnit().toString(-0.00050));  // check boundary of change in format

    // negative numbers <= 1E6
    EXPECT_EQ("-123", Unit::noUnit().toString(-123.20));
    EXPECT_EQ("-124", Unit::noUnit().toString(-123.50));  // round to even
    EXPECT_EQ("-124", Unit::noUnit().toString(-123.55));
    EXPECT_EQ("-124", Unit::noUnit().toString(-124.20));
    EXPECT_EQ("-124", Unit::noUnit().toString(-124.50));  // round to even
    EXPECT_EQ("-125", Unit::noUnit().toString(-124.55));

    EXPECT_EQ("-1234", Unit::noUnit().toString(-1234.2));
    EXPECT_EQ("-1234", Unit::noUnit().toString(-1234.5));  // round to even
    EXPECT_EQ("-1235", Unit::noUnit().toString(-1234.6));
    EXPECT_EQ("-1235", Unit::noUnit().toString(-1235.2));
    EXPECT_EQ("-1236", Unit::noUnit().toString(-1235.5));  // round to even
    EXPECT_EQ("-1236", Unit::noUnit().toString(-1235.6));

    EXPECT_EQ("-123457", Unit::noUnit().toString(-123456.789));

    EXPECT_EQ("-1000000", Unit::noUnit().toString(-1000000));  // boundary check

    // testNegativeWithPoint
    EXPECT_EQ("-0.001", Unit::noUnit().toString(-0.00051));  // check boundary of change in format
    EXPECT_EQ("-0.001", Unit::noUnit().toString(-0.00060));

    // rounding at third digit.
    EXPECT_EQ("-0.001", Unit::noUnit().toString(-0.0014));
    EXPECT_EQ("-0.002", Unit::noUnit().toString(-0.0015));  // round to even
    EXPECT_EQ("-0.002", Unit::noUnit().toString(-0.0016));
    EXPECT_EQ("-0.002", Unit::noUnit().toString(-0.0024));
    EXPECT_EQ("-0.002", Unit::noUnit().toString(-0.0025));  // round to even
    EXPECT_EQ("-0.003", Unit::noUnit().toString(-0.0026));
    EXPECT_EQ("-0.009", Unit::noUnit().toString(-0.0094));

    EXPECT_EQ("-0.01", Unit::noUnit().toString(-0.0095));  // no trailing zeros after rounding

    EXPECT_EQ("-0.011", Unit::noUnit().toString(-0.0114));
    EXPECT_EQ("-0.012", Unit::noUnit().toString(-0.0115));  // round to even
    EXPECT_EQ("-0.012", Unit::noUnit().toString(-0.0119));
    EXPECT_EQ("-0.012", Unit::noUnit().toString(-0.0124));
    EXPECT_EQ("-0.012", Unit::noUnit().toString(-0.0125));  // round to even
    EXPECT_EQ("-0.013", Unit::noUnit().toString(-0.0129));

    EXPECT_EQ("-0.095", Unit::noUnit().toString(-0.0949));  // boundary check

    // negative numbers < 100
    EXPECT_EQ("-0.01", Unit::noUnit().toString(-0.0095));  // boundary check

    EXPECT_EQ("-0.111", Unit::noUnit().toString(-0.1111));
    EXPECT_EQ("-0.112", Unit::noUnit().toString(-0.1115));  // round to even
    EXPECT_EQ("-0.112", Unit::noUnit().toString(-0.1117));
    EXPECT_EQ("-0.112", Unit::noUnit().toString(-0.1121));
    EXPECT_EQ("-0.112", Unit::noUnit().toString(-0.1125));  // round to even
    EXPECT_EQ("-0.113", Unit::noUnit().toString(-0.1127));

    EXPECT_EQ("-1.11", Unit::noUnit().toString(-1.113));
    EXPECT_EQ("-1.12", Unit::noUnit().toString(-1.115));  // round to even
    EXPECT_EQ("-1.12", Unit::noUnit().toString(-1.117));
    EXPECT_EQ("-1.12", Unit::noUnit().toString(-1.123));
    EXPECT_EQ("-1.12", Unit::noUnit().toString(-1.125));  // round to even
    EXPECT_EQ("-1.13", Unit::noUnit().toString(-1.127));

    EXPECT_EQ("-12.3", Unit::noUnit().toString(-12.320));
    EXPECT_EQ("-12.4", Unit::noUnit().toString(-12.350));  // round to even
    EXPECT_EQ("-12.4", Unit::noUnit().toString(-12.355));
    EXPECT_EQ("-12.4", Unit::noUnit().toString(-12.420));
    EXPECT_EQ("-12.4", Unit::noUnit().toString(-12.450));  // round to even
    EXPECT_EQ("-12.5", Unit::noUnit().toString(-12.455));
    // negative numbers > 1E6
    EXPECT_EQ("-1.23E6", Unit::noUnit().toString(-1234567.89));
    EXPECT_EQ("-1.23E7", Unit::noUnit().toString(-12345678.9));
}

// testLocaleChangeAfterUnitInitialization has no counterpart: the formatting never consults a
// locale here, which the "-0.001" case above already pins.

// ---- QtRocket additions ----

TEST(UnitToString, WiderSweepMatchesOpenRocket)
{
    EXPECT_EQ(Unit::noUnit().toString(0.0), "0");
    EXPECT_EQ(Unit::noUnit().toString(-0.0), "0");
    EXPECT_EQ(Unit::noUnit().toString(1.0E-320), "0");
    EXPECT_EQ(Unit::noUnit().toString(5.0000001E-4), "0.001");
    EXPECT_EQ(Unit::noUnit().toString(-5.0000001E-4), "-0.001");
    EXPECT_EQ(Unit::noUnit().toString(9.9E-4), "0.001");
    EXPECT_EQ(Unit::noUnit().toString(0.001), "0.001");
    EXPECT_EQ(Unit::noUnit().toString(0.0995), "0.1");
    EXPECT_EQ(Unit::noUnit().toString(-0.0995), "-0.1");
    EXPECT_EQ(Unit::noUnit().toString(0.9995), "1");
    EXPECT_EQ(Unit::noUnit().toString(-0.9995), "-1");
    EXPECT_EQ(Unit::noUnit().toString(0.99995), "1");
    EXPECT_EQ(Unit::noUnit().toString(1.0005), "1");
    EXPECT_EQ(Unit::noUnit().toString(1.00049), "1");
    EXPECT_EQ(Unit::noUnit().toString(2.0005), "2");
    EXPECT_EQ(Unit::noUnit().toString(9.9995), "10");
    EXPECT_EQ(Unit::noUnit().toString(-9.9995), "-10");
    EXPECT_EQ(Unit::noUnit().toString(9.99949), "10");
    EXPECT_EQ(Unit::noUnit().toString(99.9995), "100");
    EXPECT_EQ(Unit::noUnit().toString(99.995), "100");
    EXPECT_EQ(Unit::noUnit().toString(99.95), "100");
    EXPECT_EQ(Unit::noUnit().toString(99.9999), "100");
    EXPECT_EQ(Unit::noUnit().toString(-99.99999), "-100");
    EXPECT_EQ(Unit::noUnit().toString(100.0), "100");
    EXPECT_EQ(Unit::noUnit().toString(100.4), "100");
    EXPECT_EQ(Unit::noUnit().toString(100.5), "100");
    EXPECT_EQ(Unit::noUnit().toString(-100.5), "-100");
    EXPECT_EQ(Unit::noUnit().toString(101.5), "102");
    EXPECT_EQ(Unit::noUnit().toString(-101.5), "-102");
    EXPECT_EQ(Unit::noUnit().toString(999999.5), "1000000");
    EXPECT_EQ(Unit::noUnit().toString(-999999.5), "-1000000");
    EXPECT_EQ(Unit::noUnit().toString(1000000.5), "1.00E6");
    EXPECT_EQ(Unit::noUnit().toString(-1000000.5), "-1.00E6");
    EXPECT_EQ(Unit::noUnit().toString(1000001.0), "1.00E6");
    EXPECT_EQ(Unit::noUnit().toString(9995000.0), "1.00E7");
    EXPECT_EQ(Unit::noUnit().toString(-9995000.0), "-1.00E7");
    EXPECT_EQ(Unit::noUnit().toString(1235000.0), "1.24E6");  // DecimalFormat rounds this up
    EXPECT_EQ(Unit::noUnit().toString(1245000.0), "1.25E6");  // ... and this integer tie too
    EXPECT_EQ(Unit::noUnit().toString(1.0E300), "1.00E300");
    EXPECT_EQ(Unit::noUnit().toString(-1.0E300), "-1.00E300");
    EXPECT_EQ(Unit::noUnit().toString(kInf), "∞");
    EXPECT_EQ(Unit::noUnit().toString(-kInf), "-∞");
    EXPECT_EQ(Unit::noUnit().toString(kNaN), "N/A");
    EXPECT_EQ(Unit::noUnit().toString(12.5), "12.5");
    EXPECT_EQ(Unit::noUnit().toString(0.15), "0.15");
    EXPECT_EQ(Unit::noUnit().toString(0.25), "0.25");
    EXPECT_EQ(Unit::noUnit().toString(0.35), "0.35");
    EXPECT_EQ(Unit::noUnit().toString(0.045), "0.045");
    EXPECT_EQ(Unit::noUnit().toString(0.055), "0.055");
    EXPECT_EQ(Unit::noUnit().toString(0.0045), "0.004");
    EXPECT_EQ(Unit::noUnit().toString(-0.0045), "-0.004");
    EXPECT_EQ(Unit::noUnit().toString(0.0055), "0.005");
    EXPECT_EQ(Unit::noUnit().toString(0.1), "0.1");
    EXPECT_EQ(Unit::noUnit().toString(1.0), "1");
    EXPECT_EQ(Unit::noUnit().toString(-1.0), "-1");
    EXPECT_EQ(Unit::noUnit().toString(10.0), "10");
    EXPECT_EQ(Unit::noUnit().toString(50.0), "50");
    EXPECT_EQ(Unit::noUnit().toString(99.0), "99");
    EXPECT_EQ(Unit::noUnit().toString(0.5), "0.5");
    EXPECT_EQ(Unit::noUnit().toString(2.5), "2.5");
    EXPECT_EQ(Unit::noUnit().toString(0.05), "0.05");
    EXPECT_EQ(Unit::noUnit().toString(1.05), "1.05");
    EXPECT_EQ(Unit::noUnit().toString(10.05), "10");
    EXPECT_EQ(Unit::noUnit().toString(-10.05), "-10");
    EXPECT_EQ(Unit::noUnit().toString(0.0105), "0.01");
    EXPECT_EQ(Unit::noUnit().toString(0.10005), "0.1");
    EXPECT_EQ(Unit::noUnit().toString(5.0001E-4), "0.001");
}

TEST(UnitToString, LargeMagnitudesTiesAndZerosMatchOpenRocket)
{
    // DecimalFormat's digits come from FloatingDecimal, which is not the shortest conversion, and
    // its ties are decided by how that conversion rounded; the cases of the differential sweep
    // against OpenRocket that exercise both.
    const Unit&       none = Unit::noUnit();
    const GeneralUnit mm(0.001, "mm");
    EXPECT_EQ(none.toString(0x1p69), "5.90E20");
    EXPECT_EQ(none.toString(1.245e19), "1.24E19");
    EXPECT_EQ(none.toString(1.235e19), "1.24E19");
    EXPECT_EQ(none.toString(9.5e18), "9.50E18");
    EXPECT_EQ(none.toString(1e23), "1.00E23");
    EXPECT_EQ(none.toString(-2e23), "-2.00E23");
    EXPECT_EQ(none.toString(1.7976931348623157e308), "1.80E308");
    EXPECT_EQ(none.toString(-4.9e-324), "0");
    EXPECT_EQ(none.toString(-0.00015), "0");
    EXPECT_EQ(none.toString(1.0005e-3), "0.001");
    EXPECT_EQ(none.toString(2.675), "2.68");
    EXPECT_EQ(mm.toString(2.5e-5), "0.025");
    EXPECT_EQ(mm.toString(-3.5e-5), "-0.035");
    EXPECT_EQ(mm.toString(4.5e-4), "0.45");
    EXPECT_EQ(mm.toString(0.1235), "124");
}

TEST(Unit, NoUnitProperties)
{
    const Unit& none = Unit::noUnit();
    EXPECT_EQ(none.getUnit(), "​");  // zero-width space
    EXPECT_EQ(none.getMultiplier(), 1.0);
    EXPECT_TRUE(none.hasSpace());
    EXPECT_EQ(none.toUnit(2.5), 2.5);
    EXPECT_EQ(none.fromUnit(2.5), 2.5);
    EXPECT_EQ(none.toStringUnit(1.5), "1.5 ​");
    EXPECT_EQ(none.toStringUnit(kNaN), "N/A");
    EXPECT_EQ(&Unit::noUnit(), &none);  // one instance
    EXPECT_NE(dynamic_cast<const GeneralUnit*>(&none), nullptr);
    EXPECT_EQ(dynamic_cast<const GeneralUnit&>(none).getSignificantNumbers(), 2);
}

TEST(Unit, ToStringUnitAndConversions)
{
    const GeneralUnit mm(0.001, "mm");
    EXPECT_EQ(mm.toStringUnit(0.12345), "123 mm");
    EXPECT_EQ(mm.toString(0.12345), "123");
    EXPECT_EQ(mm.toString(2.5), "2500");
    EXPECT_DOUBLE_EQ(mm.toUnit(0.5), 500.0);
    EXPECT_DOUBLE_EQ(mm.fromUnit(500.0), 0.5);
    EXPECT_EQ(mm.getUnit(), "mm");
    EXPECT_EQ(mm.getMultiplier(), 0.001);

    const QtRocket::Value value = mm.toValue(0.25);
    EXPECT_EQ(value.getValue(), 0.25);
    EXPECT_EQ(&value.getUnit(), &mm);
}

TEST(Unit, MultiplierZeroIsRejected)
{
    EXPECT_THROW(GeneralUnit(0, "x"), BugError);
    EXPECT_THROW(FixedPrecisionUnit("x", 0.1, 0.0), BugError);
    EXPECT_THROW(FrequencyUnit(0, "x"), BugError);
    EXPECT_NO_THROW(GeneralUnit(-1, "x"));  // a negative multiplier is fine (° S is -1)
}

TEST(Unit, EqualsAndHashFollowUnitJava)
{
    const GeneralUnit m1(1, "m");
    const GeneralUnit m2(1, "m");
    const GeneralUnit m3(1, "m", 3);
    const GeneralUnit mm(1, "mm");
    const GeneralUnit twoM(2, "m");
    const InchUnit    inchM(1, "m");

    EXPECT_TRUE(m1.equals(m2));
    EXPECT_TRUE(m1 == m2);
    EXPECT_TRUE(m1.equals(m3));  // the rounding does not count
    EXPECT_FALSE(m1.equals(mm));
    EXPECT_FALSE(m1.equals(twoM));
    EXPECT_FALSE(m1.equals(inchM));  // another class
    EXPECT_FALSE(inchM.equals(m1));
    EXPECT_EQ(m1.hash(), m2.hash());
    EXPECT_EQ(m1.hash(), m3.hash());
    EXPECT_NE(m1.hash(), inchM.hash());
    EXPECT_NE(m1.hash(), mm.hash());
}

TEST(Unit, CloneKeepsTheDynamicType)
{
    const InchUnit              inch(0.0254, "in", 0.5);
    const std::unique_ptr<Unit> copy = inch.clone();
    ASSERT_NE(dynamic_cast<const InchUnit*>(copy.get()), nullptr);
    EXPECT_TRUE(copy->equals(inch));
    EXPECT_EQ(copy->getNextValue(1.0), 1.5);

    const TemperatureUnit       celsius(1, 273.15, 0.01, "°C");
    const std::unique_ptr<Unit> celsiusCopy = celsius.clone();
    ASSERT_NE(dynamic_cast<const TemperatureUnit*>(celsiusCopy.get()), nullptr);
    EXPECT_EQ(celsiusCopy->toStringUnit(283.15), "10.00°C");
}

TEST(Tick, ToStringWritesJavaDoubles)
{
    EXPECT_EQ((Tick{.value = 1.5, .unitValue = 150, .major = true, .notable = false}).toString(),
              "Tick[value=1.5,major]");
    EXPECT_EQ((Tick{.value = 0.1, .unitValue = 1e-4, .major = false, .notable = true}).toString(),
              "Tick[value=0.1,minor,notable]");
    EXPECT_EQ((Tick{.value = -2.0, .unitValue = 1e7, .major = true, .notable = true}).toString(),
              "Tick[value=-2.0,major,notable]");
    EXPECT_EQ((Tick{.value = kNaN, .unitValue = 0, .major = false, .notable = false}).toString(),
              "Tick[value=NaN,minor]");
}

// ---- GeneralUnit ----

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

// ---- getTicks (GeneralUnit and FractionalUnit share the test helper) ----

void expectTick(const Tick& actual, const Tick& expected, std::size_t index)
{
    EXPECT_DOUBLE_EQ(actual.value, expected.value) << "tick " << index;
    EXPECT_DOUBLE_EQ(actual.unitValue, expected.unitValue) << "tick " << index;
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
        EXPECT_DOUBLE_EQ(hundred[i].value, static_cast<double>(i));
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

// ---- InchUnit ----

TEST(InchUnit, KeepsThreeDecimals)
{
    const InchUnit inch(0.0254, "in", 1);
    EXPECT_EQ(inch.getPrecision(), 1.0);
    EXPECT_EQ(inch.toString(25.125 * 25.4 / 1000), "25.125");
    EXPECT_EQ(inch.toStringUnit(25.125 * 25.4 / 1000), "25.125 in");
    EXPECT_EQ(inch.toString(0.0254 * 0.0005), "0");
    EXPECT_EQ(inch.toString(0.0254 * 1.0005), "1");
    EXPECT_EQ(inch.toString(0.0254 * 2.00049), "2");
    EXPECT_EQ(inch.toString(0.0254 * 99.9995), "100");
    EXPECT_EQ(inch.toString(0.0254 * 100), "100");
    EXPECT_EQ(inch.toString(0.0254 * 0.0004), "0");
    EXPECT_EQ(inch.toString(1e-9), "0");
    EXPECT_EQ(inch.toString(0.0254 * 1.0004), "1");
    EXPECT_EQ(inch.toString(0.0254 * 1.00051), "1.001");
    EXPECT_EQ(inch.toString(0.0254 * 0.00051), "0.001");
    EXPECT_EQ(inch.toString(0.0254 * 0.0006), "0.001");
    EXPECT_EQ(inch.toString(0.0254 * 12.3456), "12.346");
    EXPECT_EQ(inch.toString(0.0254 * 12.3455), "12.346");
    EXPECT_EQ(inch.toString(0.0254 * 12.3445), "12.344");
    EXPECT_EQ(inch.toString(0.0254 * 1e7), "1.00E7");
    EXPECT_EQ(inch.toString(kNaN), "N/A");
    EXPECT_EQ(inch.toString(0.0254 * 1e-3), "0.001");
    EXPECT_EQ(inch.toString(0.0254 * 0.9995), "1");
    EXPECT_EQ(inch.toString(0.0254 * 0.99949), "0.999");
    EXPECT_EQ(inch.toStringUnit(0.0254 * 0.99949), "0.999 in");
}

TEST(InchUnit, StepsByPrecisionAndRoundsAsGeneralUnit)
{
    const InchUnit inch(0.0254, "in", 1);
    const InchUnit inchDefault(0.0254, "in");
    EXPECT_DOUBLE_EQ(inch.getNextValue(2.5), 3.5);
    EXPECT_DOUBLE_EQ(inch.getPreviousValue(2.5), 1.5);
    EXPECT_DOUBLE_EQ(inchDefault.getNextValue(2.5), 3.5);
    EXPECT_DOUBLE_EQ(InchUnit(0.0254, "in", 0.125).getNextValue(2.5), 2.625);
    EXPECT_DOUBLE_EQ(inch.round(2.55), 2.6);
    EXPECT_DOUBLE_EQ(inch.round(12.5), 12.0);
    EXPECT_DOUBLE_EQ(inch.round(125.0), 120.0);
}

TEST(InchUnit, TiesMatchOpenRocket)
{
    const InchUnit inch(0.0254, "in", 1);
    EXPECT_EQ(inch.toString(0.0254 * 2.0005), "2.001");
    EXPECT_EQ(inch.toString(0.0254 * 0.0015), "0.002");
    EXPECT_EQ(inch.toString(-0.0254 * 0.0005), "0");
    EXPECT_EQ(inch.round(2.25), 2.2);
    EXPECT_EQ(inch.round(2.35), 2.4);
    EXPECT_EQ(inch.getPreviousValue(-0.5), -1.5);
}

// ---- CaliberUnit and PercentageOfLengthUnit ----

TEST(CaliberUnit, ConstantReference)
{
    const CaliberUnit cal(0.05);
    EXPECT_EQ(cal.getUnit(), "cal");
    EXPECT_EQ(cal.getMultiplier(), 1.0);
    EXPECT_TRUE(cal.hasReference());
    EXPECT_DOUBLE_EQ(cal.getReferenceLength(), 0.05);
    EXPECT_DOUBLE_EQ(cal.toUnit(0.1), 2.0);
    EXPECT_DOUBLE_EQ(cal.fromUnit(2.0), 0.1);
    EXPECT_EQ(cal.toString(0.1), "2");
    EXPECT_EQ(cal.toStringUnit(0.125), "2.5 cal");
    EXPECT_EQ(CaliberUnit::kDefaultCaliber, 0.01);
}

TEST(CaliberUnit, ProviderIsReadOnEveryConversion)
{
    double            reference = 0.1;
    const CaliberUnit cal([&reference] { return reference; });
    EXPECT_TRUE(cal.hasReference());
    EXPECT_DOUBLE_EQ(cal.toUnit(0.2), 2.0);
    reference = 0.4;
    EXPECT_DOUBLE_EQ(cal.toUnit(0.2), 0.5);
    EXPECT_DOUBLE_EQ(cal.fromUnit(0.5), 0.2);

    // A clone keeps reading the same provider.
    const std::unique_ptr<Unit> copy = cal.clone();
    reference                        = 0.8;
    EXPECT_DOUBLE_EQ(copy->toUnit(0.2), 0.25);
}

TEST(CaliberUnit, WithoutReferenceConvertingIsABug)
{
    const CaliberUnit placeholder;
    EXPECT_FALSE(placeholder.hasReference());
    EXPECT_THROW(static_cast<void>(placeholder.toUnit(1.0)), BugError);
    EXPECT_THROW(static_cast<void>(placeholder.fromUnit(1.0)), BugError);
    EXPECT_THROW(static_cast<void>(placeholder.toString(1.0)), BugError);
    EXPECT_FALSE(CaliberUnit(std::function<double()>{}).hasReference());
    EXPECT_THROW(CaliberUnit(0.0), BugError);
    EXPECT_THROW(CaliberUnit(-1.0), BugError);
    // Two placeholders are equal, as are a placeholder and a bound unit: only the class, the
    // multiplier and the name count.
    EXPECT_TRUE(placeholder.equals(CaliberUnit(0.05)));
}

TEST(PercentageOfLengthUnit, ScalesByReferenceAndPercent)
{
    const PercentageOfLengthUnit percent(2.0);
    EXPECT_EQ(percent.getUnit(), "%");
    EXPECT_EQ(percent.getMultiplier(), 0.01);
    EXPECT_DOUBLE_EQ(percent.getReferenceLength(), 2.0);
    EXPECT_DOUBLE_EQ(percent.toUnit(0.5), 25.0);
    EXPECT_DOUBLE_EQ(percent.fromUnit(25.0), 0.5);
    EXPECT_EQ(percent.toStringUnit(0.5), "25 %");

    const PercentageOfLengthUnit placeholder;
    EXPECT_FALSE(placeholder.hasReference());
    EXPECT_THROW(static_cast<void>(placeholder.toUnit(1.0)), BugError);
    EXPECT_THROW(PercentageOfLengthUnit(0.0), BugError);
    EXPECT_THROW(PercentageOfLengthUnit(-0.5), BugError);
}

TEST(PercentageOfLengthUnit, ProviderIsReadOnEveryConversion)
{
    double                       reference = 1.0;
    const PercentageOfLengthUnit dynamic([&reference] { return reference; });
    EXPECT_DOUBLE_EQ(dynamic.toUnit(0.5), 50.0);
    reference = 4.0;
    EXPECT_DOUBLE_EQ(dynamic.toUnit(0.5), 12.5);
}

// ---- TemperatureUnit ----

TEST(TemperatureUnit, CelsiusAndFahrenheit)
{
    const TemperatureUnit celsius(1, 273.15, 0.01, "°C");
    const TemperatureUnit fahrenheit(5.0 / 9.0, 459.67, 0.01, "°F");
    EXPECT_FALSE(celsius.hasSpace());
    EXPECT_EQ(celsius.getUnit(), "°C");
    EXPECT_EQ(celsius.getAddition(), 273.15);
    EXPECT_EQ(fahrenheit.getPrecision(), 0.01);

    EXPECT_DOUBLE_EQ(celsius.toUnit(273.15), 0.0);
    EXPECT_DOUBLE_EQ(celsius.fromUnit(273.15), 546.3);
    EXPECT_EQ(celsius.toString(273.15), "0.00");
    EXPECT_EQ(celsius.toStringUnit(273.15), "0.00°C");
    EXPECT_DOUBLE_EQ(fahrenheit.toUnit(273.15), 31.999999999999943);
    EXPECT_DOUBLE_EQ(fahrenheit.fromUnit(273.15), 407.1222222222222);
    EXPECT_EQ(fahrenheit.toString(273.15), "32.00");
    EXPECT_EQ(fahrenheit.toStringUnit(273.15), "32.00°F");
    EXPECT_DOUBLE_EQ(celsius.toUnit(283.15), 10.0);
    EXPECT_EQ(celsius.toStringUnit(283.15), "10.00°C");
    EXPECT_DOUBLE_EQ(fahrenheit.toUnit(283.15), 49.99999999999994);
    EXPECT_EQ(fahrenheit.toStringUnit(283.15), "50.00°F");
    EXPECT_DOUBLE_EQ(celsius.toUnit(0.0), -273.15);
    EXPECT_DOUBLE_EQ(celsius.fromUnit(0.0), 273.15);
    EXPECT_EQ(celsius.toStringUnit(0.0), "-273.15°C");
    EXPECT_DOUBLE_EQ(fahrenheit.toUnit(0.0), -459.67);
    EXPECT_DOUBLE_EQ(fahrenheit.fromUnit(0.0), 255.37222222222223);
    EXPECT_EQ(fahrenheit.toStringUnit(0.0), "-459.67°F");
    EXPECT_DOUBLE_EQ(celsius.toUnit(300.0), 26.850000000000023);
    EXPECT_EQ(celsius.toString(300.0), "26.85");
    EXPECT_DOUBLE_EQ(fahrenheit.toUnit(300.0), 80.32999999999998);
    EXPECT_EQ(fahrenheit.toString(300.0), "80.33");
    EXPECT_TRUE(std::isnan(celsius.toUnit(kNaN)));
    EXPECT_EQ(celsius.toString(kNaN), "NaN");
    EXPECT_EQ(celsius.toStringUnit(kNaN), "N/A");
    EXPECT_EQ(fahrenheit.toString(283.153), "50.01");
    EXPECT_EQ(fahrenheit.toStringUnit(310.928), "100.00°F");
    EXPECT_EQ(celsius.toStringUnit(310.928), "37.78°C");
    EXPECT_EQ(celsius.toStringUnit(-1.0), "-274.15°C");
    EXPECT_EQ(fahrenheit.toStringUnit(-1.0), "-461.47°F");
    EXPECT_DOUBLE_EQ(fahrenheit.fromUnit(-1.0), 254.8166666666667);
}

TEST(TemperatureUnit, RoundingTiesAreJavasMathRound)
{
    const TemperatureUnit celsius(1, 273.15, 0.01, "C");
    const TemperatureUnit fahrenheit(5.0 / 9.0, 459.67, 0.01, "F");
    EXPECT_EQ(celsius.round(0.005), 0.01);
    EXPECT_EQ(celsius.round(-0.005), 0.0);
    EXPECT_FALSE(std::signbit(celsius.round(-0.005)));
    EXPECT_EQ(celsius.round(1.005), 1.0);
    EXPECT_EQ(celsius.round(-2.675), -2.67);
    EXPECT_EQ(celsius.getNextValue(0.005), 0.02);
    EXPECT_EQ(celsius.getPreviousValue(0.005), 0.0);
    EXPECT_EQ(celsius.getNextValue(-1.015), -1.0);
    EXPECT_EQ(celsius.getPreviousValue(-1.015), -1.02);
    EXPECT_EQ(fahrenheit.round(98.605), 98.61);
    EXPECT_EQ(fahrenheit.getNextValue(-40.005), -40.0);
}

// ---- DegreeUnit and RadianUnit ----

TEST(DegreeUnit, FormatsWithOneDecimalAndNoSpace)
{
    const DegreeUnit degree;
    constexpr double kPi = std::numbers::pi;
    EXPECT_FALSE(degree.hasSpace());
    EXPECT_EQ(degree.getUnit(), "°");
    EXPECT_DOUBLE_EQ(degree.getMultiplier(), 0.017453292519943295);

    EXPECT_EQ(degree.toString(0.0), "0");
    EXPECT_EQ(degree.toStringUnit(0.0), "0°");
    EXPECT_EQ(degree.toString(kPi / 4), "45");
    EXPECT_EQ(degree.toStringUnit(kPi / 2), "90°");
    EXPECT_EQ(degree.toString(kPi), "180");
    EXPECT_EQ(degree.toString(-kPi / 180 * 0.04), "-0");
    EXPECT_EQ(degree.toStringUnit(-kPi / 180 * 0.04), "-0°");
    EXPECT_EQ(degree.toString(kPi / 180 * 45.25), "45.2");
    EXPECT_EQ(degree.toString(kPi / 180 * 45.35), "45.4");
    EXPECT_EQ(degree.toString(kNaN), "NaN");
    EXPECT_EQ(degree.toStringUnit(kNaN), "N/A");
    EXPECT_EQ(degree.toString(kPi / 180 * 1e7), "10000000");
    EXPECT_EQ(degree.toString(kInf), "∞");
    EXPECT_EQ(degree.toStringUnit(kInf), "∞°");
    EXPECT_EQ(degree.toString(-0.0), "-0");
    EXPECT_EQ(degree.toString(1.0), "57.3");
    EXPECT_EQ(degree.toString(2.5), "143.2");
    EXPECT_EQ(degree.toString(0.05), "2.9");
    EXPECT_EQ(degree.toString(-0.04), "-2.3");
    EXPECT_EQ(degree.toString(kPi / 180 * 0.05), "0.1");
    EXPECT_EQ(degree.toString(kPi / 180 * 12.34), "12.3");
    EXPECT_EQ(degree.toString(1e-9), "0");
    EXPECT_EQ(degree.toString(123.456), "7073.5");

    EXPECT_DOUBLE_EQ(degree.round(0.7853981633974483), 1.0);
    EXPECT_DOUBLE_EQ(degree.round(2.5), 2.0);
    EXPECT_DOUBLE_EQ(degree.round(-0.04), -0.0);
    EXPECT_DOUBLE_EQ(degree.round(174532.92519943297), 174533.0);
    EXPECT_TRUE(std::isnan(degree.round(kNaN)));
    EXPECT_EQ(degree.round(kInf), kInf);
}

TEST(DegreeUnit, LargeMagnitudesTiesAndZerosMatchOpenRocket)
{
    const DegreeUnit degree;
    EXPECT_EQ(degree.toString(-2.594816859051859e+23), "-14867205463306412000000000");
    EXPECT_EQ(degree.toString(793017819599871232.0), "45436574141739516000");
    EXPECT_EQ(degree.toString(4.7636871895947546e+23), "27293917088431583000000000");
    EXPECT_EQ(degree.toString(-1e-300), "-0");
    EXPECT_EQ(degree.toString(0.0043633231299858239), "0.2");
    EXPECT_EQ(degree.toString(-0.0026179938779914941), "-0.1");
}

TEST(RadianUnit, FormatsWithExactlyOneDecimal)
{
    const RadianUnit radian;
    constexpr double kPi = std::numbers::pi;
    EXPECT_TRUE(radian.hasSpace());
    EXPECT_EQ(radian.getUnit(), "rad");
    EXPECT_EQ(radian.getMultiplier(), 1.0);

    EXPECT_EQ(radian.toString(0.0), "0.0");
    EXPECT_EQ(radian.toStringUnit(0.0), "0.0 rad");
    EXPECT_EQ(radian.toString(kPi / 4), "0.8");
    EXPECT_EQ(radian.toString(kPi / 2), "1.6");
    EXPECT_EQ(radian.toString(kPi), "3.1");
    EXPECT_EQ(radian.toString(-6.981317007977319E-4), "-0.0");
    EXPECT_EQ(radian.toStringUnit(-6.981317007977319E-4), "-0.0 rad");
    EXPECT_EQ(radian.toString(kNaN), "NaN");
    EXPECT_EQ(radian.toStringUnit(kNaN), "N/A");
    EXPECT_EQ(radian.toString(174532.92519943297), "174532.9");
    EXPECT_EQ(radian.toString(kInf), "∞");
    EXPECT_EQ(radian.toStringUnit(kInf), "∞ rad");
    EXPECT_EQ(radian.toString(-0.0), "-0.0");
    EXPECT_EQ(radian.toString(1.0), "1.0");
    EXPECT_EQ(radian.toString(2.5), "2.5");
    EXPECT_EQ(radian.toString(0.05), "0.1");
    EXPECT_EQ(radian.toString(0.15), "0.1");
    EXPECT_EQ(radian.toString(0.25), "0.2");
    EXPECT_EQ(radian.toString(-0.04), "-0.0");
    EXPECT_EQ(radian.toString(123.456), "123.5");

    EXPECT_DOUBLE_EQ(radian.round(0.7853981633974483), 0.8);
    EXPECT_DOUBLE_EQ(radian.round(1.5707963267948966), 1.6);
    EXPECT_DOUBLE_EQ(radian.round(0.05), 0.0);
    EXPECT_DOUBLE_EQ(radian.round(0.15), 0.2);
    EXPECT_DOUBLE_EQ(radian.round(0.25), 0.2);
    EXPECT_DOUBLE_EQ(radian.round(123.456), 123.5);
    EXPECT_TRUE(std::isnan(radian.round(kNaN)));
}

TEST(RadianUnit, LargeMagnitudesTiesAndZerosMatchOpenRocket)
{
    const RadianUnit radian;
    EXPECT_EQ(radian.toString(0x1p69), "590295810358705650000.0");
    EXPECT_EQ(radian.toString(1e23), "99999999999999990000000.0");
    EXPECT_EQ(radian.toString(8.41e21), "8409999999999999000000.0");
    EXPECT_EQ(radian.toString(0.35), "0.3");
    EXPECT_EQ(radian.toString(0.45), "0.5");
    EXPECT_EQ(radian.toString(-0.05), "-0.1");
    EXPECT_EQ(radian.toString(-1e-300), "-0.0");
    EXPECT_EQ(radian.toString(9.95), "9.9");
    EXPECT_EQ(radian.toString(-99.95), "-100.0");
}

// ---- FrequencyUnit ----

TEST(FrequencyUnit, InvertsThePeriod)
{
    const FrequencyUnit hz(1, "Hz");
    const FrequencyUnit mhz(0.001, "mHz");
    const FrequencyUnit khz(1000, "kHz");

    EXPECT_DOUBLE_EQ(hz.toUnit(0.5), 2.0);
    EXPECT_DOUBLE_EQ(hz.fromUnit(0.5), 2.0);
    EXPECT_EQ(hz.toString(0.5), "2");
    EXPECT_DOUBLE_EQ(mhz.toUnit(0.5), 2000.0);
    EXPECT_EQ(mhz.toString(0.5), "2000");
    EXPECT_DOUBLE_EQ(khz.toUnit(0.5), 0.002);
    EXPECT_EQ(khz.toString(0.5), "0.002");
    EXPECT_DOUBLE_EQ(hz.toUnit(2.0), 0.5);
    EXPECT_EQ(hz.toString(2.0), "0.5");
    EXPECT_DOUBLE_EQ(khz.toUnit(2.0), 5.0E-4);
    EXPECT_EQ(khz.toString(2.0), "0");
    EXPECT_EQ(hz.toUnit(0.0), kInf);
    EXPECT_EQ(hz.toString(0.0), "∞");
    EXPECT_EQ(hz.toUnit(kInf), 0.0);
    EXPECT_EQ(hz.toString(kInf), "0");
    EXPECT_TRUE(std::isnan(hz.toUnit(kNaN)));
    EXPECT_EQ(hz.toString(kNaN), "N/A");
    EXPECT_DOUBLE_EQ(hz.toUnit(0.001), 1000.0);
    EXPECT_EQ(mhz.toString(0.001), "1000000");
    EXPECT_EQ(khz.toString(0.001), "1");
    EXPECT_EQ(hz.toString(1000.0), "0.001");
    EXPECT_DOUBLE_EQ(khz.toUnit(1000.0), 1.0E-6);
    EXPECT_EQ(khz.toString(1000.0), "0");
    EXPECT_DOUBLE_EQ(hz.toUnit(-0.5), -2.0);
    EXPECT_EQ(hz.toString(-0.5), "-2");
    EXPECT_EQ(khz.toString(-0.5), "-0.002");
}

}  // namespace
