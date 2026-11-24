#include "QtRocket/simulation/SimulationProgressEstimator.h"

// Tests of the progress estimate of OpenRocket's run dialog (swing gui/simulation/
// SimulationRunDialog.java, InteractiveSimulationWorker, :307-487), which no JUnit test covers:
// the burn-out estimate from the active motors, the three stages with their switches, the
// maxima, the progress in percent and the texts the dialog shows. The samples are made by hand,
// so every expected value is computed with + - * / alone (the Java formulas written out below)
// and compared exactly; the samples of whole flights are RunSampleListenerTests'.

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/RunSample.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/MathUtil.h"
#include "rocket/TestRockets.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::IgnitionEvent;
using QtRocket::MotorConfiguration;
using QtRocket::MotorMount;
using QtRocket::RunSample;
using QtRocket::SimulationProgressEstimator;
using QtRocket::unitGroup;
using QtRocket::UnitGroup;
using QtRocket::UnitGroupId;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::motorA10;
using QtRocket::Test::motorC6;
using QtRocket::Test::motorD21;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestEstesAlphaIIIWithMotorPods;
using QtRocket::Test::TestEstesAlphaIIIWithSecondMotor;
using QtRocket::Test::testFcid;
using QtRocket::Test::TestMultiStageEventTestRocket;
using Stage = QtRocket::SimulationProgressEstimator::Stage;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// A sample at @p time, @p altitude and @p verticalVelocity, of the sustainer unless
/// @p sustainerActive is false.
[[nodiscard]] RunSample sampleAt(double time, double altitude, double verticalVelocity,
                                 bool sustainerActive = true)
{
    return RunSample{.simulationTime   = time,
                     .altitude         = altitude,
                     .verticalVelocity = verticalVelocity,
                     .sustainerActive  = sustainerActive};
}

/// Takes in the one sample @p sample.
void processOne(SimulationProgressEstimator& estimator, const RunSample& sample)
{
    const std::vector<RunSample> samples{sample};
    estimator.process(samples);
}

/// MathUtil.map() of Java for a range that is not singular, as it is written there.
[[nodiscard]] double javaMap(double value, double fromMin, double fromMax, double toMin,
                             double toMax)
{
    return ((value - fromMin) / (fromMax - fromMin) * (toMax - toMin)) + toMin;
}

/// The progress setSimulationProgress(@p fraction) gives from a progress of 0: (int) (100 * p +
/// 0.5), clamped to 0 ... 100.
[[nodiscard]] int javaPercent(double fraction)
{
    return QtRocket::MathUtil::clamp(QtRocket::MathUtil::javaIntCast((100.0 * fraction) + 0.5), 0,
                                     100);
}

/// The burn-time estimate of the motor @p mount has for @p fcid; NaN when it has none.
[[nodiscard]] double burnTimeIn(const MotorMount& mount, const FlightConfigurationId& fcid)
{
    const std::shared_ptr<const QtRocket::Motor>& motor = mount.getMotorConfig(fcid).getMotor();
    if (motor == nullptr)
    {
        ADD_FAILURE() << "the mount has no motor";
        return kNaN;
    }
    return motor->getBurnTimeEstimate();
}

/// Gives the motor configuration of @p mount for @p fcid the ignition event @p event (a copy
/// that replaces it), as the motor configuration panel does.
void setIgnitionEvent(MotorMount& mount, const FlightConfigurationId& fcid, IgnitionEvent event)
{
    MotorConfiguration changed{mount, fcid, mount.getMotorConfig(fcid)};
    changed.setIgnitionEvent(event);
    mount.setMotorConfig(std::move(changed), fcid);
}

// --------------------------------------------------------------------- the burn-out estimate

