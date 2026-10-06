#include "QtRocket/simulation/SimulationStatus.h"

#include <algorithm>
#include <any>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/mass/ThrustState.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/EventQueue.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Monitorable.h"
#include "QtRocket/util/Quaternion.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/WorldCoordinate.h"
#include "rocket/JavaValueDifferences.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationOptionsSupport.h"
#include "simulation/SimulationStatusSupport.h"
#include "simulation/SimulationTestSupport.h"

namespace
{

using QtRocket::CloneableSimulationListener;
using QtRocket::Coordinate;
using QtRocket::EventQueue;
using QtRocket::FlightConfiguration;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::FlightEvent;
using QtRocket::ModId;
using QtRocket::MotorClusterState;
using QtRocket::Quaternion;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::SimulationAbort;
using QtRocket::SimulationConditions;
using QtRocket::SimulationListener;
using QtRocket::SimulationStatus;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::WorldCoordinate;
using QtRocket::Test::bugText;
using QtRocket::Test::isJavaQuaternion;
using QtRocket::Test::isJavaValue;
using QtRocket::Test::JavaValueDifferences;
using QtRocket::Test::ModIdWatch;
using QtRocket::Test::newBranch;
using QtRocket::Test::statusConfiguration;
using QtRocket::Test::TestClusterPods;
using QtRocket::Test::testFcid;
using QtRocket::Test::TestMultiStageEventTestRocket;
using QtRocket::Test::TestRocketMaker;
using QtRocket::Test::testRocketMakers;
using QtRocket::Test::TestStatus;

constexpr double kInf = std::numeric_limits<double>::infinity();

// The status can be watched as every Monitorable. A copy is Java's copy constructor, made on
// purpose; a status is handed over by moving it.
static_assert(QtRocket::Monitorable<SimulationStatus>);
static_assert(!std::is_convertible_v<const SimulationStatus&, SimulationStatus>);
static_assert(std::is_constructible_v<SimulationStatus, const SimulationStatus&>);
static_assert(!std::is_copy_assignable_v<SimulationStatus>);
static_assert(std::is_nothrow_move_constructible_v<SimulationStatus>);
static_assert(std::is_nothrow_move_assignable_v<SimulationStatus>);

// OpenRocket has no test of SimulationStatus itself. The expectations are what
// probes/tier8b-status/StatusProbe.java printed (package info.openrocket.core.simulation),
// quoted where they are used.

// ============================================================================ test support

/// One of TestRockets' rockets under the name the probe gives it, and the configuration the
/// probe simulates: the first test configuration for the Alpha III and its variants, the
/// selected configuration for the others (StatusProbe.make() and configOf()).
struct ProbeRocket
{
    std::unique_ptr<Rocket> rocket;
    FlightConfiguration*    config{nullptr};

    explicit ProbeRocket(std::string_view name)
    {
        static constexpr std::array<std::pair<std::string_view, std::string_view>, 9> kMethods{{
            {"alpha", "makeEstesAlphaIII"},
            {"beta", "makeBeta"},
            {"bigblue", "makeBigBlue"},
            {"isohaisu", "makeIsoHaisu"},
            {"falcon", "makeFalcon9Heavy"},
            {"multistage", "makeMultiStageEventTestRocket"},
            {"motorpods", "makeEstesAlphaIIIWithMotorPods"},
            {"secondmotor", "makeEstesAlphaIIIWithSecondMotor"},
            {"clusterpods", "makeClusterPods"},
        }};
        std::string_view                                                              method;
        for (const auto& [probeName, makerMethod] : kMethods)
        {
            if (probeName == name)
            {
                method = makerMethod;
            }
        }
        for (const TestRocketMaker& maker : testRocketMakers())
        {
            if (maker.method == method)
            {
                rocket = maker.make();
            }
        }
        if (rocket == nullptr)
        {
            ADD_FAILURE() << "no test rocket " << name;
            rocket = std::make_unique<Rocket>();
        }
        const bool alphaVariant = name == "alpha" || name == "motorpods" || name == "secondmotor";
        config                  = alphaVariant ? &rocket->getFlightConfiguration(testFcid(0))
                                               : &rocket->getSelectedConfiguration();
    }

    /// A status of the probe's configuration on @p conditions (a clone of the configuration, as
    /// a status holds one).
    [[nodiscard]] SimulationStatus status(std::shared_ptr<SimulationConditions> conditions) const
    {
        return {std::make_shared<FlightConfiguration>(config->clone()), std::move(conditions)};
    }

    /// The same on default conditions (Java: new SimulationStatus(config, new
    /// SimulationConditions())).
    [[nodiscard]] SimulationStatus status() const
    {
        return status(std::make_shared<SimulationConditions>());
    }
};

/// The component of @p rocket named @p name.
[[nodiscard]] const RocketComponent* componentNamed(const FlightConfiguration& config,
                                                    std::string_view           name)
{
    for (const RocketComponent* component : config.getAllComponents())
    {
        if (component->getName() == name)
        {
            return component;
        }
    }
    return nullptr;
}

/// The events of @p queue in array order, as the probe prints them: "<TYPE>@<time>:<source
/// name or -> ...".
[[nodiscard]] std::string queueText(const EventQueue& queue)
{
    std::string line;
    for (const FlightEvent& event : queue)
    {
        if (!line.empty())
        {
            line += ' ';
        }
        line += name(event.getType());
        line += '@';
        line += QtRocket::Strings::javaDoubleToString(event.getTime());
        line += ':';
        line += event.getSource() == nullptr ? std::string{"-"} : event.getSource()->getName();
    }
    return line;
}

// ============================================================================ a new status

/// One row of the probe's "orientation" table.
struct OrientationPin
{
    constexpr OrientationPin(std::string_view rocketName, double length, double angle,
                             double direction, double qw, double qx, double qy, double qz,
                             double effectiveLength) noexcept
      : rocket(rocketName),
        rodLength(length),
        rodAngle(angle),
        rodDirection(direction),
        w(qw),
        x(qx),
        y(qy),
        z(qz),
        effectiveRodLength(effectiveLength)
    {
    }

