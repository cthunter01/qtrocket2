#include "QtRocket/rocket/ParallelStage.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

ParallelStage::ParallelStage()
{
    // Two instances pi apart (the member initialisers).
    m_axialMethod = AxialMethod::BOTTOM;
}

ParallelStage::ParallelStage(int count)
  : m_instanceCount(count),
    // Java divides without a check: a count of 0 gives an infinite separation.
    m_angleSeparation(std::numbers::pi * 2 / count)
{
    m_axialMethod = AxialMethod::BOTTOM;
}

std::unique_ptr<RocketComponent> ParallelStage::cloneShallow() const
{
    return std::make_unique<ParallelStage>(*this);
}

std::vector<Coordinate> ParallelStage::getComponentBounds() const
{
    // Not strictly accurate, but an acceptable estimate of the total vehicle size.
    std::vector<Coordinate> bounds;
    bounds.reserve(8);
    double xMin = std::numeric_limits<double>::max();
    // Java's Double.MIN_VALUE: the smallest positive double, not the most negative one.
    double xMax = std::numeric_limits<double>::denorm_min();
    double rMax = 0;

    // std::min() and std::max() keep the current value for a NaN, as Java's comparisons do.
    for (const Coordinate& currentInstanceLocation : getComponentLocations())
    {
        xMin = std::min(xMin, currentInstanceLocation.x);
        xMax = std::max(xMax, currentInstanceLocation.x + m_length);
        rMax = std::max(rMax, getRadiusOffset());
    }
    addBound(bounds, xMin, rMax);
    addBound(bounds, xMax, rMax);
    return bounds;
}

bool ParallelStage::isAfter() const
{
    return false;
}

bool ParallelStage::isLaunchStage(const FlightConfiguration& config) const
{
    return config.isStageActive(m_stageNumber);
}

int ParallelStage::getInstanceCount() const
{
    return m_instanceCount;
}

void ParallelStage::setInstanceCount(int newCount)
{
    if (newCount < 1)
    {
        // There must be at least one instance.
        return;
    }
    m_instanceCount   = newCount;
    m_angleSeparation = std::numbers::pi * 2 / m_instanceCount;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

std::vector<Coordinate> ParallelStage::getInstanceLocations() const
{
    return RocketComponent::getInstanceLocations();
}

std::vector<Coordinate> ParallelStage::getInstanceOffsets() const
{
    const double radius = QtRocket::getRadius(m_radiusMethod, m_parent, this, m_radiusOffset);

    std::vector<Coordinate>   toReturn;
    const std::vector<double> angles = getInstanceAngles();
    toReturn.reserve(angles.size());
    for (const double angle : angles)
    {
        const double curY = radius * std::cos(angle);
        const double curZ = radius * std::sin(angle);
        toReturn.emplace_back(0, curY, curZ);
    }
    return toReturn;
}

std::string ParallelStage::getPatternName() const
{
    return std::to_string(getInstanceCount()) + "-ring";
}

double ParallelStage::getInstanceAngleIncrement() const
{
    return m_angleSeparation;
}

std::vector<double> ParallelStage::getInstanceAngles() const
{
    const double baseAngle = getAngleOffset();
    const double incrAngle = getInstanceAngleIncrement();

    std::vector<double> result;
    result.reserve(static_cast<std::size_t>(std::max(getInstanceCount(), 0)));
    for (int i = 0; i < getInstanceCount(); ++i)
    {
        result.push_back(baseAngle + (incrAngle * i));
    }
    return result;
}

double ParallelStage::getAngleOffset() const
{
    return m_angleOffset;
}

void ParallelStage::setAngleOffset(double angle)
{
    m_angleOffset = MathUtil::reducePi(angle);
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

AngleMethod ParallelStage::getAngleMethod() const
{
    return m_angleMethod;
}

void ParallelStage::setAngleMethod(AngleMethod newMethod)
{
    m_angleMethod = newMethod;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

double ParallelStage::getBoundingRadius() const
{
    return ComponentAssembly::getBoundingRadius();
}

double ParallelStage::getRadiusOffset() const
{
    return m_radiusOffset;
}

void ParallelStage::setRadiusOffset(double radius)
{
    if (radius == m_radiusOffset)
    {
        return;
    }
    m_radiusOffset = clampToZero(m_radiusMethod) ? 0.0 : radius;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

RadiusMethod ParallelStage::getRadiusMethod() const
{
    return m_radiusMethod;
}

void ParallelStage::setRadiusMethod(RadiusMethod method)
{
    if (method == m_radiusMethod)
    {
        return;
    }
    // The radius from the parent's centre.
    const double radius = QtRocket::getRadius(m_radiusMethod, m_parent, this, m_radiusOffset);
    setRadius(method, radius);
}

void ParallelStage::setRadius(RadiusMethod method, double radius)
{
    double newRadius = radius;
    if (clampToZero(method))
    {
        newRadius = 0.0;
    }
    m_radiusMethod = method;
    m_radiusOffset = QtRocket::getAsOffset(m_radiusMethod, m_parent, this, newRadius);
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void ParallelStage::setAxialMethod(AxialMethod newMethod)
{
    ComponentAssembly::setAxialMethod(newMethod);
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

}  // namespace QtRocket