// The center booster's C6 and the side boosters' A10 ignite at LAUNCH: the longer of the two
// counts. The sustainer's C6 ignites at the BURNOUT of the stage below: it is added.
TEST(SimulationProgressEstimator, TheBurnoutEstimateIsTheLongestLaunchMotorPlusTheOthers)
{
    const TestMultiStageEventTestRocket r;
    const FlightConfiguration&          config = r.rocket->getFlightConfiguration(r.fcid);
    ASSERT_EQ(config.getActiveMotors().size(), 3U);
    const double c6  = motorC6()->getBurnTimeEstimate();
    const double a10 = motorA10()->getBurnTimeEstimate();
    ASSERT_NE(c6, a10);

    EXPECT_EQ(SimulationProgressEstimator::estimateBurnoutTime(config), std::max(c6, a10) + c6);
    EXPECT_EQ(SimulationProgressEstimator(config).getBurnoutTimeEstimate(), std::max(c6, a10) + c6);
}

// AUTOMATIC, the default ignition event, is not LAUNCH: two motors ignited automatically are
// added (Java compares with == IgnitionEvent.LAUNCH), two at LAUNCH give the longer.
TEST(SimulationProgressEstimator, MotorsIgnitedAutomaticallyAreAdded)
{
    TestEstesAlphaIIIWithSecondMotor alpha;
    FlightConfiguration&             config = alpha.rocket->getFlightConfiguration(testFcid(0));
    // The maker gives the second mount its motors without an event.
    config.update();
    ASSERT_EQ(config.getActiveMotors().size(), 2U);
    ASSERT_EQ(config.getActiveMotors()[0].getIgnitionEvent(), IgnitionEvent::AUTOMATIC);
    ASSERT_EQ(config.getActiveMotors()[1].getIgnitionEvent(), IgnitionEvent::AUTOMATIC);
    const double a8  = burnTimeIn(*alpha.inner, testFcid(0));
    const double a10 = burnTimeIn(*alpha.secondMount, testFcid(0));
    ASSERT_NE(a8, a10);
    EXPECT_EQ(SimulationProgressEstimator::estimateBurnoutTime(config), a8 + a10);

    setIgnitionEvent(*alpha.inner, testFcid(0), IgnitionEvent::LAUNCH);
    setIgnitionEvent(*alpha.secondMount, testFcid(0), IgnitionEvent::LAUNCH);
    config.update();
    ASSERT_EQ(config.getActiveMotors()[0].getIgnitionEvent(), IgnitionEvent::LAUNCH);
    ASSERT_EQ(config.getActiveMotors()[1].getIgnitionEvent(), IgnitionEvent::LAUNCH);
    EXPECT_EQ(SimulationProgressEstimator::estimateBurnoutTime(config), std::max(a8, a10));
}

// One motor configuration per mount, whatever the mount's instances: the two pods share one.
TEST(SimulationProgressEstimator, AMountCountsOnceWhateverItsInstances)
{
    TestEstesAlphaIIIWithMotorPods alpha;
    FlightConfiguration&           config = alpha.rocket->getFlightConfiguration(testFcid(0));
    config.update();
    ASSERT_EQ(config.getActiveMotors().size(), 2U);
    EXPECT_EQ(SimulationProgressEstimator::estimateBurnoutTime(config),
              burnTimeIn(*alpha.inner, testFcid(0)) + burnTimeIn(*alpha.podMount, testFcid(0)));
}

// Only the motors of active stages count: the booster's D21 drops out with its stage.
TEST(SimulationProgressEstimator, OnlyTheMotorsOfActiveStagesCount)
{
    TestBeta             beta;
    FlightConfiguration& config = beta.rocket->getFlightConfiguration(testFcid(1));
    ASSERT_EQ(config.getActiveMotors().size(), 2U);
    const double b4  = burnTimeIn(*beta.inner, testFcid(1));
    const double d21 = burnTimeIn(*beta.boosterMmt, testFcid(1));
    EXPECT_EQ(d21, motorD21()->getBurnTimeEstimate());
    EXPECT_EQ(SimulationProgressEstimator::estimateBurnoutTime(config), b4 + d21);

    config.setStageActive(1, false);
    ASSERT_EQ(config.getActiveMotors().size(), 1U);
    EXPECT_EQ(SimulationProgressEstimator::estimateBurnoutTime(config), b4);
}