    std::string_view rocket;
    double           rodLength;
    double           rodAngle;
    double           rodDirection;
    double           w;
    double           x;
    double           y;
    double           z;
    double           effectiveRodLength;
};

/// Eight rockets on six launch rods each (the rod length, the rod angle and the rod direction;
/// Java's orientation w, x, y, z; Java's effective launch rod length).
constexpr std::array<OrientationPin, 48> kOrientations{{
    {"alpha", 1.0, 0.0, 0.0, 1.0000000000000002, 0.0, 0.0, 0.0, 0.961},
    {"alpha", 1.5, 0.1, 0.0, 0.9987502603949664, -0.04997916927067834, 0.0, 0.0,
     1.4609999999999999},
    {"alpha", 2.0, 0.2, 1.5707963267948966, 0.9950041652780258, 0.0, 0.09983341664682815, 0.0,
     1.9609999999999999},
    {"alpha", 0.5, -0.3, 2.5, 0.9887710779360422, -0.11972140575048093, -0.08943455955236601, 0.0,
     0.46099999999999997},
    {"alpha", 1.0, 1.0471975511965976, 4.0, 0.8660254037844385, 0.3268218104318058,
     -0.378401247653964, 0.0, 0.961},
    {"alpha", 0.02, 0.05, 6.0, 0.9996875162757026, -0.02400175680094203, -0.006984659833185136, 0.0,
     0.0},
    {"beta", 1.0, 0.0, 0.0, 1.0000000000000002, 0.0, 0.0, 0.0, 0.985},
    {"beta", 1.5, 0.1, 0.0, 0.9987502603949664, -0.04997916927067834, 0.0, 0.0, 1.4849999999999999},
    {"beta", 2.0, 0.2, 1.5707963267948966, 0.9950041652780258, 0.0, 0.09983341664682815, 0.0,
     1.9849999999999999},
    {"beta", 0.5, -0.3, 2.5, 0.9887710779360422, -0.11972140575048093, -0.08943455955236601, 0.0,
     0.485},
    {"beta", 1.0, 1.0471975511965976, 4.0, 0.8660254037844385, 0.3268218104318058,
     -0.378401247653964, 0.0, 0.985},
    {"beta", 0.02, 0.05, 6.0, 0.9996875162757026, -0.02400175680094203, -0.006984659833185136, 0.0,
     0.004999999999999987},
    {"bigblue", 1.0, 0.0, 0.0, 1.0000000000000002, 0.0, 0.0, 0.0, 1.0},
    {"bigblue", 1.5, 0.1, 0.0, 0.9987502603949664, -0.04997916927067834, 0.0, 0.0, 1.5},
    {"bigblue", 2.0, 0.2, 1.5707963267948966, 0.9950041652780258, 0.0, 0.09983341664682815, 0.0,
     2.0},
    {"bigblue", 0.5, -0.3, 2.5, 0.9887710779360422, -0.11972140575048093, -0.08943455955236601, 0.0,
     0.5},
    {"bigblue", 1.0, 1.0471975511965976, 4.0, 0.8660254037844385, 0.3268218104318058,
     -0.378401247653964, 0.0, 1.0},
    {"bigblue", 0.02, 0.05, 6.0, 0.9996875162757026, -0.02400175680094203, -0.006984659833185136,
     0.0, 0.02},
    {"isohaisu", 1.0, 0.0, 0.0, 1.0000000000000002, 0.0, 0.0, 0.0, 0.0},
    {"isohaisu", 1.5, 0.1, 0.0, 0.9987502603949664, -0.04997916927067834, 0.0, 0.0, 0.0},
    {"isohaisu", 2.0, 0.2, 1.5707963267948966, 0.9950041652780258, 0.0, 0.09983341664682815, 0.0,
     0.09250000000000003},
    {"isohaisu", 0.5, -0.3, 2.5, 0.9887710779360422, -0.11972140575048093, -0.08943455955236601,
     0.0, 0.0},
    {"isohaisu", 1.0, 1.0471975511965976, 4.0, 0.8660254037844385, 0.3268218104318058,
     -0.378401247653964, 0.0, 0.0},
    {"isohaisu", 0.02, 0.05, 6.0, 0.9996875162757026, -0.02400175680094203, -0.006984659833185136,
     0.0, 0.0},
    {"falcon", 1.0, 0.0, 0.0, 1.0000000000000002, 0.0, 0.0, 0.0, 1.0},
    {"falcon", 1.5, 0.1, 0.0, 0.9987502603949664, -0.04997916927067834, 0.0, 0.0, 1.5},
    {"falcon", 2.0, 0.2, 1.5707963267948966, 0.9950041652780258, 0.0, 0.09983341664682815, 0.0,
     2.0},
    {"falcon", 0.5, -0.3, 2.5, 0.9887710779360422, -0.11972140575048093, -0.08943455955236601, 0.0,
     0.5},
    {"falcon", 1.0, 1.0471975511965976, 4.0, 0.8660254037844385, 0.3268218104318058,
     -0.378401247653964, 0.0, 1.0},
    {"falcon", 0.02, 0.05, 6.0, 0.9996875162757026, -0.02400175680094203, -0.006984659833185136,
     0.0, 0.02},
    {"multistage", 1.0, 0.0, 0.0, 1.0000000000000002, 0.0, 0.0, 0.0, 1.0},
    {"multistage", 1.5, 0.1, 0.0, 0.9987502603949664, -0.04997916927067834, 0.0, 0.0, 1.5},
    {"multistage", 2.0, 0.2, 1.5707963267948966, 0.9950041652780258, 0.0, 0.09983341664682815, 0.0,
     2.0},
    {"multistage", 0.5, -0.3, 2.5, 0.9887710779360422, -0.11972140575048093, -0.08943455955236601,
     0.0, 0.5},
    {"multistage", 1.0, 1.0471975511965976, 4.0, 0.8660254037844385, 0.3268218104318058,
     -0.378401247653964, 0.0, 1.0},
    {"multistage", 0.02, 0.05, 6.0, 0.9996875162757026, -0.02400175680094203, -0.006984659833185136,
     0.0, 0.02},
    {"motorpods", 1.0, 0.0, 0.0, 1.0000000000000002, 0.0, 0.0, 0.0, 0.961},
    {"motorpods", 1.5, 0.1, 0.0, 0.9987502603949664, -0.04997916927067834, 0.0, 0.0,
     1.4609999999999999},
    {"motorpods", 2.0, 0.2, 1.5707963267948966, 0.9950041652780258, 0.0, 0.09983341664682815, 0.0,
     1.9609999999999999},
    {"motorpods", 0.5, -0.3, 2.5, 0.9887710779360422, -0.11972140575048093, -0.08943455955236601,
     0.0, 0.46099999999999997},
    {"motorpods", 1.0, 1.0471975511965976, 4.0, 0.8660254037844385, 0.3268218104318058,
     -0.378401247653964, 0.0, 0.961},
    {"motorpods", 0.02, 0.05, 6.0, 0.9996875162757026, -0.02400175680094203, -0.006984659833185136,
     0.0, 0.0},
    {"clusterpods", 1.0, 0.0, 0.0, 1.0000000000000002, 0.0, 0.0, 0.0, 1.0},
    {"clusterpods", 1.5, 0.1, 0.0, 0.9987502603949664, -0.04997916927067834, 0.0, 0.0, 1.5},
    {"clusterpods", 2.0, 0.2, 1.5707963267948966, 0.9950041652780258, 0.0, 0.09983341664682815, 0.0,
     2.0},
    {"clusterpods", 0.5, -0.3, 2.5, 0.9887710779360422, -0.11972140575048093, -0.08943455955236601,
     0.0, 0.5},
    {"clusterpods", 1.0, 1.0471975511965976, 4.0, 0.8660254037844385, 0.3268218104318058,
     -0.378401247653964, 0.0, 1.0},
    {"clusterpods", 0.02, 0.05, 6.0, 0.9996875162757026, -0.02400175680094203,
     -0.006984659833185136, 0.0, 0.02},
}};

/// Expects the status of @p pin's rocket on @p pin's launch rod to have Java's orientation and
/// effective launch rod length.
void expectOrientation(const OrientationPin& pin)
{
    SCOPED_TRACE(
        std::format("{} rod {} {} {}", pin.rocket, pin.rodLength, pin.rodAngle, pin.rodDirection));
    const ProbeRocket probe(pin.rocket);
    const auto        conditions = std::make_shared<SimulationConditions>();
    conditions->setLaunchRodLength(pin.rodLength);
    conditions->setLaunchRodAngle(pin.rodAngle);
    conditions->setLaunchRodDirection(pin.rodDirection);
    const SimulationStatus status = probe.status(conditions);
    // The quaternion went through sin() and cos().
    EXPECT_TRUE(
        isJavaQuaternion(status.getRocketOrientationQuaternion(), pin.w, pin.x, pin.y, pin.z));
    // The rod length is a difference of positions along the rocket: Java's value to the last
    // digits, and exactly zero where the lug sits further forward than the rod is long.
    EXPECT_NEAR(status.getEffectiveLaunchRodLength(), pin.effectiveRodLength,
                1e-15 * std::max(1.0, pin.effectiveRodLength));
    if (pin.effectiveRodLength == 0.0)
    {
        EXPECT_EQ(status.getEffectiveLaunchRodLength(), 0.0);
    }
}

TEST(SimulationStatus, TheInitialOrientationAndTheEffectiveRodLengthAreJavas)
{
    for (const OrientationPin& pin : kOrientations)
    {
        expectOrientation(pin);
    }
}

TEST(SimulationStatus, TheOrientationIsTheLaunchRodsWhateverTheRocket)
{
    // The roll angle of least stability is that of new flight conditions, 0, so the orientation
    // depends on the rod alone; a vertical rod leaves the rocket's z axis up.
    //
    // The components went through the sines and cosines of three half angles and two quaternion
    // products, and are compared with sin() and cos() of this platform: 1e-12, as for a value
    // of a mathematical function (a library that is one ulp off per call moves them by up to
    // 5e-16). Java's values of the same orientations are pinned in the test above.
    constexpr double  kRotatedTolerance = 1e-12;
    const ProbeRocket probe("alpha");
    const auto        conditions = std::make_shared<SimulationConditions>();
    conditions->setLaunchRodDirection(1.25);
    const SimulationStatus vertical = probe.status(conditions);
    const Coordinate       up       = vertical.getRocketOrientationQuaternion().rotateZ();
    EXPECT_NEAR(up.x, 0.0, kRotatedTolerance);
    EXPECT_NEAR(up.y, 0.0, kRotatedTolerance);
    EXPECT_NEAR(up.z, 1.0, kRotatedTolerance);

    // A rod tilted by 0.2 rad towards the east (direction pi / 2, 0 being north): the axis
    // leans by sin(0.2) to +x.
    conditions->setLaunchRodAngle(0.2);
    conditions->setLaunchRodDirection(std::numbers::pi / 2);
    const Coordinate east = probe.status(conditions).getRocketOrientationQuaternion().rotateZ();
    EXPECT_NEAR(east.x, std::sin(0.2), kRotatedTolerance);
    EXPECT_NEAR(east.y, 0.0, kRotatedTolerance);
    EXPECT_NEAR(east.z, std::cos(0.2), kRotatedTolerance);

    // Towards the north (direction 0): to +y.
    conditions->setLaunchRodDirection(0);
    const Coordinate north = probe.status(conditions).getRocketOrientationQuaternion().rotateZ();
    EXPECT_NEAR(north.x, 0.0, kRotatedTolerance);
    EXPECT_NEAR(north.y, std::sin(0.2), kRotatedTolerance);
    EXPECT_NEAR(north.z, std::cos(0.2), kRotatedTolerance);
}

TEST(SimulationStatus, ANewStatusStartsAtTheLaunchConditions)
{
    // "== initial": "time 0.0", "position (1.00000,2.00000,3.00000)", "velocity (4.00000,
    // 5.00000,6.00000)", "world 28.610000000000003 -80.6 123.0", "rotationVelocity (0.00000,
    // 0.00000,0.00000)", "flags falsefalsefalsefalsefalsefalsefalse", "maxAlt -kInf maxAltTime
    // 0.0 maxZVelocity -kInf", "branch null true", "warnings empty true", "queue size 0",
    // "deployed 0", "modID invalid true", "extra null", "recordWarnings false", "tumble 0.0",
    // "moveBurntOutMotor false"
    const ProbeRocket probe("alpha");
    const auto        conditions = std::make_shared<SimulationConditions>();
    conditions->setLaunchPosition(Coordinate{1, 2, 3});
    conditions->setLaunchVelocity(Coordinate{4, 5, 6});
    conditions->setLaunchSite({28.61, -80.6, 123});
    const auto                                 before = SimulationStatus::WallClock::now();
    const std::shared_ptr<FlightConfiguration> config =
        std::make_shared<FlightConfiguration>(probe.config->clone());
    SimulationStatus s(config, conditions);
    const auto       after = SimulationStatus::WallClock::now();

    EXPECT_EQ(s.getSimulationTime(), 0.0);
    EXPECT_TRUE(s.getRocketPosition().exactlyEquals(Coordinate{1, 2, 3}));
    EXPECT_TRUE(s.getRocketVelocity().exactlyEquals(Coordinate{4, 5, 6}));
    EXPECT_EQ(s.getRocketWorldPosition(), (WorldCoordinate{28.61, -80.6, 123}));
    EXPECT_EQ(s.getRocketWorldPosition().getAltitude(), 123.0);
    EXPECT_TRUE(s.getRocketRotationVelocity().exactlyEquals(Coordinate::kNul));
    EXPECT_FALSE(s.isMotorIgnited());
    EXPECT_FALSE(s.isLiftoff());
    EXPECT_FALSE(s.isLaunchRodCleared());
    EXPECT_FALSE(s.isApogeeReached());
    EXPECT_FALSE(s.isTumbling());
    EXPECT_FALSE(s.isLanded());
    EXPECT_FALSE(s.isSeparatedStage());
    EXPECT_EQ(s.getMaxAlt(), -kInf);
    EXPECT_EQ(s.getMaxAltTime(), 0.0);
    EXPECT_EQ(s.getMaxZVelocity(), -kInf);
    EXPECT_EQ(s.getStartWarningsTime(), 1200.0) << "the recommended maximum simulation time";
    EXPECT_EQ(s.getFlightDataBranch(), nullptr);
    ASSERT_NE(s.getWarnings(), nullptr);
    EXPECT_TRUE(s.getWarnings()->empty());
    EXPECT_TRUE(s.getEventQueue().empty());
    EXPECT_TRUE(s.getDeployedRecoveryDevices().empty());
    EXPECT_EQ(s.getModId(), ModId::invalid());
    EXPECT_EQ(s.modId(), ModId::invalid());
    EXPECT_EQ(s.getExtraData("x"), nullptr);
    EXPECT_FALSE(s.recordWarnings());
    EXPECT_EQ(s.getTumbleDetector().getFilteredAOA(), 0.0);
    EXPECT_FALSE(s.moveBurntOutMotor(s.getMotors().front()->getId()));

    // What it was given, and the clock.
    EXPECT_EQ(s.getSimulationConditions(), conditions);
    EXPECT_EQ(&s.getConfiguration(), config.get());
    EXPECT_EQ(&s.getFlightConfiguration(), config.get());
    EXPECT_EQ(s.getConfigurationPointer(), config);
    EXPECT_GE(s.getSimulationStartWallTime(), before);
    EXPECT_LE(s.getSimulationStartWallTime(), after);
    const SimulationStatus& constant = s;
    EXPECT_EQ(&constant.getConfiguration(), config.get());
    EXPECT_EQ(&constant.getFlightConfiguration(), config.get());
    EXPECT_EQ(&constant.getEventQueue(), &s.getEventQueue());
    EXPECT_EQ(&constant.getTumbleDetector(), &s.getTumbleDetector());
    EXPECT_EQ(&constant.getDeployedRecoveryDevices(), &s.getDeployedRecoveryDevices());
}

TEST(SimulationStatus, NeedsConditionsAndAConfiguration)
{
    // Java: a NullPointerException for either.
    const ProbeRocket probe("alpha");
    const auto        conditions = std::make_shared<SimulationConditions>();
    const auto        config     = std::make_shared<FlightConfiguration>(probe.config->clone());
    EXPECT_EQ(bugText([&] { const SimulationStatus s(config, nullptr); }),
              "A simulation status needs simulation conditions");
    EXPECT_EQ(bugText([&] { const SimulationStatus s(nullptr, conditions); }),
              "A simulation status needs a flight configuration");
    SimulationStatus s(config, conditions);
    EXPECT_EQ(bugText([&] { s.setConfiguration(nullptr); }),
              "A simulation status needs a flight configuration");
    EXPECT_EQ(&s.getConfiguration(), config.get());
}

// ============================================================================ the motors

/// The motor states of @p states as the probe prints them, sorted: "<mount name>|<designation>
/// |<motor count>|<ignition event>".
[[nodiscard]] std::vector<std::string> motorLines(
    std::span<const std::shared_ptr<MotorClusterState>> states)
{
    std::vector<std::string> lines;
    for (const std::shared_ptr<MotorClusterState>& state : states)
    {
        const MotorClusterState& motorState = *state;
        lines.push_back(
            std::format("{}|{}|{}|{}", QtRocket::asComponent(motorState.getMount()).getName(),
                        motorState.getMotor()->getDesignation(), motorState.getMotorCount(),
                        name(motorState.getIgnitionEvent())));
    }
    std::ranges::sort(lines);
    return lines;
}

/// The active motor states of @p status as the probe prints them, sorted: "<mount name>|<motor
/// count>".
[[nodiscard]] std::vector<std::string> activeMotorLines(const SimulationStatus& status)
{
    std::vector<std::string> lines;
    for (const std::shared_ptr<MotorClusterState>& state : status.getActiveMotors())
    {
        const MotorClusterState& motorState = *state;
        lines.push_back(std::format("{}|{}", QtRocket::asComponent(motorState.getMount()).getName(),
                                    motorState.getMotorCount()));
    }
    std::ranges::sort(lines);
    return lines;
}

/// The motors of a new status of the probe's rocket @p name (sorted, see motorLines()).
[[nodiscard]] std::vector<std::string> motorsOf(std::string_view name)
{
    const ProbeRocket      probe(name);
    const SimulationStatus status = probe.status();
    return motorLines(status.getMotors());
}

/// The active motors of a status of the probe's rocket @p name when only stage @p stage flies.
[[nodiscard]] std::vector<std::string> activeMotorsOfStage(std::string_view name, int stage)
{
    const ProbeRocket probe(name);
    SimulationStatus  status = probe.status();
    status.getConfiguration().setOnlyStage(stage);
    return activeMotorLines(status);
}

using Lines = std::vector<std::string>;

TEST(SimulationStatus, HasOneMotorStateForEveryMotorOfTheConfiguration)
{
    // MotorsProbe.java, "<rocket> all: [...]": Java's states as a set (its order is the hash
    // order of a HashMap), for a status whose configuration is a clone of the rocket's, as the
    // engine makes it.
    EXPECT_EQ(motorsOf("alpha"), (Lines{"Motor Mount Tube|A8|1|AUTOMATIC"}));
    EXPECT_EQ(motorsOf("beta"),
              (Lines{"Booster MMT|D21|1|AUTOMATIC", "Motor Mount Tube|B4|1|AUTOMATIC"}));
    EXPECT_EQ(motorsOf("falcon"),
              (Lines{"Booster Motor Tubes|G77|8|AUTOMATIC", "Core Stage Body|M1350|1|AUTOMATIC"}));
    EXPECT_EQ(motorsOf("multistage"),
              (Lines{"Center Booster Body Tube|C6|1|LAUNCH", "Side Booster Body Tubes|A10|2|LAUNCH",
                     "Sustainer Body Tube|C6|1|BURNOUT"}));
    EXPECT_EQ(motorsOf("motorpods"),
              (Lines{"Motor Mount Tube|A8|1|AUTOMATIC", "Pod Motor Mount|A10|2|AUTOMATIC"}));
    EXPECT_EQ(motorsOf("secondmotor"), (Lines{"Motor Mount Tube|A8|1|AUTOMATIC",
                                              "Second Motor Mount Tube|A10|1|AUTOMATIC"}));
    EXPECT_EQ(motorsOf("clusterpods"),
              (Lines{"Inner Tube|C6|12|LAUNCH", "Inner Tube|C6|2|LAUNCH"}));
}

TEST(SimulationStatus, TheMotorsAreThoseTheConfigurationKnowsWhenTheStatusIsMade)
{
    // MotorsProbe: "motorpods own configuration: 1 motors, clone: 2", "motorpods own
    // configuration after update(): 2". Giving a mount a motor fires no event, so the rocket's
    // own configuration does not know the pods' motors, which TestRockets adds last, until it is
    // updated; a clone of it is computed from the rocket as it is. A status takes the motors
    // its configuration has.
    const ProbeRocket probe("motorpods");
    EXPECT_EQ(probe.config->getAllMotors().size(), 1U);
    EXPECT_EQ(probe.status().getMotors().size(), 2U);
    probe.config->update();
    EXPECT_EQ(probe.config->getAllMotors().size(), 2U);

    // The motors a configuration gets later do not reach a status that exists.
    const ProbeRocket second("secondmotor");
    EXPECT_EQ(second.config->getAllMotors().size(), 1U);
    SimulationStatus status = second.status();
    EXPECT_EQ(status.getMotors().size(), 2U);
    status.getConfiguration().clearAllStages();
    EXPECT_EQ(status.getMotors().size(), 2U);
    EXPECT_TRUE(status.getActiveMotors().empty());
}

TEST(SimulationStatus, TheMotorStatesAreInTheOrderOfTheConfigurationsMotors)
{
    // Java's order is the hash order of the random motor configuration ids. Here it is the
    // order of FlightConfiguration::getAllMotors(), the order in which the mounts were found.
    const ProbeRocket                                      probe("multistage");
    const SimulationStatus                                 status = probe.status();
    const std::vector<std::shared_ptr<MotorClusterState>>& states = status.getMotors();
    std::vector<const QtRocket::MotorMount*>               mountsOfStates;
    std::vector<QtRocket::ThrustState>                     thrustStates;
    for (const std::shared_ptr<MotorClusterState>& state : states)
    {
        const MotorClusterState& motorState = *state;
        mountsOfStates.push_back(&motorState.getMount());
        thrustStates.push_back(motorState.getState());
    }
    std::vector<const QtRocket::MotorMount*> mountsOfMotors;
    for (const QtRocket::MotorConfiguration& motor : status.getConfiguration().getAllMotors())
    {
        mountsOfMotors.push_back(&motor.getMount());
    }
    ASSERT_EQ(states.size(), 3U);
    EXPECT_EQ(mountsOfStates, mountsOfMotors);
    EXPECT_EQ(thrustStates, std::vector<QtRocket::ThrustState>(3, QtRocket::ThrustState::ARMED));
    // Each state is an object of its own, shared from then on.
    EXPECT_NE(states[0], states[1]);
    EXPECT_EQ(states[0].use_count(), 1);
}

TEST(SimulationStatus, TheActiveMotorsAreThoseOfTheActiveStages)
{
    // "<rocket> onlyStage <n>: [...]"
    EXPECT_EQ(activeMotorsOfStage("alpha", 0), (Lines{"Motor Mount Tube|1"}));
    EXPECT_EQ(activeMotorsOfStage("beta", 0), (Lines{"Motor Mount Tube|1"}));
    EXPECT_EQ(activeMotorsOfStage("beta", 1), (Lines{"Booster MMT|1"}));
    EXPECT_EQ(activeMotorsOfStage("falcon", 0), (Lines{}));
    EXPECT_EQ(activeMotorsOfStage("falcon", 1), (Lines{"Core Stage Body|1"}));
    EXPECT_EQ(activeMotorsOfStage("falcon", 2), (Lines{"Booster Motor Tubes|8"}));
    EXPECT_EQ(activeMotorsOfStage("multistage", 0), (Lines{"Sustainer Body Tube|1"}));
    EXPECT_EQ(activeMotorsOfStage("multistage", 1), (Lines{"Center Booster Body Tube|1"}));
    EXPECT_EQ(activeMotorsOfStage("multistage", 2), (Lines{"Side Booster Body Tubes|2"}));
    EXPECT_EQ(activeMotorsOfStage("motorpods", 0),
              (Lines{"Motor Mount Tube|1", "Pod Motor Mount|2"}));
    EXPECT_EQ(activeMotorsOfStage("secondmotor", 0),
              (Lines{"Motor Mount Tube|1", "Second Motor Mount Tube|1"}));
    EXPECT_EQ(activeMotorsOfStage("clusterpods", 0), (Lines{"Inner Tube|2"}));
    EXPECT_EQ(activeMotorsOfStage("clusterpods", 1), (Lines{"Inner Tube|12"}));
}

TEST(SimulationStatus, TheActiveMotorsFollowTheConfigurationAndKeepTheOrder)
{
    const ProbeRocket probe("multistage");
    SimulationStatus  status = probe.status();
    // "multistage allStages: 3"
    EXPECT_EQ(status.getActiveMotors(), status.getMotors());
    status.getConfiguration().setOnlyStage(1);
    ASSERT_EQ(status.getActiveMotors().size(), 1U);
    EXPECT_EQ(status.getMotors().size(), 3U) << "every state is kept";
    status.getConfiguration().clearAllStages();
    EXPECT_TRUE(status.getActiveMotors().empty());
    status.getConfiguration().setAllStages();
    EXPECT_EQ(status.getActiveMotors(), status.getMotors());

    // The states as MassCalculator takes them: the same objects, in the same order.
    const std::vector<const MotorClusterState*> pointers = status.getActiveMotorStates();
    ASSERT_EQ(pointers.size(), 3U);
    EXPECT_EQ(pointers[0], status.getMotors()[0].get());
    EXPECT_EQ(pointers[1], status.getMotors()[1].get());
    EXPECT_EQ(pointers[2], status.getMotors()[2].get());
    status.getConfiguration().setOnlyStage(0);
    EXPECT_EQ(status.getActiveMotorStates().size(), 1U);
}

/// The thrust of the active motors of @p status at its simulation time, summed as
/// AbstractRKSimulationStepper.calculateThrust() sums it.
[[nodiscard]] double activeThrust(const SimulationStatus& status)
{
    double thrust = 0;
    for (const std::shared_ptr<MotorClusterState>& state : status.getActiveMotors())
    {
        thrust += state->getThrust(status.getSimulationTime());
    }
    return thrust;
}

TEST(SimulationStatus, TheActiveMotorsOfTheClusterPodsThrustStageByStage)
{
    // The status's part of MotorClusterPodsTest.testMotorClusterPods (the test itself asks the
    // stepper, and is ported with it): "clusterpods thrust stage -1: 70.0", "stage 0: 10.0",
    // "stage 1: 60.0". The thrust of a single C6 at 0.4 s is 5 (TestRockets' curve).
    TestClusterPods  pods;
    SimulationStatus status(statusConfiguration(*pods.rocket, pods.fcid),
                            std::make_shared<SimulationConditions>());
    for (const std::shared_ptr<MotorClusterState>& clusterState : status.getMotors())
    {
        clusterState->ignite(0.0);
    }
    status.setSimulationTime(0.4);
    const double c6Thrust = 5.0;

    // Two motors in sustainer
    status.getConfiguration().setOnlyStage(0);
    EXPECT_NEAR(2.0 * c6Thrust, activeThrust(status), QtRocket::MathUtil::kEpsilon);
    // Three side boosters with four motors in each
    status.getConfiguration().setOnlyStage(1);
    EXPECT_NEAR(12.0 * c6Thrust, activeThrust(status), QtRocket::MathUtil::kEpsilon);
    // All 14 motors now
    status.getConfiguration().setAllStages();
    EXPECT_NEAR(14.0 * c6Thrust, activeThrust(status), QtRocket::MathUtil::kEpsilon);
}

// ====================================================== the copy constructor and clone()

/// A listener with a value member and a log its clones share (StatusProbe.Counting). It
/// refuses TUMBLE events.
class Counting final : public CloneableSimulationListener<Counting>
{
public:
    [[nodiscard]] bool addFlightEvent(SimulationStatus& /*status*/,
                                      const FlightEvent& event) override
    {
        m_log->push_back("add " + std::string{name(event.getType())});
        return event.getType() != FlightEvent::Type::TUMBLE;
    }

