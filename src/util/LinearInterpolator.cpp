#include "QtRocket/util/LinearInterpolator.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <iterator>
#include <map>
#include <span>
#include <vector>

#include "QtRocket/util/BugError.h"

namespace QtRocket
{

bool LinearInterpolator::JavaDoubleLess::operator()(double a, double b) const noexcept
{
    if (std::isnan(a))
    {
        return false;
    }
    if (std::isnan(b))
    {
        return true;
    }
    if (a < b)
    {
        return true;
    }
    if (a > b)
    {
        return false;
    }
    return std::signbit(a) && !std::signbit(b);
}

LinearInterpolator::LinearInterpolator(std::span<const double> x, std::span<const double> y)
{
    addPoints(x, y);
}

LinearInterpolator::LinearInterpolator(std::initializer_list<double> x,
                                       std::initializer_list<double> y)
{
    addPoints(x, y);
}

void LinearInterpolator::addPoint(double x, double y)
{
    m_points.insert_or_assign(x, y);
}

void LinearInterpolator::addPoints(std::span<const double> x, std::span<const double> y)
{
    if (x.size() != y.size())
    {
        bug(std::format("Array lengths do not match, x={} y={}", x.size(), y.size()));
    }
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        m_points.insert_or_assign(x[i], y[i]);
    }
}

void LinearInterpolator::addPoints(std::initializer_list<double> x, std::initializer_list<double> y)
{
    addPoints(std::span<const double>{x.begin(), x.size()},
              std::span<const double>{y.begin(), y.size()});
}

double LinearInterpolator::getValue(double x) const
{
    if (const auto exact = m_points.find(x); exact != m_points.end())
    {
        return exact->second;
    }
    if (m_points.empty())
    {
        bug("No points added yet to the interpolator.");
    }
    // x is not a point, so the first point not below it is strictly above it.
    const auto above = m_points.lower_bound(x);
    if (above == m_points.begin())
    {
        return above->second;  // below the first point: hold its value
    }
    const auto below = std::prev(above);
    if (above == m_points.end())
    {
        return below->second;  // above the last point (or NaN): hold its value
    }
    const double x1 = below->first;
    const double y1 = below->second;
    const double x2 = above->first;
    const double y2 = above->second;
    return ((x - x1) / (x2 - x1) * (y2 - y1)) + y1;
}

std::vector<double> LinearInterpolator::xPoints() const
{
    std::vector<double> x;
    x.reserve(m_points.size());
    for (const auto& point : m_points)
    {
        x.push_back(point.first);
    }
    return x;
}

}  // namespace QtRocket
