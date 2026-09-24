#include "QtRocket/unit/PercentageOfLengthUnit.h"

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::BugError;
using QtRocket::PercentageOfLengthUnit;

// ---- QtRocket additions ----

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
    reference = 0.0;
    EXPECT_EQ(dynamic.toUnit(0.5), std::numeric_limits<double>::infinity());
}

TEST(PercentageOfLengthUnit, NaNReferenceIsAcceptedAsInJava)
{
    // `reference <= 0` is false for NaN, so Java accepts it and every conversion gives NaN.
    const PercentageOfLengthUnit percent(std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(percent.hasReference());
    EXPECT_TRUE(std::isnan(percent.toUnit(0.5)));
    EXPECT_TRUE(std::isnan(percent.fromUnit(25.0)));
    EXPECT_EQ(percent.toString(0.5), "NaN");
    EXPECT_EQ(percent.toStringUnit(0.5), "NaN %");
}

}  // namespace
