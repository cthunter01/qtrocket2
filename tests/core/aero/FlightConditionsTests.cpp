#include "QtRocket/aero/FlightConditions.h"

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Monitorable.h"
#include "QtRocket/util/Signal.h"

namespace
{

using QtRocket::AtmosphericConditions;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentAssembly;
using QtRocket::Coordinate;
using QtRocket::FlightConditions;
using QtRocket::FlightConfiguration;
using QtRocket::ModId;
using QtRocket::Rocket;

using Areas = std::vector<FlightConditions::NozzleExitArea>;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kPi  = std::numbers::pi;

static_assert(QtRocket::Monitorable<FlightConditions>);

/// FlightConditionsTest's fixture: a new rocket (one stage) with a body tube of length 1 m and
/// outer radius 0.05 m in the stage, and conditions for a configuration of it.
class FlightConditionsTest : public ::testing::Test
{
protected:
    FlightConditionsTest()
    {
        m_stage    = &m_rocket->addChild(std::make_unique<AxialStage>());
        m_bodyTube = &m_stage->addChild(std::make_unique<BodyTube>(1.0, 0.05));
        m_rocket->enableEvents();
        m_config     = std::make_unique<FlightConfiguration>(*m_rocket);
        m_conditions = FlightConditions{*m_config};
    }

    /// The body tube's assembly (Java: bodyTube.getAssembly()).
    [[nodiscard]] const ComponentAssembly& assembly() const { return m_bodyTube->getAssembly(); }

    std::unique_ptr<Rocket>              m_rocket = std::make_unique<Rocket>();
    AxialStage*                          m_stage{nullptr};
    BodyTube*                            m_bodyTube{nullptr};
    std::unique_ptr<FlightConfiguration> m_config;
    FlightConditions                     m_conditions;
};

/// Counts the change events of @p conditions while it lives.
class EventCounter
{
public:
    explicit EventCounter(FlightConditions& conditions)
      : m_connection(conditions.changed().connect([this] { ++m_count; }))
    {
    }

