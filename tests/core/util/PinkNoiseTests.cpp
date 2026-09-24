#include "QtRocket/util/PinkNoise.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using QtRocket::PinkNoise;

// PinkNoiseWindModel's filter: alpha = 5/3 with two poles, whose standard deviation OpenRocket
// quotes as 2.252.
constexpr double        kWindAlpha  = 5.0 / 3.0;
constexpr int           kWindPoles  = 2;
constexpr double        kWindStddev = 2.252;
constexpr std::uint32_t kSeed       = 12345;
constexpr std::uint32_t kSeedCount  = 5;  ///< statistics are averaged over kSeed .. kSeed + 4

struct Statistics
{
    double mean{0.0};
    double stddev{0.0};
};

Statistics statistics(PinkNoise& noise, std::size_t count)
{
    std::vector<double> samples(count);
    double              sum = 0;
    for (double& sample : samples)
    {
        sample = noise.nextValue();
        sum += sample;
    }
    const double mean         = sum / static_cast<double>(count);
    double       sumOfSquares = 0;
    for (const double sample : samples)
    {
        sumOfSquares += (sample - mean) * (sample - mean);
    }
    return {.mean = mean, .stddev = std::sqrt(sumOfSquares / static_cast<double>(count))};
}

/// The sample mean and standard deviation of @p count values, averaged over kSeedCount seeds.
/// std::normal_distribution differs between standard libraries, so one seed's sample is not the
/// same on every platform; the average keeps the bounds well clear of the sampling noise on all.
Statistics averagedStatistics(double alpha, int poles, std::size_t count)
{
    Statistics total;
    for (std::uint32_t seed = kSeed; seed < kSeed + kSeedCount; ++seed)
    {
        PinkNoise        noise(alpha, poles, seed);
        const Statistics stats = statistics(noise, count);
        total.mean += stats.mean / static_cast<double>(kSeedCount);
        total.stddev += stats.stddev / static_cast<double>(kSeedCount);
    }
    return total;
}

std::vector<double> take(PinkNoise& noise, std::size_t count)
{
    std::vector<double> samples(count);
    for (double& sample : samples)
    {
        sample = noise.nextValue();
    }
    return samples;
}

TEST(PinkNoise, SameSeedGivesTheSameSequence)
{
    PinkNoise first(kWindAlpha, kWindPoles, kSeed);
    PinkNoise second(kWindAlpha, kWindPoles, kSeed);
    EXPECT_EQ(take(first, 1000), take(second, 1000));
}

TEST(PinkNoise, DifferentSeedsGiveDifferentSequences)
{
    PinkNoise first(kWindAlpha, kWindPoles, kSeed);
    PinkNoise second(kWindAlpha, kWindPoles, kSeed + 1);
    EXPECT_NE(take(first, 100), take(second, 100));
}

TEST(PinkNoise, WindModelNoiseHasZeroMeanAndTheKnownStandardDeviation)
{
    const Statistics stats = averagedStatistics(kWindAlpha, kWindPoles, 100'000);
    EXPECT_NEAR(stats.mean, 0.0, 0.15);
    EXPECT_NEAR(stats.stddev, kWindStddev, 0.1);
}

TEST(PinkNoise, AlphaZeroIsWhiteNoise)
{
    // With alpha = 0 every filter multiplier is 0, so the output is the unit Gaussian input.
    const Statistics stats = averagedStatistics(0.0, 5, 100'000);
    EXPECT_NEAR(stats.mean, 0.0, 0.02);
    EXPECT_NEAR(stats.stddev, 1.0, 0.02);
}

TEST(PinkNoise, ZeroPolesIsWhiteNoise)
{
    EXPECT_EQ(PinkNoise(kWindAlpha, 0, kSeed).poles(), 0);
    const Statistics stats = averagedStatistics(kWindAlpha, 0, 100'000);
    EXPECT_NEAR(stats.mean, 0.0, 0.02);
    EXPECT_NEAR(stats.stddev, 1.0, 0.02);
}

TEST(PinkNoise, MorePolesAmplifyLowerFrequencies)
{
    // The same alpha with more poles lets more low-frequency power through, so the spread grows.
    const Statistics two  = averagedStatistics(kWindAlpha, 2, 100'000);
    const Statistics five = averagedStatistics(kWindAlpha, 5, 100'000);
    EXPECT_GT(five.stddev, two.stddev);
}

TEST(PinkNoise, AlphaTwoWithOnePoleIsARandomWalk)
{
    // alpha = 2, one pole: multiplier -1, so x[n] = g[n] + x[n-1] and the spread keeps growing.
    const Statistics stats = averagedStatistics(2.0, 1, 10'000);
    EXPECT_GT(stats.stddev, 5.0);
}

TEST(PinkNoise, ReportsItsPoles)
{
    EXPECT_EQ(PinkNoise(1.0, 3, kSeed).poles(), 3);
    EXPECT_EQ(PinkNoise(1.0).poles(), 5);
    EXPECT_EQ(PinkNoise().poles(), 5);
}

TEST(PinkNoise, UnseededSourcesProduceFiniteValues)
{
    PinkNoise noise;
    for (int i = 0; i < 100; ++i)
    {
        EXPECT_TRUE(std::isfinite(noise.nextValue()));
    }
}

TEST(PinkNoise, RejectsNegativePoles)
{
    EXPECT_THROW(PinkNoise(1.0, -1, kSeed), std::invalid_argument);
}

}  // namespace
