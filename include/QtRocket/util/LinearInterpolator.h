#pragma once

#include <cstddef>
#include <initializer_list>
#include <map>
#include <span>
#include <vector>

namespace QtRocket
{

/// Piecewise linear interpolation through a set of (x, y) points, as OpenRocket's
/// LinearInterpolator. Outside the points the value is held at the nearest end point. Adding a
/// point at an x that is already present replaces its y. Copying copies the points (Java's
/// clone()).
class LinearInterpolator
{
public:
    /// No points: add some before calling getValue().
    LinearInterpolator() = default;

    /// Throws std::invalid_argument when @p x and @p y differ in length
    /// (Java: IllegalArgumentException).
    LinearInterpolator(std::span<const double> x, std::span<const double> y);
    LinearInterpolator(std::initializer_list<double> x, std::initializer_list<double> y);

    void addPoint(double x, double y);
    /// Throws std::invalid_argument when @p x and @p y differ in length; nothing is added then.
    void addPoints(std::span<const double> x, std::span<const double> y);
    void addPoints(std::initializer_list<double> x, std::initializer_list<double> y);

    /// The interpolated value at @p x: the y of the first point below it, the y of the last point
    /// above it (also for NaN, which Java orders after every number). Throws std::logic_error
    /// when there are no points (Java: IllegalStateException). Deviation: getValue(-0.0) when the
    /// first point is at 0.0 holds that point's value, where OpenRocket's TreeMap.subMap(0.0, -0.0)
    /// throws IllegalArgumentException.
    [[nodiscard]] double getValue(double x) const;

    /// The x coordinates of the points in ascending order (Java: getXPoints()).
    [[nodiscard]] std::vector<double> xPoints() const;

    [[nodiscard]] std::size_t size() const noexcept { return m_points.size(); }
    [[nodiscard]] bool        empty() const noexcept { return m_points.empty(); }

private:
    /// java.lang.Double.compareTo, which OpenRocket's TreeMap orders the points by: NaN is greater
    /// than everything and equal to itself, and -0.0 is less than 0.0. A plain operator< would
    /// not be a strict weak ordering with NaN keys.
    struct JavaDoubleLess
    {
        [[nodiscard]] bool operator()(double a, double b) const noexcept;
    };

    std::map<double, double, JavaDoubleLess> m_points;
};

}  // namespace QtRocket