    [[nodiscard]] int count() const noexcept { return m_count; }

private:
    int                                  m_count{0};
    QtRocket::Signal<>::ScopedConnection m_connection;
};

// ---- Ported from FlightConditionsTest.java ----

TEST_F(FlightConditionsTest, SetAndGetRefLength)
{
    const double expectedLength = m_bodyTube->getOuterRadius() * 2;
    EXPECT_NEAR(expectedLength, m_conditions.getRefLength(), 1e-6);

    m_conditions.setRefLength(2.0);
    EXPECT_NEAR(2.0, m_conditions.getRefLength(), 1e-6);
    EXPECT_NEAR(kPi, m_conditions.getRefArea(), 1e-6);
}

TEST_F(FlightConditionsTest, SetAndGetRefArea)
{
    // Get the actual reference area from FlightConditions
    const double actualRefArea = m_conditions.getRefArea();

    // Test that setting this area results in the same value
    m_conditions.setRefArea(actualRefArea);
    EXPECT_NEAR(actualRefArea, m_conditions.getRefArea(), 1e-6);

    // Test that setting a new area works correctly
    const double newArea = 4.0;
    m_conditions.setRefArea(newArea);
    EXPECT_NEAR(newArea, m_conditions.getRefArea(), 1e-6);
    EXPECT_NEAR(std::sqrt(newArea / kPi) * 2, m_conditions.getRefLength(), 1e-6);
}

TEST_F(FlightConditionsTest, SetAndGetThrustingNozzleExitArea)
{
    const double nozzleExitArea = kPi * std::pow(0.010 / 2, 2);

    const Areas areas{{&assembly(), nozzleExitArea}};
    m_conditions.setThrustingNozzleExitAreas(areas);

    EXPECT_NEAR(nozzleExitArea, m_conditions.getThrustingNozzleExitArea(), 1e-6);
    EXPECT_NEAR(nozzleExitArea, m_conditions.getThrustingNozzleExitArea(assembly()), 1e-6);
}

TEST_F(FlightConditionsTest, SetAndGetAOA)
{
    m_conditions.setAOA(kPi / 4);
    EXPECT_NEAR(kPi / 4, m_conditions.getAOA(), 1e-6);
    EXPECT_NEAR(std::sin(kPi / 4), m_conditions.getSinAOA(), 1e-6);
    EXPECT_NEAR(std::sin(kPi / 4) / (kPi / 4), m_conditions.getSincAOA(), 1e-6);
}

TEST_F(FlightConditionsTest, SetAndGetTheta)
{
    m_conditions.setTheta(kPi / 3);
    EXPECT_NEAR(kPi / 3, m_conditions.getTheta(), 1e-6);
}

TEST_F(FlightConditionsTest, SetAndGetMach)
{
    m_conditions.setMach(0.2);
    EXPECT_NEAR(0.2, m_conditions.getMach(), 1e-6);
    EXPECT_NEAR(0.9797958971, m_conditions.getBeta(), 1e-6);

    m_conditions.setMach(0.8);
    EXPECT_NEAR(0.8, m_conditions.getMach(), 1e-6);
    EXPECT_NEAR(0.6, m_conditions.getBeta(), 1e-6);

    m_conditions.setMach(0.9999999999);
    EXPECT_NEAR(0.9999999999, m_conditions.getMach(), 1e-6);
    EXPECT_NEAR(0.25, m_conditions.getBeta(), 1e-6);

    m_conditions.setMach(1.00000000001);
    EXPECT_NEAR(1.00000000001, m_conditions.getMach(), 1e-6);
    EXPECT_NEAR(0.25, m_conditions.getBeta(), 1e-6);

    m_conditions.setMach(1.3);
    EXPECT_NEAR(1.3, m_conditions.getMach(), 1e-6);
    EXPECT_NEAR(0.8306623863, m_conditions.getBeta(), 1e-6);

    m_conditions.setMach(3);
    EXPECT_NEAR(3, m_conditions.getMach(), 1e-6);
    EXPECT_NEAR(2.8284271247, m_conditions.getBeta(), 1e-6);
}

TEST_F(FlightConditionsTest, SetAndGetVelocity)
{
    const AtmosphericConditions atm;
    m_conditions.setAtmosphericConditions(atm);

    const double expectedMachSpeed = atm.getMachSpeed();
    m_conditions.setVelocity(expectedMachSpeed / 2);

    EXPECT_NEAR(0.5, m_conditions.getMach(), 1e-6);
    EXPECT_NEAR(expectedMachSpeed / 2, m_conditions.getVelocity(), 1e-6);
}

TEST_F(FlightConditionsTest, SetAndGetRollRate)
{
    m_conditions.setRollRate(5.0);
    EXPECT_NEAR(5.0, m_conditions.getRollRate(), 1e-6);
}

TEST_F(FlightConditionsTest, SetAndGetPitchRate)
{
    m_conditions.setPitchRate(2.5);
    EXPECT_NEAR(2.5, m_conditions.getPitchRate(), 1e-6);
}

TEST_F(FlightConditionsTest, SetAndGetYawRate)
{
    m_conditions.setYawRate(1.5);
    EXPECT_NEAR(1.5, m_conditions.getYawRate(), 1e-6);
}

TEST_F(FlightConditionsTest, SetAndGetPitchCenter)
{
    const Coordinate center{1.0, 2.0, 3.0};
    m_conditions.setPitchCenter(center);
    EXPECT_EQ(center, m_conditions.getPitchCenter());
}

TEST_F(FlightConditionsTest, Clone)
{
    m_conditions.setAOA(kPi / 6);
    m_conditions.setMach(0.7);
    m_conditions.setRollRate(3.0);
    const Areas areas{{&assembly(), 0.00025}};
    m_conditions.setThrustingNozzleExitAreas(areas);
    const AtmosphericConditions atm{280, 90000};
    m_conditions.setAtmosphericConditions(atm);

    FlightConditions cloned = m_conditions.clone();

    EXPECT_NE(&m_conditions, &cloned);
    EXPECT_NEAR(m_conditions.getAOA(), cloned.getAOA(), 1e-6);
    EXPECT_NEAR(m_conditions.getMach(), cloned.getMach(), 1e-6);
    EXPECT_NEAR(m_conditions.getRollRate(), cloned.getRollRate(), 1e-6);
    EXPECT_NEAR(m_conditions.getThrustingNozzleExitArea(), cloned.getThrustingNozzleExitArea(),
                1e-6);
    EXPECT_EQ(m_conditions.getThrustingNozzleExitAreas(), cloned.getThrustingNozzleExitAreas());
    cloned.setThrustingNozzleExitAreas(Areas{});
    EXPECT_NEAR(0.00025, m_conditions.getThrustingNozzleExitArea(assembly()), 1e-6)
        << "Changing a clone's wake areas must not modify the original conditions";
    EXPECT_NEAR(m_conditions.getAtmosphericConditions().getTemperature(),
                cloned.getAtmosphericConditions().getTemperature(), 1e-6);
    EXPECT_NEAR(m_conditions.getAtmosphericConditions().getPressure(),
                cloned.getAtmosphericConditions().getPressure(), 1e-6);
}

TEST_F(FlightConditionsTest, Equals)
{
    FlightConditions conditions1;
    FlightConditions conditions2;

    conditions1.setAOA(kPi / 6);
    conditions1.setMach(0.7);
    conditions2.setAOA(kPi / 6);
    conditions2.setMach(0.7);

    EXPECT_TRUE(conditions1 == conditions2);

    const Areas areas{{&assembly(), 0.00025}};
    conditions2.setThrustingNozzleExitAreas(areas);
    EXPECT_FALSE(conditions1 == conditions2);
}

TEST_F(FlightConditionsTest, SetAndGetAtmosphericConditions)
{
    const AtmosphericConditions atm{280, 90000};
    m_conditions.setAtmosphericConditions(atm);

    EXPECT_NEAR(280, m_conditions.getAtmosphericConditions().getTemperature(), 1e-6);
    EXPECT_NEAR(90000, m_conditions.getAtmosphericConditions().getPressure(), 1e-6);
}

TEST_F(FlightConditionsTest, HumidityOnlyAtmosphericChangeIsApplied)
{
    const double                dryDensity = m_conditions.getAtmosphericConditions().getDensity();
    const AtmosphericConditions humid{AtmosphericConditions::kStandardTemperature,
                                      AtmosphericConditions::kStandardPressure, 1.0};
    const EventCounter          changes{m_conditions};

    m_conditions.setAtmosphericConditions(humid);

    EXPECT_NEAR(1.0, m_conditions.getAtmosphericConditions().getRelativeHumidity(), 1e-6);
    EXPECT_LT(m_conditions.getAtmosphericConditions().getDensity(), dryDensity)
        << "Applying humid conditions should reduce density at fixed pressure and temperature";
    EXPECT_EQ(1, changes.count())
        << "A humidity-only atmospheric change should fire a change event";
}

TEST_F(FlightConditionsTest, GetVelocityWithChangedAtmosphere)
{
    AtmosphericConditions atm{280, 90000};
    m_conditions.setAtmosphericConditions(atm);
    m_conditions.setMach(0.5);

    double expectedVelocity = 0.5 * atm.getMachSpeed();
    EXPECT_NEAR(expectedVelocity, m_conditions.getVelocity(), 1e-6);

    // Change atmospheric conditions (Java changes the shared object; here the copy is replaced)
    atm.setTemperature(300);
    m_conditions.setAtmosphericConditions(atm);

    // Velocity should change with new atmospheric conditions
    expectedVelocity = 0.5 * atm.getMachSpeed();
    EXPECT_NEAR(expectedVelocity, m_conditions.getVelocity(), 1e-6);
}

// ---- Beyond the JUnit tests (values pinned with OpenRocket's FlightConditions on JDK 17) ----

TEST(FlightConditions, DefaultsAreJavas)
{
    const FlightConditions conditions;
    EXPECT_EQ(conditions.getRefLength(), 1.0);
    EXPECT_EQ(conditions.getRefArea(), 0.78539816339744830);
    EXPECT_EQ(conditions.getAOA(), 0.0);
    EXPECT_EQ(conditions.getSinAOA(), 0.0);
    EXPECT_EQ(conditions.getSincAOA(), 1.0);
    EXPECT_EQ(conditions.getTheta(), 0.0);
    EXPECT_EQ(conditions.getMach(), 0.3);
    EXPECT_EQ(conditions.getBeta(), 0.95393920141694570);  // sqrt is correctly rounded
    EXPECT_EQ(conditions.getRollRate(), 0.0);
    EXPECT_EQ(conditions.getPitchRate(), 0.0);
    EXPECT_EQ(conditions.getYawRate(), 0.0);
    EXPECT_TRUE(conditions.getPitchCenter().exactlyEquals(Coordinate::kNul));
    EXPECT_EQ(conditions.getAtmosphericConditions(), AtmosphericConditions{});
    EXPECT_TRUE(conditions.getThrustingNozzleExitAreas().empty());
    EXPECT_EQ(conditions.getThrustingNozzleExitArea(), 0.0);
    EXPECT_EQ(conditions.modId(), ModId::invalid());  // Java: null until the first change
    EXPECT_EQ(conditions.getVelocity(), 0.3 * AtmosphericConditions{}.getMachSpeed());
}

TEST(FlightConditions, ConfigurationWithoutBodiesGivesTheDefaultReferenceLength)
{
    Rocket rocket;
    rocket.addChild(std::make_unique<AxialStage>());
    const FlightConfiguration config{rocket};
    FlightConditions          conditions{config};
    EXPECT_EQ(conditions.getRefLength(), Rocket::kDefaultReferenceLength);

    conditions.setRefLength(3);
    conditions.setReference(config);
    EXPECT_EQ(conditions.getRefLength(), Rocket::kDefaultReferenceLength);
}

TEST(FlightConditions, BetaIsJavasBitForBit)
{
    struct Case
    {
        double mach;
        double beta;
    };
    constexpr std::array<Case, 11> kCases{{
        {.mach = 0.0, .beta = 1.0},
        {.mach = 0.2, .beta = 0.97979589711327120},
        {.mach = 0.3, .beta = 0.95393920141694570},
        {.mach = 0.8, .beta = 0.59999999999999990},
        {.mach = 0.9999999999, .beta = 0.25},
        {.mach = 1.0, .beta = 0.25},
        {.mach = 1.00000000001, .beta = 0.25},
        {.mach = 1.3, .beta = 0.83066238629180760},
        {.mach = 3.0, .beta = 2.8284271247461903},
        {.mach = 0.97, .beta = 0.25},
        {.mach = 1.03, .beta = 0.25},
    }};
    for (const Case& c : kCases)
    {
        FlightConditions conditions;
        conditions.setMach(c.mach);
        EXPECT_EQ(conditions.getBeta(), c.beta) << "Mach " << c.mach;
    }
}

TEST(FlightConditions, SetMachIsAtLeastZeroAndNaNGivesTheSmallestBeta)
{
    FlightConditions conditions;
    conditions.setMach(-2);
    EXPECT_EQ(conditions.getMach(), 0.0);
    EXPECT_EQ(conditions.getBeta(), 1.0);

    conditions.setMach(0.5);
    conditions.setMach(-0.0);  // Java's Math.max(-0.0, 0) is 0.0
    EXPECT_EQ(conditions.getMach(), 0.0);
    EXPECT_FALSE(std::signbit(conditions.getMach()));

    conditions.setMach(kNaN);
    EXPECT_TRUE(std::isnan(conditions.getMach()));
    EXPECT_EQ(conditions.getBeta(), 0.25);  // MathUtil.max skips the NaN
    EXPECT_TRUE(std::isnan(conditions.getVelocity()));
}

TEST(FlightConditions, SetAOAComputesSineAndSincAsJava)
{
    struct Case
    {
        double aoa;
        double sin;
        double sinc;
    };
    constexpr std::array<Case, 6> kCases{{
        {.aoa = 0.0005, .sin = 0.0005, .sinc = 1.0},
        {.aoa = 0.001, .sin = 0.00099999983333334170, .sinc = 0.99999983333334160},
        {.aoa = 0.5, .sin = 0.47942553860420300, .sinc = 0.95885107720840600},
        {.aoa = kPi / 4, .sin = 0.70710678118654750, .sinc = 0.90031631615710610},
        {.aoa = kPi / 6, .sin = 0.49999999999999994, .sinc = 0.95492965855137200},
        {.aoa = 3.0, .sin = 0.14112000805986720, .sinc = 0.047040002686622400},
    }};
    for (const Case& c : kCases)
    {
        FlightConditions conditions;
        conditions.setAOA(c.aoa);
        // The sine comes from the math library: a last-bit difference to Java is allowed.
        EXPECT_NEAR(conditions.getSinAOA(), c.sin, 1e-12 * c.sin) << "aoa " << c.aoa;
        EXPECT_NEAR(conditions.getSincAOA(), c.sinc, 1e-12 * c.sinc) << "aoa " << c.aoa;
    }
}

TEST(FlightConditions, BelowAMilliradianTheSineIsTheAngle)
{
    FlightConditions conditions;
    conditions.setAOA(0.000999);
    EXPECT_EQ(conditions.getSinAOA(), 0.000999);
    EXPECT_EQ(conditions.getSincAOA(), 1.0);
}

TEST(FlightConditions, SetAOAClampsToZeroToPi)
{
    FlightConditions conditions;
    conditions.setAOA(0.5);
    conditions.setAOA(-0.2);
    EXPECT_EQ(conditions.getAOA(), 0.0);
    EXPECT_EQ(conditions.getSinAOA(), 0.0);
    EXPECT_EQ(conditions.getSincAOA(), 1.0);

    conditions.setAOA(4.0);
    EXPECT_EQ(conditions.getAOA(), kPi);
    EXPECT_NEAR(conditions.getSinAOA(), std::sin(kPi), 1e-15);

    conditions.setAOA(kNaN);  // NaN passes the clamp, as in Java
    EXPECT_TRUE(std::isnan(conditions.getAOA()));
    EXPECT_TRUE(std::isnan(conditions.getSinAOA()));
}

TEST(FlightConditions, SetAOAWithSineClampsTheSineAndUsesIt)
{
    FlightConditions conditions;
    conditions.setAOA(0.5, 1.5);
    EXPECT_EQ(conditions.getAOA(), 0.5);
    EXPECT_EQ(conditions.getSinAOA(), 1.0);
    EXPECT_EQ(conditions.getSincAOA(), 2.0);

    conditions.setAOA(0.0005, 0.25);  // the given sine is kept, the sinc is 1
    EXPECT_EQ(conditions.getSinAOA(), 0.25);
    EXPECT_EQ(conditions.getSincAOA(), 1.0);

    conditions.setAOA(-1.0, -0.5);
    EXPECT_EQ(conditions.getAOA(), 0.0);
    EXPECT_EQ(conditions.getSinAOA(), 0.0);

    // An angle within MathUtil::equals of the current one changes nothing, the sine included.
    conditions.setAOA(0.3, 0.29552020666133955);
    conditions.setAOA(0.3 * (1 + 1e-10), 0.9);
    EXPECT_EQ(conditions.getSinAOA(), 0.29552020666133955);
}

TEST(FlightConditions, SettersWithinEpsilonChangeNothing)
{
    FlightConditions conditions;
    conditions.setAOA(0.5);
    conditions.setTheta(1.0);
    conditions.setMach(0.7);
    conditions.setRollRate(2.0);
    conditions.setPitchRate(3.0);
    conditions.setYawRate(4.0);
    conditions.setPitchCenter(Coordinate{1, 2, 3});
    const ModId        before = conditions.modId();
    const EventCounter changes{conditions};

    constexpr double kTiny = 1 + 1e-10;
    conditions.setAOA(0.5 * kTiny);
    conditions.setTheta(1.0 * kTiny);
    conditions.setMach(0.7 * kTiny);
    conditions.setRollRate(2.0 * kTiny);
    conditions.setPitchRate(3.0 * kTiny);
    conditions.setYawRate(4.0 * kTiny);
    conditions.setPitchCenter(Coordinate{1 * kTiny, 2, 3});
    conditions.setAtmosphericConditions(AtmosphericConditions{});

    EXPECT_EQ(changes.count(), 0);
    EXPECT_EQ(conditions.modId(), before);
    EXPECT_EQ(conditions.getAOA(), 0.5);
    EXPECT_EQ(conditions.getMach(), 0.7);
    EXPECT_EQ(conditions.getPitchCenter().x, 1.0);
}

TEST(FlightConditions, EverySetterFiresOneEventAndDrawsANewId)
{
    FlightConditions   conditions;
    const EventCounter changes{conditions};
    int                expected = 0;
    ModId              last     = conditions.modId();

    const auto check = [&](const char* what) {
        ++expected;
        EXPECT_EQ(changes.count(), expected) << what;
        EXPECT_GT(conditions.modId(), last) << what;
        last = conditions.modId();
    };

    conditions.setRefLength(0.1);
    check("setRefLength");
    conditions.setRefArea(0.5);
    check("setRefArea");
    conditions.setAOA(0.2);
    check("setAOA");
    conditions.setAOA(0.4, 0.38941834230865049);
    check("setAOA(aoa, sin)");
    conditions.setTheta(1.5);
    check("setTheta");
    conditions.setMach(1.2);
    check("setMach");
    conditions.setVelocity(100);
    check("setVelocity");
    conditions.setRollRate(1);
    check("setRollRate");
    conditions.setPitchRate(1);
    check("setPitchRate");
    conditions.setYawRate(1);
    check("setYawRate");
    conditions.setPitchCenter(Coordinate{0.5});
    check("setPitchCenter");
    conditions.setAtmosphericConditions(AtmosphericConditions{250, 80000});
    check("setAtmosphericConditions");

    // Setting the same values again fires nothing.
    conditions.setRefLength(conditions.getRefLength());
    conditions.setRefArea(conditions.getRefArea());
    conditions.setTheta(1.5);
    conditions.setRollRate(1);
    EXPECT_EQ(changes.count(), expected);
    EXPECT_EQ(conditions.modId(), last);
}

TEST(FlightConditions, ReferenceLengthAndAreaFollowEachOther)
{
    FlightConditions conditions;
    conditions.setRefArea(4.0);
    EXPECT_EQ(conditions.getRefLength(), 2.2567583341910250);
    conditions.setRefLength(0.1);
    EXPECT_EQ(conditions.getRefArea(), 0.0078539816339744830);

    conditions.setRefArea(-1.0);  // safeSqrt: a negative area gives length 0
    EXPECT_EQ(conditions.getRefLength(), 0.0);
    EXPECT_EQ(conditions.getRefArea(), -1.0);
}

TEST(FlightConditions, AtmosphericConditionsModifiedInPlaceFireNoEvent)
{
    FlightConditions   conditions;
    const EventCounter changes{conditions};
    const ModId        before = conditions.modId();

    conditions.getAtmosphericConditions().setTemperature(250);

    EXPECT_EQ(conditions.getAtmosphericConditions().getTemperature(), 250);
    EXPECT_EQ(conditions.getVelocity(), 0.3 * (165.77 + (0.606 * 250)));
    EXPECT_EQ(changes.count(), 0);
    EXPECT_EQ(conditions.modId(), before);
}

TEST(FlightConditions, CopyKeepsTheValuesAndIdButNotTheConnections)
{
    FlightConditions original;
    original.setAOA(0.3);
    original.setMach(1.4);
    original.getAtmosphericConditions().setPressure(95000);
    const EventCounter originalChanges{original};

    FlightConditions copy{original};
    EXPECT_EQ(copy, original);
    EXPECT_EQ(copy.modId(), original.modId());
    EXPECT_EQ(copy.getBeta(), original.getBeta());
    EXPECT_EQ(copy.getSinAOA(), original.getSinAOA());
    EXPECT_EQ(copy.getAtmosphericConditions().getPressure(), 95000);
    EXPECT_TRUE(copy.changed().empty());

    copy.setMach(2.0);
    copy.getAtmosphericConditions().setPressure(80000);
    EXPECT_EQ(originalChanges.count(), 0);
    EXPECT_EQ(original.getMach(), 1.4);
    EXPECT_EQ(original.getAtmosphericConditions().getPressure(), 95000);

    // Copy assignment takes the values and keeps the target's own connections, firing nothing.
    FlightConditions   target;
    const EventCounter targetChanges{target};
    target = original;
    EXPECT_EQ(target, original);
    EXPECT_EQ(targetChanges.count(), 0);
    target.setMach(0.1);
    EXPECT_EQ(targetChanges.count(), 1);
    EXPECT_EQ(originalChanges.count(), 0);
}

TEST(FlightConditions, MoveConstructionTakesTheConnectionsAlong)
{
    FlightConditions   original;
    const EventCounter changes{original};
    FlightConditions   moved{std::move(original)};
    moved.setMach(0.9);
    EXPECT_EQ(changes.count(), 1);
}

TEST(FlightConditions, AssigningAMovedValueKeepsTheTargetsConnections)
{
    // `conditions = other.clone()` is the C++ form of Java's reference reassignment (the
    // simulation steppers' store.flightConditions = c): the listeners of the target stay, as with
    // the copy assignment, and nothing fires.
    FlightConditions source;
    source.setAOA(0.3);
    source.setMach(1.4);
    AxialStage stage;
    source.setThrustingNozzleExitAreas(Areas{{&stage, 0.25}});
    const EventCounter sourceChanges{source};

    FlightConditions   target;
    const EventCounter targetChanges{target};
    target = source.clone();
    EXPECT_EQ(target, source);
    EXPECT_EQ(target.modId(), source.modId());
    EXPECT_EQ(target.getThrustingNozzleExitArea(stage), 0.25);
    EXPECT_EQ(targetChanges.count(), 0);
    target.setMach(0.9);
    EXPECT_EQ(targetChanges.count(), 1);
    EXPECT_EQ(sourceChanges.count(), 0);

    // Moving a named object: the same, and the source keeps its connections, its other values and
    // no nozzle areas.
    FlightConditions   other;
    const EventCounter otherChanges{other};
    other = std::move(source);
    EXPECT_EQ(other.getMach(), 1.4);
    EXPECT_EQ(other.getThrustingNozzleExitArea(), 0.25);
    other.setMach(0.5);
    EXPECT_EQ(otherChanges.count(), 1);
    EXPECT_EQ(sourceChanges.count(), 0);
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): moved from on purpose
    EXPECT_TRUE(source.getThrustingNozzleExitAreas().empty());
    EXPECT_EQ(source.getMach(), 1.4);
    source.setMach(2.0);
    EXPECT_EQ(sourceChanges.count(), 1);
    EXPECT_EQ(otherChanges.count(), 1);

