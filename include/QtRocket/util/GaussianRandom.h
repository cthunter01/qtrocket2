#pragma once

#include <cstdint>

namespace QtRocket
{

/// Normally distributed random numbers that are the same, bit for bit, on every platform: the
/// generator of QtRocket's own the wind turbulence draws from (PinkNoise), the user's decision
/// D12 of 2026-10-14. No OpenRocket class; OpenRocket draws from java.util.Random's
/// nextGaussian(), which the user decided against.
///
/// Made only of operations that IEEE 754 defines exactly or correctly rounded (+, -, *, /, sqrt,
/// integer operations and the bits of a double), so that every compiler and processor gives the
/// same numbers:
/// - the uniform source is SplitMix64 (Steele, Lea and Flood, 2014; the constants of Vigna's
///   splitmix64.c): 64 bits of state, which start as the seed and grow by 0x9e3779b97f4a7c15
///   with every draw, mixed into the output. A double is the upper 53 bits times 2^-53, exact;
/// - a Gaussian is made by Marsaglia's polar method, as java.util.Random.nextGaussian() makes
///   one: v1, v2 = 2u - 1 until s = v1^2 + v2^2 lies in (0, 1), then v1 * m and v2 * m with
///   m = sqrt(-2 log(s) / s); the second value is kept for the next draw;
/// - the logarithm is strictLog(), a transcription of fdlibm's __ieee754_log (the algorithm of
///   Java's StrictMath.log), not std::log, whose last bit differs between glibc, Apple's libm and
///   the UCRT; sqrt is correctly rounded everywhere.
/// No expression multiplies and adds in one (a*b + c): a compiler that contracted it into a
/// fused multiply-add would change the last bit. The project also compiles every target with
/// -ffp-contract=off (GCC and Clang; MSVC's /fp:precise does not contract), and
/// GaussianRandomTests pins the SHA-256 digest of 100,000 values, which CI checks on Linux, macOS
/// and Windows. Java's arithmetic is strict: GaussianProbe.java of the probes of tier 10a, part
/// "goldens-and-turbulence", computes the same numbers with StrictMath.log and StrictMath.sqrt.
///
/// Not thread-safe: one generator belongs to one noise source.
class GaussianRandom
{
public:
    /// A generator whose state starts as @p seed.
    explicit constexpr GaussianRandom(std::uint64_t seed) noexcept : m_state(seed) { }

    /// The next 64 random bits (SplitMix64).
    std::uint64_t nextBits() noexcept;

    /// The next double, uniformly from [0, 1) in steps of 2^-53: the upper 53 bits of
    /// nextBits() times 2^-53.
    double nextDouble() noexcept;

    /// The next value of the standard normal distribution (mean 0, standard deviation 1), by
    /// the polar method (see the class comment): every other call returns the value the call
    /// before it made along with its own.
    double nextGaussian() noexcept;

    /// The natural logarithm of @p x as fdlibm's __ieee754_log computes it (Java:
    /// StrictMath.log), with its error below one ulp: -infinity for +-0, NaN for a negative
    /// number and for NaN, +infinity for +infinity.
    [[nodiscard]] static double strictLog(double x) noexcept;

private:
    std::uint64_t m_state;
    double        m_nextGaussian{0};
    bool          m_haveNextGaussian{false};
};

}  // namespace QtRocket
