#include "QtRocket/simulation/extension/example/AirStart.h"

#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/example/AirStartProvider.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Signal.h"
#include "rocket/JavaValueDifferences.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationStatusSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::AirStart;
using QtRocket::AirStartProvider;
using QtRocket::Config;
using QtRocket::Coordinate;
using QtRocket::SimulationConditions;
using QtRocket::SimulationExtension;
using QtRocket::SimulationListener;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::TestStatus;

// OpenRocket has no test of AirStart. The expectations here are the Java source's, and for the
// names the output of a Java probe on OpenRocket's compiled core (probes/tier9a-extensions/java:
// ExtPins2.java, the "airname" lines), for the listener on a tilted launch rod that of another
// (probes/tier9a-fix-extensions/java: FixHooks.java, the "AIRSTART" lines of
// out/fixhooks-java.txt). The flights are in extension_flight_tests.cpp.

static_assert(std::is_final_v<AirStart>);
static_assert(!std::is_copy_assignable_v<AirStart>);

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// Counts the emissions of a signal while it lives.
class ChangeCounter
{
public:
    explicit ChangeCounter(QtRocket::Signal<>& signal)
      : m_connection(signal.connect([this] { ++m_count; }))
    {
    }

    [[nodiscard]] int count() const noexcept { return m_count; }

private:
    mutable int                          m_count{0};
    QtRocket::Signal<>::ScopedConnection m_connection;
};

/// The name of an AirStart with the launch altitude @p altitude and the launch velocity
/// @p velocity.
[[nodiscard]] std::string nameOf(double altitude, double velocity)
{
    AirStart airStart;
    airStart.setLaunchAltitude(altitude);
    airStart.setLaunchVelocity(velocity);
    return airStart.getName();
}

TEST(AirStart, HasOpenRocketsIdAndDescription)
{
    const AirStart airStart;
    EXPECT_EQ(AirStart::kId, "info.openrocket.core.simulation.extension.example.AirStart");
    EXPECT_EQ(airStart.getId(), AirStart::kId);
    EXPECT_EQ(airStart.getDescription(),
              "Start simulation with a configurable altitude and velocity");
    EXPECT_TRUE(airStart.isMonteCarloSafe());
    EXPECT_TRUE(airStart.getFlightDataTypes().empty());
}

TEST(AirStart, HasJavasDefaults)
{
    const AirStart airStart;
    EXPECT_TRUE(airStart.getConfig().keySet().empty());
    EXPECT_EQ(airStart.getLaunchAltitude(), 100.0);
    EXPECT_EQ(airStart.getLaunchVelocity(), 50.0);
}

// ExtPins2, the "airname" lines: the name with the velocity while the velocity is above
// 0.01 m/s, and each value as its unit group prints it with its default unit.
TEST(AirStart, NamesItselfByItsConfiguration)
{
    const DefaultUnitsGuard units;
    EXPECT_EQ(AirStart().getName(), "Air-start (100 m, 50 m/s)");
    EXPECT_EQ(nameOf(100.0, 50.0), "Air-start (100 m, 50 m/s)");
    EXPECT_EQ(nameOf(100.0, 0.01), "Air-start (100 m)");
    EXPECT_EQ(nameOf(100.0, 0.011), "Air-start (100 m, 0.011 m/s)");
    EXPECT_EQ(nameOf(0.0, 0.0), "Air-start (0 m)");
    EXPECT_EQ(nameOf(1234.5678, 12.345), "Air-start (1235 m, 12.3 m/s)");
    EXPECT_EQ(nameOf(-5.0, -3.0), "Air-start (-5 m)");
    EXPECT_EQ(nameOf(100.0, kNaN), "Air-start (100 m)");
    EXPECT_EQ(nameOf(kNaN, 50.0), "Air-start (N/A, 50 m/s)");
    EXPECT_EQ(nameOf(30000.0, 343.0), "Air-start (30000 m, 343 m/s)");
}

TEST(AirStart, NamesItselfInTheDefaultUnits)
{
    const DefaultUnitsGuard units;
    ASSERT_TRUE(QtRocket::unitGroup(QtRocket::UnitGroupId::DISTANCE).setDefaultUnit("ft"));
    ASSERT_TRUE(QtRocket::unitGroup(QtRocket::UnitGroupId::VELOCITY).setDefaultUnit("km/h"));
    EXPECT_EQ(nameOf(100.0, 50.0), "Air-start (328 ft, 180 km/h)");
}

