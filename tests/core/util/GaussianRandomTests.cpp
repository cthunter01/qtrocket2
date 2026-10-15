// GaussianRandom, the generator of QtRocket's own the wind turbulence draws from (the user's
// decision D12 of 2026-10-14). It has no OpenRocket counterpart and no JUnit test. Its values are
// to be the same, bit for bit, on every platform, so they are pinned as bit patterns (never as
// text) and, for 100,000 of them, by a SHA-256 digest: the test CI checks on Linux, macOS and
// Windows. Every pinned value comes from GaussianProbe.java of the probes of tier 10a, part
// "goldens-and-turbulence", which computes the same generator in Java, whose arithmetic is
// strict, with StrictMath.log and StrictMath.sqrt; the tier 10 scouts' C++ reference
// (gaussian.cpp, built by clang and GCC at -O0 to -O3) and their Python reference gave the same
// digest for seed 42.

#include "QtRocket/util/GaussianRandom.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Sha256.h"

namespace
{

using QtRocket::GaussianRandom;

/// The bits of @p value.
[[nodiscard]] std::uint64_t bitsOf(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

/// Appends the eight bytes of @p value, the least significant first, as Java's ByteBuffer in
/// little-endian order writes them (whatever the order of the platform).
void appendLittleEndian(std::vector<std::byte>& bytes, double value)
{
    const std::uint64_t bits = bitsOf(value);
    for (unsigned shift = 0; shift < 64; shift += 8)
    {
        bytes.push_back(static_cast<std::byte>((bits >> shift) & 0xffU));
    }
}

/// The SHA-256 digest of the first @p count Gaussians of the seed @p seed, each as the eight
/// bytes of its bits in little-endian order.
[[nodiscard]] std::string digestOfGaussians(std::uint64_t seed, std::size_t count)
{
    GaussianRandom         random(seed);
    std::vector<std::byte> bytes;
    bytes.reserve(count * 8);
    for (std::size_t i = 0; i < count; ++i)
    {
        appendLittleEndian(bytes, random.nextGaussian());
    }
    return QtRocket::sha256Hex(bytes);
}

/// The bits of the first @p count Gaussians of the seed @p seed.
[[nodiscard]] std::vector<std::uint64_t> firstGaussians(std::uint64_t seed, std::size_t count)
{
    GaussianRandom             random(seed);
    std::vector<std::uint64_t> bits;
    bits.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        bits.push_back(bitsOf(random.nextGaussian()));
    }
    return bits;
}

TEST(GaussianRandom, TheBitsAreSplitMix64s)
{
    GaussianRandom random(42);
    EXPECT_EQ(random.nextBits(), 0xbdd732262feb6e95ULL);
    EXPECT_EQ(random.nextBits(), 0x28efe333b266f103ULL);
    EXPECT_EQ(random.nextBits(), 0x47526757130f9f52ULL);
    EXPECT_EQ(random.nextBits(), 0x581ce1ff0e4ae394ULL);
}

TEST(GaussianRandom, ADoubleIsTheUpper53BitsTimesTwoToTheMinus53)
{
    GaussianRandom random(42);
    EXPECT_EQ(bitsOf(random.nextDouble()), 0x3fe7bae644c5fd6dULL);
    EXPECT_EQ(bitsOf(random.nextDouble()), 0x3fc477f199d93378ULL);
    EXPECT_EQ(bitsOf(random.nextDouble()), 0x3fd1d499d5c4c3e6ULL);
    EXPECT_EQ(bitsOf(random.nextDouble()), 0x3fd607387fc392b8ULL);
    // The upper 53 bits of the first draw, 0xbdd732262feb6e95 >> 11, times 2^-53.
    GaussianRandom again(42);
    EXPECT_EQ(again.nextDouble(), static_cast<double>(0xbdd732262feb6e95ULL >> 11U) * 0x1.0p-53);
}

TEST(GaussianRandom, TheFirstGaussiansOfSeveralSeeds)
{
    // Seed 42: the scouts' 0x1.f8c80e851c08cp-2, -0x1.6354b5f7dce79p-1, -0x1.47f4af60c7f94p+0
    // first. Each pair shares the multiplier of one accepted point of the polar method.
    EXPECT_EQ(firstGaussians(42, 6),
              (std::vector<std::uint64_t>{0x3fdf8c80e851c08cULL, 0xbfe6354b5f7dce79ULL,
                                          0xbff47f4af60c7f94ULL, 0xbfecd98e78b167aeULL,
                                          0xbfe342959c9fa344ULL, 0x3fe48fe9fcfdf274ULL}));
    EXPECT_EQ(firstGaussians(0, 3),
              (std::vector<std::uint64_t>{0x3fef8140ae1026c7ULL, 0xbfc682e27f92f3d9ULL,
                                          0xbfe6c93ef6b47edaULL}));
    EXPECT_EQ(firstGaussians(1, 3),
              (std::vector<std::uint64_t>{0x3fdb7c251a5470ccULL, 0x3ff95f5305298699ULL,
                                          0x3fdd368fe72bb620ULL}));
    EXPECT_EQ(firstGaussians(0xffffffffULL, 3),
              (std::vector<std::uint64_t>{0xbfeb8507f4601311ULL, 0xc00142230ef06096ULL,
                                          0xbfd4995f67f135feULL}));
    EXPECT_EQ(firstGaussians(std::numeric_limits<std::uint64_t>::max(), 3),
              (std::vector<std::uint64_t>{0xbff6d65ad500de8dULL, 0xbfd805794c7286c9ULL,
                                          0x3fe190d6568b4982ULL}));
    // As doubles, as Java prints them.
    GaussianRandom random(42);
    EXPECT_EQ(random.nextGaussian(), 0.49295065581737485);
    EXPECT_EQ(random.nextGaussian(), -0.6940056672160174);
    EXPECT_EQ(random.nextGaussian(), -1.2810773478777024);
}

// The test CI runs on Linux, macOS and Windows: 100,000 values of seed 42, bit for bit. Java's
// digest of the same values is this one, and so were the scouts' (C++ under clang and GCC with
// and without optimisation, and Python). A compiler that fused a multiply-add of the generator,
// or a platform whose log or sqrt the generator relied on, would change it.
TEST(GaussianRandom, TheDigestOfAHundredThousandValuesIsTheSameEverywhere)
{
    EXPECT_EQ(digestOfGaussians(42, 100'000),
              "5e691cf87a64cfc17e6cc77fdffd73490e7f0c522157cdca693270b817a81dbc");
    EXPECT_EQ(digestOfGaussians(12345, 100'000),
              "d0cddc5f9a94a054d3ebb7b743da5dca8c943c204fded077458a25bb4f7d715a");
}

TEST(GaussianRandom, TheValuesAreStandardNormal)
{
    // Java: mean 0.0030118990607714, mean square 1.000128518978297 over these values (sums in
    // the same order, so the same doubles). The bounds are a sanity check: three standard
    // errors of the mean of 100,000 values, and of the mean square.
    GaussianRandom random(42);
    double         sum          = 0;
    double         sumOfSquares = 0;
    constexpr int  kCount       = 100'000;
    for (int i = 0; i < kCount; ++i)
    {
        const double value  = random.nextGaussian();
        const double square = value * value;
        sum += value;
        sumOfSquares += square;
    }
    EXPECT_EQ(sum / kCount, 0.0030118990607714);
    EXPECT_EQ(sumOfSquares / kCount, 1.000128518978297);
    EXPECT_NEAR(sum / kCount, 0.0, 0.01);
    EXPECT_NEAR(sumOfSquares / kCount, 1.0, 0.015);
}

TEST(GaussianRandom, AGeneratorIsAFunctionOfItsSeed)
{
    GaussianRandom first(7);
    GaussianRandom second(7);
    GaussianRandom other(8);
    bool           differs = false;
    for (int i = 0; i < 1000; ++i)
    {
        const double value = first.nextGaussian();
        EXPECT_EQ(bitsOf(value), bitsOf(second.nextGaussian()));
        differs = differs || bitsOf(value) != bitsOf(other.nextGaussian());
    }
    EXPECT_TRUE(differs);
}

/// A logarithm pinned against Java's StrictMath.log: the argument and the bits of the result.
struct PinnedLog
{
    double        x;
    std::uint64_t bits;
};

// fdlibm's log, as Java's StrictMath.log computes it (GaussianProbe.java prints both; Math.log
// gives the same for these): exact powers of two, both branches of an argument near a power of
// two (|f| < 2^-20), the reduced argument of either sign of i, subnormal and extreme arguments.
TEST(GaussianRandom, StrictLogIsJavasStrictMathLog)
{
    constexpr std::array<PinnedLog, 17> kLogs{{
        {.x = 1.0, .bits = 0x0000000000000000ULL},
        {.x = 2.0, .bits = 0x3fe62e42fefa39efULL},
        {.x = 0.5, .bits = 0xbfe62e42fefa39efULL},
        {.x = std::numbers::e, .bits = 0x3ff0000000000000ULL},
        {.x = 10.0, .bits = 0x40026bb1bbb55516ULL},
        {.x = 1.0000000009313226, .bits = 0x3e0fffffffc00000ULL},       // 1 + 2^-30
        {.x = 0.9999999990686774, .bits = 0xbe10000000200000ULL},       // 1 - 2^-30
        {.x = 1.0000004768371582, .bits = 0x3e9fffff800002abULL},       // 1 + 2^-21
        {.x = 4.9E-324, .bits = 0xc0874385446d71c3ULL},                 // the least subnormal
        {.x = 1.5E-323, .bits = 0xc0873abb4f301b42ULL},                 // three times that
        {.x = 2.2250738585072014E-308, .bits = 0xc086232bdd7abcd2ULL},  // the least normal
        {.x = 1.7976931348623157E308, .bits = 0x40862e42fefa39efULL},   // the largest double
        {.x = 0.1, .bits = 0xc0026bb1bbb55515ULL},
        {.x = 0.7, .bits = 0xbfd6d3c324e13f50ULL},
        {.x = 1.5, .bits = 0x3fd9f323ecbf984cULL},
        {.x = 1.0E-300, .bits = 0xc085963447f87fb5ULL},
        {.x = 1.0E300, .bits = 0x4085963447f87fb5ULL},
    }};
    for (const PinnedLog& pinned : kLogs)
    {
        EXPECT_EQ(bitsOf(GaussianRandom::strictLog(pinned.x)), pinned.bits) << pinned.x;
    }
}

TEST(GaussianRandom, StrictLogOfZeroNegativeInfiniteAndNaN)
{
    constexpr double kInfinity = std::numeric_limits<double>::infinity();
    EXPECT_EQ(GaussianRandom::strictLog(0.0), -kInfinity);
    EXPECT_EQ(GaussianRandom::strictLog(-0.0), -kInfinity);
    EXPECT_EQ(GaussianRandom::strictLog(kInfinity), kInfinity);
    EXPECT_TRUE(std::isnan(GaussianRandom::strictLog(-1.0)));
    EXPECT_TRUE(std::isnan(GaussianRandom::strictLog(-kInfinity)));
    EXPECT_TRUE(std::isnan(GaussianRandom::strictLog(-4.9E-324)));
    EXPECT_TRUE(std::isnan(GaussianRandom::strictLog(std::numeric_limits<double>::quiet_NaN())));
}

/// The digests of the arguments and of their logarithms that the test below makes.
struct LogDigests
{
    std::string arguments;
    std::string logarithms;
    int         subnormal{0};
};

/// 100,000 arguments of the log, as GaussianProbe.java makes them from a SplitMix64 of the seed
/// 20261014: for an even index a nextDouble() (the arguments the polar method has), for an odd
/// one the 63 lower bits of nextBits() as a double, drawn again while they are an infinity or a
/// NaN (every exponent, subnormal numbers included), with the digests of the arguments and of
/// their strictLog().
[[nodiscard]] LogDigests logDigests()
{
    constexpr std::uint64_t kNoSign       = 0x7fffffffffffffffULL;
    constexpr std::uint64_t kExponentMask = 0x7ffU;
    GaussianRandom          random(20261014);
    std::vector<std::byte>  arguments;
    std::vector<std::byte>  logarithms;
    LogDigests              digests;
    for (int i = 0; i < 100'000; ++i)
    {
        double x = 0;
        if (i % 2 == 0)
        {
            x = random.nextDouble();
        }
        else
        {
            std::uint64_t bits = random.nextBits() & kNoSign;
            while ((bits >> 52U) == kExponentMask)
            {
                bits = random.nextBits() & kNoSign;
            }
            x = std::bit_cast<double>(bits);
        }
        digests.subnormal += x != 0 && x < std::numeric_limits<double>::min() ? 1 : 0;
        appendLittleEndian(arguments, x);
        appendLittleEndian(logarithms, GaussianRandom::strictLog(x));
    }
    digests.arguments  = QtRocket::sha256Hex(arguments);
    digests.logarithms = QtRocket::sha256Hex(logarithms);
    return digests;
}

// The logarithms of 100,000 arguments of every exponent are StrictMath.log's, bit for bit (the
// scouts compared 400,000 others one by one: no difference, where glibc's log differed in
// 14,807).
TEST(GaussianRandom, StrictLogEqualsStrictMathLogOnAHundredThousandArguments)
{
    const LogDigests digests = logDigests();
    EXPECT_EQ(digests.arguments,
              "a69382b91c4a9f8d470e3f3199532c6f94d779692ff6009eaa12a2f82bac5934");
    EXPECT_EQ(digests.logarithms,
              "76ea931d0d92da955cb6bd1722c5042e3473cf3a5bfdccf8e15a3816a1591fe7");
    EXPECT_EQ(digests.subnormal, 24);
}

}  // namespace
