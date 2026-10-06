#include "QtRocket/simulation/TumbleDetector.h"

#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <span>
#include <type_traits>

#include <gtest/gtest.h>

#include "QtRocket/util/MathUtil.h"
#include "simulation/SimulationOptionsSupport.h"

namespace
{

using QtRocket::TumbleDetector;
using QtRocket::Test::isJavaValue;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// ---------------------------------------------------------------- TumbleDetectorTest.java
//
// The cases that drive the detector directly. The integration cases of the Java class
// (testSteadyCrosswindDoesNotAbort, testRecoveredFlightReachesExpectedApogee,
// testUnstableRocketIsStillDetected) run a simulation: they are in
// tumble_detector_simulation_tests.cpp.

/// TumbleDetectorTest.SEA_LEVEL_DENSITY
constexpr double kSeaLevelDensity = 1.225;

/// TumbleDetectorTest.OMEGA_N: a representative pitch natural frequency for a small model
/// rocket, rad/s.
constexpr double kOmegaN = 10.0;

/// TumbleDetectorTest.PITCH_PERIOD: one pitch period at kOmegaN, s.
constexpr double kPitchPeriod = 2 * std::numbers::pi / kOmegaN;

/// TumbleDetectorTest.drive(): drives @p detector for @p duration at a fixed angle of attack
/// (@p aoaDeg, degrees) and air speed, starting at @p startTime; returns the simulation time
/// after the last step.
double drive(TumbleDetector& detector, double aoaDeg, double airSpeed, double duration,
             double startTime)
{
    const double dt    = 0.01;
    double       time  = startTime;
    const long   steps = std::lround(duration / dt);
    for (long i = 0; i < steps; i++)
    {
        time += dt;
        detector.update(time, true, QtRocket::MathUtil::deg2rad(aoaDeg), airSpeed, kSeaLevelDensity,
                        kOmegaN);
    }
    return time;
}

// TumbleDetectorTest.testAlignedFlightIsNotTumbling
TEST(TumbleDetector, AlignedFlightIsNotTumbling)
{
    TumbleDetector detector;
    drive(detector, 5, 100.0, 5.0, 0.0);

    EXPECT_FALSE(detector.isTumbling())
        << "a rocket flying along its own axis must never be reported as tumbling";
}

// TumbleDetectorTest.testSustainedHighAOAIsTumbling
TEST(TumbleDetector, SustainedHighAOAIsTumbling)
{
    TumbleDetector detector;
    drive(detector, 120, 20.0, 5.0, 0.0);

    EXPECT_TRUE(detector.isTumbling())
        << "a sustained high angle of attack must be reported as tumbling";
}

// TumbleDetectorTest.testBriefExcursionIsNotTumbling: the regression that issue #3183 is really
// about: a brief excursion must not latch the detector. A statically stable rocket recovers
// from a gust-induced excursion within a fraction of its pitch period.
TEST(TumbleDetector, BriefExcursionIsNotTumbling)
{
    TumbleDetector detector;

    double time = drive(detector, 2, 60.0, 1.0, 0.0);
    // The excursion lasts a tenth of a pitch period.
    time = drive(detector, 150, 60.0, kPitchPeriod / 10, time);
    drive(detector, 2, 60.0, 1.0, time);

    EXPECT_FALSE(detector.isTumbling())
        << "a transient excursion shorter than a pitch period must not trigger tumbling";
}

// TumbleDetectorTest.testSingleStepSpikeIsNotTumbling: a single step above the stall angle,
// which a turbulence sample can produce on its own, must not be enough.
TEST(TumbleDetector, SingleStepSpikeIsNotTumbling)
{
    TumbleDetector detector;

    double time = drive(detector, 3, 20.0, 1.0, 0.0);
    time        = drive(detector, 179, 20.0, 0.01, time);
    static_cast<void>(time);

    EXPECT_FALSE(detector.isTumbling())
        << "one integration step at a high angle of attack must not trigger tumbling";
}

// TumbleDetectorTest.testDescentTumblingIsDetected: tumbling must still be detected on the way
// down, for rockets that recover by tumbling rather than under a parachute.
TEST(TumbleDetector, DescentTumblingIsDetected)
{
    TumbleDetector detector;
    // Descending at 15 m/s broadside to the airflow.
    drive(detector, 95, 15.0, 5.0, 0.0);

    EXPECT_TRUE(detector.isTumbling())
        << "descent tumbling must be detected; the criterion must not be gated on flight phase";
}

// TumbleDetectorTest.testStateIsHeldWhileAirflowIsNegligible: near apogee the velocity
// direction is ill-conditioned. The detector must hold its state rather than accumulate noise,
// and must not reset what it learned before apogee.
TEST(TumbleDetector, StateIsHeldWhileAirflowIsNegligible)
{
    TumbleDetector detector;
    drive(detector, 120, 20.0, 5.0, 0.0);
    ASSERT_TRUE(detector.isTumbling()) << "precondition: detector has latched onto tumbling";

    const double before = detector.getFilteredAOA();
    // Below the dynamic pressure gate: ~1.3 m/s at sea level.
    drive(detector, 0, 0.5, 2.0, 5.0);

    EXPECT_NEAR(before, detector.getFilteredAOA(), 1e-12)
        << "filter state must be held, not updated, while the airflow direction is meaningless";
    EXPECT_TRUE(detector.isTumbling())
        << "passing through apogee must not discard evidence accumulated before it";
}

// TumbleDetectorTest.testCopyPreservesFilterState
TEST(TumbleDetector, CopyPreservesFilterState)
{
    TumbleDetector detector;
    drive(detector, 120, 20.0, 5.0, 0.0);

    const TumbleDetector copy(detector);

    EXPECT_NEAR(detector.getFilteredAOA(), copy.getFilteredAOA(), 1e-12)
        << "a branch created at stage separation must inherit its parent's filter state";
    EXPECT_EQ(detector.isTumbling(), copy.isTumbling());
}

// ============================================================================ Java's values

TEST(TumbleDetector, TheConstantsAreJavas)
{
    // 60 * Math.PI / 180 (probes/tier8b-status/ConditionsProbe.java)
    EXPECT_EQ(TumbleDetector::kTumbleThreshold, 1.0471975511965976);
    EXPECT_EQ(TumbleDetector::kMinDynamicPressure, 1.0);
    EXPECT_EQ(TumbleDetector::kDwellPeriods, 2.0);
    EXPECT_EQ(TumbleDetector::kMinTimeConstant, 0.05);
    EXPECT_EQ(TumbleDetector::kMaxTimeConstant, 2.0);
}

TEST(TumbleDetector, ANewDetectorHasSeenNothing)
{
    const TumbleDetector detector;
    EXPECT_EQ(detector.getFilteredAOA(), 0.0);
    EXPECT_FALSE(detector.isTumbling());
    static_assert(std::is_copy_constructible_v<TumbleDetector>);
}

/// One update() and what Java's detector made of it (ConditionsProbe.tumble()).
struct Step
{
    constexpr Step(double stepTime, bool cleared, double angle, double speed, double density,
                   double frequency, double filtered, bool isTumbling) noexcept
      : time(stepTime),
        guideCleared(cleared),
        aoa(angle),
        airSpeed(speed),
        airDensity(density),
        naturalFrequency(frequency),
        filteredAoa(filtered),
        tumbling(isTumbling)
    {
    }