// At least 0.1 s: without motors, and with a motor that burns for less.
TEST(SimulationProgressEstimator, TheBurnoutEstimateIsAtLeastATenthOfASecond)
{
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& empty = alpha.rocket->getSelectedConfiguration();
    ASSERT_FALSE(empty.hasMotors());
    EXPECT_EQ(SimulationProgressEstimator::estimateBurnoutTime(empty), 0.1);
    EXPECT_EQ(SimulationProgressEstimator::kMinimumBurnoutTime, 0.1);

    TestEstesAlphaIII alphaWithAShortMotor;
    const auto        shortMotor =
        QtRocket::Test::makeTestMotor("Test", "S1", "A short motor", "case", {0.0}, 0.018, 0.07,
                                      {0.0, 0.02, 0.05}, {0.0, 10.0, 0.0}, {0.01, 0.005, 0.0});
    ASSERT_LT(shortMotor->getBurnTimeEstimate(), 0.1);
    QtRocket::Test::addMotor(*alphaWithAShortMotor.inner, testFcid(0), shortMotor);
    FlightConfiguration& config = alphaWithAShortMotor.rocket->getFlightConfiguration(testFcid(0));
    config.update();
    ASSERT_EQ(config.getActiveMotors().size(), 1U);
    ASSERT_EQ(config.getActiveMotors()[0].getMotor(), shortMotor);
    EXPECT_EQ(SimulationProgressEstimator::estimateBurnoutTime(config), 0.1);
}

// A burn-out estimate given as a number has the same floor, so no range the estimate maps from
// is singular: a launch sample at 0 s with an estimate of nearly 0 maps (MathUtil::map would
// throw for a range of 0 ... 1e-10).
TEST(SimulationProgressEstimator, AGivenBurnoutEstimateIsAtLeastATenthOfASecondToo)
{
    EXPECT_EQ(SimulationProgressEstimator(0.0).getBurnoutTimeEstimate(), 0.1);
    EXPECT_EQ(SimulationProgressEstimator(-1.0).getBurnoutTimeEstimate(), 0.1);
    EXPECT_EQ(SimulationProgressEstimator(0.25).getBurnoutTimeEstimate(), 0.25);
    EXPECT_TRUE(std::isnan(SimulationProgressEstimator(kNaN).getBurnoutTimeEstimate()));

    SimulationProgressEstimator estimator(1e-10);
    processOne(estimator, sampleAt(0.0, 0.0, 0.0));
    EXPECT_EQ(estimator.getStage(), Stage::BOOST);
    EXPECT_EQ(estimator.getProgress(), 0);
}

// ------------------------------------------------------------------------------ the stages

TEST(SimulationProgressEstimator, StartsInTheBoostWithNothingToShow)
{
    const DefaultUnitsGuard           units;
    const SimulationProgressEstimator estimator(2.0);
    EXPECT_EQ(estimator.getProgress(), 0);
    EXPECT_EQ(estimator.getStage(), Stage::BOOST);
    EXPECT_EQ(estimator.getBurnoutTimeEstimate(), 2.0);
    EXPECT_EQ(estimator.getBurnoutVelocity(), 0.0);
    EXPECT_EQ(estimator.getApogeeAltitude(), 0.0);
    EXPECT_EQ(estimator.getMaxAltitude(), 0.0);
    EXPECT_EQ(estimator.getMaxVelocity(), 0.0);
    EXPECT_FALSE(estimator.getLastSample().has_value());
    EXPECT_EQ(estimator.getTimeText(), "");
    EXPECT_EQ(estimator.getAltitudeText(), "");
    EXPECT_EQ(estimator.getVelocityText(), "");
}

