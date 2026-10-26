#include "QtRocket/motor/chooser/MotorFilterSliders.h"

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "unit/DefaultUnitsGuard.h"

// OpenRocket has no JUnit test of MotorFilterPanel; the labels and the limits of a 28 mm mount are
// pinned in motor_chooser_golden_tests.cpp.

namespace
{

namespace MotorFilterSliders = QtRocket::MotorFilterSliders;
using QtRocket::Test::DefaultUnitsGuard;

using Labels = std::vector<std::string>;

TEST(MotorFilterSliders, LabelsTheDiameterStops)
{
    const DefaultUnitsGuard guard;
    EXPECT_EQ(MotorFilterSliders::diameterLabels(),
              (Labels{"0", "6", "13", "18", "24", "29", "32", "38", "54", "75", "98", "+"}));
    ASSERT_TRUE(QtRocket::unitGroup(QtRocket::UnitGroupId::MOTOR_DIMENSIONS).setDefaultUnit("in"));
    EXPECT_EQ(MotorFilterSliders::diameterLabels(), (Labels{"0", ".24", ".51", ".71", ".94", "1.1",
                                                            "1.3", "1.5", "2.1", "3", "3.9", "+"}));
}

/// A thumb sets the stop's actual diameter; the last stop, "+", means no upper limit.
TEST(MotorFilterSliders, GivesTheDiametersOfTheStops)
{
    EXPECT_EQ(MotorFilterSliders::minimumDiameter(0), 0.0);
    EXPECT_EQ(MotorFilterSliders::minimumDiameter(7), 0.0381);
    EXPECT_EQ(MotorFilterSliders::maximumDiameter(4), std::optional(0.024));
    EXPECT_EQ(MotorFilterSliders::maximumDiameter(10), std::optional(0.0983));
    EXPECT_EQ(MotorFilterSliders::maximumDiameter(11), std::nullopt);
    EXPECT_THROW(static_cast<void>(MotorFilterSliders::minimumDiameter(12)), QtRocket::BugError);
}

/// The largest nominal diameter that fits the mount, with 0.5 mm to spare.
TEST(MotorFilterSliders, LimitsTheDiameterToTheMount)
{
    EXPECT_EQ(MotorFilterSliders::limitDiameterStop(0.028), 4);
    EXPECT_EQ(MotorFilterSliders::limitDiameterStop(0.029), 5);
    EXPECT_EQ(MotorFilterSliders::limitDiameterStop(0.0381), 7);
    EXPECT_EQ(MotorFilterSliders::limitDiameterStop(0), 0);
    EXPECT_EQ(MotorFilterSliders::limitDiameterStop(2.0), 10);
    EXPECT_EQ(MotorFilterSliders::limitDiameterStop(std::numeric_limits<double>::quiet_NaN()), 10);
    EXPECT_EQ(MotorFilterSliders::limitDiameterStop(-0.001), -1);
}

/// The mount's length in whole mm, at most 1000.
TEST(MotorFilterSliders, LimitsTheLengthToTheMount)
{
    EXPECT_EQ(MotorFilterSliders::limitLengthStop(0.1), 100);
    EXPECT_EQ(MotorFilterSliders::limitLengthStop(0.0005), 1);
    EXPECT_EQ(MotorFilterSliders::limitLengthStop(0.0004), 0);
    EXPECT_EQ(MotorFilterSliders::limitLengthStop(5), MotorFilterSliders::kLengthSliderMaximum);
    EXPECT_EQ(MotorFilterSliders::limitLengthStop(std::numeric_limits<double>::quiet_NaN()), 0);
}

}  // namespace
