#include "QtRocket/simulation/SimulationConditions.h"

#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/BarrowmanCalculator.h"
#include "QtRocket/mass/MassCalculator.h"
#include "QtRocket/models/ConstantGravityModel.h"
#include "QtRocket/models/ExtendedIsaModel.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WgsGravityModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/simulation/AbstractRkSimulationStepper.h"
#include "QtRocket/simulation/DefaultSimulationOptionFactory.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Monitorable.h"
#include "QtRocket/util/WorldCoordinate.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationTestSupport.h"

namespace
{

using QtRocket::AbstractRkSimulationStepper;
using QtRocket::BarrowmanCalculator;
using QtRocket::CloneableSimulationListener;
using QtRocket::ConstantGravityModel;
using QtRocket::Coordinate;
using QtRocket::DefaultSimulationOptionFactory;
using QtRocket::ExtendedIsaModel;
using QtRocket::GeodeticComputationStrategy;
using QtRocket::InMemoryPreferences;
using QtRocket::MassCalculator;
using QtRocket::ModId;
using QtRocket::MultiLevelPinkNoiseWindModel;
using QtRocket::PinkNoiseWindModel;
using QtRocket::Simulation;
using QtRocket::SimulationConditions;
using QtRocket::SimulationListener;
using QtRocket::SimulationOptions;
using QtRocket::WgsGravityModel;
using QtRocket::WorldCoordinate;
using QtRocket::Test::bugText;
using QtRocket::Test::ModIdWatch;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;

// The conditions can be watched as every Monitorable; a copy is made on purpose, by clone().
static_assert(QtRocket::Monitorable<SimulationConditions>);
static_assert(!std::is_copy_constructible_v<SimulationConditions>);
static_assert(!std::is_copy_assignable_v<SimulationConditions>);
static_assert(std::is_nothrow_move_constructible_v<SimulationConditions>);

/// SimulationConditionsTest.EPSILON
constexpr double kEpsilon = QtRocket::MathUtil::kEpsilon;

/// Adds a wind level, which must succeed.
void addLevel(MultiLevelPinkNoiseWindModel& model, double altitude, double speed, double direction,
              double standardDeviation)
{
    EXPECT_TRUE(model.addWindLevel(altitude, speed, direction, standardDeviation).has_value());
}

// ------------------------------------------------------------ SimulationConditionsTest.java

// SimulationConditionsTest.testDefaultSimulationOptionFactory. Java asks its injector for the
// factory over MockPreferences, an empty preference store; here that is an empty
// InMemoryPreferences.
TEST(SimulationConditions, DefaultSimulationOptionFactory)
{
    InMemoryPreferences                  preferences;
    const DefaultSimulationOptionFactory factory(preferences);
    const SimulationOptions              options = factory.getDefault();

    EXPECT_NEAR(28.61, options.getLaunchLatitude(), kEpsilon);
    EXPECT_NEAR(0.0, options.getLaunchAltitude(), kEpsilon);
    EXPECT_NEAR(-80.60, options.getLaunchLongitude(), kEpsilon);
    EXPECT_TRUE(options.isIsaAtmosphere());
    EXPECT_NEAR(288.15, options.getLaunchTemperature(), kEpsilon);
    EXPECT_NEAR(101325, options.getLaunchPressure(), kEpsilon);
    EXPECT_NEAR(0, options.getLaunchRelativeHumidity(), kEpsilon);
    EXPECT_NEAR(1.0, options.getLaunchRodLength(), kEpsilon);
    EXPECT_NEAR(std::numbers::pi / 2, options.getLaunchRodDirection(), kEpsilon);
    EXPECT_NEAR(0.0, options.getLaunchRodAngle(), kEpsilon);
    EXPECT_TRUE(options.getLaunchIntoWind());
    EXPECT_NEAR(std::numbers::pi / 2, options.getAverageWindModel().getDirection(), kEpsilon);
    EXPECT_NEAR(0.1, options.getAverageWindModel().getTurbulenceIntensity(), kEpsilon);
    EXPECT_NEAR(2.0, options.getAverageWindModel().getAverage(), kEpsilon);
    EXPECT_NEAR(0.2, options.getAverageWindModel().getStandardDeviation(), kEpsilon);

    EXPECT_NEAR(0.05, options.getTimeStep(), kEpsilon);
    EXPECT_NEAR(1200, options.getMaxSimulationTime(), kEpsilon);
    EXPECT_NEAR(3 * std::numbers::pi / 180, options.getMaximumStepAngle(), kEpsilon);
}

// SimulationConditionsTest.testWindModelComparison: "Compare PinkNoiseWindModel and
// MultiLevelPinkNoiseWindModel in SimulationConditions". The models are seeded here, so that
// the test is deterministic (Java leaves them their random seeds).
TEST(SimulationConditions, WindModelComparison)
{
    SimulationConditions conditions;

    // Test PinkNoiseWindModel
    const std::shared_ptr<PinkNoiseWindModel> pinkNoiseModel =
        std::make_shared<PinkNoiseWindModel>(11);
    pinkNoiseModel->setAverage(5.0);
    pinkNoiseModel->setStandardDeviation(1.0);
    pinkNoiseModel->setDirection(std::numbers::pi / 4);  // 45 degrees

    conditions.setWindModel(pinkNoiseModel);

    const Coordinate pinkNoiseVelocity = conditions.getWindModel()->getWindVelocity(0, 100);
    EXPECT_TRUE(pinkNoiseVelocity.length() > 0);

    // Test MultiLevelPinkNoiseWindModel
    const InMemoryPreferences                           preferences;
    const std::shared_ptr<MultiLevelPinkNoiseWindModel> multiLevelModel =
        std::make_shared<MultiLevelPinkNoiseWindModel>(preferences);
    multiLevelModel->clearLevels();
    addLevel(*multiLevelModel, 0, 5.0, std::numbers::pi / 4, 1);
    addLevel(*multiLevelModel, 1000, 10.0, std::numbers::pi / 2, 2);
    multiLevelModel->setSeed(12);

    conditions.setWindModel(multiLevelModel);

    const Coordinate multiLevelVelocity = conditions.getWindModel()->getWindVelocity(0, 100);
    EXPECT_TRUE(multiLevelVelocity.length() > 0);

    // Compare behaviors
    EXPECT_FALSE(pinkNoiseVelocity == multiLevelVelocity);
}

// SimulationConditionsTest.testMultiLevelWindModelConsistency: "Test wind velocity consistency
// for MultiLevelPinkNoiseWindModel".
TEST(SimulationConditions, MultiLevelWindModelConsistency)
{
    SimulationConditions                                conditions;
    const InMemoryPreferences                           preferences;
    const std::shared_ptr<MultiLevelPinkNoiseWindModel> multiLevelModel =
        std::make_shared<MultiLevelPinkNoiseWindModel>(preferences);
    multiLevelModel->clearLevels();
    addLevel(*multiLevelModel, 0, 5.0, std::numbers::pi / 4, 2);
    addLevel(*multiLevelModel, 1000, 10.0, std::numbers::pi / 2, 1);
    multiLevelModel->setSeed(13);

    conditions.setWindModel(multiLevelModel);

    const Coordinate velocity1 = conditions.getWindModel()->getWindVelocity(0, 500);
    const Coordinate velocity2 = conditions.getWindModel()->getWindVelocity(0, 500);

    EXPECT_TRUE(velocity1 == velocity2);
}

// SimulationConditionsTest.testPinkNoiseWindModelVariation: "Test wind velocity variation for
// PinkNoiseWindModel".
TEST(SimulationConditions, PinkNoiseWindModelVariation)
{
    SimulationConditions                      conditions;
    const std::shared_ptr<PinkNoiseWindModel> pinkNoiseModel =
        std::make_shared<PinkNoiseWindModel>(14);
    pinkNoiseModel->setAverage(5.0);
    pinkNoiseModel->setStandardDeviation(1.0);
    pinkNoiseModel->setDirection(std::numbers::pi / 4);

    conditions.setWindModel(pinkNoiseModel);

    const Coordinate velocity1 = conditions.getWindModel()->getWindVelocity(0, 100);
    const Coordinate velocity2 = conditions.getWindModel()->getWindVelocity(1, 100);

    EXPECT_FALSE(velocity1 == velocity2);
}

// SimulationConditionsTest.testMultiLevelWindModelAltitudeDependence: "Test altitude dependence
// of MultiLevelPinkNoiseWindModel".
TEST(SimulationConditions, MultiLevelWindModelAltitudeDependence)
{
    SimulationConditions                                conditions;
    const InMemoryPreferences                           preferences;
    const std::shared_ptr<MultiLevelPinkNoiseWindModel> multiLevelModel =
        std::make_shared<MultiLevelPinkNoiseWindModel>(preferences);
    multiLevelModel->clearLevels();
    addLevel(*multiLevelModel, 0, 5.0, 0, 0);
    addLevel(*multiLevelModel, 1000, 10.0, std::numbers::pi / 2, 0);

    conditions.setWindModel(multiLevelModel);

    const Coordinate velocityLow  = conditions.getWindModel()->getWindVelocity(0, 0);
    const Coordinate velocityHigh = conditions.getWindModel()->getWindVelocity(0, 1000);
    const Coordinate velocityMid  = conditions.getWindModel()->getWindVelocity(0, 500);

    EXPECT_FALSE(velocityLow == velocityHigh);
    EXPECT_TRUE(velocityMid.length() > velocityLow.length() &&
                velocityMid.length() < velocityHigh.length());
}

// ============================================================================ the defaults

TEST(SimulationConditions, NewConditionsHaveJavasDefaults)
{
    // probes/tier8b-status/ConditionsProbe.java, "defaults"
    const SimulationConditions conditions;
    EXPECT_EQ(conditions.getLaunchRodLength(), 1.0);
    EXPECT_EQ(conditions.getLaunchRodAngle(), 0.0);
    EXPECT_EQ(conditions.getLaunchRodDirection(), 0.0);
    EXPECT_EQ(conditions.getLaunchSite().getLatitudeDeg(), 0.0);
    EXPECT_EQ(conditions.getLaunchSite().getLongitudeDeg(), 0.0);
    EXPECT_EQ(conditions.getLaunchSite().getAltitude(), 0.0);
    EXPECT_EQ(conditions.getGeodeticComputation(), GeodeticComputationStrategy::SPHERICAL);
    EXPECT_TRUE(conditions.getLaunchPosition().exactlyEquals(Coordinate::kNul));
    EXPECT_TRUE(conditions.getLaunchVelocity().exactlyEquals(Coordinate::kNul));
    EXPECT_EQ(conditions.getWindModel(), nullptr);
    EXPECT_EQ(conditions.getAtmosphericModel(), nullptr);
    EXPECT_EQ(conditions.getGravityModel(), nullptr);
    EXPECT_EQ(conditions.getAerodynamicCalculator(), nullptr);
    EXPECT_FALSE(conditions.getMassCalculator().has_value());
    EXPECT_EQ(conditions.getTimeStep(), 0.05);
    EXPECT_EQ(conditions.getMaxSimulationTime(), 1200.0);
    EXPECT_EQ(conditions.getMaximumAngleStep(), 0.05235987755982988);
    EXPECT_EQ(conditions.getRecoverySpeedWarning(), 20.0);
    EXPECT_EQ(conditions.getDrogueLowSpeedWarning(), 3.048);
    EXPECT_EQ(conditions.getRecoveryDrogueMainHighSpeedWarning(), 30.48);
    EXPECT_EQ(conditions.getRecoveryDrogueMainLowSpeedWarning(), 15.24);
    EXPECT_EQ(conditions.getRandomSeed(), 0);
    EXPECT_TRUE(conditions.getSimulationListenerList().empty());
    EXPECT_EQ(conditions.getSimulation(), nullptr);
    EXPECT_EQ(conditions.getModId(), ModId::invalid());
    EXPECT_EQ(conditions.modId(), ModId::invalid());
}

TEST(SimulationConditions, TheRecommendedValuesAreTheSteppers)
{
    // Java: RK4SimulationStepper.RECOMMENDED_TIME_STEP, RECOMMENDED_MAX_TIME and
    // RECOMMENDED_ANGLE_STEP (3 * Math.PI / 180), and PITCH_YAW_RANDOM.
    EXPECT_EQ(AbstractRkSimulationStepper::kRecommendedTimeStep, 0.05);
    EXPECT_EQ(AbstractRkSimulationStepper::kRecommendedMaxTime, 1200.0);
    EXPECT_EQ(AbstractRkSimulationStepper::kRecommendedAngleStep, 0.05235987755982988);
    EXPECT_EQ(AbstractRkSimulationStepper::kPitchYawRandom, 0.0005);
    const SimulationConditions conditions;
    EXPECT_EQ(conditions.getTimeStep(), AbstractRkSimulationStepper::kRecommendedTimeStep);
    EXPECT_EQ(conditions.getMaxSimulationTime(), AbstractRkSimulationStepper::kRecommendedMaxTime);
    EXPECT_EQ(conditions.getMaximumAngleStep(), AbstractRkSimulationStepper::kRecommendedAngleStep);
}

// ============================================================================ the setters

TEST(SimulationConditions, TheSettersOfPlainValuesDrawAModificationId)
{
    // ConditionsProbe, "modID effects": each of these setters draws an id, also for the value
    // it already has.
    SimulationConditions             c;
    ModIdWatch<SimulationConditions> watch(c);
    c.setLaunchRodLength(1);
    EXPECT_TRUE(watch.drew());
    c.setLaunchRodAngle(0);
    EXPECT_TRUE(watch.drew());
    c.setLaunchRodDirection(0);
    EXPECT_TRUE(watch.drew());
    c.setTimeStep(0.05);
    EXPECT_TRUE(watch.drew());
    c.setMaxSimulationTime(1200);
    EXPECT_TRUE(watch.drew());
    c.setMaximumAngleStep(0.05);
    EXPECT_TRUE(watch.drew());
    c.setRecoverySpeedWarning(20);
    EXPECT_TRUE(watch.drew());
    c.setDrogueLowSpeedWarning(3.048);
    EXPECT_TRUE(watch.drew());
    c.setRecoveryDrogueMainHighSpeedWarning(30.48);
    EXPECT_TRUE(watch.drew());
    c.setRecoveryDrogueMainLowSpeedWarning(15.24);
    EXPECT_TRUE(watch.drew());
    c.setRandomSeed(0);
    EXPECT_TRUE(watch.drew());

    EXPECT_EQ(c.getMaximumAngleStep(), 0.05);
    c.setLaunchRodLength(2.5);
    c.setLaunchRodAngle(-0.25);
    c.setLaunchRodDirection(4.5);
    c.setTimeStep(0.01);
    c.setMaxSimulationTime(60);
    c.setRecoverySpeedWarning(1);
    c.setDrogueLowSpeedWarning(2);
    c.setRecoveryDrogueMainHighSpeedWarning(3);
    c.setRecoveryDrogueMainLowSpeedWarning(4);
    c.setRandomSeed(-17);
    EXPECT_EQ(c.getLaunchRodLength(), 2.5);
    EXPECT_EQ(c.getLaunchRodAngle(), -0.25);
    EXPECT_EQ(c.getLaunchRodDirection(), 4.5) << "not reduced, not clamped";
    EXPECT_EQ(c.getTimeStep(), 0.01);
    EXPECT_EQ(c.getMaxSimulationTime(), 60.0);
    EXPECT_EQ(c.getRecoverySpeedWarning(), 1.0);
    EXPECT_EQ(c.getDrogueLowSpeedWarning(), 2.0);
    EXPECT_EQ(c.getRecoveryDrogueMainHighSpeedWarning(), 3.0);
    EXPECT_EQ(c.getRecoveryDrogueMainLowSpeedWarning(), 4.0);
    EXPECT_EQ(c.getRandomSeed(), -17);
}

TEST(SimulationConditions, TheLaunchSiteSettersDoNothingForAnEqualValue)
{
    // ConditionsProbe: "setLaunchSite (equal): same", "(within epsilon): same", "(other): new",
    // and the same for the launch position, the launch velocity and the geodetic computation.
    SimulationConditions             c;
    ModIdWatch<SimulationConditions> watch(c);
    c.setLaunchSite({0, 0, 0});
    EXPECT_FALSE(watch.drew());
    c.setLaunchSite({0, 0, 1e-12});
    EXPECT_FALSE(watch.drew());
    EXPECT_EQ(c.getLaunchSite().getAltitude(), 0.0) << "the equal site was not stored";
    c.setLaunchSite({1, 2, 3});
    EXPECT_TRUE(watch.drew());
    EXPECT_EQ(c.getLaunchSite().getAltitude(), 3.0);

    c.setLaunchPosition(Coordinate{0, 0, 0});
    EXPECT_FALSE(watch.drew());
    c.setLaunchPosition(Coordinate{0, 0, 5});
    EXPECT_TRUE(watch.drew());
    EXPECT_EQ(c.getLaunchPosition().z, 5.0);

    c.setLaunchVelocity(Coordinate{0, 0, 0});
    EXPECT_FALSE(watch.drew());
    c.setLaunchVelocity(Coordinate{0, 0, 5});
    EXPECT_TRUE(watch.drew());
    EXPECT_EQ(c.getLaunchVelocity().z, 5.0);

    c.setGeodeticComputation(GeodeticComputationStrategy::SPHERICAL);
    EXPECT_FALSE(watch.drew());
    c.setGeodeticComputation(GeodeticComputationStrategy::FLAT);
    EXPECT_TRUE(watch.drew());
    EXPECT_EQ(c.getGeodeticComputation(), GeodeticComputationStrategy::FLAT);
}

TEST(SimulationConditions, OfTheModelsOnlyTheGravityModelDrawsAModificationId)
{
    // ConditionsProbe: the wind model, the atmospheric model and the two calculators leave the
    // id alone, the first time and when they replace one (Java draws its unread modIDadd
    // then); the gravity model always draws one, also for a null model.
    SimulationConditions             c;
    ModIdWatch<SimulationConditions> watch(c);
    const auto                       calculator = std::make_shared<BarrowmanCalculator>();
    c.setAerodynamicCalculator(calculator);
    EXPECT_FALSE(watch.drew());
    EXPECT_EQ(c.getAerodynamicCalculator(), calculator);
    c.setAerodynamicCalculator(std::make_shared<BarrowmanCalculator>());
    EXPECT_FALSE(watch.drew());
    EXPECT_NE(c.getAerodynamicCalculator(), calculator);

    c.setMassCalculator(MassCalculator());
    EXPECT_FALSE(watch.drew());
    EXPECT_TRUE(c.getMassCalculator().has_value());
    c.setMassCalculator(MassCalculator());
    EXPECT_FALSE(watch.drew());
    c.setMassCalculator(std::nullopt);
    EXPECT_FALSE(watch.drew());
    EXPECT_FALSE(c.getMassCalculator().has_value());

    const auto wind = std::make_shared<PinkNoiseWindModel>(1);
    c.setWindModel(wind);
    EXPECT_FALSE(watch.drew());
    EXPECT_EQ(c.getWindModel(), wind);
    c.setWindModel(std::make_shared<PinkNoiseWindModel>(2));
    EXPECT_FALSE(watch.drew());

    const auto atmosphere = std::make_shared<const ExtendedIsaModel>();
    c.setAtmosphericModel(atmosphere);
    EXPECT_FALSE(watch.drew());
    EXPECT_EQ(c.getAtmosphericModel(), atmosphere);
    c.setAtmosphericModel(std::make_shared<const ExtendedIsaModel>());
    EXPECT_FALSE(watch.drew());

    const auto gravity = std::make_shared<const WgsGravityModel>();
    c.setGravityModel(gravity);
    EXPECT_TRUE(watch.drew());
    EXPECT_EQ(c.getGravityModel(), gravity);
    c.setGravityModel(nullptr);
    EXPECT_TRUE(watch.drew());
    EXPECT_EQ(c.getGravityModel(), nullptr);
}

/// A listener with a value member, to tell a clone from its original.
class Marker final : public CloneableSimulationListener<Marker>
{
public:
    [[nodiscard]] int value() const noexcept { return m_value; }
    void              setValue(int value) noexcept { m_value = value; }

private:
    int m_value{1};
};

TEST(SimulationConditions, TheSimulationAndTheListenerListDrawNoModificationId)
{
    TestEstesAlphaIII                alpha;
    Simulation                       simulation(*alpha.rocket);
    SimulationConditions             c;
    ModIdWatch<SimulationConditions> watch(c);
    c.setSimulation(&simulation);
    EXPECT_FALSE(watch.drew());
    EXPECT_EQ(c.getSimulation(), &simulation);
    c.getSimulationListenerList().push_back(std::make_shared<Marker>());
    EXPECT_FALSE(watch.drew());
    EXPECT_EQ(c.getSimulationListenerList().size(), 1U);
    EXPECT_EQ(c.getModId(), ModId::invalid());
}

// ============================================================ the rocket and the configuration

TEST(SimulationConditions, TheRocketAndTheConfigurationIdComeFromTheSimulation)
{
    TestEstesAlphaIII alpha;
    Simulation        simulation(*alpha.rocket);
    simulation.setFlightConfigurationId(testFcid(2));

    SimulationConditions conditions;
    conditions.setSimulation(&simulation);
    EXPECT_EQ(&conditions.getRocket(), alpha.rocket.get());
    EXPECT_EQ(conditions.getFlightConfigurationId(), testFcid(2));
    EXPECT_EQ(conditions.getMotorConfigurationId(), testFcid(2));

    // Not a snapshot: the simulation is asked each time.
    simulation.setFlightConfigurationId(testFcid(3));
    EXPECT_EQ(conditions.getFlightConfigurationId(), testFcid(3));
    EXPECT_EQ(conditions.getMotorConfigurationId(), testFcid(3));
}

TEST(SimulationConditions, WithoutASimulationThereIsNoRocket)
{
    // Java: a NullPointerException (ConditionsProbe: "getRocket without simulation").
    const SimulationConditions conditions;
    const std::string          text = "The simulation conditions have no simulation";
    EXPECT_EQ(bugText([&] { static_cast<void>(conditions.getRocket()); }), text);
    EXPECT_EQ(bugText([&] { static_cast<void>(conditions.getFlightConfigurationId()); }), text);
    EXPECT_EQ(bugText([&] { static_cast<void>(conditions.getMotorConfigurationId()); }), text);
}

// ============================================================================ clone()

TEST(SimulationConditions, ACloneSharesTheModelsAndHasClonesOfTheListeners)
{
    // ConditionsProbe, "clone": "wind same true atmosphere same true gravity same true aero
    // same true mass same true modID same true", "list same false size 1 listener same false
    // value 9", "values 1.0 (0.0, 0.0, 5.0) FLAT 0.05 0", and the original untouched by what
    // is done to the clone.
    TestEstesAlphaIII    alpha;
    Simulation           simulation(*alpha.rocket);
    SimulationConditions c;
    c.setSimulation(&simulation);
    c.setAerodynamicCalculator(std::make_shared<BarrowmanCalculator>());
    c.setMassCalculator(MassCalculator());
    c.setWindModel(std::make_shared<PinkNoiseWindModel>(3));
    c.setAtmosphericModel(std::make_shared<const ExtendedIsaModel>());
    c.setGravityModel(std::make_shared<const ConstantGravityModel>(5));
    c.setLaunchPosition(Coordinate{0, 0, 5});
    c.setLaunchVelocity(Coordinate{1, 2, 3});
    c.setLaunchSite({10, 20, 30});
    c.setGeodeticComputation(GeodeticComputationStrategy::FLAT);
    c.setLaunchRodLength(1.5);
    c.setLaunchRodAngle(0.25);
    c.setLaunchRodDirection(2.5);
    c.setMaxSimulationTime(99);
    c.setMaximumAngleStep(0.07);
    c.setRecoverySpeedWarning(1);
    c.setDrogueLowSpeedWarning(2);
    c.setRecoveryDrogueMainHighSpeedWarning(3);
    c.setRecoveryDrogueMainLowSpeedWarning(4);
    c.setRandomSeed(77);
    const std::shared_ptr<Marker> marker = std::make_shared<Marker>();
    marker->setValue(9);
    c.getSimulationListenerList().push_back(marker);
    c.getSimulationListenerList().push_back(std::make_shared<Marker>());

    SimulationConditions k = c.clone();

    // Shared: the models, the calculator, the simulation.
    EXPECT_EQ(k.getWindModel(), c.getWindModel());
    EXPECT_EQ(k.getAtmosphericModel(), c.getAtmosphericModel());
    EXPECT_EQ(k.getGravityModel(), c.getGravityModel());
    EXPECT_EQ(k.getAerodynamicCalculator(), c.getAerodynamicCalculator());
    EXPECT_TRUE(k.getMassCalculator().has_value());
    EXPECT_EQ(k.getSimulation(), &simulation);
    EXPECT_EQ(k.getModId(), c.getModId());

    // Cloned one by one, in order: the listeners.
    ASSERT_EQ(k.getSimulationListenerList().size(), 2U);
    EXPECT_NE(k.getSimulationListenerList()[0], c.getSimulationListenerList()[0]);
    EXPECT_NE(k.getSimulationListenerList()[1], c.getSimulationListenerList()[1]);
    const auto cloned = std::dynamic_pointer_cast<Marker>(k.getSimulationListenerList()[0]);
    ASSERT_NE(cloned, nullptr);
    EXPECT_EQ(cloned->value(), 9);
    const auto second = std::dynamic_pointer_cast<Marker>(k.getSimulationListenerList()[1]);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->value(), 1);
    EXPECT_EQ(c.getSimulationListenerList()[0], marker) << "the original keeps the caller's object";