// Boost: the time from 0 to the burn-out estimate onto 0 ... 40 %, rounded half up.
TEST(SimulationProgressEstimator, TheBoostMapsTheTimeOntoTheFirstFortyPercent)
{
    SimulationProgressEstimator estimator(2.0);
    processOne(estimator, sampleAt(0.0, 0.0, 0.0));
    EXPECT_EQ(estimator.getProgress(), 0);
    EXPECT_EQ(estimator.getStage(), Stage::BOOST);

    processOne(estimator, sampleAt(1.0, 20.0, 40.0));
    EXPECT_EQ(estimator.getProgress(), 20);
    EXPECT_EQ(estimator.getProgress(), javaPercent(javaMap(1.0, 0.0, 2.0, 0.0, 0.4)));

    // 0.625 s is 12.5 %, which rounds up.
    SimulationProgressEstimator rounding(2.0);
    processOne(rounding, sampleAt(0.625, 5.0, 20.0));
    EXPECT_EQ(rounding.getProgress(), 13);

    processOne(estimator, sampleAt(1.9, 60.0, 80.0));
    EXPECT_EQ(estimator.getProgress(), javaPercent(javaMap(1.9, 0.0, 2.0, 0.0, 0.4)));
    EXPECT_EQ(estimator.getStage(), Stage::BOOST);
}

// The progress never goes back, whatever a later sample says.
TEST(SimulationProgressEstimator, TheProgressNeverGoesBack)
{
    SimulationProgressEstimator estimator(2.0);
    processOne(estimator, sampleAt(1.0, 20.0, 40.0));
    ASSERT_EQ(estimator.getProgress(), 20);
    processOne(estimator, sampleAt(0.5, 10.0, 30.0));
    EXPECT_EQ(estimator.getProgress(), 20);
    EXPECT_EQ(estimator.getLastSample(), sampleAt(0.5, 10.0, 30.0));
}

// Coast: from the first sample at the burn-out estimate, the vertical velocity from the burn-out
// velocity (that sample's) to 0 onto 40 ... 70 %.
TEST(SimulationProgressEstimator, TheCoastMapsTheVelocityFromTheBurnoutVelocityToZero)
{
    SimulationProgressEstimator estimator(2.0);
    processOne(estimator, sampleAt(2.0, 100.0, 50.0));
    EXPECT_EQ(estimator.getStage(), Stage::COAST);
    EXPECT_EQ(estimator.getBurnoutVelocity(), 50.0);
    EXPECT_EQ(estimator.getProgress(), 40);

    processOne(estimator, sampleAt(3.0, 140.0, 25.0));
    EXPECT_EQ(estimator.getProgress(), javaPercent(javaMap(25.0, 50.0, 0.0, 0.4, 0.7)));
    EXPECT_EQ(estimator.getProgress(), 55);
    EXPECT_EQ(estimator.getStage(), Stage::COAST);

    // A vertical velocity of 0 is still the coast: 70 %.
    processOne(estimator, sampleAt(4.0, 160.0, 0.0));
    EXPECT_EQ(estimator.getStage(), Stage::COAST);
    EXPECT_EQ(estimator.getProgress(), 70);
    // The burn-out velocity stays.
    EXPECT_EQ(estimator.getBurnoutVelocity(), 50.0);
}

// The burn-out velocity is at least 0.1 m/s (MathUtil.max, which skips a NaN).
TEST(SimulationProgressEstimator, TheBurnoutVelocityIsAtLeastATenthOfAMetrePerSecond)
{
    SimulationProgressEstimator slow(1.0);
    processOne(slow, sampleAt(1.0, 10.0, 0.05));
    EXPECT_EQ(slow.getBurnoutVelocity(), 0.1);
    EXPECT_EQ(slow.getProgress(), javaPercent(javaMap(0.05, 0.1, 0.0, 0.4, 0.7)));

    SimulationProgressEstimator unknown(1.0);
    processOne(unknown, sampleAt(1.0, 10.0, kNaN));
    EXPECT_EQ(unknown.getBurnoutVelocity(), 0.1);
}

