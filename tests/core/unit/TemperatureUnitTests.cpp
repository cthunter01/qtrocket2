#include "QtRocket/unit/TemperatureUnit.h"

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

namespace
{

using QtRocket::TemperatureUnit;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// ---- QtRocket additions ----

TEST(TemperatureUnit, CelsiusAndFahrenheit)
{
    const TemperatureUnit celsius(1, 273.15, 0.01, "°C");
    const TemperatureUnit fahrenheit(5.0 / 9.0, 459.67, 0.01, "°F");
    EXPECT_FALSE(celsius.hasSpace());
    EXPECT_EQ(celsius.getUnit(), "°C");
    EXPECT_EQ(celsius.getAddition(), 273.15);
    EXPECT_EQ(fahrenheit.getPrecision(), 0.01);

    EXPECT_DOUBLE_EQ(celsius.toUnit(273.15), 0.0);
    EXPECT_DOUBLE_EQ(celsius.fromUnit(273.15), 546.3);
    EXPECT_EQ(celsius.toString(273.15), "0.00");
    EXPECT_EQ(celsius.toStringUnit(273.15), "0.00°C");
    EXPECT_DOUBLE_EQ(fahrenheit.toUnit(273.15), 31.999999999999943);
    EXPECT_DOUBLE_EQ(fahrenheit.fromUnit(273.15), 407.1222222222222);
    EXPECT_EQ(fahrenheit.toString(273.15), "32.00");
    EXPECT_EQ(fahrenheit.toStringUnit(273.15), "32.00°F");
    EXPECT_DOUBLE_EQ(celsius.toUnit(283.15), 10.0);
    EXPECT_EQ(celsius.toStringUnit(283.15), "10.00°C");
    EXPECT_DOUBLE_EQ(fahrenheit.toUnit(283.15), 49.99999999999994);
    EXPECT_EQ(fahrenheit.toStringUnit(283.15), "50.00°F");
    EXPECT_DOUBLE_EQ(celsius.toUnit(0.0), -273.15);
    EXPECT_DOUBLE_EQ(celsius.fromUnit(0.0), 273.15);
    EXPECT_EQ(celsius.toStringUnit(0.0), "-273.15°C");
    EXPECT_DOUBLE_EQ(fahrenheit.toUnit(0.0), -459.67);
    EXPECT_DOUBLE_EQ(fahrenheit.fromUnit(0.0), 255.37222222222223);
    EXPECT_EQ(fahrenheit.toStringUnit(0.0), "-459.67°F");
    EXPECT_DOUBLE_EQ(celsius.toUnit(300.0), 26.850000000000023);
    EXPECT_EQ(celsius.toString(300.0), "26.85");
    EXPECT_DOUBLE_EQ(fahrenheit.toUnit(300.0), 80.32999999999998);
    EXPECT_EQ(fahrenheit.toString(300.0), "80.33");
    EXPECT_TRUE(std::isnan(celsius.toUnit(kNaN)));
    EXPECT_EQ(celsius.toString(kNaN), "NaN");
    EXPECT_EQ(celsius.toStringUnit(kNaN), "N/A");
    EXPECT_EQ(fahrenheit.toString(283.153), "50.01");
    EXPECT_EQ(fahrenheit.toStringUnit(310.928), "100.00°F");
    EXPECT_EQ(celsius.toStringUnit(310.928), "37.78°C");
    EXPECT_EQ(celsius.toStringUnit(-1.0), "-274.15°C");
    EXPECT_EQ(fahrenheit.toStringUnit(-1.0), "-461.47°F");
    EXPECT_DOUBLE_EQ(fahrenheit.fromUnit(-1.0), 254.8166666666667);
}

TEST(TemperatureUnit, RoundingTiesAreJavasMathRound)
{
    const TemperatureUnit celsius(1, 273.15, 0.01, "C");
    const TemperatureUnit fahrenheit(5.0 / 9.0, 459.67, 0.01, "F");
    EXPECT_EQ(celsius.round(0.005), 0.01);
    EXPECT_EQ(celsius.round(-0.005), 0.0);
    EXPECT_FALSE(std::signbit(celsius.round(-0.005)));
    EXPECT_EQ(celsius.round(1.005), 1.0);
    EXPECT_EQ(celsius.round(-2.675), -2.67);
    EXPECT_EQ(celsius.getNextValue(0.005), 0.02);
    EXPECT_EQ(celsius.getPreviousValue(0.005), 0.0);
    EXPECT_EQ(celsius.getNextValue(-1.015), -1.0);
    EXPECT_EQ(celsius.getPreviousValue(-1.015), -1.02);
    EXPECT_EQ(fahrenheit.round(98.605), 98.61);
    EXPECT_EQ(fahrenheit.getNextValue(-40.005), -40.0);
}

}  // namespace