    // Copied: every value.
    EXPECT_EQ(k.getLaunchRodLength(), 1.5);
    EXPECT_EQ(k.getLaunchRodAngle(), 0.25);
    EXPECT_EQ(k.getLaunchRodDirection(), 2.5);
    EXPECT_TRUE(k.getLaunchPosition().exactlyEquals(Coordinate{0, 0, 5}));
    EXPECT_TRUE(k.getLaunchVelocity().exactlyEquals(Coordinate{1, 2, 3}));
    EXPECT_EQ(k.getLaunchSite(), (WorldCoordinate{10, 20, 30}));
    EXPECT_EQ(k.getGeodeticComputation(), GeodeticComputationStrategy::FLAT);
    EXPECT_EQ(k.getTimeStep(), 0.05);
    EXPECT_EQ(k.getMaxSimulationTime(), 99.0);
    EXPECT_EQ(k.getMaximumAngleStep(), 0.07);
    EXPECT_EQ(k.getRecoverySpeedWarning(), 1.0);
    EXPECT_EQ(k.getDrogueLowSpeedWarning(), 2.0);
    EXPECT_EQ(k.getRecoveryDrogueMainHighSpeedWarning(), 3.0);
    EXPECT_EQ(k.getRecoveryDrogueMainLowSpeedWarning(), 4.0);
    EXPECT_EQ(k.getRandomSeed(), 77);

