#include "QtRocket/models/PinkNoiseWindModel.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/models/WindModel.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Monitorable.h"
#include "QtRocket/util/Signal.h"

namespace
{

namespace MathUtil = QtRocket::MathUtil;
using QtRocket::BugError;
using QtRocket::Coordinate;
using QtRocket::InMemoryPreferences;
using QtRocket::ModId;
using QtRocket::PinkNoiseWindModel;
using QtRocket::WindModel;

constexpr double      kEpsilon    = MathUtil::kEpsilon;
constexpr double      kDeltaT     = PinkNoiseWindModel::kDeltaT;
constexpr std::size_t kSampleSize = 1000;
constexpr double      kPi         = std::numbers::pi;
constexpr double      kNaN        = std::numeric_limits<double>::quiet_NaN();

static_assert(QtRocket::Monitorable<PinkNoiseWindModel>);

/// Counts the emissions of a model's changed() while it lives.
class ChangeCounter
{
public:
    explicit ChangeCounter(WindModel& windModel)
      : m_connection(windModel.changed().connect([this] { ++m_count; }))
    {
    }

    [[nodiscard]] int count() const noexcept { return m_count; }

private:
    int                                  m_count{0};
    QtRocket::Signal<>::ScopedConnection m_connection;
};

/// The velocities at 0, dt, 2 dt, ... for @p count samples.
std::vector<Coordinate> sampleVelocities(PinkNoiseWindModel& windModel, std::size_t count)
{
    std::vector<Coordinate> velocities;
    velocities.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        velocities.push_back(windModel.getWindVelocity(static_cast<double>(i) * kDeltaT, 0));
    }
    return velocities;
}

/// Bitwise equality of two velocity sequences (Coordinate's == is tolerant).
bool sameSequence(const std::vector<Coordinate>& a, const std::vector<Coordinate>& b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (!a[i].exactlyEquals(b[i]))
        {
            return false;
        }
    }
    return true;
}

/// PinkNoiseWindModelTest.setUp(): a model with a fixed seed.
class PinkNoiseWindModelTest : public ::testing::Test
{
protected:
    PinkNoiseWindModel m_model{42};
};

// ---- Ported from PinkWindModelTest.java (PinkNoiseWindModelTest) ----

TEST_F(PinkNoiseWindModelTest, InitialState)
{
    EXPECT_NEAR(m_model.getAverage(), 0, kEpsilon);
    EXPECT_NEAR(m_model.getStandardDeviation(), 0, kEpsilon);
    EXPECT_NEAR(m_model.getDirection(), kPi / 2, kEpsilon);
    EXPECT_NEAR(m_model.getTurbulenceIntensity(), 0, kEpsilon);
}

TEST_F(PinkNoiseWindModelTest, SetAverageIncludingNegativeValues)
{
    // Positive speed
    m_model.setAverage(10.0);
    EXPECT_NEAR(m_model.getAverage(), 10.0, kEpsilon);
    const double originalDirection = m_model.getDirection();

    // Negative speed: the direction flips and the speed is positive
    m_model.setAverage(-10.0);
    EXPECT_NEAR(m_model.getAverage(), 10.0, kEpsilon);
    EXPECT_NEAR(m_model.getDirection(), MathUtil::reduce2Pi(kPi + originalDirection), kEpsilon);

    // Another negative speed flips it back
    m_model.setAverage(-15.0);
    EXPECT_NEAR(m_model.getAverage(), 15.0, kEpsilon);
    EXPECT_NEAR(m_model.getDirection(), originalDirection, kEpsilon);

    // Positive again
    m_model.setAverage(20.0);
    EXPECT_NEAR(m_model.getAverage(), 20.0, kEpsilon);
    EXPECT_NEAR(m_model.getDirection(), originalDirection, kEpsilon);
}