    [[nodiscard]] int value() const noexcept { return m_value; }
    void              setValue(int value) noexcept { m_value = value; }
    [[nodiscard]] const std::shared_ptr<std::vector<std::string>>& log() const noexcept
    {
        return m_log;
    }

private:
    int                                       m_value{0};
    std::shared_ptr<std::vector<std::string>> m_log{std::make_shared<std::vector<std::string>>()};
};

/// The status the probe copies ("== copy constructor and clone()"): the multi-stage rocket with
/// only its sustainer active, every value and flag set, a listener, a branch with one row, a
/// warning, a deployed device, three queued events, extra data and a tumble detector that has
/// seen two steps.
struct CopySource
{
    TestMultiStageEventTestRocket         rocket;
    std::shared_ptr<SimulationConditions> conditions = std::make_shared<SimulationConditions>();
    std::shared_ptr<Counting>             listener   = std::make_shared<Counting>();
    std::shared_ptr<FlightDataBranch>     branch     = newBranch("b");
    std::shared_ptr<int>                  extra      = std::make_shared<int>(5);
    std::unique_ptr<SimulationStatus>     status;

    CopySource()
      : status(std::make_unique<SimulationStatus>(statusConfiguration(*rocket.rocket, rocket.fcid),
                                                  conditions))
    {
        conditions->getSimulationListenerList().push_back(listener);
        SimulationStatus& orig = *status;
        orig.setFlightDataBranch(branch);
        orig.setSimulationTime(1.5);
        orig.setRocketPosition(Coordinate{1, 2, 3});
        orig.setRocketVelocity(Coordinate{4, 5, 6});
        orig.setRocketWorldPosition({10, 20, 30});
        orig.setRocketOrientationQuaternion({0.5, 0.5, 0.5, 0.5});
        orig.setRocketRotationVelocity(Coordinate{7, 8, 9});
        orig.setEffectiveLaunchRodLength(0.75);
        orig.setMotorIgnited(true);
        orig.setLiftoff(true);
        orig.setLaunchRodCleared(true);
        orig.setApogeeReached(true);
        orig.setTumbling(true);
        orig.setLanded(true);
        orig.setSeparatedStage(true);
        orig.setMaxAlt(100);
        orig.setMaxAltTime(5);
        orig.storeData();
        orig.putExtraData("key", extra);
        orig.addWarning(Warning::kListenersAffected);
        orig.getDeployedRecoveryDevices().add(rocket.sustainerChute);
        const Rocket* source = rocket.rocket.get();
        orig.getEventQueue().add({FlightEvent::Type::APOGEE, 3.0, source});
        orig.getEventQueue().add({FlightEvent::Type::ALTITUDE, 2.0, source,
                                  FlightEvent::AltitudeChange{.previous = 1.0, .current = 2.0}});
        orig.getEventQueue().add({FlightEvent::Type::LIFTOFF, 2.0});
        orig.getTumbleDetector().update(0.0, true, 2.0, 50, 1.2, 10);
        orig.getTumbleDetector().update(0.1, true, 2.0, 50, 1.2, 10);
        orig.getConfiguration().setOnlyStage(0);
        listener->setValue(7);
    }
};

/// The stage flags of @p status's configuration as the probe prints them: "100".
[[nodiscard]] std::string stageFlags(const SimulationStatus& status)
{
    std::string flags;
    for (int i = 0; i < status.getConfiguration().getStageCount(); i++)
    {
        flags += status.getConfiguration().isStageActive(i) ? '1' : '0';
    }
    return flags;
}

TEST(SimulationStatusCopy, TheCopyHasClonesOfTheConditionsTheListenersAndTheConfiguration)
{
    // "conditions same false", "listener same false class Counting value 7 log same true",
    // "conditions simulation same true", "configuration same false fcid same true modID same
    // true rocket same true", "copy stages 100"
    const CopySource        source;
    const SimulationStatus& orig = *source.status;
    const SimulationStatus  copy(orig);

    EXPECT_NE(copy.getSimulationConditions(), orig.getSimulationConditions());
    ASSERT_EQ(copy.getSimulationConditions()->getSimulationListenerList().size(), 1U);
    const std::shared_ptr<SimulationListener> cloned =
        copy.getSimulationConditions()->getSimulationListenerList().front();
    EXPECT_NE(cloned, source.listener);
    const auto counting = std::dynamic_pointer_cast<Counting>(cloned);
    ASSERT_NE(counting, nullptr);
    EXPECT_EQ(counting->value(), 7);
    EXPECT_EQ(counting->log(), source.listener->log()) << "what the clones share";
    EXPECT_EQ(orig.getSimulationConditions()->getSimulationListenerList().front(), source.listener)
        << "the original keeps the caller's listener";

    EXPECT_NE(&copy.getConfiguration(), &orig.getConfiguration());
    EXPECT_EQ(copy.getConfiguration().getFlightConfigurationId(),
              orig.getConfiguration().getFlightConfigurationId());
    EXPECT_EQ(copy.getConfiguration().getModId(), orig.getConfiguration().getModId());
    EXPECT_EQ(&copy.getConfiguration().getRocket(), &orig.getConfiguration().getRocket());
    EXPECT_EQ(stageFlags(copy), "100");
    EXPECT_EQ(stageFlags(orig), "100");
}

TEST(SimulationStatusCopy, TheCopySharesTheBranchAndTheMotorStatesAndCopiesTheContainers)
{
    // "branch same true", "motors list same false size 3 elements same true", "motor states
    // identical in order true", "queue same false size 3", "queue order orig [LIFTOFF APOGEE
    // ALTITUDE ] copy [LIFTOFF APOGEE ALTITUDE ] same events true", "queue modID copy invalid
    // false", "warnings same false copy size 0 orig size 1", "extra value same value true",
    // "extra map shared false", "deployed same false size 1 contains true", "tumble same false
    // filtered 0.15298705655378297 orig 0.15298705655378297"
    CopySource        source;
    SimulationStatus& orig = *source.status;
    SimulationStatus  copy(orig);

    EXPECT_EQ(copy.getFlightDataBranch(), source.branch);

    EXPECT_NE(&copy.getMotors(), &orig.getMotors());
    ASSERT_EQ(copy.getMotors().size(), 3U);
    EXPECT_EQ(copy.getMotors(), orig.getMotors()) << "the same state objects, in order";

    EXPECT_NE(&copy.getEventQueue(), &orig.getEventQueue());
    EXPECT_EQ(queueText(orig.getEventQueue()),
              "LIFTOFF@2.0:- APOGEE@3.0:Rocket ALTITUDE@2.0:Rocket");
    EXPECT_EQ(queueText(copy.getEventQueue()), queueText(orig.getEventQueue()));
    EXPECT_TRUE(copy.getEventQueue().begin()->sameEvent(*orig.getEventQueue().begin()));
    EXPECT_NE(copy.getEventQueue().modId(), ModId::invalid()) << "filled with clear() and addAll()";

    EXPECT_NE(copy.getWarnings(), orig.getWarnings());
    ASSERT_NE(copy.getWarnings(), nullptr);
    EXPECT_TRUE(copy.getWarnings()->empty()) << "the warning set is not cloned: a new one";
    EXPECT_EQ(orig.getWarnings()->size(), 1U);

    const std::any* extra = copy.getExtraData("key");
    ASSERT_NE(extra, nullptr);
    EXPECT_EQ(std::any_cast<std::shared_ptr<int>>(*extra), source.extra) << "the same value";
    copy.putExtraData("other", 1);
    EXPECT_EQ(orig.getExtraData("other"), nullptr) << "a map of its own";

    EXPECT_NE(&copy.getDeployedRecoveryDevices(), &orig.getDeployedRecoveryDevices());
    EXPECT_EQ(copy.getDeployedRecoveryDevices().size(), 1U);
    EXPECT_TRUE(copy.getDeployedRecoveryDevices().contains(source.rocket.sustainerChute));

    EXPECT_NE(&copy.getTumbleDetector(), &orig.getTumbleDetector());
    EXPECT_TRUE(isJavaValue(0.15298705655378297, orig.getTumbleDetector().getFilteredAOA()));
    EXPECT_EQ(copy.getTumbleDetector().getFilteredAOA(), orig.getTumbleDetector().getFilteredAOA());
}

TEST(SimulationStatusCopy, TheCopyHasTheValuesButNotTheMaximumAltitude)
{
    // "modID same true", "time 1.5 position (1.00000,2.00000,3.00000) velocity (4.00000,
    // 5.00000,6.00000) rotation (7.00000,8.00000,9.00000) rod 0.75", "flags truetruetruetrue
    // truetruetrue", "maxAlt -kInf maxAltTime 0.0 maxZVelocity 6.0 orig maxAlt 100.0", "wall
    // same true", "recordWarnings orig false copy false", "copy recordWarnings at 1.74 false",
    // "copy recordWarnings at 1.75 true"
    const CopySource        source;
    const SimulationStatus& orig = *source.status;
    SimulationStatus        copy(orig);

    EXPECT_EQ(copy.getModId(), orig.getModId());
    EXPECT_EQ(copy.getSimulationTime(), 1.5);
    EXPECT_TRUE(copy.getRocketPosition().exactlyEquals(Coordinate{1, 2, 3}));
    EXPECT_TRUE(copy.getRocketVelocity().exactlyEquals(Coordinate{4, 5, 6}));
    EXPECT_TRUE(copy.getRocketRotationVelocity().exactlyEquals(Coordinate{7, 8, 9}));
    EXPECT_EQ(copy.getRocketOrientationQuaternion(), (Quaternion{0.5, 0.5, 0.5, 0.5}));
    EXPECT_EQ(copy.getRocketWorldPosition(), (WorldCoordinate{10, 20, 30}));
    EXPECT_EQ(copy.getEffectiveLaunchRodLength(), 0.75);
    EXPECT_TRUE(copy.isMotorIgnited());
    EXPECT_TRUE(copy.isLiftoff());
    EXPECT_TRUE(copy.isLaunchRodCleared());
    EXPECT_TRUE(copy.isApogeeReached());
    EXPECT_TRUE(copy.isTumbling());
    EXPECT_TRUE(copy.isLanded());
    EXPECT_TRUE(copy.isSeparatedStage());
    EXPECT_EQ(copy.getMaxZVelocity(), 6.0);
    EXPECT_EQ(copy.getSimulationStartWallTime(), orig.getSimulationStartWallTime());

    // Java's copy constructor does not copy these two.
    EXPECT_EQ(copy.getMaxAlt(), -kInf);
    EXPECT_EQ(copy.getMaxAltTime(), 0.0);
    EXPECT_EQ(orig.getMaxAlt(), 100.0);
    EXPECT_EQ(orig.getMaxAltTime(), 5.0);

    // The time from which warnings are recorded came along: the rod was cleared at 1.5 s.
    EXPECT_EQ(copy.getStartWarningsTime(), 1.75);
    EXPECT_FALSE(orig.recordWarnings());
    EXPECT_FALSE(copy.recordWarnings());
    copy.setSimulationTime(1.74);
    EXPECT_FALSE(copy.recordWarnings());
    copy.setSimulationTime(1.75);
    EXPECT_TRUE(copy.recordWarnings());
}

TEST(SimulationStatusCopy, TheCopyAndTheOriginalGoTheirOwnWaysButForTheMotorStates)
{
    // "orig stage 1 active after copy.setAllStages false", "orig queue size after copy poll 3",
    // "orig first motor ignition 0.5"
    CopySource        source;
    SimulationStatus& orig = *source.status;
    SimulationStatus  copy(orig);

    copy.getConfiguration().setAllStages();
    EXPECT_FALSE(orig.getConfiguration().isStageActive(1));
    EXPECT_EQ(stageFlags(copy), "111");
    static_cast<void>(copy.getEventQueue().poll());
    EXPECT_EQ(orig.getEventQueue().size(), 3U);
    EXPECT_EQ(copy.getEventQueue().size(), 2U);
    copy.getDeployedRecoveryDevices().clear();
    EXPECT_EQ(orig.getDeployedRecoveryDevices().size(), 1U);
    copy.getTumbleDetector().update(5.0, true, 3.0, 50, 1.2, 10);
    EXPECT_NE(copy.getTumbleDetector().getFilteredAOA(), orig.getTumbleDetector().getFilteredAOA());
    copy.setSimulationTime(9);
    EXPECT_EQ(orig.getSimulationTime(), 1.5);

    // The motor states are the simulation's: igniting one ignites it for every status.
    copy.getMotors().front()->ignite(0.5);
    EXPECT_EQ(orig.getMotors().front()->getIgnitionTime(), 0.5);

    // A warning of the copy goes to its own set and to the shared branch.
    copy.addWarning(Warning::kNoRecoveryDevice);
    EXPECT_EQ(copy.getWarnings()->size(), 1U);
    EXPECT_EQ(orig.getWarnings()->size(), 1U);
    EXPECT_EQ(source.branch->getEvents().size(), 2U);
}

TEST(SimulationStatusCopy, ACopyOfACopyClonesTheListenersAgain)
{
    CopySource             source;
    const SimulationStatus first(*source.status);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization): the copy is what is tested
    const SimulationStatus                    second(first);
    const std::shared_ptr<SimulationListener> a =
        first.getSimulationConditions()->getSimulationListenerList().front();
    const std::shared_ptr<SimulationListener> b =
        second.getSimulationConditions()->getSimulationListenerList().front();
    EXPECT_NE(a, b);
    EXPECT_NE(b, source.listener);
    const std::shared_ptr<Counting> counting = std::dynamic_pointer_cast<Counting>(b);
    ASSERT_NE(counting, nullptr);
    EXPECT_EQ(counting->log(), source.listener->log());
    EXPECT_EQ(second.getMotors(), source.status->getMotors());
    EXPECT_EQ(second.getFlightDataBranch(), source.branch);
}

