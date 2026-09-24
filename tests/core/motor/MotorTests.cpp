#include "QtRocket/motor/Motor.h"

#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/util/Coordinate.h"

namespace
{

using QtRocket::Coordinate;
using QtRocket::InMemoryPreferences;
using QtRocket::Manufacturer;
using QtRocket::Motor;
using QtRocket::motorTypeFromOrkName;
using QtRocket::ThrustCurveMotor;

/// A small motor whose designation and common name differ.
ThrustCurveMotor buildMotor()
{
    auto built = ThrustCurveMotor::Builder()
                     .setManufacturer(Manufacturer::getManufacturer("Estes"))
                     .setDesignation("C6-5")
                     .setCommonName("C6")
                     .setMotorType(Motor::Type::SINGLE)
                     .setDiameter(0.018)
                     .setLength(0.07)
                     .setTimePoints({0, 0.5, 1.8})
                     .setThrustPoints({0, 14, 0})
                     .setCGPoints({Coordinate(0.035, 0, 0, 0.024), Coordinate(0.035, 0, 0, 0.02),
                                   Coordinate(0.035, 0, 0, 0.012)})
                     .build();
    EXPECT_TRUE(built.has_value()) << built.error().toString();
    return built.value();
}

TEST(Motor, TypeNamesAndDescriptions)
{
    EXPECT_EQ(name(Motor::Type::SINGLE), "Single-use");
    EXPECT_EQ(name(Motor::Type::RELOAD), "Reloadable");
    EXPECT_EQ(name(Motor::Type::HYBRID), "Hybrid");
    EXPECT_EQ(name(Motor::Type::UNKNOWN), "Unknown");

    EXPECT_EQ(description(Motor::Type::SINGLE), "Single-use solid propellant motor");
    EXPECT_EQ(description(Motor::Type::RELOAD), "Reloadable solid propellant motor");
    EXPECT_EQ(description(Motor::Type::HYBRID), "Hybrid rocket motor engine");
    EXPECT_EQ(description(Motor::Type::UNKNOWN), "Unknown motor type");
}

TEST(Motor, TypesAreInDeclarationOrder)
{
    const std::vector<Motor::Type> expected{Motor::Type::SINGLE, Motor::Type::RELOAD,
                                            Motor::Type::HYBRID, Motor::Type::UNKNOWN};
    EXPECT_EQ(std::vector<Motor::Type>(Motor::kAllTypes.begin(), Motor::kAllTypes.end()), expected);
}

TEST(Motor, OrkNamesAreTheLowerCaseConstantNames)
{
    EXPECT_EQ(orkName(Motor::Type::SINGLE), "single");
    EXPECT_EQ(orkName(Motor::Type::RELOAD), "reload");
    EXPECT_EQ(orkName(Motor::Type::HYBRID), "hybrid");
    EXPECT_EQ(orkName(Motor::Type::UNKNOWN), "unknown");

    for (const Motor::Type type : Motor::kAllTypes)
    {
        EXPECT_EQ(motorTypeFromOrkName(orkName(type)), type);
    }
}

TEST(Motor, UnknownOrkNamesGiveNothing)
{
    // MotorHandler compares the trimmed content exactly, so case and spacing matter.
    EXPECT_EQ(motorTypeFromOrkName("Single"), std::nullopt);
    EXPECT_EQ(motorTypeFromOrkName("SINGLE"), std::nullopt);
    EXPECT_EQ(motorTypeFromOrkName(" single"), std::nullopt);
    EXPECT_EQ(motorTypeFromOrkName("Single-use"), std::nullopt);
    EXPECT_EQ(motorTypeFromOrkName(""), std::nullopt);
}

TEST(Motor, Constants)
{
    EXPECT_TRUE(std::isinf(Motor::kPluggedDelay));
    EXPECT_GT(Motor::kPluggedDelay, 0);
    EXPECT_EQ(Motor::kMarginalThrust, 0.05);
    EXPECT_TRUE(std::isnan(Motor::kPseudoTimeEmpty));
    EXPECT_EQ(Motor::kPseudoTimeLaunch, 0.0);
    EXPECT_EQ(Motor::kPseudoTimeBurnout, std::numeric_limits<double>::max());
}

TEST(Motor, MotorNameFollowsThePreference)
{
    const ThrustCurveMotor motor   = buildMotor();
    const Motor&           asMotor = motor;
    InMemoryPreferences    preferences;

    // The designation is the default.
    EXPECT_EQ(asMotor.getMotorName(preferences), "C6-5");
    EXPECT_EQ(asMotor.getMotorName(preferences, 3), "C6-5-3");

    preferences.setMotorNameColumn(false);
    EXPECT_EQ(asMotor.getMotorName(preferences), "C6");
    EXPECT_EQ(asMotor.getMotorName(preferences, 3), "C6-3");
    EXPECT_EQ(asMotor.getMotorName(preferences, Motor::kPluggedDelay), "C6-P");

    preferences.setMotorNameColumn(true);
    EXPECT_EQ(asMotor.getMotorName(preferences, 2.5), "C6-5-2.5");
}

TEST(Motor, InterfaceReachesTheThrustCurve)
{
    const ThrustCurveMotor motor   = buildMotor();
    const Motor&           asMotor = motor;

    EXPECT_EQ(asMotor.getMotorType(), Motor::Type::SINGLE);
    EXPECT_EQ(asMotor.getDesignation(), "C6-5");
    EXPECT_EQ(asMotor.getCommonName(), "C6");
    EXPECT_DOUBLE_EQ(asMotor.getThrust(0.25), 7.0);
    EXPECT_DOUBLE_EQ(asMotor.getTotalMass(0.5), 0.02);
    EXPECT_DOUBLE_EQ(asMotor.getPropellantMass(0.0), 0.012);
    EXPECT_DOUBLE_EQ(asMotor.getCMx(1.0), 0.035);
    EXPECT_DOUBLE_EQ(asMotor.getBurnTime(), 1.8);
    EXPECT_DOUBLE_EQ(asMotor.getLaunchMass(), 0.024);
    EXPECT_DOUBLE_EQ(asMotor.getBurnoutMass(), 0.012);
    EXPECT_DOUBLE_EQ(asMotor.getLaunchCGx(), 0.035);
    EXPECT_DOUBLE_EQ(asMotor.getBurnoutCGx(), 0.035);
    EXPECT_DOUBLE_EQ(asMotor.getUnitIxx(), 0.009 * 0.009 / 2);
    EXPECT_DOUBLE_EQ(asMotor.getUnitIyy(), ((3 * 0.009 * 0.009) + (0.07 * 0.07)) / 12);
    EXPECT_DOUBLE_EQ(asMotor.getUnitIzz(), asMotor.getUnitIyy());
}

}  // namespace
