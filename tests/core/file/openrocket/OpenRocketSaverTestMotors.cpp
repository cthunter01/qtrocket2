#include "file/openrocket/OpenRocketSaverTestMotors.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/motor/GeneralMotorLoader.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"

namespace QtRocket::Test
{

std::shared_ptr<const ThrustCurveMotor> readEstesA8Motor()
{
    const Result<std::vector<std::byte>> a8 = readFile(testDataDir() / "motors" / "Estes_A8.rse");
    if (!a8.has_value())
    {
        ADD_FAILURE() << "Problem in unit test, cannot find Estes_A8.rse";
        return nullptr;
    }
    const Result<std::vector<ThrustCurveMotor::Builder>> builders =
        GeneralMotorLoader().load(*a8, "Estes_A8.rse");
    if (!builders.has_value() || builders->empty())
    {
        ADD_FAILURE() << "Could not load motor";
        return nullptr;
    }
    Result<ThrustCurveMotor> motor = builders->front().build();
    if (!motor.has_value())
    {
        ADD_FAILURE() << motor.error().toString();
        return nullptr;
    }
    return std::make_shared<const ThrustCurveMotor>(std::move(*motor));
}

std::unique_ptr<ThrustCurveMotorSetDatabase> openRocketSaverTestMotors()
{
    auto database = std::make_unique<ThrustCurveMotorSetDatabase>();
    if (std::shared_ptr<const ThrustCurveMotor> a8 = readEstesA8Motor())
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
