#include "QtRocket/unit/Value.h"

#include <limits>
#include <memory>

#include <gtest/gtest.h>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::GeneralUnit;
using QtRocket::Unit;
using QtRocket::UnitGroup;
using QtRocket::unitGroup;
using QtRocket::UnitGroupId;
using QtRocket::Value;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

const Unit& temperatureUnit(const char* approximateName)
{
    const Unit* unit = unitGroup(UnitGroupId::TEMPERATURE).findApproximate(approximateName);
    QTROCKET_ASSERT(unit != nullptr);  // a typo in the test itself
    return *unit;
}

// ---- Ported from ValueTest.java ----

TEST(Value, Values)
{
    Value v1(273.15, temperatureUnit("F"));
    Value v2(283.153, temperatureUnit("C"));

    EXPECT_GT(v1.compareTo(v2), 0);
    EXPECT_LT(v2.compareTo(v1), 0);
    EXPECT_EQ(v1.compareTo(v1), 0);
    EXPECT_EQ(v2.compareTo(v2), 0);

    v2 = Value(283.15, temperatureUnit("K"));
    EXPECT_GT(v1.compareTo(v2), 0);
    EXPECT_LT(v2.compareTo(v1), 0);
    EXPECT_EQ(v2.toString(), "283.15 K");

    v2 = Value(283.15, temperatureUnit("F"));
    EXPECT_LT(v1.compareTo(v2), 0);
    EXPECT_GT(v2.compareTo(v1), 0);

    v1 = Value(kNaN, temperatureUnit("F"));
    EXPECT_GT(v1.compareTo(v2), 0);
    EXPECT_LT(v2.compareTo(v1), 0);

    v2 = Value(kNaN, temperatureUnit("F"));
    EXPECT_EQ(v1.compareTo(v2), 0);
    EXPECT_EQ(v1.compareTo(v2), 0);
    EXPECT_EQ(v1.toString(), "N/A");
    EXPECT_EQ(v2.toString(), "N/A");
}

// ---- QtRocket additions ----

TEST(Value, GettersAndUnitValue)
{
    const GeneralUnit mm(0.001, "mm");
    const Value       value(0.25, mm);
    EXPECT_EQ(value.getValue(), 0.25);
    EXPECT_DOUBLE_EQ(value.getUnitValue(), 250.0);
    EXPECT_EQ(&value.getUnit(), &mm);
    EXPECT_EQ(value.toString(), "250 mm");

    // The group constructor takes the group's default unit.
    UnitGroup::resetDefaultUnits();
    const Value fromGroup(0.25, unitGroup(UnitGroupId::LENGTH));
    EXPECT_EQ(&fromGroup.getUnit(), &unitGroup(UnitGroupId::LENGTH).getDefaultUnit());
    EXPECT_EQ(fromGroup.toString(), "25 cm");
}

TEST(Value, EqualsNeedsTheSameUnitObject)
{
    const GeneralUnit mm(0.001, "mm");
    const GeneralUnit mmToo(0.001, "mm");
    const Value       a(0.25, mm);
    const Value       b(0.25, mm);
    const Value       c(0.25 + 1e-12, mm);  // within MathUtil::equals
    const Value       d(0.26, mm);
    const Value       e(0.25, mmToo);  // an equal but different Unit object

    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a == c);
    EXPECT_FALSE(a == d);
    EXPECT_FALSE(a == e);
    EXPECT_TRUE(a != e);
    EXPECT_EQ(a.hash(), b.hash());
    EXPECT_NE(a.hash(), d.hash());
    // equals never holds between two NaN values (MathUtil::equals), while compareTo puts equal
    // NaNs together; a Value is always equal to itself (Java's this == obj shortcut).
    const Value nan1(kNaN, mm);
    const Value nan2(kNaN, mm);
    EXPECT_FALSE(nan1 == nan2);
    EXPECT_EQ(nan1.compareTo(nan2), 0);
    const Value& sameObject = nan1;
    EXPECT_TRUE(nan1 == sameObject);
}

TEST(Value, UnitsSharedBetweenGroupsGiveEqualValues)
{
    // A StabilityUnitGroup shares UNITS_STABILITY's plain units (units.addAll), so Values in its
    // millimetres equal Values in UNITS_STABILITY's; its own caliber unit is another object.
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> stability =
        UnitGroup::stabilityUnits(0.05);
    const UnitGroup& source = unitGroup(UnitGroupId::STABILITY);
    EXPECT_EQ(&stability->getUnit(0), &source.getUnit(0));
    EXPECT_TRUE(Value(0.01, stability->getUnit(0)) == Value(0.01, source.getUnit(0)));
    EXPECT_NE(&stability->getUnit(4), &source.getUnit(4));
    EXPECT_FALSE(Value(0.01, stability->getUnit(4)) == Value(0.01, source.getUnit(4)));

    // A Value of the dimensionless unit equals one of UNITS_NONE, the same object.
    EXPECT_TRUE(Value(2.0, Unit::noUnit()) == Value(2.0, unitGroup(UnitGroupId::NONE)));
}

TEST(Value, CompareToOrdersByUnitNameThenUnitValue)
{
    const GeneralUnit cm(0.01, "cm");
    const GeneralUnit m(1, "m");
    const GeneralUnit mm(0.001, "mm");

    // "cm" < "m" < "mm" by name, whatever the values.
    EXPECT_LT(Value(100.0, cm).compareTo(Value(0.001, m)), 0);
    EXPECT_LT(Value(100.0, m).compareTo(Value(0.001, mm)), 0);
    EXPECT_GT(Value(1.0, m).compareTo(Value(1.0, cm)), 0);

    // The same unit name: by the value in that unit.
    EXPECT_LT(Value(0.1, cm).compareTo(Value(0.2, cm)), 0);
    EXPECT_GT(Value(0.2, cm).compareTo(Value(0.1, cm)), 0);
    EXPECT_EQ(Value(0.1, cm).compareTo(Value(0.1, cm)), 0);
    // Double.compare's order: -0.0 below 0.0 and NaN above everything.
    EXPECT_LT(Value(-0.0, cm).compareTo(Value(0.0, cm)), 0);
    EXPECT_LT(Value(1e300, cm).compareTo(Value(kNaN, cm)), 0);
    EXPECT_GT(Value(kNaN, cm).compareTo(Value(1e300, cm)), 0);
    EXPECT_EQ(Value(kNaN, cm).compareTo(Value(kNaN, cm)), 0);

    // Different Unit objects with the same name order by value.
    const GeneralUnit cmToo(0.01, "cm");
    EXPECT_LT(Value(0.1, cm).compareTo(Value(0.2, cmToo)), 0);
    EXPECT_EQ(Value(0.1, cm).compareTo(Value(0.1, cmToo)), 0);
}

TEST(Value, UnitNamesCompareAsJavaStrings)
{
    // String.compareTo compares UTF-16 code units: a name with a character above U+FFFF (a
    // surrogate pair, D800-DFFF) sorts before one with U+FF4D (fullwidth m), where UTF-8 bytes
    // would sort it after. The difference is that of the first differing code units.
    const GeneralUnit astral(1, "\U0001D40C");  // MATHEMATICAL BOLD CAPITAL M
    const GeneralUnit fullwidth(1, "\uFF4D");
    EXPECT_EQ(Value(1.0, astral).compareTo(Value(1.0, fullwidth)), 0xD835 - 0xFF4D);
    EXPECT_GT(Value(1.0, fullwidth).compareTo(Value(1.0, astral)), 0);
}

}  // namespace
