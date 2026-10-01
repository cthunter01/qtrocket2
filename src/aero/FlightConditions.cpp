#include "QtRocket/aero/FlightConditions.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numbers>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// Java's HashMap key equality for the nozzle areas: RocketComponent.equals (the same class and
/// id), which is identity within one component tree.
[[nodiscard]] bool sameAssembly(const ComponentAssembly& a, const ComponentAssembly& b) noexcept
{
    return a.equals(b);
}

/// The entry of @p areas whose assembly equals @p assembly, or nullptr.
[[nodiscard]] const FlightConditions::NozzleExitArea* findArea(
    std::span<const FlightConditions::NozzleExitArea> areas, const ComponentAssembly& assembly)
{
    for (const FlightConditions::NozzleExitArea& entry : areas)
    {
        if (sameAssembly(*entry.first, assembly))
        {
            return &entry;
        }
    }
    return nullptr;
}

/// Java's AbstractMap.equals for the nozzle areas: the same size, and every assembly of @p a
/// found in @p b with an equal area (Double.equals; the areas are finite and positive, so this is
/// ==).
[[nodiscard]] bool sameAreas(std::span<const FlightConditions::NozzleExitArea> a,
                             std::span<const FlightConditions::NozzleExitArea> b)
{
    return a.size() == b.size() &&
           std::ranges::all_of(a, [b](const FlightConditions::NozzleExitArea& entry) {
               const FlightConditions::NozzleExitArea* match = findArea(b, *entry.first);
               return match != nullptr && match->second == entry.second;
           });
}

}  // namespace

FlightConditions::FlightConditions(const FlightConfiguration& config)
{
    setRefLength(config.getReferenceLength());
}

FlightConditions::FlightConditions(const FlightConditions& other)
{
    copyValuesFrom(other);
}

FlightConditions& FlightConditions::operator=(const FlightConditions& other)
{
    if (this != &other)
    {
        copyValuesFrom(other);
    }
    return *this;
}

FlightConditions& FlightConditions::operator=(FlightConditions&& other) noexcept
{
    // Like the copy assignment, the values only: m_changed (this object's own connections) is
    // left alone, where a defaulted move would take the source's connections instead.
    if (this != &other)
    {
        m_thrustingNozzleExitAreas = std::move(other.m_thrustingNozzleExitAreas);
        other.m_thrustingNozzleExitAreas.clear();
        copyScalarsFrom(other);
    }
    return *this;
}

void FlightConditions::copyValuesFrom(const FlightConditions& other)
{
    m_thrustingNozzleExitAreas = other.m_thrustingNozzleExitAreas;
    copyScalarsFrom(other);
}

void FlightConditions::copyScalarsFrom(const FlightConditions& other) noexcept
{
    m_refLength             = other.m_refLength;
    m_refArea               = other.m_refArea;
    m_aoa                   = other.m_aoa;
    m_sinAOA                = other.m_sinAOA;
    m_sincAOA               = other.m_sincAOA;
    m_theta                 = other.m_theta;
    m_mach                  = other.m_mach;
    m_beta                  = other.m_beta;
    m_rollRate              = other.m_rollRate;
    m_pitchRate             = other.m_pitchRate;
    m_yawRate               = other.m_yawRate;
    m_pitchCenter           = other.m_pitchCenter;
    m_atmosphericConditions = other.m_atmosphericConditions;
    m_modId                 = other.m_modId;
}

void FlightConditions::setReference(const FlightConfiguration& config)
{
    setRefLength(config.getReferenceLength());
}

void FlightConditions::setRefLength(double length)
{
    if (m_refLength == length)
    {
        return;
    }
    m_refLength = length;
    m_refArea   = std::numbers::pi * MathUtil::pow2(length / 2);
    fireChangeEvent();
}

void FlightConditions::setRefArea(double area)
{
    if (m_refArea == area)
    {
        return;
    }
    m_refArea   = area;
    m_refLength = MathUtil::safeSqrt(area / std::numbers::pi) * 2;
    fireChangeEvent();
}

void FlightConditions::setThrustingNozzleExitAreas(std::span<const NozzleExitArea> areas)
{
    std::vector<NozzleExitArea> validated;
    for (std::size_t i = 0; i < areas.size(); ++i)
    {
        const NozzleExitArea& entry = areas[i];
        if (entry.first == nullptr)
        {
            bug("Thrusting nozzle exit areas must not contain null entries");
        }
        if (entry.second < 0 || !std::isfinite(entry.second))
        {
            bug("Thrusting nozzle exit area must be finite and non-negative");
        }
        if (findArea(areas.first(i), *entry.first) != nullptr)
        {
            bug("Thrusting nozzle exit areas must not contain an assembly twice");
        }
        if (entry.second > 0)
        {
            validated.push_back(entry);
        }
    }

    if (sameAreas(m_thrustingNozzleExitAreas, validated))
    {
        return;
    }
    m_thrustingNozzleExitAreas = std::move(validated);
    fireChangeEvent();
}

double FlightConditions::getThrustingNozzleExitArea(const ComponentAssembly& assembly) const
{
    const NozzleExitArea* entry = findArea(m_thrustingNozzleExitAreas, assembly);
    return entry != nullptr ? entry->second : 0.0;
}

double FlightConditions::getThrustingNozzleExitArea() const noexcept
{
    double totalArea = 0;
    for (const NozzleExitArea& entry : m_thrustingNozzleExitAreas)
    {
        totalArea += entry.second;
    }
    return totalArea;
}