TEST(AirStart, EverySetterStoresItsKeyAndAnnouncesTheChange)
{
    AirStart            airStart;
    const ChangeCounter events(airStart.changed());

    airStart.setLaunchAltitude(250.0);
    airStart.setLaunchVelocity(20.0);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(airStart.getLaunchAltitude(), 250.0);
    EXPECT_EQ(airStart.getLaunchVelocity(), 20.0);

    // The keys a .ork file holds, with Java's type: two Doubles.
    const Config config = airStart.getConfig();
    EXPECT_EQ(config.keySet(), (std::vector<std::string>{"launchAltitude", "launchVelocity"}));
    EXPECT_TRUE(config.get("launchAltitude") == Config::Value{250.0});
    EXPECT_TRUE(config.get("launchVelocity") == Config::Value{20.0});

    // And back: another extension that is given this configuration has the two settings.
    AirStart other;
    other.setConfig(config);
    EXPECT_EQ(other.getLaunchAltitude(), 250.0);
    EXPECT_EQ(other.getLaunchVelocity(), 20.0);
    EXPECT_EQ(other.getConfig().keySet(), config.keySet());
    EXPECT_TRUE(other.getConfig().sameEntries(config));

    // A setter announces also a value that is the stored one, as in Java.
    airStart.setLaunchVelocity(20.0);
    EXPECT_EQ(events.count(), 3);
}

TEST(AirStart, ReadsItsSettingsFromAConfiguration)
{
    // What the .ork reader does: setConfig() with the entries of the file.
    Config config;
    config.put("launchAltitude", 300);               // an Integer
    config.put("launchVelocity", std::int64_t{75});  // a Long
    AirStart airStart;
    airStart.setConfig(config);
    EXPECT_EQ(airStart.getLaunchAltitude(), 300.0);
    EXPECT_EQ(airStart.getLaunchVelocity(), 75.0);

    // An entry that is no number is the default; the examples' AirStart has no entry at all.
    Config texts;
    texts.put("launchAltitude", "high");
    texts.put("launchVelocity", true);
    airStart.setConfig(texts);
    EXPECT_EQ(airStart.getLaunchAltitude(), 100.0);
    EXPECT_EQ(airStart.getLaunchVelocity(), 50.0);
    EXPECT_TRUE(airStart.getConfig().sameEntries(texts));
}

TEST(AirStart, CloneCopiesTheConfigurationAndNothingElse)
{
    AirStart airStart;
    airStart.setLaunchAltitude(250.0);
    const ChangeCounter events(airStart.changed());

    const std::unique_ptr<SimulationExtension> copy  = airStart.clone();
    auto*                                      clone = dynamic_cast<AirStart*>(copy.get());
    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->getId(), AirStart::kId);
    EXPECT_TRUE(clone->getConfig().sameEntries(airStart.getConfig()));
    EXPECT_TRUE(clone->isMonteCarloSafe());

    clone->setLaunchAltitude(1.0);
    EXPECT_EQ(airStart.getLaunchAltitude(), 250.0);
    EXPECT_EQ(events.count(), 0);
    airStart.setLaunchVelocity(2.0);
    EXPECT_EQ(clone->getLaunchVelocity(), 50.0);
}

// QtRocket's own, for Simulation::validateInputs(): the two numbers the listener takes, each
// called by its key in the configuration and by the name without the configuration in it, with
// the value the getter gives.
TEST(AirStart, ListsTheNumbersItsListenerReads)
{
    AirStart                                      airStart;
    std::vector<SimulationExtension::InputNumber> numbers = airStart.getInputNumbers();
    ASSERT_EQ(numbers.size(), 2U);
    EXPECT_EQ(numbers[0].what, "the 'launchAltitude' of the simulation extension 'Air-start'");
    EXPECT_EQ(numbers[0].value, 100.0);
    EXPECT_EQ(numbers[1].what, "the 'launchVelocity' of the simulation extension 'Air-start'");
    EXPECT_EQ(numbers[1].value, 50.0);

    airStart.setLaunchAltitude(kNaN);
    airStart.setLaunchVelocity(-std::numeric_limits<double>::infinity());
    numbers = airStart.getInputNumbers();
    ASSERT_EQ(numbers.size(), 2U);
    EXPECT_TRUE(std::isnan(numbers[0].value));
    EXPECT_EQ(numbers[1].value, -std::numeric_limits<double>::infinity());

    // An entry that is no number is the default, and the clone lists its own.
    Config texts;
    texts.put("launchAltitude", "NaN");
    texts.put("launchVelocity", 75);
    const std::unique_ptr<SimulationExtension> copy = airStart.clone();
    airStart.setConfig(texts);
    numbers = airStart.getInputNumbers();
    ASSERT_EQ(numbers.size(), 2U);
    EXPECT_EQ(numbers[0].value, 100.0);
    EXPECT_EQ(numbers[1].value, 75.0);
    ASSERT_EQ(copy->getInputNumbers().size(), 2U);
    EXPECT_TRUE(std::isnan(copy->getInputNumbers()[0].value));
}