TEST_F(PinkNoiseWindModelTest, SetDirectionNormalizes)
{
    m_model.setDirection(kPi);
    EXPECT_NEAR(m_model.getDirection(), kPi, kEpsilon);

    // Direction > 2*PI
    m_model.setDirection(3 * kPi);
    EXPECT_NEAR(m_model.getDirection(), kPi, kEpsilon);

    // Direction > 4*PI
    m_model.setDirection(9 * kPi);
    EXPECT_NEAR(m_model.getDirection(), kPi, kEpsilon);

    // Negative direction
    m_model.setDirection(-kPi / 2);
    EXPECT_NEAR(m_model.getDirection(), 3 * kPi / 2, kEpsilon);

    // Negative direction < -2*PI
    m_model.setDirection(-3 * kPi);
    EXPECT_NEAR(m_model.getDirection(), kPi, kEpsilon);

    // Negative direction < -4*PI
    m_model.setDirection(-5 * kPi);
    EXPECT_NEAR(m_model.getDirection(), kPi, kEpsilon);

    // Very large values
    m_model.setDirection(1000 * kPi);
    EXPECT_TRUE(m_model.getDirection() >= 0 && m_model.getDirection() <= 2 * kPi)
        << "Direction should be normalized to [0, 2*PI)";
}

TEST_F(PinkNoiseWindModelTest, StandardDeviationAndTurbulence)
{
    m_model.setAverage(10.0);
    m_model.setStandardDeviation(2.0);

    EXPECT_NEAR(m_model.getStandardDeviation(), 2.0, kEpsilon);
    EXPECT_NEAR(m_model.getTurbulenceIntensity(), 0.2, kEpsilon);

    // Setting the turbulence intensity
    m_model.setTurbulenceIntensity(0.3);
    EXPECT_NEAR(m_model.getStandardDeviation(), 3.0, kEpsilon);

    // Zero average
    m_model.setAverage(0.0);
    m_model.setStandardDeviation(0.0);
    EXPECT_NEAR(m_model.getTurbulenceIntensity(), 0.0, kEpsilon);

    m_model.setStandardDeviation(1.0);
    EXPECT_NEAR(m_model.getTurbulenceIntensity(), 1, kEpsilon);
}

TEST_F(PinkNoiseWindModelTest, WindVelocityGeneration)
{
    // Deviation: Java checks one seed's 1000 samples against +-0.5; the mean of 1000 samples of
    // this pink noise scatters by about 0.29 m/s here, and std::normal_distribution differs
    // between standard libraries, so the mean over five seeds is checked instead (seed 42 first,
    // as in Java), which keeps the bound clear of the scatter on every platform.
    double totalAverage = 0;
    for (int seed = 42; seed < 47; ++seed)
    {
        PinkNoiseWindModel seeded(seed);
        seeded.setAverage(10.0);
        seeded.setDirection(0.0);  // Wind blowing toward positive X
        seeded.setStandardDeviation(2.0);

        double sum = 0;
        for (const Coordinate& velocity : sampleVelocities(seeded, kSampleSize))
        {
            sum += velocity.length();
        }
        totalAverage += sum / static_cast<double>(kSampleSize) / 5;
    }
    EXPECT_NEAR(totalAverage, 10.0, 0.5);

    // Verify that reset works properly
    m_model.setAverage(10.0);
    m_model.setDirection(0.0);
    m_model.setStandardDeviation(2.0);
    const Coordinate v1 = m_model.getWindVelocity(1.0, 0);
    const Coordinate v2 = m_model.getWindVelocity(0.5, 0);
    EXPECT_NE(v1, v2);
}

TEST_F(PinkNoiseWindModelTest, Clone)
{
    m_model.setAverage(10.0);
    m_model.setDirection(kPi / 4);
    m_model.setStandardDeviation(2.0);

    const std::unique_ptr<WindModel> clone = m_model.clone();
    const auto*                      pink  = dynamic_cast<const PinkNoiseWindModel*>(clone.get());
    ASSERT_NE(pink, nullptr);

    EXPECT_NEAR(pink->getAverage(), m_model.getAverage(), kEpsilon);
    EXPECT_NEAR(pink->getDirection(), m_model.getDirection(), kEpsilon);
    EXPECT_NEAR(pink->getStandardDeviation(), m_model.getStandardDeviation(), kEpsilon);
    EXPECT_NE(pink, &m_model);
}

