#include "QtRocket/util/GaussianRandom.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace QtRocket
{

namespace
{

// SplitMix64 (Vigna's splitmix64.c).
constexpr std::uint64_t kGoldenGamma = 0x9e3779b97f4a7c15ULL;
constexpr std::uint64_t kMix1        = 0xbf58476d1ce4e5b9ULL;
constexpr std::uint64_t kMix2        = 0x94d049bb133111ebULL;
/// 2^-53: a double of 53 random bits is in [0, 1).
constexpr double kDoubleUnit = 0x1.0p-53;

// fdlibm 5.3, e_log.c: ln 2 in two parts (the upper one with 32 trailing zero bits), 2^54, and
// the coefficients of the polynomial, each with its bits as fdlibm gives them.
// NOLINTNEXTLINE(modernize-use-std-numbers): the upper part of ln 2, not ln 2 (fdlibm's ln2_hi)
constexpr double kLn2Hi    = 6.93147180369123816490e-01;  // 3fe62e42 fee00000
constexpr double kLn2Lo    = 1.90821492927058770002e-10;  // 3dea39ef 35793c76
constexpr double kTwo54    = 1.80143985094819840000e+16;  // 43500000 00000000
constexpr double kLg1      = 6.666666666666735130e-01;    // 3FE55555 55555593
constexpr double kLg2      = 3.999999999940941908e-01;    // 3FD99999 9997FA04
constexpr double kLg3      = 2.857142874366239149e-01;    // 3FD24924 94229359
constexpr double kLg4      = 2.222219843214978396e-01;    // 3FCC71C5 1D8E78AF
constexpr double kLg5      = 1.818357216161805012e-01;    // 3FC74664 96CB03DE
constexpr double kLg6      = 1.531383769920937332e-01;    // 3FC39A09 D078C69F
constexpr double kLg7      = 1.479819860511658591e-01;    // 3FC2F112 DF3E5244
constexpr double kOneThird = 0.33333333333333333;

/// The upper 32 bits of @p x as a signed number (fdlibm: __HI(x), an int).
[[nodiscard]] std::int32_t highWord(double x) noexcept
{
    return static_cast<std::int32_t>(
        static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(x) >> 32U));
}

/// The lower 32 bits of @p x (fdlibm: __LO(x)).
[[nodiscard]] std::uint32_t lowWord(double x) noexcept
{
    return static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(x) & 0xffffffffU);
}

/// @p x with its upper 32 bits replaced by @p high (fdlibm: __HI(x) = high).
[[nodiscard]] double withHighWord(double x, std::uint32_t high) noexcept
{
    return std::bit_cast<double>((static_cast<std::uint64_t>(high) << 32U) | lowWord(x));
}

/// log(2^k (1 + f)) for |f| < 2^-20, fdlibm's branch for an argument near a power of two.
[[nodiscard]] double logNearAPowerOfTwo(double f, std::int32_t k) noexcept
{
    const auto dk = static_cast<double>(k);
    if (f == 0.0)
    {
        if (k == 0)
        {
            return 0.0;
        }
        // dk*ln2_hi + dk*ln2_lo
        const double high = dk * kLn2Hi;
        const double low  = dk * kLn2Lo;
        return high + low;
    }
    // R = f*f*(0.5 - 0.33333333333333333*f)
    const double third  = kOneThird * f;
    const double factor = 0.5 - third;
    const double square = f * f;
    const double r      = square * factor;
    if (k == 0)
    {
        return f - r;
    }
    // dk*ln2_hi - ((R - dk*ln2_lo) - f)
    const double low     = dk * kLn2Lo;
    const double withLow = r - low;
    const double withF   = withLow - f;
    const double high    = dk * kLn2Hi;
    return high - withF;
}

/// The polynomial of fdlibm in z = s^2 and w = z^2: R = t2 + t1 with
/// t1 = w*(Lg2 + w*(Lg4 + w*Lg6)) and t2 = z*(Lg1 + w*(Lg3 + w*(Lg5 + w*Lg7))).
[[nodiscard]] double polynomial(double z, double w) noexcept
{
    const double w6 = w * kLg6;
    const double p4 = kLg4 + w6;
    const double w4 = w * p4;
    const double p2 = kLg2 + w4;
    const double t1 = w * p2;
    const double w7 = w * kLg7;
    const double p5 = kLg5 + w7;
    const double w5 = w * p5;
    const double p3 = kLg3 + w5;
    const double w3 = w * p3;
    const double p1 = kLg1 + w3;
    const double t2 = z * p1;
    return t2 + t1;
}