void FlightConditions::setAOA(double aoa)
{
    aoa = MathUtil::clamp(aoa, 0, std::numbers::pi);
    if (MathUtil::equals(m_aoa, aoa))
    {
        return;
    }

    m_aoa = aoa;
    if (aoa < 0.001)
    {
        m_sinAOA  = aoa;
        m_sincAOA = 1.0;
    }
    else
    {
        m_sinAOA  = std::sin(aoa);
        m_sincAOA = m_sinAOA / aoa;
    }
    fireChangeEvent();
}

void FlightConditions::setAOA(double aoa, double sinAOA)
{
    aoa    = MathUtil::clamp(aoa, 0, std::numbers::pi);
    sinAOA = MathUtil::clamp(sinAOA, 0, 1);
    if (MathUtil::equals(m_aoa, aoa))
    {
        return;
    }

    m_aoa    = aoa;
    m_sinAOA = sinAOA;
    if (aoa < 0.001)
    {
        m_sincAOA = 1.0;
    }
    else
    {
        m_sincAOA = sinAOA / aoa;
    }
    fireChangeEvent();
}

void FlightConditions::setTheta(double theta)
{
    if (MathUtil::equals(m_theta, theta))
    {
        return;
    }
    m_theta = theta;
    fireChangeEvent();
}

void FlightConditions::setMach(double mach)
{
    mach = MathUtil::javaMax(mach, 0);
    if (MathUtil::equals(m_mach, mach))
    {
        return;
    }
    m_mach = mach;
    m_beta = calculateBeta(mach);
    fireChangeEvent();
}

double FlightConditions::getVelocity() const noexcept
{
    return m_mach * m_atmosphericConditions.getMachSpeed();
}

void FlightConditions::setVelocity(double velocity)
{
    setMach(velocity / m_atmosphericConditions.getMachSpeed());
}

double FlightConditions::calculateBeta(double mach) noexcept
{
    if (mach < 1)
    {
        return MathUtil::max(kMinBeta, MathUtil::safeSqrt(1 - (mach * mach)));
    }
    return MathUtil::max(kMinBeta, MathUtil::safeSqrt((mach * mach) - 1));
}

void FlightConditions::setRollRate(double rate)
{
    if (MathUtil::equals(m_rollRate, rate))
    {
        return;
    }
    m_rollRate = rate;
    fireChangeEvent();
}

void FlightConditions::setPitchRate(double pitchRate)
{
    if (MathUtil::equals(m_pitchRate, pitchRate))
    {
        return;
    }
    m_pitchRate = pitchRate;
    fireChangeEvent();
}

void FlightConditions::setYawRate(double yawRate)
{
    if (MathUtil::equals(m_yawRate, yawRate))
    {
        return;
    }
    m_yawRate = yawRate;
    fireChangeEvent();
}

void FlightConditions::setPitchCenter(const Coordinate& pitchCenter)
{
    if (m_pitchCenter == pitchCenter)
    {
        return;
    }
    m_pitchCenter = pitchCenter;
    fireChangeEvent();
}

void FlightConditions::setAtmosphericConditions(const AtmosphericConditions& conditions)
{
    if (m_atmosphericConditions == conditions)
    {
        return;
    }
    m_atmosphericConditions = conditions;
    fireChangeEvent();
}

std::string FlightConditions::toString() const
{
    // Java's String.format with %.2f etc.; the pitch centre and atmospheric conditions are
    // concatenated into the format string.
    return std::format(
        "FlightConditions[aoa={}°,theta={}°,mach={},thrustingNozzleExitArea={},"
        "rollRate={},pitchRate={},yawRate={},refLength={},pitchCenter={},"
        "atmosphericConditions={}]",
        Strings::formatFixed(m_aoa * 180 / std::numbers::pi, 2),
        Strings::formatFixed(m_theta * 180 / std::numbers::pi, 2), Strings::formatFixed(m_mach, 3),
        Strings::formatFixed(getThrustingNozzleExitArea(), 6), Strings::formatFixed(m_rollRate, 2),
        Strings::formatFixed(m_pitchRate, 2), Strings::formatFixed(m_yawRate, 2),
        Strings::formatFixed(m_refLength, 3), m_pitchCenter.toString(),
        m_atmosphericConditions.toString());
}

bool FlightConditions::operator==(const FlightConditions& other) const noexcept
{
    if (this == &other)
    {
        return true;
    }
    return MathUtil::equals(m_refLength, other.m_refLength) &&
           sameAreas(m_thrustingNozzleExitAreas, other.m_thrustingNozzleExitAreas) &&
           MathUtil::equals(m_aoa, other.m_aoa) && MathUtil::equals(m_theta, other.m_theta) &&
           MathUtil::equals(m_mach, other.m_mach) &&
           MathUtil::equals(m_rollRate, other.m_rollRate) &&
           MathUtil::equals(m_pitchRate, other.m_pitchRate) &&
           MathUtil::equals(m_yawRate, other.m_yawRate) && m_pitchCenter == other.m_pitchCenter &&
           m_atmosphericConditions == other.m_atmosphericConditions;
}

int FlightConditions::hashCode() const noexcept
{
    const int hash = MathUtil::javaIntCast(
        1000 * (m_refLength + m_aoa + m_theta + m_mach + m_rollRate + m_pitchRate + m_yawRate));

    // HashMap.hashCode(): the sum of key.hashCode() ^ value.hashCode() over the entries, wrapping.
    std::uint32_t mapHash = 0;
    for (const NozzleExitArea& entry : m_thrustingNozzleExitAreas)
    {
        const int entryHash = entry.first->hashCode() ^ MathUtil::javaDoubleHashCode(entry.second);
        mapHash += static_cast<std::uint32_t>(entryHash);
    }
    return MathUtil::javaHashCombine(hash, static_cast<int>(mapHash));
}

void FlightConditions::fireChangeEvent()
{
    m_modId = ModId{};
    m_changed.emit();
}

}  // namespace QtRocket