TEST(AirStartProvider, MakesAirStartsUnderOpenRocketsMenuName)
{
    const AirStartProvider provider;
    EXPECT_EQ(provider.getIds(), (std::vector<std::string>{std::string(AirStart::kId)}));
    EXPECT_EQ(provider.getName(AirStart::kId),
              (std::vector<std::string>{"Launch conditions", "Air-start"}));
    EXPECT_EQ(provider.getName("something.Else"), std::nullopt);

    const std::unique_ptr<SimulationExtension> made = provider.getInstance(AirStart::kId);
    ASSERT_NE(made, nullptr);
    EXPECT_NE(dynamic_cast<AirStart*>(made.get()), nullptr);
    EXPECT_TRUE(made->getConfig().keySet().empty());
    EXPECT_NE(provider.getInstance(AirStart::kId), made);
}

// The one hook: at the start of the simulation the rocket is put at the launch altitude, with
// the launch velocity along its axis as the status has it oriented. The status changes, which
// is what makes the engine add its "listeners affected the simulation" warning.
TEST(AirStartListener, PutsTheRocketInTheAirWhenTheSimulationStarts)
{
    TestStatus s;
    AirStart   airStart;
    airStart.setLaunchAltitude(250.0);
    airStart.setLaunchVelocity(20.0);
    airStart.initialize(*s.conditions);
    ASSERT_EQ(s.conditions->getSimulationListenerList().size(), 1U);
    const std::shared_ptr<SimulationListener> listener =
        s.conditions->getSimulationListenerList().front();
    EXPECT_FALSE(listener->isSystemListener());

    const Coordinate expectedVelocity =
        s.status.getRocketOrientationQuaternion().rotate(Coordinate(0, 0, 20.0));
    const QtRocket::ModId before = s.status.getModId();
    listener->startSimulation(s.status);

    EXPECT_EQ(s.status.getRocketPosition(), Coordinate(0, 0, 250.0));
    EXPECT_EQ(s.status.getRocketVelocity().x, expectedVelocity.x);
    EXPECT_EQ(s.status.getRocketVelocity().y, expectedVelocity.y);
    EXPECT_EQ(s.status.getRocketVelocity().z, expectedVelocity.z);
    // On the default conditions the rod is vertical: straight up.
    EXPECT_NEAR(s.status.getRocketVelocity().z, 20.0, 1e-12);
    EXPECT_NE(s.status.getModId(), before);

    // No other hook does anything.
    const QtRocket::ModId started = s.status.getModId();
    EXPECT_TRUE(listener->preStep(s.status));
    listener->postStep(s.status);
    listener->endSimulation(s.status, nullptr);
    EXPECT_EQ(s.status.getModId(), started);
}

/// A simulation status of the Estes Alpha III (TEST_FCID_0) on conditions whose launch rod is
/// tilted, moved off the origin and given a velocity, as FixHooks.java makes one: the status
/// takes its orientation from the launch rod of the conditions it is made with.
struct TiltedStatus
{
    QtRocket::Test::TestEstesAlphaIII     alpha;
    std::shared_ptr<SimulationConditions> conditions;
    QtRocket::SimulationStatus            status;

    TiltedStatus(double rodAngle, double rodDirection)
      : conditions(conditionsWith(rodAngle, rodDirection)),
        status(QtRocket::Test::statusConfiguration(*alpha.rocket, QtRocket::Test::testFcid(0)),
               conditions)
    {
        status.setRocketPosition(Coordinate(3, -4, 5));
        status.setRocketVelocity(Coordinate(1, 2, 3));
    }

    [[nodiscard]] static std::shared_ptr<SimulationConditions> conditionsWith(double rodAngle,
                                                                              double rodDirection)
    {
        std::shared_ptr<SimulationConditions> made = std::make_shared<SimulationConditions>();
        made->setLaunchRodAngle(rodAngle);
        made->setLaunchRodDirection(rodDirection);
        return made;
    }