TEST_F(PinkNoiseWindModelTest, Equality)
{
    m_model.setAverage(10.0);
    m_model.setDirection(kPi / 4);
    m_model.setStandardDeviation(2.0);

    PinkNoiseWindModel other(42);
    other.setAverage(10.0);
    other.setDirection(kPi / 4);
    other.setStandardDeviation(2.0);

    EXPECT_TRUE(m_model == other);
    EXPECT_EQ(m_model.hashCode(), other.hashCode());

    other.setAverage(11.0);
    EXPECT_FALSE(m_model == other);
}

TEST_F(PinkNoiseWindModelTest, ChangeListeners)
{
    bool listenerCalled = false;
    auto connection     = m_model.changed().connect([&listenerCalled] { listenerCalled = true; });

    m_model.setAverage(10.0);
    EXPECT_TRUE(listenerCalled);

    listenerCalled = false;
    EXPECT_TRUE(m_model.changed().disconnect(connection));
    m_model.setAverage(15.0);
    EXPECT_FALSE(listenerCalled);
}

TEST_F(PinkNoiseWindModelTest, ModIdIsZero)
{
    EXPECT_EQ(m_model.modId(), ModId::zero());
}

TEST_F(PinkNoiseWindModelTest, NegativeTimeIsABug)
{
    EXPECT_THROW((void)m_model.getWindVelocity(-1.0, 0), BugError);
    try
    {
        (void)m_model.getWindVelocity(-0.5, 0);
        FAIL() << "no BugError";
    }
    catch (const BugError& e)
    {
        EXPECT_NE(std::string(e.what()).find("Requesting wind speed at t=-0.5"), std::string::npos);
    }
}

// ---- QtRocket additions: determinism and seeds ----

TEST_F(PinkNoiseWindModelTest, SameSeedGivesTheSameWind)
{
    m_model.setAverage(8.0);
    m_model.setStandardDeviation(2.0);
    PinkNoiseWindModel other(42);
    other.setAverage(8.0);
    other.setStandardDeviation(2.0);

    const std::vector<Coordinate> first = sampleVelocities(m_model, 500);
    EXPECT_TRUE(sameSequence(first, sampleVelocities(other, 500)));

    // The wind is a function of the time: going back restarts the source.
    PinkNoiseWindModel again(42);
    again.setAverage(8.0);
    again.setStandardDeviation(2.0);
    (void)again.getWindVelocity(20.0, 0);
    EXPECT_TRUE(sameSequence(first, sampleVelocities(again, 500)));

    // Skipping ahead gives the same values as stepping there.
    PinkNoiseWindModel skipping(42);
    skipping.setAverage(8.0);
    skipping.setStandardDeviation(2.0);
    EXPECT_TRUE(skipping.getWindVelocity(499 * kDeltaT, 0).exactlyEquals(first.back()));

    // Between samples the noise is interpolated linearly.
    const Coordinate between = m_model.getWindVelocity(10.025, 0);
    EXPECT_NEAR(between.x, (first[200].x + first[201].x) / 2, 1e-9);
}

TEST_F(PinkNoiseWindModelTest, DifferentSeedsGiveDifferentWind)
{
    m_model.setAverage(8.0);
    m_model.setStandardDeviation(2.0);
    PinkNoiseWindModel other(43);
    other.setAverage(8.0);
    other.setStandardDeviation(2.0);
    EXPECT_FALSE(sameSequence(sampleVelocities(m_model, 100), sampleVelocities(other, 100)));
}

TEST_F(PinkNoiseWindModelTest, SetSeedReseedsAndDiscardsTheRandomState)
{
    m_model.setAverage(8.0);
    m_model.setStandardDeviation(2.0);
    const std::vector<Coordinate> seed42 = sampleVelocities(m_model, 200);

    PinkNoiseWindModel other(7);
    other.setAverage(8.0);
    other.setStandardDeviation(2.0);
    (void)sampleVelocities(other, 300);  // advance its source
    other.setSeed(42);
    EXPECT_TRUE(other == m_model);  // the seed takes part in equality
    EXPECT_TRUE(sameSequence(seed42, sampleVelocities(other, 200)));
}

