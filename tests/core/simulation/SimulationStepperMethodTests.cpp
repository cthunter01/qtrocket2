#include "QtRocket/simulation/SimulationStepperMethod.h"

#include <optional>

#include <gtest/gtest.h>

#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"

namespace
{

using QtRocket::getDescription;
using QtRocket::getName;
using QtRocket::getShortName;
using QtRocket::kAllSimulationStepperMethods;
using QtRocket::SimulationStepperMethod;
using QtRocket::simulationStepperMethodFromName;
using QtRocket::simulationStepperMethodFromString;
using QtRocket::simulationStepperMethodName;

// OpenRocket has no SimulationStepperMethodTest. The texts are what OptionsProbe prints for each
// constant: name() | getName() | getShortName() | getDescription() | toString().

TEST(SimulationStepperMethod, ValuesInDeclarationOrder)
{
    ASSERT_EQ(kAllSimulationStepperMethods.size(), 2U);
    EXPECT_EQ(kAllSimulationStepperMethods[0], SimulationStepperMethod::RK4);
    EXPECT_EQ(kAllSimulationStepperMethods[1], SimulationStepperMethod::RK6);
}

TEST(SimulationStepperMethod, Names)
{
    EXPECT_EQ(getName(SimulationStepperMethod::RK4), "6-DOF Runge-Kutta 4");
    EXPECT_EQ(getName(SimulationStepperMethod::RK6), "6-DOF Runge-Kutta 6");
}

TEST(SimulationStepperMethod, ShortNames)
{
    EXPECT_EQ(getShortName(SimulationStepperMethod::RK4), "RK4");
    EXPECT_EQ(getShortName(SimulationStepperMethod::RK6), "RK6");
}

TEST(SimulationStepperMethod, Descriptions)
{
    EXPECT_EQ(getDescription(SimulationStepperMethod::RK4), "6-DOF Runge-Kutta 4");
    EXPECT_EQ(getDescription(SimulationStepperMethod::RK6),
              "6-DOF Runge-Kutta 6: Slower than RK4, but more accurate in some cases");
}

TEST(SimulationStepperMethod, EnumNamesAndOrkSpellings)
{
    EXPECT_EQ(simulationStepperMethodName(SimulationStepperMethod::RK4), "RK4");
    EXPECT_EQ(simulationStepperMethodName(SimulationStepperMethod::RK6), "RK6");
    // OpenRocketSaver: name().toLowerCase(Locale.ENGLISH)
    EXPECT_EQ(toString(SimulationStepperMethod::RK4), "rk4");
    EXPECT_EQ(toString(SimulationStepperMethod::RK6), "rk6");
}

TEST(SimulationStepperMethod, FromStringReadsTheOrkSpelling)
{
    // DocumentConfig.findEnum(): trimmed, then exactly the lower-cased constant name.
    EXPECT_EQ(simulationStepperMethodFromString("rk4"), SimulationStepperMethod::RK4);
    EXPECT_EQ(simulationStepperMethodFromString("rk6"), SimulationStepperMethod::RK6);
    EXPECT_EQ(simulationStepperMethodFromString("  rk6\n"), SimulationStepperMethod::RK6);
    EXPECT_EQ(simulationStepperMethodFromString("RK4"), std::nullopt);
    EXPECT_EQ(simulationStepperMethodFromString("Rk6"), std::nullopt);
    EXPECT_EQ(simulationStepperMethodFromString("rk5"), std::nullopt);
    EXPECT_EQ(simulationStepperMethodFromString("6-DOF Runge-Kutta 4"), std::nullopt);
    EXPECT_EQ(simulationStepperMethodFromString(""), std::nullopt);
}

TEST(SimulationStepperMethod, EveryOrkSpellingReadsBack)
{
    EXPECT_EQ(simulationStepperMethodFromString(toString(SimulationStepperMethod::RK4)),
              SimulationStepperMethod::RK4);
    EXPECT_EQ(simulationStepperMethodFromString(toString(SimulationStepperMethod::RK6)),
              SimulationStepperMethod::RK6);
}

TEST(SimulationStepperMethod, FromNameIsEnumValueOf)
{
    EXPECT_EQ(simulationStepperMethodFromName("RK4"), SimulationStepperMethod::RK4);
    EXPECT_EQ(simulationStepperMethodFromName("RK6"), SimulationStepperMethod::RK6);
    EXPECT_EQ(simulationStepperMethodFromName("rk4"), std::nullopt);
    EXPECT_EQ(simulationStepperMethodFromName(" RK4"), std::nullopt);
    EXPECT_EQ(simulationStepperMethodFromName(""), std::nullopt);
}

TEST(SimulationStepperMethod, TheConstantNamesAreThePreferenceSpellings)
{
    // Preferences keeps the choice by its constant name and only takes these names.
    ASSERT_EQ(QtRocket::Preferences::kSimulationStepperMethodNames.size(), 2U);
    EXPECT_EQ(QtRocket::Preferences::kSimulationStepperMethodNames[0],
              simulationStepperMethodName(SimulationStepperMethod::RK4));
    EXPECT_EQ(QtRocket::Preferences::kSimulationStepperMethodNames[1],
              simulationStepperMethodName(SimulationStepperMethod::RK6));

    QtRocket::InMemoryPreferences preferences;
    EXPECT_EQ(simulationStepperMethodFromName(preferences.getSimulationStepperMethodName()),
              SimulationStepperMethod::RK4);
    preferences.setSimulationStepperMethodName(
        simulationStepperMethodName(SimulationStepperMethod::RK6));
    EXPECT_EQ(simulationStepperMethodFromName(preferences.getSimulationStepperMethodName()),
              SimulationStepperMethod::RK6);
}

}  // namespace
