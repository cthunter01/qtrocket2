#include "QtRocket/gui/binding/SliderMapping.h"

#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

namespace
{

/// Java's pow2(x) of ValueSliderModel.
[[nodiscard]] double pow2(double x) noexcept
{
    return x * x;
}

}  // namespace

SliderMapping::SliderMapping(bool linear, double min, double linearPosition, double mid,
                             double max) noexcept
  : m_linear(linear), m_min(min), m_linearPosition(linearPosition), m_mid(mid), m_max(max)
{
}

SliderMapping SliderMapping::linear(double min, double max) noexcept
{
    // Java: linearPosition 1, the mid model is the max model itself, the quadratic terms 0.
    return SliderMapping{true, min, 1.0, max, max};
}

SliderMapping SliderMapping::quadratic(double min, double linearPosition, double mid, double max)
{
    // Java's test, which a NaN fails too.
    const bool valid = min < mid && mid <= max && 0 < linearPosition && linearPosition < 1;
    if (!valid)
    {
        bug("Bad arguments for a slider mapping: it needs min < mid <= max and 0 < pos < 1");
    }
    SliderMapping mapping{false, min, linearPosition, mid, max};
    mapping.updateQuadraticParameters();
    return mapping;
}

SliderMapping SliderMapping::fromRange(double min, double mid, double max)
{
    if (MathUtil::equals(mid, (max + min) / 2.0))
    {
        return linear(min, max);
    }
    return quadratic(min, 0.5, mid, max);
}

void SliderMapping::updateQuadraticParameters() noexcept
{
    // Java's updateExponentialParameters(), operation for operation.
    const double pos   = m_linearPosition;
    const double delta = (m_mid - m_min) / pos;
    m_quad2            = (m_max - m_mid - delta + (delta * pos)) / pow2(pos - 1);
    m_quad1 = (delta + (2 * (m_mid - m_max) * pos) - (delta * pos * pos)) / pow2(pos - 1);
    m_quad0 =
        (m_mid - (((2 * m_mid) + delta) * pos) + ((m_max + delta) * pos * pos)) / pow2(pos - 1);
}

int SliderMapping::positionOf(double value, const Unit& unit) const
{
    const bool inverted = unit.getMultiplier() < 0;
    if (value <= m_min)
    {
        return inverted ? kMaximum : 0;
    }
    if (value >= m_max)
    {
        return inverted ? 0 : kMaximum;
    }

    double x = 0;
    if (value <= m_mid || m_quad2 == 0)
    {
        // The linear part (the whole range of a linear mapping).
        x = (value - m_min) * m_linearPosition / (m_mid - m_min);
    }
    else
    {
        // The quadratic part: the root of quad2 x^2 + quad1 x + quad0 - value.
        x = (MathUtil::safeSqrt((m_quad1 * m_quad1) - (4 * m_quad2 * (m_quad0 - value))) -
             m_quad1) /
            (2 * m_quad2);
    }
    if (inverted)
    {
        x = 1 - x;
    }
    return MathUtil::javaIntCast(x * kMaximum);
}

double SliderMapping::valueAt(int position, const Unit& unit) const
{
    const bool inverted = unit.getMultiplier() < 0;
    double     x        = static_cast<double>(position) / kMaximum;
    if (inverted)
    {
        x = 1 - x;
    }
    double scaledValue = 0;
    if (x <= m_linearPosition)
    {
        scaledValue = ((m_mid - m_min) / m_linearPosition * x) + m_min;
    }
    else
    {
        scaledValue = (m_quad2 * x * x) + (m_quad1 * x) + m_quad0;
    }
    return unit.fromUnit(unit.round(unit.toUnit(scaledValue)));
}

void SliderMapping::setBounds(double min, double max)
{
    m_min = min;
    m_max = max;
    if (m_linear)
    {
        m_mid = max;
        return;
    }
    // Java: mid := (max - min) / 3 (not min + (max - min) / 3), then the parameters again.
    m_mid = (max - min) / 3.0;
    updateQuadraticParameters();
}

}  // namespace QtRocket::Gui