TEST_F(PinkNoiseWindModelTest, CopyReproducesTheWindFromTheStart)
{
    m_model.setAverage(8.0);
    m_model.setStandardDeviation(2.0);
    (void)sampleVelocities(m_model, 300);  // the random state is not copied

    PinkNoiseWindModel copy{m_model};
    EXPECT_TRUE(copy == m_model);
    PinkNoiseWindModel fresh(42);
    fresh.setAverage(8.0);
    fresh.setStandardDeviation(2.0);
    EXPECT_TRUE(sameSequence(sampleVelocities(copy, 200), sampleVelocities(fresh, 200)));
}

TEST_F(PinkNoiseWindModelTest, DefaultConstructedModelsDrawTheirOwnSeeds)
{
    // new Random().nextInt(): two models almost surely differ in their seed.
    const PinkNoiseWindModel a;
    const PinkNoiseWindModel b;
    const PinkNoiseWindModel c;
    EXPECT_FALSE(a == b && b == c);
    EXPECT_EQ(a.getAverage(), 0.0);
    EXPECT_EQ(a.getDirection(), kPi / 2);
}

// ---- QtRocket additions: values ----

TEST_F(PinkNoiseWindModelTest, ConstantsMatchOpenRocket)
{
    EXPECT_EQ(PinkNoiseWindModel::kDeltaT, 0.05);
    EXPECT_EQ(PinkNoiseWindModel::kSeedRandomization, 0x7343AA03);
    EXPECT_EQ(PinkNoiseWindModel::kAlpha, 5.0 / 3.0);
    EXPECT_EQ(PinkNoiseWindModel::kPoles, 2);
    EXPECT_EQ(PinkNoiseWindModel::kStdDev, 2.252);
}

TEST_F(PinkNoiseWindModelTest, SteadyWindHasNoNoise)
{
    // With no deviation the velocity is exactly speed * (sin(dir), cos(dir), 0).
    m_model.setAverage(5.0);
    m_model.setDirection(kPi / 3);
    for (const double time : {0.0, 0.01, 3.3, 100.0})
    {
        const Coordinate velocity = m_model.getWindVelocity(time, 1234.0, 12.0);
        EXPECT_EQ(velocity.x, 5.0 * std::sin(kPi / 3));
        EXPECT_EQ(velocity.y, 5.0 * std::cos(kPi / 3));
        EXPECT_EQ(velocity.z, 0.0);
        EXPECT_EQ(velocity.weight, 0.0);
    }
}

TEST_F(PinkNoiseWindModelTest, AltitudesDoNotMatter)
{
    m_model.setAverage(8.0);
    m_model.setStandardDeviation(2.0);
    PinkNoiseWindModel other(42);
    other.setAverage(8.0);
    other.setStandardDeviation(2.0);
    for (int i = 0; i < 50; ++i)
    {
        const double time = i * 0.13;
        EXPECT_TRUE(m_model.getWindVelocity(time, 0).exactlyEquals(
            other.getWindVelocity(time, 9000.0, 3000.0)));
    }
}

TEST_F(PinkNoiseWindModelTest, HashCodeMatchesOpenRocket)
{
    // PinkNoiseWindModel.hashCode() printed by OpenRocket on JDK 17.
    EXPECT_EQ(m_model.hashCode(), -2145924809);
    m_model.setAverage(10.0);
    m_model.setDirection(kPi / 4);
    m_model.setStandardDeviation(2.0);
    EXPECT_EQ(m_model.hashCode(), -612120265);

    PinkNoiseWindModel negative(-7);
    negative.setAverage(-3.5);  // turns the wind around
    negative.setStandardDeviation(-1.0);
    EXPECT_EQ(negative.getAverage(), 3.5);
    EXPECT_EQ(negative.getDirection(), 4.71238898038469);
    EXPECT_EQ(negative.getStandardDeviation(), 0.0);
    EXPECT_EQ(negative.hashCode(), 1732051613);
}

