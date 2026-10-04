#include "QtRocket/rocket/TrapezoidFinSet.h"

#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

TrapezoidFinSet::TrapezoidFinSet() : TrapezoidFinSet(3, 0.05, 0.05, 0.025, 0.03) { }

TrapezoidFinSet::TrapezoidFinSet(int fins, double rootChord, double tipChord, double sweep,
                                 double height)
  : m_tipChord(tipChord), m_height(height), m_sweep(sweep)
{
    // Java calls setFinCount() here; its event reaches no one (the new fin set has no parent).
    assignFinCount(fins);
    m_length = rootChord;
}

std::unique_ptr<RocketComponent> TrapezoidFinSet::cloneShallow() const
{
    return std::make_unique<TrapezoidFinSet>(*this);
}

void TrapezoidFinSet::setFinShape(double rootChord, double tipChord, double sweep, double height,
                                  double thickness)
{
    if (m_length == rootChord && m_tipChord == tipChord && m_sweep == sweep && m_height == height &&
        m_thickness == thickness)
    {
        return;
    }
    m_length    = rootChord;
    m_tipChord  = tipChord;
    m_sweep     = sweep;
    m_height    = height;
    m_thickness = thickness;

    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void TrapezoidFinSet::setRootChord(double r)
{
    if (m_length == r)
    {
        return;
    }
    m_length = MathUtil::javaMax(r, 0);
    updateTabPosition();

    fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
}

void TrapezoidFinSet::setTipChord(double r)
{
    if (m_tipChord == r)
    {
        return;
    }
    m_tipChord = MathUtil::javaMax(r, 0);
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void TrapezoidFinSet::setSweep(double r)
{
    if (m_sweep == r)
    {
        return;
    }
    m_sweep = r;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

double TrapezoidFinSet::getSweepAngle() const
{
    if (m_height == 0)
    {
        if (m_sweep > 0)
        {
            return std::numbers::pi / 2;
        }
        if (m_sweep < 0)
        {
            return -std::numbers::pi / 2;
        }
        return 0;
    }
    return std::atan2(m_sweep, m_height);
}

void TrapezoidFinSet::setSweepAngle(double r)
{
    const double angle   = MathUtil::clamp(r, -kMaxSweepAngle, kMaxSweepAngle);
    const double mySweep = std::tan(angle) * m_height;
    if (std::isnan(mySweep) || std::isinf(mySweep))
    {
        return;
    }
    setSweep(mySweep);
}

void TrapezoidFinSet::setHeight(double r)
{
    if (m_height == r)
    {
        return;
    }
    m_height = MathUtil::javaMax(r, 0);
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

std::vector<Coordinate> TrapezoidFinSet::getFinPoints() const
{
    std::vector<Coordinate> points;
    points.reserve(4);

    points.push_back(Coordinate::kNul);
    points.emplace_back(m_sweep, m_height);
    if (m_tipChord > 0.0001)
    {
        points.emplace_back(m_sweep + m_tipChord, m_height);
    }
    points.emplace_back(MathUtil::max(m_length, 0.0001), 0);

    alignEndsWithRoot(points);

    return points;
}

double TrapezoidFinSet::getSpan() const
{
    return m_height;
}

}  // namespace QtRocket
