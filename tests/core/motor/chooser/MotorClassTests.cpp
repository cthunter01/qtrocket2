#include "QtRocket/motor/chooser/MotorClass.h"

#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "unit/DefaultUnitsGuard.h"

// OpenRocket has no JUnit test of MotorClass; its answers for the bundled database's motors and
// some impulses are pinned in motor_chooser_golden_tests.cpp (MotorChooserPinsGolden).

namespace
{

using QtRocket::BugError;
using QtRocket::MotorClass;
using QtRocket::Test::DefaultUnitsGuard;

TEST(MotorClass, HasOpenRocketsNamesAndRanges)
{
    EXPECT_EQ(QtRocket::kAllMotorClasses.size(), 19U);
    EXPECT_EQ(enumName(MotorClass::A18), "A18");
    EXPECT_EQ(className(MotorClass::A18), "1/8A");
    EXPECT_EQ(className(MotorClass::A12), "1/2A");
    EXPECT_EQ(className(MotorClass::OVER), "> O");
    EXPECT_EQ(getMin(MotorClass::A18), 0.0);
    EXPECT_EQ(getMax(MotorClass::A18), 0.3125);
    EXPECT_EQ(getMin(MotorClass::A), 1.25);
    EXPECT_EQ(getMax(MotorClass::O), 40960.0);
    EXPECT_EQ(getMax(MotorClass::OVER), std::numeric_limits<double>::max());
}

/// A class's upper end (plus 1e-7) belongs to it; from 100 Ns the impulse is rounded to whole Ns,
/// half to even.
TEST(MotorClass, ClassifiesAnImpulse)
{
    EXPECT_EQ(QtRocket::getMotorClass(-5), MotorClass::A18);
    EXPECT_EQ(QtRocket::getMotorClass(0.3125), MotorClass::A18);
    EXPECT_EQ(QtRocket::getMotorClass(0.3125001), MotorClass::A18);
    EXPECT_EQ(QtRocket::getMotorClass(0.3125002), MotorClass::A14);
    EXPECT_EQ(QtRocket::getMotorClass(10), MotorClass::C);
    EXPECT_EQ(QtRocket::getMotorClass(99.9), MotorClass::G);
    EXPECT_EQ(QtRocket::getMotorClass(640.4), MotorClass::I);
    EXPECT_EQ(QtRocket::getMotorClass(640.5), MotorClass::I);
    EXPECT_EQ(QtRocket::getMotorClass(641.5), MotorClass::J);
    EXPECT_EQ(QtRocket::getMotorClass(40960.4), MotorClass::O);
    EXPECT_EQ(QtRocket::getMotorClass(std::numeric_limits<double>::max()), MotorClass::OVER);
}

/// The message of the BugError getMotorClass(@p impulse) throws, or "" when it throws none.
std::string motorClassError(double impulse)
{
    try
    {
        static_cast<void>(QtRocket::getMotorClass(impulse));
    }
    catch (const BugError& error)
    {
        return error.what();
    }
    return "";
}

TEST(MotorClass, FindsNoClassForNanOrInfinity)
{
    EXPECT_NE(motorClassError(std::numeric_limits<double>::quiet_NaN())
                  .find("Could not find motor class for impulse NaN"),
              std::string::npos);
    EXPECT_NE(motorClassError(std::numeric_limits<double>::infinity())
                  .find("Could not find motor class for impulse Infinity"),
              std::string::npos);
    EXPECT_EQ(motorClassError(1), "");
}

/// The percentage of the class, at least 1, rounded as Math.round.
TEST(MotorClass, DescribesWhereAnImpulseLiesInItsClass)
{
    EXPECT_EQ(getPercent(MotorClass::C, 8.82), 76);
    EXPECT_EQ(getDescription(MotorClass::C, 8.82), "76% C");
    EXPECT_EQ(getDescription(MotorClass::A18, 0.1), "32% 1/8A");
    EXPECT_EQ(getDescription(MotorClass::B, 2.5), "1% B");
    EXPECT_EQ(getPercent(MotorClass::C, 5.025), 1);   // 0.5 %
    EXPECT_EQ(getPercent(MotorClass::C, 9.99), 100);  // 99.8 %
    EXPECT_EQ(getPercent(MotorClass::C, std::numeric_limits<double>::quiet_NaN()), 0);
    EXPECT_EQ(getDescription(MotorClass::OVER, 50000), "Over O");
}

/// The class's range in the impulse unit group's default unit.
TEST(MotorClass, DescribesTheClassInTheDefaultUnit)
{
    const DefaultUnitsGuard guard;
    EXPECT_EQ(getClassDescription(MotorClass::C), "Class C (5 Ns - 10 Ns)");
    EXPECT_EQ(getClassDescription(MotorClass::A18), "Class 1/8A (0 Ns - 0.312 Ns)");
    EXPECT_EQ(getClassDescription(MotorClass::OVER), "Over O-class (over 40960 Ns)");
    QtRocket::UnitGroup& impulse = QtRocket::unitGroup(QtRocket::UnitGroupId::IMPULSE);
    impulse.setDefaultUnit(1);
    EXPECT_EQ(getClassDescription(MotorClass::C),
              "Class C (" + impulse.toStringUnit(5) + " - " + impulse.toStringUnit(10) + ")");
    EXPECT_NE(getClassDescription(MotorClass::C), "Class C (5 Ns - 10 Ns)");
}

}  // namespace