    // Self-assignment changes nothing.
    FlightConditions& self = other;
    other                  = std::move(self);
    EXPECT_EQ(other.getMach(), 0.5);
    EXPECT_EQ(other.getThrustingNozzleExitArea(), 0.25);
}

TEST_F(FlightConditionsTest, NozzleAreasDropZerosAndSumInOrder)
{
    AxialStage& booster = m_rocket->addChild(std::make_unique<AxialStage>());
    const Areas areas{{&booster, 0.5}, {m_rocket.get(), 0.0}, {&assembly(), 0.25}};
    m_conditions.setThrustingNozzleExitAreas(areas);

    const Areas expected{{&booster, 0.5}, {&assembly(), 0.25}};
    EXPECT_EQ(m_conditions.getThrustingNozzleExitAreas(), expected);
    EXPECT_EQ(m_conditions.getThrustingNozzleExitArea(), 0.75);
    EXPECT_EQ(m_conditions.getThrustingNozzleExitArea(booster), 0.5);
    EXPECT_EQ(m_conditions.getThrustingNozzleExitArea(assembly()), 0.25);
    EXPECT_EQ(m_conditions.getThrustingNozzleExitArea(*m_rocket), 0.0);
}

TEST_F(FlightConditionsTest, NozzleAreasAreFoundByComponentEquality)
{
    // A copy of the rocket keeps the ids, and Java's HashMap finds an equal component.
    const std::unique_ptr<Rocket> copy = m_rocket->copyRocketWithOriginalId();
    const auto& copiedStage            = dynamic_cast<const ComponentAssembly&>(copy->getChild(0));
    ASSERT_NE(&copiedStage, m_stage);

    const Areas areas{{m_stage, 0.125}};
    m_conditions.setThrustingNozzleExitAreas(areas);
    EXPECT_EQ(m_conditions.getThrustingNozzleExitArea(copiedStage), 0.125);

    // The same areas keyed by the equal component are no change.
    const EventCounter changes{m_conditions};
    const Areas        sameAreas{{&copiedStage, 0.125}};
    m_conditions.setThrustingNozzleExitAreas(sameAreas);
    EXPECT_EQ(changes.count(), 0);
    EXPECT_EQ(m_conditions.getThrustingNozzleExitAreas().front().first, m_stage);

    m_conditions.setThrustingNozzleExitAreas(Areas{{&copiedStage, 0.25}});
    EXPECT_EQ(changes.count(), 1);
}

