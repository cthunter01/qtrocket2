#include "QtRocket/unit/CaliberUnit.h"

#include <cmath>
#include <functional>
#include <limits>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::BugError;
using QtRocket::CaliberUnit;

// ---- QtRocket additions ----

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
}

TEST(CaliberUnit, ProviderValuesAreNotChecked)
{
    // A provider does not floor or check what it returns (the rocket model applies OpenRocket's
    // DEFAULT_CALIBER floor): a zero reference divides to an infinity and NaN stays NaN, as Java
    // divides.
    double            reference = 0.0;
    const CaliberUnit cal([&reference] { return reference; });
    EXPECT_EQ(cal.toUnit(0.2), std::numeric_limits<double>::infinity());
    EXPECT_EQ(cal.fromUnit(0.5), 0.0);
    reference = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(std::isnan(cal.toUnit(0.2)));
}

TEST(CaliberUnit, NaNReferenceIsAcceptedAsInJava)
{
    // CaliberUnit(double) rejects `reference <= 0`, which is false for NaN, so Java accepts it.
    const CaliberUnit cal(std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(cal.hasReference());
    EXPECT_TRUE(std::isnan(cal.getReferenceLength()));
    EXPECT_TRUE(std::isnan(cal.toUnit(0.1)));
    EXPECT_TRUE(std::isnan(cal.fromUnit(2.0)));
    EXPECT_EQ(cal.toString(0.1), "NaN");  // only a NaN SI value is "N/A"
    EXPECT_EQ(cal.toStringUnit(0.125), "NaN cal");
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

}  // namespace
