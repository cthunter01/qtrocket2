#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace QtRocket
{

/// A source of pink noise with a power spectral density proportional to 1/f^alpha, as
/// OpenRocket's PinkNoise: Gaussian random numbers passed through the IIR filter of N. Jeremy
/// Kasdin, Proceedings of the IEEE, Vol. 83, No. 5, May 1995, p. 822. The more poles, the lower
/// the frequencies that are still amplified; three already give good results. Below the cutoff
/// frequency the density is flat.
///
/// Deviation from OpenRocket: the Gaussian input comes from std::mt19937 and
/// std::normal_distribution instead of java.util.Random, so a seeded run is reproducible within
/// QtRocket (for one standard library: normal_distribution is not specified bit for bit across
/// libstdc++, libc++ and MSVC) but never bit-identical to OpenRocket's sequence for the same seed.
/// PinkNoise.main(String[]), a development harness with no effect (an empty loop and commented-out
/// statistics), is not ported.
class PinkNoise
{
public:
    /// alpha = 1 with a five-pole filter and a nondeterministic seed.
    PinkNoise();
    /// A five-pole filter with a nondeterministic seed.
    explicit PinkNoise(double alpha);
    /// A nondeterministic seed from std::random_device, like Java's new Random().
    PinkNoise(double alpha, int poles);
    /// Throws std::invalid_argument when @p poles is negative (Java: NegativeArraySizeException).
    /// Zero poles give plain white noise (Java fails on the first nextValue() then). OpenRocket
    /// keeps its seed as an int (PinkNoiseWindModel): pass static_cast<std::uint32_t>(seed),
    /// which keeps the bit pattern.
    PinkNoise(double alpha, int poles, std::uint32_t seed);

    /// The next sample. The filter history is primed with 5 * poles samples at construction.
    double nextValue();

    [[nodiscard]] int poles() const noexcept { return static_cast<int>(m_values.size()); }

private:
    std::vector<double>              m_multipliers;
    std::vector<double>              m_values;  ///< the most recent outputs, newest first
    std::mt19937                     m_generator;
    std::normal_distribution<double> m_gaussian;
};

}  // namespace QtRocket