TEST_F(FlightConditionsTest, NozzleAreasThatAreUnchangedFireNothing)
{
    const Areas areas{{&assembly(), 0.5}};
    m_conditions.setThrustingNozzleExitAreas(areas);
    const ModId        before = m_conditions.modId();
    const EventCounter changes{m_conditions};

    m_conditions.setThrustingNozzleExitAreas(areas);
    m_conditions.setThrustingNozzleExitAreas(Areas{{&assembly(), 0.5}, {m_rocket.get(), 0.0}});
    EXPECT_EQ(changes.count(), 0);
    EXPECT_EQ(m_conditions.modId(), before);

    m_conditions.setThrustingNozzleExitAreas(Areas{});
    EXPECT_EQ(changes.count(), 1);
    EXPECT_TRUE(m_conditions.getThrustingNozzleExitAreas().empty());
}

TEST_F(FlightConditionsTest, InvalidNozzleAreasAreBugsAndChangeNothing)
{
    const Areas valid{{&assembly(), 0.5}};
    m_conditions.setThrustingNozzleExitAreas(valid);

    EXPECT_THROW(m_conditions.setThrustingNozzleExitAreas(Areas{{&assembly(), -1e-9}}), BugError);
    EXPECT_THROW(m_conditions.setThrustingNozzleExitAreas(Areas{{&assembly(), kNaN}}), BugError);
    EXPECT_THROW(m_conditions.setThrustingNozzleExitAreas(Areas{{&assembly(), kInf}}), BugError);
    EXPECT_THROW(m_conditions.setThrustingNozzleExitAreas(Areas{{nullptr, 0.5}}), BugError);
    EXPECT_THROW(m_conditions.setThrustingNozzleExitAreas(
                     Areas{{m_rocket.get(), 0.1}, {&assembly(), 0.2}, {&assembly(), 0.3}}),
                 BugError);
    // A valid entry before the bad one is not applied either.
    EXPECT_THROW(
        m_conditions.setThrustingNozzleExitAreas(Areas{{m_rocket.get(), 0.1}, {nullptr, 0.0}}),
        BugError);

    EXPECT_EQ(m_conditions.getThrustingNozzleExitAreas(), valid);
}

