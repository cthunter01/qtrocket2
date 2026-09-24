#pragma once

#include <concepts>
#include <numbers>
#include <span>

namespace QtRocket
{

class Coordinate;

/// The numeric helpers the rest of the core uses, ported from OpenRocket's MathUtil. Every function
/// keeps OpenRocket's formulas, tolerances and NaN behaviour; the ones that need <cmath> are
/// defined in MathUtil.cpp, the rest are constexpr.
namespace MathUtil
{

/// The relative tolerance of equals(): 1e-8 ("10mm^3 in m^3" in OpenRocket).
inline constexpr double kEpsilon = 0.00000001;

/// x^2
[[nodiscard]] constexpr double pow2(double x) noexcept
{
    return x * x;
}

/// x^3
[[nodiscard]] constexpr double pow3(double x) noexcept
{
    return x * x * x;
}

/// x^4
[[nodiscard]] constexpr double pow4(double x) noexcept
{
    return (x * x) * (x * x);
}

/// Clamps x to [min, max]. A NaN x comes back unchanged, since neither comparison holds. Float
/// arguments widen to this overload: OpenRocket's clamp(float, float, float) is not ported, since
/// a separate float overload would make a call like clamp(1.5f, 0, 1) ambiguous in C++.
[[nodiscard]] constexpr double clamp(double x, double min, double max) noexcept
{
    if (x < min)
    {
        return min;
    }
    if (x > max)
    {
        return max;
    }
    return x;
}

/// Clamps an integer to [min, max]. A template so that a mixed call such as clamp(someDouble, 0, 1)
/// fails deduction here and takes the double overload, as Java's widening does, instead of being
/// ambiguous.
template <std::integral T>
[[nodiscard]] constexpr T clamp(T x, T min, T max) noexcept
{
    if (x < min)
    {
        return min;
    }
    if (x > max)
    {
        return max;
    }
    return x;
}

/// Integers of mixed types, such as clamp(someSizeT, 0, n), would otherwise fall through to the
/// double overload and come back as a double. That call is rejected instead: give the bounds the
/// value's type. (Java would widen such a call to its float overload, which is not ported.)
template <std::integral T, std::integral U, std::integral V>
    requires(!std::same_as<T, U> || !std::same_as<T, V>)
auto clamp(T x, U min, V max) = delete;

/// Maps @p value from the range [fromMin, fromMax] onto [toMin, toMax] linearly. When the
/// destination range is singular (toMin equals toMax within kEpsilon) the result is toMin.
/// @throws BugError when the source range is singular but the destination is not (OpenRocket:
///         IllegalArgumentException).
[[nodiscard]] double map(double value, double fromMin, double fromMax, double toMin, double toMax);

/// Maps @p value from [fromMin, fromMax] onto the segment from @p toMin to @p toMax: the result is
/// toMax * a + toMin * (1 - a), weights included. Same singular-range rules as the double overload.
/// @throws BugError when the source range is singular but the destination is not.
[[nodiscard]] Coordinate map(double value, double fromMin, double fromMax, const Coordinate& toMin,
                             const Coordinate& toMax);

/// The smaller of two values by direct comparison; when one is NaN the other is returned.
[[nodiscard]] double min(double x, double y) noexcept;

/// The larger of two values by direct comparison; when one is NaN the other is returned.
[[nodiscard]] double max(double x, double y) noexcept;

/// The smallest of three values, ignoring NaNs unless all are NaN.
[[nodiscard]] double min(double x, double y, double z) noexcept;

/// The smallest of four values, ignoring NaNs unless all are NaN.
[[nodiscard]] double min(double w, double x, double y, double z) noexcept;

/// The largest of three values, ignoring NaNs unless all are NaN.
[[nodiscard]] double max(double x, double y, double z) noexcept;

/// sqrt(x^2 + y^2), computed directly (OpenRocket deliberately avoids the slower, overflow-safe
/// library hypot; this matches its rounding).
[[nodiscard]] double hypot(double x, double y) noexcept;

/// Reduces an angle to the range -pi ... pi. Either -pi or pi may be returned at the boundary.
[[nodiscard]] double reducePi(double x) noexcept;

/// Reduces an angle to the range 0 ... 2*pi.
[[nodiscard]] double reduce2Pi(double x) noexcept;

/// The square root of @p d, or zero when @p d is negative (rounding errors may push a value that
/// should be zero slightly below it). A NaN stays NaN.
[[nodiscard]] double safeSqrt(double d) noexcept;

/// True when @p a and @p b differ by less than @p epsilon relative to |b|; values within epsilon/2
/// of zero are compared to zero absolutely. Never true when either value is NaN.
[[nodiscard]] bool equals(double a, double b, double epsilon) noexcept;

/// equals(a, b, kEpsilon)
[[nodiscard]] bool equals(double a, double b) noexcept;

/// -1.0 when x < 0, otherwise 1.0 (also for zero and NaN, unlike a signum).
[[nodiscard]] constexpr double sign(double x) noexcept
{
    return (x < 0) ? -1.0 : 1.0;
}

/// The arithmetic mean, or NaN for no values.
[[nodiscard]] double average(std::span<const double> values) noexcept;

/// The sample standard deviation (n - 1 in the denominator), or NaN for fewer than two values.
[[nodiscard]] double stddev(std::span<const double> values) noexcept;

/// The median (the mean of the two middle values for an even count), or NaN for no values. Values
/// are ordered as Java's Double.compare does: -0.0 before 0.0 and NaN after everything.
[[nodiscard]] double median(std::span<const double> values);

/// Linear interpolation of the sampled function (domain[i], range[i]) at @p t. The domain must be
/// sorted. Returns NaN when the spans differ in size, hold fewer than two samples, or @p t lies
/// outside the domain (with kEpsilon slack past the last sample). A segment shorter than kEpsilon
/// gives +inf, -inf or 0 depending on the sign of its rise.
[[nodiscard]] double interpolate(std::span<const double> domain, std::span<const double> range,
                                 double t) noexcept;

/// a + (b - a) * fraction
[[nodiscard]] constexpr double interpolate(double a, double b, double fraction) noexcept
{
    return a + ((b - a) * fraction);
}

/// Degrees to radians.
[[nodiscard]] constexpr double deg2rad(double deg) noexcept
{
    return deg * std::numbers::pi / 180;
}

/// Radians to degrees.
[[nodiscard]] constexpr double rad2deg(double rad) noexcept
{
    return rad * 180 / std::numbers::pi;
}

}  // namespace MathUtil

}  // namespace QtRocket