// Landing: from the first sample with a negative vertical velocity, the altitude from the apogee
// altitude (the maximum altitude then) to 0 onto 70 ... 100 %.
TEST(SimulationProgressEstimator, TheLandingMapsTheAltitudeFromTheApogeeToTheGround)
{
    SimulationProgressEstimator estimator(2.0);
    processOne(estimator, sampleAt(2.0, 100.0, 50.0));
    processOne(estimator, sampleAt(5.0, 200.0, 0.5));
    ASSERT_EQ(estimator.getStage(), Stage::COAST);
    ASSERT_EQ(estimator.getMaxAltitude(), 200.0);

    processOne(estimator, sampleAt(5.5, 199.0, -2.0));
    EXPECT_EQ(estimator.getStage(), Stage::LANDING);
    EXPECT_EQ(estimator.getApogeeAltitude(), 200.0);
    EXPECT_EQ(estimator.getProgress(), javaPercent(javaMap(199.0, 200.0, 0.0, 0.7, 1.0)));

    processOne(estimator, sampleAt(20.0, 50.0, -5.0));
    EXPECT_EQ(estimator.getProgress(), javaPercent(javaMap(50.0, 200.0, 0.0, 0.7, 1.0)));
    EXPECT_EQ(estimator.getProgress(), 93);
    // The landing stays, whatever the velocity.
    processOne(estimator, sampleAt(21.0, 40.0, 3.0));
    EXPECT_EQ(estimator.getStage(), Stage::LANDING);
    EXPECT_EQ(estimator.getApogeeAltitude(), 200.0);
    EXPECT_EQ(estimator.getProgress(), javaPercent(javaMap(40.0, 200.0, 0.0, 0.7, 1.0)));

    // On the ground: 100 %.
    processOne(estimator, sampleAt(25.0, 0.0, 0.0));
    EXPECT_EQ(estimator.getProgress(), 100);
}

// The apogee altitude is at least 1 m (MathUtil.max), and below the ground the progress stays
// at 100 %.
TEST(SimulationProgressEstimator, TheApogeeIsAtLeastOneMetreAndTheProgressAtMostAHundred)
{
    SimulationProgressEstimator estimator(0.5);
    processOne(estimator, sampleAt(0.5, 0.25, 0.5));
    processOne(estimator, sampleAt(1.0, 0.4, -0.5));
    EXPECT_EQ(estimator.getStage(), Stage::LANDING);
    EXPECT_EQ(estimator.getMaxAltitude(), 0.4);
    EXPECT_EQ(estimator.getApogeeAltitude(), 1.0);
    EXPECT_EQ(estimator.getProgress(), javaPercent(javaMap(0.4, 1.0, 0.0, 0.7, 1.0)));

    processOne(estimator, sampleAt(1.5, -0.5, -1.0));
    EXPECT_GT(javaMap(-0.5, 1.0, 0.0, 0.7, 1.0), 1.0);
    EXPECT_EQ(estimator.getProgress(), 100);
}

// One sample can leave the boost and the coast at once: past the burn-out estimate and falling.
TEST(SimulationProgressEstimator, OneSampleCanGoFromTheBoostToTheLanding)
{
    SimulationProgressEstimator estimator(2.0);
    processOne(estimator, sampleAt(2.5, 80.0, -3.0));
    EXPECT_EQ(estimator.getStage(), Stage::LANDING);
    EXPECT_EQ(estimator.getBurnoutVelocity(), 0.1);
    // The sample is higher than the maximum so far, which it becomes.
    EXPECT_EQ(estimator.getMaxAltitude(), 80.0);
    EXPECT_EQ(estimator.getMaxVelocity(), 0.0);
    EXPECT_EQ(estimator.getApogeeAltitude(), 80.0);
    EXPECT_EQ(estimator.getProgress(), 70);
}

// A NaN burn-out estimate: the time is never below it, so the boost never applies.
TEST(SimulationProgressEstimator, ANaNBurnoutEstimateSkipsTheBoost)
{
    SimulationProgressEstimator estimator(kNaN);
    processOne(estimator, sampleAt(0.0, 0.0, 10.0));
    EXPECT_EQ(estimator.getStage(), Stage::COAST);
    EXPECT_EQ(estimator.getBurnoutVelocity(), 10.0);
    EXPECT_EQ(estimator.getProgress(), 40);
}

