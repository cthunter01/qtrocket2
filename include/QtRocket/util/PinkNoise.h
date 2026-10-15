#pragma once

#include <cstdint>
#include <vector>

#include "QtRocket/util/GaussianRandom.h"

namespace QtRocket
{

/// A source of pink noise with a power spectral density proportional to 1/f^alpha, as
/// OpenRocket's PinkNoise: Gaussian random numbers passed through the IIR filter of N. Jeremy
/// Kasdin, Proceedings of the IEEE, Vol. 83, No. 5, May 1995, p. 822. The more poles, the lower
/// the frequencies that are still amplified; three already give good results. Below the cutoff
/// frequency the density is flat.
///
/// Deviation from OpenRocket (the user's decision D12 of 2026-10-14): the Gaussian input comes
/// from GaussianRandom, a generator of QtRocket's own, instead of java.util.Random's
/// nextGaussian(). A seeded source gives the same values, bit for bit, on every platform, but
/// never OpenRocket's sequence for the same seed. The filter is made of products and
/// differences, and that it is bit-identical rests on no product being fused with the
/// difference into a multiply-add: on -ffp-contract=off, which QtRocket_configure_target()
/// passes to every target under GCC and Clang (GCC's default, -ffp-contract=fast, contracts
/// across statements too), and on MSVC's /fp:precise, which does not contract. That each product
/// stands in a statement of its own guards only against Clang's default, -ffp-contract=on,
/// which contracts within an expression.
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
    /// The Gaussian input is GaussianRandom(@p seed). Throws BugError when @p poles is negative
    /// (Java: NegativeArraySizeException). Zero poles give plain white noise (Java fails on the
    /// first nextValue() then). OpenRocket keeps its seed as an int (PinkNoiseWindModel): pass
    /// static_cast<std::uint32_t>(seed), which keeps the bit pattern.
    PinkNoise(double alpha, int poles, std::uint32_t seed);

    /// The next sample. The filter history is primed with 5 * poles samples at construction.
    double nextValue();

    [[nodiscard]] int poles() const noexcept { return static_cast<int>(m_values.size()); }

private:
    std::vector<double> m_multipliers;
    std::vector<double> m_values;  ///< the most recent outputs, newest first
    GaussianRandom      m_gaussian;
};

}  // namespace QtRocket
