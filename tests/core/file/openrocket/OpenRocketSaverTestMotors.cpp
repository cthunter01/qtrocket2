#include "file/openrocket/OpenRocketSaverTestMotors.h"

#include <memory>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "motor/TestMotorDatabase.h"

namespace QtRocket::Test
{

std::unique_ptr<ThrustCurveMotorSetDatabase> openRocketSaverTestMotors()
{
    auto database = std::make_unique<ThrustCurveMotorSetDatabase>();
    if (std::shared_ptr<const ThrustCurveMotor> a8 = estesA8Motor())
    {
        database->addMotor(std::move(a8));
    }
    Result<ThrustCurveMotor> f12x =
        ThrustCurveMotor::Builder()
            .setManufacturer(Manufacturer::getManufacturer("A"))
            .setDesignation("F12X")
            .setDescription("Desc")
            .setMotorType(Motor::Type::UNKNOWN)
            .setStandardDelays({})
            .setDiameter(0.024)
            .setLength(0.07)
            .setTimePoints({0, 1, 2})
            .setThrustPoints({0, 1, 0})
            .setCGPoints({Coordinate::kNul, Coordinate::kNul, Coordinate::kNul})
            .setDigest("digestA")
            .build();
    EXPECT_TRUE(f12x.has_value());
    if (f12x.has_value())
    {
        database->addMotor(std::make_shared<const ThrustCurveMotor>(std::move(*f12x)));
    }
    EXPECT_EQ(database->getMotorSets().size(), 2U);
    return database;
}

}  // namespace QtRocket::Test