// Deviation (a bug found while porting): a NaN velocity in the coast is neither negative nor
// not. Java goes on to the landing estimate with an apogee altitude of 0 and MathUtil.map throws
// for the singular range; here the progress and the stage stay, and the sample is the last one.
TEST(SimulationProgressEstimator, ANaNVelocityInTheCoastLeavesTheProgress)
{
    SimulationProgressEstimator estimator(1.0);
    processOne(estimator, sampleAt(1.0, 50.0, 20.0));
    ASSERT_EQ(estimator.getStage(), Stage::COAST);
    const int progress = estimator.getProgress();

    processOne(estimator, sampleAt(1.5, 55.0, kNaN));
    EXPECT_EQ(estimator.getStage(), Stage::COAST);
    EXPECT_EQ(estimator.getApogeeAltitude(), 0.0);
    EXPECT_EQ(estimator.getProgress(), progress);
    ASSERT_TRUE(estimator.getLastSample().has_value());
    EXPECT_EQ(estimator.getLastSample().value_or(RunSample{}).simulationTime, 1.5);

    // The next sample goes on as before.
    processOne(estimator, sampleAt(2.0, 50.0, -1.0));
    EXPECT_EQ(estimator.getStage(), Stage::LANDING);
    EXPECT_EQ(estimator.getApogeeAltitude(), 55.0);
}

// --------------------------------------------------------------------------- the maxima

// Every sample of a batch counts for the maxima; the last one alone for the stage and the
// progress.
TEST(SimulationProgressEstimator, EverySampleOfABatchCountsForTheMaxima)
{
    SimulationProgressEstimator  estimator(2.0);
    const std::vector<RunSample> samples{sampleAt(0.5, 10.0, 20.0), sampleAt(1.0, 30.0, 40.0),
                                         sampleAt(1.5, 25.0, -1.0)};
    estimator.process(samples);
    EXPECT_EQ(estimator.getMaxAltitude(), 30.0);
    EXPECT_EQ(estimator.getMaxVelocity(), 40.0);
    EXPECT_EQ(estimator.getLastSample(), sampleAt(1.5, 25.0, -1.0));
    EXPECT_EQ(estimator.getStage(), Stage::BOOST);
    EXPECT_EQ(estimator.getProgress(), javaPercent(javaMap(1.5, 0.0, 2.0, 0.0, 0.4)));
}

// Only the samples of the active sustainer count: not those of a stage that separated.
TEST(SimulationProgressEstimator, OnlyTheSustainerCountsForTheMaxima)
{
    SimulationProgressEstimator estimator(2.0);
    processOne(estimator, sampleAt(0.5, 10.0, 20.0));
    processOne(estimator, sampleAt(1.0, 30.0, 40.0, false));
    EXPECT_EQ(estimator.getMaxAltitude(), 10.0);
    EXPECT_EQ(estimator.getMaxVelocity(), 20.0);
    // It is still the last sample, and the progress follows it.
    EXPECT_EQ(estimator.getLastSample(), sampleAt(1.0, 30.0, 40.0, false));
    EXPECT_EQ(estimator.getProgress(), 20);
}

// A rising sustainer sets the maximum altitude, also to a lower one ("the last apogee found is
// the real one"); a falling one only when it is higher. The maximum velocity only rises.
TEST(SimulationProgressEstimator, ARisingSustainerSetsTheMaximumAltitude)
{
    SimulationProgressEstimator estimator(2.0);
    processOne(estimator, sampleAt(0.5, 100.0, 30.0));
    processOne(estimator, sampleAt(1.0, 90.0, -1.0));
    EXPECT_EQ(estimator.getMaxAltitude(), 100.0);
    processOne(estimator, sampleAt(1.5, 50.0, 10.0));
    EXPECT_EQ(estimator.getMaxAltitude(), 50.0);
    EXPECT_EQ(estimator.getMaxVelocity(), 30.0);
    processOne(estimator, sampleAt(1.8, 60.0, -2.0));
    EXPECT_EQ(estimator.getMaxAltitude(), 60.0);
    EXPECT_EQ(estimator.getMaxVelocity(), 30.0);
}

// Java's Math.max: a NaN velocity of a sample that counts makes the maximum velocity NaN.
TEST(SimulationProgressEstimator, ANaNVelocityThatCountsMakesTheMaximumNaN)
{
    SimulationProgressEstimator estimator(2.0);
    processOne(estimator, sampleAt(0.5, 10.0, 20.0));
    processOne(estimator, sampleAt(1.0, 30.0, kNaN));
    EXPECT_EQ(estimator.getMaxAltitude(), 30.0);
    EXPECT_TRUE(std::isnan(estimator.getMaxVelocity()));
}