    // The clone is its own object.
    k.setTimeStep(0.5);
    k.getSimulationListenerList().clear();
    k.setWindModel(nullptr);
    EXPECT_EQ(c.getTimeStep(), 0.05);
    EXPECT_EQ(c.getSimulationListenerList().size(), 2U);
    EXPECT_NE(c.getWindModel(), nullptr);
    EXPECT_NE(k.getModId(), c.getModId());

    // A clone of a clone clones the clones.
    const SimulationConditions again = c.clone().clone();
    ASSERT_EQ(again.getSimulationListenerList().size(), 2U);
    EXPECT_NE(again.getSimulationListenerList()[0], marker);
}

TEST(SimulationConditions, ACloneOfConditionsWithoutListenersHasNone)
{
    const SimulationConditions c;
    const SimulationConditions k = c.clone();
    EXPECT_TRUE(k.getSimulationListenerList().empty());
    EXPECT_EQ(k.getModId(), ModId::invalid());
}

TEST(SimulationConditions, ANullListenerCannotBeCloned)
{
    // Java: a NullPointerException in clone().
    SimulationConditions c;
    c.getSimulationListenerList().push_back(std::make_shared<Marker>());
    c.getSimulationListenerList().push_back(nullptr);
    EXPECT_EQ(bugText([&] { static_cast<void>(c.clone()); }),
              "The simulation listener list holds a null listener");
}