TEST(SimulationStatusCopy, AStatusWithoutConditionsCannotBeCopied)
{
    TestStatus fixture;
    fixture.status.setSimulationConditions(nullptr);
    EXPECT_EQ(bugText([&] { const SimulationStatus copy(fixture.status); }),
              "The simulation status to copy has no simulation conditions");
}

TEST(SimulationStatusClone, ACloneSharesEverythingAndHasItsOwnValues)
{
    // "clone conditions same true configuration same true branch same true motors same true
    // queue same true warnings same true deployed same true tumble same true modID same true",
    // "clone extra map shared true", "clone maxAlt 100.0 maxAltTime 5.0 time 1.5", "orig time
    // after clone.setSimulationTime 1.5 position (1.00000,2.00000,3.00000) modID same false",
    // "orig warnings size after clone.setWarnings 1 orig branch b"
    CopySource        source;
    SimulationStatus& orig  = *source.status;
    SimulationStatus  clone = orig.clone();

    EXPECT_EQ(clone.getSimulationConditions(), orig.getSimulationConditions());
    EXPECT_EQ(&clone.getConfiguration(), &orig.getConfiguration());
    EXPECT_EQ(clone.getFlightDataBranch(), orig.getFlightDataBranch());
    EXPECT_EQ(&clone.getMotors(), &orig.getMotors());
    EXPECT_EQ(&clone.getEventQueue(), &orig.getEventQueue());
    EXPECT_EQ(clone.getWarnings(), orig.getWarnings());
    EXPECT_EQ(&clone.getDeployedRecoveryDevices(), &orig.getDeployedRecoveryDevices());
    EXPECT_EQ(&clone.getTumbleDetector(), &orig.getTumbleDetector());
    EXPECT_EQ(clone.getModId(), orig.getModId());
    clone.putExtraData("fromClone", 1);
    EXPECT_NE(orig.getExtraData("fromClone"), nullptr);

    // Every value, the maximum altitude included.
    EXPECT_EQ(clone.getMaxAlt(), 100.0);
    EXPECT_EQ(clone.getMaxAltTime(), 5.0);
    EXPECT_EQ(clone.getMaxZVelocity(), 6.0);
    EXPECT_EQ(clone.getStartWarningsTime(), 1.75);
    EXPECT_EQ(clone.getSimulationTime(), 1.5);
    EXPECT_TRUE(clone.getRocketPosition().exactlyEquals(Coordinate{1, 2, 3}));
    EXPECT_TRUE(clone.getRocketVelocity().exactlyEquals(Coordinate{4, 5, 6}));
    EXPECT_TRUE(clone.getRocketRotationVelocity().exactlyEquals(Coordinate{7, 8, 9}));
    EXPECT_EQ(clone.getRocketOrientationQuaternion(), (Quaternion{0.5, 0.5, 0.5, 0.5}));
    EXPECT_EQ(clone.getRocketWorldPosition(), (WorldCoordinate{10, 20, 30}));
    EXPECT_EQ(clone.getEffectiveLaunchRodLength(), 0.75);
    EXPECT_EQ(clone.getSimulationStartWallTime(), orig.getSimulationStartWallTime());
    EXPECT_TRUE(clone.isMotorIgnited());
    EXPECT_TRUE(clone.isLiftoff());
    EXPECT_TRUE(clone.isLaunchRodCleared());
    EXPECT_TRUE(clone.isApogeeReached());
    EXPECT_TRUE(clone.isTumbling());
    EXPECT_TRUE(clone.isLanded());
    EXPECT_TRUE(clone.isSeparatedStage());

    // The values are the clone's own (what a stepper sets for an intermediate point).
    clone.setSimulationTime(9);
    clone.setRocketPosition(Coordinate{9, 9, 9});
    EXPECT_EQ(orig.getSimulationTime(), 1.5);
    EXPECT_TRUE(orig.getRocketPosition().exactlyEquals(Coordinate{1, 2, 3}));
    EXPECT_NE(clone.getModId(), orig.getModId());

    // What it shares, it shares: an event queued through the clone is queued in the original.
    clone.getEventQueue().add({FlightEvent::Type::TUMBLE, 4.0});
    EXPECT_EQ(orig.getEventQueue().size(), 4U);
    clone.getConfiguration().setAllStages();
    EXPECT_TRUE(orig.getConfiguration().isStageActive(1));

    // Replacing a shared object replaces it for the clone only.
    clone.setWarnings(std::make_shared<WarningSet>());
    clone.setFlightDataBranch(newBranch("c"));
    EXPECT_EQ(orig.getWarnings()->size(), 1U);
    EXPECT_EQ(orig.getFlightDataBranch()->getName(), "b");
    EXPECT_TRUE(clone.getWarnings()->empty());
}