TEST(FlightConditions, ToStringIsJavas)
{
    FlightConditions conditions;
    EXPECT_EQ(conditions.toString(),
              "FlightConditions[aoa=0.00°,theta=0.00°,mach=0.300,"
              "thrustingNozzleExitArea=0.000000,rollRate=0.00,pitchRate=0.00,yawRate=0.00,"
              "refLength=1.000,pitchCenter=(0.00000,0.00000,0.00000),"
              "atmosphericConditions=AtmosphericConditions[T=293.15,P=101325.00]]");

    conditions.setAOA(kPi / 6);
    conditions.setTheta(kPi / 3);
    conditions.setMach(0.7);
    conditions.setRollRate(3.0);
    conditions.setPitchRate(-1.25);
    conditions.setYawRate(0.5);
    conditions.setRefLength(0.0254);
    conditions.setPitchCenter(Coordinate{0.5, 0.25, -0.125});
    conditions.setAtmosphericConditions(AtmosphericConditions{280, 90000});
    EXPECT_EQ(conditions.toString(),
              "FlightConditions[aoa=30.00°,theta=60.00°,mach=0.700,"
              "thrustingNozzleExitArea=0.000000,rollRate=3.00,pitchRate=-1.25,yawRate=0.50,"
              "refLength=0.025,pitchCenter=(0.50000,0.25000,-0.12500),"
              "atmosphericConditions=AtmosphericConditions[T=280.00,P=90000.00]]");

    AxialStage first;
    AxialStage second;
    conditions.setThrustingNozzleExitAreas(Areas{{&first, 0.00025}, {&second, 0.0001}});
    EXPECT_EQ(conditions.toString(),
              "FlightConditions[aoa=30.00°,theta=60.00°,mach=0.700,"
              "thrustingNozzleExitArea=0.000350,rollRate=3.00,pitchRate=-1.25,yawRate=0.50,"
              "refLength=0.025,pitchCenter=(0.50000,0.25000,-0.12500),"
              "atmosphericConditions=AtmosphericConditions[T=280.00,P=90000.00]]");

    // The pitch centre rounds as Java's %.5f does (pinned with OpenRocket on JDK 17).
    conditions.setPitchCenter(Coordinate{0.015625, 5e-6, -1.49999e-5});
    EXPECT_EQ(conditions.toString(),
              "FlightConditions[aoa=30.00°,theta=60.00°,mach=0.700,"
              "thrustingNozzleExitArea=0.000350,rollRate=3.00,pitchRate=-1.25,yawRate=0.50,"
              "refLength=0.025,pitchCenter=(0.01563,0.00001,-0.00001),"
              "atmosphericConditions=AtmosphericConditions[T=280.00,P=90000.00]]");
}

