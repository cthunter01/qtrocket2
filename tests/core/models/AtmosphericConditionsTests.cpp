#include "QtRocket/models/AtmosphericConditions.h"

#include <array>
#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Monitorable.h"

namespace
{

using QtRocket::AtmosphericConditions;
using QtRocket::BugError;
using QtRocket::ModId;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

static_assert(QtRocket::Monitorable<AtmosphericConditions>);

// ---- Ported from AtmosphericConditionsTest.java ----

TEST(AtmosphericConditions, StandardConditions)
{
    const AtmosphericConditions conditions;
    EXPECT_NEAR(conditions.getTemperature(), 293.15, 0.001);
    EXPECT_NEAR(conditions.getPressure(), 101325.0, 0.001);
}

TEST(AtmosphericConditions, DensityCalculation)
{
    // rho = P/(R*T) where R = 287.053
    const AtmosphericConditions conditions;
    const double                expectedDensity = 101325.0 / (287.053 * 293.15);
    EXPECT_NEAR(conditions.getDensity(), expectedDensity, 0.001);
}

TEST(AtmosphericConditions, NegativeTemperatureIsRejected)
{
    AtmosphericConditions conditions;
    EXPECT_THROW(conditions.setTemperature(-1.0), BugError);
}

TEST(AtmosphericConditions, MachSpeed)
{
    struct Case
    {
        double temperature;
        double expectedSpeed;
    };
    constexpr std::array<Case, 3> kCases{{
        {.temperature = 273.15, .expectedSpeed = 331.3},  // 0 degC
        {.temperature = 293.15, .expectedSpeed = 343.2},  // 20 degC (standard)
        {.temperature = 313.15, .expectedSpeed = 355.1},  // 40 degC
    }};
    for (const Case& c : kCases)
    {
        AtmosphericConditions conditions;
        conditions.setTemperature(c.temperature);
        EXPECT_NEAR(conditions.getMachSpeed(), c.expectedSpeed, 1.0) << c.temperature;
    }
}

TEST(AtmosphericConditions, GasConstantOfDryAirIsR)
{
    const AtmosphericConditions conditions(288.15, 101325.0, 0.0);
    EXPECT_EQ(conditions.getGasConstant(), AtmosphericConditions::kR);
}

TEST(AtmosphericConditions, GasConstantIncreasesWithHumidity)
{
    const AtmosphericConditions dry(288.15, 101325.0, 0.0);
    const AtmosphericConditions mid(288.15, 101325.0, 0.5);
    const AtmosphericConditions wet(288.15, 101325.0, 1.0);

    EXPECT_GT(mid.getGasConstant(), dry.getGasConstant());
    EXPECT_GT(wet.getGasConstant(), mid.getGasConstant());
}

TEST(AtmosphericConditions, DensityDecreasesWithHumidity)
{
    const AtmosphericConditions dry(288.15, 101325.0, 0.0);
    const AtmosphericConditions wet(288.15, 101325.0, 1.0);
    EXPECT_LT(wet.getDensity(), dry.getDensity());
}

TEST(AtmosphericConditions, HumidityAffectsEqualityAndHashCode)
{
    const AtmosphericConditions dry(288.15, 101325.0, 0.0);
    const AtmosphericConditions dryCopy(288.15, 101325.0, 0.0);
    const AtmosphericConditions humid(288.15, 101325.0, 1.0);

    EXPECT_TRUE(dry == dryCopy);
    EXPECT_EQ(dry.hashCode(), dryCopy.hashCode());
    EXPECT_FALSE(dry == humid)
        << "Atmospheric conditions with different humidity should not compare equal";
    EXPECT_NE(dry.hashCode(), humid.hashCode())
        << "Relative humidity should contribute to the atmospheric hash code";
}

TEST(AtmosphericConditions, VaporPressureSaturationIncreasesWithTemperature)
{
    const AtmosphericConditions cold(260.0, 101325.0, 0.5);
    const AtmosphericConditions warm(300.0, 101325.0, 0.5);

    EXPECT_GT(cold.vaporPressureSaturation(), 0);
    EXPECT_GT(warm.vaporPressureSaturation(), cold.vaporPressureSaturation());
}

TEST(AtmosphericConditions, RelativeHumidityBounds)
{
    EXPECT_THROW(AtmosphericConditions(288.15, 101325.0, -0.01), BugError);
    EXPECT_THROW(AtmosphericConditions(288.15, 101325.0, 1.01), BugError);
}

// ---- QtRocket additions ----

TEST(AtmosphericConditions, ConstantsMatchOpenRocket)
{
    EXPECT_EQ(AtmosphericConditions::kR, 287.053);
    EXPECT_EQ(AtmosphericConditions::kGamma, 1.4);
    EXPECT_EQ(AtmosphericConditions::kEpsilon, 0.622);
    EXPECT_EQ(AtmosphericConditions::kStandardPressure, 101325.0);
    EXPECT_EQ(AtmosphericConditions::kStandardTemperature, 293.15);
    EXPECT_EQ(AtmosphericConditions::kStandardHumidity, 0.0);
}

TEST(AtmosphericConditions, DerivedValuesMatchOpenRocket)
{
    // Printed by OpenRocket's AtmosphericConditions on JDK 17 (the exp() of a libm may differ
    // from Java's in the last bit, hence the relative tolerance).
    const AtmosphericConditions standard;
    EXPECT_NEAR(standard.getDensity(), 1.2041057320959316, 1e-15);
    EXPECT_DOUBLE_EQ(standard.getMachSpeed(), 343.4189);
    EXPECT_NEAR(standard.getKinematicViscosity(), 1.5256287807901937E-5, 1e-19);

    const AtmosphericConditions half(288.15, 101325.0, 0.5);
    EXPECT_NEAR(half.getGasConstant(), 287.97640317357326, 1e-11);
    EXPECT_NEAR(half.getDensity(), 1.2210714734869563, 1e-14);
    EXPECT_NEAR(half.getKinematicViscosity(), 1.4839805853669027E-5, 1e-18);

    const AtmosphericConditions wet(288.15, 101325.0, 1.0);
    EXPECT_NEAR(wet.getGasConstant(), 288.9057663976301, 1e-11);
    EXPECT_NEAR(wet.getDensity(), 1.2171434836252317, 1e-14);

    const AtmosphericConditions cold(260.0, 101325.0, 0.5);
    EXPECT_NEAR(cold.vaporPressureSaturation(), 224.05588992851187, 1e-10);
}

TEST(AtmosphericConditions, TwoArgumentConstructorIsDry)
{
    const AtmosphericConditions conditions(250.0, 50000.0);
    EXPECT_EQ(conditions.getTemperature(), 250.0);
    EXPECT_EQ(conditions.getPressure(), 50000.0);
    EXPECT_EQ(conditions.getRelativeHumidity(), 0.0);
    EXPECT_EQ(conditions.getGasConstant(), AtmosphericConditions::kR);
}

TEST(AtmosphericConditions, SettersRejectNonPositiveValues)
{
    AtmosphericConditions conditions;
    EXPECT_THROW(conditions.setTemperature(0.0), BugError);
    EXPECT_THROW(conditions.setPressure(0.0), BugError);
    EXPECT_THROW(conditions.setPressure(-5.0), BugError);
    EXPECT_THROW(conditions.setRelativeHumidity(-1e-9), BugError);
    EXPECT_THROW(conditions.setRelativeHumidity(1.0000001), BugError);
    EXPECT_THROW(AtmosphericConditions(0.0, 101325.0), BugError);
    EXPECT_THROW(AtmosphericConditions(288.15, 0.0), BugError);

    // A rejected value leaves the conditions as they were.
    EXPECT_EQ(conditions.getTemperature(), AtmosphericConditions::kStandardTemperature);
    EXPECT_EQ(conditions.getPressure(), AtmosphericConditions::kStandardPressure);
    EXPECT_EQ(conditions.getRelativeHumidity(), 0.0);

    // The bounds of the humidity are allowed.
    conditions.setRelativeHumidity(0.0);
    conditions.setRelativeHumidity(1.0);
    EXPECT_EQ(conditions.getRelativeHumidity(), 1.0);
}

TEST(AtmosphericConditions, NaNPassesTheChecksAsInJava)
{
    AtmosphericConditions conditions;
    EXPECT_NO_THROW(conditions.setTemperature(kNaN));
    EXPECT_NO_THROW(conditions.setPressure(kNaN));
    EXPECT_NO_THROW(conditions.setRelativeHumidity(kNaN));
    EXPECT_TRUE(std::isnan(conditions.getDensity()));
    // A NaN humidity is not positive: the gas constant is that of dry air.
    EXPECT_EQ(conditions.getGasConstant(), AtmosphericConditions::kR);
    const AtmosphericConditions copy = conditions;
    EXPECT_FALSE(conditions == copy);  // MathUtil::equals is never true for NaN
    // Java's equals() starts with this == other, so the object itself still equals itself.
    const AtmosphericConditions& same = conditions;
    EXPECT_TRUE(conditions == same);
    EXPECT_FALSE(conditions != same);
}

TEST(AtmosphericConditions, ValidateReportsTheConstructorsChecks)
{
    EXPECT_TRUE(AtmosphericConditions::validate(288.15, 101325.0, 0.5).has_value());
    EXPECT_TRUE(AtmosphericConditions::validate(kNaN, kNaN, kNaN).has_value());  // as in Java
    EXPECT_TRUE(AtmosphericConditions::validate(1e-300, 5e-324, 1.0).has_value());

    const auto temperature = AtmosphericConditions::validate(0.0, 101325.0, 0.0);
    ASSERT_FALSE(temperature.has_value());
    EXPECT_EQ(temperature.error().code, QtRocket::ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(temperature.error().message, "Temperature must be positive (Kelvin)");

    const auto pressure = AtmosphericConditions::validate(288.15, -0.0, 0.0);
    ASSERT_FALSE(pressure.has_value());
    EXPECT_EQ(pressure.error().message, "Pressure must be positive (Pascals)");

    const auto humidity = AtmosphericConditions::validate(288.15, 101325.0, -0.1);
    ASSERT_FALSE(humidity.has_value());
    EXPECT_EQ(humidity.error().message, "Humidity must be between 0 and 1");

    // The constructor's order: the temperature first, then the pressure, then the humidity.
    const auto allBad = AtmosphericConditions::validate(-1.0, -1.0, 2.0);
    ASSERT_FALSE(allBad.has_value());
    EXPECT_EQ(allBad.error().message, "Temperature must be positive (Kelvin)");
    const auto badPressureAndHumidity = AtmosphericConditions::validate(1.0, -1.0, 2.0);
    ASSERT_FALSE(badPressureAndHumidity.has_value());
    EXPECT_EQ(badPressureAndHumidity.error().message, "Pressure must be positive (Pascals)");

    // The constructor throws exactly where validate() fails.
    EXPECT_THROW(AtmosphericConditions(288.15, 101325.0, 1.5), BugError);
    EXPECT_FALSE(AtmosphericConditions::validate(288.15, 101325.0, 1.5).has_value());
}

TEST(AtmosphericConditions, EveryChangeDrawsANewModId)
{
    AtmosphericConditions conditions;
    const ModId           first = conditions.modId();
    conditions.setTemperature(280.0);
    const ModId second = conditions.modId();
    EXPECT_GT(second, first);
    conditions.setPressure(90000.0);
    const ModId third = conditions.modId();
    EXPECT_GT(third, second);
    conditions.setRelativeHumidity(0.3);
    EXPECT_GT(conditions.modId(), third);

    // A copy is Java's clone(): the same values and the same id.
    const AtmosphericConditions copy = conditions;
    EXPECT_EQ(copy.modId(), conditions.modId());
    EXPECT_TRUE(copy == conditions);

    // A rejected value keeps the id.
    const ModId before = conditions.modId();
    EXPECT_THROW(conditions.setPressure(-1.0), BugError);
    EXPECT_EQ(conditions.modId(), before);
}

TEST(AtmosphericConditions, EqualityIsTolerant)
{
    const AtmosphericConditions a(288.15, 101325.0, 0.5);
    const AtmosphericConditions b(288.15 * (1 + 1e-10), 101325.0 * (1 - 1e-10), 0.5);
    const AtmosphericConditions c(288.16, 101325.0, 0.5);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_FALSE(a == AtmosphericConditions(288.15, 101326.0, 0.5));
}

TEST(AtmosphericConditions, HashCodeMatchesOpenRocket)
{
    // (int)(pressure + temperature * 1000 + humidity * 1000000), printed by OpenRocket.
    EXPECT_EQ(AtmosphericConditions(288.15, 101325.0, 0.0).hashCode(), 389475);
    EXPECT_EQ(AtmosphericConditions(288.15, 101325.0, 1.0).hashCode(), 1389475);
    EXPECT_EQ(AtmosphericConditions().hashCode(), 394475);
    // Java's (int) saturates and maps NaN to 0.
    AtmosphericConditions huge(1e300, 1e300);
    EXPECT_EQ(huge.hashCode(), std::numeric_limits<int>::max());
    huge.setPressure(kNaN);
    EXPECT_EQ(huge.hashCode(), 0);
}

TEST(AtmosphericConditions, ToStringMatchesOpenRocket)
{
    EXPECT_EQ(AtmosphericConditions(288.15, 101325.0, 0.0).toString(),
              "AtmosphericConditions[T=288.15,P=101325.00]");
    EXPECT_EQ(AtmosphericConditions().toString(), "AtmosphericConditions[T=293.15,P=101325.00]");
    // Half-up rounding of the decimal digits, as Java's Formatter.
    EXPECT_EQ(AtmosphericConditions(216.645, 0.125).toString(),
              "AtmosphericConditions[T=216.65,P=0.13]");
}

}  // namespace