TEST(SimulationStatus, CopyPropertiesTakesTheKinematicsAndFourFlags)
{
    // "copyProperties position (1.00000,2.00000,3.00000) velocity (4.00000,5.00000,6.00000)
    // rotation (7.00000,8.00000,9.00000) flags truetruetruetruefalsefalse time 0.0 modID same
    // true recordWarnings false"
    const CopySource             source;
    TestStatus                   fixture;
    SimulationStatus&            target = fixture.status;
    ModIdWatch<SimulationStatus> watch(target);
    target.copyProperties(*source.status);

    EXPECT_TRUE(target.getRocketPosition().exactlyEquals(Coordinate{1, 2, 3}));
    EXPECT_TRUE(target.getRocketVelocity().exactlyEquals(Coordinate{4, 5, 6}));
    EXPECT_TRUE(target.getRocketRotationVelocity().exactlyEquals(Coordinate{7, 8, 9}));
    EXPECT_EQ(target.getRocketOrientationQuaternion(), (Quaternion{0.5, 0.5, 0.5, 0.5}));
    EXPECT_EQ(target.getRocketWorldPosition(), (WorldCoordinate{10, 20, 30}));
    EXPECT_TRUE(target.isMotorIgnited());
    EXPECT_TRUE(target.isLiftoff());
    EXPECT_TRUE(target.isLaunchRodCleared());
    EXPECT_TRUE(target.isApogeeReached());
    EXPECT_FALSE(target.isTumbling()) << "not copied";
    EXPECT_FALSE(target.isLanded()) << "not copied";
    EXPECT_FALSE(target.isSeparatedStage());
    EXPECT_EQ(target.getSimulationTime(), 0.0);
    EXPECT_EQ(target.getMaxAlt(), -kInf);
    EXPECT_FALSE(watch.drew());
    // The flag was copied, not set: the wait for the warnings did not start.
    EXPECT_EQ(target.getStartWarningsTime(), 1200.0);
    EXPECT_FALSE(target.recordWarnings());
}

TEST(SimulationStatus, MovingHandsTheStatusOver)
{
    CopySource                                  source;
    const std::shared_ptr<SimulationConditions> conditions =
        source.status->getSimulationConditions();
    const FlightConfiguration* config = &source.status->getConfiguration();
    const EventQueue*          queue  = &source.status->getEventQueue();
    const ModId                id     = source.status->getModId();

    SimulationStatus moved = std::move(*source.status);
    EXPECT_EQ(moved.getSimulationConditions(), conditions) << "the very conditions, not a clone";
    EXPECT_EQ(conditions->getSimulationListenerList().front(), source.listener);
    EXPECT_EQ(&moved.getConfiguration(), config);
    EXPECT_EQ(&moved.getEventQueue(), queue);
    EXPECT_EQ(moved.getModId(), id);
    EXPECT_EQ(moved.getMaxAlt(), 100.0);

    // What a stepper's initialize() returns, assigned over the engine's current status.
    TestStatus other;
    other.status = std::move(moved);
    EXPECT_EQ(other.status.getSimulationConditions(), conditions);
    EXPECT_EQ(other.status.getSimulationTime(), 1.5);
}

// ============================================================================ the modification id

TEST(SimulationStatusModId, TheSettersOfTheKinematicsAndTheFlagsDrawOne)
{
    // "== modID effects": "new" for each of these, also for the value the status already has.
    TestStatus                   fixture;
    SimulationStatus&            s = fixture.status;
    ModIdWatch<SimulationStatus> watch(s);
    s.setSimulationTime(1);
    EXPECT_TRUE(watch.drew());
    s.setRocketPosition(Coordinate{0, 0, 1});
    EXPECT_TRUE(watch.drew());
    s.setRocketWorldPosition({1, 2, 3});
    EXPECT_TRUE(watch.drew());
    s.setRocketVelocity(Coordinate{0, 0, 10});
    EXPECT_TRUE(watch.drew());
    s.setRocketOrientationQuaternion(Quaternion{});
    EXPECT_TRUE(watch.drew());
    s.setEffectiveLaunchRodLength(1);
    EXPECT_TRUE(watch.drew());
    s.setSimulationStartWallTime(SimulationStatus::WallClock::time_point{});
    EXPECT_TRUE(watch.drew());
    s.setMotorIgnited(false);
    EXPECT_TRUE(watch.drew());
    s.setLiftoff(false);
    EXPECT_TRUE(watch.drew());
    s.setLaunchRodCleared(false);
    EXPECT_TRUE(watch.drew());
    s.setApogeeReached(false);
    EXPECT_TRUE(watch.drew());
    s.setTumbling(false);
    EXPECT_TRUE(watch.drew());
    s.setLanded(false);
    EXPECT_TRUE(watch.drew());
    s.setMaxAlt(1);
    EXPECT_TRUE(watch.drew());
    s.setMaxAltTime(1);
    EXPECT_TRUE(watch.drew());

    EXPECT_EQ(s.getSimulationTime(), 1.0);
    EXPECT_TRUE(s.getRocketPosition().exactlyEquals(Coordinate{0, 0, 1}));
    EXPECT_EQ(s.getRocketWorldPosition(), (WorldCoordinate{1, 2, 3}));
    EXPECT_TRUE(s.getRocketVelocity().exactlyEquals(Coordinate{0, 0, 10}));
    EXPECT_EQ(s.getRocketOrientationQuaternion(), Quaternion{});
    EXPECT_EQ(s.getEffectiveLaunchRodLength(), 1.0);
    EXPECT_EQ(s.getSimulationStartWallTime(), SimulationStatus::WallClock::time_point{});
    EXPECT_EQ(s.getMaxAlt(), 1.0);
    EXPECT_EQ(s.getMaxAltTime(), 1.0);
}

