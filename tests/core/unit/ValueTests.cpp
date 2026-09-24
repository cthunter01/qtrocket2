#include "QtRocket/unit/Value.h"

#include <algorithm>
#include <cmath>
#include <compare>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"

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
    if (unit == nullptr)
    {
        throw std::logic_error("no such temperature unit");
    }
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
    EXPECT_EQ(std::hash<Value>{}(a), std::hash<Value>{}(b));
    EXPECT_NE(std::hash<Value>{}(a), std::hash<Value>{}(d));
    // equals never holds for NaN (MathUtil::equals), while compareTo puts equal NaNs together.
    const Value nan1(kNaN, mm);
    const Value nan2(kNaN, mm);
    EXPECT_FALSE(nan1 == nan2);
    EXPECT_EQ(nan1.compareTo(nan2), 0);
}

TEST(Value, OrderingComparesUnitNameThenUnitValue)
{
    const GeneralUnit cm(0.01, "cm");
    const GeneralUnit m(1, "m");
    const GeneralUnit mm(0.001, "mm");

    // "cm" < "m" < "mm" by name, whatever the values.
    EXPECT_TRUE(Value(100.0, cm) < Value(0.001, m));
    EXPECT_TRUE(Value(100.0, m) < Value(0.001, mm));
    EXPECT_TRUE((Value(1.0, cm) <=> Value(1.0, m)) == std::weak_ordering::less);

    // The same unit name: by the value in that unit.
    EXPECT_TRUE(Value(0.1, cm) < Value(0.2, cm));
    EXPECT_TRUE(Value(0.2, cm) > Value(0.1, cm));
    EXPECT_TRUE((Value(0.1, cm) <=> Value(0.1, cm)) == std::weak_ordering::equivalent);
    // Double.compare's order: -0.0 below 0.0 and NaN above everything.
    EXPECT_TRUE(Value(-0.0, cm) < Value(0.0, cm));
    EXPECT_TRUE(Value(1e300, cm) < Value(kNaN, cm));
    EXPECT_TRUE(Value(kNaN, cm) > Value(1e300, cm));
    EXPECT_TRUE((Value(kNaN, cm) <=> Value(kNaN, cm)) == std::weak_ordering::equivalent);

    // Different Unit objects with the same name order by value (ValueComparator semantics).
    const GeneralUnit cmToo(0.01, "cm");
    EXPECT_TRUE(Value(0.1, cm) < Value(0.2, cmToo));
    EXPECT_EQ(Value(0.1, cm).compareTo(Value(0.1, cmToo)), 0);

    std::vector<Value> sorted{Value(0.3, cm), Value(0.1, m), Value(0.1, cm), Value(kNaN, cm)};
    std::ranges::sort(sorted);
    EXPECT_EQ(sorted[0].getValue(), 0.1);
    EXPECT_EQ(&sorted[0].getUnit(), &cm);
    EXPECT_EQ(sorted[1].getValue(), 0.3);
    EXPECT_TRUE(std::isnan(sorted[2].getValue()));
    EXPECT_EQ(&sorted[3].getUnit(), &m);
}

}  // namespace
