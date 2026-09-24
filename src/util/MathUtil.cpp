#include "QtRocket/util/MathUtil.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket::MathUtil
{

namespace
{

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// Java's Double.compare as a strict weak "less than": -0.0 < 0.0, every NaN is equal to every
/// other NaN and greater than everything else. std::sort with plain < is undefined on NaN input.
bool javaDoubleLess(double a, double b) noexcept
{
    if (a < b)
    {
        return true;
    }
    if (a > b)
    {
        return false;
    }
    const bool aNaN = std::isnan(a);
    const bool bNaN = std::isnan(b);
    if (aNaN || bNaN)
    {
        return !aNaN && bNaN;
    }
    return std::signbit(a) && !std::signbit(b);
}

}  // namespace

double map(double value, double fromMin, double fromMax, double toMin, double toMax)
{
    if (equals(toMin, toMax))
    {
        return toMin;
    }
    if (equals(fromMin, fromMax))
    {
        bug(
            std::format("from range is singular and to range is not: value={} fromMin={} "
                        "fromMax={} toMin={} toMax={}",
                        value, fromMin, fromMax, toMin, toMax));
    }
    return ((value - fromMin) / (fromMax - fromMin) * (toMax - toMin)) + toMin;
}

Coordinate map(double value, double fromMin, double fromMax, const Coordinate& toMin,
               const Coordinate& toMax)
{
    if (toMin == toMax)
    {
        return toMin;
    }
    if (equals(fromMin, fromMax))
    {
        bug(
            std::format("from range is singular and to range is not: value={} fromMin={} "
                        "fromMax={} toMin={} toMax={}",
                        value, fromMin, fromMax, toMin.toString(), toMax.toString()));
    }
    const double a = (value - fromMin) / (fromMax - fromMin);
    return toMax.multiply(a).add(toMin.multiply(1 - a));
}

double min(double x, double y) noexcept
{
    if (std::isnan(y))
    {
        return x;
    }
    return (x < y) ? x : y;
}

double max(double x, double y) noexcept
{
    if (std::isnan(x))
    {
        return y;
    }
    return (x < y) ? y : x;
}

double min(double x, double y, double z) noexcept
{
    if (x < y || std::isnan(y))
    {
        return min(x, z);
    }
    return min(y, z);
}

double min(double w, double x, double y, double z) noexcept
{
    return min(min(w, x), min(y, z));
}

double max(double x, double y, double z) noexcept
{
    if (x > y || std::isnan(y))
    {
        return max(x, z);
    }
    return max(y, z);
}

double hypot(double x, double y) noexcept
{
    return std::sqrt((x * x) + (y * y));
}

double reducePi(double x) noexcept
{
    // Java's Math.rint rounds half to even, as nearbyint does in the default rounding mode.
    const double d = std::nearbyint(x / (2 * std::numbers::pi));
    return x - (d * 2 * std::numbers::pi);
}

double reduce2Pi(double x) noexcept
{
    const double d = std::floor(x / (2 * std::numbers::pi));
    return x - (d * 2 * std::numbers::pi);
}

double safeSqrt(double d) noexcept
{
    // OpenRocket also logs a warning here; the core has no logger, so the value is just clamped.
    if (d < 0)
    {
        return 0;
    }
    return std::sqrt(d);
}

bool equals(double a, double b, double epsilon) noexcept
{
    const double absb = std::abs(b);

    if (absb < epsilon / 2)
    {
        // Near zero
        return std::abs(a) < epsilon / 2;
    }
    return std::abs(a - b) < epsilon * absb;
}

bool equals(double a, double b) noexcept
{
    return equals(a, b, kEpsilon);
}

int javaIntCast(double value) noexcept
{
    if (std::isnan(value))
    {
        return 0;
    }
    if (value >= static_cast<double>(std::numeric_limits<int>::max()))
    {
        return std::numeric_limits<int>::max();
    }
    if (value <= static_cast<double>(std::numeric_limits<int>::min()))
    {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(value);
}

double average(std::span<const double> values) noexcept
{
    if (values.empty())
    {
        return kNaN;
    }

    double avg = 0.0;
    for (const double value : values)
    {
        avg += value;
    }
    return avg / static_cast<double>(values.size());
}

double stddev(std::span<const double> values) noexcept
{
    if (values.size() < 2)
    {
        return kNaN;
    }

    const double avg = average(values);
    double       sum = 0.0;
    for (const double value : values)
    {
        sum += pow2(value - avg);
    }
    return std::sqrt(sum / static_cast<double>(values.size() - 1));
}

double median(std::span<const double> values)
{
    if (values.empty())
    {
        return kNaN;
    }

    std::vector<double> sorted(values.begin(), values.end());
    std::ranges::sort(sorted, javaDoubleLess);

    const std::size_t n = sorted.size();
    if (n % 2 == 0)
    {
        return (sorted[n / 2] + sorted[(n / 2) - 1]) / 2;
    }
    return sorted[n / 2];
}

double interpolate(std::span<const double> domain, std::span<const double> range, double t) noexcept
{
    if (domain.size() != range.size())
    {
        return kNaN;
    }

    const std::size_t length = domain.size();
    if (length <= 1 || t < domain[0] || t > domain[length - 1] + kEpsilon)
    {
        return kNaN;
    }

    // Look for the index of the right end point. The guard above bounds it by length - 1.
    std::size_t right = 1;
    while (t > domain[right] + kEpsilon)
    {
        right++;
    }
    const std::size_t left = right - 1;

    const double deltax = domain[right] - domain[left];
    const double deltay = range[right] - range[left];

    // For numerical stability, if deltax is small,
    if (std::abs(deltax) < kEpsilon)
    {
        if (deltay < -1.0 * kEpsilon)
        {
            return -std::numeric_limits<double>::infinity();
        }
        if (deltay > kEpsilon)
        {
            return std::numeric_limits<double>::infinity();
        }
        return 0.0;
    }

    return range[left] + ((t - domain[left]) * deltay / deltax);
}

int javaDoubleCompare(double a, double b) noexcept
{
    if (javaDoubleLess(a, b))
    {
        return -1;
    }
    if (javaDoubleLess(b, a))
    {
        return 1;
    }
    return 0;
}

double signum(double d) noexcept
{
    if (d > 0)
    {
        return 1.0;
    }
    if (d < 0)
    {
        return -1.0;
    }
    return d;  // a zero of either sign, or NaN
}

double javaMax(double a, double b) noexcept
{
    if (std::isnan(a))
    {
        return a;
    }
    if (a == 0.0 && b == 0.0 && std::signbit(a))
    {
        return b;  // max(-0.0, +-0.0) is the second zero, so +0.0 wins over -0.0
    }
    return (a >= b) ? a : b;  // a NaN b fails the comparison and comes back
}

int javaDoubleHashCode(double value) noexcept
{
    constexpr std::uint64_t kCanonicalNaN = 0x7ff8000000000000ULL;
    const std::uint64_t     bits =
        std::isnan(value) ? kCanonicalNaN : std::bit_cast<std::uint64_t>(value);
    // (int) of a long keeps the low 32 bits; the conversion to int is modular (C++20).
    return static_cast<int>(static_cast<std::uint32_t>(bits ^ (bits >> 32U)));
}

std::int64_t javaLongCast(double a) noexcept
{
    if (std::isnan(a))
    {
        return 0;
    }
    constexpr double kTwoPow63 = 9223372036854775808.0;
    if (a >= kTwoPow63)
    {
        return std::numeric_limits<std::int64_t>::max();
    }
    if (a <= -kTwoPow63)
    {
        return std::numeric_limits<std::int64_t>::min();
    }
    return static_cast<std::int64_t>(a);
}

std::int64_t javaRound(double a) noexcept
{
    // Math.round as JDK 8+ implements it: the significand is shifted so that one bit of fraction
    // remains, then floor(a + 1/2) is formed exactly in integer arithmetic.
    constexpr std::int64_t kExpBitMask       = 0x7FF0000000000000LL;
    constexpr std::int64_t kSignifBitMask    = 0x000FFFFFFFFFFFFFLL;
    constexpr int          kSignificandWidth = 53;
    constexpr int          kExpBias          = 1023;

    const auto         longBits  = std::bit_cast<std::int64_t>(a);
    const std::int64_t biasedExp = (longBits & kExpBitMask) >> (kSignificandWidth - 1);
    const std::int64_t shift     = (kSignificandWidth - 2 + kExpBias) - biasedExp;
    if ((shift & -64) == 0)
    {
        // a is finite and 2^-64 <= ulp(a) < 1: r is a / ulp(a).
        std::int64_t r = (longBits & kSignifBitMask) | (kSignifBitMask + 1);
        if (longBits < 0)
        {
            r = -r;
        }
        return ((r >> shift) + 1) >> 1;
    }
    // |a| < 2^-11 (rounds to 0), an integer already, an infinity or NaN.
    return javaLongCast(a);
}

}  // namespace QtRocket::MathUtil