TEST(SimulationStatusModId, TheOtherChangesDrawNone)
{
    // "setConfiguration: same", "setFlightDataBranch: same", "setRocketRotationVelocity: same",
    // "setWarnings: same", "addWarning: same", "addWarnings: same", "setSimulationConditions:
    // same", "putExtraData: same", "setSeparatedStage: same", "addEvent: same",
    // "abortSimulation: same", "eventQueue.poll: same", "removeUnattachedEvents: same",
    // "deployed.add: same", "copyProperties: same", "motor ignite: same", "tumble update: same",
    // "configuration.setOnlyStage: same"
    TestStatus                   fixture;
    SimulationStatus&            s = fixture.status;
    ModIdWatch<SimulationStatus> watch(s);
    s.setConfiguration(s.getConfigurationPointer());
    EXPECT_FALSE(watch.drew());
    s.setFlightDataBranch(newBranch("c"));
    EXPECT_FALSE(watch.drew());
    s.setRocketRotationVelocity(Coordinate{1, 0, 0});
    EXPECT_FALSE(watch.drew());
    EXPECT_TRUE(s.getRocketRotationVelocity().exactlyEquals(Coordinate{1, 0, 0}));
    s.setWarnings(std::make_shared<WarningSet>());
    EXPECT_FALSE(watch.drew());
    s.addWarning(Warning::kListenersAffected);
    EXPECT_FALSE(watch.drew());
    s.addWarnings(WarningSet{});
    EXPECT_FALSE(watch.drew());
    s.setSimulationConditions(std::make_shared<SimulationConditions>());
    EXPECT_FALSE(watch.drew());
    s.putExtraData("a", 1);
    EXPECT_FALSE(watch.drew());
    s.setSeparatedStage(true);
    EXPECT_FALSE(watch.drew());
    EXPECT_TRUE(s.isSeparatedStage());
    s.addEvent({FlightEvent::Type::LIFTOFF, 1.0});
    EXPECT_FALSE(watch.drew());
    s.abortSimulation(SimulationAbort::Cause::NO_LIFTOFF);
    EXPECT_FALSE(watch.drew());
    static_cast<void>(s.getEventQueue().poll());
    EXPECT_FALSE(watch.drew());
    s.removeUnattachedEvents();
    EXPECT_FALSE(watch.drew());
    s.getDeployedRecoveryDevices().add(nullptr);
    EXPECT_FALSE(watch.drew());
    s.copyProperties(s);
    EXPECT_FALSE(watch.drew());
    s.getMotors().front()->ignite(0);
    EXPECT_FALSE(watch.drew());
    s.getTumbleDetector().update(1, true, 1, 100, 1, 10);
    EXPECT_FALSE(watch.drew());
    s.getConfiguration().setOnlyStage(0);
    EXPECT_FALSE(watch.drew());
    static_cast<void>(s.clone());
    EXPECT_FALSE(watch.drew());
    EXPECT_EQ(s.getModId(), ModId::invalid());
}

TEST(SimulationStatusModId, StoreDataDrawsOneOnlyWhenTheMaximumZVelocityRises)
{
    // "storeData rising: new", "storeData not rising: same"
    TestStatus                   fixture;
    SimulationStatus&            s = fixture.status;
    ModIdWatch<SimulationStatus> watch(s);
    s.storeData();
    EXPECT_TRUE(watch.drew()) << "from minus infinity to 0";
    EXPECT_EQ(s.getMaxZVelocity(), 0.0);
    s.storeData();
    EXPECT_FALSE(watch.drew());
    s.setRocketVelocity(Coordinate{0, 0, -3});
    EXPECT_TRUE(watch.drew());
    s.storeData();
    EXPECT_FALSE(watch.drew()) << "falling";
    EXPECT_EQ(s.getMaxZVelocity(), 0.0);
}

// ============================================================================ the warnings

TEST(SimulationStatusWarnings, AWarningOfANewKindAddsASimWarnEvent)
{
    // "== warnings": "events 1 set 1", "again: events 1 set 1", "aoa: events 2 set 2", "larger
    // aoa: events 2 set 2", "smaller aoa: events 2 set 2", "event SIM_WARN t=0.5 source null
    // data \"Listeners modified the flight simulation\" class Other in set true", "event
    // SIM_WARN t=1.0 source null data \"Large angle of attack encountered (28.6°)\" class
    // LargeAOA in set true"
    TestStatus              fixture;
    SimulationStatus&       s      = fixture.status;
    const FlightDataBranch& branch = *fixture.branch;
    const WarningSet&       set    = *s.getWarnings();

    s.setSimulationTime(0.5);
    s.addWarning(Warning::kListenersAffected);
    EXPECT_EQ(branch.getEvents().size(), 1U);
    EXPECT_EQ(set.size(), 1U);
    s.setSimulationTime(0.75);
    s.addWarning(Warning::kListenersAffected);
    EXPECT_EQ(branch.getEvents().size(), 1U) << "the set had it";
    EXPECT_EQ(set.size(), 1U);
    s.setSimulationTime(1.0);
    s.addWarning(Warning::LargeAOA(0.3));
    EXPECT_EQ(branch.getEvents().size(), 2U);
    EXPECT_EQ(set.size(), 2U);
    s.setSimulationTime(1.25);
    s.addWarning(Warning::LargeAOA(0.5));
    s.addWarning(Warning::LargeAOA(0.4));
    EXPECT_EQ(branch.getEvents().size(), 2U) << "a larger angle replaces, a smaller is dropped";
    EXPECT_EQ(set.size(), 2U);

    const FlightEvent& first = branch.getEvents()[0];
    EXPECT_EQ(first.getType(), FlightEvent::Type::SIM_WARN);
    EXPECT_EQ(first.getTime(), 0.5);
    EXPECT_FALSE(first.hasSource());
    ASSERT_NE(first.getWarning(), nullptr);
    EXPECT_EQ(first.getWarning()->toString(), "Listeners modified the flight simulation");
    EXPECT_EQ(first.getWarning()->typeName(), "Other");
    EXPECT_EQ(set.findById(first.getWarning()->id()), set.find(Warning::kListenersAffected))
        << "the event names the warning as the set stored it";

    const FlightEvent& second = branch.getEvents()[1];
    EXPECT_EQ(second.getTime(), 1.0);
    ASSERT_NE(second.getWarning(), nullptr);
    EXPECT_EQ(second.getWarning()->typeName(), "LargeAOA");
    // Java's event holds the set's object, which shows the larger angle; here the event holds
    // the warning as it was, and the set is asked for what it is now.
    const Warning* now = set.findById(second.getWarning()->id());
    ASSERT_NE(now, nullptr);
    EXPECT_EQ(now->toString(), "Large angle of attack encountered (28.6°)");
    // "set: Messages[Listeners modified the flight simulation,Large angle of attack encountered
    // (28.6°)]"
    EXPECT_EQ(set.toString(),
              "Messages[Listeners modified the flight simulation,Large angle of "
              "attack encountered (28.6°)]");
}

TEST(SimulationStatusWarnings, AddWarningsAddsEachAndASetThatWasTakenAwayIsMadeAnew)
{
    // "addWarnings: events 4 set 4", "null warnings true", "after null: events 5 set 1"
    TestStatus        fixture;
    SimulationStatus& s = fixture.status;
    s.addWarning(Warning::kListenersAffected);
    s.addWarning(Warning::LargeAOA(0.3));

    WarningSet more;
    more.add(Warning::kNoRecoveryDevice);
    more.add(Warning::kListenersAffected);
    more.add(Warning::RecoveryDrogueWithoutMain());
    s.addWarnings(more);
    EXPECT_EQ(fixture.branch->getEvents().size(), 4U);
    EXPECT_EQ(s.getWarnings()->size(), 4U);
    // A status may be given its own warnings: nothing is new.
    s.addWarnings(*s.getWarnings());
    EXPECT_EQ(fixture.branch->getEvents().size(), 4U);

    s.setWarnings(nullptr);
    EXPECT_EQ(s.getWarnings(), nullptr);
    s.addWarning(Warning::kListenersAffected);
    ASSERT_NE(s.getWarnings(), nullptr);
    EXPECT_EQ(s.getWarnings()->size(), 1U);
    EXPECT_EQ(fixture.branch->getEvents().size(), 5U) << "new to the new set";
}

TEST(SimulationStatusWarnings, TheWarningsGoToTheSetTheStatusWasGiven)
{
    // What the engine does: every status of a run writes to the warning set of the flight data.
    TestStatus        fixture;
    const auto        shared = std::make_shared<WarningSet>();
    SimulationStatus& s      = fixture.status;
    s.setWarnings(shared);
    EXPECT_EQ(s.getWarnings(), shared);
    s.addWarning(Warning::kNoRecoveryDevice);
    EXPECT_TRUE(shared->contains(Warning::kNoRecoveryDevice));
    SimulationStatus other(s);
    other.setWarnings(shared);
    other.addWarning(Warning::kNoRecoveryDevice);
    EXPECT_EQ(fixture.branch->getEvents().size(), 1U) << "already in the shared set";
}

TEST(SimulationStatusWarnings, AWarningOrAStoredPointNeedsAFlightDataBranch)
{
    // "bare: NullPointerException set size 1", "bare storeData: NullPointerException"
    const ProbeRocket probe("alpha");
    SimulationStatus  bare = probe.status();
    EXPECT_EQ(bugText([&] { bare.addWarning(Warning::kListenersAffected); }),
              "A warning was added to a simulation status without a flight data branch");
    EXPECT_EQ(bare.getWarnings()->size(), 1U) << "the warning went into the set first";
    // The set has it now, so adding it again adds no event and needs no branch.
    EXPECT_NO_THROW(bare.addWarning(Warning::kListenersAffected));
    EXPECT_EQ(bugText([&] { bare.storeData(); }),
              "A simulation status without a flight data branch cannot store its data");
}

TEST(SimulationStatusWarnings, RecordWarningsWaitsForTheRodAndStopsWhenTheRocketSlows)
{
    // "== recordWarnings"
    TestStatus        fixture;
    SimulationStatus& s = fixture.status;
    EXPECT_EQ(SimulationStatus::kWarningsWait, 0.25);
    EXPECT_EQ(SimulationStatus::kWarningsVel, 0.2);
    EXPECT_FALSE(s.recordWarnings()) << "new false";
    s.setSimulationTime(2000);
    EXPECT_FALSE(s.recordWarnings()) << "not cleared at 2000 false";
    s.setSimulationTime(1.0);
    s.setLaunchRodCleared(true);
    EXPECT_EQ(s.getStartWarningsTime(), 1.25);
    EXPECT_FALSE(s.recordWarnings()) << "cleared at 1.0: false";
    s.setSimulationTime(1.2499);
    EXPECT_FALSE(s.recordWarnings()) << "at 1.2499: false";
    s.setSimulationTime(1.25);
    EXPECT_TRUE(s.recordWarnings()) << "at 1.25 (max -inf, vz 0): true";
    s.setRocketVelocity(Coordinate{0, 0, 10});
    s.storeData();
    EXPECT_EQ(s.getMaxZVelocity(), 10.0);
    EXPECT_TRUE(s.recordWarnings()) << "after storeData vz=10: max 10.0 true";
    s.setRocketVelocity(Coordinate{0, 0, 2.0});
    EXPECT_TRUE(s.recordWarnings()) << "vz=2.0: true (not below 20 %)";
    s.setRocketVelocity(Coordinate{0, 0, 1.9999});
    EXPECT_FALSE(s.recordWarnings()) << "vz=1.9999: false";
    s.setRocketVelocity(Coordinate{0, 0, -5});
    s.storeData();
    EXPECT_EQ(s.getMaxZVelocity(), 10.0);
    EXPECT_FALSE(s.recordWarnings()) << "after storeData vz=-5: max 10.0 false";
    s.setLaunchRodCleared(false);
    s.setRocketVelocity(Coordinate{0, 0, 10});
    EXPECT_FALSE(s.recordWarnings()) << "uncleared: false";
    EXPECT_EQ(s.getStartWarningsTime(), 1.25) << "un-clearing the rod keeps the time";
    s.setSimulationTime(5);
    s.setLaunchRodCleared(true);
    s.setSimulationTime(5.2);
    EXPECT_FALSE(s.recordWarnings()) << "cleared again at 5, now 5.2: false";
    s.setSimulationTime(5.25);
    EXPECT_TRUE(s.recordWarnings()) << "5.25: true";
    s.setRocketVelocity(Coordinate{0, 0, std::numeric_limits<double>::quiet_NaN()});
    EXPECT_TRUE(s.recordWarnings()) << "vz NaN: true";
}