    /// Starts the listener of an AirStart with the launch altitude @p altitude and the launch
    /// velocity @p velocity on the status.
    void airStart(double altitude, double velocity)
    {
        AirStart extension;
        extension.setLaunchAltitude(altitude);
        extension.setLaunchVelocity(velocity);
        extension.initialize(*conditions);
        conditions->getSimulationListenerList().back()->startSimulation(status);
    }
};

/// Whether @p actual is the vector (@p x, @p y, @p z) that the Java probe printed, each
/// component within 1e-12 of itself: the components went through sin() and cos().
[[nodiscard]] ::testing::AssertionResult isJavaVector(const Coordinate& actual, double x, double y,
                                                      double z)
{
    if (QtRocket::Test::matchesPinnedValue(x, actual.x) &&
        QtRocket::Test::matchesPinnedValue(y, actual.y) &&
        QtRocket::Test::matchesPinnedValue(z, actual.z))
    {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure()
           << std::format("expected ({}, {}, {}) (Java), got ({}, {}, {})", x, y, z, actual.x,
                          actual.y, actual.z);
}

// FixHooks: AIRSTART tilted and tilted-back. On a launch rod that is not vertical the launch
// velocity points along the rod: it is turned by the orientation of the rocket, and is not
// (0, 0, velocity). The position is exactly above the launch site, whatever it was before: x
// and y are set to 0 too. A negative launch velocity points down the rod.
TEST(AirStartListener, TurnsTheLaunchVelocityByTheOrientationOfTheRocket)
{
    TiltedStatus tilted(0.3, 1.0);
    ASSERT_TRUE(QtRocket::Test::isJavaQuaternion(tilted.status.getRocketOrientationQuaternion(),
                                                 0.9887710779360422, -0.08074176756011418,
                                                 0.12574785250041243, 0.0));
    tilted.airStart(250.0, 20.0);
    EXPECT_EQ(tilted.status.getRocketPosition(), Coordinate(0, 0, 250.0));
    EXPECT_TRUE(isJavaVector(tilted.status.getRocketVelocity(), 4.973433586599009,
                             3.1934049817950187, 19.106729782512115));
    EXPECT_NEAR(tilted.status.getRocketVelocity().length(), 20.0, 1e-13);

    TiltedStatus back(-0.5, 4.0);
    ASSERT_TRUE(QtRocket::Test::isJavaQuaternion(back.status.getRocketOrientationQuaternion(),
                                                 0.9689124217106446, -0.16171401974311986,
                                                 0.1872359337128839, 0.0));
    back.airStart(1000.0, -7.5);
    EXPECT_EQ(back.status.getRocketPosition(), Coordinate(0, 0, 1000.0));
    EXPECT_TRUE(isJavaVector(back.status.getRocketVelocity(), -2.7212283294750614,
                             -2.3503008374080387, -6.581869214177793));

    // FixHooks: AIRSTART vertical. Straight up, from wherever the status was.
    TiltedStatus vertical(0.0, 0.0);
    vertical.airStart(250.0, 20.0);
    EXPECT_EQ(vertical.status.getRocketPosition(), Coordinate(0, 0, 250.0));
    EXPECT_EQ(vertical.status.getRocketVelocity().x, 0.0);
    EXPECT_EQ(vertical.status.getRocketVelocity().y, 0.0);
    EXPECT_NEAR(vertical.status.getRocketVelocity().z, 20.0, 1e-13);
}

// The listener has the settings of the extension as they were when initialize() made it, and a
// clone of the conditions has a listener that does the same.
TEST(AirStartListener, KeepsTheSettingsItWasMadeWith)
{
    TestStatus s;
    AirStart   airStart;
    airStart.initialize(*s.conditions);
    airStart.setLaunchAltitude(9999.0);

    const SimulationConditions cloned = s.conditions->clone();
    ASSERT_EQ(cloned.getSimulationListenerList().size(), 1U);
    ASSERT_NE(cloned.getSimulationListenerList().front(),
              s.conditions->getSimulationListenerList().front());
    cloned.getSimulationListenerList().front()->startSimulation(s.status);
    EXPECT_EQ(s.status.getRocketPosition(), Coordinate(0, 0, 100.0));

    // An extension initialised later has the new setting.
    SimulationConditions later;
    airStart.initialize(later);
    later.getSimulationListenerList().front()->startSimulation(s.status);
    EXPECT_EQ(s.status.getRocketPosition(), Coordinate(0, 0, 9999.0));
}

}  // namespace
