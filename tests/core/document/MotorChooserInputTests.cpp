#include "QtRocket/document/MotorChooserInput.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

// What OpenRocket's ThrustCurveMotorSelectionPanel.setMotorMountAndConfig(fcid, mount) and
// MotorRowFilter.setMotorMount(mount) read of a mount (no JUnit test of their own).

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::Coordinate;
using QtRocket::FlightConfigurationId;
using QtRocket::Manufacturer;
using QtRocket::MotorConfiguration;
using QtRocket::Rocket;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustCurveMotorSelection;

using MotorPtr = std::shared_ptr<const ThrustCurveMotor>;

MotorPtr motor(std::string designation)
{
    QtRocket::Result<ThrustCurveMotor> built =
        ThrustCurveMotor::Builder()
            .setManufacturer(Manufacturer::getManufacturer("Estes"))
            .setDesignation(std::move(designation))
            .setDiameter(0.018)
            .setLength(0.07)
            .setTimePoints({0, 1})
            .setThrustPoints({1, 0})
            .setCGPoints({Coordinate(0.01, 0, 0, 0.01), Coordinate(0.01, 0, 0, 0.01)})
            .build();
    EXPECT_TRUE(built.has_value());
    return built.has_value() ? std::make_shared<const ThrustCurveMotor>(std::move(*built))
                             : nullptr;
}

/// A rocket of one stage whose body tube is a motor mount, with two configurations of their own
/// (a C6 with 5 s and a nozzle of 5 mm, a D12 plugged) and one without an override.
struct MountedRocket
{
    Rocket                rocket;
    BodyTube*             mount{nullptr};
    MotorPtr              c6  = motor("C6");
    MotorPtr              d12 = motor("D12");
    FlightConfigurationId withC6;
    FlightConfigurationId withD12;
    FlightConfigurationId withoutOverride;

    MountedRocket()
    {
        AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
        mount             = &stage.addChild(std::make_unique<BodyTube>());
        mount->setMotorMount(true);
        add(withC6, c6, 5, 0.005);
        add(withD12, d12, ThrustCurveMotor::kPluggedDelay, 0);
        rocket.createFlightConfiguration(withoutOverride);
    }

    void add(const FlightConfigurationId& fcid, MotorPtr motor, double delay, double nozzle)
    {
        rocket.createFlightConfiguration(fcid);
        MotorConfiguration configuration(*mount, fcid);
        configuration.setMotor(std::move(motor));
        configuration.setEjectionDelay(delay);
        EXPECT_TRUE(configuration.setNozzleExitDiameter(nozzle).has_value());
        mount->setMotorConfig(configuration, fcid);
    }
};

/// The configuration's motor, delay and nozzle, and the motors of every configuration with one.
TEST(MotorChooserInput, ReadsTheConfigurationAndTheMountsMotors)
{
    const MountedRocket                                 rocket;
    const ThrustCurveMotorSelection::MountConfiguration configuration =
        QtRocket::MotorChooserInput::fromMount(*rocket.mount, rocket.withC6);
    EXPECT_EQ(configuration.motor, rocket.c6);
    EXPECT_EQ(configuration.ejectionDelay, 5.0);
    EXPECT_EQ(configuration.nozzleExitDiameter, 0.005);
    EXPECT_EQ(configuration.mountMotors, (std::vector<MotorPtr>{rocket.c6, rocket.d12}));
}

/// A configuration without an override is the mount's default, which has no motor here.
TEST(MotorChooserInput, ReadsTheDefaultForAConfigurationWithoutOverride)
{
    const MountedRocket                                 rocket;
    const ThrustCurveMotorSelection::MountConfiguration configuration =
        QtRocket::MotorChooserInput::fromMount(*rocket.mount, rocket.withoutOverride);
    EXPECT_EQ(configuration.motor, nullptr);
    EXPECT_EQ(configuration.mountMotors.size(), 2U);
    const ThrustCurveMotorSelection::MountConfiguration plugged =
        QtRocket::MotorChooserInput::fromMount(*rocket.mount, rocket.withD12);
    EXPECT_EQ(plugged.motor, rocket.d12);
    EXPECT_EQ(plugged.ejectionDelay, ThrustCurveMotor::kPluggedDelay);
}

}  // namespace