// ============================================================================ the events

TEST(SimulationStatusEvents, AddEventAsksTheListenersAndAbortSimulationAddsAnAbort)
{
    // "== events": "queue after LIFTOFF and vetoed TUMBLE: LIFTOFF@1.0:- log [add LIFTOFF, add
    // TUMBLE] warnings Messages[Listeners modified the flight simulation] branch events 1",
    // "queue after abort: LIFTOFF@1.0:- SIM_ABORT@2.5:-", "abort event: FlightEvent[type=
    // SIM_ABORT,time=2.5,source=null,data=No motors defined in the simulation] cause
    // NO_MOTORS_DEFINED", "toEventDebug: ..."
    const ProbeRocket probe("multistage");
    const auto        conditions = std::make_shared<SimulationConditions>();
    const auto        listener   = std::make_shared<Counting>();
    conditions->getSimulationListenerList().push_back(listener);
    SimulationStatus s      = probe.status(conditions);
    const auto       branch = newBranch("b");
    s.setFlightDataBranch(branch);
    s.setSimulationTime(2.5);

    s.addEvent({FlightEvent::Type::LIFTOFF, 1.0});
    s.addEvent({FlightEvent::Type::TUMBLE, 1.0});
    EXPECT_EQ(queueText(s.getEventQueue()), "LIFTOFF@1.0:-") << "the listener refused the TUMBLE";
    EXPECT_EQ(*listener->log(), (std::vector<std::string>{"add LIFTOFF", "add TUMBLE"}));
    EXPECT_EQ(s.getWarnings()->toString(), "Messages[Listeners modified the flight simulation]");
    EXPECT_EQ(branch->getEvents().size(), 1U) << "the warning's event";

    s.abortSimulation(SimulationAbort::Cause::NO_MOTORS_DEFINED);
    EXPECT_EQ(queueText(s.getEventQueue()), "LIFTOFF@1.0:- SIM_ABORT@2.5:-");
    const FlightEvent& abort = *(s.getEventQueue().begin() + 1);
    EXPECT_EQ(abort.toString(),
              "FlightEvent[type=SIM_ABORT,time=2.5,source=null,data=No motors "
              "defined in the simulation]");
    ASSERT_NE(abort.getAbort(), nullptr);
    EXPECT_EQ(abort.getAbort()->cause(), SimulationAbort::Cause::NO_MOTORS_DEFINED);
    EXPECT_EQ(*listener->log(),
              (std::vector<std::string>{"add LIFTOFF", "add TUMBLE", "add SIM_ABORT"}))
        << "an abort is added like any event";

    EXPECT_EQ(s.toEventDebug(),
              "      [t:Lift-off @1.0]\n"
              "      [t:Simulation abort @2.5  data:SimulationAbort]\n");
}

/// The queue of the probe's removeUnattachedEvents() scenario: an ALTITUDE event from every
/// component of the multi-stage rocket, in tree order, at 1.0 s plus 0.125 s times the
/// component's number modulo 4, then a LIFTOFF without a source at 1.0 s and an APOGEE from the
/// rocket at 1.25 s.
void queueEventsOfEveryComponent(SimulationStatus& status)
{
    int n = 0;
    for (const RocketComponent* component : status.getConfiguration().getAllComponents())
    {
        status.getEventQueue().add(
            {FlightEvent::Type::ALTITUDE, 1.0 + (0.125 * (n % 4)), component});
        n++;
    }
    status.getEventQueue().add({FlightEvent::Type::LIFTOFF, 1.0});
    status.getEventQueue().add(
        {FlightEvent::Type::APOGEE, 1.25, &status.getConfiguration().getRocket()});
}

/// The events @p status polls until its queue is empty: "<TYPE>:<source name or ->".
[[nodiscard]] std::string pollAll(SimulationStatus& status)
{
    std::string line;
    while (const std::optional<FlightEvent> event = status.getEventQueue().poll())
    {
        if (!line.empty())
        {
            line += ' ';
        }
        line += name(event->getType());
        line += ':';
        line += event->getSource() == nullptr ? std::string{"-"} : event->getSource()->getName();
    }
    return line;
}

TEST(SimulationStatusEvents, RemoveUnattachedEventsDropsTheEventsOfTheStagesThatLeft)
{
    // "stages 3 components 12", "before: ...", "all attached: size 14 queue modID same true",
    // "after clearStagesBelow(1): ...", "booster after clearStagesAbove(1): ...", "polled: ..."
    const ProbeRocket probe("multistage");
    SimulationStatus  t = probe.status();
    ASSERT_EQ(t.getConfiguration().getAllComponents().size(), 12U);
    queueEventsOfEveryComponent(t);
    EXPECT_EQ(queueText(t.getEventQueue()),
              "LIFTOFF@1.0:- ALTITUDE@1.0:Sustainer Parachute ALTITUDE@1.0:Side boosters "
              "ALTITUDE@1.0:Rocket ALTITUDE@1.125:Side Booster Nose Cones ALTITUDE@1.125:Center "
              "Booster ALTITUDE@1.25:Center Booster Body Tube ALTITUDE@1.375:Sustainer Body Tube "
              "ALTITUDE@1.375:Center Booster Fin Set ALTITUDE@1.125:Sustainer "
              "ALTITUDE@1.25:Side Booster Body Tubes ALTITUDE@1.375:Side Chutes "
              "ALTITUDE@1.25:Sustainer Nose Cone APOGEE@1.25:Rocket");

    const ModId queueId = t.getEventQueue().modId();
    t.removeUnattachedEvents();
    EXPECT_EQ(t.getEventQueue().size(), 14U) << "every stage flies";
    EXPECT_EQ(t.getEventQueue().modId(), queueId);

    // A stage separates: the booster's status is a copy, then each loses the other's stages.
    SimulationStatus booster(t);
    t.getConfiguration().clearStagesBelow(1);
    t.removeUnattachedEvents();
    EXPECT_EQ(queueText(t.getEventQueue()),
              "LIFTOFF@1.0:- ALTITUDE@1.0:Sustainer Parachute ALTITUDE@1.25:Sustainer Nose Cone "
              "ALTITUDE@1.0:Rocket ALTITUDE@1.125:Sustainer ALTITUDE@1.375:Sustainer Body Tube "
              "APOGEE@1.25:Rocket");
    EXPECT_EQ(t.getEventQueue().modId(), queueId) << "removed without a new id, as in Java";

    booster.getConfiguration().clearStagesAbove(1);
    booster.removeUnattachedEvents();
    EXPECT_EQ(queueText(booster.getEventQueue()),
              "LIFTOFF@1.0:- ALTITUDE@1.0:Rocket ALTITUDE@1.0:Side boosters ALTITUDE@1.25:Side "
              "Booster Body Tubes ALTITUDE@1.125:Side Booster Nose Cones ALTITUDE@1.125:Center "
              "Booster ALTITUDE@1.25:Center Booster Body Tube APOGEE@1.25:Rocket "
              "ALTITUDE@1.375:Center Booster Fin Set ALTITUDE@1.375:Side Chutes");

    EXPECT_EQ(pollAll(t),
              "LIFTOFF:- ALTITUDE:Sustainer Parachute ALTITUDE:Rocket "
              "ALTITUDE:Sustainer ALTITUDE:Sustainer Nose Cone APOGEE:Rocket "
              "ALTITUDE:Sustainer Body Tube");
}

TEST(SimulationStatusEvents, AnEventIsAttachedWithoutASourceFromTheRocketOrFromAnActiveStage)
{
    const ProbeRocket          probe("multistage");
    SimulationStatus           s       = probe.status();
    const FlightConfiguration& config  = s.getConfiguration();
    const RocketComponent*     rocket  = &config.getRocket();
    const RocketComponent*     chute   = componentNamed(config, "Sustainer Parachute");
    const RocketComponent*     booster = componentNamed(config, "Center Booster Body Tube");
    const RocketComponent*     side    = componentNamed(config, "Side Chutes");
    ASSERT_NE(chute, nullptr);
    ASSERT_NE(booster, nullptr);
    ASSERT_NE(side, nullptr);
    const FlightEvent none(FlightEvent::Type::LIFTOFF, 1.0);
    const FlightEvent fromRocket(FlightEvent::Type::APOGEE, 1.0, rocket);
    const FlightEvent fromChute(FlightEvent::Type::ALTITUDE, 1.0, chute);
    const FlightEvent fromBooster(FlightEvent::Type::ALTITUDE, 1.0, booster);
    const FlightEvent fromSide(FlightEvent::Type::ALTITUDE, 1.0, side);
    // An event read from a file knows its source by id only: it has no component to detach.
    const FlightEvent byId(FlightEvent::Type::ALTITUDE, 1.0, booster->getId(), {});

    EXPECT_TRUE(s.isAttached(none));
    EXPECT_TRUE(s.isAttached(fromRocket));
    EXPECT_TRUE(s.isAttached(fromChute));
    EXPECT_TRUE(s.isAttached(fromBooster));
    EXPECT_TRUE(s.isAttached(fromSide));

    s.getConfiguration().setOnlyStage(0);
    EXPECT_TRUE(s.isAttached(none));
    EXPECT_TRUE(s.isAttached(fromRocket)) << "the rocket has no parent";
    EXPECT_TRUE(s.isAttached(fromChute));
    EXPECT_FALSE(s.isAttached(fromBooster));
    EXPECT_FALSE(s.isAttached(fromSide));
    EXPECT_TRUE(s.isAttached(byId));

    s.getConfiguration().setOnlyStage(2);
    EXPECT_FALSE(s.isAttached(fromChute));
    EXPECT_FALSE(s.isAttached(fromBooster));
    EXPECT_TRUE(s.isAttached(fromSide));
}

// ============================================================================ storeData()

/// The states of the probe's "storeData" table: the position (x, y, z), the velocity (x, y, z)
/// and the orientation (w, x, y, z).
constexpr std::array<std::array<double, 10>, 5> kStoredStates{{
    {0, 0, 0, 0, 0, 0, 1, 0, 0, 0},
    {3, 4, 100, 6, -8, 20, 0.9, 0.1, 0.3, 0.2},
    {-3, 4, 50, -1, 0, -7, 0.5, -0.5, 0.5, 0.5},
    {-2, -5, 10, 0, 0, 0, 0.7, 0.7, 0.1, -0.1},
    {0, -1, 0, 1, 1, 1, 0, 1, 0, 0},
}};

/// A column of the row storeData() writes, and whether its value goes through a function whose
/// last bits differ between math libraries (the angles go through atan2(), the latitude and
/// the longitude through the conversion to degrees).
struct StoredColumn
{
    constexpr StoredColumn(FlightDataTypeId columnType, bool columnExact) noexcept
      : type(columnType), exact(columnExact)
    {
    }

    FlightDataTypeId type;
    bool             exact;
};

