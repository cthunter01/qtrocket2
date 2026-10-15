#include "QtRocket/util/PinkNoise.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/GaussianRandom.h"
#include "QtRocket/util/Sha256.h"

namespace
{

using QtRocket::BugError;
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

/// The sample mean and standard deviation of @p count values, averaged over kSeedCount seeds,
/// which keeps the bounds well clear of the sampling noise of one seed. The values are the same
/// on every platform (GaussianRandom); with the generator of decision D12 of 2026-10-14 the
/// averages are a mean of -0.0175 and a deviation of 2.2525 for the wind's filter, and -0.0017
/// and 1.0009 for white noise (PinkStats.java of the probes of tier 10a, part
/// "goldens-and-turbulence", which also gives each seed's: means from -0.042 to 0.026 for the
/// wind's filter).
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

/// The bits of the first @p count values of @p noise.
std::vector<std::uint64_t> firstBits(PinkNoise& noise, std::size_t count)
{
    std::vector<std::uint64_t> bits;
    for (const double value : take(noise, count))
    {
        bits.push_back(std::bit_cast<std::uint64_t>(value));
    }
    return bits;
}

/// The SHA-256 digest of @p count values of @p noise, each as the eight bytes of its bits in
/// little-endian order.
std::string digestOf(PinkNoise& noise, std::size_t count)
{
    std::vector<std::byte> bytes;
    bytes.reserve(count * 8);
    for (const double value : take(noise, count))
    {
        const auto bits = std::bit_cast<std::uint64_t>(value);
        for (unsigned shift = 0; shift < 64; shift += 8)
        {
            bytes.push_back(static_cast<std::byte>((bits >> shift) & 0xffU));
        }
    }
    return QtRocket::sha256Hex(bytes);
}

// A seeded source gives the same values, bit for bit, on every platform (decision D12 of
// 2026-10-14): the first values after the filter's history and the digest of 100,000 of them, as
// GaussianProbe.java of the probes of tier 10a, part "goldens-and-turbulence", computes them with
// the same generator and OpenRocket's filter (and the tier 10 scouts' C++ reference did: first
// value -0x1.a04f436cb172ap-2).
TEST(PinkNoise, ASeededSourceIsTheSameOnEveryPlatform)
{
    PinkNoise wind(kWindAlpha, kWindPoles, 42);
    EXPECT_EQ(firstBits(wind, 4),
              (std::vector<std::uint64_t>{0xbfda04f436cb172aULL, 0x400304bb5ea8a44fULL,
                                          0x400477a802ad6a14ULL, 0x3ff35b186d80e7c2ULL}));
    PinkNoise again(kWindAlpha, kWindPoles, 42);
    EXPECT_EQ(digestOf(again, 100'000),
              "bd680767445e4da3b4acab61591356b0fc0ad71f348df69a4dda921e1f6d66f5");

    PinkNoise fivePoles(1.0, 5, 42);
    EXPECT_EQ(firstBits(fivePoles, 4),
              (std::vector<std::uint64_t>{0x3ff49fd985fabc76ULL, 0x3ff7485ba0db462bULL,
                                          0x3ff99f64c5aca90eULL, 0x3ffcb4853f9356b4ULL}));
}

// The input of the filter is GaussianRandom of the seed: without poles nothing is filtered (and
// no value is drawn for the history), so the source gives that generator's values.
TEST(PinkNoise, WithoutPolesItGivesTheGaussiansOfItsSeed)
{
    PinkNoise                  noise(kWindAlpha, 0, 42);
    QtRocket::GaussianRandom   gaussians(42);
    std::vector<std::uint64_t> expected;
    expected.reserve(1000);
    for (int i = 0; i < 1000; ++i)
    {
        expected.push_back(std::bit_cast<std::uint64_t>(gaussians.nextGaussian()));
    }
    EXPECT_EQ(firstBits(noise, 1000), expected);
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
    EXPECT_THROW(PinkNoise(1.0, -1, kSeed), BugError);
}

}  // namespace