/// log(2^k (1 + f)) for |f| >= 2^-20 and 1 + f in [sqrt(2)/2, sqrt(2)), where @p hx is the
/// upper word of the mantissa of the argument (fdlibm's last part).
[[nodiscard]] double logOfReduced(double f, std::int32_t hx, std::int32_t k) noexcept
{
    const double       twoPlusF = 2.0 + f;
    const double       s        = f / twoPlusF;
    const auto         dk       = static_cast<double>(k);
    const double       z        = s * s;
    const std::int32_t i        = hx - 0x6147a;
    const double       w        = z * z;
    const std::int32_t j        = 0x6b851 - hx;
    const double       r        = polynomial(z, w);
    if ((i | j) > 0)
    {
        // hfsq = 0.5*f*f
        const double halfF = 0.5 * f;
        const double hfsq  = halfF * f;
        const double sum   = hfsq + r;
        const double sR    = s * sum;
        if (k == 0)
        {
            // f - (hfsq - s*(hfsq+R))
            const double rest = hfsq - sR;
            return f - rest;
        }
        // dk*ln2_hi - ((hfsq - (s*(hfsq+R) + dk*ln2_lo)) - f)
        const double low     = dk * kLn2Lo;
        const double withLow = sR + low;
        const double rest    = hfsq - withLow;
        const double withF   = rest - f;
        const double high    = dk * kLn2Hi;
        return high - withF;
    }
    const double difference = f - r;
    const double sR         = s * difference;
    if (k == 0)
    {
        // f - s*(f-R)
        return f - sR;
    }
    // dk*ln2_hi - ((s*(f-R) - dk*ln2_lo) - f)
    const double low     = dk * kLn2Lo;
    const double withLow = sR - low;
    const double withF   = withLow - f;
    const double high    = dk * kLn2Hi;
    return high - withF;
}

}  // namespace

std::uint64_t GaussianRandom::nextBits() noexcept
{
    m_state += kGoldenGamma;
    std::uint64_t z = m_state;
    z               = (z ^ (z >> 30U)) * kMix1;
    z               = (z ^ (z >> 27U)) * kMix2;
    return z ^ (z >> 31U);
}

double GaussianRandom::nextDouble() noexcept
{
    return static_cast<double>(nextBits() >> 11U) * kDoubleUnit;
}

double GaussianRandom::nextGaussian() noexcept
{
    if (m_haveNextGaussian)
    {
        m_haveNextGaussian = false;
        return m_nextGaussian;
    }
    double v1 = 0;
    double v2 = 0;
    double s  = 0;
    while (true)
    {
        // v = 2 * u - 1, the product apart (it is exact anyway); v1 is drawn first, as in Java.
        const double twice1  = 2 * nextDouble();
        v1                   = twice1 - 1;
        const double twice2  = 2 * nextDouble();
        v2                   = twice2 - 1;
        const double square1 = v1 * v1;
        const double square2 = v2 * v2;
        s                    = square1 + square2;
        // Java: do { ... } while (s >= 1 || s == 0); s is never NaN.
        if (s < 1 && s != 0)
        {
            break;
        }
    }
    const double multiplier = std::sqrt(-2 * strictLog(s) / s);
    m_nextGaussian          = v2 * multiplier;
    m_haveNextGaussian      = true;
    return v1 * multiplier;
}

double GaussianRandom::strictLog(double x) noexcept
{
    std::int32_t        hx = highWord(x);
    const std::uint32_t lx = lowWord(x);
    std::int32_t        k  = 0;
    if (hx < 0x00100000)
    {
        // x < 2^-1022
        if ((static_cast<std::uint32_t>(hx & 0x7fffffff) | lx) == 0)
        {
            return -std::numeric_limits<double>::infinity();  // log(+-0) = -inf
        }
        if (hx < 0)
        {
            return std::numeric_limits<double>::quiet_NaN();  // log(-#) = NaN
        }
        // A subnormal number: scale x up.
        k -= 54;
        x *= kTwo54;
        hx = highWord(x);
    }
    if (hx >= 0x7ff00000)
    {
        return x + x;
    }
    k += (hx >> 20) - 1023;
    hx &= 0x000fffff;
    const std::int32_t i = (hx + 0x95f64) & 0x100000;
    // Normalise x or x / 2 into [sqrt(2)/2, sqrt(2)).
    x = withHighWord(x, static_cast<std::uint32_t>(hx | (i ^ 0x3ff00000)));
    k += i >> 20;
    const double f = x - 1.0;
    if ((0x000fffff & (2 + hx)) < 3)
    {
        // |f| < 2^-20
        return logNearAPowerOfTwo(f, k);
    }
    return logOfReduced(f, hx, k);
}

}  // namespace QtRocket