/// The columns of the probe's rows, in its order (the fifteenth value is the maximum z
/// velocity of the status afterwards).
constexpr std::array<StoredColumn, 14> kStoredColumns{{
    {FlightDataTypeId::TYPE_TIME, true},
    {FlightDataTypeId::TYPE_ALTITUDE, true},
    {FlightDataTypeId::TYPE_ALTITUDE_ABOVE_SEA, true},
    {FlightDataTypeId::TYPE_POSITION_X, true},
    {FlightDataTypeId::TYPE_POSITION_Y, true},
    {FlightDataTypeId::TYPE_LATITUDE, false},
    {FlightDataTypeId::TYPE_LONGITUDE, false},
    {FlightDataTypeId::TYPE_POSITION_XY, true},
    {FlightDataTypeId::TYPE_POSITION_DIRECTION, false},
    {FlightDataTypeId::TYPE_VELOCITY_XY, true},
    {FlightDataTypeId::TYPE_VELOCITY_Z, true},
    {FlightDataTypeId::TYPE_VELOCITY_TOTAL, true},
    {FlightDataTypeId::TYPE_ORIENTATION_THETA, false},
    {FlightDataTypeId::TYPE_ORIENTATION_PHI, false},
}};

/// The rows Java stored for kStoredStates (kStoredColumns, then the maximum z velocity).
constexpr std::array<std::array<double, 15>, 5> kStoredRows{{
    {0.0, 0.0, 5.0, 0.0, 0.0, 28.500000000000004, -80.25, 0.0, 0.0, 0.0, 0.0, 0.0,
     1.5707963267948966, 0.0, 0.0},
    {0.25, 100.0, 105.0, 3.0, 4.0, 29.500000000000004, -81.25, 5.0, 0.6435011087932843, 10.0, 20.0,
     22.360679774997898, 0.9099510309719299, 1.6738779353175968, 20.0},
    {0.5, 50.0, 55.0, -3.0, 4.0, 30.5, -82.25, 5.0, 5.639684198386302, 1.0, -7.0,
     7.0710678118654755, 0.0, 0.0, 20.0},
    {0.75, 10.0, 15.0, -2.0, -5.0, 31.5, -83.25, 5.385164807134504, 3.522099030702158, 0.0, 0.0,
     0.0, 0.0, std::numbers::pi, 20.0},
    {1.0, 0.0, 5.0, 0.0, -1.0, 32.5, -84.25, 1.0, std::numbers::pi, std::numbers::sqrt2, 1.0,
     std::numbers::sqrt3, -1.5707963267948966, 0.0, 20.0},
}};

/// The last value of the builtin type @p id in @p branch.
[[nodiscard]] double last(const FlightDataBranch& branch, FlightDataTypeId id)
{
    return branch.getLast(FlightDataType::builtin(id));
}

/// Puts @p status in the probe's state number @p index, stores it, and gives the differences
/// between the stored point and Java's row.
[[nodiscard]] std::string storedRowDifferences(SimulationStatus&       status,
                                               const FlightDataBranch& branch, std::size_t index)
{
    const std::array<double, 10>& state = kStoredStates.at(index);
    const std::array<double, 15>& row   = kStoredRows.at(index);
    const auto                    n     = static_cast<double>(index);
    status.setSimulationTime(n * 0.25);
    status.setRocketPosition(Coordinate{state[0], state[1], state[2]});
    status.setRocketWorldPosition({28.5 + n, -80.25 - n, 5 + state[2]});
    status.setRocketVelocity(Coordinate{state[3], state[4], state[5]});
    status.setRocketOrientationQuaternion({state[6], state[7], state[8], state[9]});
    status.storeData();

    JavaValueDifferences differences;
    for (std::size_t i = 0; i < kStoredColumns.size(); i++)
    {
        const StoredColumn& column = kStoredColumns.at(i);
        const std::string   field =
            std::format("row {} {}", index, FlightDataType::builtin(column.type).getName());
        if (column.exact)
        {
            // Stored as it is, or made with +, -, *, / and sqrt: Java's value exactly.
            differences.pinned(field, row.at(i), last(branch, column.type), 0.0);
        }
        else
        {
            // An angle that Java rounds to 0 may come out a rounding step from 0.
            differences.number(field, row.at(i), last(branch, column.type));
        }
    }
    differences.pinned(std::format("row {} maximum z velocity", index), row.at(14),
                       status.getMaxZVelocity(), 0.0);
    if (!(last(branch, FlightDataTypeId::TYPE_COMPUTATION_TIME) >= 0.0))
    {
        differences.problem(std::format("row {}: the computation time is not a time", index));
    }
    return differences.text();
}

TEST(SimulationStatusStoreData, StoresAPointWithJavasValues)
{
    // "== storeData": five states, the row each stores, and the maximum z velocity afterwards.
    TestStatus fixture;
    EXPECT_EQ(storedRowDifferences(fixture.status, *fixture.branch, 0), "");
    EXPECT_EQ(storedRowDifferences(fixture.status, *fixture.branch, 1), "");
    EXPECT_EQ(storedRowDifferences(fixture.status, *fixture.branch, 2), "");
    EXPECT_EQ(storedRowDifferences(fixture.status, *fixture.branch, 3), "");
    EXPECT_EQ(storedRowDifferences(fixture.status, *fixture.branch, 4), "");
    EXPECT_EQ(fixture.branch->getLength(), 5U);
}

TEST(SimulationStatusStoreData, StoresTheseColumnsAndNoOthers)
{
    // "types: Time | Altitude | Altitude above sea level | Vertical velocity | Total velocity |
    // Position East of launch | Position North of launch | Lateral distance | Lateral direction
    // | Lateral velocity | Latitude | Longitude | Vertical orientation (zenith) | Lateral
    // orientation (azimuth) | Computation time", "rows 5 computation time >= 0 true"
    TestStatus fixture;
    fixture.status.storeData();
    std::vector<std::string> names;
    for (const FlightDataType* type : fixture.branch->getTypes())
    {
        names.push_back(type->getName());
    }
    EXPECT_EQ(names, (std::vector<std::string>{
                         "Time", "Altitude", "Altitude above sea level", "Vertical velocity",
                         "Total velocity", "Position East of launch", "Position North of launch",
                         "Lateral distance", "Lateral direction", "Lateral velocity", "Latitude",
                         "Longitude", "Vertical orientation (zenith)",
                         "Lateral orientation (azimuth)", "Computation time"}));
}

TEST(SimulationStatusStoreData, TheComputationTimeIsTheTimeSinceTheWallClockStart)
{
    TestStatus        fixture;
    SimulationStatus& s = fixture.status;
    s.setSimulationStartWallTime(SimulationStatus::WallClock::now() - std::chrono::seconds(100));
    s.storeData();
    const double seconds = last(*fixture.branch, FlightDataTypeId::TYPE_COMPUTATION_TIME);
    EXPECT_GE(seconds, 100.0);
    EXPECT_LT(seconds, 160.0);
}

TEST(SimulationStatusStoreData, AnImmutableBranchRefusesThePoint)
{
    TestStatus fixture;
    fixture.branch->immute();
    EXPECT_THROW(fixture.status.storeData(), QtRocket::BugError);
}

// ============================================================================ extra data

TEST(SimulationStatusExtraData, KeepsValuesByKey)
{
    TestStatus        fixture;
    SimulationStatus& s = fixture.status;
    EXPECT_EQ(s.getExtraData("key"), nullptr);
    s.putExtraData("key", std::string{"value"});
    s.putExtraData("number", 42);
    ASSERT_NE(s.getExtraData("key"), nullptr);
    EXPECT_EQ(std::any_cast<std::string>(*s.getExtraData("key")), "value");
    EXPECT_EQ(std::any_cast<int>(*s.getExtraData("number")), 42);
    s.putExtraData("key", 1.5);
    EXPECT_EQ(std::any_cast<double>(*s.getExtraData("key")), 1.5) << "replaced";
    // Java: putExtraData(key, null), which getExtraData() cannot tell from nothing.
    s.putExtraData("key", std::any{});
    ASSERT_NE(s.getExtraData("key"), nullptr);
    EXPECT_FALSE(s.getExtraData("key")->has_value());
    const SimulationStatus& constant = s;
    EXPECT_EQ(constant.getExtraData("number"), s.getExtraData("number"));
    EXPECT_EQ(constant.getExtraData("missing"), nullptr);
}

// ============================================================================ the debug dumps

TEST(SimulationStatusDebug, TheEventDumpNamesTypeTimeSourceAndTheClassOfTheData)
{
    // "== debug": "empty toEventDebug []", and the dump of a queue with data of every kind, in
    // the queue's array order.
    TestStatus        fixture;
    SimulationStatus& s = fixture.status;
    EXPECT_EQ(s.toEventDebug(), "");

    const std::shared_ptr<MotorClusterState> state  = s.getMotors().front();
    const Rocket*                            rocket = &s.getConfiguration().getRocket();
    state->ignite(0.25);
    s.getEventQueue().add(
        {FlightEvent::Type::IGNITION, 0.25, &QtRocket::asComponent(state->getMount()), state});
    s.getEventQueue().add({FlightEvent::Type::ALTITUDE, 0.5, rocket,
                           FlightEvent::AltitudeChange{.previous = 1.0, .current = 2.5}});
    s.getEventQueue().add({FlightEvent::Type::SIM_WARN, 0.75, nullptr,
                           FlightEvent::warningData(Warning::LargeAOA(0.5))});
    s.getEventQueue().add({FlightEvent::Type::SIM_WARN, 0.8, nullptr,
                           FlightEvent::warningData(Warning::kListenersAffected)});
    s.getEventQueue().add({FlightEvent::Type::SIM_ABORT, 1.0, nullptr,
                           SimulationAbort(SimulationAbort::Cause::NO_CP)});
    s.getEventQueue().add({FlightEvent::Type::EXCEPTION, 1.5, rocket, std::string{"boom"}});
    s.getEventQueue().add({FlightEvent::Type::RECOVERY_DEVICE_DEPLOYMENT, 1.0E-4, nullptr});
    s.getEventQueue().add({FlightEvent::Type::APOGEE, 12345678.0, rocket});

    EXPECT_EQ(s.toEventDebug(),
              "      [t:Recovery device deployment @1.0E-4]\n"
              "      [t:Altitude change @0.5  src:Estes Alpha III / Code Verification Rocket  "
              "data:Pair]\n"
              "      [t:Motor ignition @0.25  src:Motor Mount Tube  data:MotorClusterState]\n"
              "      [t:Warning @0.8  data:Other]\n"
              "      [t:Simulation abort @1.0  data:SimulationAbort]\n"
              "      [t:Exception @1.5  src:Estes Alpha III / Code Verification Rocket  "
              "data:String]\n"
              "      [t:Warning @0.75  data:LargeAOA]\n"
              "      [t:Apogee @1.2345678E7  src:Estes Alpha III / Code Verification Rocket]\n");
}

TEST(SimulationStatusDebug, TheMotorDumpListsTheDescriptionOfEveryState)
{
    // "MotorState list:" / "          [       Motor Mount Tube/2cf0bab9 /   A8 - Armed]", and
    // "- Thrusting" once the motor is lit (the eight characters are of the mount's random id).
    TestStatus                               fixture;
    SimulationStatus&                        s     = fixture.status;
    const std::shared_ptr<MotorClusterState> state = s.getMotors().front();
    EXPECT_EQ(s.toMotorsDebug(), "MotorState list:\n          [" + state->toDescription() + "]\n");
    EXPECT_TRUE(
        s.toMotorsDebug().starts_with("MotorState list:\n          [       Motor Mount Tube/"));
    EXPECT_TRUE(s.toMotorsDebug().ends_with(" /   A8 - Armed]\n"));
    state->ignite(0.25);
    EXPECT_TRUE(s.toMotorsDebug().ends_with(" /   A8 - Thrusting]\n"));

    const ProbeRocket      probe("multistage");
    const SimulationStatus three    = probe.status();
    std::string            expected = "MotorState list:\n";
    for (const std::shared_ptr<MotorClusterState>& motor : three.getMotors())
    {
        expected += "          [" + motor->toDescription() + "]\n";
    }
    EXPECT_EQ(three.toMotorsDebug(), expected);
}

}  // namespace
