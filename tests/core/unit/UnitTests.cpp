#include "QtRocket/unit/Unit.h"

#include <limits>
#include <locale>
#include <string>

#include <gtest/gtest.h>

#include "QtRocket/unit/FixedPrecisionUnit.h"
#include "QtRocket/unit/FrequencyUnit.h"
#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/InchUnit.h"
#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/unit/Value.h"
#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::BugError;
using QtRocket::FixedPrecisionUnit;
using QtRocket::FrequencyUnit;
using QtRocket::GeneralUnit;
using QtRocket::InchUnit;
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

/// A German-style number format (decimal comma, point grouping) that needs no installed locale.
/// Constructed with refs = 1, so no std::locale ever deletes it.
class CommaDecimalPoint : public std::numpunct<char>
{
public:
    CommaDecimalPoint() : std::numpunct<char>(1) { }

protected:
    [[nodiscard]] char        do_decimal_point() const override { return ','; }
    [[nodiscard]] char        do_thousands_sep() const override { return '.'; }
    [[nodiscard]] std::string do_grouping() const override { return "\3"; }
};

/// Makes a comma-decimal locale the global C++ locale until destroyed.
class CommaDecimalGlobalLocale
{
public:
    CommaDecimalGlobalLocale()
      : m_previous(std::locale::global(std::locale(std::locale::classic(), &facet())))
    {
    }
    ~CommaDecimalGlobalLocale() { std::locale::global(m_previous); }
    CommaDecimalGlobalLocale(const CommaDecimalGlobalLocale&)            = delete;
    CommaDecimalGlobalLocale& operator=(const CommaDecimalGlobalLocale&) = delete;
    CommaDecimalGlobalLocale(CommaDecimalGlobalLocale&&)                 = delete;
    CommaDecimalGlobalLocale& operator=(CommaDecimalGlobalLocale&&)      = delete;

private:
    [[nodiscard]] static CommaDecimalPoint& facet()
    {
        static CommaDecimalPoint s_facet;
        return s_facet;
    }

    std::locale m_previous;
};

// testLocaleChangeAfterUnitInitialization: OpenRocket formats with the default locale, so under
// Locale.GERMANY it gives "-0,001". Deviation: QtRocket always writes a point, whatever the
// process locale, which this pins.
TEST(UnitToString, LocaleChangeAfterUnitInitialization)
{
    EXPECT_EQ(Unit::noUnit().toString(-0.00051), "-0.001");
    const CommaDecimalGlobalLocale german;
    EXPECT_EQ(Unit::noUnit().toString(-0.00051), "-0.001");
    EXPECT_EQ(Unit::noUnit().toString(1234.5), "1234");
    EXPECT_EQ(Unit::noUnit().toString(1.5e7), "1.50E7");
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getUnit("mm")->toStringUnit(0.0125), "12.5 mm");
    EXPECT_EQ(FixedPrecisionUnit("u", 0.01).toString(1.5), "1.50");
}

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

    // UNITS_NONE holds Unit.NOUNIT itself, which a group without a unit of multiplier 1 falls
    // back to as its SI unit, so Values made from them are equal.
    EXPECT_EQ(&unitGroup(UnitGroupId::NONE).getDefaultUnit(), &none);
    EXPECT_EQ(&unitGroup(UnitGroupId::NONE).getUnit(0), &none);
    const QtRocket::UnitGroup noUnits;
    EXPECT_EQ(&noUnits.getSIUnit(), &none);
    EXPECT_TRUE(QtRocket::Value(1.0, none) == unitGroup(UnitGroupId::NONE).toValue(1.0));
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

}  // namespace