TEST(FlightConditions, HashCodeIsJavas)
{
    FlightConditions conditions;
    EXPECT_EQ(conditions.hashCode(), 40300);

    conditions.setAOA(kPi / 6);
    conditions.setTheta(kPi / 3);
    conditions.setMach(0.7);
    conditions.setRollRate(3.0);
    conditions.setPitchRate(-1.25);
    conditions.setYawRate(0.5);
    conditions.setRefLength(0.0254);
    EXPECT_EQ(conditions.hashCode(), 140926);

    AxialStage first;
    ASSERT_TRUE(first.setId("12345678-9abc-4def-8123-456789abcdef").has_value());
    AxialStage second;
    ASSERT_TRUE(second.setId("fedcba98-7654-4321-8fed-cba987654321").has_value());
    EXPECT_EQ(first.hashCode(), -2147445985);
    EXPECT_EQ(second.hashCode(), -2147454671);
    conditions.setThrustingNozzleExitAreas(Areas{{&first, 0.00025}, {&second, 0.0001}});
    EXPECT_EQ(conditions.hashCode(), -1043758038);
}

TEST(FlightConditions, EqualityIsTolerantButNeverForNaN)
{
    FlightConditions a;
    FlightConditions b;
    EXPECT_EQ(a, b);

    b.setRefLength(1.0 + 1e-12);
    EXPECT_EQ(a, b);  // within MathUtil::equals
    b.setRefLength(1.001);
    EXPECT_NE(a, b);

    FlightConditions c;
    c.setPitchCenter(Coordinate{0.1});
    EXPECT_NE(a, c);

    FlightConditions d;
    d.setAtmosphericConditions(AtmosphericConditions{280, 101325});
    EXPECT_NE(a, d);

    FlightConditions e;
    e.setYawRate(0.5);
    EXPECT_NE(a, e);

    FlightConditions nan;
    nan.setTheta(kNaN);
    const FlightConditions copy{nan};
    EXPECT_FALSE(nan == copy);  // MathUtil::equals is never true for a NaN
    EXPECT_TRUE(nan == nan);    // but an object equals itself
}

}  // namespace