// No samples: nothing changes.
TEST(SimulationProgressEstimator, NoSamplesChangeNothing)
{
    SimulationProgressEstimator estimator(2.0);
    processOne(estimator, sampleAt(1.0, 20.0, 40.0));
    estimator.process({});
    EXPECT_EQ(estimator.getProgress(), 20);
    EXPECT_EQ(estimator.getLastSample(), sampleAt(1.0, 20.0, 40.0));
    EXPECT_EQ(estimator.getMaxAltitude(), 20.0);
    EXPECT_EQ(estimator.getStage(), Stage::BOOST);
}

// ------------------------------------------------------------------------------ the end

// The end of a run, however it ended, is 100 %; the rest stays, and later samples are taken in
// (the progress stays at 100 %).
TEST(SimulationProgressEstimator, TheEndIsAHundredPercent)
{
    SimulationProgressEstimator before(2.0);
    before.finish();
    EXPECT_EQ(before.getProgress(), 100);
    EXPECT_EQ(before.getStage(), Stage::BOOST);
    EXPECT_FALSE(before.getLastSample().has_value());

    SimulationProgressEstimator estimator(2.0);
    processOne(estimator, sampleAt(1.0, 20.0, 40.0));
    estimator.finish();
    EXPECT_EQ(estimator.getProgress(), 100);
    EXPECT_EQ(estimator.getLastSample(), sampleAt(1.0, 20.0, 40.0));
    processOne(estimator, sampleAt(2.5, 60.0, 30.0));
    EXPECT_EQ(estimator.getProgress(), 100);
    EXPECT_EQ(estimator.getStage(), Stage::COAST);
    EXPECT_EQ(estimator.getMaxAltitude(), 60.0);
}

// ------------------------------------------------------------------------------ the texts

// The values of the dialog's three rows, in the default units, as Java composes them.
TEST(SimulationProgressEstimator, TheTextsAreTheLastSampleAndTheMaximaInTheDefaultUnits)
{
    const DefaultUnitsGuard     units;
    SimulationProgressEstimator estimator(2.0);
    processOne(estimator, sampleAt(4.0, 150.0, 10.0));
    processOne(estimator, sampleAt(12.5, 120.0, -5.0));

    EXPECT_EQ(estimator.getTimeText(), "12.5 s");
    EXPECT_EQ(estimator.getAltitudeText(), "120 m (max. 150 m)");
    EXPECT_EQ(estimator.getVelocityText(), "-5 m/s (max. 10 m/s)");

    UnitGroup::setDefaultImperialUnits();
    const UnitGroup& distance = unitGroup(UnitGroupId::DISTANCE);
    const UnitGroup& velocity = unitGroup(UnitGroupId::VELOCITY);
    EXPECT_EQ(estimator.getTimeText(), unitGroup(UnitGroupId::LONG_TIME).toStringUnit(12.5));
    EXPECT_EQ(estimator.getAltitudeText(),
              distance.toStringUnit(120.0) + " (max. " + distance.toStringUnit(150.0) + ")");
    EXPECT_EQ(estimator.getVelocityText(),
              velocity.toStringUnit(-5.0) + " (max. " + velocity.toStringUnit(10.0) + ")");
    EXPECT_EQ(estimator.getAltitudeText(), "394 ft (max. 492 ft)");
}

TEST(SimulationProgressEstimator, TheStagesHaveNames)
{
    EXPECT_EQ(name(Stage::BOOST), "BOOST");
    EXPECT_EQ(name(Stage::COAST), "COAST");
    EXPECT_EQ(name(Stage::LANDING), "LANDING");
    EXPECT_EQ(static_cast<int>(Stage::BOOST), -2);
    EXPECT_EQ(static_cast<int>(Stage::COAST), -1);
    EXPECT_EQ(static_cast<int>(Stage::LANDING), 0);
}

}  // namespace