TEST_F(PinkNoiseWindModelTest, StandardDeviationFollowsJavasMathMax)
{
    PinkNoiseWindModel zero(0);
    zero.setStandardDeviation(kNaN);
    EXPECT_TRUE(std::isnan(zero.getStandardDeviation()));
    EXPECT_EQ(zero.getTurbulenceIntensity(), 1.0);  // average 0, deviation not 0
    EXPECT_EQ(zero.hashCode(), -502281967);

    zero.setStandardDeviation(-0.0);  // Math.max(-0.0, 0) is 0.0
    EXPECT_FALSE(std::signbit(zero.getStandardDeviation()));
    EXPECT_EQ(zero.hashCode(), -2145924847);
}

TEST_F(PinkNoiseWindModelTest, AverageKeepsTheTurbulenceIntensity)
{
    // Values printed by OpenRocket.
    PinkNoiseWindModel q(1);
    q.setAverage(4.0);
    q.setStandardDeviation(1.0);
    q.setAverage(8.0);
    EXPECT_EQ(q.getStandardDeviation(), 2.0);
    EXPECT_EQ(q.getTurbulenceIntensity(), 0.25);

    // Through a zero average the intensity is lost, as in Java.
    q.setAverage(0.0);
    EXPECT_EQ(q.getStandardDeviation(), 0.0);
    EXPECT_EQ(q.getTurbulenceIntensity(), 0.0);
    q.setAverage(5.0);
    EXPECT_EQ(q.getStandardDeviation(), 0.0);

    // setAveragePreservingStandardDeviation keeps the deviation instead.
    q.setStandardDeviation(1.0);
    q.setAveragePreservingStandardDeviation(10.0);
    EXPECT_EQ(q.getStandardDeviation(), 1.0);
    EXPECT_EQ(q.getAverage(), 10.0);
    const double direction = q.getDirection();
    q.setAveragePreservingStandardDeviation(-4.0);
    EXPECT_EQ(q.getAverage(), 4.0);
    EXPECT_EQ(q.getStandardDeviation(), 1.0);
    EXPECT_NEAR(q.getDirection(), MathUtil::reduce2Pi(direction + kPi), 1e-15);
}

TEST_F(PinkNoiseWindModelTest, DirectionIsComparedBeforeReduction)
{
    // Printed by OpenRocket: pi/2 + 2 pi reduces to pi/2, and 1000 pi to just below 2 pi.
    PinkNoiseWindModel  d(1);
    const ChangeCounter counter(d);
    d.setDirection((kPi / 2) + (2 * kPi));
    EXPECT_EQ(d.getDirection(), 1.5707963267948966);
    EXPECT_EQ(counter.count(), 1);  // unequal before reduction, so stored and emitted
    d.setDirection(1000 * kPi);
    EXPECT_EQ(d.getDirection(), 6.2831853071793375);
    d.setDirection(d.getDirection());
    EXPECT_EQ(counter.count(), 2);
}

TEST_F(PinkNoiseWindModelTest, ChangesEmitOnlyWhenSomethingChanges)
{
    const ChangeCounter counter(m_model);
    m_model.setAverage(0.0);
    m_model.setStandardDeviation(0.0);
    m_model.setDirection(kPi / 2);
    EXPECT_EQ(counter.count(), 0);

    m_model.setAverage(10.0);  // the deviation stays 0
    EXPECT_EQ(counter.count(), 1);
    m_model.setStandardDeviation(1.0);
    EXPECT_EQ(counter.count(), 2);
    m_model.setAverage(20.0);  // average and deviation (intensity kept)
    EXPECT_EQ(counter.count(), 4);
    m_model.setAverage(-20.0);  // only the direction
    EXPECT_EQ(counter.count(), 5);
    m_model.setTurbulenceIntensity(0.1);  // 20 * 0.1 = 2.0, the deviation already set
    EXPECT_EQ(counter.count(), 5);
    m_model.fireChangeEvent();
    EXPECT_EQ(counter.count(), 6);
    m_model.setSeed(3);  // not a change of the configuration
    EXPECT_EQ(counter.count(), 6);
}

