#include "QtRocket/simulation/BasicLandingStepper.h"

#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

#include <gtest/gtest.h>

#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/AbstractEulerStepper.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "rocket/JavaValueDifferences.h"
#include "simulation/SimulationTestSupport.h"
#include "simulation/StepperTablePins.h"
#include "simulation/StepperTestSupport.h"

namespace
{

using QtRocket::AbstractEulerStepper;
using QtRocket::BasicLandingStepper;
using QtRocket::FlightConfiguration;
using QtRocket::Rocket;
using QtRocket::SimulationConditions;
using QtRocket::SimulationStatus;
using QtRocket::Test::bugText;
using QtRocket::Test::deployRecoveryDevices;
using QtRocket::Test::makeScenarioRocket;
using QtRocket::Test::matchesPinnedValue;
using QtRocket::Test::scenarioConfiguration;
using QtRocket::Test::StepperPins::DragCoefficientPin;
using QtRocket::Test::StepperPins::kDragCoefficientPins;

static_assert(std::is_base_of_v<AbstractEulerStepper, BasicLandingStepper>);
static_assert(!std::is_abstract_v<BasicLandingStepper>);
static_assert(!std::is_copy_assignable_v<BasicLandingStepper>);

// OpenRocket has no test of BasicLandingStepper. The expectations are what
// probes/tier8b-steppers/StepperTableProbe.java printed ("computeCD"); the steps of the stepper
// are pinned in AbstractEulerStepperTests.cpp.

/// A status of a test rocket on default conditions, as the probe makes one: a clone of the
/// configuration the scenarios use, only one stage active when @p onlyStage is not negative,
/// and every recovery device of the active stages deployed.
struct DeployedStatus
{
    std::unique_ptr<Rocket>           rocket;
    std::unique_ptr<SimulationStatus> status;

    DeployedStatus(std::string_view maker, int onlyStage) : rocket(makeScenarioRocket(maker))
    {
        status = std::make_unique<SimulationStatus>(
            std::make_shared<FlightConfiguration>(scenarioConfiguration(*rocket).clone()),
            std::make_shared<SimulationConditions>());
        if (onlyStage >= 0)
        {
            status->getConfiguration().setOnlyStage(onlyStage);
        }
        deployRecoveryDevices(*status);
    }
};

/// The rows of the drag coefficient table whose landing CD (or reference area) is not Java's,
/// within 1e-12: the devices are summed in another order than Java's hash order.
[[nodiscard]] std::string landingDifferences()
{
    std::string differences;
    for (const DragCoefficientPin& pin : kDragCoefficientPins)
    {
        const DeployedStatus f(pin.maker, pin.onlyStage);
        BasicLandingStepper  stepper;
        const double         cd   = stepper.computeCD(*f.status);
        const double         area = f.status->getConfiguration().getReferenceArea();
        if (!matchesPinnedValue(pin.landing, cd) || !matchesPinnedValue(pin.referenceArea, area))
        {
            differences +=
                std::format("  {} stage {}: expected {} (area {}), got {} (area {})\n", pin.maker,
                            pin.onlyStage, pin.landing, pin.referenceArea, cd, area);
        }
    }
    return differences;
}

// The thirteen test rockets, whole and stage by stage: the number of active instances of each
// deployed device * its CD * its area / the reference area (0 without a recovery device).
TEST(BasicLandingStepper, TheDragCoefficientIsJavasForEveryTestRocket)
{
    EXPECT_EQ(kDragCoefficientPins.size(), 25U);
    EXPECT_EQ(landingDifferences(), "");
}

// The Alpha III: one parachute of CD 0.8 and area 0.0707 m^2 on a reference area of
// 4.52 cm^2.
TEST(BasicLandingStepper, AParachuteOnAThinRocketHasALargeCoefficient)
{
    const DeployedStatus f("makeEstesAlphaIII", -1);
    BasicLandingStepper  stepper;
    ASSERT_EQ(f.status->getDeployedRecoveryDevices().size(), 1U);
    EXPECT_TRUE(matchesPinnedValue(125.00000000000001, stepper.computeCD(*f.status)));
}

TEST(BasicLandingStepper, NothingDeployedMeansNoDrag)
{
    const DeployedStatus f("makeEstesAlphaIII", -1);
    f.status->getDeployedRecoveryDevices().clear();
    BasicLandingStepper stepper;
    EXPECT_EQ(stepper.computeCD(*f.status), 0.0);
}

// StepperTableProbe: "falcon only stage 1, devices still in the set: landing 0.0". A device
// whose stage is no longer among the active instances counts zero times.
TEST(BasicLandingStepper, ADeviceOfAStageThatIsNotActiveDoesNotCount)
{
    const DeployedStatus f("makeFalcon9Heavy", -1);
    BasicLandingStepper  stepper;
    ASSERT_EQ(f.status->getDeployedRecoveryDevices().size(), 1U);
    EXPECT_TRUE(matchesPinnedValue(6.656804733727811, stepper.computeCD(*f.status)));

    f.status->getConfiguration().setOnlyStage(1);
    EXPECT_EQ(f.status->getDeployedRecoveryDevices().size(), 1U);
    EXPECT_EQ(stepper.computeCD(*f.status), 0.0);
    f.status->getConfiguration().setOnlyStage(2);
    EXPECT_EQ(stepper.computeCD(*f.status), 0.0);
    f.status->getConfiguration().setOnlyStage(0);
    EXPECT_TRUE(matchesPinnedValue(6.656804733727811, stepper.computeCD(*f.status)));
}

TEST(BasicLandingStepper, ANullDeviceIsABug)
{
    const DeployedStatus f("makeEstesAlphaIII", -1);
    f.status->getDeployedRecoveryDevices().add(nullptr);
    BasicLandingStepper stepper;
    // Java: a NullPointerException.
    EXPECT_EQ(bugText([&] { static_cast<void>(stepper.computeCD(*f.status)); }),
              "The set of deployed recovery devices holds a null device");
}

}  // namespace
