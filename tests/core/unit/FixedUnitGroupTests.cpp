#include "QtRocket/unit/FixedUnitGroup.h"

#include <gtest/gtest.h>

#include "QtRocket/unit/DegreeUnit.h"
#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/unit/Value.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace
{

using QtRocket::BugError;
using QtRocket::DegreeUnit;
using QtRocket::ErrorCode;
using QtRocket::FixedUnitGroup;
using QtRocket::GeneralUnit;
using QtRocket::unitGroup;
using QtRocket::UnitGroup;
using QtRocket::UnitGroupId;
using QtRocket::Value;

// Every expectation was checked against OpenRocket's FixedUnitGroup on JDK 17.

// ---- QtRocket additions ----

TEST(FixedUnitGroup, IsOneArbitraryUnit)
{
    const FixedUnitGroup furlongs("furlong");
    EXPECT_EQ(furlongs.getUnitString(), "furlong");
    EXPECT_EQ(furlongs.getUnitCount(), 1);
    EXPECT_EQ(furlongs.getDefaultUnitIndex(), 0);
    EXPECT_EQ(furlongs.getDefaultUnit().getUnit(), "furlong");
    EXPECT_EQ(furlongs.getDefaultUnit().getMultiplier(), 1.0);
    EXPECT_EQ(&furlongs.getSIUnit(), &furlongs.getDefaultUnit());
    EXPECT_NE(dynamic_cast<const GeneralUnit*>(&furlongs.getDefaultUnit()), nullptr);
    EXPECT_TRUE(furlongs.contains(GeneralUnit(1, "furlong")));
    EXPECT_TRUE(furlongs.contains(GeneralUnit(3, "anything")));  // every unit
    EXPECT_TRUE(furlongs.contains(DegreeUnit()));
    EXPECT_EQ(furlongs.toString(), "FixedUnitGroup:furlong");
    EXPECT_EQ(furlongs.toString(2.5), "2.5");
    EXPECT_EQ(furlongs.toStringUnit(2.5), "2.5 furlong");
    EXPECT_EQ(furlongs.fromUnit(2.5), 2.5);
    EXPECT_EQ(furlongs.toValue(2.5).toString(), "2.5 furlong");
    EXPECT_EQ(Value(2.5, furlongs).toString(), "2.5 furlong");
}

TEST(FixedUnitGroup, EverythingElseSeesAnEmptyUnitList)
{
    // OpenRocket's FixedUnitGroup keeps UnitGroup's unit list empty.
    const FixedUnitGroup furlongs("furlong");
    EXPECT_TRUE(furlongs.getUnits().empty());
    EXPECT_EQ(furlongs.getUnit("furlong"), nullptr);  // Java: IllegalArgumentException
    EXPECT_EQ(furlongs.findApproximate("furlong"), nullptr);
    EXPECT_EQ(furlongs.getUnitIndex(GeneralUnit(1, "furlong")), -1);
    EXPECT_THROW(static_cast<void>(furlongs.getUnit(0)), BugError);  // IndexOutOfBoundsException

    FixedUnitGroup mutableFurlongs("furlong");
    EXPECT_THROW(mutableFurlongs.setDefaultUnit(0), BugError);  // "index out of range: 0"
    EXPECT_FALSE(mutableFurlongs.setDefaultUnit("furlong"));

    // A bare number is in the default unit; no unit name is known, not even the group's own.
    EXPECT_EQ(furlongs.fromString("7").value(), 7.0);
    const QtRocket::Result<double> named = furlongs.fromString("7 furlong");
    ASSERT_FALSE(named.has_value());
    EXPECT_EQ(named.error().code, ErrorCode::PARSE);
    EXPECT_EQ(named.error().message, "unknown unit furlong");
}

TEST(FixedUnitGroup, EqualsEveryGroupWithoutUnits)
{
    // UnitGroup.equals compares the unit lists, which are empty, so every FixedUnitGroup equals
    // every other and any group without units (OpenRocket's FlightDataType relies on it).
    const FixedUnitGroup furlongs("furlong");
    const FixedUnitGroup miles("mile");
    const UnitGroup      noUnits;
    EXPECT_TRUE(furlongs.equals(miles));
    EXPECT_TRUE(furlongs == miles);
    EXPECT_TRUE(furlongs.equals(noUnits));
    EXPECT_TRUE(noUnits.equals(furlongs));
    EXPECT_FALSE(furlongs.equals(unitGroup(UnitGroupId::SHORT_TIME)));
    EXPECT_FALSE(unitGroup(UnitGroupId::SHORT_TIME).equals(furlongs));
    EXPECT_EQ(furlongs.hash(), 0U);
    EXPECT_EQ(miles.hash(), 0U);
}

TEST(FixedUnitGroup, ValuesShareTheGroupsOneUnit)
{
    // Deviation: OpenRocket makes a new unit on each getDefaultUnit() call, so its Values from
    // one FixedUnitGroup never compare equal; here they share the group's unit.
    const FixedUnitGroup furlongs("furlong");
    EXPECT_EQ(&furlongs.getDefaultUnit(), &furlongs.getDefaultUnit());
    EXPECT_TRUE(furlongs.toValue(1.0) == furlongs.toValue(1.0));
}

}  // namespace