TEST_F(PinkNoiseWindModelTest, IntensityDescriptionKeys)
{
    struct Case
    {
        double           intensity;
        std::string_view key;
    };
    constexpr std::array<Case, 9> kCases{{
        {.intensity = 0.0, .key = "simedtdlg.IntensityDesc.None"},
        {.intensity = 0.0009, .key = "simedtdlg.IntensityDesc.None"},
        {.intensity = 0.001, .key = "simedtdlg.IntensityDesc.Verylow"},
        {.intensity = 0.07, .key = "simedtdlg.IntensityDesc.Low"},
        {.intensity = 0.12, .key = "simedtdlg.IntensityDesc.Medium"},
        {.intensity = 0.17, .key = "simedtdlg.IntensityDesc.High"},
        {.intensity = 0.22, .key = "simedtdlg.IntensityDesc.Veryhigh"},
        {.intensity = 0.25, .key = "simedtdlg.IntensityDesc.Extreme"},
        {.intensity = 3.0, .key = "simedtdlg.IntensityDesc.Extreme"},
    }};
    m_model.setAverage(10.0);
    for (const Case& c : kCases)
    {
        m_model.setTurbulenceIntensity(c.intensity);
        EXPECT_EQ(m_model.getIntensityDescriptionKey(), c.key) << c.intensity;
    }
}

TEST_F(PinkNoiseWindModelTest, CopyAndMoveSemantics)
{
    m_model.setAverage(3.0);
    const ChangeCounter counter(m_model);

    // A copy has no connections.
    PinkNoiseWindModel copy{m_model};
    copy.setAverage(4.0);
    EXPECT_EQ(counter.count(), 0);

    // A move takes them along.
    PinkNoiseWindModel moved{std::move(m_model)};
    moved.setAverage(5.0);
    EXPECT_EQ(counter.count(), 1);
}

TEST_F(PinkNoiseWindModelTest, LoadFromCopiesTheConfigurationButNotTheSeed)
{
    PinkNoiseWindModel source(99);
    source.setAverage(6.0);
    source.setDirection(1.0);
    source.setStandardDeviation(0.5);

    const ChangeCounter counter(m_model);
    m_model.loadFrom(source);
    EXPECT_EQ(counter.count(), 0);
    EXPECT_EQ(m_model.getAverage(), 6.0);
    EXPECT_EQ(m_model.getDirection(), 1.0);
    EXPECT_EQ(m_model.getStandardDeviation(), 0.5);
    EXPECT_FALSE(m_model == source);  // the seeds differ
    m_model.setSeed(99);
    EXPECT_TRUE(m_model == source);
}

TEST_F(PinkNoiseWindModelTest, PreferencesRoundTrip)
{
    // ApplicationPreferences.loadWindModelState() from OpenRocket's defaults: 2 m/s, intensity
    // 0.1 (so a deviation of 0.2 m/s), from the east.
    const InMemoryPreferences defaults;
    PinkNoiseWindModel        loaded(1);
    loaded.loadFrom(defaults);
    EXPECT_EQ(loaded.getAverage(), 2.0);
    EXPECT_NEAR(loaded.getStandardDeviation(), 0.2, 1e-15);
    EXPECT_EQ(loaded.getDirection(), kPi / 2);

    loaded.setAverage(7.0);
    loaded.setStandardDeviation(1.4);
    loaded.setDirection(2.5);
    InMemoryPreferences prefs;
    loaded.storeTo(prefs);
    EXPECT_EQ(prefs.getWindAverage(), 7.0);
    EXPECT_NEAR(prefs.getWindTurbulenceIntensity(), 0.2, 1e-15);
    EXPECT_EQ(prefs.getWindDirection(), 2.5);

    PinkNoiseWindModel reloaded(1);
    reloaded.loadFrom(prefs);
    EXPECT_EQ(reloaded.getAverage(), 7.0);
    EXPECT_NEAR(reloaded.getStandardDeviation(), 1.4, 1e-14);
    EXPECT_EQ(reloaded.getDirection(), 2.5);
}

}  // namespace