TEST(SimulationConditions, MovingHandsTheConditionsOver)
{
    SimulationConditions c;
    c.setTimeStep(0.02);
    c.setWindModel(std::make_shared<PinkNoiseWindModel>(3));
    c.getSimulationListenerList().push_back(std::make_shared<Marker>());
    const ModId                                id       = c.getModId();
    const std::shared_ptr<QtRocket::WindModel> wind     = c.getWindModel();
    const std::shared_ptr<SimulationListener>  listener = c.getSimulationListenerList()[0];

    SimulationConditions moved = std::move(c);
    EXPECT_EQ(moved.getTimeStep(), 0.02);
    EXPECT_EQ(moved.getModId(), id);
    EXPECT_EQ(moved.getWindModel(), wind);
    ASSERT_EQ(moved.getSimulationListenerList().size(), 1U);
    EXPECT_EQ(moved.getSimulationListenerList()[0], listener) << "the very listener, not a clone";

    // What a simulation status holds: conditions on the heap, made from the moved ones.
    const std::shared_ptr<SimulationConditions> shared =
        std::make_shared<SimulationConditions>(std::move(moved));
    EXPECT_EQ(shared->getModId(), id);
    EXPECT_EQ(shared->getSimulationListenerList()[0], listener);
}

}  // namespace