    double time;
    bool   guideCleared;
    double aoa;
    double airSpeed;
    double airDensity;
    double naturalFrequency;
    double filteredAoa;
    bool   tumbling;
};

/// Drives @p detector through @p steps and expects Java's filter value and answer after each.
void expectSteps(TumbleDetector& detector, std::span<const Step> steps)
{
    for (const Step& step : steps)
    {
        SCOPED_TRACE(step.time);
        const bool tumbling = detector.update(step.time, step.guideCleared, step.aoa, step.airSpeed,
                                              step.airDensity, step.naturalFrequency);
        // The filter goes through exp(): Java's value within the libm tolerance, accumulated
        // over the steps.
        EXPECT_TRUE(isJavaValue(step.filteredAoa, detector.getFilteredAOA()));
        EXPECT_EQ(tumbling, step.tumbling);
        EXPECT_EQ(detector.isTumbling(), step.tumbling);
    }
}

TEST(TumbleDetector, TheFilterFollowsJavasStepForStep)
{
    // One detector driven through every branch of update(): the first step (no time has
    // passed), on the launch guide, a NaN angle, a dynamic pressure below the floor and a NaN
    // one, a step that takes no time and one that goes back in time (each holds the filter),
    // a rocket without a natural frequency (NaN, zero, negative: the shortest time constant),
    // a stiff and a slow one (the limits of the time constant), a dynamic pressure just at the
    // floor, and long steps.
    const std::array<Step, 22> steps{{
        {0.0, true, 2.0, 50.0, 1.225, 10.0, 0.0, false},
        {0.05, true, 2.0, 50.0, 1.225, 10.0, 0.07801511793343341, false},
        {0.1, true, 2.0, 50.0, 1.225, 10.0, 0.15298705655378306, false},
        {0.15, false, 3.0, 50.0, 1.225, 10.0, 0.15298705655378306, false},
        {0.2, true, kNaN, 50.0, 1.225, 10.0, 0.15298705655378306, false},
        {0.25, true, 3.0, 1.0, 1.225, 10.0, 0.15298705655378306, false},
        {0.3, true, 3.0, 50.0, kNaN, 10.0, 0.15298705655378306, false},
        {0.3, true, 3.0, 50.0, 1.225, 10.0, 0.15298705655378306, false},
        {0.25, true, 3.0, 50.0, 1.225, 10.0, 0.15298705655378306, false},
        {0.35, true, 3.0, 50.0, 1.225, kNaN, 2.6146986969204034, true},
        {0.4, true, 3.0, 50.0, 1.225, 0.0, 2.8582555719404494, true},
        {0.45, true, 3.0, 50.0, 1.225, -5.0, 2.947855139016287, true},
        {0.5, true, 0.1, 50.0, 1.225, 1000.0, 1.1476673570785318, true},
        {0.55, true, 0.1, 50.0, 1.225, 0.5, 1.1218003578672187, true},
        {0.6, true, 0.1, 50.0, 1.225, 6.0, 1.096572017141996, true},
        {1.6, true, 1.5, 1.2777531299998799, 1.225, 20.0, 1.4178580736968298, true},
        {1.7, true, 1.5, 1.27, 1.225, 20.0, 1.4178580736968298, true},
        {3.0, true, 2.5, 30.0, 1.0, 4.0, 1.9350723775173995, true},
        {3.5, true, 2.5, 30.0, 1.0, 4.0, 2.0600339252318838, true},
        {4.0, true, 2.5, 30.0, 1.0, 4.0, 2.157354076445739, true},
        {6.0, true, 0.05, 30.0, 1.0, 4.0, 0.8252522399932192, false},
        {8.0, true, 0.05, 30.0, 1.0, 4.0, 0.3351993608156144, false},
    }};

    TumbleDetector detector;
    expectSteps(detector, steps);

    // A copy goes on from where the original is, on its own.
    TumbleDetector copy(detector);
    EXPECT_EQ(copy.getFilteredAOA(), detector.getFilteredAOA());
    copy.update(8.5, true, 3.0, 30, 1.0, 4.0);
    EXPECT_TRUE(isJavaValue(0.9246511754740202, copy.getFilteredAOA()));
    EXPECT_TRUE(isJavaValue(0.3351993608156144, detector.getFilteredAOA()));
    detector.update(8.5, true, 3.0, 30, 1.0, 4.0);
    EXPECT_EQ(copy.getFilteredAOA(), detector.getFilteredAOA());
}

TEST(TumbleDetector, AHeldStepIsExactlyHeld)
{
    // The steps that carry no information leave the filter bit for bit as it was, and still
    // count as the last step: the time that passed while the filter was held is not made up
    // for afterwards.
    TumbleDetector detector;
    detector.update(0.0, true, 2.0, 50, 1.225, 10);
    detector.update(0.1, true, 2.0, 50, 1.225, 10);
    const double filtered = detector.getFilteredAOA();
    EXPECT_GT(filtered, 0.0);

    EXPECT_FALSE(detector.update(5.0, false, 3.0, 50, 1.225, 10));
    EXPECT_EQ(detector.getFilteredAOA(), filtered);

    // The next step that counts covers 0.1 s, not 5 s: the same change as from a twin that
    // was never held.
    TumbleDetector twin;
    twin.update(0.0, true, 2.0, 50, 1.225, 10);
    twin.update(0.1, true, 2.0, 50, 1.225, 10);
    twin.update(0.2, true, 3.0, 50, 1.225, 10);
    detector.update(5.1, true, 3.0, 50, 1.225, 10);
    EXPECT_NEAR(detector.getFilteredAOA(), twin.getFilteredAOA(), 1e-12);
}

TEST(TumbleDetector, TheThresholdItselfIsNotTumbling)
{
    // isTumbling() is "greater than": a filter that settles on the threshold stays below it.
    TumbleDetector detector;
    detector.update(0.0, true, TumbleDetector::kTumbleThreshold, 50, 1.225, 10);
    for (int i = 1; i <= 2000; i++)
    {
        detector.update(0.05 * i, true, TumbleDetector::kTumbleThreshold, 50, 1.225, 10);
    }
    EXPECT_LE(detector.getFilteredAOA(), TumbleDetector::kTumbleThreshold);
    EXPECT_NEAR(detector.getFilteredAOA(), TumbleDetector::kTumbleThreshold, 1e-9);
    EXPECT_FALSE(detector.isTumbling());
}

TEST(TumbleDetector, ANaNTimePoisonsTheFilterAsInJava)
{
    // Java: dt is NaN, which is not "<= 0", so the step counts and the filter becomes NaN. The
    // next step is held (the last time is NaN, so dt is 0), and every later step keeps the NaN,
    // since NaN + alpha * (aoa - NaN) is NaN (ConditionsProbe: "NaN time NaN false").
    TumbleDetector detector;
    detector.update(0, true, 1, 50, 1.2, 10);
    EXPECT_FALSE(detector.update(kNaN, true, 1, 50, 1.2, 10));
    EXPECT_TRUE(std::isnan(detector.getFilteredAOA()));
    EXPECT_FALSE(detector.isTumbling());
    EXPECT_FALSE(detector.update(1, true, 1, 50, 1.2, 10));
    EXPECT_TRUE(std::isnan(detector.getFilteredAOA()));
}

}  // namespace
