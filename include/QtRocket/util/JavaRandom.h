#pragma once

#include <cstdint>

namespace QtRocket
{

/// java.util.Random, reproduced exactly: the 48-bit linear congruential generator of the Java
/// class library (the one of drand48: seed' = (seed * 0x5DEECE66D + 0xB) mod 2^48), with Java's
/// scrambling of the seed and Java's way of making an int and a double of the state. It uses
/// integer operations and, in nextDouble(), one multiplication by a power of two, so a sequence
/// is the same on every platform and equals what Java draws for the same seed.
///
/// It exists for the pitch and yaw jitter of the simulation (AbstractRkSimulationStepper), which
/// OpenRocket draws from a java.util.Random: with it a QtRocket run and an OpenRocket run of the
/// same seed see the same jitter. It is not a general-purpose generator (the low bits of an LCG
/// are weak) and nothing else should start using it: the wind turbulence keeps its own source
/// (PinkNoise).
///
/// Only what the simulation draws is ported: setSeed(), next(), nextInt() and nextDouble().
///
/// Deviations from java.util.Random:
/// - Not thread-safe (Java keeps the state in an AtomicLong): one generator belongs to one
///   simulation stepper, which belongs to one thread.
/// - There is no constructor without a seed (Java seeds it from the clock).
/// - next() is public (Java: protected), so that it can be tested.
class JavaRandom
{
public:
    /// A generator with the seed @p seed (Java: new Random(seed)). A Java int seed is passed as
    /// it is: the conversion to std::int64_t extends the sign, as Java's widening does.
    explicit constexpr JavaRandom(std::int64_t seed) noexcept : m_seed(initialScramble(seed)) { }

    /// Starts the sequence of @p seed anew (Java: setSeed(seed)): the state is
    /// (seed ^ 0x5DEECE66D) mod 2^48.
    constexpr void setSeed(std::int64_t seed) noexcept { m_seed = initialScramble(seed); }

    /// The next @p bits pseudorandom bits, 1 ... 32 of them (Java: next(bits)): advances the
    /// state and returns its upper bits, narrowed as Java's (int) cast narrows, so 32 bits give
    /// any int and fewer bits a value from 0 to 2^bits - 1.
    [[nodiscard]] constexpr std::int32_t next(int bits) noexcept
    {
        m_seed = ((m_seed * kMultiplier) + kAddend) & kMask;
        // Java: (int) (nextseed >>> (48 - bits)); the narrowing to 32 bits is modular.
        return static_cast<std::int32_t>(static_cast<std::uint32_t>(m_seed >> (48 - bits)));
    }

    /// The next int, uniformly from all 2^32 values (Java: nextInt()).
    [[nodiscard]] constexpr std::int32_t nextInt() noexcept { return next(32); }

    /// The next double, uniformly from [0, 1) in steps of 2^-53 (Java: nextDouble()): 26 and
    /// then 27 bits, as (((long) next(26) << 27) + next(27)) * 0x1.0p-53.
    [[nodiscard]] constexpr double nextDouble() noexcept
    {
        // Two statements: Java evaluates the operands from left to right, C++ in any order.
        const std::int64_t high = next(26);
        const std::int64_t low  = next(27);
        return static_cast<double>((high << 27) + low) * kDoubleUnit;
    }

private:
    static constexpr std::uint64_t kMultiplier = 0x5DEECE66DULL;
    static constexpr std::uint64_t kAddend     = 0xBULL;
    static constexpr std::uint64_t kMask       = (1ULL << 48) - 1;
    /// 1.0 / (1L << 53) (Java: DOUBLE_UNIT, 0x1.0p-53).
    static constexpr double kDoubleUnit = 0x1.0p-53;

    /// Java's initialScramble(): (seed ^ multiplier) & mask.
    [[nodiscard]] static constexpr std::uint64_t initialScramble(std::int64_t seed) noexcept
    {
        return (static_cast<std::uint64_t>(seed) ^ kMultiplier) & kMask;
    }

    /// The state: 48 bits. Unsigned, so that the multiplication wraps as Java's long does.
    std::uint64_t m_seed;
};

}  // namespace QtRocket
