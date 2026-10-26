#include "QtRocket/motor/chooser/ThrustCurveMotorColumns.h"

#include <memory>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "unit/DefaultUnitsGuard.h"

// OpenRocket has no JUnit test of ThrustCurveMotorColumns; the table's texts and order over the
// bundled database are pinned in motor_chooser_golden_tests.cpp.

namespace
{

using QtRocket::Coordinate;
using QtRocket::InMemoryPreferences;
using QtRocket::Manufacturer;
using QtRocket::Motor;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustCurveMotorColumns;
using QtRocket::ThrustCurveMotorSet;
using QtRocket::Test::DefaultUnitsGuard;

/// A set of one motor of @p manufacturer, @p designation (common name @p commonName) and case
/// @p caseInfo, @p diameter x @p length, of 10 Ns total impulse.
struct SetSpec
{
    std::string manufacturer;
    std::string designation;
    std::string commonName;
    std::string caseInfo;
    double      diameter;
    double      length;
};

ThrustCurveMotorSet setOf(SetSpec spec)
{
    QtRocket::Result<ThrustCurveMotor> built =
        ThrustCurveMotor::Builder()
            .setManufacturer(Manufacturer::getManufacturer(spec.manufacturer))
            .setDesignation(std::move(spec.designation))
            .setCommonName(std::move(spec.commonName))
            .setCaseInfo(std::move(spec.caseInfo))
            .setDiameter(spec.diameter)
            .setLength(spec.length)
            .setTimePoints({0, 1, 2})
            .setThrustPoints({0, 10, 0})
            .setCGPoints({Coordinate(0.01, 0, 0, 0.1), Coordinate(0.01, 0, 0, 0.08),
                          Coordinate(0.01, 0, 0, 0.06)})
            .setMotorType(Motor::Type::RELOAD)
            .build();
    EXPECT_TRUE(built.has_value());
    ThrustCurveMotorSet set;
    if (built.has_value())
    {
        set.addMotor(std::make_shared<const ThrustCurveMotor>(std::move(*built)));
    }
    return set;
}

ThrustCurveMotorSet cesaroni()
{
    return setOf({.manufacturer = "Cesaroni",
                  .designation  = "14263N3400-P",
                  .commonName   = "N3400",
                  .caseInfo     = "Pro98-6GXL",
                  .diameter     = 0.098,
                  .length       = 1.239});
}

ThrustCurveMotorSet loki()
{
    return setOf({.manufacturer = "Loki",
                  .designation  = "N5500LW",
                  .commonName   = "N5500",
                  .caseInfo     = "Pro-98 6g XL",
                  .diameter     = 0.098,
                  .length       = 1.383});
}

TEST(ThrustCurveMotorColumns, HasOpenRocketsTitlesAndWidths)
{
    EXPECT_EQ(QtRocket::kAllThrustCurveMotorColumns.size(), 6U);
    EXPECT_EQ(getTitleKey(ThrustCurveMotorColumns::MANUFACTURER), "TCurveMotorCol.MANUFACTURER");
    EXPECT_EQ(getTitleKey(ThrustCurveMotorColumns::NAME), "TCurveMotorCol.NAME");
    EXPECT_EQ(getTitleKey(ThrustCurveMotorColumns::TOTAL_IMPULSE), "TCurveMotorCol.TOTAL_IMPULSE");
    EXPECT_EQ(getTitleKey(ThrustCurveMotorColumns::CASEINFO), "TCurveMotorCol.CASEINFO");
    EXPECT_EQ(getTitleKey(ThrustCurveMotorColumns::DIAMETER), "TCurveMotorCol.DIAMETER");
    EXPECT_EQ(getTitleKey(ThrustCurveMotorColumns::LENGTH), "TCurveMotorCol.LENGTH");
    EXPECT_EQ(getWidth(ThrustCurveMotorColumns::MANUFACTURER), 100);
    EXPECT_EQ(getWidth(ThrustCurveMotorColumns::LENGTH), 50);
}

TEST(ThrustCurveMotorColumns, ShowsTheSetsValues)
{
    const DefaultUnitsGuard   guard;
    InMemoryPreferences       preferences;
    const ThrustCurveMotorSet set = cesaroni();
    EXPECT_EQ(getValueText(ThrustCurveMotorColumns::MANUFACTURER, set, preferences),
              "Cesaroni Technology Inc.");
    EXPECT_EQ(getValueText(ThrustCurveMotorColumns::NAME, set, preferences), "14263N3400-P");
    EXPECT_EQ(getValueText(ThrustCurveMotorColumns::TOTAL_IMPULSE, set, preferences), "10");
    EXPECT_EQ(getValueText(ThrustCurveMotorColumns::CASEINFO, set, preferences), "Pro98-6GXL");
    EXPECT_EQ(getValueText(ThrustCurveMotorColumns::DIAMETER, set, preferences), "98 mm");
    EXPECT_EQ(getValueText(ThrustCurveMotorColumns::LENGTH, set, preferences), "1239 mm");
    preferences.setMotorNameColumn(false);
    EXPECT_EQ(getValueText(ThrustCurveMotorColumns::NAME, set, preferences), "N3400");
}

/// The dimensions follow the motor dimension unit group's default unit.
TEST(ThrustCurveMotorColumns, ShowsTheDimensionsInTheDefaultUnit)
{
    const DefaultUnitsGuard   guard;
    const InMemoryPreferences preferences;
    const ThrustCurveMotorSet set   = cesaroni();
    QtRocket::UnitGroup&      units = QtRocket::unitGroup(QtRocket::UnitGroupId::MOTOR_DIMENSIONS);
    ASSERT_TRUE(units.setDefaultUnit("in"));
    EXPECT_EQ(getValueText(ThrustCurveMotorColumns::DIAMETER, set, preferences),
              units.toStringUnit(0.098));
    EXPECT_NE(getValueText(ThrustCurveMotorColumns::DIAMETER, set, preferences), "98 mm");
}

/// The Case column sorts with the tertiary collator: Loki's "Pro-98 6g XL" after Cesaroni's
/// "Pro98-6GXL" (the primary collator calls them equal).
TEST(ThrustCurveMotorColumns, SortsTextsWithTheTertiaryCollator)
{
    const InMemoryPreferences preferences;
    const ThrustCurveMotorSet a = cesaroni();
    const ThrustCurveMotorSet b = loki();
    EXPECT_EQ(QtRocket::Strings::javaPrimaryCollatorCompare("Pro-98 6g XL", "Pro98-6GXL"), 0);
    EXPECT_GT(compareValues(ThrustCurveMotorColumns::CASEINFO, b, a, preferences), 0);
    EXPECT_LT(compareValues(ThrustCurveMotorColumns::CASEINFO, a, b, preferences), 0);
    EXPECT_LT(compareValues(ThrustCurveMotorColumns::MANUFACTURER, a, b, preferences), 0);
    EXPECT_EQ(compareValues(ThrustCurveMotorColumns::DIAMETER, a, b, preferences), 0);
    EXPECT_LT(compareValues(ThrustCurveMotorColumns::LENGTH, a, b, preferences), 0);
    EXPECT_EQ(compareValues(ThrustCurveMotorColumns::TOTAL_IMPULSE, a, b, preferences), 0);
}

/// The Name column sorts the shown names with DesignationComparator: by class, then thrust.
TEST(ThrustCurveMotorColumns, SortsNamesAsDesignations)
{
    InMemoryPreferences       preferences;
    const ThrustCurveMotorSet a = cesaroni();
    const ThrustCurveMotorSet b = loki();
    // "14263N3400-P" against "N5500LW": both N, 3400 before 5500.
    EXPECT_LT(compareValues(ThrustCurveMotorColumns::NAME, a, b, preferences), 0);
    preferences.setMotorNameColumn(false);
    EXPECT_LT(compareValues(ThrustCurveMotorColumns::NAME, a, b, preferences), 0);
    EXPECT_GT(compareValues(ThrustCurveMotorColumns::NAME, b, a, preferences), 0);
}

TEST(ThrustCurveMotorColumns, RefusesAnEmptySet)
{
    const InMemoryPreferences preferences;
    EXPECT_THROW(static_cast<void>(getValueText(ThrustCurveMotorColumns::CASEINFO,
                                                ThrustCurveMotorSet{}, preferences)),
                 QtRocket::BugError);
    EXPECT_THROW(static_cast<void>(compareValues(ThrustCurveMotorColumns::NAME,
                                                 ThrustCurveMotorSet{}, cesaroni(), preferences)),
                 QtRocket::BugError);
}

}  // namespace
